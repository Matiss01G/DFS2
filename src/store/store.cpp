#include "store/store.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace store {

Store::Store(const std::filesystem::path& base_path) : base_path_(base_path) {
    BOOST_LOG_TRIVIAL(info) << "Initializing Store with base path: " << base_path.string();
    std::filesystem::create_directories(base_path_);
    BOOST_LOG_TRIVIAL(debug) << "Store directory created/verified at: " << base_path.string();
}

void Store::store(const std::string& file_key, std::istream& input_stream) {
    BOOST_LOG_TRIVIAL(info) << "Storing data with key: " << file_key;
    
    if (!input_stream) {
        BOOST_LOG_TRIVIAL(error) << "Invalid input stream provided for key: " << file_key;
        throw crypto::CryptoError("Invalid input stream provided");
    }

    auto file_path = get_file_path(file_key);
    BOOST_LOG_TRIVIAL(debug) << "Calculated storage path: " << file_path.string();
    
    std::filesystem::create_directories(file_path.parent_path());
    BOOST_LOG_TRIVIAL(debug) << "Created/verified parent directories";
    
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        BOOST_LOG_TRIVIAL(error) << "Failed to create file at: " << file_path.string();
        throw crypto::CryptoError("Failed to create file: " + file_path.string());
    }

    // Copy stream data to file using buffer for efficiency
    constexpr size_t buffer_size = 4096;
    char buffer[buffer_size];
    size_t total_bytes = 0;
    
    while (input_stream.read(buffer, buffer_size)) {
        auto bytes_read = input_stream.gcount();
        file.write(buffer, bytes_read);
        total_bytes += bytes_read;
        
        if (!file) {
            BOOST_LOG_TRIVIAL(error) << "Failed to write to file: " << file_path.string();
            throw crypto::CryptoError("Failed to write to file: " + file_path.string());
        }
        
        BOOST_LOG_TRIVIAL(debug) << "Wrote " << bytes_read << " bytes to file";
    }
    
    // Write any remaining data
    if (input_stream.gcount() > 0) {
        auto bytes_read = input_stream.gcount();
        file.write(buffer, bytes_read);
        total_bytes += bytes_read;
        
        if (!file) {
            BOOST_LOG_TRIVIAL(error) << "Failed to write final data block to file: " << file_path.string();
            throw crypto::CryptoError("Failed to write final data to file: " + file_path.string());
        }
        
        BOOST_LOG_TRIVIAL(debug) << "Wrote final " << bytes_read << " bytes to file";
    }

    file.close();
    if (file.fail()) {
        BOOST_LOG_TRIVIAL(error) << "Failed to close file after writing: " << file_path.string();
        throw crypto::CryptoError("Failed to close file after writing: " + file_path.string());
    }
    
    BOOST_LOG_TRIVIAL(info) << "Successfully stored " << total_bytes << " bytes with key: " << file_key;
}

std::unique_ptr<std::istream> Store::get(const std::string& file_key) {
    BOOST_LOG_TRIVIAL(info) << "Retrieving data for key: " << file_key;
    
    auto file_path = get_file_path(file_key);
    BOOST_LOG_TRIVIAL(debug) << "Calculated file path: " << file_path.string();
    
    if (!std::filesystem::exists(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "File not found for key: " << file_key;
        throw crypto::CryptoError("File not found: " + file_key);
    }

    auto input_stream = std::make_unique<std::ifstream>(file_path, std::ios::binary);
    if (!*input_stream) {
        BOOST_LOG_TRIVIAL(error) << "Failed to open file for reading: " << file_path.string();
        throw crypto::CryptoError("Failed to open file for reading: " + file_path.string());
    }

    BOOST_LOG_TRIVIAL(debug) << "Successfully opened file stream for reading";
    return input_stream;
}

bool Store::has(const std::string& file_key) const {
    auto file_path = get_file_path(file_key);
    bool exists = std::filesystem::exists(file_path);
    BOOST_LOG_TRIVIAL(debug) << "Checking existence of key: " << file_key << " at path: " << file_path.string() << " - " << (exists ? "exists" : "not found");
    return exists;
}

void Store::remove(const std::string& file_key) {
    BOOST_LOG_TRIVIAL(info) << "Removing file with key: " << file_key;
    
    auto file_path = get_file_path(file_key);
    BOOST_LOG_TRIVIAL(debug) << "Attempting to remove file at: " << file_path.string();
    
    if (!std::filesystem::remove(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to remove file: " << file_path.string();
        throw crypto::CryptoError("Failed to remove file: " + file_path.string());
    }
    
    BOOST_LOG_TRIVIAL(info) << "Successfully removed file with key: " << file_key;
}

void Store::clear() {
    BOOST_LOG_TRIVIAL(info) << "Clearing all files from store at: " << base_path_.string();
    
    size_t removed_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(base_path_)) {
        BOOST_LOG_TRIVIAL(debug) << "Removing: " << entry.path().string();
        std::filesystem::remove_all(entry.path());
        removed_count++;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Successfully removed " << removed_count << " entries from store";
}

std::string Store::hash_key(const std::string& file_key) const {
    BOOST_LOG_TRIVIAL(debug) << "Generating hash for key: " << file_key;
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, file_key.c_str(), file_key.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    auto result = ss.str();
    BOOST_LOG_TRIVIAL(debug) << "Generated hash: " << result;
    return result;
}

std::filesystem::path Store::get_file_path(const std::string& file_key) const {
    return base_path_ / hash_key(file_key);
}

} // namespace store
} // namespace dfs
