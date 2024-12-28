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
    BOOST_LOG_TRIVIAL(debug) << "Padding calculation: input_size=" << input_size 
                            << ", block_size=" << block_size 
                            << ", padded_size=" << padded_size;
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
    std::size_t field_size = 0;

    try {
        // Write unencrypted header fields
        BOOST_LOG_TRIVIAL(debug) << "Writing header fields";

        // IV field
        field_size = frame.initialization_vector.size();
        write_bytes(output, frame.initialization_vector.data(), field_size);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Wrote IV field: " << field_size << " bytes, total=" << total_bytes;

        // Message type field
        field_size = sizeof(uint8_t);
        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        write_bytes(output, &msg_type, field_size);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Wrote message type field: " << field_size 
                                << " bytes, type=" << static_cast<int>(msg_type)
                                << ", total=" << total_bytes;

        // Source ID field
        field_size = sizeof(uint32_t);
        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, field_size);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Wrote source ID field: " << field_size 
                                << " bytes, ID=" << frame.source_id
                                << ", total=" << total_bytes;

        // Payload size field
        field_size = sizeof(uint64_t);
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, field_size);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Wrote payload size field: " << field_size 
                                << " bytes, size=" << frame.payload_size
                                << ", total=" << total_bytes;

        // Initialize CryptoStream for filename length encryption
        crypto::CryptoStream crypto_length;
        crypto_length.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(debug) << "Initialized CryptoStream for filename length encryption";

        // Handle filename length encryption
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        std::string length_data(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        std::stringstream filename_length_stream(length_data);
        BOOST_LOG_TRIVIAL(debug) << "Preparing filename length: " << frame.filename_length 
                                << " (" << sizeof(uint32_t) << " bytes)";

        // Encrypt filename length
        std::stringstream encrypted_length;
        crypto_length.encrypt(filename_length_stream, encrypted_length);
        std::string encrypted_length_data = encrypted_length.str();
        std::size_t padded_length_size = get_padded_size(encrypted_length_data.size());

        BOOST_LOG_TRIVIAL(debug) << "Filename length encryption: original=" << encrypted_length_data.size() 
                                << " bytes, padded=" << padded_length_size << " bytes";

        encrypted_length_data.resize(padded_length_size, 0);
        write_bytes(output, encrypted_length_data.data(), padded_length_size);
        total_bytes += padded_length_size;
        BOOST_LOG_TRIVIAL(debug) << "Wrote encrypted filename length field: " << padded_length_size 
                                << " bytes, total=" << total_bytes;

        // Initialize CryptoStream for payload
        crypto::CryptoStream crypto_payload;
        crypto_payload.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(debug) << "Initialized CryptoStream for payload encryption";

        // Process payload
        if (frame.payload_size > 0 && frame.payload_stream) {
            BOOST_LOG_TRIVIAL(debug) << "Processing payload of size: " << frame.payload_size << " bytes";
            frame.payload_stream->seekg(0);

            std::string payload_data;
            payload_data.resize(frame.payload_size);
            frame.payload_stream->read(&payload_data[0], frame.payload_size);

            std::stringstream payload_stream(payload_data);
            std::stringstream encrypted_stream;

            crypto_payload.encrypt(payload_stream, encrypted_stream);
            BOOST_LOG_TRIVIAL(debug) << "Payload encrypted successfully";

            std::string encrypted_data = encrypted_stream.str();
            std::size_t padded_payload_size = get_padded_size(encrypted_data.size());

            BOOST_LOG_TRIVIAL(debug) << "Payload encryption: original=" << encrypted_data.size() 
                                    << " bytes, padded=" << padded_payload_size << " bytes";

            encrypted_data.resize(padded_payload_size, 0);
            write_bytes(output, encrypted_data.data(), padded_payload_size);
            total_bytes += padded_payload_size;

            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(debug) << "Wrote encrypted payload: " << padded_payload_size 
                                    << " bytes, total=" << total_bytes;
        } else {
            const std::size_t block_size = 16;
            BOOST_LOG_TRIVIAL(debug) << "Writing padding block for empty payload: " << block_size << " bytes";
            std::string padding_block(block_size, 0);
            write_bytes(output, padding_block.data(), block_size);
            total_bytes += block_size;
            BOOST_LOG_TRIVIAL(debug) << "Wrote padding block, total=" << total_bytes;
        }

        BOOST_LOG_TRIVIAL(info) << "Message frame serialization completed: total bytes written=" << total_bytes;
        return total_bytes;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Serialization failed at " << total_bytes << " bytes: " << e.what();
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
    std::size_t total_bytes = 0;
    std::size_t field_size = 0;

    try {
        // Read unencrypted header fields
        BOOST_LOG_TRIVIAL(debug) << "Reading header fields";

        // Read IV
        field_size = frame.initialization_vector.size();
        read_bytes(input, frame.initialization_vector.data(), field_size);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Read IV field: " << field_size << " bytes, total=" << total_bytes;

        // Read message type
        field_size = sizeof(uint8_t);
        uint8_t msg_type;
        read_bytes(input, &msg_type, field_size);
        frame.message_type = static_cast<MessageType>(msg_type);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Read message type field: " << field_size 
                                << " bytes, type=" << static_cast<int>(msg_type)
                                << ", total=" << total_bytes;

        // Read source ID
        field_size = sizeof(uint32_t);
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, field_size);
        frame.source_id = from_network_order(network_source_id);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Read source ID field: " << field_size 
                                << " bytes, ID=" << frame.source_id
                                << ", total=" << total_bytes;

        // Read payload size
        field_size = sizeof(uint64_t);
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, field_size);
        frame.payload_size = from_network_order(network_payload_size);
        total_bytes += field_size;
        BOOST_LOG_TRIVIAL(debug) << "Read payload size field: " << field_size 
                                << " bytes, size=" << frame.payload_size
                                << ", total=" << total_bytes;

        // Initialize CryptoStream for filename length decryption
        crypto::CryptoStream crypto_length;
        crypto_length.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(debug) << "Initialized CryptoStream for filename length decryption";

        // Read and decrypt filename length
        const std::size_t block_size = 16;
        std::vector<char> encrypted_length_buffer(block_size);
        read_bytes(input, encrypted_length_buffer.data(), block_size);
        total_bytes += block_size;
        BOOST_LOG_TRIVIAL(debug) << "Read encrypted filename length block: " << block_size 
                                << " bytes, total=" << total_bytes;

        std::stringstream encrypted_length;
        encrypted_length.write(encrypted_length_buffer.data(), block_size);
        std::stringstream decrypted_length;
        crypto_length.decrypt(encrypted_length, decrypted_length);
        BOOST_LOG_TRIVIAL(debug) << "Filename length decrypted";

        uint32_t network_filename_length;
        decrypted_length.read(reinterpret_cast<char*>(&network_filename_length), sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Decoded filename length: " << frame.filename_length;

        // Initialize payload stream
        frame.payload_stream = std::make_shared<std::stringstream>();

        // Initialize CryptoStream for payload decryption
        crypto::CryptoStream crypto_payload;
        crypto_payload.initialize(key_, to_vector(frame.initialization_vector));
        BOOST_LOG_TRIVIAL(debug) << "Initialized CryptoStream for payload decryption";

        // Process payload
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(debug) << "Processing payload of size: " << frame.payload_size << " bytes";
            std::size_t padded_size = get_padded_size(frame.payload_size);
            std::vector<char> encrypted_data(padded_size);
            read_bytes(input, encrypted_data.data(), padded_size);
            total_bytes += padded_size;
            BOOST_LOG_TRIVIAL(debug) << "Read encrypted payload block: " << padded_size 
                                    << " bytes, total=" << total_bytes;

            std::stringstream encrypted_stream;
            encrypted_stream.write(encrypted_data.data(), padded_size);
            std::stringstream decrypted_stream;
            crypto_payload.decrypt(encrypted_stream, decrypted_stream);
            BOOST_LOG_TRIVIAL(debug) << "Payload decrypted successfully";

            std::string decrypted_data;
            decrypted_data.resize(frame.payload_size);
            decrypted_stream.read(&decrypted_data[0], frame.payload_size);

            frame.payload_stream->write(decrypted_data.data(), frame.payload_size);
            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(debug) << "Decrypted payload written to stream: " << frame.payload_size << " bytes";
        } else {
            BOOST_LOG_TRIVIAL(debug) << "Reading padding block for empty payload: " << block_size << " bytes";
            std::vector<char> padding_block(block_size);
            read_bytes(input, padding_block.data(), block_size);
            total_bytes += block_size;
            BOOST_LOG_TRIVIAL(debug) << "Read padding block, total=" << total_bytes;
        }

        BOOST_LOG_TRIVIAL(info) << "Producing message frame to channel, total bytes read=" << total_bytes;
        channel.produce(frame);

        BOOST_LOG_TRIVIAL(info) << "Message frame deserialization completed successfully";
        return frame;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization failed at " << total_bytes << " bytes: " << e.what();
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