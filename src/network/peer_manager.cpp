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

bool PeerManager::broadcast_stream(std::istream& input_stream) {
    if (!input_stream.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream provided for broadcast";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (peers_.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "No peers available for broadcast";
        return false;
    }

    bool all_success = true;
    size_t success_count = 0;

    // Store the current position in the stream
    std::streampos initial_pos = input_stream.tellg();

    for (auto& peer_pair : peers_) {
        try {
            // Reset stream position for each peer
            input_stream.seekg(initial_pos);

            if (!peer_pair.second->is_connected()) {
                BOOST_LOG_TRIVIAL(warning) << "Skipping disconnected peer: " << peer_pair.first;
                all_success = false;
                continue;
            }

            if (peer_pair.second->send_stream(input_stream)) {
                success_count++;
                BOOST_LOG_TRIVIAL(debug) << "Successfully broadcast to peer: " << peer_pair.first;
            } else {
                all_success = false;
                BOOST_LOG_TRIVIAL(error) << "Failed to broadcast to peer: " << peer_pair.first;
            }
        } catch (const std::exception& e) {
            all_success = false;
            BOOST_LOG_TRIVIAL(error) << "Exception while broadcasting to peer " 
                                   << peer_pair.first << ": " << e.what();
        }
    }

    BOOST_LOG_TRIVIAL(info) << "Broadcast completed. Successfully sent to " 
                           << success_count << " out of " << peers_.size() << " peers";

    return all_success;
}

bool PeerManager::send_to_peer(uint32_t peer_id, std::istream& stream) {
    if (!stream.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream provided for peer_id: " << peer_id;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const std::string peer_id_str = std::to_string(peer_id);
    auto it = peers_.find(peer_id_str);

    if (it == peers_.end()) {
        BOOST_LOG_TRIVIAL(warning) << "Peer not found with ID: " << peer_id;
        return false;
    }

    if (!it->second->is_connected()) {
        BOOST_LOG_TRIVIAL(warning) << "Peer is not connected: " << peer_id;
        return false;
    }

    try {
        bool success = it->second->send_stream(stream);
        if (success) {
            BOOST_LOG_TRIVIAL(debug) << "Successfully sent stream to peer: " << peer_id;
        } else {
            BOOST_LOG_TRIVIAL(error) << "Failed to send stream to peer: " << peer_id;
        }
        return success;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Exception while sending to peer " << peer_id 
                               << ": " << e.what();
        return false;
    }
}

bool PeerManager::send_stream(const std::string& peer_id, std::istream& stream) {
    if (!stream.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream provided for peer_id: " << peer_id;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = peers_.find(peer_id);
    if (it == peers_.end()) {
        BOOST_LOG_TRIVIAL(warning) << "Peer not found with ID: " << peer_id;
        return false;
    }

    if (!it->second->is_connected()) {
        BOOST_LOG_TRIVIAL(warning) << "Peer is not connected: " << peer_id;
        return false;
    }

    try {
        bool success = it->second->send_stream(stream);
        if (success) {
            BOOST_LOG_TRIVIAL(debug) << "Successfully sent stream to peer: " << peer_id;
        } else {
            BOOST_LOG_TRIVIAL(error) << "Failed to send stream to peer: " << peer_id;
        }
        return success;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Exception while sending to peer " << peer_id 
                               << ": " << e.what();
        return false;
    }
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