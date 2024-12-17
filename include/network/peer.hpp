#ifndef DFS_NETWORK_PEER_HPP
#define DFS_NETWORK_PEER_HPP

#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include <streambuf>
#include "connection_state.hpp"
#include "message_frame.hpp"

namespace dfs {
namespace network {

/**
 * Network peer interface for stream-based communication in DFS.
 * Implements streaming data transfer with state management and optional
 * message-based support for backward compatibility.
 * 
 * Primary Interface:
 * - Stream-based data transfer (input/output streams)
 * - Asynchronous processing with callbacks
 * - Connection state management
 */
class Peer {
public:
    // Stream callback type for processing received data
    using StreamProcessor = std::function<void(std::istream&)>;
    
    virtual ~Peer() = default;

    // Connection management
    virtual bool connect(const std::string& address, uint16_t port) = 0;
    virtual bool disconnect() = 0;

    // Stream operations (required)
    virtual std::ostream* get_output_stream() = 0;
    virtual std::istream* get_input_stream() = 0;

    // Stream processing
    virtual void set_stream_processor(StreamProcessor processor) = 0;
    virtual bool start_stream_processing() = 0;
    virtual void stop_stream_processing() = 0;

    // Optional message-based operations
    virtual bool send_frame(const MessageFrame& frame) { return false; }
    virtual void set_frame_callback(OnDataReceived callback) {}

    // State management
    virtual ConnectionState::State get_connection_state() const = 0;
    bool is_connected() const {
        return get_connection_state() == ConnectionState::State::CONNECTED;
    }

protected:
    Peer() = default;
    
    bool validate_state(ConnectionState::State required_state) const {
        return get_connection_state() == required_state;
    }
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_PEER_HPP
