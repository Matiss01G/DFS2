#include "crypto/crypto_stream.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <array>
#include <stdexcept>
#include <boost/log/trivial.hpp>
#include <algorithm> //Added for std::min and std::max


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

// Initialize cryptographic context and OpenSSL algorithms
CryptoStream::CryptoStream() {
    BOOST_LOG_TRIVIAL(info) << "Initializing CryptoStream";
    OpenSSL_add_all_algorithms();  // Required for OpenSSL operation
    context_ = std::make_unique<CipherContext>();  // Create cipher context for AES operations
    BOOST_LOG_TRIVIAL(debug) << "CryptoStream initialization complete";
}

// Cleanup OpenSSL resources
CryptoStream::~CryptoStream() {
    BOOST_LOG_TRIVIAL(debug) << "Cleaning up CryptoStream resources";
    EVP_cleanup();  // Clean up OpenSSL algorithms to prevent memory leaks
}

// Initialize with encryption key and initialization vector (IV)
void CryptoStream::initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    BOOST_LOG_TRIVIAL(info) << "Initializing crypto parameters";
    
    // Validate key size (256 bits for AES-256)
    if (key.size() != KEY_SIZE) {
        BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << key.size() << " bytes (expected " << KEY_SIZE << " bytes)";
        throw InitializationError("Invalid key size");
    }
    
    // Validate IV size (128 bits for CBC mode)
    if (iv.size() != IV_SIZE) {
        BOOST_LOG_TRIVIAL(error) << "Invalid IV size: " << iv.size() << " bytes (expected " << IV_SIZE << " bytes)";
        throw InitializationError("Invalid IV size");
    }

    // Store key and IV for crypto operations
    key_ = key;
    iv_ = iv;
    is_initialized_ = true;
    BOOST_LOG_TRIVIAL(debug) << "Crypto parameters initialized successfully";
}

void CryptoStream::initializeCipher(bool encrypting) {
    BOOST_LOG_TRIVIAL(debug) << "Initializing cipher for " << (encrypting ? "encryption" : "decryption");
    
    if (!is_initialized_) {
        BOOST_LOG_TRIVIAL(error) << "Attempted to use uninitialized CryptoStream";
        throw InitializationError("CryptoStream not initialized");
    }

    if (encrypting) {
        if (!EVP_EncryptInit_ex(context_->get(), EVP_aes_256_cbc(), nullptr, 
                            key_.data(), iv_.data())) {
            BOOST_LOG_TRIVIAL(error) << "Failed to initialize encryption context";
            throw EncryptionError("Failed to initialize encryption");
        }
    } else {
        if (!EVP_DecryptInit_ex(context_->get(), EVP_aes_256_cbc(), nullptr, 
                            key_.data(), iv_.data())) {
            BOOST_LOG_TRIVIAL(error) << "Failed to initialize decryption context";
            throw DecryptionError("Failed to initialize decryption");
        }
    }
    
    BOOST_LOG_TRIVIAL(debug) << "Cipher initialization complete";
}

