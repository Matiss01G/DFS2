#ifndef DFS_CRYPTO_STREAM_HPP
#define DFS_CRYPTO_STREAM_HPP

#include <istream>
#include <ostream>
#include <vector>
#include <memory>
#include <array>
#include <functional>
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

  // ---- CONSTRUCTOR AND DESTRUCTOR ----
  CryptoStream();
  ~CryptoStream();

  // Generate an initialization vector
  std::array<uint8_t, IV_SIZE> generate_IV() const;
  
  // ---- INITIALIZATION ----
  // Initializes crypto_stream parameters
  void initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv);

  
  // ---- ENCRYPTION/DECRYPTION OPERATIONS ----
  std::ostream& encrypt(std::istream& input, std::ostream& output);
  std::ostream& decrypt(std::istream& input, std::ostream& output);

  
  // ---- GETTERS/SETTERS ----
  void setMode(Mode mode) { mode_ = mode; }
  Mode getMode() const { return mode_; }

private:
  // ---- PARAMETERS ----
  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
  std::unique_ptr<CipherContext> context_;
  bool is_initialized_ = false;
  Mode mode_ = Mode::Encrypt;  // Default to encryption mode
  std::istream* pending_input_ = nullptr;  
  static constexpr size_t BUFFER_SIZE = 8192; 

  
  // ---- INITIALIZATION ----  
  // Initializes cipher context
  void initializeCipher(bool encrypting);


  // ---- STREAM PROCESSING - ENCRYPTION/DECRYPTION ----
  // Process data through OpenSSL cipher context
  void processStream(std::istream& input, std::ostream& output, bool encrypting);
  // Manages stream positions and handles automatic position restoration on errors
  void saveStreamPos(std::istream& input, std::ostream& output, 
                                   const std::function<void()>& operation);
  // Performs the main encryption/decryption loop on the input stream
  void processStreamData(std::istream& input, std::ostream& output, bool encrypting);
  // Encrypts or decrypts a single block of data using the configured cipher
  size_t processDataBlock(const uint8_t* inbuf, size_t bytes_read, uint8_t* outbuf, 
                        bool encrypting);
  // Safely writes a block of processed data to the output stream
  void writeOutputBlock(std::ostream& output, const uint8_t* data, size_t length);
  // Handles the final block with padding in encryption/decryption operations
  void processFinalBlock(uint8_t* outbuf, int& outlen, bool encrypting);
};
  
} // namespace dfs::crypto

#endif // DFS_CRYPTO_STREAM_HPP