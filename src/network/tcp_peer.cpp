#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

TCP_Peer::TCP_Peer(const std::string& peer_id)
  : peer_id_(peer_id),
    connection_state_(),
    socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),
    input_buffer_(std::make_unique<boost::asio::streambuf>()),
    output_buffer_(std::make_unique<boost::asio::streambuf>()) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Constructing TCP_Peer";
  initialize_streams();
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input buffer size: " << input_buffer_->max_size();
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Output buffer size: " << output_buffer_->max_size();
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] TCP_Peer instance created successfully with IO context: " 
                         << &io_context_;
}

TCP_Peer::~TCP_Peer() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Entering destructor";
  if (is_connected()) {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Active connection detected in destructor, initiating disconnect";
    disconnect();
  }
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Destructor completed. Final state: " 
                          << ConnectionState::state_to_string(get_connection_state());
}

void TCP_Peer::initialize_streams() {
  BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Initializing IO streams";
  input_stream_ = std::make_unique<std::istream>(input_buffer_.get());
  output_stream_ = std::make_unique<std::ostream>(output_buffer_.get());
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] IO streams initialized successfully";
}

bool TCP_Peer::connect(const std::string& address, uint16_t port) {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Entering connect() method";
  
  if (!validate_connection_state(ConnectionState::State::INITIAL)) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection attempt failed - Invalid state: " << 
      ConnectionState::state_to_string(get_connection_state());
    return false;
  }

  try {
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Initiating connection to " << address << ":" << port;
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Connection parameters:"
                            << "\n  - Peer ID: " << peer_id_
                            << "\n  - Target address: " << address
                            << "\n  - Target port: " << port
                            << "\n  - Initial state: " << ConnectionState::state_to_string(get_connection_state());
    
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Transitioning state to CONNECTING";
    connection_state_.transition_to(ConnectionState::State::CONNECTING);
    
    BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Creating resolver with IO context: " << &io_context_;
    boost::asio::ip::tcp::resolver resolver(io_context_);
    boost::system::error_code resolve_ec;
    
    auto resolve_start = std::chrono::steady_clock::now();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Starting DNS resolution for " << address;
    auto endpoints = resolver.resolve(address, std::to_string(port), resolve_ec);
    auto resolve_end = std::chrono::steady_clock::now();
    auto resolve_duration = std::chrono::duration_cast<std::chrono::milliseconds>(resolve_end - resolve_start);
    
    if (resolve_ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] DNS resolution failed:"
                              << "\n  - Error: " << resolve_ec.message()
                              << "\n  - Error code: " << resolve_ec.value()
                              << "\n  - Time spent: " << resolve_duration.count() << "ms"
                              << "\n  - Target: " << address << ":" << port;
      connection_state_.transition_to(ConnectionState::State::ERROR);
      return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Address resolved successfully:"
                            << "\n  - Resolution time: " << resolve_duration.count() << "ms"
                            << "\n  - Number of endpoints: " << std::distance(endpoints.begin(), endpoints.end());
    
    endpoint_ = std::make_unique<boost::asio::ip::tcp::endpoint>(*endpoints.begin());
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Selected endpoint:"
                            << "\n  - Address: " << endpoint_->address().to_string()
                            << "\n  - Port: " << endpoint_->port()
                            << "\n  - Protocol: " << endpoint_->protocol().name();
    
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Attempting socket connection";
    auto connect_start = std::chrono::steady_clock::now();
    boost::system::error_code connect_ec;
    socket_->connect(*endpoint_, connect_ec);
    auto connect_end = std::chrono::steady_clock::now();
    auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(connect_end - connect_start);
    
    if (connect_ec) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection failed:"
                              << "\n  - Error: " << connect_ec.message()
                              << "\n  - Error code: " << connect_ec.value()
                              << "\n  - Time spent: " << connect_duration.count() << "ms"
                              << "\n  - Target endpoint: " << endpoint_->address().to_string() << ":" << endpoint_->port();
      connection_state_.transition_to(ConnectionState::State::ERROR);
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
    
    connection_state_.transition_to(ConnectionState::State::CONNECTED);
    BOOST_LOG_TRIVIAL(info) << "Successfully connected to " << address << ":" << port;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Connection error: " << e.what();
    connection_state_.transition_to(ConnectionState::State::ERROR);
    cleanup_connection();
    return false;
  }
}

bool TCP_Peer::disconnect() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Initiating disconnect sequence";
  
  if (!validate_connection_state(ConnectionState::State::CONNECTED)) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot disconnect - Invalid state: " 
                            << ConnectionState::state_to_string(get_connection_state());
    return false;
  }

  try {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Transitioning to DISCONNECTING state";
    connection_state_.transition_to(ConnectionState::State::DISCONNECTING);
    
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stopping stream processing";
    stop_stream_processing();
    
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Initiating connection cleanup";
    cleanup_connection();

    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Transitioning to DISCONNECTED state";
    connection_state_.transition_to(ConnectionState::State::DISCONNECTED);
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Peer successfully disconnected";
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Disconnect error at " << __FILE__ << ":" << __LINE__ 
                            << " - " << e.what();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Transitioning to ERROR state due to disconnect failure";
    connection_state_.transition_to(ConnectionState::State::ERROR);
    return false;
  }
}

