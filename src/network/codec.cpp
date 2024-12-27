#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include "crypto/crypto_stream.hpp"  

namespace dfs {
namespace network {

namespace {
// Helper to convert array to vector for CryptoStream initialization
std::vector<uint8_t> to_vector(const std::array<uint8_t, 16>& arr) {
    return std::vector<uint8_t>(arr.begin(), arr.end());
}
}  // namespace

Codec::Codec(const std::vector<uint8_t>& encryption_key) : key_(encryption_key) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Codec";
    if (encryption_key.size() != 32) {
        BOOST_LOG_TRIVIAL(error) << "Invalid encryption key size: " << encryption_key.size() << " bytes (expected 32 bytes)";
        throw std::invalid_argument("Encryption key must be 32 bytes (256 bits)");
    }
    BOOST_LOG_TRIVIAL(debug) << "Codec initialized successfully";
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    BOOST_LOG_TRIVIAL(info) << "Starting frame serialization";

    if (!output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid output stream state";
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        BOOST_LOG_TRIVIAL(debug) << "Writing frame header components";

        // Write initialization vector
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();
        BOOST_LOG_TRIVIAL(debug) << "Wrote initialization vector: " << frame.initialization_vector.size() << " bytes";

        // Write message type
        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, sizeof(uint8_t));
        total_bytes += sizeof(uint8_t);
        BOOST_LOG_TRIVIAL(debug) << "Wrote message type: " << static_cast<int>(msg_type);

        // Write source ID (network byte order)
        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);
        BOOST_LOG_TRIVIAL(debug) << "Wrote source ID: " << frame.source_id;

        // Write payload size (network byte order)
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);
        BOOST_LOG_TRIVIAL(debug) << "Wrote payload size: " << frame.payload_size << " bytes";

        // Initialize CryptoStream for filename length
        BOOST_LOG_TRIVIAL(debug) << "Initializing crypto stream for filename length encryption";
        crypto::CryptoStream filename_crypto;
        filename_crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Prepare filename length buffer
        std::stringstream filename_length_stream;
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        filename_length_stream.write(reinterpret_cast<const char*>(&network_filename_length), sizeof(uint32_t));
        filename_length_stream.seekg(0);

        // Encrypt and write filename length
        std::stringstream encrypted_filename_length;
        filename_crypto.encrypt(filename_length_stream, encrypted_filename_length);

        std::string encrypted_length = encrypted_filename_length.str();
        write_bytes(output, encrypted_length.data(), encrypted_length.size());
        total_bytes += encrypted_length.size();
        BOOST_LOG_TRIVIAL(debug) << "Wrote encrypted filename length: " << frame.filename_length 
                                << " (encrypted size: " << encrypted_length.size() << " bytes)";

        // Process payload if present
        if (frame.payload_stream && frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(info) << "Processing payload of size: " << frame.payload_size << " bytes";

            // Initialize separate CryptoStream for payload
            BOOST_LOG_TRIVIAL(debug) << "Initializing crypto stream for payload encryption";
            crypto::CryptoStream payload_crypto;
            payload_crypto.initialize(key_, to_vector(frame.initialization_vector));

            // Reset stream position for reading
            frame.payload_stream->seekg(0);

            // Process payload in chunks
            char buffer[4096];
            size_t chunks_processed = 0;
            while (frame.payload_stream->good() && !frame.payload_stream->eof()) {
                frame.payload_stream->read(buffer, sizeof(buffer));
                std::streamsize bytes_read = frame.payload_stream->gcount();
                if (bytes_read > 0) {
                    // Create a temporary stream for the chunk
                    std::stringstream chunk;
                    chunk.write(buffer, bytes_read);
                    chunk.seekg(0);

                    // Create a temporary stream for the encrypted chunk
                    std::stringstream encrypted_chunk;
                    payload_crypto.encrypt(chunk, encrypted_chunk);

                    // Write the encrypted chunk
                    std::string encrypted_data = encrypted_chunk.str();
                    write_bytes(output, encrypted_data.data(), encrypted_data.size());
                    total_bytes += encrypted_data.size();
                    chunks_processed++;
                    BOOST_LOG_TRIVIAL(debug) << "Processed payload chunk " << chunks_processed 
                                          << ": " << bytes_read << " bytes";
                }
            }

            // Reset stream position
            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(info) << "Completed payload processing: " << chunks_processed << " chunks";
        }

        BOOST_LOG_TRIVIAL(info) << "Frame serialization complete. Total bytes written: " << total_bytes;
        return total_bytes;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Serialization error: " << e.what();
        throw;
    }
}

