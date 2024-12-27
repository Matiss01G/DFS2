#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include "crypto/crypto_stream.hpp"  

namespace dfs {
namespace network {

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        // Write IV to socket
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        // write message type to socket
        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, sizeof(uint8_t));
        total_bytes += sizeof(uint8_t);

        // write source ID to socket
        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Write payload size to socket
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        if (frame.payload_stream && frame.payload_size > 0) {
            // Initialize CryptoStream with key derived from IV
            crypto::CryptoStream crypto;
            std::vector<uint8_t> key(frame.initialization_vector.begin(), 
                                   frame.initialization_vector.end());
            key.resize(32); // Ensure key is 256 bits for AES-256
            crypto.initialize(key, std::vector<uint8_t>(frame.initialization_vector.begin(), 
                                              frame.initialization_vector.end()));

            // convert filename_length to network byte order
            uint32_t network_filename_length = to_network_order(frame.filename_length);

            // Write to temporary stream for encryption
            std::stringstream filename_length_stream;
            write_bytes(filename_length_stream, &network_filename_length, sizeof(uint32_t));

            // Encrypt and write to output 
            std::stringstream encrypted_filename_length;
            crypto.encrypt(filename_length_stream, encrypted_filename_length);

            // Write the encrypted filename_length to output stream
            std::string encrypted_data = encrypted_filename_length.str();
            write_bytes(output, encrypted_data.data(), encrypted_data.size());
            total_bytes += encrypted_data.size();

            // Encrypt payload using the same CryptoStream instance
            frame.payload_stream->seekg(0);
            crypto.encrypt(*frame.payload_stream, output);
            total_bytes += frame.payload_size;

            // Reset stream position
            frame.payload_stream->seekg(0);
        } else {
            uint32_t network_filename_length = 0;
            write_bytes(output, &network_filename_length, sizeof(uint32_t));
            total_bytes += sizeof(uint32_t);
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

        // Read and convert source_id from network byte order
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        // Read and convert payload_size from network byte order
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        // Initialize CryptoStream with key derived from IV
        crypto::CryptoStream crypto;
        std::vector<uint8_t> key(frame.initialization_vector.begin(), 
                               frame.initialization_vector.end());
        key.resize(32); // Ensure key is 256 bits for AES-256
        crypto.initialize(key, std::vector<uint8_t>(frame.initialization_vector.begin(),
                                          frame.initialization_vector.end()));

        // Read the encrypted filename length
        char buffer[sizeof(uint32_t)];
        std::stringstream encrypted_filename_length;
        read_bytes(input, buffer, sizeof(uint32_t));
        encrypted_filename_length.write(buffer, sizeof(uint32_t));
        encrypted_filename_length.seekg(0);

        // Decrypt the filename length
        std::stringstream decrypted_filename_length;
        crypto.decrypt(encrypted_filename_length, decrypted_filename_length);

        // Read the decrypted filename length
        uint32_t network_filename_length;
        decrypted_filename_length.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        // Create payload stream before processing payload
        if (frame.payload_size > 0) {
            frame.payload_stream = std::make_shared<std::stringstream>();
        }

        // Push frame to channel early with empty payload stream
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(frame);
        }

        // Process payload if present
        if (frame.payload_size > 0) {
            char chunk_buffer[4096];  // 4KB chunks for streaming
            std::stringstream encrypted_chunk;
            std::size_t remaining = frame.payload_size;

            // Process payload in chunks
            while (remaining > 0 && input.good()) {
                std::size_t chunk_size = std::min(remaining, sizeof(chunk_buffer));

                // Read encrypted chunk
                read_bytes(input, chunk_buffer, chunk_size);
                encrypted_chunk.write(chunk_buffer, chunk_size);
                encrypted_chunk.seekg(0);

                // Decrypt chunk directly to payload stream
                crypto.decrypt(encrypted_chunk, *frame.payload_stream);

                // Clear chunk buffer for next iteration
                encrypted_chunk.str("");
                encrypted_chunk.clear();

                remaining -= chunk_size;
            }

            if (remaining > 0) {
                throw std::runtime_error("Failed to read complete payload");
            }

            // Reset stream position after all chunks processed
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