#include "network/tcp_server.hpp"
#include "network/tcp_peer.hpp"
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

void TCP_Server::start_accept() {
  if (!acceptor_ || !is_running_) {
    return;
  }

  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

  acceptor_->async_accept(*socket,
    [this, socket](const boost::system::error_code& error) {
      if (!error) {
        handle_new_connection(socket);
      } else {
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
      }
      start_accept(); // Continue accepting new connections
    });
}
    
void TCP_Server::handle_new_connection(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  try {
    auto remote_endpoint = socket->remote_endpoint();

    // Check if this peer already exists
    if (auto existing_peer = peer_manager_.find_peer_by_endpoint(remote_endpoint)) {
        BOOST_LOG_TRIVIAL(warning) << "Rejecting connection from " 
            << remote_endpoint.address().to_string() << ":" 
            << remote_endpoint.port() << " - peer already exists";

        // Close the new socket
        boost::system::error_code ec;
        socket->close(ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Error closing duplicate connection: " << ec.message();
        }
        return;
    }
    
    // Get the source port from the remote endpoint
    uint16_t source_port = socket->local_endpoint().port();
    BOOST_LOG_TRIVIAL(debug) << "New connection from port: " << source_port;

    // Send the source port back to the client
    send_local_port(socket, local_port);

    // Then create the peer
    peer_manager_.create_peer(boost::system::error_code(), socket);

  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error handling new connection: " << e.what();
  }
}

void TCP_Server::send_local_port(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint16_t local_port) {
  try {
    // Convert port to network byte order
    uint16_t port_network = htons(local_port);

    // Send the port number
    boost::system::error_code ec;
    boost::asio::write(*socket, boost::asio::buffer(&port_network, sizeof(port_network)), ec);

    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "Error sending local port: " << ec.message();
    }
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error sending local port: " << e.what();
  }
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