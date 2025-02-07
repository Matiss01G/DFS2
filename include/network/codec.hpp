#ifndef DFS_NETWORK_CODEC_HPP
#define DFS_NETWORK_CODEC_HPP

#include <cstdint>
#include <iostream>
#include <mutex>
#include "network/message_frame.hpp"
#include "network/channel.hpp"

namespace dfs {
namespace network {

class Codec {
public:
  // ---- CONSTRUCTOR AND DESTRUCTOR ----
  explicit Codec(const std::vector<uint8_t>& key, Channel& channel);

  
  // ---- SERIALIZATION AND DESERIALIZATION ----
  // Serializes a message frame to an output stream
  std::size_t serialize(const MessageFrame& frame, std::ostream& output);
  // Deserializes a message frame from input stream and pushes to channel
  MessageFrame deserialize(std::istream& input);

private:
  // ---- PARAMETERS ----
  std::vector<uint8_t> key_;
  Channel& channel_;

  
  // ---- STREAM OPERATIONS ----
  // Writes bytes to an output stream
  void write_bytes(std::ostream& output, const void* data, std::size_t size);
  // Reads bytes from an input stream
  void read_bytes(std::istream& input, void* data, std::size_t size);

  
  // ---- HOST TO NETWORK BYTE ORDER CONVERSION ----
  // Convert host byte order to network byte order
  static uint32_t to_network_order(uint32_t host_value) {
    return boost::endian::native_to_big(host_value);
  }
  // Convert from host byte order to netwrok byte order
  static uint64_t to_network_order(uint64_t host_value) {
    return boost::endian::native_to_big(host_value);
  }

  
  // ---- NETWORK TO HOST BYTE ORDER CONVERSION ----
  // Convert from network byte order to host byte order
  static uint32_t from_network_order(uint32_t network_value) {
    return boost::endian::big_to_native(network_value);
  }
  static uint64_t from_network_order(uint64_t network_value) {
    return boost::endian::big_to_native(network_value);
  }


  // ---- UTILITY METHODS ----
  // Returns size of data + padding bytes from encryption
  static size_t get_padded_size(size_t original_size);
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_CODEC_HPP