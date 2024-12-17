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

// Network streaming interface for DFS peers
class Peer {
public:
    // Handler for processing streamed data chunks, manages buffering and async reading
    using OnDataReceived = std::function<void(std::istream&)>;
    
    virtual ~Peer() = default;

    // Connect to remote peer at address:port
    virtual bool connect(const std::string& address, uint16_t port) = 0;

    // Close active connection
    virtual bool disconnect() = 0;

    // Send data frame to peer
    virtual bool send_data(const MessageFrame& frame) = 0;

    // Set handler for async data reception
    virtual void set_receive_callback(OnDataReceived callback) = 0;

    // Begin background data reading loop
    virtual void start_read_loop() = 0;

    // Stop background data reading loop
    virtual void stop_read_loop() = 0;

    // Get current connection state
    virtual ConnectionState::State get_connection_state() const = 0;

    // Check if peer is connected
    bool is_connected() const {
        return get_connection_state() == ConnectionState::State::CONNECTED;
    }

protected:
    Peer() = default;

    // Validate operation for current state
    virtual bool validate_state(ConnectionState::State required_state) const {
        return get_connection_state() == required_state;
    }
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_PEER_HPP
