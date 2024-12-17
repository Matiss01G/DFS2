#ifndef DFS_NETWORK_PEER_HPP
#define DFS_NETWORK_PEER_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "connection_state.hpp"
#include "message_frame.hpp"

namespace dfs {
namespace network {

// Network streaming interface for DFS peers
class Peer {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    
    virtual ~Peer() = default;

    // Connect to remote peer at address:port
    virtual bool connect(const std::string& address, uint16_t port) = 0;

    // Close active connection
    virtual bool disconnect() = 0;

    // Send data frame to peer
    virtual bool send_data(const MessageFrame& frame) = 0;

    // Set callback for received data
    virtual void set_receive_callback(DataCallback callback) = 0;

    // Begin reading data from peer
    virtual void start_read_loop() = 0;

    // Stop reading data from peer
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
