#ifndef DFS_CRYPTO_ERROR_HPP
#define DFS_CRYPTO_ERROR_HPP

#include <stdexcept>
#include <string>

namespace dfs::crypto {

class CryptoError : public std::runtime_error {
public:
    explicit CryptoError(const std::string& message) 
        : std::runtime_error(message) {}
};

class InitializationError : public CryptoError {
public:
    explicit InitializationError(const std::string& message) 
        : CryptoError("Initialization error: " + message) {}
};

class EncryptionError : public CryptoError {
public:
    explicit EncryptionError(const std::string& message) 
        : CryptoError("Encryption error: " + message) {}
};

class DecryptionError : public CryptoError {
public:
    explicit DecryptionError(const std::string& message) 
        : CryptoError("Decryption error: " + message) {}
};

} // namespace dfs::crypto

#endif // DFS_CRYPTO_ERROR_HPP
