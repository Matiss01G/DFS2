#include "../../include/network/tcp_peer.hpp"
#include <boost/log/trivial.hpp>
#include <istream>
#include <sstream>
#include <random>
#include <algorithm>

namespace dfs::network {

TcpPeer::TcpPeer(boost::asio::io_context& io_context,
                 const std::string& address,
                 uint16_t port)
    : io_context_(io_context)
    , socket_(io_context)
    , endpoint_(boost::asio::ip::address::from_string(address), port) {
    // Generate random peer ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(1, 255); // Avoid all zeros
    std::generate(peer_id_.begin(), peer_id_.end(), [&]() { return dist(gen); });
    
    BOOST_LOG_TRIVIAL(debug) << "Created TCP peer with ID: " 
                          << std::hex << peer_id_[0] << peer_id_[1];
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
                
                // Send peer ID as initial handshake
                auto id_buffer = std::make_shared<std::vector<uint8_t>>(self->peer_id_.begin(), 
                                                                      self->peer_id_.end());
                boost::asio::async_write(self->socket_,
                    boost::asio::buffer(*id_buffer),
                    [self, id_buffer](const boost::system::error_code& write_error,
                                    std::size_t /*bytes_transferred*/) {
                        if (!write_error) {
                            if (self->error_handler_) {
                                self->error_handler_(NetworkError::SUCCESS);
                            }
                            self->start_read_header();
                        } else {
                            self->handle_error(write_error);
                        }
                    });
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

void TcpPeer::send_packet(PacketType type,
                         std::shared_ptr<std::istream> payload,
                         uint32_t sequence_number) {
    if (!connected_) {
        if (error_handler_) {
            error_handler_(NetworkError::PEER_DISCONNECTED);
        }
        return;
    }

    // Create packet header
    PacketHeader header;
    header.type = type;
    header.sequence_number = sequence_number;
    
    // Copy peer ID
    std::copy(peer_id_.begin(), peer_id_.end(), header.peer_id.begin());

    // Get payload size
    payload->seekg(0, std::ios::end);
    header.payload_size = payload->tellg();
    payload->seekg(0);

    // Queue the packet
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        packet_queue_.push({header, payload});
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
        auto header = PacketHeader::deserialize(header_buffer);
        
        // Validate payload size to prevent memory exhaustion
        if (header.payload_size > MAX_PAYLOAD_SIZE) {
            BOOST_LOG_TRIVIAL(error) << "Payload size exceeds maximum allowed: " 
                                   << header.payload_size << " > " << MAX_PAYLOAD_SIZE;
            if (error_handler_) {
                error_handler_(NetworkError::INVALID_PACKET);
            }
            return;
        }
        
        // Use chunked reading for large payloads
        constexpr size_t CHUNK_SIZE = 8192; // 8KB chunks
        auto payload_buffer = std::make_shared<std::vector<uint8_t>>();
        payload_buffer->reserve(std::min(header.payload_size, CHUNK_SIZE));
        
        // Start reading payload in chunks
        auto stream_buffer = std::make_shared<std::stringstream>();
        read_payload_chunk(header, stream_buffer, 0);
        
        BOOST_LOG_TRIVIAL(debug) << "Received packet with sequence number: " 
                               << header.sequence_number;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing packet header: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::INVALID_PACKET);
        }
    }
}

