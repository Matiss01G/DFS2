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

bool Store::read_file(const std::string& key, size_t lines_per_page) const {
    BOOST_LOG_TRIVIAL(info) << "Store: Reading file with key: " << key;
    try {
        std::string hash = hash_key(key);
        std::filesystem::path file_path = get_path_for_hash(hash);

        if (!std::filesystem::exists(file_path)) {
            BOOST_LOG_TRIVIAL(error) << "Store: File not found: " << file_path.string();
            return false;
        }

        if (std::filesystem::file_size(file_path) == 0) {
            BOOST_LOG_TRIVIAL(debug) << "Store: File is empty for key: " << key;
            return true;  // Empty file is still considered successful read
        }

        std::ifstream file(file_path);
        if (!file) {
            BOOST_LOG_TRIVIAL(error) << "Store: Failed to open file: " << file_path.string();
            return false;
        }

        std::string line;
        size_t current_line = 0;
        bool continue_reading = true;

        while (continue_reading && !file.eof()) {
            #ifdef _WIN32
            if (system("cls") != 0) {
                BOOST_LOG_TRIVIAL(error) << "Store: Failed to clear screen";
                // Continue execution despite screen clear failure
            }
            #else
            if (system("clear") != 0) {
                BOOST_LOG_TRIVIAL(error) << "Store: Failed to clear screen";
                // Continue execution despite screen clear failure
            }
            #endif

            for (size_t i = 0; i < lines_per_page && std::getline(file, line); ++i) {
                std::cout << line << '\n';
                ++current_line;
            }

            std::cout << "\n--Key: " << key << " - Line " << current_line 
                     << " (Press Enter to continue, 'q' to quit)--" << std::flush;
            char input = std::cin.get();
            if (input == 'q' || input == 'Q') {
                continue_reading = false;
            }
            std::cin.clear();
            while (std::cin.get() != '\n') {}
        }

        BOOST_LOG_TRIVIAL(info) << "Store: Finished reading file with key: " << key;
        return true;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Store: Exception while reading file: " << e.what();
        return false;
    }
}

void Store::print_working_dir() const {
    std::cout << "\nLocal working directory: " << std::filesystem::current_path() << std::endl;
    std::cout << "DFS store directory: " << base_path_.filename() << "\n"<< std::endl;
}
  
void Store::list() const {
    BOOST_LOG_TRIVIAL(info) << "Store: Listing contents";

    std::cout << "Local Files:" << std::endl;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
        std::cout << "  " << (entry.is_directory() ? "[DIR] " : "[FILE]") 
                  << " " << entry.path().filename() << std::endl;
    }

    std::cout << "\nDFS Store:" << std::endl;
    for (const auto& entry : std::filesystem::directory_iterator(base_path_)) {
        std::cout << "* " << (entry.is_directory() ? "[DIR] " : "[FILE]") 
                  << " " << entry.path().filename() << std::endl;
    }
}

void Store::move_dir(const std::string& path) {
    BOOST_LOG_TRIVIAL(info) << "Store: Changing DFS directory to: " << path;

    std::filesystem::path new_path;
    // Check if path is relative or absolute
    if (std::filesystem::path(path).is_absolute()) {
        new_path = path;
    } else {
        new_path = base_path_ / path;
    }

    try {
        if (!std::filesystem::exists(new_path)) {
            BOOST_LOG_TRIVIAL(error) << "Store: DFS directory does not exist: " << path;
            throw StoreError("Store: DFS directory does not exist");
        }

        if (!std::filesystem::is_directory(new_path)) {
            BOOST_LOG_TRIVIAL(error) << "Store: DFS path exists but is not a directory: " << path;
            throw StoreError("Store: DFS path is not a directory");
        }

        base_path_ = new_path;
        BOOST_LOG_TRIVIAL(info) << "Store: Successfully changed DFS directory to: " << base_path_;

    } catch (const std::filesystem::filesystem_error& e) {
        BOOST_LOG_TRIVIAL(error) << "Store: Failed to change DFS directory: " << e.what();
        throw StoreError("Store: Failed to change DFS directory: " + std::string(e.what()));
    }
}

void Store::delete_file(const std::string& filename) {
    BOOST_LOG_TRIVIAL(info) << "Store: Deleting file: " << filename;

    std::string hash = hash_key(filename);
    std::filesystem::path file_path = get_path_for_hash(hash);

    if (!std::filesystem::exists(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "Store: File not found: " << file_path.string();
        throw StoreError("Store: File not found");
    }

    // Delete the file
    if (!std::filesystem::remove(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "Store: Failed to delete file: " << filename;
        throw StoreError("Store: Failed to delete file");
    }

    // Clean up empty parent directories up to base_path_
    auto current = file_path.parent_path();
    while (current != base_path_) {
        if (std::filesystem::is_empty(current)) {
            std::filesystem::remove(current);
            current = current.parent_path();
        } else {
            break;
        }
    }

    BOOST_LOG_TRIVIAL(info) << "Store: Successfully deleted file and cleaned up directories: " << filename;
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
  
  // Stream data in chunks for memory efficiency
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

  while (data.read(buffer, sizeof(buffer))) {
    file.write(buffer, data.gcount());
    bytes_written += data.gcount();
  }

  // Handle final partial chunk
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