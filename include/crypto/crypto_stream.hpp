#ifndef DFS_CRYPTO_STREAM_HPP
#define DFS_CRYPTO_STREAM_HPP

#include <vector>
#include <span>
#include "crypto_error.hpp"

namespace dfs::crypto {

class CryptoStream {
public:
    static constexpr size_t KEY_SIZE = 32;  // 256 bits
    static constexpr size_t IV_SIZE = 16;   // 128 bits
    static constexpr size_t BLOCK_SIZE = 16; // AES block size

    CryptoStream();
    ~CryptoStream();

    void initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv);
    std::vector<uint8_t> encryptStream(std::span<const uint8_t> data);
    std::vector<uint8_t> decryptStream(std::span<const uint8_t> data);

private:
    std::vector<uint8_t> key_;
    std::vector<uint8_t> iv_;
};

} // namespace dfs::crypto

#endif // DFS_CRYPTO_STREAM_HPP
