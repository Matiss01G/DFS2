#pragma once

#include <string>
#include <vector>
#include <memory>
#include "network/tcp_server.hpp"
#include "network/peer_manager.hpp"
#include "network/channel.hpp"
#include "file_server/file_server.hpp"
#include "cli/cli.hpp"

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

    // Add getters
    PeerManager& get_peer_manager() { return *peer_manager_; }
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
};

} // namespace network
} // namespace dfs