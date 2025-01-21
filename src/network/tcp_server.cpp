#include "network/tcp_server.hpp"
#include "network/tcp_peer.hpp"
#include "network/peer_manager.hpp"
#include <boost/bind/bind.hpp>
#include <thread>

namespace dfs {
namespace network {

TCP_Server::TCP_Server(uint16_t port,
           const std::string& address,
           PeerManager& peer_manager)
  : peer_manager_(peer_manager)
  , is_running_(false)
  , port_(port)
  , address_(address) {

  BOOST_LOG_TRIVIAL(info) << "Initializing TCP server on " << address << ":" << port;
}

bool TCP_Server::start_listener() {
  if (is_running_) {
    BOOST_LOG_TRIVIAL(warning) << "Server already running";
    return false;
  }

  try {
    // Create endpoint
    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::make_address(address_),
      port_
    );

    // Create acceptor
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
      io_context_,
      endpoint
    );

    // Start accepting connections
    start_accept();

    // Start io_context in a separate thread
    is_running_ = true;
    io_thread_ = std::make_unique<std::thread>([this]() {
      try {
        boost::asio::io_context::work work(io_context_);
        io_context_.run();
      } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "IO context error: " << e.what();
        is_running_ = false;
      }
    });

    BOOST_LOG_TRIVIAL(info) << "Server started successfully on " << address_ << ":" << port_;
    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to start server: " << e.what();
    return false;
  }
}

void TCP_Server::create_peer(const boost::system::error_code& error,
        std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  if (!error) {
    try {
      // Create a unique peer ID based on endpoint
      auto endpoint = socket->remote_endpoint();
      std::string peer_id = endpoint.address().to_string() + ":" + 
          std::to_string(endpoint.port());

      // Create new TCP peer
      auto peer = std::make_shared<TCP_Peer>(peer_id);

      // Move the accepted socket to the peer
      peer->get_socket() = std::move(*socket);

      // Add peer to manager
      peer_manager_.add_peer(peer);

      BOOST_LOG_TRIVIAL(info) << "Accepted and initialized new connection from " << peer_id;
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Error handling new connection: " << e.what();
    }
  } else {
    BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
  }

  // Continue accepting new connections if server is still running
  if (is_running_) {
    start_accept();
  }
}

void TCP_Server::start_accept() {
  if (!acceptor_ || !is_running_) {
    return;
  }

  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

  acceptor_->async_accept(*socket,
    [this, socket](const boost::system::error_code& error) {
      create_peer(error, socket);
    });
}

void TCP_Server::shutdown() {
  if (!is_running_) {
    return;
  }

  BOOST_LOG_TRIVIAL(info) << "Initiating server shutdown";

  is_running_ = false;

  // Stop accepting new connections
  if (acceptor_ && acceptor_->is_open()) {
    boost::system::error_code ec;
    acceptor_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "Error closing acceptor: " << ec.message();
    }
  }

  // Stop io_context
  io_context_.stop();

  // Wait for io_thread to finish
  if (io_thread_ && io_thread_->joinable()) {
    io_thread_->join();
  }

  BOOST_LOG_TRIVIAL(info) << "Server shutdown complete";
}

TCP_Server::~TCP_Server() {
  shutdown();
}

} // namespace network
} // namespace dfs