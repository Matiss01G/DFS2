#include "network/tcp_peer.hpp"
#include <boost/bind/bind.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <iostream>

namespace dfs {
namespace network {

static constexpr size_t BUFFER_SIZE = 8192;

TcpPeer::TcpPeer()
    : socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_))
    , input_buffer_(std::make_unique<boost::asio::streambuf>())
    , output_buffer_(std::make_unique<boost::asio::streambuf>())
    , input_stream_(std::make_unique<std::istream>(input_buffer_.get()))
    , output_stream_(std::make_unique<std::ostream>(output_buffer_.get()))
    , state_(ConnectionState::State::INITIAL)
    , processing_(false) {
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
        BOOST_LOG_TRIVIAL(warning) << "Invalid state for connection: " << ConnectionState::state_to_string(get_connection_state());
        return false;
    }

    state_.store(ConnectionState::State::CONNECTING);
    
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(address, std::to_string(port));
        
        boost::asio::connect(*socket_, endpoints);
        socket_->set_option(boost::asio::ip::tcp::no_delay(true));
        
        state_.store(ConnectionState::State::CONNECTED);
        BOOST_LOG_TRIVIAL(info) << "Connected to " << address << ":" << port;
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
        BOOST_LOG_TRIVIAL(warning) << "Invalid state for disconnection: " << ConnectionState::state_to_string(get_connection_state());
        return false;
    }

    state_.store(ConnectionState::State::DISCONNECTING);
    stop_stream_processing();
    close_socket();
    state_.store(ConnectionState::State::DISCONNECTED);
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
        BOOST_LOG_TRIVIAL(info) << "Stopped stream processing";
    }
}

ConnectionState::State TcpPeer::get_connection_state() const {
    return state_.load();
}

void TcpPeer::process_streams() {
    BOOST_LOG_TRIVIAL(info) << "Starting stream processing";
    while (processing_ && is_connected()) {
        try {
            boost::system::error_code ec;
            
            // Read data into input buffer
            size_t bytes = boost::asio::read(*socket_,
                input_buffer_->prepare(BUFFER_SIZE),
                boost::asio::transfer_at_least(1),
                ec);
            
            if (ec) {
                if (ec != boost::asio::error::eof) {
                    BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                    handle_error(ec);
                }
                break;
            }
            
            input_buffer_->commit(bytes);
            
            // Process received data
            if (stream_processor_) {
                stream_processor_(*input_stream_);
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
    BOOST_LOG_TRIVIAL(info) << "Stream processing stopped";
}

void TcpPeer::handle_error(const std::error_code& error) {
    BOOST_LOG_TRIVIAL(error) << "Socket error: " << error.message();
    state_.store(ConnectionState::State::ERROR);
    close_socket();
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