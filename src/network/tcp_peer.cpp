#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

// Implementation methods for PeerManager's use
bool TCP_Peer::connect_impl(const std::string& address, uint16_t port) {
    if (socket_->is_open()) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot connect - socket already connected";
        return false;
    }

    try {
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Entering connect() method";
        BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Initiating connection to " << address << ":" << port;

        // Convert hostname/address string to IP address using async resolver
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

        // Attempt synchronous connection to resolved endpoint
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Attempting socket connection";
        boost::system::error_code connect_ec;
        socket_->connect(*endpoint_, connect_ec);

        if (connect_ec) {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection failed: " << connect_ec.message();
            cleanup_connection();  // Clean up resources on failure
            return false;
        }

        // Reset IO context if it's active to ensure clean state
        if (!io_context_.stopped()) {
            io_context_.reset();
        }

        BOOST_LOG_TRIVIAL(info) << "Successfully connected to " << address << ":" << port;
        return true;
    }
    catch (const std::exception& e) {
        // Handle any unexpected errors during connection process
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Connection error: " << e.what();
        cleanup_connection();  // Ensure resources are cleaned up
        return false;
    }
}

bool TCP_Peer::disconnect_impl() {
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

bool TCP_Peer::is_connected_impl() const {
    return socket_ && socket_->is_open();
}

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

// Destructor ensures proper cleanup
TCP_Peer::~TCP_Peer() {
    if (socket_ && socket_->is_open()) {
        disconnect_impl();
    }
    BOOST_LOG_TRIVIAL(debug) << "TCP_Peer destroyed: " << peer_id_;
}

// Initialize input stream for reading incoming data
void TCP_Peer::initialize_streams() {
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

        // Signal processing to stop
        processing_active_ = false;

        // Cancel any pending asynchronous operations
        if (socket_ && socket_->is_open()) {
            boost::system::error_code ec;
            socket_->cancel(ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Error canceling socket operations: " << ec.message();
            }
        }

        // Stop io_context
        io_context_.stop();

        // Wait for processing thread to complete
        if (processing_thread_ && processing_thread_->joinable()) {
            processing_thread_->join();
            processing_thread_.reset();
            BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing thread joined";
        }

        // Reset io_context for potential reuse
        io_context_.restart();

        BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
    }
}

// Main processing function that sets up async reading
void TCP_Peer::process_stream() {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting up stream processing";

    try {
        // Create work guard to keep io_context running
        auto work_guard = boost::asio::make_work_guard(io_context_);

        // Start the initial async read operation
        if (processing_active_ && socket_->is_open()) {
            async_read_next();
        }

        // Run IO context in this thread
        while (processing_active_ && socket_->is_open()) {
            io_context_.run_one();
        }

        // Release work guard to allow io_context to stop
        work_guard.reset();

        // Run any remaining handlers
        while (io_context_.poll_one()) {}

    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
    }

    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
}

// Handles the actual async reading of data
void TCP_Peer::async_read_next() {
    // Skip if processing is stopped or socket is closed
    if (!processing_active_ || !socket_->is_open()) {
        return;
    }

    BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Setting up next async read";

    // Set up asynchronous read operation that continues until newline character
    boost::asio::async_read_until(
        *socket_,
        *input_buffer_,
        '\n',  // Delimiter for message boundary
        [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
                // Extract data from buffer into string
                std::string data;
                std::istream is(input_buffer_.get());
                std::getline(is, data);

                if (!data.empty()) {
                    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Received data: " << data;

                    if (stream_processor_) {
                        // If stream processor exists, pass data through it
                        std::string framed_data = data + '\n';
                        std::istringstream iss(framed_data);
                        try {
                            stream_processor_(iss);  // Process data using registered callback
                        } catch (const std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processor error: " << e.what();
                        }
                    } else {
                        // No processor - store data in input buffer for manual retrieval
                        std::string framed_data = data + '\n';
                        input_buffer_->sputn(framed_data.c_str(), framed_data.length());
                        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Data forwarded to input stream";
                    }
                }

                // Set up next read if still active
                if (processing_active_ && socket_->is_open()) {
                    async_read_next();  // Chain the next read operation
                }
            } else if (ec != boost::asio::error::operation_aborted) {
                // Handle non-abort errors (e.g., connection loss)
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Read error: " << ec.message();
                if (processing_active_ && socket_->is_open()) {
                    async_read_next();  // Attempt to recover by initiating new read
                }
            }
        });
}

// Send data from an input stream through the socket
bool TCP_Peer::send_message(const std::string& message) {
    std::istringstream iss(message);
    return send_stream(iss);
}

bool TCP_Peer::send_stream(std::istream& input_stream, std::size_t buffer_size) {
    // Verify socket is valid and connected before attempting transmission
    if (!socket_ || !socket_->is_open()) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot send stream - socket not connected";
        return false;
    }

    try {
        // Lock to prevent concurrent socket writes
        std::unique_lock<std::mutex> lock(io_mutex_);
        std::vector<char> buffer(buffer_size);  // Temporary buffer for chunked reading
        boost::system::error_code ec;

        // Continue reading and sending while input stream is valid
        while (input_stream.good()) {
            // Read chunk from input stream into buffer
            input_stream.read(buffer.data(), buffer_size);
            std::size_t bytes_read = input_stream.gcount();  // Get actual bytes read

            if (bytes_read == 0) {
                break;  // Exit if no more data to read
            }

            // Transmit chunk through socket synchronous write with exact transfer size guarantee
            std::size_t bytes_written = boost::asio::write(
                *socket_,
                boost::asio::buffer(buffer.data(), bytes_read),
                boost::asio::transfer_exactly(bytes_read),  // Ensure all bytes are written
                ec
            );

            // Verify transmission success
            if (ec || bytes_written != bytes_read) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream send error: " << ec.message();
                return false;  // Return on any transmission error
            }

            BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Sent " << bytes_written << " bytes from stream";
        }

        return true;  // All data sent successfully
    } catch (const std::exception& e) {
        // Handle any unexpected errors during transmission
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream send error: " << e.what();
        return false;
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

// Get the peer's unique identifier
const std::string& TCP_Peer::get_peer_id() const {
    return peer_id_;
}

} // namespace network
} // namespace dfs