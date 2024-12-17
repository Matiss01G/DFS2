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
            uint16_t port,
            std::shared_ptr<crypto::CryptoStream> crypto_stream);
    
    ~TcpPeer() override;
    
    // IPeer implementation
    void connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void send_message(MessageType type, std::shared_ptr<std::istream> payload,
                     const std::vector<uint8_t>& file_key = {}) override;
    void set_message_handler(message_handler handler) override;
    void set_error_handler(error_handler handler) override;
    std::string get_address() const override;
    uint16_t get_port() const override;
    const std::array<uint8_t, 32>& get_id() const override;

private:
    // Internal types for managing async operations
    struct PendingMessage {
        MessageHeader header;
        std::shared_ptr<std::istream> payload;
    };

    // Asio components
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::endpoint endpoint_;
    
    // Stream encryption
    std::shared_ptr<crypto::CryptoStream> crypto_stream_;
    
public:
    // Allow accepting incoming connections
    void accept(boost::asio::ip::tcp::acceptor& acceptor,
               const std::function<void(const boost::system::error_code&)>& handler);
    
private:
    
    // State
    std::atomic<bool> connected_{false};
    std::array<uint8_t, 32> peer_id_;
    message_handler message_handler_;
    error_handler error_handler_;
    
    // Message queue
    std::queue<PendingMessage> message_queue_;
    std::mutex queue_mutex_;
    bool writing_{false};
    
    // Internal methods for async operations
    void start_read_header();
    void handle_read_header(const boost::system::error_code& error,
                          std::vector<uint8_t>& header_buffer);
    void handle_read_payload(const boost::system::error_code& error,
                           const MessageHeader& header,
                           std::shared_ptr<std::vector<uint8_t>> payload_buffer);
    void start_write();
    void handle_write(const boost::system::error_code& error);
    
    // Error handling
    void handle_error(const boost::system::error_code& error);
};

} // namespace dfs::network

#endif // DFS_NETWORK_TCP_PEER_HPP
