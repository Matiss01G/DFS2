#ifndef DFS_NETWORK_MANAGER_HPP
#define DFS_NETWORK_MANAGER_HPP

#include <memory>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include "peer_manager.hpp"
#include "types.hpp"

namespace dfs::network {

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // Initialization and shutdown
    void start(uint16_t listen_port);
    void stop();
    
    // High-level network operations     
    void connect_to_peer(const std::string& address, uint16_t port);
    void send_file(const std::array<uint8_t, 32>& peer_id,
                  std::shared_ptr<std::istream> file_stream,
                  const std::vector<uint8_t>& file_key);
    void request_file(const std::array<uint8_t, 32>& peer_id,
                     const std::string& file_id);
    void broadcast_peer_list();
    
    // Event handlers
    using file_handler = std::function<void(const std::array<uint8_t, 32>&,
                                          const std::vector<uint8_t>&,
                                          std::shared_ptr<std::istream>)>;
    using error_handler = std::function<void(NetworkError)>;
    
    void set_file_received_handler(file_handler handler);
    void set_file_requested_handler(std::function<void(const std::string&)> handler);
    void set_error_handler(error_handler handler);
    
private:
    // Asio components
    boost::asio::io_context io_context_;
    std::unique_ptr<std::thread> io_thread_;
    
    // Core components
    std::shared_ptr<crypto::CryptoStream> crypto_stream_;
    std::unique_ptr<PeerManager> peer_manager_;
    
    // Event handlers
    file_handler file_received_handler_;
    std::function<void(const std::string&)> file_requested_handler_;
    error_handler error_handler_;
    
    // Message handling
    void handle_message(const MessageHeader& header,
                       std::shared_ptr<std::istream> payload);
    
    // Internal state
    bool running_{false};
};

} // namespace dfs::network

#endif // DFS_NETWORK_MANAGER_HPP
