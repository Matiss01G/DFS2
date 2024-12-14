#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include "crypto/logger.hpp"

using namespace dfs::crypto;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger with test configuration
        init_logging("test.log", severity_level::trace);
    }

    void TearDown() override {
        std::remove("test.log");
    }

    bool log_contains(const std::string& text, int max_retries = 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Initial delay
        
        for (int i = 0; i < max_retries; ++i) {
            try {
                std::ifstream file("test.log");
                if (!file.is_open()) {
                    std::cerr << "Could not open log file, retry " << i + 1 << " of " << max_retries << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                
                std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
                file.close();
                
                if (content.find(text) != std::string::npos) {
                    return true;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (const std::exception& e) {
                std::cerr << "Error reading log file: " << e.what() << std::endl;
            }
        }
        
        // If we get here, we failed to find the text
        std::cerr << "Failed to find text: '" << text << "' in log file after " << max_retries << " retries" << std::endl;
        return false;
    }
};

TEST_F(LoggerTest, BasicLogging) {
    LOG_INFO << "Test info message";
    LOG_ERROR << "Test error message";
    
    // Allow a small delay for log writes
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(log_contains("Test info message"));
    EXPECT_TRUE(log_contains("Test error message"));
}

TEST_F(LoggerTest, ThreadLogging) {
    auto thread_func = []() {
        LOG_INFO << "Message from thread";
    };

    std::thread t(thread_func);
    t.join();

    // Allow a small delay for log writes
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(log_contains("Message from thread"));
}

TEST_F(LoggerTest, SeverityLevels) {
    LOG_TRACE << "Trace message";
    LOG_DEBUG << "Debug message";
    LOG_INFO << "Info message";
    LOG_WARN << "Warning message";
    LOG_ERROR << "Error message";
    LOG_FATAL << "Fatal message";

    // Allow a small delay for log writes
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(log_contains("Trace message"));
    EXPECT_TRUE(log_contains("Debug message"));
    EXPECT_TRUE(log_contains("Info message"));
    EXPECT_TRUE(log_contains("Warning message"));
    EXPECT_TRUE(log_contains("Error message"));
    EXPECT_TRUE(log_contains("Fatal message"));
}
