#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include "crypto/crypto_stream.hpp"  

namespace dfs {
namespace network {

Codec::Codec(const std::vector<uint8_t>& encryption_key) : key_(encryption_key) {
    if (encryption_key.size() != 32) {
        throw std::invalid_argument("Encryption key must be 32 bytes (256 bits)");
    }
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        // Write initialization vector
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        // Write message type
        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, sizeof(uint8_t));
        total_bytes += sizeof(uint8_t);

        // Write source ID (network byte order)
        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Write payload size (network byte order)
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        // Initialize CryptoStream for filename length encryption
        crypto::CryptoStream filename_crypto;
        filename_crypto.initialize(key_, std::vector<uint8_t>(frame.initialization_vector.begin(), 
                                            frame.initialization_vector.end()));

        // Encrypt and write filename length
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        std::stringstream filename_length_stream;
        filename_length_stream.write(reinterpret_cast<const char*>(&network_filename_length), sizeof(uint32_t));
        filename_length_stream.seekg(0);

        std::stringstream encrypted_filename_length;
        filename_crypto.encrypt(filename_length_stream, encrypted_filename_length);

        std::string encrypted_length = encrypted_filename_length.str();
        write_bytes(output, encrypted_length.data(), encrypted_length.size());
        total_bytes += encrypted_length.size();

        if (frame.payload_stream && frame.payload_size > 0) {
            // Initialize separate CryptoStream for payload encryption
            crypto::CryptoStream payload_crypto;
            payload_crypto.initialize(key_, std::vector<uint8_t>(frame.initialization_vector.begin(), 
                                                frame.initialization_vector.end()));

            // Reset stream position for reading
            frame.payload_stream->seekg(0);

            // Process payload in chunks
            char buffer[4096];
            while (frame.payload_stream->good() && !frame.payload_stream->eof()) {
                frame.payload_stream->read(buffer, sizeof(buffer));
                std::streamsize bytes_read = frame.payload_stream->gcount();
                if (bytes_read > 0) {
                    // Create a temporary stream for the chunk
                    std::stringstream chunk;
                    chunk.write(buffer, bytes_read);
                    chunk.seekg(0);

                    // Create a temporary stream for encrypted chunk
                    std::stringstream encrypted_chunk;
                    payload_crypto.encrypt(chunk, encrypted_chunk);

                    // Write encrypted chunk to output
                    std::string encrypted_data = encrypted_chunk.str();
                    write_bytes(output, encrypted_data.data(), encrypted_data.size());
                    total_bytes += encrypted_data.size();
                }
            }

            // Reset stream position
            frame.payload_stream->seekg(0);
        }

        return total_bytes;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Serialization error: " << e.what();
        throw;
    }
}

MessageFrame Codec::deserialize(std::istream& input, Channel& channel) {
    if (!input.good()) {
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;

    try {
        // Read initialization vector
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        // Read message type
        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(uint8_t));
        frame.message_type = static_cast<MessageType>(msg_type);

        // Read source_id from network byte order
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        // Read payload_size from network byte order
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        // Initialize CryptoStream for filename length decryption
        crypto::CryptoStream filename_crypto;
        filename_crypto.initialize(key_, std::vector<uint8_t>(frame.initialization_vector.begin(),
                                            frame.initialization_vector.end()));

        // Read and decrypt filename length
        char filename_length_buffer[sizeof(uint32_t)];
        read_bytes(input, filename_length_buffer, sizeof(uint32_t));

        std::stringstream encrypted_filename_length;
        encrypted_filename_length.write(filename_length_buffer, sizeof(uint32_t));
        encrypted_filename_length.seekg(0);

        std::stringstream decrypted_filename_length;
        filename_crypto.decrypt(encrypted_filename_length, decrypted_filename_length);

        uint32_t network_filename_length;
        decrypted_filename_length.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        // Create empty payload stream if payload expected
        if (frame.payload_size > 0) {
            frame.payload_stream = std::make_shared<std::stringstream>();
        }

        // Push frame to channel before processing payload
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(frame);
        }

        // Process payload if present
        if (frame.payload_size > 0) {
            // Initialize separate CryptoStream for payload decryption
            crypto::CryptoStream payload_crypto;
            payload_crypto.initialize(key_, std::vector<uint8_t>(frame.initialization_vector.begin(),
                                                frame.initialization_vector.end()));

            // Process payload in chunks
            std::size_t remaining = frame.payload_size;
            char chunk_buffer[4096];  // 4KB chunks for streaming

            while (remaining > 0 && input.good()) {
                std::size_t chunk_size = std::min(remaining, sizeof(chunk_buffer));

                // Read encrypted chunk
                read_bytes(input, chunk_buffer, chunk_size);

                // Create temporary stream for the encrypted chunk
                std::stringstream encrypted_chunk;
                encrypted_chunk.write(chunk_buffer, chunk_size);
                encrypted_chunk.seekg(0);

                // Create temporary stream for decrypted chunk
                std::stringstream decrypted_chunk;
                payload_crypto.decrypt(encrypted_chunk, decrypted_chunk);

                // Write decrypted chunk to payload stream
                frame.payload_stream->write(decrypted_chunk.str().data(), decrypted_chunk.str().size());

                remaining -= chunk_size;
            }

            if (remaining > 0) {
                throw std::runtime_error("Failed to read complete payload");
            }

            // Reset stream position
            frame.payload_stream->seekg(0);
        }

        return frame;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
        throw;
    }
}

void Codec::write_bytes(std::ostream& output, const void* data, std::size_t size) {
    if (!output.write(static_cast<const char*>(data), size)) {
        throw std::runtime_error("Failed to write to output stream");
    }
}

void Codec::read_bytes(std::istream& input, void* data, std::size_t size) {
    if (!input.read(static_cast<char*>(data), size)) {
        throw std::runtime_error("Failed to read from input stream");
    }
}

} // namespace network
} // namespace dfs