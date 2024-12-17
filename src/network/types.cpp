#include "../../include/network/types.hpp"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>

namespace dfs::network {

std::vector<uint8_t> PacketHeader::serialize() const {
    std::vector<uint8_t> buffer(HEADER_SIZE);
    size_t offset = 0;

    // Serialize packet type
    buffer[offset++] = static_cast<uint8_t>(type);

    // Serialize payload size (convert to network byte order)
    uint64_t net_payload_size = htobe64(payload_size);
    std::memcpy(buffer.data() + offset, &net_payload_size, sizeof(net_payload_size));
    offset += sizeof(net_payload_size);

    // Serialize sequence number (convert to network byte order)
    uint32_t net_sequence = htonl(sequence_number);
    std::memcpy(buffer.data() + offset, &net_sequence, sizeof(net_sequence));
    offset += sizeof(net_sequence);

    // Copy peer ID
    std::copy(peer_id.begin(), peer_id.end(), buffer.begin() + offset);

    return buffer;
}

PacketHeader PacketHeader::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < HEADER_SIZE) {
        throw std::runtime_error("Invalid packet header size");
    }

    PacketHeader header;
    size_t offset = 0;

    // Deserialize packet type
    header.type = static_cast<PacketType>(data[offset++]);

    // Deserialize payload size (convert from network byte order)
    uint64_t net_payload_size;
    std::memcpy(&net_payload_size, data.data() + offset, sizeof(net_payload_size));
    header.payload_size = be64toh(net_payload_size);
    offset += sizeof(net_payload_size);

    // Deserialize sequence number (convert from network byte order)
    uint32_t net_sequence;
    std::memcpy(&net_sequence, data.data() + offset, sizeof(net_sequence));
    header.sequence_number = ntohl(net_sequence);
    offset += sizeof(net_sequence);

    // Copy peer ID
    std::copy(data.begin() + offset,
             data.begin() + offset + 16,
             header.peer_id.begin());

    return header;
}

std::string to_string(NetworkError error) {
    switch (error) {
        case NetworkError::SUCCESS:
            return "Success";
        case NetworkError::CONNECTION_FAILED:
            return "Connection failed";
        case NetworkError::PEER_DISCONNECTED:
            return "Peer disconnected";
        case NetworkError::INVALID_PACKET:
            return "Invalid packet";
        case NetworkError::PROTOCOL_ERROR:
            return "Protocol error";
        case NetworkError::UNKNOWN_ERROR:
            return "Unknown error";
        default:
            return "Unrecognized error";
    }
}

} // namespace dfs::network
