#pragma once

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
#include <functional>
#include <boost/system/error_code.hpp>
#include "network/peer_manager.hpp"

namespace dfs {
namespace network {

class PeerManager; // Forward declaration

class TCP_Server {
public:
  friend class PeerManager;  // Gives PeerManager access to private members

  TCP_Server(const uint16_t port, const std::string& address, const uint8_t ID);

  // Initialization and teardown of server
  bool start_listener();
  void shutdown();

  // Establishes connection to remote host
  bool connect(const std::string& remote_address, uint16_t remote_port);

  // Sets the peer manager after construction
  void set_peer_manager(PeerManager& peer_manager);

  ~TCP_Server();

private:
  void start_accept();

  bool initiate_connection(const std::string& remote_address, uint16_t remote_port, 
  std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

  // Async handshaking methods
  void initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
  void receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket);

  // Async ID exchange methods
  void async_send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                    std::function<void(const boost::system::error_code&)> callback);
  void async_read_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                    std::function<void(const boost::system::error_code&, uint8_t)> callback);

  // Parameters
  boost::asio::io_context io_context_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  PeerManager* peer_manager_;  // Changed from reference to pointer
  bool is_running_;
  std::unique_ptr<std::thread> io_thread_;
  const uint16_t port_;
  const std::string address_;
  const uint8_t ID_;
};

} // namespace network
} // namespace dfs