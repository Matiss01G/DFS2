#ifndef DFS_NETWORK_TCP_PEER_HPP
#define DFS_NETWORK_TCP_PEER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <system_error>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/log/trivial.hpp>
#include "peer.hpp"
#include "connection_state.hpp"

namespace dfs {
namespace network {

/**
 * TCP implementation of the Peer interface.
 * Provides stream-based communication over TCP/IP using boost::asio.
 * 
 * Features:
 * - Asynchronous I/O with boost::asio
 * - Stream-based data transfer
 * - Connection state management
 * - Error handling and recovery
 */
class TcpPeer final : public Peer {
public:
    explicit TcpPeer();
    ~TcpPeer() override;

    // Deleted copy operations to prevent accidental copies
    TcpPeer(const TcpPeer&) = delete;
    TcpPeer& operator=(const TcpPeer&) = delete;

    // Connection management - from Peer interface
    bool connect(const std::string& address, uint16_t port) override;
    bool disconnect() override;

    // Stream operations - from Peer interface
    std::ostream* get_output_stream() override;
    std::istream* get_input_stream() override;
    
    // Stream processing - from Peer interface
    void set_stream_processor(StreamProcessor processor) override;
    bool start_stream_processing() override;
    void stop_stream_processing() override;

    // State management - from Peer interface
    ConnectionState::State get_connection_state() const override;

private:
    // Internal processing
    void process_streams();
    void handle_error(const std::error_code& error);
    void close_socket();

    // Configuration
    static constexpr size_t BUFFER_SIZE = 8192;
    static constexpr std::chrono::seconds CONNECTION_TIMEOUT{30};
    static constexpr std::chrono::seconds SOCKET_TIMEOUT{5};
    static constexpr size_t MAX_BUFFER_SIZE = BUFFER_SIZE * 2;
    static constexpr int MAX_RETRY_ATTEMPTS = 3;

    // Network I/O components
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::streambuf> input_buffer_;
    std::unique_ptr<boost::asio::streambuf> output_buffer_;
    std::unique_ptr<std::istream> input_stream_;
    std::unique_ptr<std::ostream> output_stream_;
    
    // Stream processing state
    StreamProcessor stream_processor_;
    std::atomic<bool> processing_;
    std::unique_ptr<std::thread> processing_thread_;
    
    // Connection state
    std::atomic<ConnectionState::State> state_;
    
    // Peer endpoint information
    std::string peer_address_;
    uint16_t peer_port_;

    // Utility methods
    std::string get_peer_address() const { return peer_address_; }
    uint16_t get_peer_port() const { return peer_port_; }
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP
