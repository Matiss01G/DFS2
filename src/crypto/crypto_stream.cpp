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
            throw std::runtime_error("Crypto stream: Failed to create cipher context");
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
    BOOST_LOG_TRIVIAL(info) << "Crypto stream: Initializing CryptoStream";
    OpenSSL_add_all_algorithms();
    context_ = std::make_unique<CipherContext>();
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: initialization complete";
}

CryptoStream::~CryptoStream() {
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Cleaning up CryptoStream resources";
    EVP_cleanup();
}

void CryptoStream::initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    BOOST_LOG_TRIVIAL(info) << "Crypto stream: Initializing crypto parameters";

    if (key.size() != KEY_SIZE) {
        BOOST_LOG_TRIVIAL(error) << "Crypto stream: Invalid key size: " << key.size() << " bytes (expected " << KEY_SIZE << " bytes)";
        throw InitializationError("Invalid key size");
    }

    key_ = key;
    iv_ = iv;
    is_initialized_ = true;
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Crypto parameters initialized successfully";
}

void CryptoStream::initializeCipher(bool encrypting) {
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Initializing cipher for " << (encrypting ? "encryption" : "decryption");

    if (!is_initialized_) {
        BOOST_LOG_TRIVIAL(error) << "Crypto stream: Attempted to use uninitialized CryptoStream";
        throw InitializationError("Crypto stream: CryptoStream not initialized");
    }

    // Reset the context state
    EVP_CIPHER_CTX_reset(context_->get());

    // Initialize cipher context
    const EVP_CIPHER* cipher = EVP_aes_256_cbc();
    if (encrypting) {
        if (!EVP_EncryptInit_ex(context_->get(), cipher, nullptr, key_.data(), iv_.data())) {
            BOOST_LOG_TRIVIAL(error) << "Crypto stream: Failed to initialize encryption context";
            throw EncryptionError("Crypto stream: Failed to initialize encryption");
        }
    } else {
        if (!EVP_DecryptInit_ex(context_->get(), cipher, nullptr, key_.data(), iv_.data())) {
            BOOST_LOG_TRIVIAL(error) << "Crypto stream: Failed to initialize decryption context";
            throw DecryptionError("Crypto stream: Failed to initialize decryption");
        }
    }

    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Cipher initialization complete";
}

