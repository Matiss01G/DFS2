#pragma once

#include <string>
#include <vector>
#include <memory>
#include "network/tcp_server.hpp"
#include "network/peer_manager.hpp"
#include "network/channel.hpp"
#include "file_server/file_server.hpp"
#include "store/store.hpp"

namespace dfs {
namespace network {

class Bootstrap {
public:
    Bootstrap(const std::string& address, uint16_t port, 
             const std::vector<uint8_t>& key, uint8_t ID,
             const std::vector<std::string>& bootstrap_nodes);
    ~Bootstrap();

    bool start();
    bool connect_to_bootstrap_nodes();
    bool shutdown();

    // Add getter for peer manager
    PeerManager& get_peer_manager() { return *peer_manager_; }

    // Add getter for file server
    FileServer& get_file_server() { return *file_server_; }

private:
    std::string address_;
    uint16_t port_;
    std::vector<uint8_t> key_;
    uint8_t ID_;
    std::vector<std::string> bootstrap_nodes_;

    std::unique_ptr<Channel> channel_;
    std::unique_ptr<TCP_Server> tcp_server_;
    std::unique_ptr<PeerManager> peer_manager_;
    std::unique_ptr<FileServer> file_server_;
    std::unique_ptr<store::Store> store_; // Added store member
};

} // namespace network
} // namespace dfs