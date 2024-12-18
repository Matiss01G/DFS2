#ifndef DFS_NETWORK_MANAGER_HPP
#define DFS_NETWORK_MANAGER_HPP

#include <string>
#include <memory>
#include <map>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include "network_error.hpp"
#include "tcp_peer.hpp"

namespace dfs {
namespace network {

class NetworkManager {
public:
    using ConnectionCallback = std::function<void(const std::string&, bool)>;
    using TransferCallback = std::function<void(const std::string&, bool)>;
    using ErrorCallback = std::function<void(const std::string&, NetworkError)>;

    explicit NetworkManager(uint16_t port);
    ~NetworkManager();

    // Delete copy operations
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // Core operations
    void start();
    void stop();

    // Peer management
    bool add_peer(const std::string& peer_address);
    bool remove_peer(const std::string& peer_address);
    bool is_peer_connected(const std::string& peer_address) const;
    
    // Data transfer
    bool send_to_peer(const std::string& peer_address, const std::string& data);

    // Callback setters
    void set_connection_callback(ConnectionCallback callback) { connection_callback_ = std::move(callback); }
    void set_transfer_callback(TransferCallback callback) { transfer_callback_ = std::move(callback); }
    void set_error_callback(ErrorCallback callback) { error_callback_ = std::move(callback); }

private:
    void accept_connections();
    void handle_accept(const boost::system::error_code& error, std::shared_ptr<TcpPeer> peer);
    void handle_error(const std::string& peer_address, NetworkError error);
    std::string get_peer_key(const std::string& address, uint16_t port) const;

    uint16_t port_;
    bool running_;
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unique_ptr<std::thread> accept_thread_;
    std::map<std::string, std::shared_ptr<TcpPeer>> peers_;
    mutable std::mutex peers_mutex_;

    ConnectionCallback connection_callback_;
    TransferCallback transfer_callback_;
    ErrorCallback error_callback_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_MANAGER_HPP
