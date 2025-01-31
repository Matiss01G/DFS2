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
  BOOST_LOG_TRIVIAL(info) << "Codec: Initializing Codec with key of size: " << key_.size();
}

std::size_t Codec::serialize(const MessageFrame& frame, std::ostream& output) {
  if (!output.good()) {
    BOOST_LOG_TRIVIAL(error) << "Codec: Invalid output stream state";
    throw std::runtime_error("Codec: Invalid output stream");
  }

  std::size_t total_bytes = 0;

  // Create and itialize crypto stream with key and IV
  crypto::CryptoStream filename_crypto;
  crypto::CryptoStream payload_crypto;

  filename_crypto.initialize(key_, frame.iv_);
  payload_crypto.initialize(key_, frame.iv_);

  BOOST_LOG_TRIVIAL(info) << "Codec: Starting message frame serialization";

  try {
    // Write IV as first header
    BOOST_LOG_TRIVIAL(debug) << "Codec: Writing IV of size: " << frame.iv_.size();
    write_bytes(output, frame.iv_.data(), frame.iv_.size());
    total_bytes += frame.iv_.size();

    // Write message type
    uint8_t msg_type = static_cast<uint8_t>(frame.message_type);
    BOOST_LOG_TRIVIAL(debug) << "Codec: Writing message type: " << static_cast<int>(msg_type);
    write_bytes(output, &msg_type, sizeof(msg_type));
    total_bytes += sizeof(msg_type);

    // Write source id 
    BOOST_LOG_TRIVIAL(debug) << "Codec: Writing source id: " << static_cast<int>(frame.source_id);
    write_bytes(output, &frame.source_id, sizeof(frame.source_id));
    total_bytes += sizeof(frame.source_id);

    // Write payload size in network byte order
    uint64_t network_payload_size = boost::endian::native_to_big(frame.payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Codec: Writing payload size: " << frame.payload_size;
    write_bytes(output, &network_payload_size, sizeof(network_payload_size));
    total_bytes += sizeof(network_payload_size);

    BOOST_LOG_TRIVIAL(debug) << "Codec: Writing filename length: " << frame.filename_length;
    // Encrypt and write filename length
    uint32_t network_filename_length = boost::endian::native_to_big(frame.filename_length);
    // Create stream for raw data
    std::stringstream filename_length_stream;
    filename_length_stream.write(reinterpret_cast<char*>(&network_filename_length), sizeof(network_filename_length));
    // Encrypt the filename length
    std::stringstream encrypted_filename_length;
    filename_crypto.encrypt(filename_length_stream, encrypted_filename_length);
    // Write filename length
    BOOST_LOG_TRIVIAL(debug) << "Codec: Writing encrypted filename length";
    write_bytes(output, encrypted_filename_length.str().data(), encrypted_filename_length.str().size());
    total_bytes += encrypted_filename_length.str().size();

    // Encrypt and write payload if present
    if (frame.payload_size > 0 && frame.payload_stream) {
      BOOST_LOG_TRIVIAL(debug) << "Codec: Encrypting and writing payload of size: " << frame.payload_size;
      frame.payload_stream->seekg(0);
      payload_crypto.encrypt(*frame.payload_stream, output);
      total_bytes += get_padded_size(frame.payload_size);
    } 

    output.flush();
    BOOST_LOG_TRIVIAL(info) << "Codec: Encrypted message frame serialization complete. Total bytes written: " << total_bytes;
    return total_bytes;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Codec: Error during serialization: " << e.what();
    throw;
  }
}

MessageFrame Codec::deserialize(std::istream& input) {
  if (!input.good()) {
    BOOST_LOG_TRIVIAL(error) << "Codec: Invalid input stream state";
    throw std::runtime_error("Codec: Invalid input stream");
  }

  MessageFrame frame;
  std::size_t total_bytes = 0;

  // Create CryptoStream instance
  crypto::CryptoStream filename_crypto;
  crypto::CryptoStream payload_crypto;

  BOOST_LOG_TRIVIAL(info) << "Codec: Starting message frame deserialization";

  try {
    // Read IV first
    frame.iv_.resize(crypto::CryptoStream::IV_SIZE);
    BOOST_LOG_TRIVIAL(debug) << "Codec: Reading IV";
    read_bytes(input, frame.iv_.data(), frame.iv_.size());
    total_bytes += frame.iv_.size();

    // Initialize crypto stream with key and IV
    filename_crypto.initialize(key_, frame.iv_);
    payload_crypto.initialize(key_, frame.iv_);


    // Read message type
    uint8_t msg_type;
    read_bytes(input, &msg_type, sizeof(msg_type));
    frame.message_type = static_cast<MessageType>(msg_type);
    BOOST_LOG_TRIVIAL(debug) << "Codec: Read message type: " << static_cast<int>(msg_type);
    total_bytes += sizeof(msg_type);

    // Read source id
    uint8_t source_id;
    read_bytes(input, &source_id, sizeof(source_id));
    frame.source_id = source_id;
    BOOST_LOG_TRIVIAL(debug) << "Codec: Read source id: " << static_cast<int>(source_id);
    total_bytes += sizeof(source_id);

    // Read payload size
    uint64_t network_payload_size;
    read_bytes(input, &network_payload_size, sizeof(network_payload_size));
    frame.payload_size = boost::endian::big_to_native(network_payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Codec: Read payload size: " << frame.payload_size;
    total_bytes += sizeof(network_payload_size);

    // Decrypt filename length
    // create buffer and read 1 block of encrypted data into it
    std::vector<char> encrypted_filename_length(crypto::CryptoStream::BLOCK_SIZE);
    read_bytes(input, encrypted_filename_length.data(), encrypted_filename_length.size());
    // Create stream for encrypted data and copy encrypted data into it
    std::stringstream encrypted_filename_length_stream;
    encrypted_filename_length_stream.write(encrypted_filename_length.data(), encrypted_filename_length.size());
    // Create stream for decrypted data and decrypt filename_length into it
    std::stringstream decrypted_filename_length_stream;
    filename_crypto.decrypt(encrypted_filename_length_stream, decrypted_filename_length_stream);
    // Create variable for network ordered filename_length and read decrypted data into it
    uint32_t network_filename_length;
    decrypted_filename_length_stream.read(reinterpret_cast<char*>(&network_filename_length), sizeof(network_filename_length));
    // Convert to host byte order
    frame.filename_length = boost::endian::big_to_native(network_filename_length);
    BOOST_LOG_TRIVIAL(debug) << "Codec: Read decrypted filename length: " << frame.filename_length;
    total_bytes += encrypted_filename_length.size();

    frame.payload_stream = std::make_shared<std::stringstream>();

    // Decrypt payload if present
    if (frame.payload_size > 0) {
      BOOST_LOG_TRIVIAL(debug) << "Codec: Decrypting payload of size: " << frame.payload_size;
      payload_crypto.decrypt(input, *frame.payload_stream);
      total_bytes += frame.payload_size;
      frame.payload_stream->seekg(0);
    } 

    channel_.produce(frame);
    BOOST_LOG_TRIVIAL(debug) << "Codec: New frame added to channel";

    BOOST_LOG_TRIVIAL(info) << "Codec: Message frame deserialization complete. Total bytes read: " << total_bytes;
    return frame;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Codec: Error during deserialization: " << e.what();
    throw;
  }
}

size_t Codec::get_padded_size(size_t original_size) {
    return ((original_size + crypto::CryptoStream::BLOCK_SIZE -1) / crypto::CryptoStream::BLOCK_SIZE) * crypto::CryptoStream::BLOCK_SIZE;
}

void Codec::write_bytes(std::ostream& output, const void* data, std::size_t size) {
  if (!output.write(static_cast<const char*>(data), size)) {
    BOOST_LOG_TRIVIAL(error) << "Codec: Failed to write " << size << " bytes to output stream";
    throw std::runtime_error("Codec: Failed to write to output stream");
  }
}

void Codec::read_bytes(std::istream& input, void* data, std::size_t size) {
  if (!input.read(static_cast<char*>(data), size)) {
    BOOST_LOG_TRIVIAL(error) << "Codec: Failed to read " << size << " bytes from input stream";
    throw std::runtime_error("Codec: Failed to read from input stream");
  }
}

} // namespace network
} // namespace dfs