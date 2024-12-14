#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include "crypto/logger.hpp"

using namespace dfs::crypto;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::init("test.log");
    }

    void TearDown() override {
        std::remove("test.log");
    }

    bool log_contains(const std::string& text) {
        std::ifstream file("test.log");
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

TEST_F(LoggerTest, BasicLogging) {
    LOG_INFO << "Test info message";
    LOG_ERROR << "Test error message";
    
    EXPECT_TRUE(log_contains("Test info message"));
    EXPECT_TRUE(log_contains("Test error message"));
}

TEST_F(LoggerTest, ThreadLogging) {
    auto thread_func = []() {
        LOG_INFO << "Message from thread";
    };

    std::thread t(thread_func);
    t.join();

    EXPECT_TRUE(log_contains("Message from thread"));
}

TEST_F(LoggerTest, SeverityLevels) {
    LOG_TRACE << "Trace message";
    LOG_DEBUG << "Debug message";
    LOG_INFO << "Info message";
    LOG_WARN << "Warning message";
    LOG_ERROR << "Error message";
    LOG_FATAL << "Fatal message";

    EXPECT_TRUE(log_contains("Trace message"));
    EXPECT_TRUE(log_contains("Debug message"));
    EXPECT_TRUE(log_contains("Info message"));
    EXPECT_TRUE(log_contains("Warning message"));
    EXPECT_TRUE(log_contains("Error message"));
    EXPECT_TRUE(log_contains("Fatal message"));
}