MessageFrame Codec::deserialize(std::istream& input, Channel& channel) {
    BOOST_LOG_TRIVIAL(info) << "Starting frame deserialization";

    if (!input.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream state";
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;

    try {
        BOOST_LOG_TRIVIAL(debug) << "Reading frame header components";

        // Read initialization vector
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());
        BOOST_LOG_TRIVIAL(debug) << "Read initialization vector: " << frame.initialization_vector.size() << " bytes";

        // Read message type
        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(uint8_t));
        frame.message_type = static_cast<MessageType>(msg_type);
        BOOST_LOG_TRIVIAL(debug) << "Read message type: " << static_cast<int>(msg_type);

        // Read source_id from network byte order
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);
        BOOST_LOG_TRIVIAL(debug) << "Read source ID: " << frame.source_id;

        // Read payload_size from network byte order
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Read payload size: " << frame.payload_size << " bytes";

        // Initialize CryptoStream for filename length decryption
        BOOST_LOG_TRIVIAL(debug) << "Initializing crypto stream for filename length decryption";
        crypto::CryptoStream filename_crypto;
        filename_crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Read the complete encrypted block containing filename length
        std::vector<char> filename_buffer(32);  // Using fixed crypto block size
        read_bytes(input, filename_buffer.data(), 32);
        BOOST_LOG_TRIVIAL(debug) << "Read encrypted filename length block: 32 bytes";

        // Decrypt filename length
        std::stringstream encrypted_filename_length;
        encrypted_filename_length.write(filename_buffer.data(), 32);
        encrypted_filename_length.seekg(0);

        std::stringstream decrypted_filename_length;
        filename_crypto.decrypt(encrypted_filename_length, decrypted_filename_length);

        uint32_t network_filename_length;
        decrypted_filename_length.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Decrypted filename length: " << frame.filename_length;

        // Process payload if present
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(info) << "Preparing to process payload of size: " << frame.payload_size << " bytes";
            frame.payload_stream = std::make_shared<std::stringstream>();

            // Initialize separate CryptoStream for payload decryption
            BOOST_LOG_TRIVIAL(debug) << "Initializing crypto stream for payload decryption";
            crypto::CryptoStream payload_crypto;
            payload_crypto.initialize(key_, to_vector(frame.initialization_vector));

            // Process payload in chunks
            std::size_t remaining = frame.payload_size;
            char chunk_buffer[4096];
            size_t chunks_processed = 0;

            while (remaining > 0 && input.good()) {
                std::size_t chunk_size = std::min(remaining, sizeof(chunk_buffer));
                read_bytes(input, chunk_buffer, chunk_size);

                // Create temporary stream for the encrypted chunk
                std::stringstream encrypted_chunk;
                encrypted_chunk.write(chunk_buffer, chunk_size);
                encrypted_chunk.seekg(0);

                // Create temporary stream for decrypted chunk
                std::stringstream decrypted_chunk;
                payload_crypto.decrypt(encrypted_chunk, decrypted_chunk);

                // Write decrypted chunk to payload stream
                std::string decrypted_data = decrypted_chunk.str();
                frame.payload_stream->write(decrypted_data.data(), decrypted_data.size());

                remaining -= chunk_size;
                chunks_processed++;
                BOOST_LOG_TRIVIAL(debug) << "Processed payload chunk " << chunks_processed 
                                      << ": " << chunk_size << " bytes";
            }

            if (remaining > 0) {
                BOOST_LOG_TRIVIAL(error) << "Failed to read complete payload, " << remaining << " bytes remaining";
                throw std::runtime_error("Failed to read complete payload");
            }

            // Reset stream position
            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(info) << "Completed payload processing: " << chunks_processed << " chunks";
        }

        // Push frame to channel
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(frame);
            BOOST_LOG_TRIVIAL(debug) << "Frame pushed to channel";
        }

        BOOST_LOG_TRIVIAL(info) << "Frame deserialization complete";
        return frame;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
        throw;
    }
}

void Codec::write_bytes(std::ostream& output, const void* data, std::size_t size) {
    if (!output.write(static_cast<const char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to write " << size << " bytes to output stream";
        throw std::runtime_error("Failed to write to output stream");
    }
    BOOST_LOG_TRIVIAL(debug) << "Successfully wrote " << size << " bytes to output stream";
}

void Codec::read_bytes(std::istream& input, void* data, std::size_t size) {
    if (!input.read(static_cast<char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to read " << size << " bytes from input stream";
        throw std::runtime_error("Failed to read from input stream");
    }
    BOOST_LOG_TRIVIAL(debug) << "Successfully read " << size << " bytes from input stream";
}

} // namespace network
} // namespace dfs