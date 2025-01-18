#include "store/store.hpp"
#include <iomanip>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace store {

// Initialize store with base directory path and ensure it exists
Store::Store(const std::string& base_path) : base_path_(base_path) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Store with base path: " << base_path;
    ensure_directory_exists(base_path_); // Create base directory if it doesn't exist
    BOOST_LOG_TRIVIAL(debug) << "Store directory created/verified at: " << base_path;
}

void Store::store(const std::string& key, std::istream& data) {
    BOOST_LOG_TRIVIAL(info) << "Storing data with key: " << key;

    // Check stream state before attempting any operations
    if (!data.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream provided for key: " << key;
        throw StoreError("Invalid input stream");
    }

    // Create hierarchical directory structure based on key hash
    std::string hash = hash_key(key);  // Generate SHA-256 hash from key
    std::filesystem::path file_path = get_path_for_hash(hash);  // Convert hash to path
    ensure_directory_exists(file_path.parent_path());  // Create directories if needed

    BOOST_LOG_TRIVIAL(debug) << "Calculated file path: " << file_path.string();

    // Create output file in binary mode for cross-platform compatibility
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        throw StoreError("Failed to create file: " + file_path.string());
    }

    // Stream data in chunks for memory efficiency
    size_t bytes_written = 0;
    char buffer[4096];  // 4KB chunks balance memory usage and I/O performance

    // Handle empty stream case
    data.peek();
    if (data.eof()) {
        BOOST_LOG_TRIVIAL(debug) << "Storing empty content for key: " << key;
        file.close();
        BOOST_LOG_TRIVIAL(info) << "Successfully stored 0 bytes with key: " << key;
        return;
    }

    while (data.read(buffer, sizeof(buffer))) {  // Read full chunks
        file.write(buffer, data.gcount());
        bytes_written += data.gcount();
    }
    // Handle final partial chunk if any
    if (data.gcount() > 0) {
        file.write(buffer, data.gcount());
        bytes_written += data.gcount();
    }

    file.close();
    BOOST_LOG_TRIVIAL(info) << "Successfully stored " << bytes_written << " bytes with key: " << key;
}

std::shared_ptr<std::stringstream> Store::get(const std::string& key) {
    BOOST_LOG_TRIVIAL(info) << "Retrieving data for key: " << key;

    // Generate file path and verify existence
    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);
    if (!std::filesystem::exists(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "File not found: " << file_path.string();
        throw StoreError("File not found");
    }

    auto output = std::make_shared<std::stringstream>();

    // Handle empty file case
    if (std::filesystem::file_size(file_path) == 0) {
        BOOST_LOG_TRIVIAL(debug) << "Retrieved empty content for key: " << key;
        return output;
    }

    // Open file in binary mode and stream content
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw StoreError("Failed to open file: " + file_path.string());
    }

    // Copy file contents to output stream
    *output << file.rdbuf();

    // Verify stream state and reset position
    if (!output->good()) {
        throw StoreError("Failed to read file contents");
    }
    output->seekg(0);  // Reset read position to beginning

    BOOST_LOG_TRIVIAL(debug) << "Successfully retrieved data for key: " << key;
    return output;
}

bool Store::has(const std::string& key) const {
    BOOST_LOG_TRIVIAL(debug) << "Checking existence of key: " << key;
    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);
    bool exists = std::filesystem::exists(file_path);
    BOOST_LOG_TRIVIAL(debug) << "Key " << key << (exists ? " exists" : " not found") 
                            << " at path: " << file_path.string();
    return exists;
}

std::uintmax_t Store::get_file_size(const std::string& key) const {
    BOOST_LOG_TRIVIAL(debug) << "Getting file size for key: " << key;

    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);

    if (!std::filesystem::exists(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "File not found: " << file_path.string();
        throw StoreError("File not found");
    }

    std::uintmax_t size = std::filesystem::file_size(file_path);
    BOOST_LOG_TRIVIAL(debug) << "File size for key " << key << ": " << size << " bytes";
    return size;
}

void Store::remove(const std::string& key) {
    BOOST_LOG_TRIVIAL(info) << "Removing file with key: " << key;

    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);

    if (std::filesystem::remove(file_path)) {
        BOOST_LOG_TRIVIAL(info) << "Successfully removed file with key: " << key;
    } else {
        BOOST_LOG_TRIVIAL(error) << "Failed to remove file with key: " << key;
        throw StoreError("Failed to remove file");
    }
}

void Store::clear() {
    BOOST_LOG_TRIVIAL(info) << "Clearing entire store at: " << base_path_;
    std::filesystem::remove_all(base_path_);
    ensure_directory_exists(base_path_);
    BOOST_LOG_TRIVIAL(info) << "Store cleared successfully";
}

std::string Store::hash_key(const std::string& key) const {
    BOOST_LOG_TRIVIAL(debug) << "Generating hash for key: " << key;

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw StoreError("Failed to create hash context");
    }

    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
        EVP_MD_CTX_free(ctx);
        throw StoreError("Failed to initialize hash context");
    }

    if (!EVP_DigestUpdate(ctx, key.c_str(), key.length())) {
        EVP_MD_CTX_free(ctx);
        throw StoreError("Failed to update hash");
    }

    if (!EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        EVP_MD_CTX_free(ctx);
        throw StoreError("Failed to finalize hash");
    }

    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(hash[i]);
    }

    std::string result = ss.str();
    BOOST_LOG_TRIVIAL(debug) << "Generated hash: " << result;
    return result;
}

std::filesystem::path Store::get_path_for_hash(const std::string& hash) const {
    std::filesystem::path path = base_path_;
    for (size_t i = 0; i < 6; i += 2) {
        path /= hash.substr(i, 2);
    }
    path /= hash.substr(6);
    BOOST_LOG_TRIVIAL(debug) << "Calculated path: " << path.string();
    return path;
}

void Store::ensure_directory_exists(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
}

} // namespace store
} // namespace dfs