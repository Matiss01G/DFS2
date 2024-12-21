#ifndef DFS_NETWORK_MESSAGE_FRAME_HPP
#define DFS_NETWORK_MESSAGE_FRAME_HPP

#include <cstdint>
#include <array>
#include <istream>
#include <ostream>
#include <memory>
#include <stdexcept>
#include <boost/endian/conversion.hpp>

namespace dfs {
namespace network {

enum class MessageType : uint8_t {
    STORE_FILE = 0,
    GET_FILE = 1
};

struct MessageFrame {
    std::array<uint8_t, 16> initialization_vector;
    MessageType message_type;
    uint32_t source_id;
    uint64_t payload_size;
};

class MessageHandler {
public:
    // Serializes a message frame and data stream into an output stream
    // Returns number of bytes written
    static std::size_t serialize(const MessageFrame& frame, std::istream& data, std::ostream& output);

    // Deserializes a message frame from an input stream and positions the stream at payload start
    // Returns the parsed message frame and a pointer to the input stream positioned at payload start
    static std::pair<MessageFrame, std::istream*> deserialize(std::istream& input);

private:
    static constexpr std::size_t FRAME_SIZE = 
        16 +     // initialization_vector size
        1 +      // message_type size
        4 +      // source_id size
        8;       // payload_size

    // Helper methods for serialization
    static void write_bytes(std::ostream& output, const void* data, std::size_t size);
    static void read_bytes(std::istream& input, void* data, std::size_t size);

    // Network byte order conversion helpers
    static uint32_t to_network_order(uint32_t host_value);
    static uint64_t to_network_order(uint64_t host_value);
    static uint32_t from_network_order(uint32_t network_value);
    static uint64_t from_network_order(uint64_t network_value);
};

class MessageError : public std::runtime_error {
public:
    explicit MessageError(const std::string& message) 
        : std::runtime_error(message) {}
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_MESSAGE_FRAME_HPP