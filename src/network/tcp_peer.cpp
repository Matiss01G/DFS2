#include "../../include/network/tcp_peer.hpp"
#include <boost/log/trivial.hpp>
#include <istream>
#include <sstream>

namespace dfs::network {

TcpPeer::TcpPeer(boost::asio::io_context& io_context,
                 const std::string& address,
                 uint16_t port,
                 std::shared_ptr<crypto::CryptoStream> crypto_stream)
    : io_context_(io_context)
    , socket_(io_context)
    , endpoint_(boost::asio::ip::address::from_string(address), port)
    , crypto_stream_(crypto_stream) {
    // Generate random peer ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (auto& byte : peer_id_) {
        byte = dist(gen);
    }
}

TcpPeer::~TcpPeer() {
    disconnect();
}

void TcpPeer::connect() {
    if (connected_) return;

    socket_.async_connect(endpoint_,
        [self = shared_from_this()](const boost::system::error_code& error) {
            if (!error) {
                self->connected_ = true;
                BOOST_LOG_TRIVIAL(info) << "Connected to peer: " 
                                      << self->endpoint_.address().to_string()
                                      << ":" << self->endpoint_.port();
                self->start_read_header();
            } else {
                self->handle_error(error);
            }
        });
}

void TcpPeer::disconnect() {
    if (!connected_) return;

    boost::system::error_code ec;
    socket_.close(ec);
    connected_ = false;

    if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Error closing socket: " << ec.message();
    }
}

bool TcpPeer::is_connected() const {
    return connected_;
}

void TcpPeer::send_message(MessageType type,
                          std::shared_ptr<std::istream> payload,
                          const std::vector<uint8_t>& file_key) {
    if (!connected_) {
        if (error_handler_) {
            error_handler_(NetworkError::PEER_DISCONNECTED);
        }
        return;
    }

    // Create message header
    MessageHeader header;
    header.type = type;
    
    // Generate random IV for this message
    std::random_device rd;
    std::generate(header.iv.begin(), header.iv.end(), rd);
    
    // Copy source ID and file key
    std::copy(peer_id_.begin(), peer_id_.end(), header.source_id.begin());
    if (!file_key.empty()) {
        std::copy(file_key.begin(), file_key.end(), header.file_key.begin());
    }

    // Get payload size
    payload->seekg(0, std::ios::end);
    header.payload_size = payload->tellg();
    payload->seekg(0);

    // Queue the message
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push({header, payload});
    }

    // Start async write if not already writing
    if (!writing_) {
        io_context_.post([self = shared_from_this()]() {
            self->start_write();
        });
    }
}

void TcpPeer::start_read_header() {
    auto header_buffer = std::make_shared<std::vector<uint8_t>>(MessageHeader::HEADER_SIZE);
    
    boost::asio::async_read(socket_,
        boost::asio::buffer(*header_buffer),
        [self = shared_from_this(), header_buffer](const boost::system::error_code& error,
                                                 std::size_t /*bytes_transferred*/) {
            self->handle_read_header(error, *header_buffer);
        });
}

void TcpPeer::handle_read_header(const boost::system::error_code& error,
                                std::vector<uint8_t>& header_buffer) {
    if (error) {
        handle_error(error);
        return;
    }

    try {
        // Deserialize header
        auto header = MessageHeader::deserialize(header_buffer);
        
        // Prepare for payload
        auto payload_buffer = std::make_shared<std::vector<uint8_t>>(header.payload_size);
        
        // Start reading payload
        boost::asio::async_read(socket_,
            boost::asio::buffer(*payload_buffer),
            [self = shared_from_this(), header, payload_buffer](
                const boost::system::error_code& error,
                std::size_t /*bytes_transferred*/) {
                self->handle_read_payload(error, header, payload_buffer);
            });
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing message header: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::INVALID_MESSAGE);
        }
    }
}