// Process input stream through the cipher (encryption or decryption)
void CryptoStream::processStream(std::istream& input, std::ostream& output, bool encrypting) {
    BOOST_LOG_TRIVIAL(info) << "Starting stream " << (encrypting ? "encryption" : "decryption");

    // Verify stream states before processing
    if (!input.good() || !output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid stream state detected";
        throw std::runtime_error("Invalid stream state");
    }

    // Initialize cipher context for either encryption or decryption
    initializeCipher(encrypting);

    // Calculate optimal chunk size based on system memory constraints
    // Default to 1KB for embedded systems, can be increased for systems with more resources
    static constexpr size_t MIN_CHUNK_SIZE = 1024;    // 1KB minimum
    static constexpr size_t MAX_CHUNK_SIZE = 4096;    // 4KB maximum
    const size_t CHUNK_SIZE = std::min(MAX_CHUNK_SIZE, 
                                     std::max(MIN_CHUNK_SIZE, 
                                            static_cast<size_t>(EVP_MAX_BLOCK_LENGTH * 4)));

    BOOST_LOG_TRIVIAL(debug) << "Using chunk size: " << CHUNK_SIZE << " bytes";

    std::vector<uint8_t> inbuf(CHUNK_SIZE);
    std::vector<uint8_t> outbuf(CHUNK_SIZE + EVP_MAX_BLOCK_LENGTH);
    int outlen;

    try {
        while (input.good() && !input.eof()) {
            // Read data in chunks
            input.read(reinterpret_cast<char*>(inbuf.data()), CHUNK_SIZE);
            const size_t bytes_read = input.gcount();

            if (bytes_read == 0) break;

            // Process the chunk
            if (encrypting) {
                if (!EVP_EncryptUpdate(context_->get(), outbuf.data(), &outlen,
                                    inbuf.data(), static_cast<int>(bytes_read))) {
                    BOOST_LOG_TRIVIAL(error) << "Encryption update failed";
                    throw EncryptionError("Failed to encrypt data chunk");
                }
            } else {
                if (!EVP_DecryptUpdate(context_->get(), outbuf.data(), &outlen,
                                    inbuf.data(), static_cast<int>(bytes_read))) {
                    BOOST_LOG_TRIVIAL(error) << "Decryption update failed";
                    throw DecryptionError("Failed to decrypt data chunk");
                }
            }

            // Write processed chunk immediately
            if (outlen > 0) {
                output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
                if (!output.good()) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to write to output stream";
                    throw std::runtime_error("Failed to write to output stream");
                }
                output.flush(); // Ensure data is written immediately
            }
        }

        // Finalize the operation
        if (encrypting) {
            if (!EVP_EncryptFinal_ex(context_->get(), outbuf.data(), &outlen)) {
                BOOST_LOG_TRIVIAL(error) << "Failed to finalize encryption";
                throw EncryptionError("Failed to finalize encryption");
            }
        } else {
            if (!EVP_DecryptFinal_ex(context_->get(), outbuf.data(), &outlen)) {
                BOOST_LOG_TRIVIAL(error) << "Failed to finalize decryption";
                throw DecryptionError("Failed to finalize decryption");
            }
        }

        // Write final block if any
        if (outlen > 0) {
            output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
            if (!output.good()) {
                BOOST_LOG_TRIVIAL(error) << "Failed to write final block";
                throw std::runtime_error("Failed to write final block");
            }
            output.flush();
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
        throw;
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

// Stream operator for outputting processed data (encryption or decryption)
CryptoStream& CryptoStream::operator>>(std::ostream& output) {
    BOOST_LOG_TRIVIAL(debug) << "Stream operator>> called in " << (mode_ == Mode::Encrypt ? "encryption" : "decryption") << " mode";
    
    // Ensure we have an input stream to process
    if (!pending_input_) {
        BOOST_LOG_TRIVIAL(error) << "Stream operator>> called without prior input stream";
        throw std::runtime_error("No pending input stream. Use operator<< first.");
    }
    
    // Process the stored input stream based on current mode (encrypt or decrypt)
    if (mode_ == Mode::Encrypt) {
        encrypt(*pending_input_, output);
    } else {
        decrypt(*pending_input_, output);
    }
    
    // Clear the stored stream after processing to prevent accidental reuse
    pending_input_ = nullptr;
    BOOST_LOG_TRIVIAL(debug) << "Stream processing complete";
    
    return *this;
}

// Stream operator for accepting input data
CryptoStream& CryptoStream::operator<<(std::istream& input) {
    BOOST_LOG_TRIVIAL(debug) << "Stream operator<< called, storing input stream for processing";
    // Store input stream for later processing by operator>>
    pending_input_ = &input;
    return *this;
}

} // namespace dfs::crypto