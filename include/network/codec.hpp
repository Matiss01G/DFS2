#ifndef DFS_NETWORK_CODEC_HPP
#define DFS_NETWORK_CODEC_HPP

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

namespace dfs {
namespace network {

class Codec {
  public:
    // Serialize a MessageFrame to an output stream
    // Returns the total number of bytes written
    std::size_t serialize(const MessageFrame& frame, std::ostream& output);
  
    // Deserialize a MessageFrame from an input stream
    // The frame is pushed to the provided channel
    MessageFrame deserialize(std::istream& input, Channel& channel);
  
    private:
    // Helper methods for byte-level operations
    void write_bytes(std::ostream& output, const void* data, std::size_t size);
    void read_bytes(std::istream& input, void* data, std::size_t size);
  
    // Network byte order conversion helpers
    template<typename T>
    static T to_network_order(T value) {
      if constexpr (sizeof(T) == 8) {
        return htobe64(value);
      } else if constexpr (sizeof(T) == 4) {
        return htobe32(value);
      } else if constexpr (sizeof(T) == 2) {
        return htobe16(value);
      }
      return value;
    }
  
    template<typename T>
    static T from_network_order(T value) {
      if constexpr (sizeof(T) == 8) {
        return be64toh(value);
      } else if constexpr (sizeof(T) == 4) {
        return be32toh(value);
      } else if constexpr (sizeof(T) == 2) {
        return be16toh(value);
      }
      return value;
    }
  
    // Mutex for thread-safe channel operations
    std::mutex mutex_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_CODEC_HPP
