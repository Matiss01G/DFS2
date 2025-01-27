#include "network/bootstrap.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <thread>
#include <chrono>

namespace dfs {
namespace network {

Bootstrap::Bootstrap(const std::string& address, uint16_t port, 
                    const std::vector<uint8_t>& key, uint8_t ID,
                    const std::vector<std::string>& bootstrap_nodes)
    : address_(address)
    , port_(port)
    , key_(key)
    , ID_(ID)
    , bootstrap_nodes_(bootstrap_nodes)
    , should_shutdown_(false) {

    BOOST_LOG_TRIVIAL(info) << "Initializing Bootstrap with ID: " << static_cast<int>(ID);

    try {
        // Create channel first (no dependencies)
        channel_ = std::make_unique<Channel>();
        BOOST_LOG_TRIVIAL(debug) << "Bootstrap program: Channel created successfully";

        // Create TCP server without peer manager initially
        tcp_server_ = std::make_unique<TCP_Server>(port_, address_, ID_);
        BOOST_LOG_TRIVIAL(debug) << "Bootstrap program: TCP Server created successfully";

        // Create peer manager with channel and tcp_server
        peer_manager_ = std::make_unique<PeerManager>(*channel_, *tcp_server_, key_);
        BOOST_LOG_TRIVIAL(debug) << "Bootstrap program: Peer Manager created successfully";

        // Set peer manager in TCP server
        tcp_server_->set_peer_manager(*peer_manager_);

        // Create file server last as it depends on all other components
        file_server_ = std::make_unique<FileServer>(ID_, key_, *peer_manager_, *channel_);
        BOOST_LOG_TRIVIAL(debug) << "Bootstrap program: File Server created successfully";

        BOOST_LOG_TRIVIAL(info) << "Bootstrap program: Successfully created all components";
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Bootstrap program: Failed to initialize components: " << e.what();
        throw;
    }
}

bool Bootstrap::connect_to_bootstrap_nodes() {
    BOOST_LOG_TRIVIAL(info) << "Bootstrap program: Connecting to bootstrap nodes...";

    bool all_connected = true;

    for (const auto& node : bootstrap_nodes_) {
        try {
            // Parse address:port
            std::string address;
            uint16_t port;

            size_t delimiter_pos = node.find(':');
            if (delimiter_pos == std::string::npos) {
                BOOST_LOG_TRIVIAL(error) << "Bootstrap program: Invalid bootstrap node format: " << node;
                all_connected = false;
                continue;
            }

            address = node.substr(0, delimiter_pos);
            port = std::stoi(node.substr(delimiter_pos + 1));

            if (!tcp_server_->connect(address, port)) {
                all_connected = false;
                continue;
            }
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Bootstrap program: Error connecting to " << node << ": " << e.what();
            all_connected = false;
        }
    }

    return all_connected;
}

bool Bootstrap::start() {
    try {
        // Start TCP server
        if (!tcp_server_->start_listener()) {
            BOOST_LOG_TRIVIAL(error) << "Bootstrap program: Failed to start TCP server";
            return false;
        }

        // Connect to bootstrap nodes
        if (!bootstrap_nodes_.empty()) {
            if (!connect_to_bootstrap_nodes()) {
                BOOST_LOG_TRIVIAL(warning) << "Bootstrap program: Failed to connect to some bootstrap nodes";
                // Continue anyway as this might be expected in some cases
            }
        }

        BOOST_LOG_TRIVIAL(info) << "Bootstrap program: Bootstrap successfully started";

        // Keep running until shutdown is requested
        while (!should_shutdown_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Bootstrap program: Failed to start bootstrap: " << e.what();
        return false;
    }
}

void Bootstrap::shutdown() {
    BOOST_LOG_TRIVIAL(info) << "Bootstrap program: Initiating shutdown...";
    should_shutdown_ = true;

    if (tcp_server_) {
        tcp_server_->shutdown();
    }

    // Wait a bit for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    BOOST_LOG_TRIVIAL(info) << "Bootstrap program: Shutdown complete";
}

Bootstrap::~Bootstrap() {
    shutdown();
}

} // namespace network
} // namespace dfs