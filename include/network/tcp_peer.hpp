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


class TCP_Peer : public Peer, public std::enable_shared_from_this<TCP_Peer> {
public:
    // Update the StreamProcessor type to include the source identifier
    using StreamProcessor = std::function<void(std::istream&)>;

    // Delete copy operations to prevent socket duplication
    TCP_Peer(const TCP_Peer&) = delete;
    TCP_Peer& operator=(const TCP_Peer&) = delete;


    // ---- CONSTRUCTOR AND DESTRUCTOR ----
    explicit TCP_Peer(uint8_t peer_id, Channel& channel, const std::vector<uint8_t>& key);
    ~TCP_Peer() override;
    

    // ---- STREAM CONTROL OPERATIONS ----
    // Start asynchronous stream processing
    bool start_stream_processing() override;
    // Stop stream processing and cleanup resources
    void stop_stream_processing() override;


    // ---- OUTGOING DATA STREAM PROCESSING ----
    // Send data stream to peer with buffered writing
    bool send_stream(std::istream& input_stream, std::size_t total_size, std::size_t buffer_size = 8192);
    // Convenience method to send string message
    bool send_message(const std::string& message, std::size_t total_size) override;

    
    // ---- GETTERS AND SETTERS ----
    // Returns input stream if socket is connected
    std::istream* get_input_stream() override;
    uint8_t get_peer_id() const;
    boost::asio::ip::tcp::socket& get_socket();
    
    // Sets callback function for processing received data streams
    void set_stream_processor(StreamProcessor processor) override;

private:
    // ---- PARAMETERS ----
    uint8_t peer_id_;
    StreamProcessor stream_processor_;
    std::size_t expected_size_;

    // Stream buffers
    std::unique_ptr<boost::asio::streambuf> input_buffer_;
    std::unique_ptr<std::istream> input_stream_;

    // Network components
    boost::asio::io_context io_context_;
    std::mutex io_mutex_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::ip::tcp::endpoint> endpoint_;

    // Processing thread
    std::unique_ptr<std::thread> processing_thread_;
    std::atomic<bool> processing_active_{false};

    // Codec for encryption/decryption
    std::unique_ptr<Codec> codec_;


    // ---- STREAM CONTROL OPERATIONS ----
    void initialize_streams();

    
    // ---- INCOMING DATA STREAM PROCESSING ----
    // Main stream processing loop that handles incoming data
    void process_stream();
    // Reads size of incoming data for size-based framing
    void handle_read_size(const boost::system::error_code& ec, std::size_t bytes_transferred);
    // Reads data from socket
    void handle_read_data(const boost::system::error_code& ec, std::size_t bytes_transferred);
    // Passes read data to stream processor
    void process_received_data();
    // Initiates an asynchronous read operation for the next message
    void async_read_next();


    // ---- OUTGOING DATA STREAM PROCESSING ----
    // Sends total size of incoming data
    bool send_size(std::size_t total_size);
    

    // ---- TEARDOWN ----
    // Cleans up connection resources and reset state
    void cleanup_connection();

    
    friend class PeerManager;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_TCP_PEER_HPP