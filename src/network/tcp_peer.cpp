#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

// TCP_Peer represents a network peer connection that can send and receive data streams
// It manages socket connections and provides asynchronous I/O operations
TCP_Peer::TCP_Peer(uint8_t peer_id, Channel& channel, const std::vector<uint8_t>& key)
  : peer_id_(peer_id),  
  socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),  
  input_buffer_(std::make_unique<boost::asio::streambuf>()),
  codec_(std::make_unique<Codec>(key, channel)),  
  message_size_(0) {
  initialize_streams();
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Constructing TCP_Peer";
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input stream initialized";
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] TCP_Peer instance created successfully";
}

// Cleanup connection and resources on destruction
TCP_Peer::~TCP_Peer() {
  cleanup_connection();
  BOOST_LOG_TRIVIAL(debug) << "TCP_Peer destroyed: " << peer_id_;
}

// Initialize input stream buffer for receiving data
void TCP_Peer::initialize_streams() {
  input_stream_ = std::make_unique<std::istream>(input_buffer_.get());
}

// Get input stream if socket is connected, otherwise return nullptr
std::istream* TCP_Peer::get_input_stream() {
  if (!socket_->is_open()) {
    return nullptr;
  }
  return input_stream_.get();
}

// Set callback function for processing received data streams
void TCP_Peer::set_stream_processor(StreamProcessor processor) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting stream processor";
  stream_processor_ = std::move(processor);
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processor configured";
}

// Start asynchronous stream processing if socket is connected and processor is set
bool TCP_Peer::start_stream_processing() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Attempting to start stream processing";

  if (!socket_->is_open() || !stream_processor_) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot start processing - socket not connected or no processor set";
    return false;
  }

  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processing already active";
    return true;
  }

  processing_active_ = true;
  processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing started successfully";
  return true;
}

// Gracefully stop stream processing and cleanup resources
void TCP_Peer::stop_stream_processing() {
  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stopping stream processing";

    processing_active_ = false;

    // Cancel any pending asynchronous operations
    if (socket_ && socket_->is_open()) {
      boost::system::error_code ec;
      socket_->cancel(ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Error canceling socket operations: " << ec.message();
      }
    }

    io_context_.stop();

    // Wait for processing thread to complete
    if (processing_thread_ && processing_thread_->joinable()) {
      processing_thread_->join();
      processing_thread_.reset();
      BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing thread joined";
    }

    io_context_.restart();

    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
  }
}

// Main stream processing loop that handles incoming data
void TCP_Peer::process_stream() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting up stream processing";

  try {
    // Keep io_context running while processing is active
    auto work_guard = boost::asio::make_work_guard(io_context_);

    if (processing_active_ && socket_->is_open()) {
      async_read_next();
    }

    // Process events while active and connected
    while (processing_active_ && socket_->is_open()) {
      io_context_.run_one();
    }

    work_guard.reset();

    // Process any remaining events
    while (io_context_.poll_one()) {}

  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
  }

  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
}

// Setup next asynchronous read operation with size-based framing
void TCP_Peer::async_read_next() {
  if (!processing_active_ || !socket_->is_open()) {
    return;
  }

  BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Setting up next async read";

  // First read message size (4 bytes) - capture the object for lambda
  auto self(shared_from_this());
  boost::asio::async_read(*socket_, 
    boost::asio::buffer(&message_size_, sizeof(message_size_)),
    [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
      if (!ec) {
        // Convert from network byte order
        message_size_ = ntohl(message_size_);
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Reading message of size: " << message_size_;

        // Read the actual message
        boost::asio::async_read(*socket_,
          *input_buffer_,
          boost::asio::transfer_exactly(message_size_),
          [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
              BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Received " << bytes_transferred << " bytes";

              if (stream_processor_) {
                try {
                  boost::asio::ip::tcp::endpoint remote_endpoint = socket_->remote_endpoint();
                  std::string source_id = remote_endpoint.address().to_string() + ":" + 
                              std::to_string(remote_endpoint.port());
                  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing data from " << source_id;

                  std::istream is(input_buffer_.get());
                  stream_processor_(is);
                  input_buffer_->consume(bytes_transferred); // Clear processed data
                } catch (const std::exception& e) {
                  BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processor error: " << e.what();
                }
              }

              // Continue reading next message
              if (processing_active_ && socket_->is_open()) {
                async_read_next();
              }
            } else if (ec != boost::asio::error::operation_aborted) {
              BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Read error: " << ec.message();
              if (processing_active_ && socket_->is_open()) {
                async_read_next();
              }
            }
          });
      } else if (ec != boost::asio::error::operation_aborted) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Size read error: " << ec.message();
        if (processing_active_ && socket_->is_open()) {
          async_read_next();
        }
      }
    });
}

// Send data stream to peer with buffered writing and message size header
bool TCP_Peer::send_stream(std::istream& input_stream, std::size_t buffer_size) {
  if (!socket_ || !socket_->is_open()) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot send stream - socket not connected";
    return false;
  }

  try {
    std::unique_lock<std::mutex> lock(io_mutex_);

    // Get stream size
    input_stream.seekg(0, std::ios::end);
    uint32_t stream_size = static_cast<uint32_t>(input_stream.tellg());
    input_stream.seekg(0, std::ios::beg);

    // Send size header in network byte order
    uint32_t size_network = htonl(stream_size);
    boost::system::error_code ec;
    boost::asio::write(*socket_, boost::asio::buffer(&size_network, sizeof(size_network)), ec);

    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Failed to send size header: " << ec.message();
      return false;
    }

    // Send actual data
    std::vector<char> buffer(buffer_size);
    while (input_stream.good()) {
      input_stream.read(buffer.data(), buffer_size);
      std::size_t bytes_read = input_stream.gcount();

      if (bytes_read == 0) {
        break;
      }

      std::size_t bytes_written = boost::asio::write(
        *socket_,
        boost::asio::buffer(buffer.data(), bytes_read),
        boost::asio::transfer_exactly(bytes_read),
        ec
      );

      if (ec || bytes_written != bytes_read) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream send error: " << ec.message();
        return false;
      }

      BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Sent " << bytes_written << " bytes from stream";
    }

    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream send error: " << e.what();
    return false;
  }
}

// Convenience method to send string message
bool TCP_Peer::send_message(const std::string& message) {
  std::istringstream iss(message);
  return send_stream(iss);
}

// Clean up connection resources and reset state
void TCP_Peer::cleanup_connection() {
  processing_active_ = false;

  if (processing_thread_ && processing_thread_->joinable()) {
    processing_thread_->join();
    processing_thread_.reset();
  }

  if (socket_ && socket_->is_open()) {
    boost::system::error_code ec;

    // Shutdown both send and receive operations
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket shutdown error: " << ec.message();
    }

    socket_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket close error: " << ec.message();
    }
  }

  // Clear input buffer
  if (input_buffer_) {
    input_buffer_->consume(input_buffer_->size());
  }

  endpoint_.reset();
}

// Get peer identifier
uint8_t TCP_Peer::get_peer_id() const {
  return peer_id_;
}

// Get reference to underlying socket
boost::asio::ip::tcp::socket& TCP_Peer::get_socket() {
  return *socket_;
}

} // namespace network
} // namespace dfs