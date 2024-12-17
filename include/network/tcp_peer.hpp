#ifndef DFS_NETWORK_TCP_PEER_HPP
#define DFS_NETWORK_TCP_PEER_HPP

#include <queue>
#include <mutex>
#include "peer.hpp"

namespace dfs::network {

class TcpPeer : public IPeer, public std::enable_shared_from_this<TcpPeer> {
public:
    TcpPeer(boost::asio::io_context& io_context,
            const std::string& address,
            uint16_t port);
    
    ~TcpPeer() override;
    
    // IPeer implementation
    void connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void send_packet(PacketType type, 
                    std::shared_ptr<std::istream> payload,
                    uint32_t sequence_number) override;
    void set_packet_handler(packet_handler handler) override;
    void set_error_handler(error_handler handler) override;
    std::string get_address() const override;
    uint16_t get_port() const override;
    const std::array<uint8_t, 16>& get_id() const override;

private:
    // Constants
    static constexpr size_t MAX_PAYLOAD_SIZE = 1024 * 1024 * 1024; // 1GB max payload size
    
    // Internal types for managing async operations
    struct PendingPacket {
        PacketHeader header;
        std::shared_ptr<std::istream> payload;
    };

    // Asio components
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::endpoint endpoint_;
    
public:
    // Allow accepting incoming connections
    void accept(boost::asio::ip::tcp::acceptor& acceptor,
               const std::function<void(const boost::system::error_code&)>& handler);
    
private:
    
    // State
    std::atomic<bool> connected_{false};
    std::array<uint8_t, 16> peer_id_;
    packet_handler packet_handler_;
    error_handler error_handler_;
    
    // Packet queue
    std::queue<PendingPacket> packet_queue_;
    std::mutex queue_mutex_;
    bool writing_{false};
    
    // Internal methods for async operations
    void start_read_header();
    void handle_read_header(const boost::system::error_code& error,
                          std::vector<uint8_t>& header_buffer);
    void read_payload_chunk(const PacketHeader& header,
                          std::shared_ptr<std::stringstream> stream_buffer,
                          size_t bytes_read);
    void handle_read_payload(const boost::system::error_code& error,
                           const PacketHeader& header,
                           std::shared_ptr<std::vector<uint8_t>> payload_buffer);
    void start_write();
    void handle_write(const boost::system::error_code& error);
    
    // Error handling
    void handle_error(const boost::system::error_code& error);
};

} // namespace dfs::network

#endif // DFS_NETWORK_TCP_PEER_HPP