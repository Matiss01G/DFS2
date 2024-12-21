#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include <boost/endian/conversion.hpp>

namespace dfs {
namespace network {

uint32_t Codec::to_network_order(uint32_t host_value) {
    return boost::endian::native_to_big(host_value);
}

uint64_t Codec::to_network_order(uint64_t host_value) {
    return boost::endian::native_to_big(host_value);
}

uint32_t Codec::from_network_order(uint32_t network_value) {
    return boost::endian::big_to_native(network_value);
}

uint64_t Codec::from_network_order(uint64_t network_value) {
    return boost::endian::big_to_native(network_value);
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;

    try {
        // Write initialization vector
        write_bytes(output, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        // Write message type
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

        // Convert and write payload_size in network byte order
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        write_bytes(output, &network_payload_size, sizeof(uint64_t));
        total_bytes += sizeof(uint64_t);

        // Write payload data if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            // Create a copy of the stringstream to preserve its state
            std::string payload_data = frame.payload_stream->str();
            write_bytes(output, payload_data.c_str(), payload_data.size());
            total_bytes += payload_data.size();
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
            std::vector<char> buffer(frame.payload_size);
            read_bytes(input, buffer.data(), frame.payload_size);
            payload_stream->write(buffer.data(), frame.payload_size);
            frame.payload_stream = payload_stream;
        }

        // Thread-safe channel push
        {
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