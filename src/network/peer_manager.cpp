#include "network/peer_manager.hpp"
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

PeerManager::PeerManager(Channel& channel, TCP_Server& tcp_server, const std::vector<uint8_t>& key)
  : channel_(channel)
  , tcp_server_(tcp_server)
  , key_(key) {

  if (key_.empty() || key_.size() != 32) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Invalid key size: " << key_.size() << " bytes. Expected 32 bytes.";
    throw std::invalid_argument("Peer manager: Invalid cryptographic key size");
  }

  BOOST_LOG_TRIVIAL(info) << "Peer manager: initialized with key size: " << key_.size() << " bytes";
}

PeerManager::~PeerManager() {
  shutdown();
}

void PeerManager::create_peer(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint8_t peer_id) {
  try {

    // Create new TCP peer with channel and default key
    auto peer = std::make_shared<TCP_Peer>(peer_id, channel_, key_);

    // Move the accepted socket to the peer
    peer->get_socket() = std::move(*socket);

    // Add peer to map
    add_peer(peer);

    // Set up stream processor to use codec's deserialize function
    peer->set_stream_processor(
       [peer](std::istream& stream) {
         try {
           peer->codec_->deserialize(stream);
         } catch (const std::exception& e) {
           BOOST_LOG_TRIVIAL(error) << "Peer manager: Deserialization error: " << e.what();
         }
       }
     );

    // Start stream processing
    if (!peer->start_stream_processing()) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Failed to start stream processing for peer: " << static_cast<int>(peer_id);
    return;
    }

    BOOST_LOG_TRIVIAL(info) << "Peer manager: Accepted and initialized new connection from peer: " << static_cast<int>(peer_id);
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Error handling new connection: " << e.what();
  }

  // Continue accepting new connections if server is still running
  tcp_server_.start_accept();
}

void PeerManager::add_peer(std::shared_ptr<TCP_Peer> peer) {
  if (!peer) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Attempted to add null peer";
    return;
  }

  uint8_t peer_id = peer->get_peer_id();

  std::lock_guard<std::mutex> lock(mutex_);

  peers_[peer_id] = peer;
  BOOST_LOG_TRIVIAL(info) << "Peer manager: Added peer with ID: " << static_cast<int>(peer_id);
}

void PeerManager::remove_peer(uint8_t peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it != peers_.end()) {
    disconnect(peer_id);
    peers_.erase(it);
    BOOST_LOG_TRIVIAL(info) << "Peer manager: Removed peer with ID: " << static_cast<int>(peer_id);
  } else {
    BOOST_LOG_TRIVIAL(warning) << "Peer manager: Attempted to remove non-existent peer: " << static_cast<int>(peer_id);
  }
}

bool PeerManager::disconnect(uint8_t peer_id) {

  auto it = peers_.find(peer_id);
  if (it == peers_.end()) {
    BOOST_LOG_TRIVIAL(warning) << "Peer manager: Cannot disconnect - peer not found: " << static_cast<int>(peer_id);
    return false;
  }

  try {
    auto& peer = it->second;
    peer->stop_stream_processing();
    peer->cleanup_connection();
    BOOST_LOG_TRIVIAL(info) << "Peer manager: Successfully disconnected peer: " << static_cast<int>(peer_id);
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Disconnect error for peer " << static_cast<int>(peer_id) 
                << ": " << e.what();
    return false;
  }
}

bool PeerManager::is_connected(uint8_t peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it == peers_.end()) {
    return false;
  }

  return it->second->get_socket().is_open();
}

bool PeerManager::has_peer(uint8_t peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_.find(peer_id) != peers_.end();
}

std::shared_ptr<TCP_Peer> PeerManager::get_peer(uint8_t peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it != peers_.end()) {
    return it->second;
  }

  return nullptr;
}

bool PeerManager::broadcast_stream(dfs::utils::Pipeliner& pipeline) {
  if (!pipeline.good()) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Invalid input stream provided for broadcast";
    return false;
  }

  if (peers_.empty()) {
    BOOST_LOG_TRIVIAL(warning) << "Peer manager: No peers available for broadcast";
    return false;
  }

  // Get the total size from pipeline
  std::size_t total_size = pipeline.get_total_size();

  bool all_success = true;
  size_t success_count = 0;

  for (auto& peer_pair : peers_) {
    try {
      if (!is_connected(peer_pair.first)) {
        BOOST_LOG_TRIVIAL(warning) << "Peer manager: Skipping disconnected peer: " << static_cast<int>(peer_pair.first);
        all_success = false;
        continue;
      }

      std::lock_guard<std::mutex> lock(mutex_);

      if (peer_pair.second->send_stream(pipeline, total_size)) {
        success_count++;
        BOOST_LOG_TRIVIAL(debug) << "Peer manager: Successfully broadcast to peer: " << static_cast<int>(peer_pair.first);
      } else {
        all_success = false;
        BOOST_LOG_TRIVIAL(error) << "Peer manager: Failed to broadcast to peer: " << static_cast<int>(peer_pair.first);
      }
    } catch (const std::exception& e) {
      all_success = false;
      BOOST_LOG_TRIVIAL(error) << "Peer manager: Exception while broadcasting to peer " 
                  << peer_pair.first << ": " << e.what();
    }
  }

  BOOST_LOG_TRIVIAL(info) << "Peer manager: Broadcast completed. Successfully sent to " 
              << success_count << " out of " << peers_.size() << " peers";

  return all_success;
}

bool PeerManager::send_to_peer(uint8_t peer_id, dfs::utils::Pipeliner& pipeline) {
  if (!pipeline.good()) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Invalid input stream provided for peer_id: " << static_cast<int>(peer_id);
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it == peers_.end()) {
    BOOST_LOG_TRIVIAL(warning) << "Peer manager: Peer not found with ID: " << static_cast<int>(peer_id);
    return false;
  }

  if (!is_connected(peer_id)) {
    BOOST_LOG_TRIVIAL(warning) << "Peer manager: Peer is not connected: " << static_cast<int>(peer_id);
    return false;
  }
  
  // Get the total size from pipeline
  std::size_t total_size = pipeline.get_total_size();


  try {
    bool success = it->second->send_stream(pipeline, total_size);
    if (success) {
      BOOST_LOG_TRIVIAL(debug) << "Peer manager: Successfully sent stream to peer: " << static_cast<int>(peer_id);
    } else {
      BOOST_LOG_TRIVIAL(error) << "Peer manager: Failed to send stream to peer: " << static_cast<int>(peer_id);
    }
    return success;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Peer manager: Exception while sending to peer " << static_cast<int>(peer_id) 
                << ": " << e.what();
    return false;
  }
}

void PeerManager::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);

  BOOST_LOG_TRIVIAL(info) << "Peer manager: Initiating PeerManager shutdown";

  for (auto& peer_pair : peers_) {
    try {
      disconnect(peer_pair.first);
      BOOST_LOG_TRIVIAL(debug) << "Peer manager: Disconnected peer: " << peer_pair.first;
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Peer manager: Error disconnecting peer " << peer_pair.first 
                  << ": " << e.what();
    }
  }

  peers_.clear();
  BOOST_LOG_TRIVIAL(info) << "Peer manager: shutdown complete";
}

std::size_t PeerManager::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return peers_.size();
}

} // namespace network
} // namespace dfs