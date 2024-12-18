#include "network/network_manager.hpp"
#include <boost/log/trivial.hpp>

namespace dfs {
namespace network {

NetworkManager::NetworkManager(uint16_t port)
    : port_(port)
    , running_(false)
    , acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    BOOST_LOG_TRIVIAL(info) << "Network manager created on port " << port;
}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::start() {
    if (running_) {
        return;
    }

    running_ = true;
    acceptor_.listen();
    accept_thread_ = std::make_unique<std::thread>([this]() { accept_connections(); });
    BOOST_LOG_TRIVIAL(info) << "Network manager started on port " << port_;
}

void NetworkManager::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    
    // Stop acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Error closing acceptor: " << ec.message();
    }

    // Stop IO context
    io_context_.stop();

    // Wait for accept thread
    if (accept_thread_ && accept_thread_->joinable()) {
        accept_thread_->join();
    }

    // Disconnect all peers
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& peer_pair : peers_) {
        peer_pair.second->disconnect();
    }
    peers_.clear();

    BOOST_LOG_TRIVIAL(info) << "Network manager stopped";
}

bool NetworkManager::add_peer(const std::string& peer_address) {
    BOOST_LOG_TRIVIAL(info) << "Adding new peer: " << peer_address;
    
    std::size_t colon_pos = peer_address.find(':');
    if (colon_pos == std::string::npos) {
        handle_error(peer_address, NetworkError::INVALID_PEER);
        return false;
    }

    std::string address = peer_address.substr(0, colon_pos);
    uint16_t port;
    try {
        port = static_cast<uint16_t>(std::stoi(peer_address.substr(colon_pos + 1)));
    } catch (const std::exception&) {
        handle_error(peer_address, NetworkError::INVALID_PEER);
        return false;
    }

    auto peer = std::make_shared<TcpPeer>();
    BOOST_LOG_TRIVIAL(debug) << "Creating TCP peer for " << peer_address;
    
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        std::string key = get_peer_key(address, port);
        peers_[key] = peer;
        BOOST_LOG_TRIVIAL(debug) << "Added peer to map: " << key;
    }

    BOOST_LOG_TRIVIAL(info) << "Initiated connection to peer: " << peer_address;
    if (!peer->connect(address, port)) {
        handle_error(peer_address, NetworkError::CONNECTION_FAILED);
        return false;
    }

    if (connection_callback_) {
        connection_callback_(peer_address, true);
    }

    return true;
}

bool NetworkManager::remove_peer(const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_address);
    if (it != peers_.end()) {
        it->second->disconnect();
        peers_.erase(it);
        BOOST_LOG_TRIVIAL(info) << "Removed disconnected peer: " << peer_address;
        return true;
    }
    return false;
}

bool NetworkManager::is_peer_connected(const std::string& peer_address) const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_address);
    return it != peers_.end() && 
           it->second->get_connection_state() == ConnectionState::State::CONNECTED;
}

bool NetworkManager::send_to_peer(const std::string& peer_address, const std::string& data) {
    BOOST_LOG_TRIVIAL(info) << "Initiating file transfer to peer: " << peer_address;
    
    std::shared_ptr<TcpPeer> peer;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto it = peers_.find(peer_address);
        if (it == peers_.end()) {
            BOOST_LOG_TRIVIAL(error) << "Peer not found";
            handle_error(peer_address, NetworkError::INVALID_PEER);
            return false;
        }
        peer = it->second;
    }

    if (!peer || peer->get_connection_state() != ConnectionState::State::CONNECTED) {
        handle_error(peer_address, NetworkError::CONNECTION_LOST);
        return false;
    }

    auto* output_stream = peer->get_output_stream();
    if (!output_stream) {
        handle_error(peer_address, NetworkError::TRANSFER_FAILED);
        return false;
    }

    try {
        *output_stream << data;
        output_stream->flush();
        
        if (transfer_callback_) {
            transfer_callback_(peer_address, true);
        }
        return true;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Transfer error: " << e.what();
        handle_error(peer_address, NetworkError::TRANSFER_FAILED);
        return false;
    }
}

void NetworkManager::accept_connections() {
    while (running_) {
        auto peer = std::make_shared<TcpPeer>();
        
        acceptor_.async_accept(
            *peer->get_socket(),
            [this, peer](const boost::system::error_code& error) {
                handle_accept(error, peer);
            }
        );

        io_context_.run();
    }
}

void NetworkManager::handle_accept(const boost::system::error_code& error, std::shared_ptr<TcpPeer> peer) {
    if (!running_) {
        return;
    }

    if (error) {
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << error.message();
        return;
    }

    auto endpoint = peer->get_socket()->remote_endpoint();
    std::string peer_address = endpoint.address().to_string();
    uint16_t peer_port = endpoint.port();
    
    BOOST_LOG_TRIVIAL(info) << "Accepted connection from " << peer_address << ":" << peer_port;
    
    std::string key = get_peer_key(peer_address, peer_port);
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_[key] = peer;
    }

    if (connection_callback_) {
        connection_callback_(key, true);
    }
}

void NetworkManager::handle_error(const std::string& peer_address, NetworkError error) {
    BOOST_LOG_TRIVIAL(info) << "Handling peer error for " << peer_address << ": " 
                           << network_error_to_string(error);
    
    if (error_callback_) {
        error_callback_(peer_address, error);
    }

    if (error != NetworkError::SUCCESS) {
        remove_peer(peer_address);
    }
}

std::string NetworkManager::get_peer_key(const std::string& address, uint16_t port) const {
    return address + ":" + std::to_string(port);
}

} // namespace network
} // namespace dfs
