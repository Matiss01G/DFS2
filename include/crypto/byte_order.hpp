#ifndef DFS_BYTE_ORDER_HPP
#define DFS_BYTE_ORDER_HPP

#include <cstdint>
#include <array>
#include <algorithm>
#include <cstring>

namespace dfs::crypto {
class ByteOrder {
public:
    // Detects if system is little endian
    static bool isLittleEndian() {
        static const uint16_t value = 0x0001;
        return *reinterpret_cast<const uint8_t*>(&value) == 0x01;
    }

    // Converts host byte order to network byte order (big endian)
    template<typename T>
    static T toNetworkOrder(T value) {
        if (isLittleEndian()) {
            return byteSwap(value);
        }
        return value;  
    }

    // Converts network byte order (big endian) back to host byte order    
    template<typename T>
    static T fromNetworkOrder(T value) {
        if (isLittleEndian()) {
            return byteSwap(value);
        }
        return value;  

private:
    // Generic byte swap implementation that works for any size T
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
