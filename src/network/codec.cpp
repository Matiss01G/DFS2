#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include "crypto/crypto_stream.hpp"

namespace dfs {
namespace network {

void Codec::set_encryption_key(const std::vector<uint8_t>& key) {
    if (key.size() != crypto::CryptoStream::KEY_SIZE) {
        throw std::invalid_argument("Invalid key size");
    }
    encryption_key_ = key;
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        throw std::runtime_error("Invalid output stream");
    }

    if (encryption_key_.empty()) {
        throw std::runtime_error("Encryption key not set");
    }

    std::size_t total_bytes = 0;

    try {
        // Write header fields in network byte order
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, sizeof(uint8_t));
        total_bytes += sizeof(uint8_t);

        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        uint32_t network_filename_length = to_network_order(frame.filename_length);
        write_bytes(output, &network_filename_length, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        if (frame.payload_stream && frame.payload_size > 0) {
            // Write payload size
            uint64_t network_payload_size = to_network_order(frame.payload_size);
            write_bytes(output, &network_payload_size, sizeof(uint64_t));
            total_bytes += sizeof(uint64_t);

            // Initialize CryptoStream and encrypt payload directly to output
            crypto::CryptoStream crypto;
            crypto.initialize(encryption_key_, 
                            std::vector<uint8_t>(frame.initialization_vector.begin(), 
                                                frame.initialization_vector.end()));

            // Stream directly from input to encrypted output
            frame.payload_stream->seekg(0);
            crypto.encrypt(*frame.payload_stream, output);
            total_bytes += frame.payload_size;

            // Reset stream position
            frame.payload_stream->seekg(0);
        } else {
            uint64_t network_payload_size = 0;
            write_bytes(output, &network_payload_size, sizeof(uint64_t));
            total_bytes += sizeof(uint64_t);
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

    if (encryption_key_.empty()) {
        throw std::runtime_error("Encryption key not set");
    }

    MessageFrame frame;
    try {
        // Read header fields
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(uint8_t));
        frame.message_type = static_cast<MessageType>(msg_type);

        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        uint32_t network_filename_length;
        read_bytes(input, &network_filename_length, sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        if (frame.payload_size > 0) {
            // Initialize CryptoStream for decryption
            crypto::CryptoStream crypto;
            crypto.initialize(encryption_key_, 
                            std::vector<uint8_t>(frame.initialization_vector.begin(), 
                                                frame.initialization_vector.end()));

            // Create a shared stream for the decrypted data
            frame.payload_stream = std::make_shared<std::stringstream>();

            // Decrypt input directly to the payload stream
            crypto.decrypt(input, *frame.payload_stream);
            frame.payload_stream->seekg(0);

            // Use same decrypted stream for channel
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(frame);
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