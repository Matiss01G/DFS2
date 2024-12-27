#ifndef DFS_NETWORK_CODEC_HPP
#define DFS_NETWORK_CODEC_HPP

#include <cstdint>
#include <iostream>
#include <mutex>
#include <boost/endian/conversion.hpp>
#include "network/message_frame.hpp"
#include "network/channel.hpp"

namespace dfs {
namespace network {

class Codec {
public:
    // Serializes a message frame to an output stream
    std::size_t serialize(const MessageFrame& frame, std::ostream& output);

    // Deserializes from an input stream into the channel and returns total bytes read
    std::size_t deserialize(std::istream& input, Channel& channel);

private:
    // Writes bytes to an output stream
    void write_bytes(std::ostream& output, const void* data, std::size_t size);

    // Reads bytes from an input stream
    void read_bytes(std::istream& input, void* data, std::size_t size);

    // Convert host byte order to network byte order
    static uint32_t to_network_order(uint32_t host_value) {
        return boost::endian::native_to_big(host_value);
    }

    static uint64_t to_network_order(uint64_t host_value) {
        return boost::endian::native_to_big(host_value);
    }

    // Convert network byte order to host byte order
    static uint32_t from_network_order(uint32_t network_value) {
        return boost::endian::big_to_native(network_value);
    }

    static uint64_t from_network_order(uint64_t network_value) {
        return boost::endian::big_to_native(network_value);
    }

    std::mutex mutex_;  // Mutex for thread-safe channel operations
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_CODEC_HPP