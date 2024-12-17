#include "../../include/network/peer_manager.hpp"
#include "../../include/network/tcp_peer.hpp"
#include <boost/log/trivial.hpp>

namespace dfs::network {

PeerManager::PeerManager(boost::asio::io_context& io_context)
    : io_context_(io_context) {}

void PeerManager::start_listening(uint16_t port) {
    try {
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context_,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)
        );
        start_accept();
        BOOST_LOG_TRIVIAL(info) << "Started listening on port " << port;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to start listening: " << e.what();
        throw;
    }
}

void PeerManager::stop_listening() {
    if (acceptor_) {
        boost::system::error_code ec;
        acceptor_->close(ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Error closing acceptor: " << ec.message();
        }
        acceptor_.reset();
    }

    // Disconnect all peers
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& peer_pair : peers_) {
        peer_pair.second->disconnect();
    }
    peers_.clear();
}

void PeerManager::add_peer(const std::string& address, uint16_t port) {
    BOOST_LOG_TRIVIAL(info) << "Adding new peer: " << address << ":" << port;
    
    try {
        // Check if peer already exists
        std::string peer_key = address + ":" + std::to_string(port);
        {
            std::lock_guard<std::mutex> lock(peers_mutex_);
            if (auto it = peers_.find(peer_key); it != peers_.end()) {
                if (it->second->is_connected()) {
                    BOOST_LOG_TRIVIAL(warning) << "Peer already connected: " << peer_key;
                    return;
                }
                // Remove disconnected peer before adding new one
                peers_.erase(it);
            }
        }
        
        // Create new peer
        auto peer = create_tcp_peer(io_context_, address, port);
        
        // Set up packet and error handlers
        peer->set_packet_handler(packet_handler_);
        peer->set_error_handler(
            [this, peer_key](NetworkError error) {
                BOOST_LOG_TRIVIAL(info) << "Peer error handler triggered for " << peer_key;
                handle_peer_error(peer_key, error);
            }
        );

        // Add to peers map
        {
            std::lock_guard<std::mutex> lock(peers_mutex_);
            peers_[peer_key] = peer;
            BOOST_LOG_TRIVIAL(debug) << "Added peer to map: " << peer_key;
        }

        // Connect to peer
        peer->connect();
        BOOST_LOG_TRIVIAL(info) << "Initiated connection to peer: " << peer_key;
        
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to add peer " << address << ":" << port
                               << " - " << e.what();
        throw;
    }
}

void PeerManager::remove_peer(const std::array<uint8_t, 32>& peer_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    for (auto it = peers_.begin(); it != peers_.end(); ) {
        if (it->second->get_id() == peer_id) {
            it->second->disconnect();
            it = peers_.erase(it);
            BOOST_LOG_TRIVIAL(info) << "Removed peer with ID: "
                                  << std::hex << peer_id[0] << peer_id[1];
        } else {
            ++it;
        }
    }
}

void PeerManager::broadcast_packet(PacketType type,
                                   std::shared_ptr<std::istream> payload,
                                   uint32_t sequence_number) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    BOOST_LOG_TRIVIAL(info) << "Broadcasting packet of type " 
                           << static_cast<int>(type) 
                           << " to " << peers_.size() << " peers";

    // Store initial position to restore later
    auto initial_pos = payload->tellg();
    
    for (const auto& [peer_key, peer] : peers_) {
        if (peer->is_connected()) {
            try {
                // Create a new stream for each peer to avoid sharing stream position
                auto payload_copy = std::make_shared<std::stringstream>();
                
                // Reset payload position for each copy
                payload->seekg(0);
                if (!payload->good()) {
                    throw std::runtime_error("Failed to seek payload stream");
                }
                
                *payload_copy << payload->rdbuf();
                if (!payload_copy->good()) {
                    throw std::runtime_error("Failed to copy payload stream");
                }
                
                payload_copy->seekg(0);
                if (!payload_copy->good()) {
                    throw std::runtime_error("Failed to seek payload copy stream");
                }
                
                BOOST_LOG_TRIVIAL(debug) << "Sending broadcast message to peer: " << peer_key;
                peer->send_packet(type, payload_copy, sequence_number);
                
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Failed to broadcast message to peer " 
                                       << peer_key << ": " << e.what();
            }
        }
    }
    
    // Restore original position
    payload->seekg(initial_pos);
    
    BOOST_LOG_TRIVIAL(debug) << "Broadcast complete";
}

