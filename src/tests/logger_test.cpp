#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <thread>
#include <filesystem>
#include "crypto/logger.hpp"

using namespace dfs::crypto::logging;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_logging("test-node");
        // Create logs directory if it doesn't exist
        std::filesystem::create_directories("logs");
    }

    void TearDown() override {
        // Cleanup log files
        try {
            for (const auto& entry : std::filesystem::directory_iterator("logs")) {
                std::filesystem::remove(entry.path());
            }
        } catch (const std::exception& e) {
            std::cerr << "Error cleaning up logs: " << e.what() << std::endl;
        }
    }

    bool log_contains(const std::string& text) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow time for log writes

        try {
            for (const auto& entry : std::filesystem::directory_iterator("logs")) {
                if (entry.path().extension() == ".log") {
                    std::ifstream file(entry.path());
                    std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                    
                    if (content.find(text) != std::string::npos) {
                        return true;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reading log files: " << e.what() << std::endl;
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
    std::thread t([]() {
        LOG_INFO << "Message from thread";
    });
    t.join();
    
    EXPECT_TRUE(log_contains("Message from thread"));
}

TEST_F(LoggerTest, SeverityLevels) {
    const char* messages[] = {
        "Trace message",
        "Debug message",
        "Info message",
        "Warning message",
        "Error message",
        "Fatal message"
    };
    
    LOG_TRACE << messages[0];
    LOG_DEBUG << messages[1];
    LOG_INFO << messages[2];
    LOG_WARN << messages[3];
    LOG_ERROR << messages[4];
    LOG_FATAL << messages[5];
    
    for (const auto& msg : messages) {
        EXPECT_TRUE(log_contains(msg)) << "Failed to find message: " << msg;
    }
}

TEST_F(LoggerTest, LogLevelFiltering) {
    set_log_level(boost::log::trivial::warning);
    
    LOG_DEBUG << "Should not appear";
    LOG_WARN << "Should appear";
    
    EXPECT_FALSE(log_contains("Should not appear"));
    EXPECT_TRUE(log_contains("Should appear"));
}

TEST_F(LoggerTest, EnableDisableLogging) {
    disable_logging();
    LOG_INFO << "Should not appear";
    
    enable_logging();
    LOG_INFO << "Should appear";
    
    EXPECT_FALSE(log_contains("Should not appear"));
    EXPECT_TRUE(log_contains("Should appear"));
}
