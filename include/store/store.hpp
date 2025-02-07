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

  // ---- CONSTRUCTOR AND DESTRUCTOR ----
  explicit Store(const std::string& base_path);


  // ---- CORE STORAGE OPERATIONS ----
  // stores data stream under given key
  void store(const std::string& key, std::istream& data);
  // Retrieves data stream using given key
  void get(const std::string& key, std::stringstream& output);
  // Removes data associated with given key
  void remove(const std::string& key);
  // Removes all stored data and reset store
  void clear();


  // ---- QUERY OPERATIONS ----
  // Checks if data exists using given key
  bool has(const std::string& key) const;
  // Returns the size of the stored file in bytes
  std::uintmax_t get_file_size(const std::string& key) const;


  // ---- CLI COMMAND SUPPORT ----
   bool read_file(const std::string& key, size_t lines_per_page) const;
  void print_working_dir() const;
  void list() const;
  void move_dir(const std::string& path);
  void delete_file(const std::string& filename);

private:
  // ---- PARAMETERS ----
  // Root path for all stored files
  std::filesystem::path base_path_;

  
  // ---- CLI COMMAND SUPPORT ----
  bool display_file_contents(std::ifstream& file, const std::string& key, 
    size_t lines_per_page) const;

  
  // ---- CAS STORAGE SUPPORT ----
  // Generate SHA-256 hash from key using OpenSSL EVP
  std::string hash_key(const std::string& key) const;
  // Creates a directory structure using parts of the hash:
  // {base_path}/{hash[0:2]}/{hash[2:4]}/{hash[4:6]}/{remaining_hash}
  std::filesystem::path get_path_for_hash(const std::string& hash) const;

  
  // ---- QUERY OPERATIONS ----
  // Ensures directory exists, create if needed
  void check_directory_exists(const std::filesystem::path& path) const;
  // Resolves a key to its corresponding filesystem path by generating hash and converting to path
  std::filesystem::path resolve_key_path(const std::string& key) const;
  // Verifies if a file exists at the given path, throws StoreError if not found
  void verify_file_exists(const std::filesystem::path& file_path) const;
};

class StoreError : public std::runtime_error {
public:
  explicit StoreError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace store
} // namespace dfs