#ifndef DFS_NETWORK_TYPES_HPP
#define DFS_NETWORK_TYPES_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include "../crypto/crypto_stream.hpp"

namespace dfs::network {

// Message types for network communication
enum class MessageType : uint8_t {
    HANDSHAKE = 0x01,
    FILE_TRANSFER = 0x02,
    FILE_REQUEST = 0x03,
    PEER_LIST = 0x04,
    ERROR = 0xFF
};

// Message header structure for framing
struct MessageHeader {
    MessageType type;
    uint64_t payload_size;
    std::array<uint8_t, crypto::CryptoStream::IV_SIZE> iv;
    std::array<uint8_t, 32> source_id;  // Unique identifier for the source peer
    std::array<uint8_t, crypto::CryptoStream::KEY_SIZE> file_key;  // Optional, used for file transfer
    
    // Serialization methods
    std::vector<uint8_t> serialize() const;
    static MessageHeader deserialize(const std::vector<uint8_t>& data);
    
    static constexpr size_t HEADER_SIZE = sizeof(MessageType) + sizeof(uint64_t) + 
                                        crypto::CryptoStream::IV_SIZE + 32 + 
                                        crypto::CryptoStream::KEY_SIZE;
};

// Network error types
enum class NetworkError {
    SUCCESS = 0,
    CONNECTION_FAILED,
    PEER_DISCONNECTED,
    INVALID_MESSAGE,
    ENCRYPTION_ERROR,
    UNKNOWN_ERROR
};

// Convert NetworkError to string for logging
std::string to_string(NetworkError error);

} // namespace dfs::network

#endif // DFS_NETWORK_TYPES_HPP
