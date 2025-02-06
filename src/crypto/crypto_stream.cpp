#include "crypto/crypto_stream.hpp"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <array>
#include <stdexcept>
#include <boost/log/trivial.hpp>

namespace dfs::crypto {

//=================================================
// RAII wrapper to manage cipher context lifecycle
//=================================================

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

//==============================================
// Constructor and Destructor
//==============================================

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

//==============================================
// Crypto unit nitialization
//==============================================

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
    throw InitializationError("Crypto stream: CryptoStream not initialized");
  }

  // Reset the context state
  EVP_CIPHER_CTX_reset(context_->get());

  // Initialize cipher context
  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  if (encrypting) {
    if (!EVP_EncryptInit_ex(context_->get(), cipher, nullptr, key_.data(), iv_.data())) {
      throw EncryptionError("Crypto stream: Failed to initialize encryption context");
    }
  } else {
    if (!EVP_DecryptInit_ex(context_->get(), cipher, nullptr, key_.data(), iv_.data())) {
      throw DecryptionError("Crypto stream: Failed to initialize decryption context");
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Cipher initialization complete";
}

//==============================================
// Stream Processing - Encryption/Decryption
//==============================================

void CryptoStream::processStream(std::istream& input, std::ostream& output, bool encrypting) {
  BOOST_LOG_TRIVIAL(info) << "Crypto stream: Starting stream " << (encrypting ? "encryption" : "decryption");

  validateStreams(input, output);
  initializeCipher(encrypting);

  saveAndRestoreStreamPositions(input, output, [&]() {
    processStreamData(input, output, encrypting);
  });
}

  
void CryptoStream::validateStreams(const std::istream& input, const std::ostream& output) const {
  if (!input.good() || !output.good()) {
    throw std::runtime_error("Crypto stream: Invalid stream state");
  }
}

void CryptoStream::saveAndRestoreStreamPositions(std::istream& input, std::ostream& output, 
                                             const std::function<void()>& operation) {
  auto input_pos = input.tellg();
  auto output_pos = output.tellp();

  try {
      operation();
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

  // Reset stream positions
  input.clear();
  input.seekg(input_pos);
  output.clear();
  output.seekp(output_pos);
  output.flush();
}

void CryptoStream::processStreamData(std::istream& input, std::ostream& output, bool encrypting) {
  std::array<uint8_t, BUFFER_SIZE> inbuf;
  std::array<uint8_t, BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH> outbuf;
  size_t block_count = 0;
  size_t total_bytes_processed = 0;

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
      break;
    }

    // Process the chunk
    auto outlen = processDataBlock(inbuf.data(), bytes_read, outbuf.data(), encrypting);

    // Write processed data
    writeOutputBlock(output, outbuf.data(), outlen);
    total_bytes_processed += outlen;
    block_count++;
  }

  // Process final block with padding
  int final_outlen = 0;
  processFinalBlock(outbuf.data(), final_outlen, encrypting);
  writeOutputBlock(output, outbuf.data(), final_outlen);
  total_bytes_processed += final_outlen;

  BOOST_LOG_TRIVIAL(info) << "Crypto stream: Completed " << (encrypting ? "encryption" : "decryption")
                          << ": Processed " << total_bytes_processed 
                          << " bytes in " << block_count << " blocks";
}

size_t CryptoStream::processDataBlock(const uint8_t* inbuf, size_t bytes_read, uint8_t* outbuf, 
                                  bool encrypting) {
  int outlen;
  if (encrypting) {
      BOOST_LOG_TRIVIAL(trace) << "Crypto stream: Encrypting block of size " << bytes_read;
      if (!EVP_EncryptUpdate(context_->get(), outbuf, &outlen,
                            inbuf, static_cast<int>(bytes_read))) {
        throw EncryptionError("Crypto stream: Failed to encrypt data block");
      }
  } else {
      BOOST_LOG_TRIVIAL(trace) << "Crypto stream: Decrypting block of size " << bytes_read;
      if (!EVP_DecryptUpdate(context_->get(), outbuf, &outlen,
                           inbuf, static_cast<int>(bytes_read))) {
          throw DecryptionError("Crypto stream: Failed to decrypt data block");
      }
  }
  return outlen;
}

void CryptoStream::writeOutputBlock(std::ostream& output, const uint8_t* data, size_t length) {
  if (length > 0) {
    output.write(reinterpret_cast<const char*>(data), length);
    if (!output.good()) {
      throw std::runtime_error("Crypto stream: Failed to write to output stream");
    }
  }
}

void CryptoStream::processFinalBlock(uint8_t* outbuf, int& outlen, bool encrypting) {
  BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Finalizing " << (encrypting ? "encryption" : "decryption");

  if (encrypting) {
    if (!EVP_EncryptFinal_ex(context_->get(), outbuf, &outlen)) {
      throw EncryptionError("Crypto stream: Failed to finalize encryption");
    }
  } else {
    if (!EVP_DecryptFinal_ex(context_->get(), outbuf, &outlen)) {
      throw DecryptionError("Crypto stream: Failed to finalize decryption");
    }
  }
}

//==============================================
// Public encrypt/decrypt methods
//==============================================
  
std::ostream& CryptoStream::encrypt(std::istream& input, std::ostream& output) {
  processStream(input, output, true);
  return output;
}

std::ostream& CryptoStream::decrypt(std::istream& input, std::ostream& output) {
  processStream(input, output, false);
  return output;
}

//==============================================
// Public method IV generation
//==============================================

std::array<uint8_t, CryptoStream::IV_SIZE> CryptoStream::generate_IV() const {
  BOOST_LOG_TRIVIAL(debug) << "Crypto stream: Generating initialization vector";

  std::array<uint8_t, IV_SIZE> iv;
  if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
    throw std::runtime_error("Crypto stream: Failed to generate  random IV");
  }

  BOOST_LOG_TRIVIAL(debug) << "Successfully generated initialization vector";
  return iv;
}

} // namespace dfs::crypto