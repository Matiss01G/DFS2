#include "store/store.hpp"
#include <iomanip>
#include <boost/log/trivial.hpp>
#include <thread>

namespace dfs {
namespace store {

// Initialize store with base directory path and ensure it exists
Store::Store(const std::string& base_path) : base_path_(base_path) {
  BOOST_LOG_TRIVIAL(info) << "Store: Initializing Store with base path: " << base_path;
  ensure_directory_exists(base_path_); // Create base directory if it doesn't exist
  BOOST_LOG_TRIVIAL(debug) << "Store: Store directory created/verified at: " << base_path;
}

void Store::store(const std::string& key, std::istream& data) {
  BOOST_LOG_TRIVIAL(info) << "Store: Storing data with key: " << key;
  
  // Check stream state before attempting any operations
  if (!data.good()) {
    BOOST_LOG_TRIVIAL(error) << "Store: Invalid input stream provided for key: " << key;
    throw StoreError("Store: Invalid input stream");
  }
  
  // Create hierarchical directory structure based on key hash
  std::string hash = hash_key(key);  // Generate SHA-256 hash from key
  std::filesystem::path file_path = get_path_for_hash(hash);  // Convert hash to path
  ensure_directory_exists(file_path.parent_path());  // Create directories if needed
  
  BOOST_LOG_TRIVIAL(debug) << "Store: Calculated file path: " << file_path.string();
  
  // Create output file in binary mode for cross-platform compatibility
  std::ofstream file(file_path, std::ios::binary);
  if (!file) {
    throw StoreError("Store: Failed to create file: " + file_path.string());
  }
  
  // Stream data in chunks for memory effi    ciency
  size_t bytes_written = 0;
  char buffer[4096];  // 4KB chunks balance memory usage and I/O performance
  
  // Handle empty stream case
  data.peek();
  if (data.eof()) {
    BOOST_LOG_TRIVIAL(debug) << "Store: Storing empty content for key: " << key;
    file.close();
    BOOST_LOG_TRIVIAL(info) << "Store: Successfully stored 0 bytes with key: " << key;
    return;
  }

  const auto TIMER = std::chrono::milliseconds(500);

  while (true) {
     if (data.read(buffer, sizeof(buffer))) {
       file.write(buffer, data.gcount());
       bytes_written += data.gcount();
       continue;
     }

     if (data.eof()) {
         data.clear();
         if (data.peek() == EOF) break;
     }
  }

  // Handle final partial chunk if any
  if (data.gcount() > 0) {
    file.write(buffer, data.gcount());
    bytes_written += data.gcount();
  }
  
  file.close();
  BOOST_LOG_TRIVIAL(info) << "Store: Successfully stored " << bytes_written << " bytes with key: " << key;
}

void Store::get(const std::string& key, std::stringstream& output) {
  BOOST_LOG_TRIVIAL(info) << "Store: Retrieving data for key: " << key;

  // Generate file path and verify existence
  std::string hash = hash_key(key);
  std::filesystem::path file_path = get_path_for_hash(hash);
  if (!std::filesystem::exists(file_path)) {
    BOOST_LOG_TRIVIAL(error) << "Store: File not found: " << file_path.string();
    throw StoreError("Store: File not found");
  }

  // Handle empty file case
  if (std::filesystem::file_size(file_path) == 0) {
    BOOST_LOG_TRIVIAL(debug) << "Store: Retrieved empty content for key: " << key;
    return;
  }

  // Open file in binary mode
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    throw StoreError("Store: Failed to open file: " + file_path.string());
  }

  // Stream data in chunks for memory efficiency
  char buffer[4096];  // 4KB chunks for balanced memory usage and I/O performance
  size_t total_bytes = 0;

  while (file.read(buffer, sizeof(buffer))) {
    output.write(buffer, file.gcount());
    total_bytes += file.gcount();
  }
  // Handle final partial chunk if any
  if (file.gcount() > 0) {
    output.write(buffer, file.gcount());
    total_bytes += file.gcount();
  }

