#pragma once

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
#include "network/peer_manager.hpp"

namespace dfs {
namespace network {

class TCP_Server {
public:
  TCP_Server(uint16_t port, const std::string& address, PeerManager& peer_manager);

  bool start_listener();

  void shutdown();

  ~TCP_Server();

private:

  void start_accept();

  boost::asio::io_context io_context_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
  PeerManager& peer_manager_;
  bool is_running_;
  std::unique_ptr<std::thread> io_thread_;
  const uint16_t port_;
  const std::string address_;
};

} // namespace network
} // namespace dfs
