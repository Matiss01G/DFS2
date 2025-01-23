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
    BOOST_LOG_TRIVIAL(debug) << "Starting to send ID";
    socket->set_option(boost::asio::ip::tcp::no_delay(true));
    boost::system::error_code ec;
    size_t bytes_written = boost::asio::write(*socket, boost::asio::buffer(&ID_, sizeof(ID_)), ec);

    if (ec || bytes_written != sizeof(ID_)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to send complete ID: " << ec.message();
      return false;
    }

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
    socket->set_option(boost::asio::ip::tcp::no_delay(true));
    boost::system::error_code ec;
    size_t bytes_read = boost::asio::read(*socket, boost::asio::buffer(&peer_id, sizeof(peer_id)), ec);

    if (ec || bytes_read != sizeof(peer_id)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to read complete ID: " << ec.message();
      throw std::runtime_error("Failed to read peer ID");
    }

    BOOST_LOG_TRIVIAL(info) << "Received ID: " << static_cast<int>(peer_id);
    return peer_id;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to read ID: " << e.what();
    throw;
  }
}

void TCP_Server::receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  if (!peer_manager_) {
    BOOST_LOG_TRIVIAL(error) << "No PeerManager set for TCP server " << ID_;
    socket->close();
    return;
  }

  try {
    // Configure socket for sync operations during handshake
    socket->set_option(boost::asio::ip::tcp::no_delay(true));
    socket->non_blocking(false);

    // First read peer's ID
    uint8_t peer_id = read_ID(socket);
    BOOST_LOG_TRIVIAL(info) << "Successfully read peer ID: " << static_cast<int>(peer_id);

    if (peer_manager_->has_peer(peer_id)) {
      BOOST_LOG_TRIVIAL(warning) << "Peer " << peer_id << " already exists";
      socket->close();
      return;
    }

    // Then send our ID back
    if (!send_ID(socket)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to send ID back to peer";
      socket->close();
      return;
    }
    BOOST_LOG_TRIVIAL(info) << "Successfully sent our ID back to peer";

    // Create peer after successful ID exchange
    peer_manager_->create_peer(socket, peer_id);

    // Verify peer creation was successful
    if (!peer_manager_->has_peer(peer_id) || !peer_manager_->is_connected(peer_id)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to verify peer creation and connection";
      socket->close();
      return;
    }

    BOOST_LOG_TRIVIAL(info) << "Handshake complete - peer " << static_cast<int>(peer_id) 
                << " successfully registered";
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Handshake failed: " << e.what();
    socket->close();
  }
}

bool TCP_Server::initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  if (!socket || !socket->is_open()) {
    BOOST_LOG_TRIVIAL(error) << "Invalid socket for handshake";
    return false;
  }

  try {
    // Configure socket for sync operations during handshake
    socket->set_option(boost::asio::ip::tcp::no_delay(true));
    socket->non_blocking(false);

    // First send our ID
    if (!send_ID(socket)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to send our ID";
      return false;
    }

    // Then read peer's ID
    uint8_t peer_id = read_ID(socket);
    BOOST_LOG_TRIVIAL(info) << "Successfully read peer ID: " << static_cast<int>(peer_id);

    // Create peer after successful ID exchange
    if (peer_manager_) {
      if (peer_manager_->has_peer(peer_id)) {
        BOOST_LOG_TRIVIAL(warning) << "Peer " << peer_id << " already exists";
        return false;
      }

      peer_manager_->create_peer(socket, peer_id);

      // Verify peer creation was successful
      if (!peer_manager_->has_peer(peer_id) || !peer_manager_->is_connected(peer_id)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to verify peer creation and connection";
        return false;
      }

      BOOST_LOG_TRIVIAL(info) << "Handshake complete - peer " << static_cast<int>(peer_id) 
                  << " successfully registered";
      return true;
    }

    BOOST_LOG_TRIVIAL(error) << "No peer manager available";
    return false;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Handshake failed: " << e.what();
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

  auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

  acceptor_->async_accept(*socket,
    [this, socket](const boost::system::error_code& error) {
      if (!error) {
        BOOST_LOG_TRIVIAL(info) << "New connection accepted";
        try {
          // Handle handshake synchronously to ensure completion
          receive_handshake(socket);
        }
        catch (const std::exception& e) {
          BOOST_LOG_TRIVIAL(error) << "Error in connection handler: " << e.what();
          socket->close();
        }
      }
      else if (error != boost::asio::error::operation_aborted) {
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
      }

      // Continue accepting new connections if still running
      if (is_running_) {
        start_accept();
      }
    });
}

bool TCP_Server::start_listener() {
  if (is_running_) {
    BOOST_LOG_TRIVIAL(warning) << "Server already running";
    return false;
  }

  try {
    boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::make_address(address_),
      port_
    );

    acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
      io_context_,
      endpoint
    );

    BOOST_LOG_TRIVIAL(debug) << "Starting to accept connections";
    start_accept();

    BOOST_LOG_TRIVIAL(debug) << "Starting IO context";
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

bool TCP_Server::initiate_connection(const std::string& remote_address, uint16_t remote_port,
                                   std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Resolving address to endpoints";

    boost::asio::ip::tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(remote_address, std::to_string(remote_port));

    BOOST_LOG_TRIVIAL(info) << "Attempting to connect to " << remote_address << ":" << remote_port;

    boost::asio::connect(*socket, endpoints);
    socket->set_option(boost::asio::ip::tcp::no_delay(true));

    BOOST_LOG_TRIVIAL(info) << "Successfully connected to " << remote_address << ":" << remote_port;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Connection failed: " << e.what();
    return false;
  }
}

void TCP_Server::shutdown() {
  if (!is_running_) {
    return;
  }

  BOOST_LOG_TRIVIAL(info) << "Initiating server shutdown";
  is_running_ = false;

  if (acceptor_ && acceptor_->is_open()) {
    boost::system::error_code ec;
    acceptor_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "Error closing acceptor: " << ec.message();
    }
  }

  io_context_.stop();

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