#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <memory>
#include "network/codec.hpp"
#include "network/channel.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    std::vector<uint8_t> encryption_key;
    std::shared_ptr<Codec> codec;
    std::shared_ptr<Channel> channel;
    
    void SetUp() override {
        // Initialize 256-bit (32 byte) encryption key
        encryption_key.resize(32, 0x42);  // Test key filled with 0x42
        codec = std::make_shared<Codec>(encryption_key);
        channel = std::make_shared<Channel>();
    }

    // Helper to create a test message frame
    MessageFrame createTestFrame(const std::string& payload = "test payload", 
                               MessageType type = MessageType::Data) {
        MessageFrame frame;
        frame.message_type = type;
        frame.source_id = 1;
        frame.payload_size = payload.size();
        frame.filename_length = 8;  // Default test filename length
        
        // Generate test IV
        std::generate(frame.initialization_vector.begin(), 
                     frame.initialization_vector.end(), 
                     []() { return static_cast<uint8_t>(0x24); });

        // Set up payload stream
        frame.payload_stream = std::make_shared<std::stringstream>(payload);
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
            original.payload_stream->seekg(0);
            deserialized.payload_stream->seekg(0);
            
            std::getline(*original.payload_stream, original_payload, '\0');
            std::getline(*deserialized.payload_stream, deserialized_payload, '\0');
            
            EXPECT_EQ(original_payload, deserialized_payload);
        } else {
            EXPECT_EQ(original.payload_stream == nullptr, deserialized.payload_stream == nullptr);
        }
    }
};

// Test basic message serialization/deserialization
TEST_F(CodecTest, BasicSerializationDeserialization) {
    // Create a simple test frame
    MessageFrame original = createTestFrame("Hello, World!");
    std::stringstream buffer;
    
    // Serialize
    std::size_t bytes_written = codec->serialize(original, buffer);
    ASSERT_GT(bytes_written, 0);
    ASSERT_NE(buffer.str().find("Hello, World!"), std::string::npos);
    
    // Deserialize
    MessageFrame deserialized;
    std::size_t bytes_read = codec->deserialize(buffer, deserialized, *channel);
    ASSERT_EQ(bytes_written, bytes_read);
    
    // Verify
    verifyFramesEqual(original, deserialized);
}

// Test encryption functionality
TEST_F(CodecTest, EncryptionVerification) {
    MessageFrame original = createTestFrame("Sensitive Data");
    std::stringstream buffer;
    
    // Serialize (which encrypts)
    codec->serialize(original, buffer);
    std::string serialized = buffer.str();
    
    // Verify the sensitive data is not visible in plain text
    EXPECT_EQ(serialized.find("Sensitive Data"), std::string::npos);
    
    // Deserialize and verify we can recover the original data
    MessageFrame deserialized;
    codec->deserialize(buffer, deserialized, *channel);
    
    std::string recovered_payload;
    deserialized.payload_stream->seekg(0);
    std::getline(*deserialized.payload_stream, recovered_payload, '\0');
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
    
    MessageFrame original = createTestFrame(large_payload);
    std::stringstream buffer;
    
    // Serialize
    std::size_t bytes_written = codec->serialize(original, buffer);
    ASSERT_GT(bytes_written, large_payload.size());
    
    // Deserialize
    MessageFrame deserialized;
    std::size_t bytes_read = codec->deserialize(buffer, deserialized, *channel);
    ASSERT_EQ(bytes_written, bytes_read);
    
    // Verify
    verifyFramesEqual(original, deserialized);
}

// Test channel integration
TEST_F(CodecTest, ChannelIntegration) {
    MessageFrame original = createTestFrame("Channel Test Data");
    std::stringstream buffer;
    
    // Serialize and deserialize through channel
    codec->serialize(original, buffer);
    
    MessageFrame deserialized;
    codec->deserialize(buffer, deserialized, *channel);
    
    // Verify channel received the message
    ASSERT_EQ(channel->size(), 1);
    
    // Consume from channel
    auto received_frame = channel->consume();
    ASSERT_TRUE(received_frame.has_value());
    
    // Verify frame from channel matches original
    verifyFramesEqual(original, *received_frame);
}