  // Verify stream state
  if (!output.good()) {
    throw StoreError("Store: Failed to write to output stream");
  }

  BOOST_LOG_TRIVIAL(info) << "Store: Successfully streamed " << total_bytes << " bytes for key: " << key;
}

bool Store::has(const std::string& key) const {
  BOOST_LOG_TRIVIAL(debug) << "Store: Checking existence of key: " << key;
  std::string hash = hash_key(key);
  std::filesystem::path file_path = get_path_for_hash(hash);
  bool exists = std::filesystem::exists(file_path);
  BOOST_LOG_TRIVIAL(debug) << "Store: Key " << key << (exists ? " exists" : " not found") 
              << " at path: " << file_path.string();
  return exists;
}

std::uintmax_t Store::get_file_size(const std::string& key) const {
  BOOST_LOG_TRIVIAL(debug) << "Store: Getting file size for key: " << key;

  std::string hash = hash_key(key);
  std::filesystem::path file_path = get_path_for_hash(hash);

  if (!std::filesystem::exists(file_path)) {
    BOOST_LOG_TRIVIAL(error) << "Store: File not found: " << file_path.string();
    throw StoreError("Store: File not found");
  }

  std::uintmax_t size = std::filesystem::file_size(file_path);
  BOOST_LOG_TRIVIAL(debug) << "Store: File size for key " << key << ": " << size << " bytes";
  return size;
}

void Store::remove(const std::string& key) {
  BOOST_LOG_TRIVIAL(info) << "Store: Removing file with key: " << key;

  std::string hash = hash_key(key);
  std::filesystem::path file_path = get_path_for_hash(hash);

  if (std::filesystem::remove(file_path)) {
    BOOST_LOG_TRIVIAL(info) << "Store: Successfully removed file with key: " << key;
  } else {
    BOOST_LOG_TRIVIAL(error) << "Store: Failed to remove file with key: " << key;
    throw StoreError("Store: Failed to remove file");
  }
}

void Store::clear() {
  BOOST_LOG_TRIVIAL(info) << "Store: Clearing entire store at: " << base_path_;
  std::filesystem::remove_all(base_path_);
  ensure_directory_exists(base_path_);
  BOOST_LOG_TRIVIAL(info) << "Store: Store cleared successfully";
}

std::string Store::hash_key(const std::string& key) const {
  BOOST_LOG_TRIVIAL(debug) << "Store: Generating hash for key: " << key;

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    throw StoreError("Store: Failed to create hash context");
  }

  if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
    EVP_MD_CTX_free(ctx);
    throw StoreError("Store: Failed to initialize hash context");
  }

  if (!EVP_DigestUpdate(ctx, key.c_str(), key.length())) {
    EVP_MD_CTX_free(ctx);
    throw StoreError("Store: Failed to update hash");
  }

  if (!EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
    EVP_MD_CTX_free(ctx);
    throw StoreError("Store: Failed to finalize hash");
  }

  EVP_MD_CTX_free(ctx);

  std::stringstream ss;
  for (unsigned int i = 0; i < hash_len; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') 
       << static_cast<int>(hash[i]);
  }

  std::string result = ss.str();
  BOOST_LOG_TRIVIAL(debug) << "Store: Generated hash: " << result;
  return result;
}

std::filesystem::path Store::get_path_for_hash(const std::string& hash) const {
  std::filesystem::path path = base_path_;
  for (size_t i = 0; i < 6; i += 2) {
    path /= hash.substr(i, 2);
  }
  path /= hash.substr(6);
  BOOST_LOG_TRIVIAL(debug) << "Store: Calculated path: " << path.string();
  return path;
}

void Store::ensure_directory_exists(const std::filesystem::path& path) const {
  if (!std::filesystem::exists(path)) {
    std::filesystem::create_directories(path);
  }
}

} // namespace store
} // namespace dfs