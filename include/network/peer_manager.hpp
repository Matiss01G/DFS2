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
#include "utils/pipeliner.hpp"


namespace dfs {
namespace network {

class TCP_Server; 

class PeerManager {
public:  
  // Delete copy constructor and assignment operator
  PeerManager(const PeerManager&) = delete;
  PeerManager& operator=(const PeerManager&) = delete;

  
  // ---- CONSTRUCTOR AND DESTRUCTOR ----
  PeerManager(Channel& channel, TCP_Server& tcp_server, const std::vector<uint8_t>& key);
  ~PeerManager();

  
  // ---- CONNECTION MANAGEMENT ----
  bool disconnect(uint8_t peer_id);
  bool is_connected(uint8_t peer_id);

  
  // ---- PEER MANAGEMENT ----
  void create_peer(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint8_t peer_id);
  void add_peer(const std::shared_ptr<TCP_Peer> peer);
  void remove_peer(uint8_t peer_id);
  bool has_peer(uint8_t peer_id);
  std::shared_ptr<TCP_Peer> get_peer(uint8_t peer_id);

  
  // ---- STREAM OPERATIONS ----
  // Sends to a single peer
  bool send_to_peer(uint8_t peer_id, dfs::utils::Pipeliner& pipeline);
  // Sends to all connected peers
  bool broadcast_stream(dfs::utils::Pipeliner& pipeline);

  
  // ---- UTILITY METHODS ----
  std::size_t size() const;
  void shutdown();

private:
  // ---- PARAMETERS ----
  // System components
  Channel& channel_;
  TCP_Server& tcp_server_;

  // Cryptographic key
  std::vector<uint8_t> key_;

  // Peers map and access mutex
  std::map<uint8_t, std::shared_ptr<TCP_Peer>> peers_;
  mutable std::mutex mutex_;
};

} // namespace network
} // namespace dfs

#endif // PEER_MANAGER_HPP