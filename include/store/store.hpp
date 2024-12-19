#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <istream>
#include <ostream>
#include "crypto/crypto_error.hpp"
#include <openssl/sha.h>

namespace dfs {
namespace store {

class Store {
public:
    explicit Store(const std::filesystem::path& base_path);
    
    // Store data from input stream using content-addressable storage
    void store(const std::string& file_key, std::istream& input_stream);
    
    // Get data as input stream using file key
    // Returns a unique pointer to the input stream for reading stored data
    std::unique_ptr<std::istream> get(const std::string& file_key);
    
    // Check if file exists
    bool has(const std::string& file_key) const;
    
    // Remove file by key
    void remove(const std::string& file_key);
    
    // Clear all files from store
    void clear();

private:
    // Hash file key for content-addressable storage
    std::string hash_key(const std::string& file_key) const;
    
    // Get filesystem path for file key
    std::filesystem::path get_file_path(const std::string& file_key) const;
    
    std::filesystem::path base_path_;
};

} // namespace store
} // namespace dfs
