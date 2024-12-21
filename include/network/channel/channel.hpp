#ifndef DFS_NETWORK_CHANNEL_HPP
#define DFS_NETWORK_CHANNEL_HPP

#include <queue>
#include <mutex>
#include "network/message_frame.hpp"

namespace dfs {
namespace network {

class Channel {
public:
    void produce(const MessageFrame& frame);
    bool consume(MessageFrame& frame);
    bool empty() const;
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::queue<MessageFrame> queue_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_CHANNEL_HPP
