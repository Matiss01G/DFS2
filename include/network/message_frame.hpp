#ifndef DFS_NETWORK_MESSAGE_FRAME_HPP
#define DFS_NETWORK_MESSAGE_FRAME_HPP

#include <cstdint>
#include <memory>
#include <sstream>
#include <vector>
#include <boost/endian/conversion.hpp>

namespace dfs {
namespace network {

enum class MessageType : uint8_t {
    STORE_FILE = 0,
    GET_FILE = 1
};

struct MessageFrame {
    std::vector<uint8_t> iv_;
    MessageType message_type;
    uint32_t source_id;
    uint64_t payload_size;
    uint32_t filename_length;
    std::shared_ptr<std::stringstream> payload_stream;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_MESSAGE_FRAME_HPP