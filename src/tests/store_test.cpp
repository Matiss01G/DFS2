#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "store/store.hpp"
#include <sstream>
#include <filesystem>
#include <fstream>

using namespace dfs::store;

class StoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test directory
        test_dir_ = std::filesystem::temp_directory_path() / "store_test";
        std::filesystem::create_directories(test_dir_);
        store_ = std::make_unique<Store>(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    // Helper function to verify directory structure
    bool verify_directory_structure(const std::string& file_key) {
        auto file_path = store_->get_file_path(file_key);
        return std::filesystem::exists(file_path) &&
               std::filesystem::exists(file_path.parent_path()) &&
               std::filesystem::exists(file_path.parent_path().parent_path()) &&
               std::filesystem::exists(file_path.parent_path().parent_path().parent_path());
    }

    // Helper function to verify directory structure
    bool verify_directory_structure(const std::string& file_key) {
        auto file_path = store_->get_file_path(file_key);
        return std::filesystem::exists(file_path) &&
               std::filesystem::exists(file_path.parent_path()) &&
               std::filesystem::exists(file_path.parent_path().parent_path()) &&
               std::filesystem::exists(file_path.parent_path().parent_path().parent_path());
    }

    std::filesystem::path test_dir_;
    std::unique_ptr<Store> store_;
};

// Test basic file storage and retrieval
TEST_F(StoreTest, BasicStoreAndRetrieve) {
    const std::string test_key = "test_file";
    const std::string test_data = "Hello, World!";
    std::istringstream input(test_data);

    // Store the data
    store_->store(test_key, input);

    // Verify directory structure
    ASSERT_TRUE(verify_directory_structure(test_key));

    // Retrieve and verify data
    auto output_stream = store_->get(test_key);
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

    EXPECT_FALSE(store_->has(test_key));
    store_->store(test_key, input);
    EXPECT_TRUE(store_->has(test_key));
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
        
        store_->store(key, input);
        keys.push_back(key);
        
        // Verify directory structure for each file
        ASSERT_TRUE(verify_directory_structure(key));
    }
    
    // Verify all files exist and are retrievable
    for (int i = 0; i < num_files; ++i) {
        ASSERT_TRUE(store_->has(keys[i]));
        auto stream = store_->get(keys[i]);
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

    store_->store(test_key, input);
    ASSERT_TRUE(store_->has(test_key));
    
    store_->remove(test_key);
    EXPECT_FALSE(store_->has(test_key));
    EXPECT_THROW(store_->get(test_key), crypto::CryptoError);
}

// Test store with empty input
TEST_F(StoreTest, EmptyInput) {
    const std::string test_key = "empty_test";
    std::istringstream empty_input("");

    store_->store(test_key, empty_input);
    ASSERT_TRUE(verify_directory_structure(test_key));
    
    auto output_stream = store_->get(test_key);
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

    EXPECT_THROW(store_->store(test_key, input), crypto::CryptoError);
}

// Test clearing all files
TEST_F(StoreTest, ClearStore) {
    // Store some test files
    for (int i = 0; i < 5; ++i) {
        std::string key = "clear_test_" + std::to_string(i);
        std::istringstream input("Test data " + std::to_string(i));
        store_->store(key, input);
        ASSERT_TRUE(store_->has(key));
    }

    // Clear the store
    store_->clear();

    // Verify all files are removed
    for (int i = 0; i < 5; ++i) {
        std::string key = "clear_test_" + std::to_string(i);
        EXPECT_FALSE(store_->has(key));
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
