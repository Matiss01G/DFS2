#include "network/channel.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <istream>

namespace dfs {
namespace network {

void Channel::produce(const MessageFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create a copy of the frame
    MessageFrame frame_copy = frame;

    // If there's a payload stream, create a new stream with the same content
    if (frame.payload_stream) {
        auto new_payload = std::make_shared<std::stringstream>();
        frame.payload_stream->seekg(0, std::ios::beg);
        *new_payload << frame.payload_stream->rdbuf();
        frame_copy.payload_stream = new_payload;
    }

    queue_.push(frame_copy);
    BOOST_LOG_TRIVIAL(debug) << "Added message frame to channel. Channel size: " << queue_.size();
}

bool Channel::consume(MessageFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }

    // Get the front message
    MessageFrame& queued_frame = queue_.front();

    // Copy all basic fields
    frame = queued_frame;

    // If there's a payload stream, create a new stream with the same content
    if (queued_frame.payload_stream) {
        auto new_payload = std::make_shared<std::stringstream>();
        queued_frame.payload_stream->seekg(0, std::ios::beg);
        *new_payload << queued_frame.payload_stream->rdbuf();
        frame.payload_stream = new_payload;
    }

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