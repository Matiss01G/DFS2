#ifndef PEER_MANAGER_HPP
#define PEER_MANAGER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <istream>
#include "peer.hpp"
#include "tcp_peer.hpp"

namespace dfs {
namespace network {

class PeerManager {
public:
    PeerManager();
    ~PeerManager();

    // Add a new peer to the manager
    void add_peer(std::shared_ptr<TCP_Peer> peer);

    // Remove a peer by its ID
    void remove_peer(const std::string& peer_id);

    // Get a peer by its ID
    std::shared_ptr<TCP_Peer> get_peer(const std::string& peer_id);

    // Broadcast data from input stream to all connected peers
    bool broadcast_stream(std::istream& input_stream);

    // Shutdown all peer connections and clear the peers map
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