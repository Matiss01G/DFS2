#include "network/tcp_peer.hpp"
#include <stdexcept>

namespace dfs {
namespace network {

TCP_Peer::TCP_Peer(const std::string& peer_id)
  : peer_id_(peer_id),  
    socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_context_)),  
    input_buffer_(std::make_unique<boost::asio::streambuf>()) {  
    initialize_streams();
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Constructing TCP_Peer";
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Input stream initialized";
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] TCP_Peer instance created successfully";
}

TCP_Peer::~TCP_Peer() {
    cleanup_connection();
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

    processing_active_ = true;
    processing_thread_ = std::make_unique<std::thread>(&TCP_Peer::process_stream, this);
    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing started successfully";
    return true;
}

void TCP_Peer::stop_stream_processing() {
    if (processing_active_) {
        BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Stopping stream processing";

        processing_active_ = false;

        if (socket_ && socket_->is_open()) {
            boost::system::error_code ec;
            socket_->cancel(ec);
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Error canceling socket operations: " << ec.message();
            }
        }

        io_context_.stop();

        if (processing_thread_ && processing_thread_->joinable()) {
            processing_thread_->join();
            processing_thread_.reset();
            BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Processing thread joined";
        }

        io_context_.restart();

        BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
    }
}

void TCP_Peer::process_stream() {
    BOOST_LOG_TRIVIAL(debug) << "[" << peer_id_ << "] Setting up stream processing";

    try {
        auto work_guard = boost::asio::make_work_guard(io_context_);

        if (processing_active_ && socket_->is_open()) {
            async_read_next();
        }

        while (processing_active_ && socket_->is_open()) {
            io_context_.run_one();
        }

        work_guard.reset();

        while (io_context_.poll_one()) {}

    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Stream processing error: " << e.what();
    }

    BOOST_LOG_TRIVIAL(info) << "[" << peer_id_ << "] Stream processing stopped";
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

    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
        processing_thread_.reset();
    }

    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;

        socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket shutdown error: " << ec.message();
        }

        socket_->close(ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "[" << peer_id_ << "] Socket close error: " << ec.message();
        }
    }

    if (input_buffer_) {
        input_buffer_->consume(input_buffer_->size());
    }

    endpoint_.reset();
}

bool TCP_Peer::send_message(const std::string& message) {
    std::istringstream iss(message);
    return send_stream(iss);
}

const std::string& TCP_Peer::get_peer_id() const {
    return peer_id_;
}

boost::asio::ip::tcp::socket& TCP_Peer::get_socket() {
    return *socket_;
}

} // namespace network
} // namespace dfs