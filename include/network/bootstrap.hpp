#ifndef DFS_NETWORK_BOOTSTRAP_HPP
#define DFS_NETWORK_BOOTSTRAP_HPP

#include "network/channel.hpp"
#include "network/tcp_server.hpp"
#include "network/peer_manager.hpp"
#include "file_server/file_server.hpp"
#include <string>
#include <vector>
#include <memory>

namespace dfs {
namespace network {

class Bootstrap {
public:
    /**
     * @brief Constructs a new Bootstrap instance
     * 
     * @param address IP address to bind to
     * @param port Port to listen on
     * @param key Cryptographic key for secure communication
     * @param ID Unique identifier for this node
     * @param bootstrap_nodes List of bootstrap nodes to connect to in format "IP:port"
     */
    Bootstrap(const std::string& address, uint16_t port,
             const std::vector<uint8_t>& key, uint8_t ID,
             const std::vector<std::string>& bootstrap_nodes);
    
    /**
     * @brief Starts the bootstrap process
     * 
     * @return true if startup successful
     * @return false if startup failed
     */
    bool start();
    
    /**
     * @brief Destructor ensures proper cleanup
     */
    ~Bootstrap();
    
private:
    /**
     * @brief Connects to all configured bootstrap nodes
     * 
     * @return true if all connections successful
     * @return false if any connection failed
     */
    bool connect_to_bootstrap_nodes();
    
    // Configuration
    std::string address_;
    uint16_t port_;
    std::vector<uint8_t> key_;
    uint8_t ID_;
    std::vector<std::string> bootstrap_nodes_;
    
    // Components
    std::unique_ptr<Channel> channel_;
    std::unique_ptr<PeerManager> peer_manager_;
    std::unique_ptr<TCP_Server> tcp_server_;
    std::unique_ptr<FileServer> file_server_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_BOOTSTRAP_HPP
