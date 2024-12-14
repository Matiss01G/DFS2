#include "crypto/crypto_stream.hpp"
#include "crypto/logger.hpp"
using namespace dfs::logging;
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <memory>

namespace dfs::crypto {

CryptoStream::CryptoStream() {
    OpenSSL_add_all_algorithms();
    LOG_INFO << "CryptoStream instance created";
}

CryptoStream::~CryptoStream() {
    EVP_cleanup();
}

void CryptoStream::initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    LOG_DEBUG << "Initializing CryptoStream with key size: " << key.size() << ", IV size: " << iv.size();
    
    if (key.size() != KEY_SIZE) {
        LOG_ERROR << "Invalid key size: " << key.size() << " (expected " << KEY_SIZE << ")";
        throw InitializationError("Invalid key size");
    }
    if (iv.size() != IV_SIZE) {
        LOG_ERROR << "Invalid IV size: " << iv.size() << " (expected " << IV_SIZE << ")";
        throw InitializationError("Invalid IV size");
    }

    key_ = key;
    iv_ = iv;
    LOG_INFO << "CryptoStream initialized successfully";
}

std::vector<uint8_t> CryptoStream::encryptStream(std::span<const uint8_t> data) {
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

std::vector<uint8_t> CryptoStream::decryptStream(std::span<const uint8_t> data) {
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

} // namespace dfs::crypto
