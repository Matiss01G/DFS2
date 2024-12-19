#include "network/peer_manager.hpp"
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

PeerManager::PeerManager() {
    BOOST_LOG_TRIVIAL(info) << "PeerManager initialized";
}

PeerManager::~PeerManager() {
    shutdown();
}

void PeerManager::add_peer(std::shared_ptr<TCP_Peer> peer) {
    if (!peer) {
        BOOST_LOG_TRIVIAL(error) << "Attempted to add null peer";
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const std::string& peer_id = peer->get_peer_id();
    
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        BOOST_LOG_TRIVIAL(warning) << "Peer with ID " << peer_id << " already exists";
        return;
    }

    peers_[peer_id] = peer;
    BOOST_LOG_TRIVIAL(info) << "Added peer with ID: " << peer_id;
}

void PeerManager::remove_peer(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        it->second->disconnect();
        peers_.erase(it);
        BOOST_LOG_TRIVIAL(info) << "Removed peer with ID: " << peer_id;
    } else {
        BOOST_LOG_TRIVIAL(warning) << "Attempted to remove non-existent peer: " << peer_id;
    }
}

std::shared_ptr<TCP_Peer> PeerManager::get_peer(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        return it->second;
    }
    
    return nullptr;
}

void PeerManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    BOOST_LOG_TRIVIAL(info) << "Initiating PeerManager shutdown";
    
    for (auto& peer_pair : peers_) {
        try {
            peer_pair.second->disconnect();
            BOOST_LOG_TRIVIAL(debug) << "Disconnected peer: " << peer_pair.first;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error disconnecting peer " << peer_pair.first 
                                   << ": " << e.what();
        }
    }
    
    peers_.clear();
    BOOST_LOG_TRIVIAL(info) << "PeerManager shutdown complete";
}

} // namespace network
} // namespace dfs
