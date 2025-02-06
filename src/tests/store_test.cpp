// #include <gtest/gtest.h>
// #include <sstream>
// #include <filesystem>
// #include "store/store.hpp"
// #include <chrono>
// #include <thread>
// #include <atomic>
// #include <set>

// using namespace dfs::store;

// class StoreTest : public ::testing::Test {
// protected:
//   std::string test_dir;
//   std::unique_ptr<Store> store;

//   void SetUp() override {
//     // Create a unique temporary test directory for each test
//     test_dir = std::filesystem::temp_directory_path() / 
//            ("store_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

//     // Ensure test directory exists with proper permissions
//     std::filesystem::create_directories(test_dir);
//     ASSERT_TRUE(std::filesystem::exists(test_dir)) << "Failed to create test directory: " << test_dir;

//     // Initialize store with the test directory
//     store = std::make_unique<Store>(test_dir);
//     ASSERT_NE(store, nullptr) << "Failed to create Store instance";
//   }

//   void TearDown() override {
//     // Clean up test directory after each test
//     if (store) {
//       try {
//         store->clear();  // Clear store contents first
//         store.reset();   // Destroy store instance
//       } catch (const std::exception& e) {
//         ADD_FAILURE() << "Failed to cleanup store: " << e.what();
//       }
//     }

//     if (std::filesystem::exists(test_dir)) {
//       try {
//         std::filesystem::remove_all(test_dir);
//       } catch (const std::exception& e) {
//         ADD_FAILURE() << "Failed to remove test directory: " << e.what();
//       }
//     }
//   }

//   // Helper to create test data stream with proper initialization
//   static std::unique_ptr<std::stringstream> create_test_stream(const std::string& content) {
//     auto ss = std::make_unique<std::stringstream>();
//     if (!content.empty()) {
//       ss->write(content.c_str(), content.length());
//       ss->seekg(0);  // Reset read position to beginning
//     }
//     EXPECT_TRUE(ss->good()) << "Failed to create test stream";
//     return ss;
//   }

//   // Helper to verify stream contents with proper error handling
//   static void verify_stream_content(std::stringstream& stream, const std::string& expected) {
//     ASSERT_TRUE(stream.good()) << "Input stream is not in good state";
//     std::string content;
//     if (!expected.empty()) {
//       content = stream.str();
//     }
//     ASSERT_EQ(content, expected) << "Stream content doesn't match expected value";
//   }
// };

// /**
//  * Test: BasicStoreAndRetrieve
//  * Purpose: Verifies the basic functionality of storing and retrieving data from the store.
//  * 
//  * Methodology:
//  * 1. Stores a simple string using a key
//  * 2. Verifies the key exists in the store
//  * 3. Retrieves the data and verifies its content
//  * 
//  * Expected Results:
//  * - Data should be stored successfully
//  * - Key should exist in the store after storing
//  * - Retrieved data should match the original input
//  * - No exceptions should be thrown during normal operation
//  */
// TEST_F(StoreTest, BasicStoreAndRetrieve) {
//   const std::string test_key = "test_key";
//   const std::string test_data = "Hello, Store!";

//   // Store data
//   auto input = create_test_stream(test_data);
//   ASSERT_TRUE(input->good()) << "Input stream should be valid";
//   ASSERT_NO_THROW({
//     store->store(test_key, *input);
//   }) << "Store operation should not throw";
//   ASSERT_TRUE(store->has(test_key)) << "Key should exist after storing";

//   // Retrieve and verify
//   std::stringstream output;
//   ASSERT_NO_THROW(store->get(test_key, output)) << "Get operation should not throw";
//   verify_stream_content(output, test_data);
// }

// /**
//  * Test: EmptyInput
//  * Purpose: Tests the store's handling of empty data streams.
//  * 
//  * Methodology:
//  * 1. Attempts to store an empty string
//  * 2. Verifies the key exists
//  * 3. Retrieves and verifies the empty content
//  * 
//  * Expected Results:
//  * - Empty data should be stored successfully
//  * - Key should exist in the store
//  * - Retrieved stream should be valid but empty
//  * - No exceptions should be thrown
//  */
// TEST_F(StoreTest, EmptyInput) {
//   const std::string test_key = "empty_test";

