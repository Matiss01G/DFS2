#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include "crypto/crypto_stream.hpp"  

namespace dfs {
namespace network {

namespace {
std::vector<uint8_t> to_vector(const std::array<uint8_t, 16>& arr) {
    BOOST_LOG_TRIVIAL(debug) << "Converting IV array to vector";
    return std::vector<uint8_t>(arr.begin(), arr.end());
}

std::size_t get_padded_size(std::size_t input_size) {
    const std::size_t block_size = 16;  // AES block size
    std::size_t padded_size = ((input_size + block_size - 1) / block_size) * block_size;
    BOOST_LOG_TRIVIAL(debug) << "Calculated padded size: " << padded_size << " for input size: " << input_size;
    return padded_size;
}
}  // namespace

Codec::Codec(const std::vector<uint8_t>& encryption_key) : key_(encryption_key) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Codec with key size: " << encryption_key.size();
    if (encryption_key.size() != 32) {
        BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << encryption_key.size() << " bytes. Expected 32 bytes";
        throw std::invalid_argument("Encryption key must be 32 bytes (256 bits)");
    }
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    BOOST_LOG_TRIVIAL(info) << "Starting message frame serialization";
    if (!output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid output stream state";
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        BOOST_LOG_TRIVIAL(debug) << "Writing unencrypted header fields";
        // Write unencrypted header fields
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, sizeof(uint8_t));
        total_bytes += sizeof(uint8_t);
        BOOST_LOG_TRIVIAL(debug) << "Message type written: " << static_cast<int>(msg_type);

        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);
        BOOST_LOG_TRIVIAL(debug) << "Source ID written: " << frame.source_id;

        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);
        BOOST_LOG_TRIVIAL(debug) << "Payload size written: " << frame.payload_size;

        // Initialize CryptoStream for filename length encryption
        crypto::CryptoStream crypto_length;
        crypto_length.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(info) << "Initialized CryptoStream for filename length";

        // Handle filename length
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        std::string length_data(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        std::stringstream filename_length_stream(length_data);
        BOOST_LOG_TRIVIAL(debug) << "Filename length prepared for encryption: " << frame.filename_length;

        std::stringstream encrypted_length;
        crypto_length.encrypt(filename_length_stream, encrypted_length);
        BOOST_LOG_TRIVIAL(debug) << "Filename length encrypted";

        std::string encrypted_length_data = encrypted_length.str();
        const std::size_t block_size = 16;
        std::size_t padded_size = get_padded_size(encrypted_length_data.size());
        encrypted_length_data.resize(padded_size, 0);
        write_bytes(output, encrypted_length_data.data(), padded_size);
        total_bytes += padded_size;

        // Initialize new CryptoStream for payload encryption
        crypto::CryptoStream crypto_payload;
        crypto_payload.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(info) << "Initialized CryptoStream for payload";

        // Process payload data if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            BOOST_LOG_TRIVIAL(debug) << "Processing payload of size: " << frame.payload_size;
            frame.payload_stream->seekg(0);

            // Read payload into string
            std::string payload_data;
            payload_data.resize(frame.payload_size);
            frame.payload_stream->read(&payload_data[0], frame.payload_size);

            // Create stream from payload data
            std::stringstream payload_stream(payload_data);
            std::stringstream encrypted_stream;

            // Encrypt payload
            crypto_payload.encrypt(payload_stream, encrypted_stream);
            BOOST_LOG_TRIVIAL(debug) << "Payload encrypted successfully";

            std::string encrypted_data = encrypted_stream.str();
            padded_size = get_padded_size(encrypted_data.size());
            encrypted_data.resize(padded_size, 0);
            write_bytes(output, encrypted_data.data(), padded_size);
            total_bytes += padded_size;

            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(debug) << "Encrypted payload written, size: " << padded_size;
        } else {
            // For empty payloads, write a single padding block
            BOOST_LOG_TRIVIAL(debug) << "Writing padding block for empty payload";
            std::string padding_block(block_size, 0);
            write_bytes(output, padding_block.data(), block_size);
            total_bytes += block_size;
        }

        BOOST_LOG_TRIVIAL(info) << "Message frame serialization completed successfully, total bytes: " << total_bytes;
        return total_bytes;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Serialization failed: " << e.what();
        throw std::runtime_error(std::string("Serialization failed: ") + e.what());
    }
}

