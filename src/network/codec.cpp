#include "network/codec.hpp"
#include "crypto/crypto_stream.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <stdexcept>

namespace dfs {
namespace network {

Codec::Codec(const std::vector<uint8_t>& key, Channel& channel) 
  : key_(key)
  , channel_(channel) {
  BOOST_LOG_TRIVIAL(info) << "Initializing Codec with key of size: " << key_.size();
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
  if (!output.good()) {
    BOOST_LOG_TRIVIAL(error) << "Invalid output stream state";
    throw std::runtime_error("Invalid output stream");
  }

  std::size_t total_bytes = 0;

  // Create and initialize crypto stream with key and IV
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

    // Write source id 
    BOOST_LOG_TRIVIAL(debug) << "Writing source id: " << static_cast<int>(frame.source_id);
    write_bytes(output, &frame.source_id, sizeof(frame.source_id));
    total_bytes += sizeof(frame.source_id);

    // Calculate and write payload size in network byte order
    uint64_t payload_size = 0;
    if (frame.payload_stream && frame.payload_stream->good()) {
        // Save current position
        std::streampos current_pos = frame.payload_stream->tellg();

        // Seek to end to get size
        frame.payload_stream->seekg(0, std::ios::end);
        payload_size = frame.payload_stream->tellg();

        // Restore position
        frame.payload_stream->seekg(current_pos);

        // Update frame's payload size
        frame.payload_size = payload_size;
    }

    uint64_t network_payload_size = boost::endian::native_to_big(payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Writing payload size: " << payload_size;
    write_bytes(output, &network_payload_size, sizeof(network_payload_size));
    total_bytes += sizeof(network_payload_size);

    // Encrypt and write payload if present
    if (payload_size > 0 && frame.payload_stream && frame.payload_stream->good()) {
      BOOST_LOG_TRIVIAL(debug) << "Encrypting and writing payload of size: " << payload_size;
      frame.payload_stream->seekg(0);
      std::stringstream encrypted_payload;
      crypto.encrypt(*frame.payload_stream, encrypted_payload);
      output << encrypted_payload.rdbuf();
      total_bytes += payload_size;
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

MessageFrame Codec::deserialize(std::istream& input) {
  if (!input.good()) {
    BOOST_LOG_TRIVIAL(error) << "Invalid input stream state";
    throw std::runtime_error("Invalid input stream");
  }

  MessageFrame frame;
  std::size_t total_bytes = 0;

  try {
    // Read IV first
    frame.iv_.resize(crypto::CryptoStream::IV_SIZE);
    BOOST_LOG_TRIVIAL(debug) << "Reading IV";
    read_bytes(input, frame.iv_.data(), frame.iv_.size());
    total_bytes += frame.iv_.size();

    // Initialize crypto stream with key and IV
    crypto::CryptoStream crypto;
    crypto.initialize(key_, frame.iv_);

    // Read message type
    uint8_t msg_type;
    read_bytes(input, &msg_type, sizeof(msg_type));
    frame.message_type = static_cast<MessageType>(msg_type);
    BOOST_LOG_TRIVIAL(debug) << "Read message type: " << static_cast<int>(msg_type);
    total_bytes += sizeof(msg_type);

    // Read source id
    read_bytes(input, &frame.source_id, sizeof(frame.source_id));
    BOOST_LOG_TRIVIAL(debug) << "Read source id: " << static_cast<int>(frame.source_id);
    total_bytes += sizeof(frame.source_id);

    // Read payload size
    uint64_t network_payload_size;
    read_bytes(input, &network_payload_size, sizeof(network_payload_size));
    frame.payload_size = boost::endian::big_to_native(network_payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Read payload size: " << frame.payload_size;
    total_bytes += sizeof(network_payload_size);

    // Create payload stream
    frame.payload_stream = std::make_shared<std::stringstream>();

    // Decrypt payload if present
    if (frame.payload_size > 0) {
      BOOST_LOG_TRIVIAL(debug) << "Decrypting payload of size: " << frame.payload_size;

      // Create a buffer for encrypted payload
      std::vector<char> encrypted_buffer(frame.payload_size);
      if (!input.read(encrypted_buffer.data(), frame.payload_size)) {
        throw std::runtime_error("Failed to read encrypted payload");
      }

      // Create stream for decryption
      std::stringstream encrypted_stream;
      encrypted_stream.write(encrypted_buffer.data(), frame.payload_size);

      // Decrypt payload
      crypto.decrypt(encrypted_stream, *frame.payload_stream);
      frame.payload_stream->seekg(0);

      total_bytes += frame.payload_size;
    }

    // Only produce to channel after frame is fully processed
    channel_.produce(frame);

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