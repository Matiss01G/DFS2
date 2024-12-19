#include <gtest/gtest.h>
#include <sstream>
#include <filesystem>
#include "store/store.hpp"

using namespace dfs::store;

class StoreTest : public ::testing::Test {
protected:
    std::string test_dir;
    std::unique_ptr<Store> store;

    void SetUp() override {
        // Create a unique temporary test directory for each test
        test_dir = std::filesystem::temp_directory_path() / 
                   ("store_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

        // Ensure test directory exists with proper permissions
        std::filesystem::create_directories(test_dir);
        ASSERT_TRUE(std::filesystem::exists(test_dir)) << "Failed to create test directory: " << test_dir;

        // Initialize store with the test directory
        store = std::make_unique<Store>(test_dir);
        ASSERT_NE(store, nullptr) << "Failed to create Store instance";
    }

    void TearDown() override {
        // Clean up test directory after each test
        if (store) {
            try {
                store->clear();  // Clear store contents first
                store.reset();   // Destroy store instance
            } catch (const std::exception& e) {
                ADD_FAILURE() << "Failed to cleanup store: " << e.what();
            }
        }

        if (std::filesystem::exists(test_dir)) {
            try {
                std::filesystem::remove_all(test_dir);
            } catch (const std::exception& e) {
                ADD_FAILURE() << "Failed to remove test directory: " << e.what();
            }
        }
    }

    // Helper to create test data stream with proper initialization
    static std::unique_ptr<std::stringstream> create_test_stream(const std::string& content) {
        auto ss = std::make_unique<std::stringstream>();
        if (!content.empty()) {
            ss->write(content.c_str(), content.length());
            ss->seekg(0);  // Reset read position to beginning
        }
        EXPECT_TRUE(ss->good()) << "Failed to create test stream";
        return ss;
    }

    // Helper to verify stream contents with proper error handling
    static void verify_stream_content(std::istream& stream, const std::string& expected) {
        ASSERT_TRUE(stream.good()) << "Input stream is not in good state";
        std::string content;
        if (!expected.empty()) {
            std::stringstream buffer;
            buffer << stream.rdbuf();
            EXPECT_TRUE(buffer.good()) << "Failed to read stream contents";
            content = buffer.str();
        } else {
            char ch;
            if (stream.get(ch)) {
                content.push_back(ch);
                while (stream.get(ch)) {
                    content.push_back(ch);
                }
            }
            // For empty content, we expect EOF immediately
            EXPECT_TRUE(stream.eof()) << "Stream should be at EOF for empty content";
        }
        ASSERT_EQ(content, expected) << "Stream content doesn't match expected value";
    }
};

// Test basic store and retrieve operations
TEST_F(StoreTest, BasicStoreAndRetrieve) {
    const std::string test_key = "test_key";
    const std::string test_data = "Hello, Store!";

    // Store data
    auto input = create_test_stream(test_data);
    ASSERT_TRUE(input->good()) << "Input stream should be valid";
    ASSERT_NO_THROW({
        store->store(test_key, *input);
    }) << "Store operation should not throw";
    ASSERT_TRUE(store->has(test_key)) << "Key should exist after storing";

    // Retrieve and verify
    auto output = store->get(test_key);
    ASSERT_TRUE(output != nullptr) << "Retrieved stream should not be null";
    verify_stream_content(*output, test_data);
}

// Test storing and retrieving empty data
TEST_F(StoreTest, EmptyInput) {
    const std::string test_key = "empty_test";

    // First test storing empty data
    auto empty_input = create_test_stream("");
    ASSERT_TRUE(empty_input->good()) << "Empty input stream should be valid";
    ASSERT_NO_THROW(store->store(test_key, *empty_input)) << "Storing empty data should not throw";
    ASSERT_TRUE(store->has(test_key)) << "Key should exist even with empty data";

    // Verify empty data retrieval
    auto output = store->get(test_key);
    ASSERT_TRUE(output != nullptr) << "Retrieved stream should not be null";
    verify_stream_content(*output, "");
}

// Test storing and retrieving multiple files
TEST_F(StoreTest, MultipleFiles) {
    const std::vector<std::pair<std::string, std::string>> test_data = {
        {"key1", "First file content"},
        {"key2", "Second file content"},
        {"key3", "Third file content"}
    };

    // Store multiple files
    for (const auto& [key, data] : test_data) {
        auto input = create_test_stream(data);
        ASSERT_TRUE(input->good()) << "Input stream for key " << key << " should be valid";
        ASSERT_NO_THROW({
            store->store(key, *input);
        }) << "Failed to store key: " << key;
        ASSERT_TRUE(store->has(key)) << "Key should exist after storing: " << key;
    }

    // Retrieve and verify all files
    for (const auto& [key, expected_data] : test_data) {
        ASSERT_TRUE(store->has(key)) << "Key should still exist: " << key;
        auto output = store->get(key);
        ASSERT_TRUE(output != nullptr) << "Retrieved stream should not be null for key: " << key;
        verify_stream_content(*output, expected_data);
    }
}

// Test error handling
TEST_F(StoreTest, ErrorHandling) {
    const std::string test_key = "error_key";

    // Test getting non-existent key
    EXPECT_FALSE(store->has(test_key)) << "Key should not exist initially";
    EXPECT_THROW(store->get(test_key), StoreError) << "Getting non-existent key should throw";

    // Test storing with invalid stream
    std::stringstream bad_stream;
    bad_stream.setstate(std::ios::badbit);
    EXPECT_THROW(store->store(test_key, bad_stream), StoreError) << "Storing with invalid stream should throw";
}

// Test clear functionality
TEST_F(StoreTest, Clear) {
    const std::vector<std::string> keys = {"key1", "key2", "key3"};
    const std::string test_data = "Test data";

    // Store multiple items
    for (const auto& key : keys) {
        auto input = create_test_stream(test_data);
        ASSERT_NO_THROW(store->store(key, *input)) << "Failed to store key: " << key;
        ASSERT_TRUE(store->has(key)) << "Key should exist after storing: " << key;
    }

    // Clear store
    ASSERT_NO_THROW(store->clear()) << "Clear operation should not throw";

    // Verify all keys are removed
    for (const auto& key : keys) {
        ASSERT_FALSE(store->has(key)) << "Key should not exist after clear: " << key;
        EXPECT_THROW(store->get(key), StoreError) << "Getting cleared key should throw";
    }
}