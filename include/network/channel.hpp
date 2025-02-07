#pragma once

#include <queue>
#include <mutex>
#include "network/message_frame.hpp"

namespace dfs {
namespace network {

class Channel {
public:
    // ---- CONSTRUCTOR AND DESTRUCTOR
    Channel() = default;
    ~Channel() = default;

    
    // ---- CHANNEL CONTROL METHODS ----
    // Adds a message frame to the back of the queue
    void produce(const MessageFrame& frame);
    // Retrieves and removes next message frame from queue
    bool consume(MessageFrame& frame);

    
    // ---- QUERY METHODS ----
    // Returns true if the channel has no messages
    bool empty() const;
    // Returns the number of messages in the channel
    std::size_t size() const;

private:
    // ---- PARAMETERS ----
    mutable std::mutex mutex_;
    std::queue<MessageFrame> queue_;
};

} // namespace network
} // namespace dfs
