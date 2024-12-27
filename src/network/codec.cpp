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
        // Write header fields (unencrypted)
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

        // Write filename length (network byte order)
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        write_bytes(output, &network_filename_length, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Write payload size (network byte order)
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        // Process payload if present
        if (frame.payload_stream && frame.payload_size > 0) {
            // Save stream position
            auto initial_pos = frame.payload_stream->tellg();

            // Initialize CryptoStream for encryption
            crypto::CryptoStream crypto;
            crypto.initialize(key_, std::vector<uint8_t>(frame.initialization_vector.begin(),
                                                       frame.initialization_vector.end()));

            // Create temporary buffer for encrypted data
            std::stringstream encrypted_buffer;
            crypto.encrypt(*frame.payload_stream, encrypted_buffer);

            // Write encrypted payload
            std::string encrypted_data = encrypted_buffer.str();
            write_bytes(output, encrypted_data.data(), encrypted_data.size());
            total_bytes += encrypted_data.size();

            // Restore stream position
            frame.payload_stream->seekg(initial_pos);
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
        // Read header fields (unencrypted)
        // Read initialization vector
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        // Read message type
        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(uint8_t));
        frame.message_type = static_cast<MessageType>(msg_type);

        // Read source_id (network byte order)
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        // Read filename_length (network byte order)
        uint32_t network_filename_length;
        read_bytes(input, &network_filename_length, sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        // Read payload_size (network byte order)
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        // Create payload stream if needed
        if (frame.payload_size > 0) {
            frame.payload_stream = std::make_shared<std::stringstream>();
        }

        // Add frame to channel before processing payload
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(frame);
        }

        // Process payload if present
        if (frame.payload_size > 0) {
            // Initialize CryptoStream for decryption
            crypto::CryptoStream crypto;
            crypto.initialize(key_, std::vector<uint8_t>(frame.initialization_vector.begin(),
                                                       frame.initialization_vector.end()));

            // Read encrypted payload into temporary buffer
            std::vector<char> encrypted_buffer(frame.payload_size);
            read_bytes(input, encrypted_buffer.data(), frame.payload_size);

            // Create stream for encrypted data
            std::stringstream encrypted_stream;
            encrypted_stream.write(encrypted_buffer.data(), frame.payload_size);
            encrypted_stream.seekg(0);

            // Decrypt payload
            crypto.decrypt(encrypted_stream, *frame.payload_stream);

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