MessageFrame Codec::deserialize(std::istream& input, Channel& channel) {
    BOOST_LOG_TRIVIAL(info) << "Starting message frame deserialization";
    if (!input.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream state";
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;

    try {
        BOOST_LOG_TRIVIAL(debug) << "Reading unencrypted header fields";
        // Read unencrypted header fields
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(uint8_t));
        frame.message_type = static_cast<MessageType>(msg_type);
        BOOST_LOG_TRIVIAL(debug) << "Message type read: " << static_cast<int>(msg_type);

        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);
        BOOST_LOG_TRIVIAL(debug) << "Source ID read: " << frame.source_id;

        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Payload size read: " << frame.payload_size;

        // Initialize CryptoStream for filename length decryption
        crypto::CryptoStream crypto_length;
        crypto_length.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(info) << "Initialized CryptoStream for filename length decryption";

        // Read and decrypt filename length
        const std::size_t block_size = 16;
        std::vector<char> encrypted_length_buffer(block_size);
        read_bytes(input, encrypted_length_buffer.data(), block_size);
        BOOST_LOG_TRIVIAL(debug) << "Read encrypted filename length block";

        std::stringstream encrypted_length;
        encrypted_length.write(encrypted_length_buffer.data(), block_size);
        std::stringstream decrypted_length;
        crypto_length.decrypt(encrypted_length, decrypted_length);
        BOOST_LOG_TRIVIAL(debug) << "Filename length decrypted";

        uint32_t network_filename_length;
        decrypted_length.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Filename length value: " << frame.filename_length;

        // Initialize payload stream
        frame.payload_stream = std::make_shared<std::stringstream>();

        // Initialize new CryptoStream for payload decryption
        crypto::CryptoStream crypto_payload;
        crypto_payload.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(info) << "Initialized CryptoStream for payload decryption";

        // Process payload data
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(debug) << "Processing payload of size: " << frame.payload_size;
            std::size_t padded_size = get_padded_size(frame.payload_size);
            std::vector<char> encrypted_data(padded_size);
            read_bytes(input, encrypted_data.data(), padded_size);
            BOOST_LOG_TRIVIAL(debug) << "Read encrypted payload block of size: " << padded_size;

            std::stringstream encrypted_stream;
            encrypted_stream.write(encrypted_data.data(), padded_size);
            std::stringstream decrypted_stream;
            crypto_payload.decrypt(encrypted_stream, decrypted_stream);
            BOOST_LOG_TRIVIAL(debug) << "Payload decrypted successfully";

            // Read exactly payload_size bytes to avoid including padding
            std::string decrypted_data;
            decrypted_data.resize(frame.payload_size);
            decrypted_stream.read(&decrypted_data[0], frame.payload_size);

            frame.payload_stream->write(decrypted_data.data(), frame.payload_size);
            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(debug) << "Decrypted payload written to stream";
        } else {
            // For empty payloads, read and discard the padding block
            BOOST_LOG_TRIVIAL(debug) << "Reading padding block for empty payload";
            std::vector<char> padding_block(block_size);
            read_bytes(input, padding_block.data(), block_size);
        }

        BOOST_LOG_TRIVIAL(info) << "Producing message frame to channel";
        channel.produce(frame);

        BOOST_LOG_TRIVIAL(info) << "Message frame deserialization completed successfully";
        return frame;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization failed: " << e.what();
        throw std::runtime_error(std::string("Deserialization failed: ") + e.what());
    }
}

void Codec::write_bytes(std::ostream& output, const void* data, std::size_t size) {
    BOOST_LOG_TRIVIAL(debug) << "Writing " << size << " bytes to output stream";
    if (!output.write(static_cast<const char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to write to output stream";
        throw std::runtime_error("Failed to write to output stream");
    }
}

void Codec::read_bytes(std::istream& input, void* data, std::size_t size) {
    BOOST_LOG_TRIVIAL(debug) << "Reading " << size << " bytes from input stream";
    if (!input.read(static_cast<char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to read from input stream";
        throw std::runtime_error("Failed to read from input stream");
    }
}

} // namespace network
} // namespace dfs