//   // First test storing empty data
//   auto empty_input = create_test_stream("");
//   ASSERT_TRUE(empty_input->good()) << "Empty input stream should be valid";
//   ASSERT_NO_THROW(store->store(test_key, *empty_input)) << "Storing empty data should not throw";
//   ASSERT_TRUE(store->has(test_key)) << "Key should exist even with empty data";

//   // Verify empty data retrieval
//   std::stringstream output;
//   ASSERT_NO_THROW(store->get(test_key, output)) << "Get operation should not throw";
//   verify_stream_content(output, "");
// }

// /**
//  * Test: MultipleFiles
//  * Purpose: Verifies the store can handle multiple files simultaneously.
//  * 
//  * Methodology:
//  * 1. Stores multiple files with different keys and content
//  * 2. Verifies each key exists
//  * 3. Retrieves and verifies content of each file
//  * 
//  * Expected Results:
//  * - All files should be stored successfully
//  * - Each key should exist in the store
//  * - Retrieved content should match original data for each file
//  * - No cross-contamination between files
//  */
// TEST_F(StoreTest, MultipleFiles) {
//   const std::vector<std::pair<std::string, std::string>> test_data = {
//     {"key1", "First file content"},
//     {"key2", "Second file content"},
//     {"key3", "Third file content"}
//   };

//   // Store multiple files
//   for (const auto& [key, data] : test_data) {
//     auto input = create_test_stream(data);
//     ASSERT_TRUE(input->good()) << "Input stream for key " << key << " should be valid";
//     ASSERT_NO_THROW({
//       store->store(key, *input);
//     }) << "Failed to store key: " << key;
//     ASSERT_TRUE(store->has(key)) << "Key should exist after storing: " << key;
//   }

//   // Retrieve and verify all files
//   for (const auto& [key, expected_data] : test_data) {
//     ASSERT_TRUE(store->has(key)) << "Key should still exist: " << key;
//     std::stringstream output;
//     ASSERT_NO_THROW(store->get(key, output)) << "Get operation failed for key: " << key;
//     verify_stream_content(output, expected_data);
//   }
// }

// /**
//  * Test: ErrorHandling
//  * Purpose: Tests the store's error handling capabilities.
//  * 
//  * Methodology:
//  * 1. Attempts to retrieve non-existent key
//  * 2. Attempts to store data with invalid stream
//  * 
//  * Expected Results:
//  * - Getting non-existent key should throw StoreError
//  * - Storing with invalid stream should throw StoreError
//  * - Error messages should be descriptive
//  */
// TEST_F(StoreTest, ErrorHandling) {
//   const std::string test_key = "error_key";

//   // Test getting non-existent key
//   EXPECT_FALSE(store->has(test_key)) << "Key should not exist initially";
//   std::stringstream output;
//   EXPECT_THROW(store->get(test_key, output), StoreError) << "Getting non-existent key should throw";

//   // Test storing with invalid stream
//   std::stringstream bad_stream;
//   bad_stream.setstate(std::ios::badbit);
//   EXPECT_THROW(store->store(test_key, bad_stream), StoreError) << "Storing with invalid stream should throw";
// }

// /**
//  * Test: Clear
//  * Purpose: Verifies the store's clear functionality.
//  * 
//  * Methodology:
//  * 1. Stores multiple items
//  * 2. Clears the store
//  * 3. Verifies all items are removed
//  * 
//  * Expected Results:
//  * - Clear operation should succeed
//  * - All keys should be removed
//  * - Attempting to access cleared keys should throw
//  */
// TEST_F(StoreTest, Clear) {
//   const std::vector<std::string> keys = {"key1", "key2", "key3"};
//   const std::string test_data = "Test data";

//   // Store multiple items
//   for (const auto& key : keys) {
//     auto input = create_test_stream(test_data);
//     ASSERT_NO_THROW(store->store(key, *input)) << "Failed to store key: " << key;
//     ASSERT_TRUE(store->has(key)) << "Key should exist after storing: " << key;
//   }

//   // Clear store
//   ASSERT_NO_THROW(store->clear()) << "Clear operation should not throw";

//   // Verify all keys are removed
//   for (const auto& key : keys) {
//     ASSERT_FALSE(store->has(key)) << "Key should not exist after clear: " << key;
//     std::stringstream output;
//     EXPECT_THROW(store->get(key, output), StoreError) << "Getting cleared key should throw";
//   }
// }

