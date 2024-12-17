#include "../../include/network/peer_manager.hpp"
#include "../../include/network/tcp_peer.hpp"
#include <boost/log/trivial.hpp>

namespace dfs::network {

PeerManager::PeerManager(boost::asio::io_context& io_context,
                        std::shared_ptr<crypto::CryptoStream> crypto_stream)
    : io_context_(io_context)
    , crypto_stream_(crypto_stream) {}

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
    auto peer = create_tcp_peer(io_context_, address, port, crypto_stream_);
    
    // Set up message and error handlers
    peer->set_message_handler(message_handler_);
    peer->set_error_handler(
        [this, peer](NetworkError error) {
            handle_peer_error(peer->get_id(), error);
        }
    );

    // Add to peers map
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        std::string peer_key = address + ":" + std::to_string(port);
        peers_[peer_key] = peer;
    }

    // Connect to peer
    peer->connect();
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

void PeerManager::broadcast_message(MessageType type,
                                  std::shared_ptr<std::istream> payload,
                                  const std::vector<uint8_t>& file_key) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    
    for (const auto& peer_pair : peers_) {
        if (peer_pair.second->is_connected()) {
            // Create a new stream for each peer to avoid sharing stream position
            auto payload_copy = std::make_shared<std::stringstream>();
            *payload_copy << payload->rdbuf();
            payload_copy->seekg(0);
            
            peer_pair.second->send_message(type, payload_copy, file_key);
        }
    }
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
    message_handler_ = std::move(handler);
    
    // Update handler for existing peers
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& peer_pair : peers_) {
        peer_pair.second->set_message_handler(message_handler_);
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
            handle_peer_error(peer->get_id(), error);
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

void PeerManager::handle_peer_error(const std::array<uint8_t, 32>& peer_id,
                                  NetworkError error) {
    if (error == NetworkError::PEER_DISCONNECTED ||
        error == NetworkError::CONNECTION_FAILED) {
        remove_peer(peer_id);
    }
}

} // namespace dfs::network
