#ifndef DFS_NETWORK_PEER_HPP
#define DFS_NETWORK_PEER_HPP

#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include <streambuf>
#include "message_frame.hpp"

namespace dfs {
namespace network {

/**
 * Network peer interface for stream-based communication in DFS.
 * Implements streaming data transfer and message-based support.
 * 
 * Primary Interface:
 * - Stream-based data transfer (input/output streams)
 * - Asynchronous processing with callbacks
 */
class Peer {
public:
    // Stream callback type for processing received data
    using StreamProcessor = std::function<void(std::istream&)>;
    
    virtual ~Peer() = default;

    // Connection management
    virtual bool connect(const std::string& address, uint16_t port) = 0;
    virtual bool disconnect() = 0;
    virtual bool is_connected() const = 0;

    // Stream operations (required)
    virtual std::istream* get_input_stream() = 0;
    
    // Message operations
    virtual bool send_message(const std::string& message) = 0;

    // Stream processing
    virtual void set_stream_processor(StreamProcessor processor) = 0;
    virtual bool start_stream_processing() = 0;
    virtual void stop_stream_processing() = 0;

protected:
    Peer() = default;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_PEER_HPP
