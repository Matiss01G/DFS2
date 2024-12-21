#ifndef DFS_NETWORK_CODEC_HPP
#define DFS_NETWORK_CODEC_HPP

#include <istream>
#include <ostream>
#include <mutex>
#include "network/message_frame.hpp"
#include "channel/channel.hpp"

namespace dfs {
namespace network {

class Codec {
public:
    /**
     * Serialize a MessageFrame into a binary format suitable for network transmission.
     * @param frame The MessageFrame to serialize
     * @param output The output stream to write the serialized data to
     * @return Number of bytes written
     */
    std::size_t serialize(const MessageFrame& frame, std::ostream& output);

    /**
     * Deserialize a MessageFrame from a binary stream and push it to the channel.
     * @param input The input stream containing serialized frame data
     * @param channel The channel to push the deserialized frame to
     * @return The deserialized MessageFrame
     */
    MessageFrame deserialize(std::istream& input, Channel& channel);

private:
    std::mutex mutex_;

    // Helper functions for network byte order handling
    static uint32_t to_network_order(uint32_t host_value);
    static uint64_t to_network_order(uint64_t host_value);
    static uint32_t from_network_order(uint32_t network_value);
    static uint64_t from_network_order(uint64_t network_value);

    // Stream read/write helpers
    static void write_bytes(std::ostream& output, const void* data, std::size_t size);
    static void read_bytes(std::istream& input, void* data, std::size_t size);
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_CODEC_HPP
