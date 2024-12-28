#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <memory>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "crypto/crypto_stream.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    std::vector<uint8_t> key_;
    Codec* codec_;
    Channel* channel_;

    void SetUp() override {
        // Initialize 256-bit (32 byte) encryption key
        key_.resize(32, 0x42);  // Test key filled with 0x42
        codec_ = new Codec(key_);
        channel_ = new Channel();
    }

    void TearDown() override {
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
        frame.payload_stream = std::make_shared<std::stringstream>();
        frame.payload_stream->write(payload.c_str(), payload.size());
        frame.payload_stream->seekg(0);  // Reset stream position for reading
        return frame;
    }

    // Helper to compare two message frames
    void verifyFramesEqual(const MessageFrame& original, const MessageFrame& deserialized) {
        EXPECT_EQ(original.message_type, deserialized.message_type);
        EXPECT_EQ(original.source_id, deserialized.source_id);
        EXPECT_EQ(original.payload_size, deserialized.payload_size);
        EXPECT_EQ(original.filename_length, deserialized.filename_length);
        EXPECT_EQ(original.initialization_vector, deserialized.initialization_vector);

        if (original.payload_stream && deserialized.payload_stream) {
            std::string original_payload, deserialized_payload;

            // Reset stream positions
            original.payload_stream->clear();
            deserialized.payload_stream->clear();
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

    // Helper to verify buffer state
    void resetBufferState(std::stringstream& buffer) {
        buffer.clear();
        buffer.seekg(0);
        buffer.seekp(0);
    }
};

// SERIALIZATION TESTS

TEST_F(CodecTest, BasicSerialization) {
    MessageFrame original = createTestFrame("Hello, World!");
    std::stringstream buffer;

    std::size_t bytes_written = codec_->serialize(original, buffer);
    ASSERT_GT(bytes_written, 0);
    ASSERT_GT(buffer.str().length(), original.payload_size);
}

TEST_F(CodecTest, SerializationEncryption) {
    MessageFrame original = createTestFrame("Sensitive Data");
    std::stringstream buffer;

    codec_->serialize(original, buffer);
    std::string serialized = buffer.str();

    // Verify the sensitive data is not visible in plain text
    EXPECT_EQ(serialized.find("Sensitive Data"), std::string::npos);
}

TEST_F(CodecTest, LargeMessageSerialization) {
    // Create a large payload (1MB)
    std::string large_payload;
    large_payload.reserve(1024 * 1024);  // 1MB
    for (int i = 0; i < 1024 * 1024; i++) {
        large_payload += static_cast<char>(i % 256);
    }

    MessageFrame original = createTestFrame(large_payload);
    std::stringstream buffer;

    std::size_t bytes_written = codec_->serialize(original, buffer);
    ASSERT_GT(bytes_written, large_payload.size());
}

// DESERIALIZATION TESTS

TEST_F(CodecTest, BasicDeserialization) {
    MessageFrame original = createTestFrame("Test Message");
    std::stringstream buffer;

    codec_->serialize(original, buffer);
    resetBufferState(buffer);  // Reset buffer state before deserialization

    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);
    ASSERT_GT(deserialized.payload_size, 0);
    verifyFramesEqual(original, deserialized);
}

TEST_F(CodecTest, EncryptedDataDeserialization) {
    const std::string test_content = "Encrypted Content";
    MessageFrame original = createTestFrame(test_content);
    std::stringstream buffer;

    codec_->serialize(original, buffer);
    resetBufferState(buffer);  // Reset buffer state before deserialization

    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);

    // Reset stream position for reading
    deserialized.payload_stream->clear();
    deserialized.payload_stream->seekg(0);

    std::string recovered_payload(deserialized.payload_size, '\0');
    deserialized.payload_stream->read(&recovered_payload[0], deserialized.payload_size);

    EXPECT_EQ(recovered_payload, test_content);
}

TEST_F(CodecTest, LargeMessageDeserialization) {
    // Create a consistent test pattern for the large message
    std::string large_payload(1024 * 1024, 'X');  // 1MB of 'X' characters
    MessageFrame original = createTestFrame(large_payload);
    std::stringstream buffer;

    codec_->serialize(original, buffer);
    resetBufferState(buffer);  // Reset buffer state before deserialization

    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);
    ASSERT_GT(deserialized.payload_size, 0);

    // Reset stream positions before comparison
    original.payload_stream->clear();
    original.payload_stream->seekg(0);
    deserialized.payload_stream->clear();
    deserialized.payload_stream->seekg(0);

    verifyFramesEqual(original, deserialized);
}

// CHANNEL INTEGRATION TESTS

TEST_F(CodecTest, ChannelIntegration) {
    MessageFrame original = createTestFrame("Channel Test Data");
    std::stringstream buffer;

    // Serialize and deserialize through channel
    codec_->serialize(original, buffer);
    resetBufferState(buffer);  // Reset buffer state before deserialization
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);

    // Verify channel received the message
    ASSERT_EQ(channel_->size(), 1);

    // Consume from channel
    MessageFrame received;
    ASSERT_TRUE(channel_->consume(received));

    // Verify frame from channel matches original
    verifyFramesEqual(original, received);
}

// ERROR HANDLING TESTS

TEST_F(CodecTest, SerializationErrorHandling) {
    MessageFrame frame = createTestFrame();
    std::stringstream invalid_stream;
    invalid_stream.setstate(std::ios::badbit);
    EXPECT_THROW(codec_->serialize(frame, invalid_stream), std::runtime_error);
}

TEST_F(CodecTest, DeserializationErrorHandling) {
    std::stringstream invalid_output;
    invalid_output.setstate(std::ios::badbit);
    EXPECT_THROW(codec_->deserialize(invalid_output, *channel_), std::runtime_error);
}

TEST_F(CodecTest, InvalidKeySize) {
    EXPECT_THROW({
        std::vector<uint8_t> invalid_key(16, 0x42);  // Wrong key size
        Codec invalid_codec(invalid_key);
    }, std::invalid_argument);
}