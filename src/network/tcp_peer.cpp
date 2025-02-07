#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

//==============================================
// CONSTRUCTOR AND DESTRUCTOR
//==============================================
  
TCP_Peer::TCP_Peer(uint8_t peer_id, Channel& channel, const std::vector<uint8_t>& key)
  : peer_id_(peer_id),  
  socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),  
  input_buffer_(std::make_unique<boost::asio::streambuf>()),
  codec_(std::make_unique<Codec>(key, channel)) {  
  initialize_streams();
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Constructing TCP_Peer";
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Input stream initialized";
  BOOST_LOG_TRIVIAL(info) << "TCP peer: TCP_Peer instance created successfully";
}

// Cleanup connection and resources on destruction
TCP_Peer::~TCP_Peer() {
  cleanup_connection();
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: TCP_Peer destroyed: " << static_cast<int>(peer_id_);
}

//==============================================
// STREAM CONTROL OPERATIONS
//==============================================

void TCP_Peer::initialize_streams() {
  input_stream_ = std::make_unique<std::istream>(input_buffer_.get());
}

void TCP_Peer::set_stream_processor(StreamProcessor processor) {
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Setting stream processor";
  stream_processor_ = std::move(processor);
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Stream processor configured";
}

bool TCP_Peer::start_stream_processing() {
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Attempting to start stream processing";

  if (!socket_->is_open() || !stream_processor_) {
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Cannot start processing - socket not connected or no processor set";
    return false;
  }

  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Stream processing already active";
    return true;
  }

  processing_active_ = true;
  processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
  BOOST_LOG_TRIVIAL(info) << "TCP peer: Stream processing started successfully";
  return true;
}

// Gracefully stop stream processing and cleanup resources
void TCP_Peer::stop_stream_processing() {
  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Stopping stream processing";

    processing_active_ = false;

    // Cancel any pending asynchronous operations
    if (socket_ && socket_->is_open()) {
      boost::system::error_code ec;
      socket_->cancel(ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "TCP peer: Error canceling socket operations: " << ec.message();
      }
    }

    io_context_.stop();

    // Wait for processing thread to complete
    if (processing_thread_ && processing_thread_->joinable()) {
      processing_thread_->join();
      processing_thread_.reset();
      BOOST_LOG_TRIVIAL(debug) << "TCP peer: Processing thread joined";
    }

    io_context_.restart();

    BOOST_LOG_TRIVIAL(info) << "TCP peer: Stream processing stopped";
  }
}

//==============================================
// INCOMING DATA STREAM PROCESSING
//==============================================

void TCP_Peer::process_stream() {
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Setting up stream processing";

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
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Stream processing error: " << e.what();
  }

  BOOST_LOG_TRIVIAL(info) << "TCP peer: Stream processing stopped";
}

void TCP_Peer::async_read_next() {
  if (!processing_active_ || !socket_->is_open()) {
    return;
  }

  BOOST_LOG_TRIVIAL(trace) << "TCP peer: Setting up next async read";

  // First read the size asynchronously
  boost::asio::async_read(
    *socket_,
    boost::asio::buffer(&expected_size_, sizeof(expected_size_)),
    std::bind(&TCP_Peer::handle_read_size, this,
              std::placeholders::_1,
              std::placeholders::_2));
}

void TCP_Peer::handle_read_size(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
  if (!ec) {
    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Expecting " << expected_size_ << " bytes of data";

    // Now read the actual data asynchronously
    boost::asio::async_read(
      *socket_,
      *input_buffer_,
      boost::asio::transfer_exactly(expected_size_),
      std::bind(&TCP_Peer::handle_read_data, this,
                std::placeholders::_1,
                std::placeholders::_2));
  } 
  else if (ec != boost::asio::error::operation_aborted) {
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Size read error: " << ec.message();
    if (processing_active_ && socket_->is_open()) {
      async_read_next();
    }
  }
}

void TCP_Peer::handle_read_data(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Read callback triggered";

  if (!ec && bytes_transferred == expected_size_) {
    process_received_data();

    // Continue reading if still active
    if (processing_active_ && socket_->is_open()) {
      async_read_next();
    }
  } 
  else if (ec != boost::asio::error::operation_aborted) {
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Read error: " << ec.message();
    if (processing_active_ && socket_->is_open()) {
      async_read_next();
    }
  }
}

