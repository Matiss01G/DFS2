#include "crypto/crypto_stream.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <array>
#include <stdexcept>
#include <boost/log/trivial.hpp>

namespace dfs::crypto {

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
    BOOST_LOG_TRIVIAL(info) << "Initializing CryptoStream";
    OpenSSL_add_all_algorithms();
    context_ = std::make_unique<CipherContext>();
    BOOST_LOG_TRIVIAL(debug) << "CryptoStream initialization complete";
}

CryptoStream::~CryptoStream() {
    BOOST_LOG_TRIVIAL(debug) << "Cleaning up CryptoStream resources";
    EVP_cleanup();
}

void CryptoStream::initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    BOOST_LOG_TRIVIAL(info) << "Initializing crypto parameters";

    if (key.size() != KEY_SIZE) {
        BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << key.size() << " bytes (expected " << KEY_SIZE << " bytes)";
        throw InitializationError("Invalid key size");
    }

    if (iv.size() != IV_SIZE) {
        BOOST_LOG_TRIVIAL(error) << "Invalid IV size: " << iv.size() << " bytes (expected " << IV_SIZE << " bytes)";
        throw InitializationError("Invalid IV size");
    }

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

    // Create a new context for clean state
    context_ = std::make_unique<CipherContext>();

    // Initialize the cipher operation
    if (encrypting) {
        if (!EVP_EncryptInit_ex(context_->get(), EVP_aes_256_cbc(), nullptr, key_.data(), iv_.data())) {
            BOOST_LOG_TRIVIAL(error) << "Failed to initialize encryption";
            throw EncryptionError("Failed to initialize encryption");
        }
    } else {
        if (!EVP_DecryptInit_ex(context_->get(), EVP_aes_256_cbc(), nullptr, key_.data(), iv_.data())) {
            BOOST_LOG_TRIVIAL(error) << "Failed to initialize decryption";
            throw DecryptionError("Failed to initialize decryption");
        }
    }

    // Enable padding
    EVP_CIPHER_CTX_set_padding(context_->get(), 1);
    BOOST_LOG_TRIVIAL(debug) << "Cipher initialization complete";
}

void CryptoStream::processStream(std::istream& input, std::ostream& output, bool encrypting) {
    BOOST_LOG_TRIVIAL(info) << "Starting stream " << (encrypting ? "encryption" : "decryption");

    if (!input.good() || !output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid stream state detected";
        throw std::runtime_error("Invalid stream state");
    }

    try {
        // Initialize cipher
        initializeCipher(encrypting);

        // Save input stream position and get total size
        auto initial_pos = input.tellg();
        input.seekg(0, std::ios::end);
        auto total_size = input.tellg() - initial_pos;
        input.seekg(initial_pos);

        // Use buffer size that's a multiple of AES block size (16 bytes)
        static constexpr size_t BUFFER_SIZE = 16 * 512; // 8KB buffer
        std::vector<uint8_t> inbuf(BUFFER_SIZE);
        std::vector<uint8_t> outbuf(BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH);
        int outlen = 0;

        // Process all complete blocks
        while (total_size > 0) {
            auto block_size = std::min(static_cast<std::streamsize>(BUFFER_SIZE), total_size);
            input.read(reinterpret_cast<char*>(inbuf.data()), block_size);

            if (!input.good() && !input.eof()) {
                throw std::runtime_error("Failed to read from input stream");
            }

            // Process block
            if (encrypting) {
                if (!EVP_EncryptUpdate(context_->get(), outbuf.data(), &outlen,
                                     inbuf.data(), static_cast<int>(block_size))) {
                    unsigned long err = ERR_get_error();
                    char err_msg[256];
                    ERR_error_string_n(err, err_msg, sizeof(err_msg));
                    BOOST_LOG_TRIVIAL(error) << "Encryption update failed: " << err_msg;
                    throw EncryptionError("Failed to encrypt data block");
                }
            } else {
                if (!EVP_DecryptUpdate(context_->get(), outbuf.data(), &outlen,
                                     inbuf.data(), static_cast<int>(block_size))) {
                    unsigned long err = ERR_get_error();
                    char err_msg[256];
                    ERR_error_string_n(err, err_msg, sizeof(err_msg));
                    BOOST_LOG_TRIVIAL(error) << "Decryption update failed: " << err_msg;
                    throw DecryptionError("Failed to decrypt data block");
                }
            }

            if (outlen > 0) {
                output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
                if (!output.good()) {
                    throw std::runtime_error("Failed to write to output stream");
                }
            }

            total_size -= block_size;
        }

        // Handle final block with padding
        int final_len = 0;
        if (encrypting) {
            if (!EVP_EncryptFinal_ex(context_->get(), outbuf.data(), &final_len)) {
                unsigned long err = ERR_get_error();
                char err_msg[256];
                ERR_error_string_n(err, err_msg, sizeof(err_msg));
                BOOST_LOG_TRIVIAL(error) << "Encryption finalization failed: " << err_msg;
                throw EncryptionError("Failed to finalize encryption");
            }
        } else {
            if (!EVP_DecryptFinal_ex(context_->get(), outbuf.data(), &final_len)) {
                unsigned long err = ERR_get_error();
                char err_msg[256];
                ERR_error_string_n(err, err_msg, sizeof(err_msg));
                BOOST_LOG_TRIVIAL(error) << "Decryption finalization failed: " << err_msg;
                throw DecryptionError("Failed to finalize decryption");
            }
        }

        // Write final block if any
        if (final_len > 0) {
            output.write(reinterpret_cast<char*>(outbuf.data()), final_len);
            if (!output.good()) {
                throw std::runtime_error("Failed to write final block");
            }
        }

        // Ensure all data is written
        output.flush();

    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error during stream processing: " << e.what();
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

CryptoStream& CryptoStream::operator>>(std::ostream& output) {
    BOOST_LOG_TRIVIAL(debug) << "Stream operator>> called in " 
                            << (mode_ == Mode::Encrypt ? "encryption" : "decryption") << " mode";

    if (!pending_input_) {
        BOOST_LOG_TRIVIAL(error) << "Stream operator>> called without prior input stream";
        throw std::runtime_error("No pending input stream. Use operator<< first.");
    }

    if (mode_ == Mode::Encrypt) {
        encrypt(*pending_input_, output);
    } else {
        decrypt(*pending_input_, output);
    }

    pending_input_ = nullptr;
    BOOST_LOG_TRIVIAL(debug) << "Stream processing complete";

    return *this;
}

CryptoStream& CryptoStream::operator<<(std::istream& input) {
    BOOST_LOG_TRIVIAL(debug) << "Stream operator<< called, storing input stream for processing";
    pending_input_ = &input;
    return *this;
}

std::array<uint8_t, CryptoStream::IV_SIZE> CryptoStream::generate_IV() const {
    BOOST_LOG_TRIVIAL(debug) << "Generating initialization vector";

    std::array<uint8_t, IV_SIZE> iv;
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        BOOST_LOG_TRIVIAL(error) << "Failed to generate random IV";
        throw std::runtime_error("Failed to generate secure random IV");
    }

    BOOST_LOG_TRIVIAL(debug) << "Successfully generated initialization vector";
    return iv;
}

} // namespace dfs::crypto