// /**
//  * Test: LargeFileHandling
//  * Purpose: Tests the store's ability to handle large files.
//  * 
//  * Methodology:
//  * 1. Creates a large (10MB) test file
//  * 2. Stores and retrieves the large file
//  * 3. Verifies content integrity
//  * 
//  * Expected Results:
//  * - Large file should be stored successfully
//  * - Retrieved content should match original
//  * - No memory issues or performance degradation
//  */
// TEST_F(StoreTest, LargeFileHandling) {
//   const std::string test_key = "large_file";
//   const size_t large_size = 10 * 1024 * 1024; // 10MB
//   std::string large_data(large_size, 'X'); // Create large content

//   // Store large file
//   auto input = create_test_stream(large_data);
//   ASSERT_NO_THROW({
//     store->store(test_key, *input);
//   }) << "Storing large file should not throw";
//   ASSERT_TRUE(store->has(test_key)) << "Large file key should exist after storing";

//   // Retrieve and verify large file
//   std::stringstream output;
//   ASSERT_NO_THROW(store->get(test_key, output)) << "Get operation should not throw";
//   verify_stream_content(output, large_data);
// }

// /**
//  * Test: InvalidKeyNames
//  * Purpose: Tests the store's handling of various potentially problematic key names.
//  * 
//  * Methodology:
//  * 1. Attempts to store data with various edge-case key names
//  * 2. Verifies proper handling of each case
//  * 
//  * Expected Results:
//  * - Store should either handle or reject invalid keys gracefully
//  * - No filesystem security vulnerabilities
//  * - Proper error handling for truly invalid keys
//  */
// TEST_F(StoreTest, InvalidKeyNames) {
//   const std::vector<std::pair<std::string, std::string>> test_cases = {
//     {"", "Empty key"}, // Empty key
//     {"../attempt/traversal", "Path traversal attempt"}, // Path traversal attempt
//     {std::string(1024, 'a'), "Very long key"}, // Extremely long key
//     {"key\0with\0nulls", "Key with null characters"}, // Null characters
//     {"/absolute/path", "Absolute path attempt"}, // Absolute path
//     {"\\windows\\path", "Windows-style path"} // Windows-style path
//   };
//   const std::string test_data = "Test data";

//   for (const auto& [key, description] : test_cases) {
//     auto input = create_test_stream(test_data);
//     try {
//       store->store(key, *input);
//       // If store succeeds, verify the data can be retrieved
//       ASSERT_TRUE(store->has(key)) << "Key should exist after storing: " << description;
//       std::stringstream output;
//       ASSERT_NO_THROW(store->get(key, output)) << "Get operation should not throw for key: " << description;
//       verify_stream_content(output, test_data);
//     } catch (const StoreError& e) {
//       // Some invalid keys might cause StoreError, which is acceptable
//       SUCCEED() << "Store rejected invalid key as expected: " << description;
//     }
//   }
// }

// /**
//  * Test: OverwriteBehavior
//  * Purpose: Tests the store's behavior when overwriting existing keys.
//  * 
//  * Methodology:
//  * 1. Stores initial data
//  * 2. Overwrites with new data
//  * 3. Verifies content is updated
//  * 
//  * Expected Results:
//  * - Initial store should succeed
//  * - Overwrite should succeed
//  * - Retrieved data should match latest stored content
//  */
// TEST_F(StoreTest, OverwriteBehavior) {
//   const std::string test_key = "overwrite_test";
//   const std::string initial_data = "Initial content";
//   const std::string updated_data = "Updated content";

//   // Store initial data
//   auto initial_input = create_test_stream(initial_data);
//   ASSERT_NO_THROW(store->store(test_key, *initial_input)) << "Initial store should not throw";
//   ASSERT_TRUE(store->has(test_key)) << "Key should exist after initial store";

//   // Verify initial content
//   std::stringstream initial_output;
//   ASSERT_NO_THROW(store->get(test_key, initial_output)) << "Get operation should not throw";
//   verify_stream_content(initial_output, initial_data);

//   // Overwrite with new data
//   auto update_input = create_test_stream(updated_data);
//   ASSERT_NO_THROW(store->store(test_key, *update_input)) << "Overwrite should not throw";

