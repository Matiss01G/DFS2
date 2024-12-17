#ifndef DFS_NETWORK_MESSAGE_FRAME_HPP
#define DFS_NETWORK_MESSAGE_FRAME_HPP

#include <cstdint>
#include <array>

namespace dfs {
namespace network {

struct MessageFrame {
    uint8_t message_type;
    std::array<uint8_t, 16> initialization_vector;
    uint32_t source_id;
    uint64_t payload_size;
    uint8_t* payload_data;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_MESSAGE_FRAME_HPP
