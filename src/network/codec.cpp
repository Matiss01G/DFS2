#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include "crypto/crypto_stream.hpp"

namespace dfs {
namespace network {

namespace {
std::vector<uint8_t> to_vector(const std::array<uint8_t, 16>& arr) {
    return std::vector<uint8_t>(arr.begin(), arr.end());
}

std::size_t get_padded_size(std::size_t input_size) {
    const std::size_t block_size = 16;  // AES block size
    return ((input_size + block_size - 1) / block_size) * block_size;
}
}  // namespace

Codec::Codec(const std::vector<uint8_t>& encryption_key) : key_(encryption_key) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Codec with key size: " << encryption_key.size() << " bytes";
    if (encryption_key.size() != 32) {
        throw std::invalid_argument("Encryption key must be 32 bytes (256 bits)");
    }
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid output stream state";
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;
    BOOST_LOG_TRIVIAL(info) << "Starting message frame serialization";

    try {
        // Write unencrypted header fields
        BOOST_LOG_TRIVIAL(debug) << "Writing initialization vector (" << frame.initialization_vector.size() << " bytes)";
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        BOOST_LOG_TRIVIAL(debug) << "Writing message type: " << static_cast<int>(msg_type) << " (1 byte)";
        write_bytes(output, &msg_type, sizeof(msg_type));
        total_bytes += sizeof(msg_type);

        uint32_t network_source_id = to_network_order(frame.source_id);
        BOOST_LOG_TRIVIAL(debug) << "Writing source ID: " << frame.source_id << " (4 bytes)";
        write_bytes(output, &network_source_id, sizeof(network_source_id));
        total_bytes += sizeof(network_source_id);

        uint64_t network_payload_size = to_network_order(frame.payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Writing payload size: " << frame.payload_size << " (8 bytes)";
        write_bytes(output, &network_payload_size, sizeof(network_payload_size));
        total_bytes += sizeof(network_payload_size);

        // Set up crypto for filename length
        BOOST_LOG_TRIVIAL(debug) << "Setting up encryption for filename length";
        crypto::CryptoStream length_crypto;
        length_crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Encrypt filename length
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        {
            std::stringstream length_stream;
            length_stream.write(reinterpret_cast<const char*>(&network_filename_length), 
                              sizeof(network_filename_length));
            length_stream.seekg(0);

            std::stringstream encrypted_length;
            length_crypto.encrypt(length_stream, encrypted_length);

            std::string length_data = encrypted_length.str();
            std::size_t padded_length_size = get_padded_size(length_data.size());
            length_data.resize(padded_length_size, 0);
            BOOST_LOG_TRIVIAL(debug) << "Writing encrypted filename length: " << frame.filename_length 
                                   << " (padded to " << padded_length_size << " bytes)";
            write_bytes(output, length_data.data(), padded_length_size);
            total_bytes += padded_length_size;
        }

        // Handle payload if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            BOOST_LOG_TRIVIAL(info) << "Processing payload of size: " << frame.payload_size << " bytes";

            // Save original position
            auto original_pos = frame.payload_stream->tellg();
            frame.payload_stream->seekg(0);

            // Read payload into buffer
            std::vector<char> payload_buffer(frame.payload_size);
            if (!frame.payload_stream->read(payload_buffer.data(), frame.payload_size)) {
                BOOST_LOG_TRIVIAL(error) << "Failed to read payload data";
                throw std::runtime_error("Failed to read payload data");
            }

            // Set up crypto for payload
            BOOST_LOG_TRIVIAL(debug) << "Setting up encryption for payload";
            crypto::CryptoStream payload_crypto;
            payload_crypto.initialize(key_, to_vector(frame.initialization_vector));

            // Encrypt payload
            {
                std::stringstream payload_stream;
                payload_stream.write(payload_buffer.data(), frame.payload_size);
                payload_stream.seekg(0);

                std::stringstream encrypted_stream;
                payload_crypto.encrypt(payload_stream, encrypted_stream);

                std::string encrypted_data = encrypted_stream.str();
                std::size_t padded_size = get_padded_size(encrypted_data.size());
                encrypted_data.resize(padded_size, 0);
                BOOST_LOG_TRIVIAL(debug) << "Writing encrypted payload (padded from " 
                                       << encrypted_data.size() << " to " << padded_size << " bytes)";
                write_bytes(output, encrypted_data.data(), padded_size);
                total_bytes += padded_size;
            }

            // Restore original position
            frame.payload_stream->clear();
            frame.payload_stream->seekg(original_pos);
        } else {
            // Write padding block for empty payload
            std::vector<char> padding(16, 0);
            BOOST_LOG_TRIVIAL(debug) << "Writing padding block for empty payload (16 bytes)";
            write_bytes(output, padding.data(), padding.size());
            total_bytes += padding.size();
        }

        output.flush();
        BOOST_LOG_TRIVIAL(info) << "Message frame serialization complete. Total bytes written: " << total_bytes;
        return total_bytes;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error during serialization: " << e.what();
        throw;
    }
}

MessageFrame Codec::deserialize(std::istream& input, Channel& channel) {
    if (!input.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream state";
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;
    std::size_t total_bytes = 0;
    BOOST_LOG_TRIVIAL(info) << "Starting message frame deserialization";

    try {
        // Read unencrypted header fields
        BOOST_LOG_TRIVIAL(debug) << "Reading initialization vector";
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(msg_type));
        frame.message_type = static_cast<MessageType>(msg_type);
        BOOST_LOG_TRIVIAL(debug) << "Read message type: " << static_cast<int>(msg_type) 
                               << " (1 byte)";
        total_bytes += sizeof(msg_type);

        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(network_source_id));
        frame.source_id = from_network_order(network_source_id);
        BOOST_LOG_TRIVIAL(debug) << "Read source ID: " << frame.source_id << " (4 bytes)";
        total_bytes += sizeof(network_source_id);

        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(network_payload_size));
        frame.payload_size = from_network_order(network_payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Read payload size: " << frame.payload_size << " (8 bytes)";
        total_bytes += sizeof(network_payload_size);

        // Set up crypto for filename length
        BOOST_LOG_TRIVIAL(debug) << "Setting up decryption for filename length";
        crypto::CryptoStream length_crypto;
        length_crypto.initialize(key_, to_vector(frame.initialization_vector));

        // Decrypt filename length
        {
            std::vector<char> encrypted_length_buffer(16);
            read_bytes(input, encrypted_length_buffer.data(), encrypted_length_buffer.size());
            total_bytes += encrypted_length_buffer.size();
            BOOST_LOG_TRIVIAL(debug) << "Read encrypted filename length buffer (" 
                                   << encrypted_length_buffer.size() << " bytes)";

            std::stringstream encrypted_length;
            encrypted_length.write(encrypted_length_buffer.data(), encrypted_length_buffer.size());
            encrypted_length.seekg(0);

            std::stringstream decrypted_length;
            length_crypto.decrypt(encrypted_length, decrypted_length);

            uint32_t network_filename_length;
            decrypted_length.seekg(0);
            if (!decrypted_length.read(reinterpret_cast<char*>(&network_filename_length), 
                                    sizeof(network_filename_length))) {
                BOOST_LOG_TRIVIAL(error) << "Failed to read decrypted filename length";
                throw std::runtime_error("Failed to read decrypted filename length");
            }
            frame.filename_length = from_network_order(network_filename_length);
            BOOST_LOG_TRIVIAL(debug) << "Decrypted filename length: " << frame.filename_length;
        }

        // Initialize payload stream
        frame.payload_stream = std::make_shared<std::stringstream>();

        // Handle payload if present
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(info) << "Processing payload of size: " << frame.payload_size << " bytes";

            // Set up crypto for payload
            BOOST_LOG_TRIVIAL(debug) << "Setting up decryption for payload";
            crypto::CryptoStream payload_crypto;
            payload_crypto.initialize(key_, to_vector(frame.initialization_vector));

            // Read encrypted payload
            std::size_t padded_size = get_padded_size(frame.payload_size);
            std::vector<char> encrypted_buffer(padded_size);
            read_bytes(input, encrypted_buffer.data(), padded_size);
            total_bytes += padded_size;
            BOOST_LOG_TRIVIAL(debug) << "Read encrypted payload buffer (padded size: " 
                                   << padded_size << " bytes)";

            // Decrypt payload
            {
                std::stringstream encrypted_stream;
                encrypted_stream.write(encrypted_buffer.data(), padded_size);
                encrypted_stream.seekg(0);

                std::stringstream decrypted_stream;
                payload_crypto.decrypt(encrypted_stream, decrypted_stream);

                std::vector<char> decrypted_buffer(frame.payload_size);
                decrypted_stream.seekg(0);
                if (!decrypted_stream.read(decrypted_buffer.data(), frame.payload_size)) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to read decrypted payload";
                    throw std::runtime_error("Failed to read decrypted payload");
                }

                frame.payload_stream->write(decrypted_buffer.data(), frame.payload_size);
                frame.payload_stream->seekg(0);
                BOOST_LOG_TRIVIAL(debug) << "Successfully decrypted and stored payload";
            }
        } else {
            // Skip padding block for empty payload
            std::vector<char> padding(16);
            read_bytes(input, padding.data(), padding.size());
            total_bytes += padding.size();
            BOOST_LOG_TRIVIAL(debug) << "Skipped padding block for empty payload (16 bytes)";
        }

        channel.produce(frame);
        BOOST_LOG_TRIVIAL(info) << "Message frame deserialization complete. Total bytes read: " << total_bytes;
        return frame;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error during deserialization: " << e.what();
        throw;
    }
}

void Codec::write_bytes(std::ostream& output, const void* data, std::size_t size) {
    if (!output.write(static_cast<const char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to write " << size << " bytes to output stream";
        throw std::runtime_error("Failed to write to output stream");
    }
    BOOST_LOG_TRIVIAL(trace) << "Successfully wrote " << size << " bytes to output stream";
}

void Codec::read_bytes(std::istream& input, void* data, std::size_t size) {
    if (!input.read(static_cast<char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to read " << size << " bytes from input stream";
        throw std::runtime_error("Failed to read from input stream");
    }
    BOOST_LOG_TRIVIAL(trace) << "Successfully read " << size << " bytes from input stream";
}

} // namespace network
} // namespace dfs