void TcpPeer::handle_read_payload(const boost::system::error_code& error,
                                 const MessageHeader& header,
                                 std::shared_ptr<std::vector<uint8_t>> payload_buffer) {
    if (error) {
        handle_error(error);
        return;
    }

    try {
        // Create input stream from payload buffer
        auto encrypted_stream = std::make_shared<std::stringstream>();
        encrypted_stream->write(reinterpret_cast<const char*>(payload_buffer->data()),
                              payload_buffer->size());
        encrypted_stream->seekg(0);

        // Prepare decryption
        crypto_stream_->setMode(crypto::CryptoStream::Mode::Decrypt);
        crypto_stream_->initialize(std::vector<uint8_t>(header.file_key.begin(),
                                                      header.file_key.end()),
                                 std::vector<uint8_t>(header.iv.begin(),
                                                    header.iv.end()));

        // Decrypt payload
        auto decrypted_stream = std::make_shared<std::stringstream>();
        crypto_stream_->decrypt(*encrypted_stream, *decrypted_stream);
        decrypted_stream->seekg(0);

        // Notify message handler
        if (message_handler_) {
            message_handler_(header, decrypted_stream);
        }

        // Continue reading
        start_read_header();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing message payload: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::ENCRYPTION_ERROR);
        }
    }
}

void TcpPeer::start_write() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (message_queue_.empty() || writing_) {
        return;
    }

    writing_ = true;
    auto& message = message_queue_.front();

    try {
        // Prepare encryption
        crypto_stream_->setMode(crypto::CryptoStream::Mode::Encrypt);
        crypto_stream_->initialize(std::vector<uint8_t>(message.header.file_key.begin(),
                                                      message.header.file_key.end()),
                                 std::vector<uint8_t>(message.header.iv.begin(),
                                                    message.header.iv.end()));

        // Encrypt payload
        auto encrypted_stream = std::make_shared<std::stringstream>();
        crypto_stream_->encrypt(*message.payload, *encrypted_stream);
        encrypted_stream->seekg(0, std::ios::end);
        message.header.payload_size = encrypted_stream->tellg();
        encrypted_stream->seekg(0);

        // Serialize header
        auto header_data = message.header.serialize();
        
        // Prepare buffers for gathering write
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(header_data));
        
        // Get encrypted payload data
        auto payload_data = std::make_shared<std::vector<uint8_t>>(message.header.payload_size);
        encrypted_stream->read(reinterpret_cast<char*>(payload_data->data()),
                             message.header.payload_size);
        buffers.push_back(boost::asio::buffer(*payload_data));

        // Write message
        boost::asio::async_write(socket_,
            buffers,
            [self = shared_from_this(), payload_data](const boost::system::error_code& error,
                                                    std::size_t /*bytes_transferred*/) {
                self->handle_write(error);
            });
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error preparing message for sending: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::ENCRYPTION_ERROR);
        }
        writing_ = false;
        message_queue_.pop();
    }
}

void TcpPeer::handle_write(const boost::system::error_code& error) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (error) {
        handle_error(error);
        writing_ = false;
        return;
    }

    message_queue_.pop();
    writing_ = false;

    // Continue writing if there are more messages
    if (!message_queue_.empty()) {
        start_write();
    }
}

void TcpPeer::handle_error(const boost::system::error_code& error) {
    NetworkError net_error;
    
    if (error == boost::asio::error::eof ||
        error == boost::asio::error::connection_reset) {
        net_error = NetworkError::PEER_DISCONNECTED;
    } else if (error == boost::asio::error::connection_refused) {
        net_error = NetworkError::CONNECTION_FAILED;
    } else {
        net_error = NetworkError::UNKNOWN_ERROR;
    }

    BOOST_LOG_TRIVIAL(error) << "Network error: " << error.message();
    
    if (error_handler_) {
        error_handler_(net_error);
    }

    disconnect();
}

void TcpPeer::set_message_handler(message_handler handler) {
    message_handler_ = std::move(handler);
}

void TcpPeer::set_error_handler(error_handler handler) {
    error_handler_ = std::move(handler);
}

std::string TcpPeer::get_address() const {
    return endpoint_.address().to_string();
}

uint16_t TcpPeer::get_port() const {
    return endpoint_.port();
}

const std::array<uint8_t, 32>& TcpPeer::get_id() const {
    return peer_id_;
}

void TcpPeer::accept(boost::asio::ip::tcp::acceptor& acceptor,
                    const std::function<void(const boost::system::error_code&)>& handler) {
    acceptor.async_accept(socket_,
        [this, handler](const boost::system::error_code& error) {
            if (!error) {
                endpoint_ = socket_.remote_endpoint();
                connected_ = true;
                BOOST_LOG_TRIVIAL(info) << "Accepted connection from " 
                                      << endpoint_.address().to_string()
                                      << ":" << endpoint_.port();
            }
            handler(error);
        });
}

} // namespace dfs::network
