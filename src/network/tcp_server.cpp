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


void TCP_Server::async_send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                             std::function<void(const boost::system::error_code&)> callback) {
  BOOST_LOG_TRIVIAL(debug) << "Starting to send ID asynchronously";

  boost::asio::async_write(*socket, 
    boost::asio::buffer(&ID_, sizeof(ID_)),
    [this, callback](const boost::system::error_code& ec, std::size_t bytes_written) {
      if (!ec) {
        BOOST_LOG_TRIVIAL(info) << "Successfully sent ID: " << static_cast<int>(ID_);
        callback(ec);
      } else {
        BOOST_LOG_TRIVIAL(error) << "Failed to send ID: " << ec.message();
        callback(ec);
      }
    });
}

void TCP_Server::async_read_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                             std::function<void(const boost::system::error_code&, uint8_t)> callback) {
  BOOST_LOG_TRIVIAL(debug) << "Starting to read ID asynchronously";

  auto peer_id_buffer = std::make_shared<uint8_t>();

  boost::asio::async_read(*socket,
    boost::asio::buffer(peer_id_buffer.get(), sizeof(uint8_t)),
    [this, callback, peer_id_buffer, socket](const boost::system::error_code& ec, std::size_t bytes_read) {
      if (!ec) {
        BOOST_LOG_TRIVIAL(info) << "Received ID: " << static_cast<int>(*peer_id_buffer);
        callback(ec, *peer_id_buffer);
      } else {
        BOOST_LOG_TRIVIAL(error) << "Failed to read ID: " << ec.message();
        callback(ec, 0);
      }
    });
}

void TCP_Server::initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  BOOST_LOG_TRIVIAL(debug) << "Initiating async handshake request";

  async_send_ID(socket, 
    [this, socket](const boost::system::error_code& send_ec) {
      if (send_ec) {
        BOOST_LOG_TRIVIAL(error) << "Failed to send ID during handshake: " << send_ec.message();
        socket->close();
        return;
      }

      BOOST_LOG_TRIVIAL(debug) << "Successfully sent ID, waiting for peer ID";

      async_read_ID(socket,
        [this, socket](const boost::system::error_code& read_ec, uint8_t peer_id) {
          if (read_ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to read ID during handshake: " << read_ec.message();
            socket->close();
            return;
          }

          BOOST_LOG_TRIVIAL(debug) << "Successfully received peer ID: " << static_cast<int>(peer_id);

          if (peer_manager_ && !peer_manager_->has_peer(peer_id)) {
            BOOST_LOG_TRIVIAL(debug) << "Creating new peer with ID: " << static_cast<int>(peer_id);
            try {
              peer_manager_->create_peer(socket, peer_id);
              BOOST_LOG_TRIVIAL(info) << "Successfully completed handshake with peer: " << static_cast<int>(peer_id);
            } catch (const std::exception& e) {
              BOOST_LOG_TRIVIAL(error) << "Failed to create peer: " << e.what();
              socket->close();
            }
          } else {
            BOOST_LOG_TRIVIAL(warning) << "Peer already exists or no peer manager available";
            socket->close();
          }
        });
    });
}

void TCP_Server::receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
  BOOST_LOG_TRIVIAL(debug) << "Receiving async handshake request";

  if (!peer_manager_) {
    BOOST_LOG_TRIVIAL(error) << "No PeerManager set";
    socket->close();
    return;
  }

  async_read_ID(socket,
    [this, socket](const boost::system::error_code& read_ec, uint8_t peer_id) {
      if (read_ec) {
        BOOST_LOG_TRIVIAL(error) << "Failed to read ID during receive handshake: " << read_ec.message();
        socket->close();
        return;
      }

      BOOST_LOG_TRIVIAL(debug) << "Received peer ID: " << static_cast<int>(peer_id) 
                              << ", checking if peer exists";

      if (peer_manager_->has_peer(peer_id)) {
        BOOST_LOG_TRIVIAL(warning) << "Peer " << static_cast<int>(peer_id) << " already exists";
        socket->close();
        return;
      }

      BOOST_LOG_TRIVIAL(debug) << "Preparing to send ID back to peer: " << static_cast<int>(ID_);

      async_send_ID(socket,
        [this, socket, peer_id](const boost::system::error_code& send_ec) {
          if (send_ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to send ID back during receive handshake: " << send_ec.message();
            socket->close();
            return;
          }

          BOOST_LOG_TRIVIAL(debug) << "Successfully sent ID back to peer";

          try {
            peer_manager_->create_peer(socket, peer_id);
            BOOST_LOG_TRIVIAL(debug) << "Handshake complete for peer: " << static_cast<int>(peer_id);
          } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Failed to create peer: " << e.what();
            socket->close();
          }
        });
    });
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
    BOOST_LOG_TRIVIAL(error) << "Failed to establish initial connection to " << remote_address << ":" << remote_port;
    return false;
  }

  BOOST_LOG_TRIVIAL(info) << "Starting handshake process with " << remote_address << ":" << remote_port;

  // Add exponential backoff retry for connection verification
  const int MAX_RETRIES = 5;
  const int BASE_DELAY_MS = 100;

  initiate_handshake(socket);

  // Wait for handshake completion with retries
  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(BASE_DELAY_MS * (1 << retry)));

    if (!socket->is_open()) {
      BOOST_LOG_TRIVIAL(error) << "Socket closed during handshake attempt " << retry + 1;
      return false;
    }

    // Get remote endpoint for logging
    boost::system::error_code ec;
    auto remote_endpoint = socket->remote_endpoint(ec);
    if (!ec) {
      BOOST_LOG_TRIVIAL(debug) << "Checking connection status for " 
                              << remote_endpoint.address().to_string() 
                              << ":" << remote_endpoint.port() 
                              << " (attempt " << retry + 1 << ")";
    }

    // Check for successful peer registration
    if (peer_manager_) {
      // Look for any peers that are connected through this socket
      bool peer_found = false;
      for (uint8_t potential_id = 0; potential_id < 255; potential_id++) {
        if (peer_manager_->has_peer(potential_id)) {
          BOOST_LOG_TRIVIAL(info) << "Connection established and peer registered with ID: " << static_cast<int>(potential_id);
          return true;
        }
      }
    }
  }

  BOOST_LOG_TRIVIAL(error) << "Failed to verify peer registration after " << MAX_RETRIES << " attempts";
  return false;
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

    // Start accepting connections
    BOOST_LOG_TRIVIAL(debug) << "Starting to accept connections";
    start_accept();

    BOOST_LOG_TRIVIAL(debug) << "Starting IO context";
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