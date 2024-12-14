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
        // Clean up any existing logs
        if (std::filesystem::exists("logs")) {
            std::filesystem::remove_all("logs");
        }
        std::filesystem::create_directories("logs");
        
        // Initialize logging
        init_logging("test-node");
        set_log_level(boost::log::trivial::trace);
        
        // Give some time for initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        // Ensure all logs are written
        boost::log::core::get()->flush();
        boost::log::core::get()->remove_all_sinks();
        
        // Clean up log files
        if (std::filesystem::exists("logs")) {
            std::filesystem::remove_all("logs");
        }
    }

    bool log_contains(const std::string& text, int max_retries = 3) {
        for (int retry = 0; retry < max_retries; ++retry) {
            // Force flush and wait with exponential backoff
            boost::log::core::get()->flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
            
            try {
                for (const auto& entry : std::filesystem::directory_iterator("logs")) {
                    if (entry.path().extension() == ".log") {
                        std::ifstream file(entry.path(), std::ios::in | std::ios::binary);
                        if (!file.is_open()) {
                            continue;
                        }
                        
                        std::string content;
                        file.seekg(0, std::ios::end);
                        content.resize(file.tellg());
                        file.seekg(0, std::ios::beg);
                        file.read(&content[0], content.size());
                        
                        if (content.find(text) != std::string::npos) {
                            return true;
                        }
                    }
                }
            } catch (const std::exception& e) {
                if (retry == max_retries - 1) {
                    std::cerr << "Error reading log file (attempt " << retry + 1 << "): " 
                             << e.what() << std::endl;
                }
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
    std::thread t([]() {
        LOG_INFO << "Message from thread";
    });
    t.join();
    
    EXPECT_TRUE(log_contains("Message from thread"));
}

TEST_F(LoggerTest, SeverityLevels) {
    // Set minimum level to trace to capture all messages
    set_log_level(boost::log::trivial::trace);
    
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