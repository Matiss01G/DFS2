#include "network/tcp_peer.hpp"
#include <boost/bind/bind.hpp>
#include <iostream>

namespace dfs {
namespace network {

TcpPeer::TcpPeer()
    : socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_))
    , input_buffer_(std::make_unique<boost::asio::streambuf>())
    , output_buffer_(std::make_unique<boost::asio::streambuf>())
    , input_stream_(std::make_unique<std::istream>(input_buffer_.get()))
    , output_stream_(std::make_unique<std::ostream>(output_buffer_.get()))
    , state_(ConnectionState::State::INITIAL)
    , processing_(false) {
}

TcpPeer::~TcpPeer() {
    if (is_connected()) {
        disconnect();
    }
}

bool TcpPeer::connect(const std::string& address, uint16_t port) {
    if (state_ != ConnectionState::State::INITIAL && 
        state_ != ConnectionState::State::DISCONNECTED) {
        return false;
    }

    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(address, std::to_string(port));
        
        boost::asio::connect(*socket_, endpoints);
        socket_->set_option(boost::asio::ip::tcp::no_delay(true));
        
        state_ = ConnectionState::State::CONNECTED;
        BOOST_LOG_TRIVIAL(info) << "Connected to " << address << ":" << port;
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Connection error: " << e.what();
        state_ = ConnectionState::State::ERROR;
        return false;
    }
}

bool TcpPeer::disconnect() {
    if (!is_connected()) {
        return false;
    }

    try {
        stop_stream_processing();
        close_socket();
        state_ = ConnectionState::State::DISCONNECTED;
        BOOST_LOG_TRIVIAL(info) << "Disconnected from peer";
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Disconnection error: " << e.what();
        state_ = ConnectionState::State::ERROR;
        return false;
    }
}

std::ostream* TcpPeer::get_output_stream() {
    return is_connected() ? output_stream_.get() : nullptr;
}

std::istream* TcpPeer::get_input_stream() {
    return is_connected() ? input_stream_.get() : nullptr;
}

void TcpPeer::set_stream_processor(StreamProcessor processor) {
    stream_processor_ = processor;
}

bool TcpPeer::start_stream_processing() {
    if (!is_connected() || processing_ || !stream_processor_) {
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
    return state_;
}

void TcpPeer::process_streams() {
    while (processing_ && is_connected()) {
        try {
            // Read data into input buffer
            boost::system::error_code error;
            size_t bytes = boost::asio::read(*socket_,
                *input_buffer_,
                boost::asio::transfer_at_least(1),
                error);

            if (error) {
                if (error != boost::asio::error::eof) {
                    handle_error(error);
                }
                break;
            }

            if (bytes > 0 && stream_processor_) {
                stream_processor_(*input_stream_);
            }

            // Write any pending output
            if (output_buffer_->size() > 0) {
                boost::asio::write(*socket_, *output_buffer_, error);
                if (error) {
                    handle_error(error);
                    break;
                }
            }
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
            state_ = ConnectionState::State::ERROR;
            break;
        }
    }
}

void TcpPeer::handle_error(const std::error_code& error) {
    BOOST_LOG_TRIVIAL(error) << "Socket error: " << error.message();
    state_ = ConnectionState::State::ERROR;
    processing_ = false;
}

void TcpPeer::close_socket() {
    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_->close(ec);
    }
}

} // namespace network
} // namespace dfs
