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

/**
 * @brief TCP implementation of the Peer interface
 * 
 * This class provides TCP/IP-based network communication with stream support.
 * It implements an asynchronous event-driven approach for handling network I/O,
 * utilizing boost::asio for efficient, non-blocking operations.
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
    // Constructs TCP peer with unique identifier
    explicit TCP_Peer(const std::string& peer_id);
    // Ensures proper cleanup of resources
    ~TCP_Peer() override;

    // Prevent copying to maintain resource safety
    TCP_Peer(const TCP_Peer&) = delete;
    TCP_Peer& operator=(const TCP_Peer&) = delete;

    // Establishes TCP connection to specified endpoint
    bool connect(const std::string& address, uint16_t port) override;
    // Closes connection and cleans up resources
    bool disconnect() override;
    // Checks if socket is currently connected
    bool is_connected() const override;

    // Returns pointer to input stream for reading data
    std::istream* get_input_stream() override;
    // Sends string message through TCP connection
    bool send_message(const std::string& message) override;
    // Sets callback for processing incoming stream data
    void set_stream_processor(StreamProcessor processor) override;
    // Begins asynchronous data processing
    bool start_stream_processing() override;
    // Stops asynchronous data processing
    void stop_stream_processing() override;
    // Returns unique identifier of this peer
    const std::string& get_peer_id() const { return peer_id_; }
    // Initiates asynchronous write operation
    void async_write();
    // Transfers data from input stream through socket
    bool send_stream(std::istream& input_stream, std::size_t buffer_size = 8192);
    // Sets up next asynchronous read operation
    void async_read_next();

private:
    std::string peer_id_;                    // Unique identifier for this peer
    StreamProcessor stream_processor_;        // Callback for processing incoming data

    boost::asio::io_context io_context_;     // Manages async I/O operations
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;    // TCP socket for communication
    std::unique_ptr<boost::asio::ip::tcp::endpoint> endpoint_;// Remote endpoint information
    std::mutex io_mutex_;                    // Synchronizes I/O operations

    std::unique_ptr<std::thread> processing_thread_;    // Thread for async processing
    std::atomic<bool> processing_active_{false};        // Controls processing loop

    std::unique_ptr<boost::asio::streambuf> input_buffer_;    // Buffer for incoming data
    std::unique_ptr<std::istream> input_stream_;              // Stream wrapper for input buffer

    // Sets up input stream for reading data
    void initialize_streams();
    // Performs connection cleanup and resource release
    void cleanup_connection();
    // Main processing loop for async operations
    void process_stream();
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP