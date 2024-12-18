#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

// Constructor: Initializes a TCP peer with a unique identifier, creates socket, input buffer, and sets up logging
TCP_Peer::TCP_Peer(const std::string& peer_id)
  : peer_id_(peer_id),  // Initialize peer identifier
    socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),  // Create async I/O socket
    input_buffer_(std::make_unique<boost::asio::streambuf>()) {  // Initialize input buffer only
  initialize_streams();
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Constructing TCP_Peer";
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input stream initialized";
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] TCP_Peer instance created successfully";
}

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

// Establish connection to remote peer with specified address and port, configures socket options for optimal performance
bool TCP_Peer::connect(const std::string& address, uint16_t port) {
  if (socket_->is_open()) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot connect - socket already connected";
    return false;
  }

  try {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Entering connect() method";
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Initiating connection to " << address << ":" << port;

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

    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Attempting socket connection";
    boost::system::error_code connect_ec;
    socket_->connect(*endpoint_, connect_ec);

    if (connect_ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection failed: " << connect_ec.message();
      cleanup_connection();
      return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Configuring socket options";
    
    // Configure socket options for optimal performance
    boost::system::error_code opt_ec;
    socket_->set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] TCP_NODELAY set: " << (opt_ec ? "failed" : "success");

    socket_->set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] SO_KEEPALIVE set: " << (opt_ec ? "failed" : "success");

    socket_->set_option(boost::asio::socket_base::send_buffer_size(8192), opt_ec);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] SO_SNDBUF set to 8192: " << (opt_ec ? "failed" : "success");

    socket_->set_option(boost::asio::socket_base::receive_buffer_size(8192), opt_ec);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] SO_RCVBUF set to 8192: " << (opt_ec ? "failed" : "success");

    socket_->set_option(boost::asio::socket_base::reuse_address(true), opt_ec);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] SO_REUSEADDR set: " << (opt_ec ? "failed" : "success");

    socket_->set_option(boost::asio::socket_base::linger(true, 0), opt_ec);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] SO_LINGER set: " << (opt_ec ? "failed" : "success");

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


std::istream* TCP_Peer::get_input_stream() {
  if (!socket_->is_open()) {
    return nullptr;
  }
  return input_stream_.get();
}

void TCP_Peer::set_stream_processor(StreamProcessor processor) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting stream processor";
  stream_processor_ = std::move(processor);
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processor configured";
}

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

void TCP_Peer::stop_stream_processing() {
  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stopping stream processing";
    processing_active_ = false;
    
    if (processing_thread_ && processing_thread_->joinable()) {
      processing_thread_->join();
      BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing thread joined";
    }
    
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
  }
}

// Direct message sending method for synchronous transmission of data
// Returns true if message was sent successfully, false otherwise
bool TCP_Peer::send_message(const std::string& message) {
    if (!socket_ || !socket_->is_open()) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot send message - socket not connected";
        return false;
    }

    try {
        std::unique_lock<std::mutex> lock(io_mutex_);
        
        // Ensure message ends with newline for proper framing
        std::string framed_message = message;
        if (framed_message.empty() || framed_message.back() != '\n') {
            framed_message += '\n';
        }

        // Write complete message with synchronous operation
        boost::system::error_code ec;
        std::size_t bytes_written = boost::asio::write(
            *socket_,
            boost::asio::buffer(framed_message),
            boost::asio::transfer_exactly(framed_message.length()),
            ec
        );

        if (!ec && bytes_written == framed_message.length()) {
            BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Sent " << bytes_written << " bytes: " << message;
            return true;
        } else {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Send error: " << ec.message();
            return false;
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Send error: " << e.what();
        return false;
    }
}

// Main processing loop for handling incoming data
void TCP_Peer::process_stream() {
    // Create work object to keep io_context running
    boost::asio::io_context::work work(io_context_);
    std::thread io_thread([this]() { io_context_.run(); });

    while (processing_active_ && socket_->is_open()) {
        try {
            boost::system::error_code ec;

            // Handle incoming data with proper synchronization and framing
      {
        std::unique_lock<std::mutex> lock(io_mutex_);

        try {
          // Check if data is available
          if (socket_->available(ec) > 0 || !ec) {
            try {
              // Read complete message frame with timeout
              boost::asio::steady_timer timer(io_context_);
              timer.expires_from_now(std::chrono::milliseconds(100));

              // Read until newline
              std::size_t n = boost::asio::read_until(
                *socket_,
                *input_buffer_,
                '\n',
                ec
              );

              if (!ec && n > 0) {
                // Process complete message
                std::string data;
                std::istream is(input_buffer_.get());
                std::getline(is, data);

                if (!data.empty()) {
                  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Received data: " << data;

                  // Handle the received data
                  if (stream_processor_) {
                    // Pass to stream processor first
                    std::string framed_data = data + '\n';
                    std::istringstream iss(framed_data);
                    stream_processor_(iss);
                  }

                  // Write to input buffer for normal reads
                  {
                    std::lock_guard<std::mutex> lock(io_mutex_);
                    std::string framed_data = data + '\n';
                    std::size_t write_size = framed_data.size();
                    auto bufs = input_buffer_->prepare(write_size);
                    std::copy(framed_data.begin(), framed_data.end(), 
                        boost::asio::buffers_begin(bufs));
                    input_buffer_->commit(write_size);
                  }
                }
              } else if (ec && 
                    ec != boost::asio::error::would_block && 
                    ec != boost::asio::error::try_again &&
                    ec != boost::asio::error::eof) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Read error: " << ec.message();
                break;
              }
            } catch (const std::exception& e) {
              BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
              break;
            }
          }
        } catch (const std::exception& e) {
          BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Read processing error: " << e.what();
          break;
        }
      }

      // Small delay to prevent busy-waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
      break;
    }
  }

  // Cleanup
  io_context_.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }
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