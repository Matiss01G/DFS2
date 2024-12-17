#ifndef DFS_NETWORK_TYPES_HPP
#define DFS_NETWORK_TYPES_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace dfs::network {

// Basic packet types for network communication
enum class PacketType : uint8_t {
    DATA = 0x01,     // Regular data packet
    CONTROL = 0x02,  // Control/management packet
    ACK = 0x03,      // Acknowledgment packet
    ERROR = 0xFF     // Error notification packet
};

// Packet header structure for framing
struct PacketHeader {
    PacketType type;
    uint64_t payload_size;
    uint32_t sequence_number;  // For packet ordering and reliability
    std::array<uint8_t, 16> peer_id;  // Unique identifier for the source peer
    
    // Serialization methods
    std::vector<uint8_t> serialize() const;
    static PacketHeader deserialize(const std::vector<uint8_t>& data);
    
    static constexpr size_t HEADER_SIZE = sizeof(PacketType) + 
                                        sizeof(uint64_t) + 
                                        sizeof(uint32_t) + 
                                        16; // peer_id size
};

// Network error types
enum class NetworkError {
    SUCCESS = 0,
    CONNECTION_FAILED,
    PEER_DISCONNECTED,
    INVALID_PACKET,
    PROTOCOL_ERROR,
    UNKNOWN_ERROR
};

// Convert NetworkError to string for logging
std::string to_string(NetworkError error);

} // namespace dfs::network

#endif // DFS_NETWORK_TYPES_HPP
