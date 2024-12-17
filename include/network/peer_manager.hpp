#ifndef DFS_NETWORK_PEER_MANAGER_HPP
#define DFS_NETWORK_PEER_MANAGER_HPP

#include <unordered_map>
#include <memory>
#include <mutex>
#include <boost/asio.hpp>
#include "peer.hpp"

namespace dfs::network {

class PeerManager {
public:
    explicit PeerManager(boost::asio::io_context& io_context);
    
    // Peer management
    void start_listening(uint16_t port);
    void stop_listening();
    
    void add_peer(const std::string& address, uint16_t port);
    void remove_peer(const std::array<uint8_t, 16>& peer_id);
    
    // Packet broadcasting
    void broadcast_packet(PacketType type, 
                        std::shared_ptr<std::istream> payload,
                        uint32_t sequence_number);
    
    // Peer access
    std::shared_ptr<IPeer> get_peer(const std::array<uint8_t, 16>& peer_id);
    std::vector<std::shared_ptr<IPeer>> get_all_peers();
    
    // Event handlers
    using packet_handler = IPeer::packet_handler;
    void set_packet_handler(packet_handler handler);

private:
    boost::asio::io_context& io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    
    std::unordered_map<std::string, std::shared_ptr<IPeer>> peers_;
    std::mutex peers_mutex_;
    packet_handler packet_handler_;
    
    // Internal methods
    void start_accept();
    void handle_accept(std::shared_ptr<IPeer> peer,
                      const boost::system::error_code& error);
    void handle_peer_error(const std::string& peer_key,
                          NetworkError error);
};

} // namespace dfs::network

#endif // DFS_NETWORK_PEER_MANAGER_HPP
