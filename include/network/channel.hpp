#pragma once

#include <queue>
#include <mutex>
#include "network/message_frame.hpp"

namespace dfs {
namespace network {

class Channel {
public:
    Channel() = default;
    ~Channel() = default;

    // Adds a message frame to the back of the queue
    void produce(const MessageFrame& frame);

    // Retrieves and removes the next message frame from the front of the queue
    // Returns true if a frame was retrieved, false if queue was empty
    bool consume(MessageFrame& frame);

    // Returns true if the channel has no messages
    bool empty() const;

    // Returns the number of messages in the channel
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::queue<MessageFrame> queue_;
};

} // namespace network
} // namespace dfs
