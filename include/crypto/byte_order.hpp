#ifndef DFS_BYTE_ORDER_HPP
#define DFS_BYTE_ORDER_HPP

#include <cstdint>
#include <array>
#include <algorithm>
#include <cstring>

namespace dfs::crypto {

/**
 * @brief Handles byte order conversions for cross-platform compatibility in distributed systems
 * 
 * This class provides methods to convert between host byte order and network byte order (big-endian)
 * to ensure consistent data representation across different architectures. This is crucial for
 * a distributed file system where data is transferred between machines with different endianness.
 */
class ByteOrder {
public:
    /**
     * @brief Detects if the current system is little-endian
     * 
     * Uses a 16-bit value (0x0001) and checks its first byte to determine endianness:
     * - If first byte is 0x01, system is little-endian
     * - If first byte is 0x00, system is big-endian
     * 
     * @return true if system is little-endian, false if big-endian
     */
    static bool isLittleEndian() {
        static const uint16_t value = 0x0001;
        return *reinterpret_cast<const uint8_t*>(&value) == 0x01;
    }

    /**
     * @brief Converts a value from host byte order to network byte order (big-endian)
     * 
     * @tparam T Integer type (uint16_t, uint32_t, uint64_t)
     * @param value The value to convert
     * @return T The value in network byte order
     */
    template<typename T>
    static T toNetworkOrder(T value) {
        if (isLittleEndian()) {
            return byteSwap(value);
        }
        return value;  // Already in network byte order on big-endian systems
    }

    /**
     * @brief Converts a value from network byte order (big-endian) to host byte order
     * 
     * @tparam T Integer type (uint16_t, uint32_t, uint64_t)
     * @param value The value to convert from network byte order
     * @return T The value in host byte order
     */
    template<typename T>
    static T fromNetworkOrder(T value) {
        if (isLittleEndian()) {
            return byteSwap(value);
        }
        return value;  // Already in host byte order on big-endian systems
    }

private:
    /**
     * @brief Swaps the bytes of a value to change its endianness
     * 
     * Uses std::array and std::memcpy for safe memory handling and
     * std::reverse for byte reordering. This operation is only performed
     * when necessary (on little-endian systems).
     * 
     * @tparam T Integer type to swap
     * @param value The value whose bytes need to be swapped
     * @return T The value with bytes in reverse order
     */
    template<typename T>
    static T byteSwap(T value) {
        std::array<uint8_t, sizeof(T)> bytes;
        std::memcpy(bytes.data(), &value, sizeof(T));
        std::reverse(bytes.begin(), bytes.end());
        T result;
        std::memcpy(&result, bytes.data(), sizeof(T));
        return result;
    }
};

} // namespace dfs::crypto

#endif // DFS_BYTE_ORDER_HPP
