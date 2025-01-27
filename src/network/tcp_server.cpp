#include "network/tcp_server.hpp"
#include "network/tcp_peer.hpp"
#include <boost/bind/bind.hpp>
#include <thread>

namespace dfs {
namespace network {

TCP_Server::TCP_Server(const uint16_t port, const std::string& address, const uint8_t ID)
  : peer_manager_(nullptr)
  , is_running_(false)
  , port_(port)
  , address_(address)
  , ID_(ID) {
  BOOST_LOG_TRIVIAL(info) << "Initializing TCP server " << ID << " on " << address << ":" << port;
}

void TCP_Server::set_peer_manager(PeerManager& peer_manager) {
  peer_manager_ = &peer_manager;
  BOOST_LOG_TRIVIAL(info) << "PeerManager set for TCP server " << ID_;
}

bool TCP_Server::send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  try {
    // Write ID_ as raw bytes to socket
    BOOST_LOG_TRIVIAL(debug) << "Starting to send ID";
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
  BOOST_LOG_TRIVIAL(debug) << "Starting to read ID";
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

bool TCP_Server::initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  BOOST_LOG_TRIVIAL(debug) << "Initiating handshake request";
  try {
    if (!send_ID(socket)) {
      return false;
    }

    uint8_t peer_id = read_ID(socket);
    // Create peer only after full ID exchange
    if (peer_manager_ && !peer_manager_->has_peer(peer_id)) {
      BOOST_LOG_TRIVIAL(debug) << "Creating new peer with ID: " << static_cast<int>(peer_id);
      peer_manager_->create_peer(socket, peer_id);
      return true;
    }
    BOOST_LOG_TRIVIAL(warning) << "Peer with ID " << static_cast<int>(peer_id) << " already exists";
    return false;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Handshake failed: " << e.what();
    return false;
  }
}

void TCP_Server::receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  BOOST_LOG_TRIVIAL(debug) << "Receiving handshake request";
  if (!peer_manager_) {
    BOOST_LOG_TRIVIAL(error) << "No PeerManager set";
    socket->close();
    return;
  }

  try {
    uint8_t peer_id = read_ID(socket);
    if (peer_manager_->has_peer(peer_id)) {
      BOOST_LOG_TRIVIAL(warning) << "Peer " << static_cast<int>(peer_id) << " already exists";
      socket->close();
      return;
    }

    BOOST_LOG_TRIVIAL(debug) << "Preparing to send ID back to peer: " << static_cast<int>(ID_);
    if (!send_ID(socket)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to send ID back to peer";
      socket->close();
      return;
    }
    BOOST_LOG_TRIVIAL(debug) << "Successfully sent ID back to peer";

    BOOST_LOG_TRIVIAL(debug) << "Creating new peer with ID: " << static_cast<int>(peer_id);
    // Create peer only after full ID exchange
    peer_manager_->create_peer(socket, peer_id);
    BOOST_LOG_TRIVIAL(debug) << "Handshake complete for peer: " << static_cast<int>(peer_id);
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Handshake failed: " << e.what();
    socket->close();
  }
}

bool TCP_Server::initiate_connection(const std::string& remote_address, uint16_t remote_port,
                                   std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Resolving address to endpoints";

    // Resolve remote address to endpoints
    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(remote_address, std::to_string(remote_port));

    BOOST_LOG_TRIVIAL(info) << "Attempting to connect to " << remote_address << ":" << remote_port;

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

  BOOST_LOG_TRIVIAL(debug) << "Creating new socket for incoming connection";

  // Create new socket for incoming connection
  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

  // Set up async accept operation
  acceptor_->async_accept(*socket,
    [this, socket](const boost::system::error_code& error) {
      if (!error) {
        BOOST_LOG_TRIVIAL(debug) << "Calling receive_handshake for incoming connection";
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
    BOOST_LOG_TRIVIAL(debug) << "Endpoint created";
    // Create endpoint
    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::make_address(address_),
      port_
    );

    BOOST_LOG_TRIVIAL(debug) << "Acceptor created";
    // Create acceptor
    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
      io_context_,
      endpoint
    );

    is_running_ = true;

    // Start accepting connections
    BOOST_LOG_TRIVIAL(debug) << "Starting to accept connections";
    start_accept();

    BOOST_LOG_TRIVIAL(debug) << "Starting IO context";
    // Start io_context in a separate thread
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