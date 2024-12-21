#ifndef DFS_NETWORK_CODEC_HPP
#define DFS_NETWORK_CODEC_HPP

#include <cstdint>
#include <iostream>
#include <mutex>

namespace dfs {
namespace network {

// Forward declarations
class Channel;
struct MessageFrame;

class Codec {
public:
    // Serializes a message frame to an output stream
    std::size_t serialize(const MessageFrame& frame, std::ostream& output);

    // Deserializes a message frame from an input stream
    MessageFrame deserialize(std::istream& input, Channel& channel);

private:

    // Writes bytes to an output stream
    void write_bytes(std::ostream& output, const void* data, std::size_t size);

    
    // Reads bytes from an input stream
    void read_bytes(std::istream& input, void* data, std::size_t size);

    std::mutex mutex_;  // Mutex for thread-safe channel operations
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_CODEC_HPP