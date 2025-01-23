#include "network/bootstrap.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>

namespace dfs {
namespace network {

Bootstrap::Bootstrap(const std::string& address, uint16_t port, 
                    const std::vector<uint8_t>& key, uint8_t ID,
                    const std::vector<std::string>& bootstrap_nodes)
    : address_(address)
    , port_(port)
    , key_(key)
    , ID_(ID)
    , bootstrap_nodes_(bootstrap_nodes) {

    BOOST_LOG_TRIVIAL(info) << "Initializing Bootstrap with ID: " << static_cast<int>(ID);

    try {
        // Create channel first (no dependencies)
        channel_ = std::make_unique<Channel>();
        BOOST_LOG_TRIVIAL(debug) << "Channel created successfully";

        // Create TCP server without peer manager initially
        tcp_server_ = std::make_unique<TCP_Server>(port_, address_, ID_);
        BOOST_LOG_TRIVIAL(debug) << "TCP Server created successfully";

        // Create peer manager with channel and tcp_server
        peer_manager_ = std::make_unique<PeerManager>(*channel_, *tcp_server_, key_);
        BOOST_LOG_TRIVIAL(debug) << "Peer Manager created successfully";

        // Set peer manager in TCP server
        tcp_server_->set_peer_manager(*peer_manager_);

        // Create file server last as it depends on all other components
        file_server_ = std::make_unique<FileServer>(ID_, key_, *peer_manager_, *channel_);
        BOOST_LOG_TRIVIAL(debug) << "File Server created successfully";

        BOOST_LOG_TRIVIAL(info) << "Successfully created all components";
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to initialize components: " << e.what();
        throw;
    }
}

bool Bootstrap::connect_to_bootstrap_nodes() {
    BOOST_LOG_TRIVIAL(info) << "Connecting to bootstrap nodes...";

    bool all_connected = true;

    for (const auto& node : bootstrap_nodes_) {
        try {
            // Parse address:port
            std::string address;
            uint16_t port;

            size_t delimiter_pos = node.find(':');
            if (delimiter_pos == std::string::npos) {
                BOOST_LOG_TRIVIAL(error) << "Invalid bootstrap node format: " << node;
                all_connected = false;
                continue;
            }

            address = node.substr(0, delimiter_pos);
            port = std::stoi(node.substr(delimiter_pos + 1));

            BOOST_LOG_TRIVIAL(info) << "Attempting to connect to " << address << ":" << port;

            if (!tcp_server_->connect(address, port)) {
                BOOST_LOG_TRIVIAL(error) << "Failed to connect to " << node;
                all_connected = false;
                continue;
            }

            BOOST_LOG_TRIVIAL(info) << "Successfully connected to " << node;
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error connecting to " << node << ": " << e.what();
            all_connected = false;
        }
    }

    return all_connected;
}

bool Bootstrap::start() {
    try {
        // Start TCP server
        if (!tcp_server_->start_listener()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to start TCP server";
            return false;
        }

        // Connect to bootstrap nodes
        if (!bootstrap_nodes_.empty()) {
            if (!connect_to_bootstrap_nodes()) {
                BOOST_LOG_TRIVIAL(warning) << "Failed to connect to some bootstrap nodes";
                // Continue anyway as this might be expected in some cases
            }
        }

        BOOST_LOG_TRIVIAL(info) << "Bootstrap successfully started";
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to start bootstrap: " << e.what();
        return false;
    }
}

Bootstrap::~Bootstrap() {
    try {
        if (tcp_server_) {
            tcp_server_->shutdown();
        }
        BOOST_LOG_TRIVIAL(info) << "Bootstrap shutdown complete";
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error during bootstrap shutdown: " << e.what();
    }
}

} // namespace network
} // namespace dfs