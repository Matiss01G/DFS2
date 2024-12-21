#include "network/codec.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>

namespace dfs {
namespace network {

MessageFrame Codec::deserialize(std::istream& input, Channel& channel) {
    BOOST_LOG_TRIVIAL(debug) << "Starting message frame deserialization";

    if (!input.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream state";
        throw std::runtime_error("Invalid input stream");
    }

    MessageFrame frame;
    try {
        // Read header fields
        BOOST_LOG_TRIVIAL(debug) << "Reading message frame headers";
        read_bytes(input, frame.initialization_vector.data(), frame.initialization_vector.size());
        read_bytes(input, &frame.message_type, sizeof(MessageType));

        uint32_t network_source_id;
        read_bytes(input, &network_source_id, sizeof(uint32_t));
        frame.source_id = from_network_order(network_source_id);

        uint32_t network_filename_length;
        read_bytes(input, &network_filename_length, sizeof(uint32_t));
        frame.filename_length = from_network_order(network_filename_length);

        uint64_t network_payload_size;
        read_bytes(input, &network_payload_size, sizeof(uint64_t));
        frame.payload_size = from_network_order(network_payload_size);

        // Step 1: Create iostream in MessageFrame
        BOOST_LOG_TRIVIAL(debug) << "Creating iostream in MessageFrame";
        frame.payload_stream = std::make_shared<std::stringstream>();
        if (!frame.payload_stream || !frame.payload_stream->good()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to create payload stream";
            throw std::runtime_error("Failed to create payload stream");
        }

        // Step 2: Push frame to channel
        BOOST_LOG_TRIVIAL(debug) << "Pushing frame to channel";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel.produce(frame);
        }

        // Step 3: Handle payload data after channel push
        if (frame.payload_size > 0) {
            BOOST_LOG_TRIVIAL(debug) << "Reading payload data of size: " << frame.payload_size;
            char buffer[4096];  // 4KB chunks for efficient streaming
            std::size_t remaining = frame.payload_size;

            while (remaining > 0 && input.good()) {
                std::size_t chunk_size = std::min(remaining, sizeof(buffer));
                read_bytes(input, buffer, chunk_size);

                if (!frame.payload_stream->write(buffer, chunk_size).good()) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to write to payload stream";
                    throw std::runtime_error("Failed to write to payload stream");
                }

                remaining -= chunk_size;
            }

            if (remaining > 0) {
                BOOST_LOG_TRIVIAL(error) << "Incomplete payload data read. Missing " << remaining << " bytes";
                throw std::runtime_error("Failed to read complete payload");
            }

            // Reset stream position for reading
            frame.payload_stream->seekg(0);
            BOOST_LOG_TRIVIAL(debug) << "Payload data read complete";
        }

        BOOST_LOG_TRIVIAL(debug) << "Message frame deserialization complete";
        return frame;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
        throw;
    }
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

    // Write message type (single byte)
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

    // Stream payload data if present
    if (frame.payload_size > 0 && frame.payload_stream) {
      char buffer[4096];  // 4KB chunks for efficient streaming
      std::size_t remaining = frame.payload_size;

      while (remaining > 0 && frame.payload_stream->good()) {
        std::size_t chunk_size = std::min(remaining, sizeof(buffer));
        frame.payload_stream->read(buffer, chunk_size);
        std::size_t bytes_read = frame.payload_stream->gcount();

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
    }

    return total_bytes;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Serialization error: " << e.what();
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