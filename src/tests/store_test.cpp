#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "store/store.hpp"
#include "crypto/crypto_error.hpp"
#include <sstream>
#include <filesystem>
#include <fstream>

using namespace dfs::store;
using namespace dfs::crypto;

class StoreTest : public ::testing::Test, protected Store {
public:
    StoreTest() : Store(std::filesystem::temp_directory_path() / "store_test") {
        test_dir_ = std::filesystem::temp_directory_path() / "store_test";
    }
protected:
    void SetUp() override {
        // Create a temporary test directory
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    // Helper function to verify directory structure
    bool verify_directory_structure(const std::string& file_key) {
        // Store test data
        std::string test_data = "test content";
        std::istringstream input(test_data);
        store(file_key, input);

        // Get the file path using inherited method
        auto file_path = get_file_path(file_key);
        bool file_exists = std::filesystem::exists(file_path);

        // Count directory levels (should have hash split into 3 levels)
        int dir_depth = 0;
        auto path = file_path;
        while (path.parent_path() != test_dir_) {
            dir_depth++;
            path = path.parent_path();
        }

        // Verify file contents
        bool content_matches = false;
        if (file_exists) {
            auto output_stream = get(file_key);
            std::stringstream buffer;
            buffer << output_stream->rdbuf();
            content_matches = (buffer.str() == test_data);
        }

        // Clean up test file
        remove(file_key);

        // Verify that:
        // 1. File exists at expected path
        // 2. Has correct hierarchical structure (3 levels)
        // 3. Contains correct data
        return file_exists && dir_depth == 3 && content_matches;
    }

    std::filesystem::path test_dir_;
};

// Test basic file storage and retrieval
TEST_F(StoreTest, BasicStoreAndRetrieve) {
    const std::string test_key = "test_file";
    const std::string test_data = "Hello, World!";
    std::istringstream input(test_data);

    // Store the data
    store(test_key, input);

    // Verify directory structure
    ASSERT_TRUE(verify_directory_structure(test_key));

    // Retrieve and verify data
    auto output_stream = get(test_key);
    ASSERT_NE(output_stream, nullptr);

    std::stringstream buffer;
    buffer << output_stream->rdbuf();
    EXPECT_EQ(buffer.str(), test_data);
}

// Test file existence check
TEST_F(StoreTest, FileExistenceCheck) {
    const std::string test_key = "existence_test";
    const std::string test_data = "Test data";
    std::istringstream input(test_data);

    EXPECT_FALSE(has(test_key));
    store(test_key, input);
    EXPECT_TRUE(has(test_key));
}

// Test storing multiple files
TEST_F(StoreTest, MultipleFiles) {
    const int num_files = 10;
    std::vector<std::string> keys;

    // Store multiple files
    for (int i = 0; i < num_files; ++i) {
        std::string key = "file_" + std::to_string(i);
        std::string data = "Data for file " + std::to_string(i);
        std::istringstream input(data);

        store(key, input);
        keys.push_back(key);

        // Verify directory structure for each file
        ASSERT_TRUE(verify_directory_structure(key));
    }

    // Verify all files exist and are retrievable
    for (int i = 0; i < num_files; ++i) {
        ASSERT_TRUE(has(keys[i]));
        auto stream = get(keys[i]);
        ASSERT_NE(stream, nullptr);

        std::stringstream buffer;
        buffer << stream->rdbuf();
        EXPECT_EQ(buffer.str(), "Data for file " + std::to_string(i));
    }
}

// Test file removal
TEST_F(StoreTest, FileRemoval) {
    const std::string test_key = "remove_test";
    const std::string test_data = "Test data for removal";
    std::istringstream input(test_data);

    store(test_key, input);
    ASSERT_TRUE(has(test_key));

    remove(test_key);
    EXPECT_FALSE(has(test_key));
    EXPECT_THROW(get(test_key), CryptoError);
}

// Test store with empty input
TEST_F(StoreTest, EmptyInput) {
    const std::string test_key = "empty_test";
    std::istringstream empty_input("");

    store(test_key, empty_input);
    ASSERT_TRUE(verify_directory_structure(test_key));

    auto output_stream = get(test_key);
    ASSERT_NE(output_stream, nullptr);

    std::stringstream buffer;
    buffer << output_stream->rdbuf();
    EXPECT_TRUE(buffer.str().empty());
}

// Test error handling for invalid input stream
TEST_F(StoreTest, InvalidInputStream) {
    const std::string test_key = "invalid_stream_test";
    std::istringstream input("Test data");
    input.setstate(std::ios::badbit);

    EXPECT_THROW(store(test_key, input), CryptoError);
}

// Test clearing all files
TEST_F(StoreTest, ClearStore) {
    // Store some test files
    for (int i = 0; i < 5; ++i) {
        std::string key = "clear_test_" + std::to_string(i);
        std::istringstream input("Test data " + std::to_string(i));
        store(key, input);
        ASSERT_TRUE(has(key));
    }

    // Clear the store
    clear();

    // Verify all files are removed
    for (int i = 0; i < 5; ++i) {
        std::string key = "clear_test_" + std::to_string(i);
        EXPECT_FALSE(has(key));
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}