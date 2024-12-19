#include "crypto/store.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>

namespace dfs {
namespace crypto {

Store::Store(const std::filesystem::path& base_path) : base_path_(base_path) {
    std::filesystem::create_directories(base_path_);
}

void Store::store(const std::string& file_key, std::istream& data_stream) {
    if (!data_stream) {
        throw CryptoError("Invalid data stream provided");
    }

    auto file_path = get_file_path(file_key);
    std::filesystem::create_directories(file_path.parent_path());
    
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        throw CryptoError("Failed to create file: " + file_path.string());
    }

    // Copy stream data to file using buffer for efficiency
    constexpr size_t buffer_size = 4096;
    char buffer[buffer_size];
    while (data_stream.read(buffer, buffer_size)) {
        file.write(buffer, data_stream.gcount());
    }
    if (data_stream.gcount() > 0) {
        file.write(buffer, data_stream.gcount());
    }

    if (!file) {
        throw CryptoError("Failed to write data to file: " + file_path.string());
    }
}

std::unique_ptr<std::istream> Store::get(const std::string& file_key) {
    auto file_path = get_file_path(file_key);
    if (!std::filesystem::exists(file_path)) {
        throw CryptoError("File not found: " + file_key);
    }

    auto stream = std::make_unique<std::ifstream>(file_path, std::ios::binary);
    if (!*stream) {
        throw CryptoError("Failed to open file: " + file_path.string());
    }

    return stream;
}

bool Store::has(const std::string& file_key) const {
    return std::filesystem::exists(get_file_path(file_key));
}

void Store::remove(const std::string& file_key) {
    auto file_path = get_file_path(file_key);
    if (!std::filesystem::remove(file_path)) {
        throw CryptoError("Failed to remove file: " + file_path.string());
    }
}

void Store::clear() {
    for (const auto& entry : std::filesystem::directory_iterator(base_path_)) {
        std::filesystem::remove_all(entry.path());
    }
}

std::string Store::hash_key(const std::string& file_key) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, file_key.c_str(), file_key.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::filesystem::path Store::get_file_path(const std::string& file_key) const {
    return base_path_ / hash_key(file_key);
}

} // namespace crypto
} // namespace dfs
