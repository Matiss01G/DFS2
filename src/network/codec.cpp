#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace dfs {
namespace network {

namespace {
std::vector<uint8_t> to_vector(const std::array<uint8_t, 16>& arr) {
    return std::vector<uint8_t>(arr.begin(), arr.end());
}
}  // namespace

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
        // Write header fields
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

        // Write filename length
        uint32_t network_filename_length = to_network_order(frame.filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Writing filename length: " << frame.filename_length << " (4 bytes)";
        write_bytes(output, &network_filename_length, sizeof(network_filename_length));
        total_bytes += sizeof(network_filename_length);

        // Handle payload if present
        if (frame.payload_size > 0 && frame.payload_stream) {
            BOOST_LOG_TRIVIAL(info) << "Processing payload of size: " << frame.payload_size << " bytes";

            // Save original position
            auto original_pos = frame.payload_stream->tellg();
            frame.payload_stream->seekg(0);

            // Read and write payload directly
            std::vector<char> payload_buffer(frame.payload_size);
            if (!frame.payload_stream->read(payload_buffer.data(), frame.payload_size)) {
                BOOST_LOG_TRIVIAL(error) << "Failed to read payload data";
                throw std::runtime_error("Failed to read payload data");
            }

            BOOST_LOG_TRIVIAL(debug) << "Writing payload (" << frame.payload_size << " bytes)";
            write_bytes(output, payload_buffer.data(), frame.payload_size);
            total_bytes += frame.payload_size;

            // Restore original position
            frame.payload_stream->clear();
            frame.payload_stream->seekg(original_pos);
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
        // Read header fields
        BOOST_LOG_TRIVIAL(debug) << "Reading initialization vector";
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());
        total_bytes += frame.initialization_vector.size();

        uint8_t msg_type;
        read_bytes(input, &msg_type, sizeof(msg_type));
        frame.message_type = static_cast<MessageType>(msg_type);
        BOOST_LOG_TRIVIAL(debug) << "Read message type: " << static_cast<int>(msg_type) << " (1 byte)";
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

        // Read filename length
        uint32_t network_filename_length;
        read_bytes(input, &network_filename_length, sizeof(network_filename_length));
        frame.filename_length = from_network_order(network_filename_length);
        BOOST_LOG_TRIVIAL(debug) << "Read filename length: " << frame.filename_length << " (4 bytes)";
        total_bytes += sizeof(network_filename_length);

        // Initialize payload stream
        frame.payload_stream = std::make_shared<std::stringstream>();

        // Handle payload if present
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(info) << "Processing payload of size: " << frame.payload_size << " bytes";

            // Read payload directly
            std::vector<char> payload_buffer(frame.payload_size);
            read_bytes(input, payload_buffer.data(), frame.payload_size);
            total_bytes += frame.payload_size;
            BOOST_LOG_TRIVIAL(debug) << "Read payload buffer (" << frame.payload_size << " bytes)";

            // Store payload in stream
            frame.payload_stream->write(payload_buffer.data(), frame.payload_size);
            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(debug) << "Successfully stored payload";
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