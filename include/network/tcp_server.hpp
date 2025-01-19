#pragma once

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
#include "network/peer_manager.hpp"
#include "network/channel.hpp"

namespace dfs {
namespace network {

class TCP_Server {
public:
    /**
     * @brief Constructs a TCP server instance
     * @param port The port number to listen on
     * @param address The IP address to bind to
     * @param peer_manager Reference to PeerManager instance
     * @param channel Reference to Channel instance
     */
    TCP_Server(uint16_t port, 
              const std::string& address,
              PeerManager& peer_manager,
              Channel& channel);

    /**
     * @brief Starts the server listener
     * @return true if successfully started, false otherwise
     */
    bool start_listener();

    /**
     * @brief Shuts down the server and performs cleanup
     */
    void shutdown();

    /**
     * @brief Destructor ensuring proper cleanup
     */
    ~TCP_Server();

private:
    void handle_accept(const boost::system::error_code& error,
                      std::shared_ptr<boost::asio::ip::tcp::socket> socket);

    void start_accept();

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    PeerManager& peer_manager_;
    Channel& channel_;
    bool is_running_;
    std::unique_ptr<std::thread> io_thread_;
    const uint16_t port_;
    const std::string address_;
};

} // namespace network
} // namespace dfs
