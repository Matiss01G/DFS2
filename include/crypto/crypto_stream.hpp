#ifndef DFS_CRYPTO_STREAM_HPP
#define DFS_CRYPTO_STREAM_HPP

#include <istream>
#include <ostream>
#include <vector>
#include <memory>
#include <array>
#include "crypto_error.hpp"

namespace dfs::crypto {

// Forward declaration for OpenSSL cipher context
struct CipherContext;

class CryptoStream {
public:
    enum class Mode {
        Encrypt,
        Decrypt
    };

    static constexpr size_t KEY_SIZE = 32;     // 256 bits for AES-256
    static constexpr size_t IV_SIZE = 16;      // 128 bits for CBC mode
    static constexpr size_t BLOCK_SIZE = 16;   // AES block size

    CryptoStream();
    ~CryptoStream();

    // Initialize with encryption key and IV
    void initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv);

    // Set operation mode
    void setMode(Mode mode) { mode_ = mode; }
    Mode getMode() const { return mode_; }

    // Stream operator overloads for encryption/decryption
    CryptoStream& operator>>(std::ostream& output);  // Process data to output
    CryptoStream& operator<<(std::istream& input);   // Accept input data

    // Encryption/decryption stream operations
    std::ostream& encrypt(std::istream& input, std::ostream& output);
    std::ostream& decrypt(std::istream& input, std::ostream& output);

    // Generate a cryptographically secure initialization vector
    std::array<uint8_t, IV_SIZE> generate_IV() const;

    // Generate a cryptographically secure key
    std::vector<uint8_t> generate_key() const;

private:
    std::vector<uint8_t> key_;
    std::vector<uint8_t> iv_;
    std::unique_ptr<CipherContext> context_;
    bool is_initialized_ = false;
    Mode mode_ = Mode::Encrypt;  // Default to encryption mode
    std::istream* pending_input_ = nullptr;  // Store input stream for operator>> to process

    // Process data through OpenSSL cipher context
    void processStream(std::istream& input, std::ostream& output, bool encrypting);
    void initializeCipher(bool encrypting);
};

} // namespace dfs::crypto

#endif // DFS_CRYPTO_STREAM_HPP