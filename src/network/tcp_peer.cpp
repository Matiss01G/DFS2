#include "network/tcp_peer.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <random>

namespace dfs {
namespace network {

TcpPeer::TcpPeer()
    : socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_))
    , input_buffer_(std::make_unique<boost::asio::streambuf>())
    , output_buffer_(std::make_unique<boost::asio::streambuf>())
    , input_stream_(std::make_unique<std::istream>(input_buffer_.get()))
    , output_stream_(std::make_unique<std::ostream>(output_buffer_.get()))
    , processing_(false)
    , state_(ConnectionState::State::INITIAL)
    , peer_port_(0) {
    BOOST_LOG_TRIVIAL(info) << "TcpPeer created";
}

TcpPeer::~TcpPeer() {
    try {
        if (get_connection_state() == ConnectionState::State::CONNECTED) {
            disconnect();
        }
        
        // Ensure processing is stopped
        stop_stream_processing();
        
        // Clean up socket and io_context
        close_socket();
        io_context_.stop();
        
        // Clear buffers
        if (input_buffer_) {
            input_buffer_->consume(input_buffer_->size());
        }
        if (output_buffer_) {
            output_buffer_->consume(output_buffer_->size());
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error in TcpPeer destructor: " << e.what();
    }
}

bool TcpPeer::connect(const std::string& address, uint16_t port) {
    auto current_state = get_connection_state();
    if (!validate_state(ConnectionState::State::INITIAL) && 
        !validate_state(ConnectionState::State::DISCONNECTED)) {
        BOOST_LOG_TRIVIAL(warning) << "Invalid state for connection: " 
                      << ConnectionState::state_to_string(current_state)
                      << ". Must be INITIAL or DISCONNECTED.";
        return false;
    }
    
    // Reset io_context and create new socket if needed
    io_context_.restart();
    if (!socket_ || !socket_->is_open()) {
        socket_ = std::make_unique<boost::asio::ip::tcp::socket>(io_context_);
    }

    // Ensure clean state transition
    {
        auto expected_state = ConnectionState::State::INITIAL;
        if (!state_.compare_exchange_strong(expected_state, 
                                          ConnectionState::State::CONNECTING)) {
            expected_state = ConnectionState::State::DISCONNECTED;
            if (!state_.compare_exchange_strong(expected_state, 
                                              ConnectionState::State::CONNECTING)) {
                BOOST_LOG_TRIVIAL(error) << "Invalid state transition from " 
                    << ConnectionState::state_to_string(expected_state);
                return false;
            }
        }
    }
    
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(address, std::to_string(port));
        
        // Set up async connect with timeout
        boost::system::error_code ec;
        boost::asio::async_connect(*socket_, endpoints,
            [&ec](const boost::system::error_code& result_ec,
                  const boost::asio::ip::tcp::endpoint&) {
                ec = result_ec;
            });

        // Run with timeout
        // Run io_context with timeout and properly handle async operations
        std::chrono::steady_clock::time_point timeout_time = 
            std::chrono::steady_clock::now() + CONNECTION_TIMEOUT;
            
        while (io_context_.run_one_for(std::chrono::milliseconds(100))) {
            if (std::chrono::steady_clock::now() > timeout_time) {
                BOOST_LOG_TRIVIAL(error) << "Connection timeout";
                handle_error(boost::system::error_code(boost::asio::error::timed_out));
                return false;
            }
            
            if (ec) {
                break;  // Connection completed with error
            }
            
            if (socket_->is_open()) {
                break;  // Connection successful
            }
        }

        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Connection error: " << ec.message();
            handle_error(ec);
            return false;
        }

        if (!socket_->is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Socket closed unexpectedly during connection";
            handle_error(boost::system::error_code(boost::asio::error::connection_aborted));
            return false;
        }

        // Configure socket options
        boost::system::error_code opt_ec;
        
        // Set TCP_NODELAY for better latency
        socket_->set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set TCP_NODELAY: " << opt_ec.message();
        }

        // Enable keep-alive
        socket_->set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set SO_KEEPALIVE: " << opt_ec.message();
        }

        // Set linger option
        socket_->set_option(boost::asio::socket_base::linger(true, 0), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set SO_LINGER: " << opt_ec.message();
        }

        // Set buffer sizes
        socket_->set_option(boost::asio::socket_base::send_buffer_size(BUFFER_SIZE), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set send buffer size: " << opt_ec.message();
        }

        socket_->set_option(boost::asio::socket_base::receive_buffer_size(BUFFER_SIZE), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set receive buffer size: " << opt_ec.message();
        }
        
        peer_address_ = address;
        peer_port_ = port;
        state_.store(ConnectionState::State::CONNECTED);
        
        // Clear any existing buffers
        input_buffer_->consume(input_buffer_->size());
        output_buffer_->consume(output_buffer_->size());
        
        BOOST_LOG_TRIVIAL(info) << "Connected to " << peer_address_ << ":" << peer_port_;
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Connection error: " << e.what();
        handle_error(boost::system::error_code());
        return false;
    }
}

