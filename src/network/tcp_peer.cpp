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
    initialize_streams();
    BOOST_LOG_TRIVIAL(info) << "TCP_Peer created with ID: " << peer_id;
}

TCP_Peer::~TCP_Peer() {
    if (is_connected()) {
        disconnect();
    }
    BOOST_LOG_TRIVIAL(debug) << "TCP_Peer destroyed: " << peer_id_;
}

void TCP_Peer::initialize_streams() {
    input_stream_ = std::make_unique<std::istream>(input_buffer_.get());
    output_stream_ = std::make_unique<std::ostream>(output_buffer_.get());
}

bool TCP_Peer::connect(const std::string& address, uint16_t port) {
    if (!validate_connection_state(ConnectionState::State::INITIAL)) {
        BOOST_LOG_TRIVIAL(error) << "Invalid state for connection: " << 
            ConnectionState::state_to_string(get_connection_state());
        return false;
    }

    try {
        BOOST_LOG_TRIVIAL(info) << "Attempting to connect to " << address << ":" << port;
        connection_state_.transition_to(ConnectionState::State::CONNECTING);
        
        boost::asio::ip::tcp::resolver resolver(io_context_);
        boost::system::error_code resolve_ec;
        auto endpoints = resolver.resolve(address, std::to_string(port), resolve_ec);
        
        if (resolve_ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to resolve address: " << resolve_ec.message();
            connection_state_.transition_to(ConnectionState::State::ERROR);
            return false;
        }

        endpoint_ = std::make_unique<boost::asio::ip::tcp::endpoint>(*endpoints.begin());
        
        boost::system::error_code connect_ec;
        socket_->connect(*endpoint_, connect_ec);
        
        if (connect_ec) {
            BOOST_LOG_TRIVIAL(error) << "Connection failed: " << connect_ec.message();
            connection_state_.transition_to(ConnectionState::State::ERROR);
            return false;
        }

        // Configure socket
        socket_->set_option(boost::asio::ip::tcp::no_delay(true));
        socket_->set_option(boost::asio::socket_base::keep_alive(true));
        
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
    if (!validate_connection_state(ConnectionState::State::CONNECTED)) {
        return false;
    }

    try {
        connection_state_.transition_to(ConnectionState::State::DISCONNECTING);
        
        stop_stream_processing();
        
        if (socket_->is_open()) {
            boost::system::error_code ec;
            socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_->close(ec);
        }

        connection_state_.transition_to(ConnectionState::State::DISCONNECTED);
        BOOST_LOG_TRIVIAL(info) << "Disconnected peer: " << peer_id_;
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Disconnect error: " << e.what();
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
    stream_processor_ = std::move(processor);
}

bool TCP_Peer::start_stream_processing() {
    if (!validate_connection_state(ConnectionState::State::CONNECTED) || 
        !stream_processor_) {
        return false;
    }

    if (processing_active_) {
        return true;  // Already processing
    }

    processing_active_ = true;
    processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
    BOOST_LOG_TRIVIAL(info) << "Started stream processing for peer: " << peer_id_;
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
    while (processing_active_ && socket_->is_open()) {
        try {
            boost::system::error_code ec;
            size_t bytes = boost::asio::read(*socket_, *input_buffer_,
                boost::asio::transfer_at_least(1), ec);

            if (ec) {
                if (ec != boost::asio::error::eof) {
                    BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                }
                break;
            }

            if (bytes > 0 && stream_processor_) {
                stream_processor_(*input_stream_);
            }
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
            break;
        }
    }
}

bool TCP_Peer::validate_connection_state(ConnectionState::State required_state) const {
    auto current_state = get_connection_state();
    if (current_state != required_state) {
        BOOST_LOG_TRIVIAL(warning) << "Invalid state transition attempted. Current: " 
            << ConnectionState::state_to_string(current_state) 
            << ", Required: " << ConnectionState::state_to_string(required_state);
        return false;
    }
    return true;
}

void TCP_Peer::cleanup_connection() {
    if (socket_ && socket_->is_open()) {
        try {
            boost::system::error_code ec;
            socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_->close(ec);
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error during connection cleanup: " << e.what();
        }
    }
    
    endpoint_.reset();
    processing_active_ = false;
    
    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
    }
    processing_thread_.reset();
}

} // namespace network
} // namespace dfs