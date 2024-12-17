#ifndef DFS_NETWORK_TCP_PEER_HPP
#define DFS_NETWORK_TCP_PEER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <system_error>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include "peer.hpp"
#include "connection_state.hpp"

namespace dfs {
namespace network {

/**
 * TCP implementation of the Peer interface.
 * Provides stream-based communication over TCP/IP using boost::asio.
 */
class TcpPeer : public Peer {
public:
    explicit TcpPeer();
    ~TcpPeer() override;

    // Connection management
    bool connect(const std::string& address, uint16_t port) override;
    bool disconnect() override;

    // Stream operations
    std::ostream* get_output_stream() override;
    std::istream* get_input_stream() override;
    
    // Stream processing
    void set_stream_processor(StreamProcessor processor) override;
    bool start_stream_processing() override;
    void stop_stream_processing() override;

    // State management
    ConnectionState::State get_connection_state() const override;

private:
    void process_streams();

    void handle_error(const std::error_code& error) {
        state_.store(ConnectionState::State::ERROR);
        close_socket();
    }

    void close_socket() {
        if (socket_ && socket_->is_open()) {
            boost::system::error_code ec;
            socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_->close(ec);
        }
    }

    static constexpr size_t BUFFER_SIZE = 8192;

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::streambuf> input_buffer_;
    std::unique_ptr<boost::asio::streambuf> output_buffer_;
    std::unique_ptr<std::istream> input_stream_;
    std::unique_ptr<std::ostream> output_stream_;
    
    StreamProcessor stream_processor_;
    std::atomic<ConnectionState::State> state_;
    std::atomic<bool> processing_;
    std::unique_ptr<std::thread> processing_thread_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP
