#ifndef PEER_MANAGER_HPP
#define PEER_MANAGER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <istream>
#include "tcp_peer.hpp"

namespace dfs {
namespace network {

class PeerManager {
public:
    // Default constructor
    PeerManager();
    ~PeerManager();

    // Connection Management
    bool connect(const std::string& peer_id, const std::string& address, uint16_t port);
    bool disconnect(const std::string& peer_id);
    bool is_connected(const std::string& peer_id);

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
    std::map<std::string, std::shared_ptr<TCP_Peer>> peers_;
    mutable std::mutex mutex_;
};

} // namespace network
} // namespace dfs

#endif // PEER_MANAGER_HPP