#include "network/tcp_peer.hpp"
#include <stdexcept>
#include <future>

namespace dfs {
namespace network {

// Constructor initializes a TCP peer with a unique identifier
TCP_Peer::TCP_Peer(const std::string& peer_id)
  : peer_id_(peer_id),
    socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),
    input_buffer_(std::make_unique<boost::asio::streambuf>()) {
    initialize_streams();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Constructing TCP_Peer";
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input stream initialized";
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] TCP_Peer instance created successfully";
}

// Destructor ensures proper cleanup
TCP_Peer::~TCP_Peer() {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Destroying TCP_Peer";
    if (socket_ && socket_->is_open()) {
        disconnect();
    }
    BOOST_LOG_TRIVIAL(debug) << "TCP_Peer destroyed: " << peer_id_;
}

void TCP_Peer::initialize_streams() {
    input_stream_ = std::make_unique<std::istream>(input_buffer_.get());
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

    {
        std::lock_guard<std::mutex> lock(io_mutex_);
        processing_active_ = true;
        io_context_.restart();
        processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
    }

    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing started successfully";
    return true;
}

void TCP_Peer::stop_stream_processing() {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stopping stream processing";

    {
        std::lock_guard<std::mutex> lock(io_mutex_);
        if (!processing_active_) {
            return;
        }
        processing_active_ = false;
    }

    // Cancel any pending operations
    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->cancel(ec);
    }

    // Stop io_context
    io_context_.stop();

    // Wait for processing thread with timeout
    if (processing_thread_ && processing_thread_->joinable()) {
        std::future<void> future = std::async(std::launch::async, [this]() {
            if (processing_thread_) {
                processing_thread_->join();
            }
        });

        if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Processing thread join timeout";
        }
        processing_thread_.reset();
    }

    {
        std::lock_guard<std::mutex> lock(io_mutex_);
        io_context_.restart();
    }

    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
}

void TCP_Peer::process_stream() {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting up stream processing";

    try {
        boost::asio::io_context::work work(io_context_);

        // Start initial read if conditions are met
        {
            std::lock_guard<std::mutex> lock(io_mutex_);
            if (processing_active_ && socket_->is_open()) {
                async_read_next();
            }
        }

        while (true) {
            {
                std::lock_guard<std::mutex> lock(io_mutex_);
                if (!processing_active_ || !socket_->is_open()) {
                    break;
                }
                io_context_.run_one();
            }
            std::this_thread::yield();
        }

        // Cleanup remaining handlers
        {
            std::lock_guard<std::mutex> lock(io_mutex_);
            while (io_context_.poll()) {}
        }

    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
    }

    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing thread ended";
}

void TCP_Peer::async_read_next() {
    if (!processing_active_ || !socket_->is_open()) {
        return;
    }

    BOOST_LOG_TRIVIAL(trace) << "[" << peer_id_ << "] Setting up next async read";

    boost::asio::async_read_until(
        *socket_,
        *input_buffer_,
        '\n',
        [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
                std::string data;
                std::istream is(input_buffer_.get());
                std::getline(is, data);

                if (!data.empty()) {
                    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Received data: " << data;

                    if (stream_processor_) {
                        std::string framed_data = data + '\n';
                        std::istringstream iss(framed_data);
                        try {
                            stream_processor_(iss);
                        } catch (const std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processor error: " << e.what();
                        }
                    } else {
                        std::string framed_data = data + '\n';
                        input_buffer_->sputn(framed_data.c_str(), framed_data.length());
                        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Data forwarded to input stream";
                    }
                }

                if (processing_active_ && socket_->is_open()) {
                    async_read_next();
                }
            } else if (ec != boost::asio::error::operation_aborted) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Read error: " << ec.message();
                if (processing_active_ && socket_->is_open()) {
                    async_read_next();
                }
            }
        });
}

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

        stop_stream_processing();
        cleanup_connection();

        BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Successfully disconnected";
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Disconnect error: " << e.what();
        return false;
    }
}

bool TCP_Peer::send_message(const std::string& message) {
    std::istringstream iss(message);
    return send_stream(iss);
}

bool TCP_Peer::send_stream(std::istream& input_stream, std::size_t buffer_size) {
    if (!socket_ || !socket_->is_open()) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Cannot send stream - socket not connected";
        return false;
    }

    try {
        std::unique_lock<std::mutex> lock(io_mutex_);
        std::vector<char> buffer(buffer_size);
        boost::system::error_code ec;

        while (input_stream.good()) {
            input_stream.read(buffer.data(), buffer_size);
            std::size_t bytes_read = input_stream.gcount();

            if (bytes_read == 0) {
                break;
            }

            std::size_t bytes_written = boost::asio::write(
                *socket_,
                boost::asio::buffer(buffer.data(), bytes_read),
                boost::asio::transfer_exactly(bytes_read),
                ec
            );

            if (ec || bytes_written != bytes_read) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream send error: " << ec.message();
                return false;
            }

            BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Sent " << bytes_written << " bytes from stream";
        }

        return true;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream send error: " << e.what();
        return false;
    }
}

void TCP_Peer::cleanup_connection() {
    processing_active_ = false;

    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->cancel(ec);
    }

    io_context_.stop();

    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
        processing_thread_.reset();
    }

    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket shutdown error: " << ec.message();
        }
        socket_->close(ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket close error: " << ec.message();
        }
    }

    io_context_.reset();

    if (input_buffer_) {
        input_buffer_->consume(input_buffer_->size());
    }

    endpoint_.reset();
}

bool TCP_Peer::is_connected() const {
    return socket_ && socket_->is_open();
}

} // namespace network
} // namespace dfs