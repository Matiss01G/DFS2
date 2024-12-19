#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include "../logger/logger.hpp"

namespace dfs {

/**
 * @brief Content-addressable storage implementation for distributed file system
 * 
 * Store provides a hierarchical storage system where files are stored and retrieved
 * using content-based addressing. File keys are hashed using SHA-256 and the resulting
 * hash is used to create a directory structure that evenly distributes files.
 */
class Store {
public:
    /**
     * @brief Construct a new Store object
     * @param base_path Root directory for all stored files
     */
    explicit Store(const std::string& base_path);
    
    /**
     * @brief Store data stream under given key
     * @param key Unique identifier for the data
     * @param data Input stream containing data to store
     * @throws StoreError if stream is invalid or storage fails
     */
    void store(const std::string& key, std::istream& data);
    
    /**
     * @brief Retrieve data stream for given key
     * @param key Unique identifier for the data
     * @return std::unique_ptr<std::stringstream> Data stream
     * @throws StoreError if file not found or cannot be opened
     */
    std::unique_ptr<std::stringstream> get(const std::string& key);
    
    /**
     * @brief Check if data exists for given key
     * @param key Unique identifier to check
     * @return true if data exists, false otherwise
     */
    bool has(const std::string& key) const;
    
    /**
     * @brief Remove data associated with given key
     * @param key Unique identifier for data to remove
     * @throws StoreError if removal fails
     */
    void remove(const std::string& key);
    
    /**
     * @brief Remove all stored data and reset store
     */
    void clear();

private:
    // Root path for all stored files
    std::filesystem::path base_path_;
    
    /**
     * @brief Generate SHA-256 hash from key using OpenSSL EVP
     * @param key Input key to hash
     * @return string Hex representation of hash
     * @throws StoreError if hashing fails
     */
    std::string hash_key(const std::string& key) const;
    
    /**
     * @brief Convert hash to hierarchical path for even distribution
     * Creates a directory structure using parts of the hash:
     * {base_path}/{hash[0:2]}/{hash[2:4]}/{hash[4:6]}/{remaining_hash}
     */
    std::filesystem::path get_path_for_hash(const std::string& hash) const;
    
    /**
     * @brief Ensure directory exists, create if needed
     * @param path Directory path to verify/create
     */
    void ensure_directory_exists(const std::filesystem::path& path) const;
};

/**
 * @brief Custom exception for Store operations
 */
class StoreError : public std::runtime_error {
public:
    explicit StoreError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace dfs
