#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include "crypto/crypto_stream.hpp"  // Add this include

namespace dfs {
namespace network {

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        // Create a non-const copy of the frame to modify the IV
        MessageFrame frame_with_iv = frame;

        // Generate and set new IV
        crypto::CryptoStream crypto;
        frame_with_iv.initialization_vector = crypto.generate_IV();

        // Write initialization vector (fixed size array)
        write_bytes(output, frame_with_iv.initialization_vector.data(), frame_with_iv.initialization_vector.size());
        total_bytes += frame_with_iv.initialization_vector.size();

        // Write message type (underlying uint8_t)
        write_bytes(output, &frame_with_iv.message_type, sizeof(MessageType));
        total_bytes += sizeof(MessageType);

        // Convert and write source_id in network byte order
        uint32_t network_source_id = to_network_order(frame_with_iv.source_id);
        write_bytes(output, &network_source_id, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Convert and write filename_length in network byte order
        uint32_t network_filename_length = to_network_order(frame_with_iv.filename_length);
        write_bytes(output, &network_filename_length, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        // Convert and write payload_size in network byte order
        uint64_t network_payload_size = to_network_order(frame_with_iv.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        // Stream payload data if present
        if (frame_with_iv.payload_size > 0 && frame_with_iv.payload_stream) {
            char buffer[4096];  // 4KB chunks for efficient streaming
            std::size_t remaining = frame_with_iv.payload_size;

            // Reset stream position to beginning
            frame_with_iv.payload_stream->seekg(0, std::ios::beg);

            while (remaining > 0 && frame_with_iv.payload_stream->good()) {
                std::size_t chunk_size = std::min(remaining, sizeof(buffer));
                frame_with_iv.payload_stream->read(buffer, chunk_size);
                std::size_t bytes_read = frame_with_iv.payload_stream->gcount();

                if (bytes_read == 0) {
                    throw std::runtime_error("Unexpected end of payload stream");
                }

                write_bytes(output, buffer, bytes_read);
                total_bytes += bytes_read;
                remaining -= bytes_read;
            }

            if (remaining > 0) {
                throw std::runtime_error("Failed to write complete payload");
            }

            // Reset stream position back to beginning
            frame_with_iv.payload_stream->seekg(0, std::ios::beg);
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

        // If payload exists, create stream and read data
        if (frame.payload_size > 0) {
            auto payload_stream = std::make_shared<std::stringstream>();
            char buffer[4096];  // 4KB chunks for efficient streaming
            std::size_t remaining = frame.payload_size;

            while (remaining > 0 && input.good()) {
                std::size_t chunk_size = std::min(remaining, sizeof(buffer));
                read_bytes(input, buffer, chunk_size);
                payload_stream->write(buffer, chunk_size);
                remaining -= chunk_size;
            }

            if (remaining > 0) {
                throw std::runtime_error("Failed to read complete payload");
            }

            // Reset stream position to beginning
            payload_stream->seekg(0, std::ios::beg);
            frame.payload_stream = payload_stream;
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

        // Thread-safe channel push with proper stream handling
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