std::shared_ptr<IPeer> PeerManager::get_peer(const std::array<uint8_t, 32>& peer_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    for (const auto& peer_pair : peers_) {
        if (peer_pair.second->get_id() == peer_id) {
            return peer_pair.second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<IPeer>> PeerManager::get_all_peers() {
    std::vector<std::shared_ptr<IPeer>> result;
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    result.reserve(peers_.size());
    for (const auto& peer_pair : peers_) {
        result.push_back(peer_pair.second);
    }
    return result;
}

void PeerManager::set_message_handler(message_handler handler) {
    BOOST_LOG_TRIVIAL(info) << "Setting new message handler for peer manager";
    message_handler_ = std::move(handler);
    
    // Update handler for all existing peers
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& [peer_key, peer] : peers_) {
        try {
            peer->set_message_handler(message_handler_);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Failed to set message handler for peer "
                                   << peer_key << ": " << e.what();
        }
    }
}

void PeerManager::start_accept() {
    if (!acceptor_) return;

    auto peer = create_tcp_peer(io_context_,
                              acceptor_->local_endpoint().address().to_string(),
                              acceptor_->local_endpoint().port(),
                              crypto_stream_);

    auto tcp_peer = std::dynamic_pointer_cast<TcpPeer>(peer);
    if (!tcp_peer) {
        BOOST_LOG_TRIVIAL(error) << "Failed to cast peer to TcpPeer";
        return;
    }
    
    tcp_peer->accept(*acceptor_,
        [this, peer](const boost::system::error_code& error) {
            handle_accept(peer, error);
        });
}

void PeerManager::handle_accept(std::shared_ptr<IPeer> peer,
                              const boost::system::error_code& error) {
    if (error) {
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
        return;
    }

    // Set up message and error handlers
    peer->set_message_handler(message_handler_);
    peer->set_error_handler(
        [this, peer](NetworkError error) {
            handle_peer_error(peer->get_address() + ":" + std::to_string(peer->get_port()), error);
        }
    );

    // Add to peers map
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        std::string peer_key = peer->get_address() + ":" + std::to_string(peer->get_port());
        peers_[peer_key] = peer;
    }

    BOOST_LOG_TRIVIAL(info) << "Accepted connection from "
                           << peer->get_address() << ":" << peer->get_port();

    // Start reading from the new peer
    peer->connect();

    // Continue accepting new connections
    start_accept();
}

void PeerManager::handle_peer_error(const std::string& peer_key, NetworkError error) {
    BOOST_LOG_TRIVIAL(info) << "Handling peer error for " << peer_key 
                           << ": " << to_string(error);

    switch (error) {
        case NetworkError::PEER_DISCONNECTED:
        case NetworkError::CONNECTION_FAILED:
            try {
                std::lock_guard<std::mutex> lock(peers_mutex_);
                if (auto it = peers_.find(peer_key); it != peers_.end()) {
                    it->second->disconnect();
                    peers_.erase(it);
                    BOOST_LOG_TRIVIAL(info) << "Removed disconnected peer: " << peer_key;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Error removing peer " << peer_key 
                                       << ": " << e.what();
            }
            break;
            
        case NetworkError::PROTOCOL_ERROR:
            BOOST_LOG_TRIVIAL(warning) << "Protocol error for peer " << peer_key 
                                      << ", attempting to re-establish connection";
            try {
                if (auto it = peers_.find(peer_key); it != peers_.end()) {
                    it->second->disconnect();
                    it->second->connect();
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Failed to recover from protocol error: " 
                                        << e.what();
                handle_peer_error(peer_key, NetworkError::CONNECTION_FAILED);
            }
            break;
            
        default:
            BOOST_LOG_TRIVIAL(warning) << "Unhandled peer error: " << to_string(error);
            break;
    }
}

} // namespace dfs::network