std::ostream* TCP_Peer::get_output_stream() {
  if (!validate_connection_state(ConnectionState::State::CONNECTED)) {
    return nullptr;
  }
  return output_stream_.get();
}

std::istream* TCP_Peer::get_input_stream() {
  if (!validate_connection_state(ConnectionState::State::CONNECTED)) {
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
  
  if (!validate_connection_state(ConnectionState::State::CONNECTED)) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot start processing - Invalid connection state: "
                            << ConnectionState::state_to_string(get_connection_state());
    return false;
  }
  
  if (!stream_processor_) {
    BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot start processing - No stream processor configured";
    return false;
  }

  if (processing_active_) {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stream processing already active";
    return true;
  }

  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Initializing stream processing thread";
  processing_active_ = true;
  processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
  BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing started successfully";
  return true;
}

void TCP_Peer::stop_stream_processing() {
  if (processing_active_) {
    processing_active_ = false;
    if (processing_thread_ && processing_thread_->joinable()) {
      processing_thread_->join();
    }
    BOOST_LOG_TRIVIAL(info) << "Stopped stream processing for peer: " << peer_id_;
  }
}

ConnectionState::State TCP_Peer::get_connection_state() const {
  return connection_state_.get_state();
}

void TCP_Peer::process_stream() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Starting stream processing thread";
  boost::asio::io_context::work work(io_context_);
  
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Launching IO context thread";
  std::thread io_thread([this]() { 
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] IO context thread started";
    io_context_.run(); 
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] IO context thread completed";
  });

  while (processing_active_ && socket_->is_open()) {
    try {
      boost::system::error_code ec;
      BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Stream processing iteration started";
      
      // Handle outgoing data with proper synchronization and framing
      if (output_buffer_->size() > 0) {
        std::unique_lock<std::mutex> lock(io_mutex_);
        
        try {
          // Ensure message ends with newline for proper framing
          std::string data(
            boost::asio::buffers_begin(output_buffer_->data()),
            boost::asio::buffers_end(output_buffer_->data())
          );
          if (data.back() != '\n') {
            data += '\n';
          }
          
          // Trace before write operation
          BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Attempting to write " << data.length() 
                                  << " bytes to socket";
          
          auto start_time = std::chrono::steady_clock::now();
          std::size_t bytes_written = boost::asio::write(
            *socket_,
            boost::asio::buffer(data),
            boost::asio::transfer_exactly(data.length()),
            ec
          );
          auto end_time = std::chrono::steady_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
          
          if (!ec && bytes_written == data.length()) {
            output_buffer_->consume(output_buffer_->size());
            BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Write operation completed:"
                                   << "\n  - Bytes written: " << bytes_written
                                   << "\n  - Duration: " << duration.count() << "µs"
                                   << "\n  - Data: " << data;
          } else {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Write operation failed:"
                                   << "\n  - Error: " << ec.message()
                                   << "\n  - Error code: " << ec.value()
                                   << "\n  - Bytes attempted: " << data.length()
                                   << "\n  - Bytes written: " << bytes_written;
            break;
          }
        } catch (const std::exception& e) {
          BOOST_LOG_TRIVIAL(error) << "Write processing error: " << e.what();
          break;
        }
      }
      
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

              BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Starting read operation with timeout";
              auto read_start_time = std::chrono::steady_clock::now();
              
              // Read until newline
              std::size_t n = boost::asio::read_until(
                *socket_,
                *input_buffer_,
                '\n',
                ec
              );
              
              auto read_end_time = std::chrono::steady_clock::now();
              auto read_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                read_end_time - read_start_time);

              if (!ec && n > 0) {
                BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Read operation completed:"
                                       << "\n  - Bytes read: " << n
                                       << "\n  - Duration: " << read_duration.count() << "µs"
                                       << "\n  - Buffer size after read: " << input_buffer_->size();
                
                // Process complete message
                std::string data;
                std::istream is(input_buffer_.get());
                std::getline(is, data);

                if (!data.empty()) {
                  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Message processing:"
                                         << "\n  - Message length: " << data.length()
                                         << "\n  - Content: " << data
                                         << "\n  - Remaining buffer: " << input_buffer_->size();
                  
                  // Handle the received data
                  if (stream_processor_) {
                    // Pass to stream processor first
                    std::string framed_data = data + '\n';
                    std::istringstream iss(framed_data);
                    stream_processor_(iss);
                  }

                  // Write to input buffer for normal reads, ensuring no duplicates
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
                BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                break;
              }
            } catch (const std::exception& e) {
              BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
              break;
            }
          }
        } catch (const std::exception& e) {
          BOOST_LOG_TRIVIAL(error) << "Read processing error: " << e.what();
          break;
        }
      }
      
      // Small delay to prevent busy-waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error in main loop: " 
                              << e.what() << " at " << __FILE__ << ":" << __LINE__;
      break;
    }
  }
  
  // Cleanup
  io_context_.stop();
  if (io_thread.joinable()) {
    io_thread.join();
  }
}

