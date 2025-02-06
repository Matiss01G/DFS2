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
namespace store {

class Store {
public:

    explicit Store(const std::string& base_path);

    // stores data stream under given key
    void store(const std::string& key, std::istream& data);

    // Retrieves data stream using given key
    void get(const std::string& key, std::stringstream& output);
    
    // Checks if data exists using given key
    bool has(const std::string& key) const;

    // Returns the size of the stored file in bytes
    std::uintmax_t get_file_size(const std::string& key) const;

    // Removes data associated with given key
    void remove(const std::string& key);

    // Removes all stored data and reset store
    void clear();

    bool read_file(const std::string& key, size_t lines_per_page = 20) const;
    void print_working_dir() const;
    void list() const;
    void move_dir(const std::string& path);
    void delete_file(const std::string& filename);

private:
    // Root path for all stored files
    std::filesystem::path base_path_;

    // Generate SHA-256 hash from key using OpenSSL EVP
    std::string hash_key(const std::string& key) const;

    // Creates a directory structure using parts of the hash:
    // {base_path}/{hash[0:2]}/{hash[2:4]}/{hash[4:6]}/{remaining_hash}
    std::filesystem::path get_path_for_hash(const std::string& hash) const;

    // Ensures directory exists, create if needed
    void ensure_directory_exists(const std::filesystem::path& path) const;
};

class StoreError : public std::runtime_error {
public:
    explicit StoreError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace store
} // namespace dfs