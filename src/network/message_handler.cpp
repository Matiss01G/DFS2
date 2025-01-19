#include "network/message_frame.hpp"
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

std::size_t MessageHandler::serialize(const MessageFrame& frame, std::istream& data, std::ostream& output) {
    if (!data.good()) {
        throw MessageError("Invalid input data stream");
    }
    if (!output.good()) {
        throw MessageError("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        // Write initialization vector
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        // Write message type (single byte, no conversion needed)
        write_bytes(output, &frame.message_type, sizeof(MessageType));
        total_bytes += sizeof(MessageType);

        // Write source_id string length and content
        uint32_t source_id_length = frame.source_id.length();
        uint32_t network_source_id_length = boost::endian::native_to_big(source_id_length);
        write_bytes(output, &network_source_id_length, sizeof(uint32_t));
        total_bytes += sizeof(uint32_t);

        if (!frame.source_id.empty()) {
            write_bytes(output, frame.source_id.c_str(), source_id_length);
            total_bytes += source_id_length;
        }

        // Convert and write payload_size in network byte order
        uint64_t network_payload_size = boost::endian::native_to_big(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        // Write payload data
        char buffer[4096];  // 4KB chunks for efficient streaming
        std::size_t remaining = frame.payload_size;

        while (remaining > 0 && data.good()) {
            std::size_t chunk_size = std::min(remaining, sizeof(buffer));
            data.read(buffer, chunk_size);
            std::size_t bytes_read = data.gcount();

            if (bytes_read == 0) {
                throw MessageError("Unexpected end of input data stream");
            }

            write_bytes(output, buffer, bytes_read);
            total_bytes += bytes_read;
            remaining -= bytes_read;
        }

        if (remaining > 0) {
            throw MessageError("Failed to read complete payload from input stream");
        }

        return total_bytes;
    }
    catch (const std::exception& e) {
        throw MessageError(std::string("Serialization error: ") + e.what());
    }
}

std::pair<MessageFrame, std::istream&> 
MessageHandler::deserialize(std::istream& input) {
    if (!input.good()) {
        throw MessageError("Invalid input stream");
    }

    MessageFrame frame;
    try {
        // Read initialization vector
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());

        // Read message type
        read_bytes(input, &frame.message_type, sizeof(MessageType));

        // Read source_id string length and content
        uint32_t network_source_id_length;
        read_bytes(input, &network_source_id_length, sizeof(uint32_t));
        uint32_t source_id_length = boost::endian::big_to_native(network_source_id_length);

        if (source_id_length > 0) {
            std::vector<char> source_id_buffer(source_id_length + 1, '\0');
            read_bytes(input, source_id_buffer.data(), source_id_length);
            frame.source_id = std::string(source_id_buffer.data(), source_id_length);
        }

        // Read and convert payload_size from network byte order
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = boost::endian::big_to_native(network_payload_size);

        // Return the frame and input stream positioned at payload start
        return std::make_pair(frame, input);
    }
    catch (const std::exception& e) {
        throw MessageError(std::string("Deserialization error: ") + e.what());
    }
}

void MessageHandler::write_bytes(std::ostream& output, const void* data, std::size_t size) {
    if (!output.write(static_cast<const char*>(data), size)) {
        throw MessageError("Failed to write to output stream");
    }
}

void MessageHandler::read_bytes(std::istream& input, void* data, std::size_t size) {
    if (!input.read(static_cast<char*>(data), size)) {
        throw MessageError("Failed to read from input stream");
    }
}

uint32_t MessageHandler::to_network_order(uint32_t host_value) {
    return boost::endian::native_to_big(host_value);
}

uint64_t MessageHandler::to_network_order(uint64_t host_value) {
    return boost::endian::native_to_big(host_value);
}

uint32_t MessageHandler::from_network_order(uint32_t network_value) {
    return boost::endian::big_to_native(network_value);
}

uint64_t MessageHandler::from_network_order(uint64_t network_value) {
    return boost::endian::big_to_native(network_value);
}

} // namespace network
} // namespace dfs