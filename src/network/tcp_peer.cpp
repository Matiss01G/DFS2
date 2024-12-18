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

        // Configure socket options for optimal performance
        socket_->set_option(boost::asio::ip::tcp::no_delay(true));
        socket_->set_option(boost::asio::socket_base::keep_alive(true));
        socket_->set_option(boost::asio::socket_base::send_buffer_size(8192));
        socket_->set_option(boost::asio::socket_base::receive_buffer_size(8192));
        socket_->set_option(boost::asio::socket_base::reuse_address(true));
        socket_->set_option(boost::asio::socket_base::linger(true, 0));
        
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
    if (!validate_connection_state(ConnectionState::State::CONNECTED)) {
        return false;
    }

    try {
        connection_state_.transition_to(ConnectionState::State::DISCONNECTING);
        
        // First stop any ongoing stream processing
        stop_stream_processing();
        
        // Then cleanup the connection
        cleanup_connection();

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
    boost::asio::io_context::work work(io_context_);
    std::thread io_thread([this]() { io_context_.run(); });

    while (processing_active_ && socket_->is_open()) {
        try {
            boost::system::error_code ec;
            
            // Handle outgoing data synchronously with proper flushing
            if (output_buffer_->size() > 0) {
                std::unique_lock<std::mutex> lock(io_mutex_);
                std::size_t bytes_to_write = output_buffer_->size();
                
                boost::asio::write(
                    *socket_, 
                    output_buffer_->data(),
                    boost::asio::transfer_exactly(bytes_to_write),
                    ec
                );
                
                if (!ec) {
                    output_buffer_->consume(bytes_to_write);
                    BOOST_LOG_TRIVIAL(debug) << "Wrote " << bytes_to_write << " bytes";
                    
                    // Ensure data is sent by flushing the socket
                    socket_->lowest_layer().flush(ec);
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Write error: " << ec.message();
                    break;
                }
            }
            
            // Check for incoming data with proper synchronization
            {
                std::unique_lock<std::mutex> lock(io_mutex_);
                
                if (socket_->available(ec) > 0 || !ec) {
                    // Read until newline delimiter
                    boost::asio::read_until(
                        *socket_,
                        *input_buffer_,
                        '\n',
                        ec
                    );
                    
                    if (!ec) {
                        // Get the complete message
                        std::string data;
                        std::istream is(input_buffer_.get());
                        std::getline(is, data);
                        
                        if (!data.empty()) {
                            BOOST_LOG_TRIVIAL(debug) << "Received data: " << data;
                            
                            if (stream_processor_) {
                                // Add back the newline for consistent processing
                                data += '\n';
                                std::istringstream iss(data);
                                stream_processor_(iss);
                            }
                        }
                    } else if (ec != boost::asio::error::would_block && 
                             ec != boost::asio::error::try_again &&
                             ec != boost::asio::error::eof) {
                        BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                        break;
                    }
                }
            }
            
            // Small delay to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
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
    if (current_state != required_state) {
        BOOST_LOG_TRIVIAL(warning) << "Invalid state transition attempted. Current: " 
            << ConnectionState::state_to_string(current_state) 
            << ", Required: " << ConnectionState::state_to_string(required_state);
        return false;
    }
    return true;
}

void TCP_Peer::cleanup_connection() {
    // First, ensure processing is stopped
    processing_active_ = false;
    
    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
        processing_thread_.reset();
    }
    
    // Then cleanup socket
    if (socket_) {
        boost::system::error_code ec;
        if (socket_->is_open()) {
            // Shutdown the socket first
            socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Socket shutdown error: " << ec.message();
            }
            
            // Then close it
            socket_->close(ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Socket close error: " << ec.message();
            }
        }
    }
    
    // Clear any remaining data in buffers
    if (input_buffer_) {
        input_buffer_->consume(input_buffer_->size());
    }
    if (output_buffer_) {
        output_buffer_->consume(output_buffer_->size());
    }
    
    endpoint_.reset();
}

} // namespace network
} // namespace dfs