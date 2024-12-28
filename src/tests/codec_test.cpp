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

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    std::vector<uint8_t> key_;
    Codec* codec_;
    Channel* channel_;

    void SetUp() override {
        // Initialize Boost.Log
        boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");

        // Add console output
        boost::log::add_console_log(
            std::cout,
            boost::log::keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%",
            boost::log::keywords::auto_flush = true
        );

        // Set logging level to debug to see all messages
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::debug
        );

        // Add commonly used attributes
        boost::log::add_common_attributes();

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
        BOOST_LOG_TRIVIAL(debug) << "Creating test frame with payload size: " << payload.size();
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
        BOOST_LOG_TRIVIAL(debug) << "Test frame created with type: " << static_cast<int>(type);
        return frame;
    }

    // Helper to compare two message frames
    void verifyFramesEqual(const MessageFrame& original, const MessageFrame& deserialized) {
        BOOST_LOG_TRIVIAL(debug) << "Verifying frame equality";
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
            BOOST_LOG_TRIVIAL(debug) << "Payload comparison completed";
        } else {
            EXPECT_EQ(original.payload_stream == nullptr, deserialized.payload_stream == nullptr);
            BOOST_LOG_TRIVIAL(debug) << "One or both payloads are null";
        }
        BOOST_LOG_TRIVIAL(debug) << "Frame verification completed";
    }
};

// // Test basic message serialization/deserialization
// TEST_F(CodecTest, BasicSerializationDeserialization) {
//     BOOST_LOG_TRIVIAL(info) << "Starting BasicSerializationDeserialization test";

//     // Create a simple test frame
//     MessageFrame original = createTestFrame("Hello, World!");
//     std::stringstream buffer;

//     // Serialize
//     std::size_t bytes_written = codec_->serialize(original, buffer);
//     BOOST_LOG_TRIVIAL(info) << "Serialized message frame, bytes written: " << bytes_written;
//     ASSERT_GT(bytes_written, 0);

//     // Deserialize
//     MessageFrame deserialized = codec_->deserialize(buffer, *channel_);
//     BOOST_LOG_TRIVIAL(info) << "Deserialized message frame, payload size: " << deserialized.payload_size;
//     ASSERT_GT(deserialized.payload_size, 0);

//     // Verify
//     verifyFramesEqual(original, deserialized);
//     BOOST_LOG_TRIVIAL(info) << "BasicSerializationDeserialization test completed successfully";
// }

// Test encryption functionality
TEST_F(CodecTest, EncryptionVerification) {
    BOOST_LOG_TRIVIAL(info) << "Starting EncryptionVerification test";
    MessageFrame original = createTestFrame("Sensitive Data");
    std::stringstream buffer;

    // Serialize (which encrypts)
    codec_->serialize(original, buffer);
    std::string serialized = buffer.str();
    BOOST_LOG_TRIVIAL(debug) << "Serialized data size: " << serialized.size();

    // Verify the sensitive data is not visible in plain text
    EXPECT_EQ(serialized.find("Sensitive Data"), std::string::npos);
    BOOST_LOG_TRIVIAL(debug) << "Verified sensitive data is not in plain text";

    // Deserialize and verify we can recover the original data
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);

    std::string recovered_payload;
    recovered_payload.resize(deserialized.payload_size);
    deserialized.payload_stream->read(&recovered_payload[0], deserialized.payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Recovered payload size: " << recovered_payload.size();
    EXPECT_EQ(recovered_payload, "Sensitive Data");
    BOOST_LOG_TRIVIAL(info) << "EncryptionVerification test completed successfully";
}

// Test large message handling
TEST_F(CodecTest, LargeMessageHandling) {
    BOOST_LOG_TRIVIAL(info) << "Starting LargeMessageHandling test";
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

    // Deserialize
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);
    BOOST_LOG_TRIVIAL(info) << "Deserialized large message frame, payload size: " << deserialized.payload_size;
    ASSERT_GT(deserialized.payload_size, 0);

    // Verify
    verifyFramesEqual(original, deserialized);
    BOOST_LOG_TRIVIAL(info) << "LargeMessageHandling test completed successfully";
}

// // Test channel integration
// TEST_F(CodecTest, ChannelIntegration) {
//     BOOST_LOG_TRIVIAL(info) << "Starting ChannelIntegration test";
//     MessageFrame original = createTestFrame("Channel Test Data");
//     std::stringstream buffer;

//     // Serialize and deserialize through channel
//     codec_->serialize(original, buffer);
//     MessageFrame deserialized = codec_->deserialize(buffer, *channel_);

//     // Verify channel received the message
//     BOOST_LOG_TRIVIAL(debug) << "Checking channel message count";
//     ASSERT_EQ(channel_->size(), 1);

//     // Consume from channel
//     MessageFrame received;
//     ASSERT_TRUE(channel_->consume(received));
//     BOOST_LOG_TRIVIAL(debug) << "Successfully consumed message from channel";

//     // Verify frame from channel matches original
//     verifyFramesEqual(original, received);
//     BOOST_LOG_TRIVIAL(info) << "ChannelIntegration test completed successfully";
// }

// // Test error handling
// TEST_F(CodecTest, ErrorHandling) {
//     BOOST_LOG_TRIVIAL(info) << "Starting ErrorHandling test";

//     // Test invalid key size
//     BOOST_LOG_TRIVIAL(debug) << "Testing invalid key size";
//     EXPECT_THROW({
//         std::vector<uint8_t> invalid_key(16, 0x42);  // Wrong key size
//         Codec invalid_codec(invalid_key);
//     }, std::invalid_argument);

//     // Test invalid input stream
//     BOOST_LOG_TRIVIAL(debug) << "Testing invalid input stream";
//     MessageFrame frame = createTestFrame();
//     std::stringstream invalid_stream;
//     invalid_stream.setstate(std::ios::badbit);
//     EXPECT_THROW(codec_->serialize(frame, invalid_stream), std::runtime_error);

//     // Test invalid output stream
//     BOOST_LOG_TRIVIAL(debug) << "Testing invalid output stream";
//     std::stringstream invalid_output;
//     invalid_output.setstate(std::ios::badbit);
//     EXPECT_THROW(codec_->deserialize(invalid_output, *channel_), std::runtime_error);

//     BOOST_LOG_TRIVIAL(info) << "ErrorHandling test completed successfully";
// }