#ifndef DFS_NETWORK_TCP_PEER_HPP
#define DFS_NETWORK_TCP_PEER_HPP

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include "peer.hpp"
#include "connection_state.hpp"

namespace dfs {
namespace network {

/**
 * TCP implementation of the Peer interface.
 * Provides TCP/IP-based network communication with stream support.
 */
class TCP_Peer : public Peer {
public:
    explicit TCP_Peer(const std::string& peer_id);
    ~TCP_Peer() override;

    // Delete copy operations to prevent socket duplication
    TCP_Peer(const TCP_Peer&) = delete;
    TCP_Peer& operator=(const TCP_Peer&) = delete;

    // Connection management
    bool connect(const std::string& address, uint16_t port) override;
    bool disconnect() override;
    bool is_connected() const { return get_connection_state() == ConnectionState::State::CONNECTED; }

    // Stream operations
    std::ostream* get_output_stream() override;
    std::istream* get_input_stream() override;

    // Stream processing
    void set_stream_processor(StreamProcessor processor) override;
    bool start_stream_processing() override;
    void stop_stream_processing() override;

    // State management
    ConnectionState::State get_connection_state() const override;

    // Accessors
    const std::string& get_peer_id() const { return peer_id_; }

private:
    // Core attributes
    std::string peer_id_;
    ConnectionState connection_state_;
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
    std::unique_ptr<boost::asio::streambuf> output_buffer_;
    std::unique_ptr<std::istream> input_stream_;
    std::unique_ptr<std::ostream> output_stream_;

    // Internal methods
    void initialize_streams();
    void cleanup_connection();
    void process_stream();
    bool validate_connection_state(ConnectionState::State required_state) const;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP
