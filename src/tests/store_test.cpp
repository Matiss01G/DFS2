#include <gtest/gtest.h>
#include <sstream>
#include <filesystem>
#include "store/store.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <set>

using namespace dfs::store;

class StoreTest : public ::testing::Test {
protected:
  std::string test_dir;
  std::unique_ptr<Store> store;

  void SetUp() override {
    test_dir = std::filesystem::temp_directory_path() / 
      ("store_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(test_dir);
    ASSERT_TRUE(std::filesystem::exists(test_dir));
    store = std::make_unique<Store>(test_dir);
    ASSERT_NE(store, nullptr);
  }

  void TearDown() override {
    if (store) {
      store->clear();
      store.reset();
    }
    if (std::filesystem::exists(test_dir)) {
      std::filesystem::remove_all(test_dir);
    }
  }

  // Helper methods to reduce repetition
  void store_and_verify(const std::string& key, const std::string& data) {
    auto input = create_test_stream(data);
    ASSERT_NO_THROW(store->store(key, *input)) << "Failed to store key: " << key;
    ASSERT_TRUE(store->has(key)) << "Key should exist after storing: " << key;

    std::stringstream output;
    ASSERT_NO_THROW(store->get(key, output)) << "Failed to retrieve key: " << key;
    ASSERT_EQ(output.str(), data) << "Data mismatch for key: " << key;
  }

  void expect_retrieval_fails(const std::string& key) {
    EXPECT_FALSE(store->has(key)) << "Key should not exist: " << key;
    std::stringstream output;
    EXPECT_THROW(store->get(key, output), StoreError) 
      << "Getting non-existent key should throw: " << key;
  }

  static std::unique_ptr<std::stringstream> create_test_stream(const std::string& content) {
    auto ss = std::make_unique<std::stringstream>();
    if (!content.empty()) {
      ss->write(content.c_str(), content.length());
      ss->seekg(0);
    }
    return ss;
  }
};

TEST_F(StoreTest, BasicOperations) {
  const std::string key = "test_key";
  const std::string data = "Hello, Store!";

  // Test storing and retrieving
  store_and_verify(key, data);

  // Test empty data
  store_and_verify("empty_key", "");

  // Test non-existent key
  expect_retrieval_fails("nonexistent_key");
}

TEST_F(StoreTest, MultipleFiles) {
  const std::vector<std::pair<std::string, std::string>> test_data = {
    {"key1", "First content"},
    {"key2", "Second content"},
    {"key3", "Third content"}
  };

  for (const auto& [key, data] : test_data) {
    store_and_verify(key, data);
  }
}

TEST_F(StoreTest, ErrorHandling) {
  // Test invalid stream
  std::stringstream bad_stream;
  bad_stream.setstate(std::ios::badbit);
  EXPECT_THROW(store->store("bad_stream", bad_stream), StoreError);

  // Test clear operation
  store_and_verify("temp_key", "temp_data");
  ASSERT_NO_THROW(store->clear());
  expect_retrieval_fails("temp_key");
}

TEST_F(StoreTest, EdgeCases) {
  const std::string data = "Test data";
  std::vector<std::string> edge_case_keys = {
    "",  // Empty key
    "../path/traversal",  // Path traversal
    std::string(1024, 'a'),  // Very long key
    "/absolute/path",  // Absolute path
    "\\windows\\path"  // Windows-style path
  };

  for (const auto& key : edge_case_keys) {
    auto input = create_test_stream(data);
    try {
      store->store(key, *input);
      ASSERT_TRUE(store->has(key));
    } catch (const StoreError&) {
      SUCCEED() << "Store rejected invalid key as expected: " << key;
    }
  }
}

TEST_F(StoreTest, AdvancedOperations) {
  const std::string key = "advanced_test";
  const size_t large_size = 1024 * 1024;  // 1MB
  const std::string large_data(large_size, 'X');

  // Test large file handling
  store_and_verify(key, large_data);
  ASSERT_EQ(store->get_file_size(key), large_size);

  // Test overwrite behavior
  const std::string updated_data = "Updated content";
  store_and_verify(key, updated_data);
  ASSERT_EQ(store->get_file_size(key), updated_data.length());
}

TEST_F(StoreTest, ConcurrentAccess) {
  const size_t num_threads = 5;
  const size_t ops_per_thread = 50;
  std::atomic<size_t> successful_ops{0};
  std::vector<std::thread> threads;

  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, i, ops_per_thread, &successful_ops]() {
      for (size_t j = 0; j < ops_per_thread; ++j) {
        try {
          std::string key = "concurrent_" + std::to_string(i) + "_" + std::to_string(j);
          std::string data = "Data for " + key;
          store_and_verify(key, data);
          successful_ops++;
        } catch (const std::exception& e) {
          ADD_FAILURE() << "Thread " << i << " failed: " << e.what();
        }
      }
    });
  }

  for (auto& thread : threads) {
      thread.join();
  }

  EXPECT_EQ(successful_ops, num_threads * ops_per_thread);
}