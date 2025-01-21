#include "network/codec.hpp"
#include "crypto/crypto_stream.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>

namespace dfs {
namespace network {

Codec::Codec(const std::vector<uint8_t>& key, Channel& channel) 
    : key_(key), channel_(channel) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Codec with key of size: " << key_.size();
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid output stream state";
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    // Create and nitialize crypto stream with key and IV
    crypto::CryptoStream crypto;
    crypto.initialize(key_, frame.iv_);
    BOOST_LOG_TRIVIAL(info) << "Starting message frame serialization";

    try {
        // Write IV as first header
        BOOST_LOG_TRIVIAL(debug) << "Writing IV of size: " << frame.iv_.size();
        write_bytes(output, frame.iv_.data(), frame.iv_.size());
        total_bytes += frame.iv_.size();

        // Write message type
        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        BOOST_LOG_TRIVIAL(debug) << "Writing message type: " << static_cast<int>(msg_type);
        write_bytes(output, &msg_type, sizeof(msg_type));
        total_bytes += sizeof(msg_type);

        // Write payload size in network byte order
        uint64_t network_payload_size = boost::endian::native_to_big(frame.payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Writing payload size: " << frame.payload_size;
        write_bytes(output, &network_payload_size, sizeof(network_payload_size));
        total_bytes += sizeof(network_payload_size);

        // Encrypt and write filename length
        uint32_t network_filename_length = boost::endian::native_to_big(frame.filename_length);
        // Create stream for raw data
        std::stringstream filename_length_stream;
        filename_length_stream.write(reinterpret_cast<char*>(&network_filename_length), sizeof(network_filename_length));
        // Encrypt the filename length
        std::stringstream encrypted_filename_length;
        crypto.encrypt(filename_length_stream, encrypted_filename_length);
        // Write filename length
        BOOST_LOG_TRIVIAL(debug) << "Writing encrypted filename length";
        write_bytes(output, encrypted_filename_length.str().data(), encrypted_filename_length.str().size());
        total_bytes += encrypted_filename_length.str().size();

        // Encrypt and write payload if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            BOOST_LOG_TRIVIAL(debug) << "Encrypting and writing payload of size: " << frame.payload_size;
            frame.payload_stream->seekg(0);
            crypto.encrypt(*frame.payload_stream, output);
            total_bytes += frame.payload_size;
        } 

        output.flush();
        BOOST_LOG_TRIVIAL(info) << "Encrypted message frame serialization complete. Total bytes written: " << total_bytes;
        return total_bytes;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error during serialization: " << e.what();
        throw;
    }
}

MessageFrame Codec::deserialize(std::istream& input, const std::string& source_id) {
    if (!input.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream state";
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;
    frame.source_id = source_id; 
    std::size_t total_bytes = 0;

    // Create CryptoStream instance
    crypto::CryptoStream crypto;

    BOOST_LOG_TRIVIAL(info) << "Starting message frame deserialization";

    try {
        // Read IV first
        frame.iv_.resize(crypto::CryptoStream::IV_SIZE);
        BOOST_LOG_TRIVIAL(debug) << "Reading IV";
        read_bytes(input, frame.iv_.data(), frame.iv_.size());
        total_bytes += frame.iv_.size();

        // Initialize crypto stream with key and IV
        crypto.initialize(key_, frame.iv_);

        // Read message type
        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(msg_type));
        frame.message_type = static_cast<MessageType>(msg_type);
        BOOST_LOG_TRIVIAL(debug) << "Read message type: " << static_cast<int>(msg_type);
        total_bytes += sizeof(msg_type);

        // Read payload size
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(network_payload_size));
        frame.payload_size = boost::endian::big_to_native(network_payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Read payload size: " << frame.payload_size;
        total_bytes += sizeof(network_payload_size);

        // Decrypt filename length
        std::vector<char> encrypted_filename_length(crypto::CryptoStream::BLOCK_SIZE);
        read_bytes(input, encrypted_filename_length.data(), encrypted_filename_length.size());
        std::stringstream encrypted_filename_length_stream;
        encrypted_filename_length_stream.write(encrypted_filename_length.data(), encrypted_filename_length.size());
        std::stringstream decrypted_filename_length_stream;
        crypto.decrypt(encrypted_filename_length_stream, decrypted_filename_length_stream);
        uint32_t network_filename_length;
        decrypted_filename_length_stream.read(reinterpret_cast<char*>(&network_filename_length), sizeof(network_filename_length));
        frame.filename_length = boost::endian::big_to_native(network_filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Read decrypted filename length: " << frame.filename_length;
        total_bytes += encrypted_filename_length.size();

        frame.payload_stream = std::make_shared<std::stringstream>();

        channel_.produce(frame); // Use the member variable channel_

        // Decrypt payload if present
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(debug) << "Decrypting payload of size: " << frame.payload_size;
            crypto.decrypt(input, *frame.payload_stream);
            total_bytes += frame.payload_size;
            frame.payload_stream->seekg(0);
        }

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
}

void Codec::read_bytes(std::istream& input, void* data, std::size_t size) {
    if (!input.read(static_cast<char*>(data), size)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to read " << size << " bytes from input stream";
        throw std::runtime_error("Failed to read from input stream");
    }
}

} // namespace network
} // namespace dfs