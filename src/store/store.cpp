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
    BOOST_LOG_TRIVIAL(debug) << "Calculated file path: " << file_path.string();

    // Create parent directories if they don't exist
    std::error_code ec;
    std::filesystem::create_directories(file_path.parent_path(), ec);
    if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Failed to create directory structure: " << ec.message();
        throw crypto::CryptoError("Failed to create directory structure");
    }

    std::ofstream output(file_path, std::ios::binary);
    if (!output) {
        BOOST_LOG_TRIVIAL(error) << "Failed to create output file: " << file_path.string();
        throw crypto::CryptoError("Failed to create output file");
    }

    // Copy stream using buffer for efficiency
    char buffer[4096];
    size_t total_bytes = 0;

    while (input_stream.read(buffer, sizeof(buffer))) {
        output.write(buffer, input_stream.gcount());
        total_bytes += input_stream.gcount();

        if (!output) {
            BOOST_LOG_TRIVIAL(error) << "Failed writing to file: " << file_path.string();
            throw crypto::CryptoError("Failed writing to file");
        }
    }

    // Write any remaining data
    if (input_stream.gcount() > 0) {
        output.write(buffer, input_stream.gcount());
        total_bytes += input_stream.gcount();
    }

    if (!output) {
        BOOST_LOG_TRIVIAL(error) << "Failed writing to file: " << file_path.string();
        throw crypto::CryptoError("Failed writing to file");
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully stored " << total_bytes << " bytes with key: " << file_key;
}

std::unique_ptr<std::istream> Store::get(const std::string& file_key) {
    BOOST_LOG_TRIVIAL(info) << "Retrieving data for key: " << file_key;

    auto file_path = get_file_path(file_key);
    if (!std::filesystem::exists(file_path)) {
        BOOST_LOG_TRIVIAL(error) << "File not found: " << file_path.string();
        throw crypto::CryptoError("File not found");
    }

    auto stream = std::make_unique<std::ifstream>(file_path, std::ios::binary);
    if (!*stream) {
        BOOST_LOG_TRIVIAL(error) << "Failed to open file: " << file_path.string();
        throw crypto::CryptoError("Failed to open file");
    }

    return stream;
}

bool Store::has(const std::string& file_key) const {
    auto file_path = get_file_path(file_key);
    bool exists = std::filesystem::exists(file_path);
    BOOST_LOG_TRIVIAL(debug) << "Checking existence of key: " << file_key << " at path: " << file_path.string() 
                            << " - " << (exists ? "exists" : "not found");
    return exists;
}

void Store::remove(const std::string& file_key) {
    BOOST_LOG_TRIVIAL(info) << "Removing file with key: " << file_key;

    auto file_path = get_file_path(file_key);
    std::error_code ec;

    if (!std::filesystem::remove(file_path, ec) && !ec) {
        BOOST_LOG_TRIVIAL(error) << "File not found: " << file_path.string();
        throw crypto::CryptoError("File not found");
    }

    if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Failed to remove file: " << ec.message();
        throw crypto::CryptoError("Failed to remove file: " + ec.message());
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully removed file with key: " << file_key;
}

void Store::clear() {
    BOOST_LOG_TRIVIAL(info) << "Clearing all files from store";

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(base_path_)) {
        std::filesystem::remove_all(entry.path(), ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Failed to remove entry: " << entry.path().string() << " - " << ec.message();
            throw crypto::CryptoError("Failed to clear store: " + ec.message());
        }
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully cleared all files from store";
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
    // Generate SHA-256 hash of the file key
    auto hash = hash_key(file_key);
    std::filesystem::path path = base_path_;

    // Create a 3-level directory structure using the hash
    // Use different portions of the hash for each level to distribute files evenly
    std::string level1 = hash.substr(0, 2);     // First 2 chars
    std::string level2 = hash.substr(2, 2);     // Next 2 chars
    std::string level3 = hash.substr(4, 2);     // Next 2 chars
    std::string filename = hash.substr(6);       // Remaining chars

    // Build the hierarchical path
    path /= level1;
    path /= level2;
    path /= level3;
    path /= filename;

    BOOST_LOG_TRIVIAL(debug) << "Calculated hierarchical path: " << path.string();
    return path;
}

} // namespace store
} // namespace dfs