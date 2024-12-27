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

        // Initialize crypto stream
        crypto::CryptoStream crypto;
        crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Handle filename length
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        std::stringstream filename_length_stream;
        filename_length_stream.write(reinterpret_cast<const char*>(&network_filename_length), sizeof(uint32_t));
        filename_length_stream.seekg(0);

        // Encrypt and pad filename length
        std::stringstream encrypted_length;
        crypto.encrypt(filename_length_stream, encrypted_length);
        std::string encrypted_length_data = encrypted_length.str();

        const std::size_t block_size = 16;  // AES block size
        std::size_t padded_size = get_padded_size(encrypted_length_data.size());
        encrypted_length_data.resize(padded_size, 0);
        write_bytes(output, encrypted_length_data.data(), padded_size);
        total_bytes += padded_size;

        // Process payload data if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            frame.payload_stream->seekg(0);
            const std::size_t chunk_size = 4096;
            std::vector<char> buffer(chunk_size);
            std::size_t remaining = frame.payload_size;

            while (remaining > 0 && frame.payload_stream->good()) {
                std::size_t current_chunk = std::min(remaining, chunk_size);
                frame.payload_stream->read(buffer.data(), current_chunk);
                std::streamsize bytes_read = frame.payload_stream->gcount();

                if (bytes_read > 0) {
                    std::stringstream chunk;
                    chunk.write(buffer.data(), bytes_read);
                    chunk.seekg(0);

                    std::stringstream encrypted_chunk;
                    crypto.encrypt(chunk, encrypted_chunk);
                    std::string encrypted_data = encrypted_chunk.str();

                    padded_size = get_padded_size(encrypted_data.size());
                    encrypted_data.resize(padded_size, 0);
                    write_bytes(output, encrypted_data.data(), padded_size);
                    total_bytes += padded_size;

                    remaining -= bytes_read;
                }
            }
            frame.payload_stream->seekg(0);
        }

        // For empty payloads, write a single padding block
        if (frame.payload_size == 0) {
            std::string padding_block(block_size, 0);
            write_bytes(output, padding_block.data(), block_size);
            total_bytes += block_size;
        }

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

        // Initialize crypto stream
        crypto::CryptoStream crypto;
        crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Read and decrypt filename length
        const std::size_t block_size = 16;  // AES block size
        std::vector<char> encrypted_buffer(block_size);
        read_bytes(input, encrypted_buffer.data(), block_size);

        std::stringstream encrypted_length;
        encrypted_length.write(encrypted_buffer.data(), block_size);
        encrypted_length.seekg(0);

        std::stringstream decrypted_length;
        crypto.decrypt(encrypted_length, decrypted_length);

        uint32_t network_filename_length;
        decrypted_length.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        // Initialize payload stream
        frame.payload_stream = std::make_shared<std::stringstream>();

        // Process payload data
        if (frame.payload_size > 0) {
            const std::size_t chunk_size = 4096;
            std::size_t remaining = frame.payload_size;

            while (remaining > 0 && input.good()) {
                std::size_t current_chunk = std::min(remaining, chunk_size);
                std::size_t padded_size = get_padded_size(current_chunk);

                std::vector<char> encrypted_chunk(padded_size);
                read_bytes(input, encrypted_chunk.data(), padded_size);

                std::stringstream encrypted_stream;
                encrypted_stream.write(encrypted_chunk.data(), padded_size);
                encrypted_stream.seekg(0);

                std::stringstream decrypted_stream;
                crypto.decrypt(encrypted_stream, decrypted_stream);

                frame.payload_stream->write(decrypted_stream.str().data(), current_chunk);
                remaining -= current_chunk;
            }

            if (remaining > 0) {
                throw std::runtime_error("Failed to read complete payload");
            }

            frame.payload_stream->seekg(0);
        } else {
            // For empty payloads, read and discard the padding block
            std::vector<char> padding_block(block_size);
            read_bytes(input, padding_block.data(), block_size);
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