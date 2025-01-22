#pragma once

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
#include "network/peer_manager.hpp"

namespace dfs {
namespace network {

class PeerManager; // Forward declaration

class TCP_Server {
public:
  friend class PeerManager;  // Gives PeerManager access to private members

  TCP_Server(const uint16_t port, const std::string& address, const uint8_t ID, PeerManager& peer_manager);

  // Initialization and teardown of server
  bool start_listener();
  void shutdown();

  // Establishes connection to remove host
  bool connect(const std::string& remote_address, uint16_t remote_port);

  ~TCP_Server();

private:
  void start_accept();\

  bool initiate_connection(const std::string& remote_address, uint16_t remote_port, 
  std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

  // Handshaking methods
  bool initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
  void receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
  bool send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
  uint8_t read_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

  // Parameters
  boost::asio::io_context io_context_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  PeerManager& peer_manager_;
  bool is_running_;
  std::unique_ptr<std::thread> io_thread_;
  const uint16_t port_;
  const std::string address_;
  const uint8_t ID_;
};

} // namespace network
} // namespace dfs
