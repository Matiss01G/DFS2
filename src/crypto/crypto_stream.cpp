#include "crypto/crypto_stream.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <array>
#include <stdexcept>

namespace dfs::crypto {

// Wrapper for OpenSSL cipher context
struct CipherContext {
    EVP_CIPHER_CTX* ctx = nullptr;
    
    CipherContext() {
        ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create cipher context");
        }
    }
    
    ~CipherContext() {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }
    
    EVP_CIPHER_CTX* get() { return ctx; }
};

CryptoStream::CryptoStream() {
    OpenSSL_add_all_algorithms();
    context_ = std::make_unique<CipherContext>();
}

CryptoStream::~CryptoStream() {
    EVP_cleanup();
}

void CryptoStream::initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    if (key.size() != KEY_SIZE) {
        throw InitializationError("Invalid key size");
    }
    if (iv.size() != IV_SIZE) {
        throw InitializationError("Invalid IV size");
    }

    key_ = key;
    iv_ = iv;
    is_initialized_ = true;
}

void CryptoStream::initializeCipher(bool encrypting) {
    if (!is_initialized_) {
        throw InitializationError("CryptoStream not initialized");
    }

    if (encrypting) {
        if (!EVP_EncryptInit_ex(context_->get(), EVP_aes_256_cbc(), nullptr, 
                               key_.data(), iv_.data())) {
            throw EncryptionError("Failed to initialize encryption");
        }
    } else {
        if (!EVP_DecryptInit_ex(context_->get(), EVP_aes_256_cbc(), nullptr, 
                               key_.data(), iv_.data())) {
            throw DecryptionError("Failed to initialize decryption");
        }
    }
}

void CryptoStream::processStream(std::istream& input, std::ostream& output, bool encrypting) {
    if (!input.good() || !output.good()) {
        throw std::runtime_error("Invalid stream state");
    }

    // Initialize cipher for operation
    initializeCipher(encrypting);
    
    // Process input stream in blocks with larger buffer for efficiency
    static constexpr size_t BUFFER_SIZE = 8192; // 8KB buffer for better performance
    std::array<uint8_t, BUFFER_SIZE> inbuf;
    std::array<uint8_t, BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH> outbuf;
    int outlen;

    while (!input.eof()) {
        // Read a block of data
        input.read(reinterpret_cast<char*>(inbuf.data()), inbuf.size());
        auto bytes_read = input.gcount();
        
        if (bytes_read <= 0) {
            if (!input.eof()) {
                throw std::runtime_error("Failed to read from input stream");
            }
            break;
        }

        // Process the block
        if (encrypting) {
            if (!EVP_EncryptUpdate(context_->get(), outbuf.data(), &outlen,
                                 inbuf.data(), static_cast<int>(bytes_read))) {
                throw EncryptionError("Failed to encrypt data block");
            }
        } else {
            if (!EVP_DecryptUpdate(context_->get(), outbuf.data(), &outlen,
                                 inbuf.data(), static_cast<int>(bytes_read))) {
                throw DecryptionError("Failed to decrypt data block");
            }
        }

        // Write processed block
        if (outlen > 0) {
            output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
            if (!output.good()) {
                throw std::runtime_error("Failed to write to output stream");
            }
        }
    }

    // Finalize the operation
    if (encrypting) {
        if (!EVP_EncryptFinal_ex(context_->get(), outbuf.data(), &outlen)) {
            throw EncryptionError("Failed to finalize encryption");
        }
    } else {
        if (!EVP_DecryptFinal_ex(context_->get(), outbuf.data(), &outlen)) {
            throw DecryptionError("Failed to finalize decryption");
        }
    }

    // Write final block if any
    if (outlen > 0) {
        output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
        if (!output.good()) {
            throw std::runtime_error("Failed to write final block");
        }
    }
}

std::ostream& CryptoStream::encrypt(std::istream& input, std::ostream& output) {
    processStream(input, output, true);
    return output;
}

std::ostream& CryptoStream::decrypt(std::istream& input, std::ostream& output) {
    processStream(input, output, false);
    return output;
}

CryptoStream& CryptoStream::operator>>(std::ostream& output) {
    if (!pending_input_) {
        throw std::runtime_error("No pending input stream. Use operator<< first.");
    }
    
    // Process the stored input stream
    encrypt(*pending_input_, output);
    
    // Clear the stored stream after processing
    pending_input_ = nullptr;
    
    return *this;
}

CryptoStream& CryptoStream::operator<<(std::istream& input) {
    // Verify initialization state
    if (!is_initialized_) {
        throw InitializationError("CryptoStream not initialized");
    }
    
    // Validate input stream state
    if (!input.good()) {
        throw std::runtime_error("Invalid input stream state");
    }

    // Clear any previous pending input
    pending_input_ = nullptr;
    
    // Store the input stream for later processing
    pending_input_ = &input;
    
    return *this;
}

} // namespace dfs::crypto
