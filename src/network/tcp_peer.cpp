#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

// Constructor initializes a TCP peer with a unique identifier
// Sets up the networking components including socket and buffers
TCP_Peer::TCP_Peer(const std::string& peer_id)
  : peer_id_(peer_id),  // Initialize peer identifier
  socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),  // Create async I/O socket
  input_buffer_(std::make_unique<boost::asio::streambuf>()) {  // Initialize input buffer only
  initialize_streams();
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Constructing TCP_Peer";
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input stream initialized";
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] TCP_Peer instance created successfully";
}

// Destructor
TCP_Peer::~TCP_Peer() {
  if (socket_ && socket_->is_open()) {
  disconnect();
  }
  BOOST_LOG_TRIVIAL(debug) << "TCP_Peer destroyed: " << peer_id_;
}

// Initialize input stream for reading incoming data
void TCP_Peer::initialize_streams() {
  // Create input stream wrapper around the buffer for convenient reading
  input_stream_ = std::make_unique<std::istream>(input_buffer_.get());
}

  // Returns pointer to input stream for reading data
  std::istream* TCP_Peer::get_input_stream() {
    if (!socket_->is_open()) {
    return nullptr;
    }
    return input_stream_.get();
  }

  // Sets the callback function for processing incoming data
void TCP_Peer::set_stream_processor(StreamProcessor processor) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting stream processor";
  stream_processor_ = std::move(processor);
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processor configured";
}

// Begins asynchronous processing of incoming data
bool TCP_Peer::start_stream_processing() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Attempting to start stream processing";

  // Verify connection and processor are ready
  if (!socket_->is_open() || !stream_processor_) {
  BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot start processing - socket not connected or no processor set";
  return false;
  }

  if (processing_active_) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processing already active";
  return true;
  }

  // Start processing thread
  processing_active_ = true;
  processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing started successfully";
  return true;
}

// Stops the asynchronous data processing
void TCP_Peer::stop_stream_processing() {
  if (processing_active_) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stopping stream processing";
  processing_active_ = false;

  // Wait for processing thread to complete
  if (processing_thread_ && processing_thread_->joinable()) {
    processing_thread_->join();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing thread joined";
  }

  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
  }
}

// Establishes a TCP connection to the specified address and port
bool TCP_Peer::connect(const std::string& address, uint16_t port) {
  if (socket_->is_open()) {
  BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot connect - socket already connected";
  return false;
  }

  try {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Entering connect() method";
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Initiating connection to " << address << ":" << port;

  // Resolve DNS name to IP address
  boost::asio::ip::tcp::resolver resolver(io_context_);
  boost::system::error_code resolve_ec;
  auto endpoints = resolver.resolve(address, std::to_string(port), resolve_ec);

  if (resolve_ec) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] DNS resolution failed: " << resolve_ec.message() 
                << " (code: " << resolve_ec.value() << ")";
    return false;
  }

  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Address resolved successfully";
  endpoint_ = std::make_unique<boost::asio::ip::tcp::endpoint>(*endpoints.begin());

  // Attempt to establish connection
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Attempting socket connection";
  boost::system::error_code connect_ec;
  socket_->connect(*endpoint_, connect_ec);

  if (connect_ec) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection failed: " << connect_ec.message();
    cleanup_connection();
    return false;
  }

  // Start IO service if not running
  if (!io_context_.stopped()) {
    io_context_.reset();
  }

  BOOST_LOG_TRIVIAL(info) << "Successfully connected to " << address << ":" << port;
  return true;
  }
  catch (const std::exception& e) {
  BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection error: " << e.what();
  cleanup_connection();
  return false;
  }
}

// Gracefully terminates the TCP connection
bool TCP_Peer::disconnect() {
  if (!socket_->is_open()) {
  return false;
  }

  try {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Initiating disconnect sequence";

  // First stop any ongoing stream processing
  stop_stream_processing();

  // Then cleanup the connection
  cleanup_connection();

  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Successfully disconnected";
  return true;
  }
  catch (const std::exception& e) {
  BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Disconnect error: " << e.what();
  return false;
  }
}

bool TCP_Peer::send_stream(std::istream& input_stream, std::size_t buffer_size = 8192) {
  if (!socket_ || !socket_->is_open()) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot send stream - socket not connected";
    return false;
  }

  try {
    std::unique_lock<std::mutex> lock(io_mutex_);
    std::vector<char> buffer(buffer_size);
    boost::system::error_code ec;

    while (input_stream.good()) {
      // Read chunk from input stream
      input_stream.read(buffer.data(), buffer_size);
      std::size_t bytes_read = input_stream.gcount();

      if (bytes_read == 0) {
        break;
      }

      // Write chunk to socket
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

void TCP_Peer::process_stream() {
  // Create work object to keep io_context running and keep peer connection running
  boost::asio::io_context::work work(io_context_);
  std::thread io_thread([this]() { 
    try {
      io_context_.run();
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] IO context error: " << e.what();
    }
  });

  // Set up deadline timer for read operations
  boost::asio::steady_timer deadline(io_context_);
  const auto max_idle_time = std::chrono::seconds(5);

  while (processing_active_ && socket_->is_open()) {
    try {
      boost::system::error_code ec;
      deadline.expires_after(max_idle_time);

      // Handle incoming data with proper synchronization
      {
        std::unique_lock<std::mutex> lock(io_mutex_);
        // Check if data is available with timeout
        socket_->async_wait(boost::asio::ip::tcp::socket::wait_read,
          [this](const boost::system::error_code& ec) {
            if (ec) {
              BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket wait error: " << ec.message();
            }
          });

        if (socket_->available(ec) > 0 || !ec) {
          // Read until newline with timeout protection
          boost::asio::async_read_until(
            *socket_,
            *input_buffer_,
            '\n',
            [this](const boost::system::error_code& ec, std::size_t n) {
              if (!ec && n > 0) {
                std::string data;
                std::istream is(input_buffer_.get());
                std::getline(is, data);
                if (!data.empty()) {
                  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Received data: " << data;
                  // Process data through registered handler
                  if (stream_processor_) {
                    std::string framed_data = data + '\n';
                    std::istringstream iss(framed_data);
                    stream_processor_(iss);
                  }
                }
              } else if (ec != boost::asio::error::would_block && 
                        ec != boost::asio::error::try_again) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Read error: " << ec.message();
              }
            });
        }
      }

      // Wait for either data or timeout
      io_context_.run_one();
      if (deadline.expires_at() <= boost::asio::steady_timer::clock_type::now()) {
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processing cycle completed";
        break;
      }
    }
    catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
      break;
    }
  }

  // Cleanup io_context and processing thread
  io_context_.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
}

// Cleanup connection and associated resources
void TCP_Peer::cleanup_connection() {
  // First ensure processing is stopped
  processing_active_ = false;

  // Join processing thread if it exists
  if (processing_thread_ && processing_thread_->joinable()) {
    processing_thread_->join();
    processing_thread_.reset();
  }

  // Cleanup socket with proper shutdown sequence
  if (socket_ && socket_->is_open()) {
    boost::system::error_code ec;

    // Gracefully shutdown the socket
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket shutdown error: " << ec.message();
    }

    // Close the socket
    socket_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket close error: " << ec.message();
    }
  }

  // Clear any remaining data in input buffer
  if (input_buffer_) {
    input_buffer_->consume(input_buffer_->size());
  }

  endpoint_.reset();
}

// Check if the peer is currently connected
// Returns true if socket exists and is open
bool TCP_Peer::is_connected() const {
  return socket_ && socket_->is_open();
}

} // namespace network
} // namespace dfs