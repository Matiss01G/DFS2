#ifndef DFS_NETWORK_PEER_HPP
#define DFS_NETWORK_PEER_HPP

#include <memory>
#include <string>
#include <functional>
#include <boost/asio.hpp>
#include "types.hpp"

namespace dfs::network {

class IPeer {
public:
    // Handler for receiving raw packet data
    using packet_handler = std::function<void(const PacketHeader&, std::shared_ptr<std::istream>)>;
    using error_handler = std::function<void(NetworkError)>;
    
    virtual ~IPeer() = default;
    
    // Connection management
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Stream-based packet sending
    virtual void send_packet(PacketType type, 
                           std::shared_ptr<std::istream> payload,
                           uint32_t sequence_number) = 0;
    
    // Event handlers
    virtual void set_packet_handler(packet_handler handler) = 0;
    virtual void set_error_handler(error_handler handler) = 0;
    
    // Peer identification
    virtual std::string get_address() const = 0;
    virtual uint16_t get_port() const = 0;
    virtual const std::array<uint8_t, 16>& get_id() const = 0;
};

// Factory function for creating TCP peers
std::shared_ptr<IPeer> create_tcp_peer(
    boost::asio::io_context& io_context,
    const std::string& address,
    uint16_t port
);

} // namespace dfs::network

#endif // DFS_NETWORK_PEER_HPP