void TCP_Peer::process_received_data() {
  std::string data;
  std::istream is(input_buffer_.get());
  data.resize(expected_size_);
  is.read(&data[0], expected_size_);

  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Read from buffer - got " << data.length() << " bytes of data";

  if (data.empty()) {
    return;
  }

  BOOST_LOG_TRIVIAL(debug) << "TCP peer: Receiving data";

  if (stream_processor_) {
    std::istringstream iss(data);
    try {
      boost::asio::ip::tcp::endpoint remote_endpoint = socket_->remote_endpoint();
      std::string source_id = remote_endpoint.address().to_string() + ":" + 
                   std::to_string(remote_endpoint.port());
      BOOST_LOG_TRIVIAL(debug) << "TCP peer: Processing data from " << source_id;
      stream_processor_(iss);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "TCP peer: Stream processor error: " << e.what();
    }
  } else {
    input_buffer_->sputn(data.c_str(), data.length());
    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Data forwarded to input stream";
  }
}

//==============================================
// OUTGOING DATA STREAM PROCESSING
//==============================================

bool TCP_Peer::send_message(const std::string& message, std::size_t total_size) {
  std::istringstream iss(message);
  return send_stream(iss, total_size);
}

bool TCP_Peer::send_size(std::size_t total_size) {
  try {
    // Write total_size as raw bytes to socket
    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Starting to send total size";
    boost::asio::write(*socket_, boost::asio::buffer(&total_size, sizeof(total_size)));
    BOOST_LOG_TRIVIAL(info) << "TCP peer: Sent total size: " << total_size;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Failed to send total size: " << e.what();
    return false;
  }
}

bool TCP_Peer::send_stream(std::istream& input_stream, std::size_t total_size, std::size_t buffer_size) {
  if (!socket_ || !socket_->is_open()) {
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Cannot send stream - socket not connected";
    return false;
  }

  try {
    std::unique_lock<std::mutex> lock(io_mutex_);
    std::vector<char> buffer(buffer_size);
    boost::system::error_code ec;
    std::size_t total_bytes_sent = 0;

    // First send the total size
    if (!send_size(total_size)) {
      BOOST_LOG_TRIVIAL(error) << "TCP peer: Failed to send total size";
      return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Peer " << static_cast<int>(peer_id_) 
                            << " starting to send " << total_size << " bytes";

    // Read and send data in chunks until we've sent exactly total_size bytes
    while (input_stream.good() && total_bytes_sent < total_size) {
      // Calculate how many bytes to read in this chunk
      std::size_t bytes_remaining = total_size - total_bytes_sent;
      std::size_t chunk_size = std::min(buffer_size, bytes_remaining);

      input_stream.read(buffer.data(), chunk_size);
      std::size_t bytes_read = input_stream.gcount();

      BOOST_LOG_TRIVIAL(debug) << "TCP peer: Peer " << static_cast<int>(peer_id_) 
                              << " read " << bytes_read << " bytes from stream";

      if (bytes_read > 0) {
        // Write exact number of bytes read
        std::size_t bytes_written = boost::asio::write(
          *socket_,
          boost::asio::buffer(buffer.data(), bytes_read),
          boost::asio::transfer_exactly(bytes_read),
          ec
        );

        if (ec || bytes_written != bytes_read) {
          BOOST_LOG_TRIVIAL(error) << "TCP peer: Stream send error: " << ec.message();
          return false;
        }

        total_bytes_sent += bytes_written;
        BOOST_LOG_TRIVIAL(debug) << "TCP peer: Sent " << bytes_written 
                                << " bytes, total sent: " << total_bytes_sent 
                                << " / " << total_size;
      }
    }

    if (total_bytes_sent != total_size) {
      BOOST_LOG_TRIVIAL(error) << "TCP peer: Failed to send expected amount of data. Sent " 
                              << total_bytes_sent << " of " << total_size << " bytes";
      return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "TCP peer: Successfully sent " << total_bytes_sent << " bytes";
    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "TCP peer: Stream send error: " << e.what();
    return false;
  }
}

//==============================================
// TEARDOWN
//==============================================

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
      BOOST_LOG_TRIVIAL(error) << "TCP peer: Socket shutdown error: " << ec.message();
    }

    socket_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "TCP peer: Socket close error: " << ec.message();
    }
  }

  // Clear input buffer
  if (input_buffer_) {
    input_buffer_->consume(input_buffer_->size());
  }

  endpoint_.reset();
}

//==============================================
// GETTERS AND SETTERS
//==============================================


std::istream* TCP_Peer::get_input_stream() {
  if (!socket_->is_open()) {
    return nullptr;
  }
  return input_stream_.get();
}
  
uint8_t TCP_Peer::get_peer_id() const {
  return peer_id_;
}

boost::asio::ip::tcp::socket& TCP_Peer::get_socket() {
  return *socket_;
}

} // namespace network
} // namespace dfs