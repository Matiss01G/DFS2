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

// Calculate padding size for complete blocks
std::size_t get_padded_size(std::size_t input_size) {
    const std::size_t block_size = 16;  // AES block size
    // Round up to next block size, PKCS7 padding will be handled by CryptoStream
    return ((input_size + block_size - 1) / block_size) * block_size;
}
}  // namespace

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
        // Write unencrypted header fields
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, sizeof(uint8_t));
        total_bytes += sizeof(uint8_t);

        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        // Initialize single CryptoStream instance for all encryption
        crypto::CryptoStream crypto;
        crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Prepare complete data buffer for encryption
        std::stringstream complete_data;

        // Add filename length
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        complete_data.write(reinterpret_cast<const char*>(&network_filename_length), sizeof(uint32_t));

        // Add payload if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            frame.payload_stream->seekg(0);
            complete_data << frame.payload_stream->rdbuf();
            frame.payload_stream->seekg(0);
        }

        // Encrypt all data at once (CryptoStream will handle PKCS7 padding)
        std::stringstream encrypted_stream;
        crypto.encrypt(complete_data, encrypted_stream);

        // Write encrypted data
        std::string encrypted_data = encrypted_stream.str();
        write_bytes(output, encrypted_data.data(), encrypted_data.size());
        total_bytes += encrypted_data.size();

        return total_bytes;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Serialization failed: ") + e.what());
    }
}

MessageFrame Codec::deserialize(std::istream& input, Channel& channel) {
    if (!input.good()) {
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;

    try {
        // Read unencrypted header fields
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(uint8_t));
        frame.message_type = static_cast<MessageType>(msg_type);

        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        // Initialize CryptoStream for decryption
        crypto::CryptoStream crypto;
        crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Calculate total encrypted size
        std::size_t total_encrypted_size = sizeof(uint32_t);  // Start with filename_length
        if (frame.payload_size > 0) {
            total_encrypted_size += frame.payload_size;
        }
        std::size_t padded_size = ((total_encrypted_size + 15) / 16) * 16;  // Round up to block size

        // Read the entire encrypted block
        std::vector<char> encrypted_data(padded_size);
        read_bytes(input, encrypted_data.data(), padded_size);

        // Decrypt all data at once
        std::stringstream encrypted_stream;
        encrypted_stream.write(encrypted_data.data(), padded_size);
        encrypted_stream.seekg(0);

        std::stringstream decrypted_stream;
        crypto.decrypt(encrypted_stream, decrypted_stream);

        // Read filename length
        uint32_t network_filename_length;
        decrypted_stream.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        // Handle payload if present
        if (frame.payload_size > 0) {
            frame.payload_stream = std::make_shared<std::stringstream>();

            // Read the remaining data as payload
            std::vector<char> payload_data(frame.payload_size);
            decrypted_stream.read(payload_data.data(), frame.payload_size);

            frame.payload_stream->write(payload_data.data(), frame.payload_size);
            frame.payload_stream->seekg(0);
        }

        channel.produce(frame);
        return frame;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Deserialization failed: ") + e.what());
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