bool TCP_Peer::validate_connection_state(ConnectionState::State required_state) const {
  auto current_state = get_connection_state();
  BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] State validation:"
                          << "\n  - Current state: " << ConnectionState::state_to_string(current_state)
                          << "\n  - Required state: " << ConnectionState::state_to_string(required_state)
                          << "\n  - Location: " << __FILE__ << ":" << __LINE__;
  
  if (current_state != required_state) {
    BOOST_LOG_TRIVIAL(warning) << "[" << peer_id_ << "] Invalid state transition detected:"
      << "\n  - From: " << ConnectionState::state_to_string(current_state)
      << "\n  - To: " << ConnectionState::state_to_string(required_state)
      << "\n  - Location: " << __FILE__ << ":" << __LINE__
      << "\n  - Error type: Connection lifecycle sequencing error"
      << "\n  - Socket state: " << (socket_ && socket_->is_open() ? "open" : "closed")
      << "\n  - Processing active: " << processing_active_
      << "\n  - Time: " << std::chrono::system_clock::now().time_since_epoch().count();
    return false;
  }
  
  BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] State validation successful:"
    << "\n  - Verified state: " << ConnectionState::state_to_string(current_state)
    << "\n  - Processing active: " << processing_active_
    << "\n  - Input buffer size: " << input_buffer_->size()
    << "\n  - Output buffer size: " << output_buffer_->size();
  return true;
}

void TCP_Peer::cleanup_connection() {
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Starting connection cleanup";
  
  // First, ensure processing is stopped
  BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Stopping stream processing, current state: " 
                          << (processing_active_ ? "active" : "inactive");
  processing_active_ = false;
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Set processing_active_ to false";
  
  if (processing_thread_ && processing_thread_->joinable()) {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Joining processing thread"
                            << "\n  - Thread ID: " << processing_thread_->get_id()
                            << "\n  - Processing state: " << (processing_active_ ? "active" : "inactive");
    processing_thread_->join();
    processing_thread_.reset();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing thread joined and reset";
  } else {
    BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] No processing thread to cleanup";
  }
  
  // Then cleanup socket
  if (socket_) {
    boost::system::error_code ec;
    BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Socket cleanup - Initial state:"
                            << "\n  - Is open: " << (socket_->is_open() ? "yes" : "no")
                            << "\n  - Has error: " << (socket_->error() ? "yes" : "no")
                            << "\n  - Available bytes: " << socket_->available(ec);
    
    if (socket_->is_open()) {
      BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Socket is open, initiating shutdown sequence";
      
      // Shutdown the socket first
      BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Attempting socket shutdown";
      socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket shutdown error:"
                                << "\n  - Error message: " << ec.message()
                                << "\n  - Error code: " << ec.value()
                                << "\n  - Category: " << ec.category().name();
      } else {
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Socket shutdown successful";
      }
      
      // Then close it
      BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Attempting socket close";
      socket_->close(ec);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket close error:"
                                << "\n  - Error message: " << ec.message()
                                << "\n  - Error code: " << ec.value()
                                << "\n  - Category: " << ec.category().name();
      } else {
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Socket closed successfully";
      }
    } else {
      BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Socket was already closed";
    }
  } else {
    BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] No socket to cleanup";
  }
  
  // Clear any remaining data in buffers
  BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Starting buffer cleanup";
  if (input_buffer_) {
    std::size_t input_size = input_buffer_->size();
    std::size_t max_size = input_buffer_->max_size();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input buffer status before cleanup:"
                            << "\n  - Current size: " << input_size << " bytes"
                            << "\n  - Maximum size: " << max_size << " bytes"
                            << "\n  - Utilization: " << (input_size * 100.0 / max_size) << "%";
    input_buffer_->consume(input_size);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Cleared input buffer (" << input_size << " bytes)";
  }
  if (output_buffer_) {
    std::size_t output_size = output_buffer_->size();
    std::size_t max_size = output_buffer_->max_size();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Output buffer status before cleanup:"
                            << "\n  - Current size: " << output_size << " bytes"
                            << "\n  - Maximum size: " << max_size << " bytes"
                            << "\n  - Utilization: " << (output_size * 100.0 / max_size) << "%";
    output_buffer_->consume(output_size);
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Cleared output buffer (" << output_size << " bytes)";
  }
  
  endpoint_.reset();
  BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Connection cleanup completed"
                          << "\n  - Final socket state: " << (socket_ && socket_->is_open() ? "open" : "closed")
                          << "\n  - Processing thread state: " << (processing_thread_ ? "exists" : "null")
                          << "\n  - Processing active flag: " << processing_active_;
}

} // namespace network
} // namespace dfs