//   // Verify updated content
//   std::stringstream updated_output;
//   ASSERT_NO_THROW(store->get(test_key, updated_output)) << "Get operation should not throw";
//   verify_stream_content(updated_output, updated_data);
// }

// /**
//  * Test: StreamStatePreservation
//  * Purpose: Verifies that stream state and position are properly preserved.
//  * 
//  * Methodology:
//  * 1. Stores test data
//  * 2. Performs partial reads
//  * 3. Verifies stream position and content
//  * 
//  * Expected Results:
//  * - Stream position should be maintained between operations
//  * - Partial reads should return correct content
//  * - Stream should remain in good state
//  */
// TEST_F(StoreTest, StreamStatePreservation) {
//   const std::string test_key = "stream_state_test";
//   const std::string test_data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

//   // Store full data
//   auto input = create_test_stream(test_data);
//   store->store(test_key, *input);

//   // Read partial data and verify stream position
//   std::stringstream output;
//   store->get(test_key, output);

//   std::string content = output.str();
//   ASSERT_EQ(content.substr(0, 5), "ABCDE") << "First 5 characters should match";
//   ASSERT_EQ(content.substr(5, 5), "FGHIJ") << "Next 5 characters should match";
// }

// /**
//  * Test: ConcurrentAccess
//  * Purpose: Tests the store's thread safety and concurrent access capabilities.
//  * 
//  * Methodology:
//  * 1. Creates multiple threads
//  * 2. Each thread performs multiple store/retrieve operations
//  * 3. Tracks successful operations
//  * 
//  * Expected Results:
//  * - All operations should complete successfully
//  * - No data corruption or race conditions
//  * - Proper handling of concurrent access
//  */
// TEST_F(StoreTest, ConcurrentAccess) {
//   const size_t num_threads = 10;
//   const size_t ops_per_thread = 100;
//   std::vector<std::thread> threads;
//   std::atomic<size_t> successful_ops{0};

//   for (size_t i = 0; i < num_threads; ++i) {
//     threads.emplace_back([this, i, ops_per_thread, &successful_ops]() {
//       for (size_t j = 0; j < ops_per_thread; ++j) {
//         const std::string key = "concurrent_key_" + std::to_string(i) + "_" + std::to_string(j);
//         const std::string data = "Concurrent test data for " + key;

//         try {
//           // Store data
//           auto input = create_test_stream(data);
//           store->store(key, *input);
//           EXPECT_TRUE(store->has(key));

//           // Retrieve and verify
//           std::stringstream output;
//           store->get(key, output);
//           EXPECT_EQ(output.str(), data);

//           successful_ops++;
//         } catch (const std::exception& e) {
//           ADD_FAILURE() << "Thread " << i << " failed: " << e.what();
//         }
//       }
//     });
//   }

//   // Wait for all threads to complete
//   for (auto& thread : threads) {
//     thread.join();
//   }

//   EXPECT_EQ(successful_ops, num_threads * ops_per_thread)
//     << "All operations should complete successfully";
// }

// /**
//  * Test: FileSizeRetrieval
//  * Purpose: Tests the get_file_size() method functionality.
//  * 
//  * Methodology:
//  * 1. Tests size retrieval for normal files
//  * 2. Tests size retrieval for empty files
//  * 3. Tests error handling for non-existent files
//  * 
//  * Expected Results:
//  * - Should return correct file size for existing files
//  * - Should return 0 for empty files
//  * - Should throw StoreError for non-existent files
//  */
// TEST_F(StoreTest, FileSizeRetrieval) {
//   const std::string test_key = "size_test";
//   const std::string test_data = "Hello, Store!";
//   const std::string empty_key = "empty_test";
//   const std::string nonexistent_key = "nonexistent_key";

//   // Test normal file size
//   auto input = create_test_stream(test_data);
//   store->store(test_key, *input);
//   ASSERT_EQ(store->get_file_size(test_key), test_data.length()) 
//     << "File size should match input data length";

//   // Test empty file size
//   auto empty_input = create_test_stream("");
//   store->store(empty_key, *empty_input);
//   ASSERT_EQ(store->get_file_size(empty_key), 0) 
//     << "Empty file should have size 0";

//   // Test non-existent file
//   EXPECT_THROW(store->get_file_size(nonexistent_key), StoreError)
//     << "Getting size of non-existent file should throw";
// }