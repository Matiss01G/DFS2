#include "../../include/network/types.hpp"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>

namespace dfs::network {

std::vector<uint8_t> MessageHeader::serialize() const {
    std::vector<uint8_t> buffer(HEADER_SIZE);
    size_t offset = 0;

    // Serialize message type
    buffer[offset++] = static_cast<uint8_t>(type);

    // Serialize payload size (convert to network byte order)
    uint64_t net_payload_size = htobe64(payload_size);
    std::memcpy(buffer.data() + offset, &net_payload_size, sizeof(net_payload_size));
    offset += sizeof(net_payload_size);

    // Copy IV
    std::copy(iv.begin(), iv.end(), buffer.begin() + offset);
    offset += iv.size();

    // Copy source ID
    std::copy(source_id.begin(), source_id.end(), buffer.begin() + offset);
    offset += source_id.size();

    // Copy file key
    std::copy(file_key.begin(), file_key.end(), buffer.begin() + offset);

    return buffer;
}

MessageHeader MessageHeader::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < HEADER_SIZE) {
        throw std::runtime_error("Invalid header data size");
    }

    MessageHeader header;
    size_t offset = 0;

    // Deserialize message type
    header.type = static_cast<MessageType>(data[offset++]);

    // Deserialize payload size (convert from network byte order)
    uint64_t net_payload_size;
    std::memcpy(&net_payload_size, data.data() + offset, sizeof(net_payload_size));
    header.payload_size = be64toh(net_payload_size);
    offset += sizeof(net_payload_size);

    // Copy IV
    std::copy(data.begin() + offset,
             data.begin() + offset + crypto::CryptoStream::IV_SIZE,
             header.iv.begin());
    offset += crypto::CryptoStream::IV_SIZE;

    // Copy source ID
    std::copy(data.begin() + offset,
             data.begin() + offset + 32,
             header.source_id.begin());
    offset += 32;

    // Copy file key
    std::copy(data.begin() + offset,
             data.begin() + offset + crypto::CryptoStream::KEY_SIZE,
             header.file_key.begin());

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
        case NetworkError::INVALID_MESSAGE:
            return "Invalid message";
        case NetworkError::ENCRYPTION_ERROR:
            return "Encryption error";
        case NetworkError::UNKNOWN_ERROR:
            return "Unknown error";
        default:
            return "Unrecognized error";
    }
}

} // namespace dfs::network
