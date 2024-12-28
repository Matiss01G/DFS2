#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>

namespace dfs {
namespace network {

Codec::Codec() {
    BOOST_LOG_TRIVIAL(info) << "Initializing Codec";
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
    if (!output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid output stream state";
        throw std::runtime_error("Invalid output stream");
    }

    std::size_t total_bytes = 0;
    BOOST_LOG_TRIVIAL(info) << "Starting message frame serialization";

    try {
        // Write message type
        uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
        BOOST_LOG_TRIVIAL(debug) << "Writing message type: " << static_cast<int>(msg_type);
        write_bytes(output, &msg_type, sizeof(msg_type));
        total_bytes += sizeof(msg_type);

        // Write source ID in network byte order
        uint32_t network_source_id = to_network_order(frame.source_id);
        BOOST_LOG_TRIVIAL(debug) << "Writing source ID: " << frame.source_id;
        write_bytes(output, &network_source_id, sizeof(network_source_id));
        total_bytes += sizeof(network_source_id);

        // Write payload size in network byte order
        uint64_t network_payload_size = to_network_order(frame.payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Writing payload size: " << frame.payload_size;
        write_bytes(output, &network_payload_size, sizeof(network_payload_size));
        total_bytes += sizeof(network_payload_size);

        // Write filename length in network byte order
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Writing filename length: " << frame.filename_length;
        write_bytes(output, &network_filename_length, sizeof(network_filename_length));
        total_bytes += sizeof(network_filename_length);

        // Write payload if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            BOOST_LOG_TRIVIAL(debug) << "Writing payload of size: " << frame.payload_size;
            frame.payload_stream->seekg(0);
            std::vector<char> buffer(frame.payload_size);
            if (!frame.payload_stream->read(buffer.data(), frame.payload_size)) {
                throw std::runtime_error("Failed to read payload data");
            }
            write_bytes(output, buffer.data(), frame.payload_size);
            total_bytes += frame.payload_size;
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

std::pair<MessageFrame, std::istream&> 
Codec::deserialize(std::istream& input) {
    if (!input.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream";
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;
    try {
        // Read message type
        read_bytes(input, &frame.message_type, sizeof(MessageType));
        BOOST_LOG_TRIVIAL(debug) << "Read message type: " << static_cast<int>(frame.message_type);

        // Read and convert source_id from network byte order
        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);
        BOOST_LOG_TRIVIAL(debug) << "Read source ID: " << frame.source_id;

        // Read and convert payload_size from network byte order
        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);
        BOOST_LOG_TRIVIAL(debug) << "Read payload size: " << frame.payload_size;

        // Read and convert filename length from network byte order
        uint32_t network_filename_length;
        read_bytes(input, &network_filename_length, sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Read filename length: " << frame.filename_length;

        // Input stream is now positioned at start of payload
        BOOST_LOG_TRIVIAL(debug) << "Input stream positioned at payload start";

        // Return the frame and input stream positioned at payload start
        return std::make_pair(frame, input);
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
        throw MessageError(std::string("Deserialization error: ") + e.what());
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