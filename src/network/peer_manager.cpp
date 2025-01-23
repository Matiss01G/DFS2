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
    std::lock_guard<std::mutex> lock(mutex_);

    if (has_peer(peer_id)) {
      BOOST_LOG_TRIVIAL(warning) << "Peer " << peer_id << " already exists";
      return;
    }

    BOOST_LOG_TRIVIAL(debug) << "Creating new peer with ID: " << peer_id;

    // Create new TCP peer with channel and key
    auto peer = std::make_shared<TCP_Peer>(peer_id, channel_, key_);

    // Move the socket to the peer
    peer->get_socket() = std::move(*socket);
    socket.reset(); // Clear the original socket pointer

    // Set up stream processor before adding to map
    peer->set_stream_processor(
      [peer](std::istream& stream) {
        try {
          peer->codec_->deserialize(stream);
        } catch (const std::exception& e) {
          BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
        }
      }
    );

    // Add peer to map first
    peers_[peer_id] = peer;

    // Start stream processing only after peer is in the map
    if (!peer->start_stream_processing()) {
      BOOST_LOG_TRIVIAL(error) << "Failed to start stream processing for peer: " << peer_id;
      peers_.erase(peer_id);
      return;
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully created and initialized peer: " << peer_id;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error creating peer " << peer_id << ": " << e.what();
    throw; // Rethrow to notify caller
  }
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
    BOOST_LOG_TRIVIAL(debug) << "Starting disconnect process for peer: " << peer_id;

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

  bool connected = it->second->get_socket().is_open();
  BOOST_LOG_TRIVIAL(debug) << "Peer " << peer_id << " connection status: " << (connected ? "connected" : "disconnected");
  return connected;
}

bool PeerManager::has_peer(uint8_t peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  bool exists = peers_.find(peer_id) != peers_.end();
  BOOST_LOG_TRIVIAL(debug) << "Checking for peer " << peer_id << ": " << (exists ? "exists" : "not found");
  return exists;
}

std::shared_ptr<TCP_Peer> PeerManager::get_peer(uint8_t peer_id) {
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
      disconnect(peer_pair.first);
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


} // namespace network
} // namespace dfs