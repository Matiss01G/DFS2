#include "network/channel.hpp"
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

void Channel::produce(const MessageFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(frame);
    BOOST_LOG_TRIVIAL(debug) << "Added message frame to channel. Channel size: " << queue_.size();
}

bool Channel::consume(MessageFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    
    frame = queue_.front();
    queue_.pop();
    BOOST_LOG_TRIVIAL(debug) << "Retrieved message frame from channel. Channel size: " << queue_.size();
    return true;
}

bool Channel::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

std::size_t Channel::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace network
} // namespace dfs
