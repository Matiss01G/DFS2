#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <istream>
#include <ostream>
#include <vector>
#include <sstream>
#include "crypto/byte_order.hpp"
#include "network/message_frame.hpp"

namespace dfs {
namespace network {

class MessageHandler {
public:
    // Constants for message handling
    static constexpr size_t MAX_FILE_SIZE = 1024 * 1024 * 1024; // 1GB limit
    static constexpr size_t IV_SIZE = 16; // Size of initialization vector

    enum class MessageType : uint32_t {
        STORE_FILE = 1,
        GET_FILE = 2,
        ERROR = 0xFFFFFFFF
    };

    struct MessageHeader {
        MessageType type;
        uint64_t file_size;
        std::string file_name;
    };

    MessageHandler() = default;
    ~MessageHandler() = default;

    // Prevent copying
    MessageHandler(const MessageHandler&) = delete;
    MessageHandler& operator=(const MessageHandler&) = delete;

    
    // Serializes character data into network ordered bytes
    bool serialize(std::istream& input, std::ostream& output);


    // Deserializes network ordered bytes into character data
    bool deserialize(std::istream& input, std::ostream& output);

    
    // Processes incoming data stream
    bool process_incoming_data(std::istream& input, const std::array<uint8_t, IV_SIZE>& iv);


    // Processes outgoing data stream
    std::unique_ptr<std::stringstream> process_outgoing_data(
        const MessageHeader& headers,
        std::istream& data
    );

private:
    // Helper methods for reading and writing headers
    bool read_headers(std::istream& input, MessageHeader& headers);
    bool write_headers(std::ostream& output, const MessageHeader& headers);
};

} // namespace network
} // namespace dfs