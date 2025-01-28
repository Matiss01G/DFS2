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
  codec_(std::make_unique<Codec>(key, channel)) {  
  initialize_streams();
  BOOST_LOG_TRIVIAL(debug) << "Constructing TCP_Peer";
  BOOST_LOG_TRIVIAL(debug) << "Input stream initialized";
  BOOST_LOG_TRIVIAL(info) << "TCP_Peer instance created successfully";
}

// Cleanup connection and resources on destruction
TCP_Peer::~TCP_Peer() {
  cleanup_connection();
  BOOST_LOG_TRIVIAL(debug) << "TCP_Peer destroyed: " << static_cast<int>(peer_id_);
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
  BOOST_LOG_TRIVIAL(debug) << "Setting stream processor";
  stream_processor_ = std::move(processor);
  BOOST_LOG_TRIVIAL(debug) << "Stream processor configured";
}

// Start asynchronous stream processing if socket is connected and processor is set
bool TCP_Peer::start_stream_processing() {
  BOOST_LOG_TRIVIAL(debug) << "Attempting to start stream processing";

  if (!socket_->is_open() || !stream_processor_) {
    BOOST_LOG_TRIVIAL(error) << "Cannot start processing - socket not connected or no processor set";
    return false;
  }

  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "Stream processing already active";
    return true;
  }

  processing_active_ = true;
  processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
  BOOST_LOG_TRIVIAL(info) << "Stream processing started successfully";
  return true;
}

// Gracefully stop stream processing and cleanup resources
void TCP_Peer::stop_stream_processing() {
  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "Stopping stream processing";

    processing_active_ = false;

    // Cancel any pending asynchronous operations
    if (socket_ && socket_->is_open()) {
      boost::system::error_code ec;
      socket_->cancel(ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Error canceling socket operations: " << ec.message();
      }
    }

    io_context_.stop();

    // Wait for processing thread to complete
    if (processing_thread_ && processing_thread_->joinable()) {
      processing_thread_->join();
      processing_thread_.reset();
      BOOST_LOG_TRIVIAL(debug) << "Processing thread joined";
    }

    io_context_.restart();

    BOOST_LOG_TRIVIAL(info) << "Stream processing stopped";
  }
}

// Main stream processing loop that handles incoming data
void TCP_Peer::process_stream() {
  BOOST_LOG_TRIVIAL(debug) << "Setting up stream processing";

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
    BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
  }

  BOOST_LOG_TRIVIAL(info) << "Stream processing stopped";
}

// Setup next asynchronous read operation with delimiter-based framing
void TCP_Peer::async_read_next() {
  if (!processing_active_ || !socket_->is_open()) {
    return;
  }

  BOOST_LOG_TRIVIAL(trace) << "Setting up next async read";

  // Use fixed size buffer for reading chunks
  static const size_t chunk_size = 1024;
  auto chunk_buffer = std::make_shared<std::vector<char>>(chunk_size);

  // Asynchronously read a chunk of data
  socket_->async_read_some(
    boost::asio::buffer(*chunk_buffer),
    [this, chunk_buffer](const boost::system::error_code& ec, std::size_t bytes_transferred) {
      BOOST_LOG_TRIVIAL(debug) << "Read callback triggered";

      if (!ec && bytes_transferred > 0) {
        // Add received data to input buffer
        input_buffer_->sputn(chunk_buffer->data(), bytes_transferred);

        // Process complete messages in the buffer
        process_buffered_messages();

        // Continue reading if still active
        if (processing_active_ && socket_->is_open()) {
          async_read_next();
        }
      } else if (ec != boost::asio::error::operation_aborted) {
        BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
        if (processing_active_ && socket_->is_open()) {
          async_read_next();
        }
      }
    });
}

void TCP_Peer::process_buffered_messages() {
  std::string data;
  std::istream is(input_buffer_.get());

  while (std::getline(is, data)) {
    if (!data.empty()) {
      BOOST_LOG_TRIVIAL(debug) << "Processing complete message of length: " << data.length();

      // Process data using stream processor if available
      if (stream_processor_) {
        std::string framed_data = data + '\n';  // Add back the newline that was removed by getline
        std::istringstream iss(framed_data);
        try {
          boost::asio::ip::tcp::endpoint remote_endpoint = socket_->remote_endpoint();
          std::string source_id = remote_endpoint.address().to_string() + ":" + 
                       std::to_string(remote_endpoint.port());
          BOOST_LOG_TRIVIAL(debug) << "Processing message from " << source_id;
          stream_processor_(iss);
        } catch (const std::exception& e) {
          BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
        }
      }
    }
  }
}

// Send data stream to peer with buffered writing
bool TCP_Peer::send_stream(std::istream& input_stream, std::size_t buffer_size) {
  if (!socket_ || !socket_->is_open()) {
    BOOST_LOG_TRIVIAL(error) << "Cannot send stream - socket not connected";
    return false;
  }

  try {
    std::unique_lock<std::mutex> lock(io_mutex_);
    std::vector<char> buffer(buffer_size);
    boost::system::error_code ec;
    bool data_sent = false;

    BOOST_LOG_TRIVIAL(debug) << "Peer " << static_cast<int>(peer_id_) << " starting to read and send data chunks";

    // Read and send data in chunks
    while (input_stream.good()) {
      input_stream.read(buffer.data(), buffer_size);
      std::size_t bytes_read = input_stream.gcount();

      BOOST_LOG_TRIVIAL(debug) << "Peer " << static_cast<int>(peer_id_) << " read " << bytes_read << " bytes from stream";

      if (bytes_read > 0) {
        // Write exact number of bytes read
        std::size_t bytes_written = boost::asio::write(
          *socket_,
          boost::asio::buffer(buffer.data(), bytes_read),
          boost::asio::transfer_exactly(bytes_read),
          ec
        );

        if (ec || bytes_written != bytes_read) {
          BOOST_LOG_TRIVIAL(error) << "Stream send error: " << ec.message();
          return false;
        }

        data_sent = true;
        BOOST_LOG_TRIVIAL(debug) << "Sent " << bytes_written << " bytes from stream";
      }
    }

    // Always append newline character for message framing
    char newline = '\n';
    std::size_t newline_written = boost::asio::write(
      *socket_,
      boost::asio::buffer(&newline, 1),
      boost::asio::transfer_exactly(1),
      ec
    );

    if (ec || newline_written != 1) {
      BOOST_LOG_TRIVIAL(error) << "Failed to send newline delimiter: " << ec.message();
      return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "Sent message delimiter";
    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Stream send error: " << e.what();
    return false;
  }
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
      BOOST_LOG_TRIVIAL(error) << "Socket shutdown error: " << ec.message();
    }

    socket_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "Socket close error: " << ec.message();
    }
  }

  // Clear input buffer
  if (input_buffer_) {
    input_buffer_->consume(input_buffer_->size());
  }

  endpoint_.reset();
}

// Convenience method to send string message
bool TCP_Peer::send_message(const std::string& message) {
  std::istringstream iss(message);
  return send_stream(iss);
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