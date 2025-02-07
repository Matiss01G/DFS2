#pragma once

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
#include "network/peer_manager.hpp"

namespace dfs {
namespace network {

class PeerManager; 

class TCP_Server {
public:
  friend class PeerManager;  // Gives PeerManager access to private members

  
  // -- CONSTRUCTOR AND DESTRUCTOR ----
  TCP_Server(const uint16_t port, const std::string& address, const uint8_t ID);
  ~TCP_Server();

  
  // ---- INITIALIZATION AND TEARDOWN ----
  bool start_listener();
  void shutdown();

  
  // ---- CONNECTION INITIATION ----
  // Establishes connection to remove host
  bool connect(const std::string& remote_address, uint16_t remote_port);

  
  // ---- GETTERS AND SETTERS ----
  void set_peer_manager(PeerManager& peer_manager);

private:

  // ---- PARAMETERS ----
  // Local ID
  const uint8_t ID_;
  
  // Network Parameters
  const uint16_t port_;
  const std::string address_;

  // Server state
  std::unique_ptr<std::thread> io_thread_;
  bool is_running_;
  
  // Incoming connection handlers
  boost::asio::io_context io_context_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;

  // System components
  PeerManager* peer_manager_; 

  
  // ---- INITIALIZATION AND TEARDOWN ----
  // Main listening loop that handles incoming connections
  void start_accept();

  
  // ---- CONNECTION INITIATION ----
  bool initiate_connection(const std::string& remote_address, uint16_t remote_port, 
  std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

  
  // ---- HANDSHAKE INITIATION ----
  // Sends ID then waits to receive remote ID
  bool initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
  // Performs ID transmission
  bool send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

  
  // ---- HANDSHAKE RECEPTION ----
  // Receives remote ID and sends local ID back 
  void receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
  // Performs ID reading
  uint8_t read_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

};

} // namespace network
} // namespace dfs