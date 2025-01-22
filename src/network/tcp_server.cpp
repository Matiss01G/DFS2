#include "network/tcp_server.hpp"
#include "network/tcp_peer.hpp"
#include <boost/bind/bind.hpp>
#include <thread>

namespace dfs {
namespace network {

TCP_Server::TCP_Server(const uint16_t port, const std::string& address, const uint8_t ID, PeerManager& peer_manager)
  : peer_manager_(peer_manager)
  , is_running_(false)
  , port_(port)
  , address_(address)
  , ID_(ID) {
  BOOST_LOG_TRIVIAL(info) << "Initializing TCP server " << ID << " on " << address << ":" << port;
}


bool TCP_Server::send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  try {
    // Write ID_ as raw bytes to socket
    boost::asio::write(*socket, boost::asio::buffer(&ID_, sizeof(ID_)));
    BOOST_LOG_TRIVIAL(info) << "Sent ID: " << static_cast<int>(ID_);
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to send ID: " << e.what();
    return false;
  }
}

  
uint8_t TCP_Server::read_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  uint8_t peer_id;
  try {
    // Read exact number of bytes for ID
    boost::asio::read(*socket, boost::asio::buffer(&peer_id, sizeof(peer_id)));
    BOOST_LOG_TRIVIAL(info) << "Received ID: " << static_cast<int>(peer_id);
    return peer_id;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to read ID: " << e.what();
    throw;
  }
}

  
// Initiates connection and performs ID exchange with remote server
bool TCP_Server::initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  try {
    if (!send_ID(socket)) {
      return false;
    }
    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Handshake failed: " << e.what();
    return false;
  }
}

  
// Handles incoming connection by performing ID exchange and peer creation
void TCP_Server::receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  try {
    // Read remote server's ID
    uint8_t peer_id = read_ID(socket);

    // Check if we already have this peer
    if (peer_manager_.has_peer(peer_id)) {
      BOOST_LOG_TRIVIAL(warning) << "Peer " << peer_id << " already exists, terminating connection";
      socket->close();
    }

    // Create new peer with the received ID
    peer_manager_.create_peer(socket, peer_id);
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Handshake failed: " << e.what();
    socket->close();
  }
}


bool TCP_Server::initiate_connection(const std::string& remote_address, uint16_t remote_port,
                                   std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
  try {
    // Resolve remote address to endpoints
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(remote_address, std::to_string(remote_port));

    // Connect to the first available endpoint
    boost::asio::connect(*socket, endpoints);

    BOOST_LOG_TRIVIAL(info) << "Successfully connected to " << remote_address << ":" << remote_port;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Connection failed: " << e.what();
    return false;
  }
}

  
bool TCP_Server::connect(const std::string& remote_address, uint16_t remote_port) {
  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

  if (!initiate_connection(remote_address, remote_port, socket)) {
    return false;
  }

  return initiate_handshake(socket);
}

  
void TCP_Server::start_accept() {
  if (!acceptor_ || !is_running_) {
    return;
  }

  // Create new socket for incoming connection
  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

  // Set up async accept operation
  acceptor_->async_accept(*socket,
    [this, socket](const boost::system::error_code& error) {
      if (!error) {
        receive_handshake(socket);  // Process new connection with handshake
      } else {
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
      }
      start_accept();  // Continue accepting new connections
    });
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