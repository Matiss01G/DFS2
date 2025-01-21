#ifndef DFS_NETWORK_TCP_PEER_HPP
#define DFS_NETWORK_TCP_PEER_HPP

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include "peer.hpp"
#include "channel.hpp"
#include "codec.hpp"

namespace dfs {
namespace network {

/**
 * @brief TCP implementation of the Peer interface
 * 
 * This class provides TCP/IP-based network communication with stream support.
 * It implements an asynchronous event-driven approach for handling network I/O,
 * utilizing boost::asio for efficient, non-blocking operations.
 */
class TCP_Peer : public Peer,
                 public std::enable_shared_from_this<TCP_Peer> {
public:
    // Update the StreamProcessor type to include the source identifier
    using StreamProcessor = std::function<void(std::istream&, const std::string&)>;

    explicit TCP_Peer(const std::string& peer_id, Channel& channel, const std::vector<uint8_t>& key);
    ~TCP_Peer() override;

    // Delete copy operations to prevent socket duplication
    TCP_Peer(const TCP_Peer&) = delete;
    TCP_Peer& operator=(const TCP_Peer&) = delete;

    // Stream operations
    std::istream* get_input_stream() override;
    bool send_message(const std::string& message) override;

    // Stream processing
    void set_stream_processor(StreamProcessor processor) override;
    bool start_stream_processing() override;
    void stop_stream_processing() override;

    // Asynchronous I/O Operations
    void async_write();
    bool send_stream(std::istream& input_stream, std::size_t buffer_size = 8192);
    void async_read_next();

    // Getters
    const std::string& get_peer_id() const;
    boost::asio::ip::tcp::socket& get_socket();

private:
    std::string peer_id_;
    StreamProcessor stream_processor_;

    // Network components
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::ip::tcp::endpoint> endpoint_;
    std::mutex io_mutex_;

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

    // Codec for encryption/decryption
    std::unique_ptr<Codec> codec_;

    friend class PeerManager;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP