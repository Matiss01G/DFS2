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

class PeerManager {
public:
    PeerManager(Channel& channel, TCP_Server& tcp_server, const std::vector<uint8_t>& key);
    ~PeerManager();

    // Connection Management
    bool connect(const std::string& peer_id, const std::string& address, uint16_t port);
    bool disconnect(const std::string& peer_id);
    bool is_connected(const std::string& peer_id);

    // Peer creation
    void create_peer(const boost::system::error_code& error,
    std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    // Peer Management
    void add_peer(std::shared_ptr<TCP_Peer> peer);
    void remove_peer(const std::string& peer_id);
    std::shared_ptr<TCP_Peer> get_peer(const std::string& peer_id);

    // Stream Operations
    bool send_to_peer(const std::string& peer_id, std::istream& stream);
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
    std::map<std::string, std::shared_ptr<TCP_Peer>> peers_;
    mutable std::mutex mutex_;
};

} // namespace network
} // namespace dfs

#endif // PEER_MANAGER_HPP