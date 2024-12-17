#ifndef DFS_NETWORK_PEER_HPP
#define DFS_NETWORK_PEER_HPP

#include <memory>
#include <string>
#include <functional>
#include <boost/asio.hpp>
#include "types.hpp"
#include "../crypto/crypto_stream.hpp"

namespace dfs::network {

class IPeer {
public:
    using message_handler = std::function<void(const MessageHeader&, std::shared_ptr<std::istream>)>;
    using error_handler = std::function<void(NetworkError)>;
    
    virtual ~IPeer() = default;
    
    // Connection management
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Stream-based message sending
    virtual void send_message(MessageType type, std::shared_ptr<std::istream> payload,
                            const std::vector<uint8_t>& file_key = {}) = 0;
    
    // Event handlers
    virtual void set_message_handler(message_handler handler) = 0;
    virtual void set_error_handler(error_handler handler) = 0;
    
    // Peer identification
    virtual std::string get_address() const = 0;
    virtual uint16_t get_port() const = 0;
    virtual const std::array<uint8_t, 32>& get_id() const = 0;
};

// Factory function for creating TCP peers
std::shared_ptr<IPeer> create_tcp_peer(
    boost::asio::io_context& io_context,
    const std::string& address,
    uint16_t port,
    std::shared_ptr<crypto::CryptoStream> crypto_stream
);

} // namespace dfs::network

#endif // DFS_NETWORK_PEER_HPP