bool TcpPeer::disconnect() {
  if (!validate_state(ConnectionState::State::CONNECTED)) {
    BOOST_LOG_TRIVIAL(warning) << "Invalid state for disconnection: " 
                  << ConnectionState::state_to_string(get_connection_state());
    return false;
  }

  state_.store(ConnectionState::State::DISCONNECTING);
  stop_stream_processing();
  close_socket();
  state_.store(ConnectionState::State::DISCONNECTED);
  peer_address_.clear();
  peer_port_ = 0;
  BOOST_LOG_TRIVIAL(info) << "Successfully disconnected";
  return true;
}

std::ostream* TcpPeer::get_output_stream() {
  return is_connected() ? output_stream_.get() : nullptr;
}

std::istream* TcpPeer::get_input_stream() {
  return is_connected() ? input_stream_.get() : nullptr;
}

void TcpPeer::set_stream_processor(StreamProcessor processor) {
  if (!is_connected()) {
    BOOST_LOG_TRIVIAL(warning) << "Cannot set stream processor: peer not connected";
    return;
  }
  stream_processor_ = std::move(processor);
}

bool TcpPeer::start_stream_processing() {
  if (!is_connected() || !stream_processor_ || processing_) {
    BOOST_LOG_TRIVIAL(warning) << "Cannot start stream processing. Connected: " 
                  << is_connected() << ", Has processor: " 
                  << (stream_processor_ != nullptr) 
                  << ", Already processing: " << processing_;
    return false;
  }

  processing_ = true;
  processing_thread_ = std::make_unique<std::thread>(&TcpPeer::process_streams, this);
  BOOST_LOG_TRIVIAL(info) << "Started stream processing";
  return true;
}

void TcpPeer::stop_stream_processing() {
  if (processing_) {
    processing_ = false;
    if (processing_thread_ && processing_thread_->joinable()) {
      processing_thread_->join();
    }
    processing_thread_.reset();
    BOOST_LOG_TRIVIAL(info) << "Stopped stream processing";
  }
}

ConnectionState::State TcpPeer::get_connection_state() const {
  return state_.load();
}

