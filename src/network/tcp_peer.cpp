#include "network/tcp_peer.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
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
    if (is_connected()) {
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

    state_.store(ConnectionState::State::CONNECTING);
    
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(address, std::to_string(port));
        
        // Set up async connect with timeout
        boost::system::error_code ec;
        socket_->async_connect(endpoints->endpoint(),
            [&ec](const boost::system::error_code& result_ec) {
                ec = result_ec;
            });

        // Run with timeout
        io_context_.run_for(CONNECTION_TIMEOUT);

        if (ec || !socket_->is_open()) {
            BOOST_LOG_TRIVIAL(error) << "Connection timeout or error: " 
                                    << (ec ? ec.message() : "Connection timeout");
            handle_error(ec);
            return false;
        }

        // Configure socket options
        boost::system::error_code opt_ec;
        socket_->set_option(boost::asio::ip::tcp::no_delay(true), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set TCP_NODELAY: " << opt_ec.message();
        }

        socket_->set_option(boost::asio::socket_base::keep_alive(true), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set SO_KEEPALIVE: " << opt_ec.message();
        }

        socket_->set_option(boost::asio::socket_base::linger(true, 0), opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set SO_LINGER: " << opt_ec.message();
        }
        
        // Set socket timeouts with error checking
        socket_->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{
            static_cast<int>(SOCKET_TIMEOUT.count())}, opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set SO_RCVTIMEO: " << opt_ec.message();
        }

        socket_->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_SNDTIMEO>{
            static_cast<int>(SOCKET_TIMEOUT.count())}, opt_ec);
        if (opt_ec) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to set SO_SNDTIMEO: " << opt_ec.message();
        }
        
        peer_address_ = address;
        peer_port_ = port;
        state_.store(ConnectionState::State::CONNECTED);
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
    
    while (processing_ && is_connected()) {
        try {
            boost::system::error_code ec;
            
            // Check buffer size before reading
            if (input_buffer_->size() >= MAX_BUFFER_SIZE) {
                BOOST_LOG_TRIVIAL(warning) << "Input buffer full, waiting for processing";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Read data into input buffer with overflow protection
            size_t available = MAX_BUFFER_SIZE - input_buffer_->size();
            size_t to_read = std::min(available, BUFFER_SIZE);
            
            boost::asio::socket_base::bytes_readable command;
            socket_->io_control(command);
            std::size_t bytes_readable = command.get();
            
            if (bytes_readable == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            size_t bytes = boost::asio::read(*socket_,
                input_buffer_->prepare(std::min(to_read, bytes_readable)),
                boost::asio::transfer_at_least(1),
                ec);
            
            if (ec) {
                if (ec == boost::asio::error::eof) {
                    BOOST_LOG_TRIVIAL(info) << "Peer disconnected (EOF)";
                    state_.store(ConnectionState::State::DISCONNECTED);
                } else if (ec == boost::asio::error::operation_aborted) {
                    BOOST_LOG_TRIVIAL(info) << "Read operation aborted";
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                    handle_error(ec);
                }
                break;
            }
            
            input_buffer_->commit(bytes);
            
            // Process received data with proper error handling
            if (stream_processor_) {
                try {
                    stream_processor_(*input_stream_);
                } catch (const std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
                    if (is_connected()) {
                        // Only attempt error recovery if still connected
                        handle_error(boost::system::error_code(boost::asio::error::fault));
                    }
                }
            }
            
            // Write any pending output with chunked transfer
            if (output_buffer_->size() > 0) {
                size_t remaining = output_buffer_->size();
                const size_t chunk_size = BUFFER_SIZE;
                
                while (remaining > 0 && is_connected()) {
                    size_t to_write = std::min(remaining, chunk_size);
                    size_t written = boost::asio::write(*socket_,
                        output_buffer_->data(),
                        boost::asio::transfer_exactly(to_write),
                        ec);
                    
                    if (ec) {
                        BOOST_LOG_TRIVIAL(error) << "Write error after " << (output_buffer_->size() - remaining)
                                               << " bytes: " << ec.message();
                        handle_error(ec);
                        break;
                    }
                    
                    output_buffer_->consume(written);
                    remaining -= written;
                    
                    if (remaining > 0) {
                        BOOST_LOG_TRIVIAL(debug) << "Chunked write: " << written << " bytes, "
                                               << remaining << " bytes remaining";
                    }
                }
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
            handle_error(boost::system::error_code());
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
    
    if (current_state != ConnectionState::State::ERROR) {
        BOOST_LOG_TRIVIAL(info) << "Transitioning from " 
                                << ConnectionState::state_to_string(current_state)
                                << " to ERROR state";
        
        // Close existing socket
        close_socket();
        
        // Attempt recovery for connection-related errors
        if (error.value() == boost::asio::error::connection_reset ||
            error.value() == boost::asio::error::connection_aborted ||
            error.value() == boost::asio::error::broken_pipe ||
            error.value() == boost::asio::error::connection_refused ||
            error.value() == boost::asio::error::timed_out) {
            
            BOOST_LOG_TRIVIAL(info) << "Attempting connection recovery for peer "
                                  << peer_address_ << ":" << peer_port_;
                                  
            for (int retry = 0; retry < MAX_RETRY_ATTEMPTS; ++retry) {
                // Exponential backoff with jitter
                auto base_delay = std::chrono::milliseconds(100 * (1 << retry));
                auto jitter = std::chrono::milliseconds(rand() % 100);
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
        
        state_.store(ConnectionState::State::ERROR);
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