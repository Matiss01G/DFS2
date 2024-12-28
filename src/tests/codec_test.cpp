#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <memory>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "crypto/crypto_stream.hpp"
#include <boost/log/trivial.hpp>

using namespace dfs::network;
using namespace dfs::crypto;

class CodecTest : public ::testing::Test {
protected:
    std::vector<uint8_t> key_;
    std::unique_ptr<Codec> codec_;
    std::unique_ptr<Channel> channel_;

    void SetUp() override {
        BOOST_LOG_TRIVIAL(info) << "Setting up CodecTest fixture";
        // Initialize with 256-bit test key
        key_.resize(32, 0x42); // 32 bytes = 256 bits
        codec_ = std::make_unique<Codec>(key_);
        channel_ = std::make_unique<Channel>();
        BOOST_LOG_TRIVIAL(debug) << "Test fixture initialized with key size: " << key_.size();
    }

    void TearDown() override {
        BOOST_LOG_TRIVIAL(debug) << "Tearing down CodecTest fixture";
        channel_.reset();
        codec_.reset();
    }

    // Helper to create a test message frame
    MessageFrame create_test_frame(const std::string& payload, MessageType type = MessageType::STORE_FILE) {
        BOOST_LOG_TRIVIAL(debug) << "Creating test frame with payload size: " << payload.size();
        MessageFrame frame;

        // Generate IV using CryptoStream
        CryptoStream crypto;
        frame.initialization_vector = {0}; // Initialize with zeros
        auto generated_iv = crypto.generate_IV();
        std::copy(generated_iv.begin(), generated_iv.end(), frame.initialization_vector.begin());

        frame.message_type = type;
        frame.source_id = 1;
        frame.payload_size = payload.size();
        frame.filename_length = 8; // Example fixed length
        frame.payload_stream = std::make_shared<std::stringstream>();
        frame.payload_stream->write(payload.data(), payload.size());
        frame.payload_stream->seekg(0);

        BOOST_LOG_TRIVIAL(debug) << "Test frame created with type: " << static_cast<int>(type);
        return frame;
    }

    // Helper to verify streams are equal
    bool verify_streams_equal(std::istream& expected, std::istream& actual) {
        expected.seekg(0);
        actual.seekg(0);

        std::string expected_str((std::istreambuf_iterator<char>(expected)),
                                std::istreambuf_iterator<char>());
        std::string actual_str((std::istreambuf_iterator<char>(actual)),
                              std::istreambuf_iterator<char>());

        return expected_str == actual_str;
    }
};

// Test basic encryption/decryption functionality
TEST_F(CodecTest, EncryptionVerification) {
    BOOST_LOG_TRIVIAL(info) << "Starting EncryptionVerification test";
    const std::string sensitive_data = "Sensitive Data";
    auto frame = create_test_frame(sensitive_data);

    // Serialize
    std::stringstream serialized;
    size_t written = codec_->serialize(frame, serialized);
    ASSERT_GT(written, 0) << "Serialization should write data";

    BOOST_LOG_TRIVIAL(debug) << "Serialized data size: " << written;

    // Verify sensitive data is not in plain text
    std::string serialized_str = serialized.str();
    BOOST_LOG_TRIVIAL(debug) << "Verified sensitive data is not in plain text";
    EXPECT_EQ(serialized_str.find(sensitive_data), std::string::npos) 
        << "Sensitive data should not appear in plain text";

    // Deserialize
    serialized.seekg(0);
    MessageFrame recovered_frame = codec_->deserialize(serialized, *channel_);

    // Verify payload
    std::string recovered_payload;
    recovered_frame.payload_stream->seekg(0);
    std::getline(*recovered_frame.payload_stream, recovered_payload);

    BOOST_LOG_TRIVIAL(debug) << "Recovered payload size: " << recovered_payload.size();
    EXPECT_EQ(recovered_payload, sensitive_data);
    EXPECT_EQ(recovered_frame.payload_size, sensitive_data.size());
    EXPECT_EQ(recovered_frame.message_type, frame.message_type);
    EXPECT_EQ(recovered_frame.source_id, frame.source_id);

    BOOST_LOG_TRIVIAL(info) << "EncryptionVerification test completed successfully";
}

// Test handling of large messages
TEST_F(CodecTest, LargeMessageHandling) {
    BOOST_LOG_TRIVIAL(info) << "Starting LargeMessageHandling test";

    // Create 64KB test payload (reduced from 1MB to avoid memory issues)
    const size_t large_size = 64 * 1024;
    std::string large_payload(large_size, 'X');
    BOOST_LOG_TRIVIAL(debug) << "Created large payload of size: " << large_payload.size();

    auto frame = create_test_frame(large_payload);

    // Serialize
    std::stringstream serialized;
    size_t written = 0;
    ASSERT_NO_THROW({
        written = codec_->serialize(frame, serialized);
    }) << "Failed to serialize large message";
    ASSERT_GT(written, large_size) << "Serialized size should be greater than payload size";

    // Deserialize
    serialized.seekg(0);
    MessageFrame recovered_frame;
    ASSERT_NO_THROW({
        recovered_frame = codec_->deserialize(serialized, *channel_);
    }) << "Failed to deserialize large message";

    // Verify payload size and content
    ASSERT_TRUE(recovered_frame.payload_stream != nullptr);
    recovered_frame.payload_stream->seekg(0);
    std::string recovered_data((std::istreambuf_iterator<char>(*recovered_frame.payload_stream)),
                              std::istreambuf_iterator<char>());

    EXPECT_EQ(recovered_data.size(), large_size);
    EXPECT_EQ(recovered_data, large_payload);

    BOOST_LOG_TRIVIAL(info) << "LargeMessageHandling test completed successfully";
}

// Test handling of empty messages
TEST_F(CodecTest, EmptyMessageHandling) {
    BOOST_LOG_TRIVIAL(info) << "Starting EmptyMessageHandling test";

    auto frame = create_test_frame("");

    // Serialize
    std::stringstream serialized;
    size_t written = codec_->serialize(frame, serialized);
    ASSERT_GT(written, 0) << "Should write header even for empty payload";

    // Deserialize
    serialized.seekg(0);
    MessageFrame recovered_frame = codec_->deserialize(serialized, *channel_);

    EXPECT_EQ(recovered_frame.payload_size, 0);
    EXPECT_TRUE(recovered_frame.payload_stream != nullptr);

    std::string recovered_data((std::istreambuf_iterator<char>(*recovered_frame.payload_stream)),
                              std::istreambuf_iterator<char>());
    EXPECT_TRUE(recovered_data.empty());

    BOOST_LOG_TRIVIAL(info) << "EmptyMessageHandling test completed successfully";
}

// Test channel integration
TEST_F(CodecTest, ChannelIntegration) {
    BOOST_LOG_TRIVIAL(info) << "Starting ChannelIntegration test";

    const std::vector<std::string> test_messages = {
        "First Message",
        "Second Message",
        "Third Message"
    };

    // Serialize and deserialize multiple messages
    for (const auto& msg : test_messages) {
        auto frame = create_test_frame(msg);
        std::stringstream serialized;

        ASSERT_NO_THROW({
            codec_->serialize(frame, serialized);
            serialized.seekg(0);
            codec_->deserialize(serialized, *channel_);
        }) << "Failed to process message: " << msg;
    }

    // Verify messages in channel
    EXPECT_EQ(channel_->size(), test_messages.size());

    // Consume and verify messages
    for (const auto& expected_msg : test_messages) {
        MessageFrame frame;
        ASSERT_TRUE(channel_->consume(frame)) << "Failed to consume message from channel";

        std::string received_msg((std::istreambuf_iterator<char>(*frame.payload_stream)),
                               std::istreambuf_iterator<char>());
        EXPECT_EQ(received_msg, expected_msg);
    }

    EXPECT_EQ(channel_->size(), 0) << "Channel should be empty after consuming all messages";
    BOOST_LOG_TRIVIAL(info) << "ChannelIntegration test completed successfully";
}

// Test error handling
TEST_F(CodecTest, ErrorHandling) {
    BOOST_LOG_TRIVIAL(info) << "Starting ErrorHandling test";

    // Test invalid stream state
    std::stringstream bad_stream;
    bad_stream.setstate(std::ios::badbit);
    auto frame = create_test_frame("Test");

    EXPECT_THROW(codec_->serialize(frame, bad_stream), std::runtime_error);
    EXPECT_THROW(codec_->deserialize(bad_stream, *channel_), std::runtime_error);

    // Test invalid message size
    try {
        MessageFrame invalid_frame = create_test_frame("Test");
        invalid_frame.payload_size = std::numeric_limits<uint64_t>::max(); // Impossibly large size
        std::stringstream serialized;
        codec_->serialize(invalid_frame, serialized);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error&) {
        // Expected exception
    } catch (const std::exception& e) {
        FAIL() << "Expected std::runtime_error but got: " << e.what();
    }

    BOOST_LOG_TRIVIAL(info) << "ErrorHandling test completed successfully";
}