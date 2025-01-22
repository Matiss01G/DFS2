#include "network/peer_manager.hpp"
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

PeerManager::PeerManager(Channel& channel, TCP_Server& tcp_server, const std::vector<uint8_t>& key)
  : channel_(channel)
  , tcp_server_(tcp_server)
  , key_(key) {

  if (key_.empty() || key_.size() != 32) {
    BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << key_.size() << " bytes. Expected 32 bytes.";
    throw std::invalid_argument("Invalid cryptographic key size");
  }

  BOOST_LOG_TRIVIAL(info) << "PeerManager initialized with key size: " << key_.size() << " bytes";
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
           BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
         }
       }
     );

    // Start stream processing
    if (!peer->start_stream_processing()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to start stream processing for peer: " << peer_id;
    return;
    }

    BOOST_LOG_TRIVIAL(info) << "Accepted and initialized new connection from " << peer_id;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error handling new connection: " << e.what();
  }

  // Continue accepting new connections if server is still running
  tcp_server_.start_accept();
}

void PeerManager::add_peer(std::shared_ptr<TCP_Peer> peer) {
  if (!peer) {
    BOOST_LOG_TRIVIAL(error) << "Attempted to add null peer";
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  uint8_t peer_id = peer->get_peer_id();

  if (has_peer(peer_id)) {
    BOOST_LOG_TRIVIAL(warning) << "Peer with ID " << peer_id << " already exists";
    return;
  }

  peers_[peer_id] = peer;
  BOOST_LOG_TRIVIAL(info) << "Added peer with ID: " << peer_id;
}

void PeerManager::remove_peer(uint8_t peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it != peers_.end()) {
    disconnect(peer_id);
    peers_.erase(it);
    BOOST_LOG_TRIVIAL(info) << "Removed peer with ID: " << peer_id;
  } else {
    BOOST_LOG_TRIVIAL(warning) << "Attempted to remove non-existent peer: " << peer_id;
  }
}

bool PeerManager::disconnect(uint8_t peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it == peers_.end()) {
    BOOST_LOG_TRIVIAL(warning) << "Cannot disconnect - peer not found: " << peer_id;
    return false;
  }

  try {
    auto& peer = it->second;
    peer->stop_stream_processing();
    peer->cleanup_connection();
    BOOST_LOG_TRIVIAL(info) << "Successfully disconnected peer: " << peer_id;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Disconnect error for peer " << peer_id 
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
  std::streampos initial_pos = input_stream.tellg();

  for (auto& peer_pair : peers_) {
    try {
      input_stream.seekg(initial_pos);

      if (!is_connected(peer_pair.first)) {
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

bool PeerManager::send_to_peer(uint8_t peer_id, std::istream& stream) {
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

  if (!is_connected(peer_id)) {
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
      disconnect(peer_pair.first);
      BOOST_LOG_TRIVIAL(debug) << "Disconnected peer: " << peer_pair.first;
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Error disconnecting peer " << peer_pair.first 
                  << ": " << e.what();
    }
  }

  peers_.clear();
  BOOST_LOG_TRIVIAL(info) << "PeerManager shutdown complete";
}

std::size_t PeerManager::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return peers_.size();
}

} // namespace network
} // namespace dfs