void TcpPeer::process_streams() {
    BOOST_LOG_TRIVIAL(info) << "Starting stream processing for peer: " 
                << peer_address_ << ":" << peer_port_;
    
    while (processing_ && get_connection_state() == ConnectionState::State::CONNECTED) {
        try {
            boost::system::error_code ec;
            
            // Handle input processing
            if (input_buffer_->size() < MAX_BUFFER_SIZE) {
                boost::asio::socket_base::bytes_readable command;
                socket_->io_control(command);
                std::size_t bytes_readable = command.get();
                
                if (bytes_readable > 0) {
                    size_t available = MAX_BUFFER_SIZE - input_buffer_->size();
                    size_t to_read = std::min(available, std::min(bytes_readable, BUFFER_SIZE));
                    
                    auto read_buffer = input_buffer_->prepare(to_read);
                    size_t bytes = boost::asio::read(*socket_, read_buffer,
                        boost::asio::transfer_at_least(1), ec);
                        
                    if (ec) {
                        if (ec == boost::asio::error::eof) {
                            BOOST_LOG_TRIVIAL(info) << "Peer disconnected (EOF)";
                            if (get_connection_state() == ConnectionState::State::CONNECTED) {
                                state_.store(ConnectionState::State::DISCONNECTING);
                                state_.store(ConnectionState::State::DISCONNECTED);
                            }
                            break;
                        } else if (ec == boost::asio::error::operation_aborted) {
                            BOOST_LOG_TRIVIAL(info) << "Read operation aborted";
                            continue;
                        } else {
                            BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                            handle_error(ec);
                            break;
                        }
                    }
                    
                    input_buffer_->commit(bytes);
                    
                    // Process received data
                    if (stream_processor_) {
                        try {
                            stream_processor_(*input_stream_);
                            input_buffer_->consume(input_buffer_->size());  // Clear processed data
                        } catch (const std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
                            if (get_connection_state() == ConnectionState::State::CONNECTED) {
                                handle_error(boost::system::error_code(boost::asio::error::fault));
                                break;
                            }
                        }
                    }
                }
            } else {
                BOOST_LOG_TRIVIAL(warning) << "Input buffer full, waiting for processing";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Handle output processing
            if (output_buffer_->size() > 0) {
                const size_t chunk_size = BUFFER_SIZE;
                size_t remaining = output_buffer_->size();
                
                while (remaining > 0 && 
                       get_connection_state() == ConnectionState::State::CONNECTED) {
                    size_t to_write = std::min(remaining, chunk_size);
                    
                    size_t written = boost::asio::write(*socket_,
                        output_buffer_->data(),
                        boost::asio::transfer_exactly(to_write),
                        ec);
                    
                    if (ec) {
                        BOOST_LOG_TRIVIAL(error) << "Write error: " << ec.message();
                        handle_error(ec);
                        break;
                    }
                    
                    output_buffer_->consume(written);
                    remaining -= written;
                    
                    if (remaining > 0) {
                        BOOST_LOG_TRIVIAL(debug) << "Chunked write: " << written 
                                    << " bytes, " << remaining << " remaining";
                    }
                }
            }
            
            // Small delay to prevent busy-waiting
            size_t current_readable_bytes = 0;
            {
                boost::asio::socket_base::bytes_readable command;
                socket_->io_control(command);
                current_readable_bytes = command.get();
            }
            
            if (!current_readable_bytes && output_buffer_->size() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
            handle_error(boost::system::error_code(boost::asio::error::fault));
            break;
        }
    }
    
    BOOST_LOG_TRIVIAL(info) << "Stream processing stopped for peer: " 
                << peer_address_ << ":" << peer_port_;
}

void TcpPeer::handle_error(const std::error_code& error) {
    auto current_state = get_connection_state();
    BOOST_LOG_TRIVIAL(error) << "Socket error for peer " << peer_address_ << ":" 
                << peer_port_ << " (current state: " 
                << ConnectionState::state_to_string(current_state)
                << "): " << error.message();
    
    if (current_state != ConnectionState::State::ERROR && 
        current_state != ConnectionState::State::DISCONNECTED &&
        current_state != ConnectionState::State::DISCONNECTING) {
        BOOST_LOG_TRIVIAL(info) << "Transitioning from " 
                    << ConnectionState::state_to_string(current_state)
                    << " to ERROR state";
        
        // Stop processing and close socket
        stop_stream_processing();
        close_socket();
        
        // Attempt recovery for connection-related errors
        if (error.value() == boost::asio::error::connection_reset ||
            error.value() == boost::asio::error::connection_aborted ||
            error.value() == boost::asio::error::broken_pipe ||
            error.value() == boost::asio::error::connection_refused ||
            error.value() == boost::asio::error::timed_out ||
            error.value() == boost::asio::error::network_down ||
            error.value() == boost::asio::error::network_reset ||
            error.value() == boost::asio::error::network_unreachable ||
            error.value() == boost::asio::error::host_unreachable) {
            
            BOOST_LOG_TRIVIAL(info) << "Attempting connection recovery for peer "
                        << peer_address_ << ":" << peer_port_;
                        
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> jitter_dist(10, 100);
            
            for (int retry = 0; retry < MAX_RETRY_ATTEMPTS; ++retry) {
                // Exponential backoff with randomized jitter
                auto base_delay = std::chrono::milliseconds(100 * (1 << retry));
                auto jitter = std::chrono::milliseconds(jitter_dist(gen));
                auto backoff = base_delay + jitter;
                
                BOOST_LOG_TRIVIAL(info) << "Retry " << retry + 1 << "/" << MAX_RETRY_ATTEMPTS
                            << ", waiting " << backoff.count() << "ms";
                std::this_thread::sleep_for(backoff);
                
                if (connect(peer_address_, peer_port_)) {
                    BOOST_LOG_TRIVIAL(info) << "Successfully recovered connection to "
                                << peer_address_ << ":" << peer_port_
                                << " on attempt " << retry + 1;
                    return;
                }
                
                BOOST_LOG_TRIVIAL(warning) << "Recovery attempt " << retry + 1 
                             << " failed for peer " << peer_address_ 
                             << ":" << peer_port_;
            }
            
            BOOST_LOG_TRIVIAL(error) << "Failed to recover connection after "
                         << MAX_RETRY_ATTEMPTS << " attempts";
        } else {
            BOOST_LOG_TRIVIAL(error) << "Non-recoverable error encountered: "
                         << error.message();
        }
        
        // Update state
        state_.store(ConnectionState::State::ERROR);
        
        // Transition to DISCONNECTED if valid
        if (ConnectionState::is_valid_transition(ConnectionState::State::ERROR, 
                                               ConnectionState::State::DISCONNECTED)) {
            state_.store(ConnectionState::State::DISCONNECTED);
        }
    }
}

void TcpPeer::close_socket() {
  if (socket_ && socket_->is_open()) {
    boost::system::error_code ec;
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec && ec != boost::asio::error::not_connected) {
      BOOST_LOG_TRIVIAL(warning) << "Socket shutdown error: " << ec.message();
    }
    socket_->close(ec);
    if (ec) {
      BOOST_LOG_TRIVIAL(warning) << "Socket close error: " << ec.message();
    }
  }
}

} // namespace network
} // namespace dfs