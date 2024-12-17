#include "network/tcp_peer.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <iostream>

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
}

bool TcpPeer::connect(const std::string& address, uint16_t port) {
    if (!validate_state(ConnectionState::State::INITIAL) && 
        !validate_state(ConnectionState::State::DISCONNECTED)) {
        BOOST_LOG_TRIVIAL(warning) << "Invalid state for connection: " 
                                  << ConnectionState::state_to_string(get_connection_state());
        return false;
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
        socket_->set_option(boost::asio::ip::tcp::no_delay(true));
        socket_->set_option(boost::asio::socket_base::keep_alive(true));
        socket_->set_option(boost::asio::socket_base::linger(true, 0));
        
        // Set socket timeouts
        socket_->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>{
            static_cast<int>(SOCKET_TIMEOUT.count())});
        socket_->set_option(boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_SNDTIMEO>{
            static_cast<int>(SOCKET_TIMEOUT.count())});
        
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
            
            size_t bytes = boost::asio::read(*socket_,
                input_buffer_->prepare(to_read),
                boost::asio::transfer_at_least(1),
                ec);
            
            if (ec) {
                if (ec != boost::asio::error::eof) {
                    BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                    handle_error(ec);
                } else {
                    BOOST_LOG_TRIVIAL(info) << "Peer disconnected (EOF)";
                }
                break;
            }
            
            input_buffer_->commit(bytes);
            
            // Process received data
            if (stream_processor_) {
                try {
                    stream_processor_(*input_stream_);
                } catch (const std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
                }
            }
            
            // Write any pending output
            if (output_buffer_->size() > 0) {
                boost::asio::write(*socket_, *output_buffer_, ec);
                if (ec) {
                    BOOST_LOG_TRIVIAL(error) << "Write error: " << ec.message();
                    handle_error(ec);
                    break;
                }
                output_buffer_->consume(output_buffer_->size());
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
        
        // Attempt recovery based on error type
        if (error == boost::asio::error::connection_reset ||
            error == boost::asio::error::connection_aborted ||
            error == boost::asio::error::broken_pipe) {
            
            BOOST_LOG_TRIVIAL(info) << "Attempting connection recovery...";
            for (int retry = 0; retry < MAX_RETRY_ATTEMPTS; ++retry) {
                std::chrono::milliseconds backoff(100 * (1 << retry));  // Exponential backoff
                std::this_thread::sleep_for(backoff);
                
                if (connect(peer_address_, peer_port_)) {
                    BOOST_LOG_TRIVIAL(info) << "Successfully recovered connection on attempt " << retry + 1;
                    return;
                }
                BOOST_LOG_TRIVIAL(warning) << "Recovery attempt " << retry + 1 << " failed";
            }
        }
        
        state_.store(ConnectionState::State::ERROR);
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