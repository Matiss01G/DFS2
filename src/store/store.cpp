#include "../../include/store/store.hpp"
#include <iomanip>
#include <boost/log/trivial.hpp>

namespace dfs {

// Initialize store with base directory path
Store::Store(const std::string& base_path) : base_path_(base_path) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Store with base path: " << base_path;
    ensure_directory_exists(base_path_);
    BOOST_LOG_TRIVIAL(debug) << "Store directory created/verified at: " << base_path;
}

void Store::store(const std::string& key, std::istream& data) {
    BOOST_LOG_TRIVIAL(info) << "Storing data with key: " << key;
    
    // Validate input stream before processing
    if (!data.good()) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream provided for key: " << key;
        throw StoreError("Invalid input stream");
    }

    // Generate file path from key hash
    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);
    ensure_directory_exists(file_path.parent_path());

    BOOST_LOG_TRIVIAL(debug) << "Calculated file path: " << file_path.string();
    
    // Open output file in binary mode
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        throw StoreError("Failed to create file: " + file_path.string());
    }

    // Copy data from input stream to file using buffer for efficiency
    size_t bytes_written = 0;
    char buffer[4096];  // 4KB buffer size for optimal I/O performance
    while (data.read(buffer, sizeof(buffer))) {
        file.write(buffer, data.gcount());
        bytes_written += data.gcount();
    }
    // Write remaining bytes from last partial buffer
    file.write(buffer, data.gcount());
    bytes_written += data.gcount();

    BOOST_LOG_TRIVIAL(info) << "Successfully stored " << bytes_written << " bytes with key: " << key;
}

std::unique_ptr<std::stringstream> Store::get(const std::string& key) {
    BOOST_LOG_TRIVIAL(info) << "Retrieving data for key: " << key;
    
    // Generate file path and verify existence
    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);
    
    if (!std::filesystem::exists(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "File not found: " << file_path.string();
        throw StoreError("File not found");
    }
    
    // Open file in binary mode and stream content
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw StoreError("Failed to open file: " + file_path.string());
    }
    
    // Create and return memory stream containing file data
    auto result = std::make_unique<std::stringstream>();
    *result << file.rdbuf();
    return result;
}

bool Store::has(const std::string& key) const {
    BOOST_LOG_TRIVIAL(debug) << "Generating hash for key: " << key;
    // Check if file exists at calculated path
    std::string hash = hash_key(key);
    std::filesystem::path file_path = get_path_for_hash(hash);
    bool exists = std::filesystem::exists(file_path);
    BOOST_LOG_TRIVIAL(debug) << "Checking existence of key: " << key 
                            << " at path: " << file_path.string()
                            << " - " << (exists ? "exists" : "not found");
    return exists;
}

void Store::remove(const std::string& key) {
    BOOST_LOG_TRIVIAL(info) << "Removing file with key: " << key;
    
    // Calculate path and attempt deletion
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
    // Remove all files and directories under base path
    std::filesystem::remove_all(base_path_);
    // Recreate empty base directory
    ensure_directory_exists(base_path_);
    BOOST_LOG_TRIVIAL(info) << "Store cleared successfully";
}

std::string Store::hash_key(const std::string& key) const {
    BOOST_LOG_TRIVIAL(debug) << "Generating hash for key: " << key;
    
    // Initialize OpenSSL EVP context for SHA-256 hashing
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    // Create new EVP message digest context
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw StoreError("Failed to create hash context");
    }
    
    // Initialize context with SHA-256 algorithm
    if (!EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr)) {
        EVP_MD_CTX_free(ctx);
        throw StoreError("Failed to initialize hash context");
    }
    
    // Update hash with input key data
    if (!EVP_DigestUpdate(ctx, key.c_str(), key.length())) {
        EVP_MD_CTX_free(ctx);
        throw StoreError("Failed to update hash");
    }
    
    // Finalize hash calculation
    if (!EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        EVP_MD_CTX_free(ctx);
        throw StoreError("Failed to finalize hash");
    }
    
    EVP_MD_CTX_free(ctx);
    
    // Convert binary hash to hexadecimal string
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    std::string result = ss.str();
    BOOST_LOG_TRIVIAL(debug) << "Generated hash: " << result;
    return result;
}

std::filesystem::path Store::get_path_for_hash(const std::string& hash) const {
    // Create hierarchical path structure:
    // base_path/aa/bb/cc/remaininghash
    // where aa, bb, cc are first three 2-character segments of hash
    std::filesystem::path path = base_path_;
    for (size_t i = 0; i < 6; i += 2) {
        path /= hash.substr(i, 2);
    }
    path /= hash.substr(6);
    
    BOOST_LOG_TRIVIAL(debug) << "Calculated hierarchical path: " << path.string();
    return path;
}

void Store::ensure_directory_exists(const std::filesystem::path& path) const {
    // Create directory and all parent directories if they don't exist
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
}

} // namespace dfs
