#ifndef DFS_NETWORK_TCP_PEER_HPP
#define DFS_NETWORK_TCP_PEER_HPP

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include "peer.hpp"

namespace dfs {
namespace network {

// Forward declarations
class PeerManager;

/**
 * @brief TCP implementation of the Peer interface
 * 
 * This class provides TCP/IP-based network communication with stream support.
 * It implements an asynchronous event-driven approach for handling network I/O,
 * utilizing boost::asio for efficient, non-blocking operations.
 * 
 * Note: While connect(), disconnect(), and is_connected() are part of the public
 * interface to satisfy the Peer contract, these operations should be performed
 * through the PeerManager for proper connection management and thread safety.
 * 
 * Key features:
 * - Asynchronous I/O operations for improved performance
 * - Automatic message framing with newline delimiters
 * - Thread-safe stream processing
 * - Comprehensive error handling and logging
 * - Support for custom stream processors
 */
class TCP_Peer : public Peer,
                 public std::enable_shared_from_this<TCP_Peer> {
public:
    explicit TCP_Peer(const std::string& peer_id);
    ~TCP_Peer() override;

    // Delete copy operations to prevent socket duplication
    TCP_Peer(const TCP_Peer&) = delete;
    TCP_Peer& operator=(const TCP_Peer&) = delete;

    // Connection management - implementations delegate to PeerManager
    bool connect(const std::string& address, uint16_t port) override;
    bool disconnect() override;
    bool is_connected() const override;

    // Stream operations
    std::istream* get_input_stream() override;
    bool send_message(const std::string& message) override;
    bool send_stream(std::istream& input_stream, std::size_t buffer_size = 8192);

    // Stream processing
    void set_stream_processor(StreamProcessor processor) override;
    bool start_stream_processing() override;
    void stop_stream_processing() override;

    // Getter
    const std::string& get_peer_id() const;

    // Allow PeerManager to access implementation details
    friend class PeerManager;

private:
    // Core attributes
    std::string peer_id_;
    StreamProcessor stream_processor_;

    // Network components
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::ip::tcp::endpoint> endpoint_;
    std::mutex io_mutex_;  // Mutex for synchronizing I/O operations

    // Processing thread
    std::unique_ptr<std::thread> processing_thread_;
    std::atomic<bool> processing_active_{false};

    // Stream buffers
    std::unique_ptr<boost::asio::streambuf> input_buffer_;
    std::unique_ptr<std::istream> input_stream_;

    // Internal methods
    void initialize_streams();
    void cleanup_connection();
    void process_stream();
    void async_read_next();

    // Implementation methods for connection management
    bool connect_impl(const std::string& address, uint16_t port);
    bool disconnect_impl();
    bool is_connected_impl() const;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP