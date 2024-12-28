#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <memory>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "crypto/crypto_stream.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <mutex>

using namespace dfs::network;

// Global singleton logger manager
class LogManager {
public:
    static LogManager& getInstance() {
        static LogManager instance;
        return instance;
    }

    void initializeLogging() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            // Remove any existing sinks to prevent duplicates
            boost::log::core::get()->remove_all_sinks();

            // Add console output with synchronized backend
            auto sink = boost::log::add_console_log(
                std::cout,
                boost::log::keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
            );
            sink->locked_backend()->auto_flush(true);

            // Set logging level to debug to see all messages
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::debug
            );

            // Add commonly used attributes
            boost::log::add_common_attributes();

            initialized_ = true;
        }
    }

    ~LogManager() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            boost::log::core::get()->remove_all_sinks();
            initialized_ = false;
        }
    }

private:
    LogManager() : initialized_(false) {}
    bool initialized_;
    std::mutex mutex_;

    // Delete copy constructor and assignment operator
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
};

// Global function to initialize logging with static initialization check
inline void initLogging() {
    static bool initialized = false;
    if (!initialized) {
        LogManager::getInstance().initializeLogging();
        initialized = true;
    }
}

class CodecTest : public ::testing::Test {
protected:
    std::vector<uint8_t> key_;
    Codec* codec_;
    Channel* channel_;

    void SetUp() override {
        // Initialize logging once through the global function
        initLogging();

        BOOST_LOG_TRIVIAL(info) << "Setting up CodecTest fixture";

        // Initialize 256-bit (32 byte) encryption key
        key_.resize(32, 0x42);  // Test key filled with 0x42
        codec_ = new Codec(key_);
        channel_ = new Channel();

        BOOST_LOG_TRIVIAL(debug) << "Test fixture initialized with key size: " << key_.size();
    }

    void TearDown() override {
        BOOST_LOG_TRIVIAL(debug) << "Tearing down CodecTest fixture";
        delete codec_;
        delete channel_;
    }

    // Helper to create a test message frame
    MessageFrame createTestFrame(const std::string& payload = "test payload", 
                              MessageType type = MessageType::STORE_FILE) {
        MessageFrame frame;
        frame.message_type = type;
        frame.source_id = 1;
        frame.payload_size = payload.size();
        frame.filename_length = 8;  // Default test filename length

        // Generate test IV using std::fill instead of std::generate
        std::fill(frame.initialization_vector.begin(), 
                 frame.initialization_vector.end(), 
                 0x24);  // Test IV filled with 0x24

        // Set up payload stream
        frame.payload_stream = std::make_shared<std::stringstream>(payload);
        return frame;
    }

    // Helper to verify two message frames are equal
    void verifyFramesEqual(const MessageFrame& original, const MessageFrame& deserialized) {
        EXPECT_EQ(original.message_type, deserialized.message_type);
        EXPECT_EQ(original.source_id, deserialized.source_id);
        EXPECT_EQ(original.payload_size, deserialized.payload_size);
        EXPECT_EQ(original.filename_length, deserialized.filename_length);
        EXPECT_EQ(original.initialization_vector, deserialized.initialization_vector);

        if (original.payload_stream && deserialized.payload_stream) {
            std::string original_payload, deserialized_payload;
            original.payload_stream->seekg(0);
            deserialized.payload_stream->seekg(0);

            original_payload.resize(original.payload_size);
            deserialized_payload.resize(deserialized.payload_size);

            original.payload_stream->read(&original_payload[0], original.payload_size);
            deserialized.payload_stream->read(&deserialized_payload[0], deserialized.payload_size);

            EXPECT_EQ(original_payload, deserialized_payload);
        } else {
            EXPECT_EQ(original.payload_stream == nullptr, deserialized.payload_stream == nullptr);
        }
    }
};

// Test encryption functionality
TEST_F(CodecTest, EncryptionVerification) {
    MessageFrame original = createTestFrame("Sensitive Data");
    std::stringstream buffer;

    // Serialize (which encrypts)
    codec_->serialize(original, buffer);
    std::string serialized = buffer.str();
    BOOST_LOG_TRIVIAL(debug) << "Serialized data size: " << serialized.size();

    // Verify the sensitive data is not visible in plain text
    EXPECT_EQ(serialized.find("Sensitive Data"), std::string::npos);

    // Deserialize and verify we can recover the original data
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);

    std::string recovered_payload;
    recovered_payload.resize(deserialized.payload_size);
    deserialized.payload_stream->read(&recovered_payload[0], deserialized.payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Recovered payload size: " << recovered_payload.size();
    EXPECT_EQ(recovered_payload, "Sensitive Data");
}

// Test large message handling
TEST_F(CodecTest, LargeMessageHandling) {
    // Create a large payload (1MB)
    std::string large_payload;
    large_payload.reserve(1024 * 1024);  // 1MB
    for (int i = 0; i < 1024 * 1024; i++) {
        large_payload += static_cast<char>(i % 256);
    }
    BOOST_LOG_TRIVIAL(debug) << "Created large payload of size: " << large_payload.size();

    MessageFrame original = createTestFrame(large_payload);
    std::stringstream buffer;

    // Serialize
    std::size_t bytes_written = codec_->serialize(original, buffer);
    BOOST_LOG_TRIVIAL(info) << "Serialized large message frame, bytes written: " << bytes_written;
    ASSERT_GT(bytes_written, large_payload.size());

    // Deserialize and verify
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);
    verifyFramesEqual(original, deserialized);
}