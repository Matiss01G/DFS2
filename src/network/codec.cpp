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
        // Generate and set new IV in the original frame
        crypto::CryptoStream crypto;
        auto& mutable_frame = const_cast<MessageFrame&>(frame);
        mutable_frame.initialization_vector = crypto.generate_IV();

        // Write initialization vector (fixed size array)
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        // Write message type (underlying uint8_t)
        write_bytes(output, &frame.message_type, sizeof(MessageType));
        total_bytes += sizeof(MessageType);

        // Convert and write source_id in network byte order
        uint32_t network_source_id = to_network_order(frame.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Convert and write filename_length in network byte order
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        write_bytes(output, &network_filename_length, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Initialize crypto for encryption
        std::vector<uint8_t> iv_vec(frame.initialization_vector.begin(), 
                                  frame.initialization_vector.end());
        crypto.initialize(encryption_key_, iv_vec);

        if (frame.payload_size > 0 && frame.payload_stream) {
            // Write the payload size first
            uint64_t network_payload_size = to_network_order(frame.payload_size);
            write_bytes(output, &network_payload_size, sizeof(uint64_t));
            total_bytes += sizeof(uint64_t);

            // Process the payload stream directly through encryption
            frame.payload_stream->seekg(0, std::ios::beg);
            crypto.processStream(*frame.payload_stream, output);
            total_bytes += frame.payload_size;

            // Reset original stream position
            frame.payload_stream->seekg(0, std::ios::beg);
        } else {
            // Write zero payload size if no payload
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
        // Read initialization vector
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        // Read message type
        read_bytes(input, &frame.message_type, sizeof(MessageType));

        // Read and convert source_id from network byte order
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        // Read and convert filename_length from network byte order
        uint32_t network_filename_length;
        read_bytes(input, &network_filename_length, sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        // Read and convert payload_size from network byte order
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        // If payload exists, decrypt it directly to a new stream
        if (frame.payload_size > 0) {
            // Initialize crypto for decryption using frame's IV
            crypto::CryptoStream crypto;
            std::vector<uint8_t> iv_vec(frame.initialization_vector.begin(), 
                                      frame.initialization_vector.end());
            crypto.initialize(encryption_key_, iv_vec);

            auto decrypted_stream = std::make_shared<std::stringstream>();
            crypto.processStream(input, *decrypted_stream);

            // Set decrypted stream as frame's payload
            decrypted_stream->seekg(0);
            frame.payload_stream = decrypted_stream;
        }

        // Create a copy of the frame for the channel
        MessageFrame channel_frame = frame;
        if (frame.payload_stream) {
            auto channel_payload = std::make_shared<std::stringstream>();
            *channel_payload << frame.payload_stream->rdbuf();
            channel_frame.payload_stream = channel_payload;
            // Reset original stream position
            frame.payload_stream->seekg(0, std::ios::beg);
        }

        // Thread-safe channel push
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(channel_frame);
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