void CryptoStream::processStream(std::istream& input, std::ostream& output, bool encrypting) {
    BOOST_LOG_TRIVIAL(info) << "Crypto stream: Starting stream " << (encrypting ? "encryption" : "decryption");

    if (!input.good() || !output.good()) {
        BOOST_LOG_TRIVIAL(error) << "Crypto stream: Invalid stream state detected";
        throw std::runtime_error("Crypto stream: Invalid stream state");
    }

    // Save stream positions
    auto input_pos = input.tellg();
    auto output_pos = output.tellp();

    try {
        initializeCipher(encrypting);

        // Use a reasonable buffer size for better performance
        static constexpr size_t BUFFER_SIZE = 8192;  // 8KB buffer
        std::array<uint8_t, BUFFER_SIZE> inbuf;
        std::array<uint8_t, BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH> outbuf;
        int outlen;
        size_t block_count = 0;
        size_t total_bytes_processed = 0;

        // Add end of stream marker for decryption
        static const std::string EOS_MARKER = "<<DFS_EOS>>";

        // Process the input stream in chunks
        while (input.good() && !input.eof()) {
            input.read(reinterpret_cast<char*>(inbuf.data()), inbuf.size());
            auto bytes_read = input.gcount();

            BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Processing block " << block_count 
                                    << ": Read " << bytes_read << " bytes"
                                    << " (total processed so far: " << total_bytes_processed << ")";

            if (bytes_read <= 0) {
                if (!input.eof()) {
                    throw std::runtime_error("Crypto stream: Failed to read from input stream");
                }
                BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Reached end of input stream after " 
                                        << block_count << " blocks";
                break;
            }

            // Process the chunk
            if (encrypting) {
                BOOST_LOG_TRIVIAL(trace) << "Crypto stream: Encrypting block " << block_count 
                                        << " of size " << bytes_read;
                if (!EVP_EncryptUpdate(context_->get(), outbuf.data(), &outlen,
                                      inbuf.data(), static_cast<int>(bytes_read))) {
                    throw EncryptionError("Crypto stream: Failed to encrypt data block");
                }
            } else {
                BOOST_LOG_TRIVIAL(trace) << "Crypto stream: Decrypting block " << block_count 
                                        << " of size " << bytes_read;
                if (!EVP_DecryptUpdate(context_->get(), outbuf.data(), &outlen,
                                      inbuf.data(), static_cast<int>(bytes_read))) {
                    throw DecryptionError("Crypto stream: Failed to decrypt data block");
                }
            }

            // Write processed data
            if (outlen > 0) {
                BOOST_LOG_TRIVIAL(debug) << "Crypto stream: "
                                        <<(encrypting ? "Encrypted" : "Decrypted")
                                        << " block " << block_count 
                                        << " produced " << outlen << " bytes";
                output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
                if (!output.good()) {
                    throw std::runtime_error("Crypto stream: Failed to write to output stream");
                }
                total_bytes_processed += outlen;
            }
            block_count++;
        }

        // Finalize the operation with padding
        BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Finalizing " << (encrypting ? "encryption" : "decryption")
                                << " after " << block_count << " blocks";
        if (encrypting) {
            if (!EVP_EncryptFinal_ex(context_->get(), outbuf.data(), &outlen)) {
                throw EncryptionError("Crypto stream: Failed to finalize encryption");
            }
        } else {
            if (!EVP_DecryptFinal_ex(context_->get(), outbuf.data(), &outlen)) {
                BOOST_LOG_TRIVIAL(error) << "Crypto stream: Failed to finalize decryption after processing "
                                        << total_bytes_processed << " bytes in "
                                        << block_count << " blocks";
                throw DecryptionError("Crypto stream: Failed to finalize decryption");
            }
        }

        // Write final block
        if (outlen > 0) {
            BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Writing final block of size " << outlen;
            output.write(reinterpret_cast<char*>(outbuf.data()), outlen);
            if (!output.good()) {
                throw std::runtime_error("Crypto stream: Failed to write final block");
            }
            total_bytes_processed += outlen;
        }

        // Write end of stream marker only for decryption
        if (!encrypting) {
            BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Writing end of stream marker";
            output.write(EOS_MARKER.c_str(), EOS_MARKER.length());
            if (!output.good()) {
                throw std::runtime_error("Crypto stream: Failed to write end of stream marker");
            }
        }

        BOOST_LOG_TRIVIAL(info) << "Crypto stream: Completed " << (encrypting ? "encryption" : "decryption")
                                << ": Processed " << total_bytes_processed 
                                << " bytes in " << block_count << " blocks";

        // Reset stream positions
        input.clear();
        input.seekg(input_pos);
        output.clear();
        output.seekp(output_pos);
        // Flush output
        output.flush();
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Crypto stream: Stream processing failed: " << e.what();
        // Restore stream positions on error
        input.clear();
        input.seekg(input_pos);
        output.clear();
        output.seekp(output_pos);
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
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Stream operator>> called in " << (mode_ == Mode::Encrypt ? "encryption" : "decryption") << " mode";

    if (!pending_input_) {
        BOOST_LOG_TRIVIAL(error) << "Crypto stream: Stream operator>> called without prior input stream";
        throw std::runtime_error("Crypto stream: No pending input stream. Use operator<< first.");
    }

    if (mode_ == Mode::Encrypt) {
        encrypt(*pending_input_, output);
    } else {
        decrypt(*pending_input_, output);
    }

    pending_input_ = nullptr;
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Stream processing complete";

    return *this;
}

CryptoStream& CryptoStream::operator<<(std::istream& input) {
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Stream operator<< called, storing input stream for processing";
    pending_input_ = &input;
    return *this;
}

std::array<uint8_t, CryptoStream::IV_SIZE> CryptoStream::generate_IV() const {
    BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Generating initialization vector";

    std::array<uint8_t, IV_SIZE> iv;
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        BOOST_LOG_TRIVIAL(error) << "Crypto stream: Failed to generate random IV";
        throw std::runtime_error("Crypto stream: Failed to generate secure random IV");
    }

    BOOST_LOG_TRIVIAL(debug) << "Successfully generated initialization vector";
    return iv;
}

} // namespace dfs::crypto