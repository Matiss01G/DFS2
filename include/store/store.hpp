#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <openssl/sha.h>
#include "crypto_stream.hpp"
#include "crypto_error.hpp"

namespace dfs {
namespace crypto {

class Store {
public:
    Store(const std::filesystem::path& base_path);
    
    // Stores data from stream using content-addressable storage
    void store(const std::string& file_key, std::unique_ptr<CryptoStream> data_stream);
    
    // Retrieves data as a stream
    std::unique_ptr<CryptoStream> get(const std::string& file_key);
    
    // Checks if file exists in store
    bool has(const std::string& file_key) const;
    
    // Deletes file from store
    void remove(const std::string& file_key);
    
    // Clears all files from store
    void clear();

private:
    std::filesystem::path base_path_;
    
    // Hashes file key to get content-addressable path
    std::string hash_key(const std::string& file_key) const;
    
    // Gets full path for a given file key
    std::filesystem::path get_file_path(const std::string& file_key) const;
};

} // namespace crypto
} // namespace dfs
