#ifndef DFS_CRYPTO_STREAM_HPP
#define DFS_CRYPTO_STREAM_HPP

#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <vector>
#include <memory>
#include <string>
#include <span>
#include "crypto_error.hpp"
#include "byte_order.hpp"

namespace dfs::crypto {

class CryptoStream {
public:
    static constexpr size_t KEY_SIZE = 32;  // 256 bits
    static constexpr size_t IV_SIZE = 16;   // 128 bits
    static constexpr size_t BLOCK_SIZE = 16; // AES block size

    CryptoStream() {
        OpenSSL_add_all_algorithms();
    }

    ~CryptoStream() {
        EVP_cleanup();
    }

    void initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
        if (key.size() != KEY_SIZE) {
            throw InitializationError("Invalid key size");
        }
        if (iv.size() != IV_SIZE) {
            throw InitializationError("Invalid IV size");
        }

        key_ = key;
        iv_ = iv;
    }

    std::vector<uint8_t> encryptStream(std::span<const uint8_t> data) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw EncryptionError("Failed to create cipher context");
        }

        std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> 
            ctx_guard(ctx, EVP_CIPHER_CTX_free);

        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_.data(), iv_.data())) {
            throw EncryptionError("Failed to initialize encryption");
        }

        std::vector<uint8_t> encrypted;
        encrypted.resize(data.size() + EVP_MAX_BLOCK_LENGTH);
        
        int outlen;
        if (!EVP_EncryptUpdate(ctx, encrypted.data(), &outlen, 
                              data.data(), static_cast<int>(data.size()))) {
            throw EncryptionError("Failed to encrypt data");
        }

        int final_len;
        if (!EVP_EncryptFinal_ex(ctx, encrypted.data() + outlen, &final_len)) {
            throw EncryptionError("Failed to finalize encryption");
        }

        encrypted.resize(outlen + final_len);
        return encrypted;
    }

    std::vector<uint8_t> decryptStream(std::span<const uint8_t> data) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw DecryptionError("Failed to create cipher context");
        }

        std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> 
            ctx_guard(ctx, EVP_CIPHER_CTX_free);

        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_.data(), iv_.data())) {
            throw DecryptionError("Failed to initialize decryption");
        }

        std::vector<uint8_t> decrypted;
        decrypted.resize(data.size());
        
        int outlen;
        if (!EVP_DecryptUpdate(ctx, decrypted.data(), &outlen, 
                              data.data(), static_cast<int>(data.size()))) {
            throw DecryptionError("Failed to decrypt data");
        }

        int final_len;
        if (!EVP_DecryptFinal_ex(ctx, decrypted.data() + outlen, &final_len)) {
            throw DecryptionError("Failed to finalize decryption");
        }

        decrypted.resize(outlen + final_len);
        return decrypted;
    }

private:
    std::vector<uint8_t> key_;
    std::vector<uint8_t> iv_;
};

} // namespace dfs::crypto

#endif // DFS_CRYPTO_STREAM_HPP
