#ifndef PEER_MANAGER_HPP
#define PEER_MANAGER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <istream>
#include "peer.hpp"
#include "tcp_peer.hpp"
#include "channel.hpp"
#include "tcp_server.hpp"

namespace dfs {
namespace network {

class TCP_Server;  // Forward declaration

class PeerManager {
public:    
    PeerManager(Channel& channel, TCP_Server& tcp_server, const std::vector<uint8_t>& key);
    ~PeerManager();

    // Connection Management
    bool disconnect(uint8_t peer_id);
    bool is_connected(uint8_t peer_id);

    // Peer creation
    void create_peer(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint8_t peer_id);

    // Peer Management
    bool has_peer(uint8_t peer_id);
    std::shared_ptr<TCP_Peer> get_peer(uint8_t peer_id);
    void add_peer(const std::shared_ptr<TCP_Peer> peer);
    void remove_peer(uint8_t peer_id);

    // Stream Operations
    bool send_to_peer(uint8_t peer_id, std::istream& stream);
    bool broadcast_stream(std::istream& input_stream);

    // Utility Methods
    std::size_t size() const;
    void shutdown();

    // Delete copy constructor and assignment operator
    PeerManager(const PeerManager&) = delete;
    PeerManager& operator=(const PeerManager&) = delete;

private:
    Channel& channel_;
    TCP_Server& tcp_server_;
    std::vector<uint8_t> key_;
    std::map<uint8_t, std::shared_ptr<TCP_Peer>> peers_;
    mutable std::mutex mutex_;
};

} // namespace network
} // namespace dfs

#endif // PEER_MANAGER_HPP