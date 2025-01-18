#include <gtest/gtest.h>
#include <filesystem>
#include "network/file_server.hpp"
#include <boost/log/trivial.hpp>

using namespace dfs::network;

class FileServerTest : public ::testing::Test {
protected:
    const uint32_t test_server_id = 12345;
    const std::vector<uint8_t> test_key{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F  // 32 bytes for AES-256
    };
    std::string test_store_path;

    void SetUp() override {
        test_store_path = "fileserver_" + std::to_string(test_server_id);

        // Clean up any existing test directory
        if (std::filesystem::exists(test_store_path)) {
            std::filesystem::remove_all(test_store_path);
        }
    }

    void TearDown() override {
        // Clean up test directory after test
        if (std::filesystem::exists(test_store_path)) {
            try {
                std::filesystem::remove_all(test_store_path);
            } catch (const std::exception& e) {
                ADD_FAILURE() << "Failed to cleanup test directory: " << e.what();
            }
        }
    }
};

TEST_F(FileServerTest, Constructor) {
    ASSERT_NO_THROW({
        FileServer server(test_server_id, test_key);
    }) << "FileServer constructor should not throw";

    // Verify store directory was created
    ASSERT_TRUE(std::filesystem::exists(test_store_path)) 
        << "Store directory should be created during initialization";
    ASSERT_TRUE(std::filesystem::is_directory(test_store_path)) 
        << "Store path should be a directory";
}

TEST_F(FileServerTest, InvalidConstructorParameters) {
    // Test with empty key
    std::vector<uint8_t> empty_key;
    EXPECT_THROW({
        FileServer server(test_server_id, empty_key);
    }, std::invalid_argument) << "Constructor should throw on empty key";

    // Test with invalid key size (not 32 bytes)
    std::vector<uint8_t> invalid_key(16, 0x42);  // Only 16 bytes
    EXPECT_THROW({
        FileServer server(test_server_id, invalid_key);
    }, std::invalid_argument) << "Constructor should throw on invalid key size";
}