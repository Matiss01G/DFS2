#include "../../include/network/network_manager.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>

namespace dfs::network {

NetworkManager::NetworkManager()
    : crypto_stream_(std::make_shared<crypto::CryptoStream>())
    , peer_manager_(std::make_unique<PeerManager>(io_context_, crypto_stream_)) {
}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::start(uint16_t listen_port) {
    if (running_) return;
    
    try {
        // Start listening for incoming connections
        peer_manager_->start_listening(listen_port);
        
        // Set up the message handler
        peer_manager_->set_message_handler(
            [this](const MessageHeader& header, std::shared_ptr<std::istream> payload) {
                handle_message(header, payload);
            }
        );
        
        // Start the IO context in a separate thread
        running_ = true;
        io_thread_ = std::make_unique<std::thread>(
            [this]() {
                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "IO context error: " << e.what();
                }
            }
        );
        
        BOOST_LOG_TRIVIAL(info) << "Network manager started on port " << listen_port;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to start network manager: " << e.what();
        stop();
        throw;
    }
}

void NetworkManager::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Stop accepting new connections and disconnect peers
    peer_manager_->stop_listening();
    
    // Stop the IO context and wait for the thread to finish
    io_context_.stop();
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
    
    BOOST_LOG_TRIVIAL(info) << "Network manager stopped";
}

void NetworkManager::connect_to_peer(const std::string& address, uint16_t port) {
    if (!running_) {
        throw std::runtime_error("Network manager not started");
    }
    
    try {
        peer_manager_->add_peer(address, port);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to connect to peer: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::CONNECTION_FAILED);
        }
    }
}

void NetworkManager::send_file(const std::array<uint8_t, 32>& peer_id,
                             std::shared_ptr<std::istream> file_stream,
                             const std::vector<uint8_t>& file_key) {
    if (!running_) {
        throw std::runtime_error("Network manager not started");
    }
    
    auto peer = peer_manager_->get_peer(peer_id);
    if (!peer) {
        if (error_handler_) {
            error_handler_(NetworkError::PEER_DISCONNECTED);
        }
        return;
    }
    
    try {
        peer->send_message(MessageType::FILE_TRANSFER, file_stream, file_key);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to send file: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::ENCRYPTION_ERROR);
        }
    }
}

void NetworkManager::request_file(const std::array<uint8_t, 32>& peer_id,
                                const std::string& file_id) {
    if (!running_) {
        throw std::runtime_error("Network manager not started");
    }
    
    auto peer = peer_manager_->get_peer(peer_id);
    if (!peer) {
        if (error_handler_) {
            error_handler_(NetworkError::PEER_DISCONNECTED);
        }
        return;
    }
    
    try {
        // Create a stream containing the file ID
        auto request_stream = std::make_shared<std::stringstream>();
        *request_stream << file_id;
        
        peer->send_message(MessageType::FILE_REQUEST, request_stream);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to request file: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::UNKNOWN_ERROR);
        }
    }
}

void NetworkManager::broadcast_peer_list() {
    if (!running_) {
        throw std::runtime_error("Network manager not started");
    }
    
    try {
        auto peers = peer_manager_->get_all_peers();
        auto peer_list_stream = std::make_shared<std::stringstream>();
        
        // Serialize peer list (simple format: address:port\n)
        for (const auto& peer : peers) {
            *peer_list_stream << peer->get_address() << ":" 
                            << peer->get_port() << "\n";
        }
        
        peer_manager_->broadcast_message(MessageType::PEER_LIST, peer_list_stream);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to broadcast peer list: " << e.what();
        if (error_handler_) {
            error_handler_(NetworkError::UNKNOWN_ERROR);
        }
    }
}

void NetworkManager::handle_message(const MessageHeader& header,
                                  std::shared_ptr<std::istream> payload) {
    switch (header.type) {
        case MessageType::FILE_TRANSFER:
            if (file_received_handler_) {
                file_received_handler_(header.source_id,
                                     std::vector<uint8_t>(header.file_key.begin(),
                                                        header.file_key.end()),
                                     payload);
            }
            break;
            
        case MessageType::FILE_REQUEST:
            if (file_requested_handler_) {
                std::string file_id;
                std::getline(*payload, file_id);
                file_requested_handler_(file_id);
            }
            break;
            
        case MessageType::PEER_LIST:
            // Handle peer list updates if needed
            break;
            
        default:
            BOOST_LOG_TRIVIAL(warning) << "Received unknown message type: "
                                     << static_cast<int>(header.type);
            break;
    }
}

void NetworkManager::set_file_received_handler(file_handler handler) {
    file_received_handler_ = std::move(handler);
}

void NetworkManager::set_file_requested_handler(
    std::function<void(const std::string&)> handler) {
    file_requested_handler_ = std::move(handler);
}

void NetworkManager::set_error_handler(error_handler handler) {
    error_handler_ = std::move(handler);
}

} // namespace dfs::network
