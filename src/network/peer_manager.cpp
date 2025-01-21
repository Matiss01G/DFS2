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

void TCP_Server::create_peer(const boost::system::error_code& error,
        std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  if (!error) {
  try {
    // Create a unique peer ID based on endpoint
    auto endpoint = socket->remote_endpoint();
    std::string peer_id = endpoint.address().to_string() + ":" + 
        std::to_string(endpoint.port());

    // Create new TCP peer with channel and default key
    std::vector<uint8_t> default_key(32, 0); // 32 bytes of zeros as default key
    auto peer = std::make_shared<TCP_Peer>(peer_id, channel_, default_key);

    // Move the accepted socket to the peer
    peer->get_socket() = std::move(*socket);

    // Set up a basic stream processor that forwards data to channel
    peer->set_stream_processor([this, peer_id](std::istream& stream, const std::string& source_id) {
    try {
      std::string data;
      std::getline(stream, data);
      if (!data.empty()) {
      BOOST_LOG_TRIVIAL(debug) << "[" << peer_id << "] Received data: " << data;
      }
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id << "] Stream processing error: " << e.what();
    }
    });

    // Start stream processing before adding to peer manager
    if (!peer->start_stream_processing()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to start stream processing for peer: " << peer_id;
    return;
    }

    // Add peer to manager
    add_peer(peer);

    BOOST_LOG_TRIVIAL(info) << "Accepted and initialized new connection from " << peer_id;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error handling new connection: " << e.what();
  }
  } else {
  BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
  }

  // Continue accepting new connections if server is still running
  if (is_running_) {
    tcp_server_.start_accept();
  }
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
    disconnect(peer_id);
    peers_.erase(it);
    BOOST_LOG_TRIVIAL(info) << "Removed peer with ID: " << peer_id;
  } else {
    BOOST_LOG_TRIVIAL(warning) << "Attempted to remove non-existent peer: " << peer_id;
  }
}

bool PeerManager::connect(const std::string& peer_id, const std::string& address, uint16_t port) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it == peers_.end()) {
    BOOST_LOG_TRIVIAL(error) << "Cannot connect - peer not found: " << peer_id;
    return false;
  }

  try {
    auto& peer_socket = it->second->get_socket();
    if (peer_socket.is_open()) {
      BOOST_LOG_TRIVIAL(error) << "Cannot connect - socket already connected for peer: " << peer_id;
      return false;
    }

    boost::asio::ip::tcp::resolver resolver(peer_socket.get_executor());
    boost::system::error_code resolve_ec;
    auto endpoints = resolver.resolve(address, std::to_string(port), resolve_ec);

    if (resolve_ec) {
      BOOST_LOG_TRIVIAL(error) << "DNS resolution failed for peer " << peer_id 
                  << ": " << resolve_ec.message();
      return false;
    }

    boost::system::error_code connect_ec;
    peer_socket.connect(*endpoints.begin(), connect_ec);

    if (connect_ec) {
      BOOST_LOG_TRIVIAL(error) << "Connection failed for peer " << peer_id 
                  << ": " << connect_ec.message();
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully connected peer " << peer_id 
                << " to " << address << ":" << port;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Connection error for peer " << peer_id 
                << ": " << e.what();
    return false;
  }
}

bool PeerManager::disconnect(const std::string& peer_id) {
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

bool PeerManager::is_connected(const std::string& peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = peers_.find(peer_id);
  if (it == peers_.end()) {
    return false;
  }

  return it->second->get_socket().is_open();
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

bool PeerManager::send_to_peer(const std::string& peer_id, std::istream& stream) {
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