void TcpPeer::read_payload_chunk(const MessageHeader& header,
                                std::shared_ptr<std::stringstream> stream_buffer,
                                size_t bytes_read) {
    constexpr size_t CHUNK_SIZE = 8192;  // 8KB chunks for efficient streaming
    size_t remaining_bytes = header.payload_size - bytes_read;
    size_t chunk_size = std::min(CHUNK_SIZE, remaining_bytes);
    
    // Create chunk buffer with appropriate size
    auto chunk_buffer = std::make_shared<std::vector<uint8_t>>(chunk_size);
    
    BOOST_LOG_TRIVIAL(debug) << "Reading chunk: " << chunk_size << " bytes "
                           << "(total read: " << bytes_read << "/"
                           << header.payload_size << ")";
    
    boost::asio::async_read(socket_,
        boost::asio::buffer(*chunk_buffer),
        [self = shared_from_this(), header, stream_buffer, chunk_buffer, bytes_read]
        (const boost::system::error_code& error, size_t bytes_transferred) {
            if (error) {
                if (error == boost::asio::error::eof) {
                    BOOST_LOG_TRIVIAL(error) << "Stream ended unexpectedly while reading chunk";
                    if (self->error_handler_) {
                        self->error_handler_(NetworkError::PEER_DISCONNECTED);
                    }
                } else {
                    self->handle_error(error);
                }
                return;
            }
            
            try {
                // Append chunk to stream buffer
                stream_buffer->write(reinterpret_cast<char*>(chunk_buffer->data()),
                                  bytes_transferred);
                if (!stream_buffer->good()) {
                    throw std::runtime_error("Failed to write to stream buffer");
                }
                
                size_t total_bytes = bytes_read + bytes_transferred;
                BOOST_LOG_TRIVIAL(debug) << "Chunk received: " << bytes_transferred 
                                      << " bytes (total: " << total_bytes << "/"
                                      << header.payload_size << ")";
                
                if (total_bytes < header.payload_size) {
                    // Continue reading chunks
                    self->read_payload_chunk(header, stream_buffer, total_bytes);
                } else {
                    // All chunks received, process complete payload
                    stream_buffer->seekg(0);
                    if (!stream_buffer->good()) {
                        throw std::runtime_error("Failed to seek stream buffer");
                    }
                    
                    self->handle_read_payload(boost::system::error_code(), header,
                                          std::make_shared<std::vector<uint8_t>>(
                                              std::istreambuf_iterator<char>(*stream_buffer),
                                              std::istreambuf_iterator<char>()));
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Error processing chunk: " << e.what();
                if (self->error_handler_) {
                    self->error_handler_(NetworkError::UNKNOWN_ERROR);
                }
            }
        });
}

void TcpPeer::handle_read_payload(const boost::system::error_code& error,
                                 const PacketHeader& header,
                                 std::shared_ptr<std::vector<uint8_t>> payload_buffer) {
    if (error) {
        handle_error(error);
        return;
    }

    try {
        // Create input stream from payload buffer
        auto payload_stream = std::make_shared<std::stringstream>();
        payload_stream->write(reinterpret_cast<const char*>(payload_buffer->data()),
                            payload_buffer->size());
        payload_stream->seekg(0);

        // Notify packet handler
        if (packet_handler_) {
            packet_handler_(header, payload_stream);
        }

        // Continue reading
        start_read_header();
        
        BOOST_LOG_TRIVIAL(debug) << "Successfully processed packet with sequence number: "
                               << header.sequence_number;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing packet payload: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::PROTOCOL_ERROR);
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
    
    BOOST_LOG_TRIVIAL(debug) << "Starting write operation for message type: " 
                           << static_cast<int>(message.header.type);

    try {
        // Prepare encryption with stream handling
        auto encrypted_stream = std::make_shared<std::stringstream>();
        {
            crypto_stream_->setMode(crypto::CryptoStream::Mode::Encrypt);
            crypto_stream_->initialize(std::vector<uint8_t>(message.header.file_key.begin(),
                                                          message.header.file_key.end()),
                                     std::vector<uint8_t>(message.header.iv.begin(),
                                                         message.header.iv.end()));

            try {
                // Stream encryption with progress logging
                BOOST_LOG_TRIVIAL(debug) << "Starting stream encryption";
                *crypto_stream_ << *message.payload >> *encrypted_stream;
                if (!encrypted_stream->good()) {
                    throw std::runtime_error("Encryption stream write failed");
                }
                BOOST_LOG_TRIVIAL(debug) << "Stream encryption completed";
            } catch (const crypto::EncryptionError& e) {
                BOOST_LOG_TRIVIAL(error) << "Encryption error: " << e.what();
                throw;
            }
        }
        
        // Update payload size after encryption
        encrypted_stream->seekg(0, std::ios::end);
        auto encrypted_size = encrypted_stream->tellg();
        if (encrypted_size == -1) {
            throw std::runtime_error("Failed to determine encrypted payload size");
        }
        message.header.payload_size = static_cast<uint64_t>(encrypted_size);
        encrypted_stream->seekg(0);

        if (message.header.payload_size > MAX_PAYLOAD_SIZE) {
            throw std::runtime_error("Encrypted payload size exceeds maximum allowed size");
        }

        // Serialize header
        auto header_data = message.header.serialize();
        
        // Prepare buffers for gathering write
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(header_data));
        
        // Get encrypted payload data with validation
        auto payload_data = std::make_shared<std::vector<uint8_t>>(message.header.payload_size);
        if (!encrypted_stream->read(reinterpret_cast<char*>(payload_data->data()),
                                  message.header.payload_size)) {
            throw std::runtime_error("Failed to read encrypted payload data");
        }
        buffers.push_back(boost::asio::buffer(*payload_data));

        BOOST_LOG_TRIVIAL(debug) << "Sending message: size=" << message.header.payload_size 
                               << " bytes";

        // Write message with detailed error handling
        boost::asio::async_write(socket_,
            buffers,
            [self = shared_from_this(), payload_data](const boost::system::error_code& error,
                                                    std::size_t bytes_transferred) {
                if (!error) {
                    BOOST_LOG_TRIVIAL(debug) << "Successfully wrote " << bytes_transferred 
                                          << " bytes";
                } else if (error == boost::asio::error::broken_pipe ||
                          error == boost::asio::error::connection_reset) {
                    BOOST_LOG_TRIVIAL(error) << "Connection lost while writing: " 
                                          << error.message();
                    if (self->error_handler_) {
                        self->error_handler_(NetworkError::PEER_DISCONNECTED);
                    }
                } else {
                    BOOST_LOG_TRIVIAL(error) << "Write error: " << error.message();
                }
                self->handle_write(error);
            });
    } catch (const crypto::EncryptionError& e) {
        BOOST_LOG_TRIVIAL(error) << "Encryption error while preparing message: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::ENCRYPTION_ERROR);
        }
        writing_ = false;
        message_queue_.pop();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error preparing message for sending: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::UNKNOWN_ERROR);
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
    
    if (error == boost::asio::error::operation_aborted) {
        // Ignore planned disconnections
        return;
    }
    
    if (error == boost::asio::error::eof ||
        error == boost::asio::error::connection_reset ||
        error == boost::asio::error::connection_aborted ||
        error == boost::asio::error::broken_pipe) {
        net_error = NetworkError::PEER_DISCONNECTED;
        BOOST_LOG_TRIVIAL(warning) << "Peer disconnected: " << error.message();
    } else if (error == boost::asio::error::connection_refused ||
               error == boost::asio::error::host_unreachable ||
               error == boost::asio::error::timed_out) {
        net_error = NetworkError::CONNECTION_FAILED;
        BOOST_LOG_TRIVIAL(error) << "Connection failed: " << error.message();
    } else if (error == boost::asio::error::no_buffer_space ||
               error == boost::asio::error::no_memory) {
        net_error = NetworkError::UNKNOWN_ERROR;
        BOOST_LOG_TRIVIAL(error) << "Resource error: " << error.message();
    } else {
        net_error = NetworkError::UNKNOWN_ERROR;
        BOOST_LOG_TRIVIAL(error) << "Unknown network error: " << error.message();
    }
    
    // Ensure we're disconnected before notifying error handler
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        writing_ = false;
        while (!message_queue_.empty()) {
            message_queue_.pop();
        }
        
        if (connected_) {
            boost::system::error_code ec;
            socket_.close(ec);
            connected_ = false;
        }
    }
    
    // Notify error handler if set
    if (error_handler_) {
        io_context_.post([this, net_error]() {
            error_handler_(net_error);
        });
    }
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
                
                // Generate a new peer ID if not already set
                if (std::all_of(peer_id_.begin(), peer_id_.end(), [](uint8_t b) { return b == 0; })) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<uint8_t> dist(1, 255);  // Avoid all zeros
                    std::generate(peer_id_.begin(), peer_id_.end(), [&]() { return dist(gen); });
                }
                
                BOOST_LOG_TRIVIAL(info) << "Accepted connection from " 
                                      << endpoint_.address().to_string()
                                      << ":" << endpoint_.port();
                
                if (error_handler_) {
                    error_handler_(NetworkError::SUCCESS);
                }
            }
            handler(error);
        });
}

} // namespace dfs::network