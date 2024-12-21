#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <mutex>

namespace dfs {
namespace network {

// Forward declarations
struct MessageFrame;
class Channel;

/**
 * @brief Core class handling MessageFrame serialization/deserialization
 */
class Codec {
public:
    Codec() = default;
    ~Codec() = default;

    /**
     * @brief Serializes a MessageFrame to a binary stream
     * @param frame The MessageFrame to serialize
     * @param output The output stream to write to
     * @return Number of bytes written
     * @throws std::runtime_error if serialization fails
     */
    std::size_t serialize(const MessageFrame& frame, std::ostream& output);

    /**
     * @brief Deserializes a MessageFrame from a binary stream
     * @param input The input stream to read from
     * @param channel The Channel to produce messages to
     * @return The deserialized MessageFrame
     * @throws std::runtime_error if deserialization fails
     */
    MessageFrame deserialize(std::istream& input, Channel& channel);

protected:
    /**
     * @brief Converts host byte order to network byte order
     * @param value The value to convert
     * @return The converted value
     */
    template<typename T>
    static T to_network_order(T value) {
        if constexpr (sizeof(T) == 8) {
            return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(value)));
        } else if constexpr (sizeof(T) == 4) {
            return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(value)));
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(value)));
        }
        return value;
    }

    /**
     * @brief Converts network byte order to host byte order
     * @param value The value to convert
     * @return The converted value
     */
    template<typename T>
    static T from_network_order(T value) {
        return to_network_order(value);  // Same operation for conversion back
    }

private:
    std::mutex mutex_;  // Mutex for thread-safe channel operations

    /**
     * @brief Helper function to write bytes to a stream
     * @param output The output stream
     * @param data The data to write
     * @param size The number of bytes to write
     */
    static void write_bytes(std::ostream& output, const void* data, std::size_t size);

    /**
     * @brief Helper function to read bytes from a stream
     * @param input The input stream
     * @param data The buffer to read into
     * @param size The number of bytes to read
     */
    static void read_bytes(std::istream& input, void* data, std::size_t size);
};

} // namespace network
} // namespace dfs