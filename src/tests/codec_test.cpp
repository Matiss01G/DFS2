#include <gtest/gtest.h>
#include <sstream>
#include <memory>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "network/message_frame.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    Codec codec;
    Channel channel;
    
    // Helper function to create a test message frame
    MessageFrame createTestFrame(const std::string& payload_data = "") {
        MessageFrame frame;
        frame.message_type = MessageType::STORE_FILE;
        frame.source_id = 12345;
        frame.filename_length = 8;
        frame.payload_size = payload_data.length();
        
        if (!payload_data.empty()) {
            auto payload = std::make_shared<std::stringstream>();
            payload->write(payload_data.c_str(), payload_data.length());
            frame.payload_stream = payload;
        }
        
        return frame;
    }
};

/**
 * @brief Tests basic serialization of a MessageFrame without payload
 * @details Verifies that:
 * - A MessageFrame can be properly serialized to a stream
 * - All basic fields are written correctly
 * Expected outcome: Serialization succeeds and returns correct number of bytes
 */
TEST_F(CodecTest, BasicSerialization) {
    MessageFrame frame = createTestFrame();
    std::stringstream output;
    
    std::size_t bytes_written = codec.serialize(frame, output);
    
    // Calculate expected size: IV + message_type + source_id + filename_length + payload_size
    std::size_t expected_size = frame.initialization_vector.size() + 
                               sizeof(MessageType) +
                               sizeof(uint32_t) +  // source_id
                               sizeof(uint32_t) +  // filename_length
                               sizeof(uint64_t);   // payload_size
    
    EXPECT_EQ(bytes_written, expected_size);
    EXPECT_GT(output.str().length(), 0);
}

/**
 * @brief Tests basic deserialization of a MessageFrame without payload
 * @details Verifies that:
 * - A serialized MessageFrame can be properly deserialized
 * - All fields match the original values
 * - The frame is properly pushed to the channel
 * Expected outcome: Deserialization succeeds with all fields matching original values
 */
TEST_F(CodecTest, BasicDeserialization) {
    MessageFrame input_frame = createTestFrame();
    std::stringstream buffer;
    
    // Serialize the frame
    codec.serialize(input_frame, buffer);
    
    // Deserialize the frame
    MessageFrame output_frame = codec.deserialize(buffer, channel);
    
    // Verify fields
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    
    // Verify initialization vector
    EXPECT_EQ(output_frame.initialization_vector, input_frame.initialization_vector);
    
    // Verify channel received the frame
    EXPECT_EQ(channel.size(), 1);
}

/**
 * @brief Tests serialization and deserialization with payload data
 * @details Verifies that:
 * - A MessageFrame with payload can be properly serialized
 * - The payload is correctly written and read
 * - The entire frame can be deserialized with payload intact
 * Expected outcome: Payload data remains unchanged through ser/de cycle
 */
TEST_F(CodecTest, PayloadHandling) {
    std::string test_payload = "Hello, World!";
    MessageFrame input_frame = createTestFrame(test_payload);
    std::stringstream buffer;
    
    // Serialize the frame
    std::size_t bytes_written = codec.serialize(input_frame, buffer);
    
    // Calculate expected size (basic fields + payload)
    std::size_t expected_size = input_frame.initialization_vector.size() + 
                               sizeof(MessageType) +
                               sizeof(uint32_t) +  // source_id
                               sizeof(uint32_t) +  // filename_length
                               sizeof(uint64_t) +  // payload_size
                               test_payload.length();
    
    EXPECT_EQ(bytes_written, expected_size);
    
    // Deserialize the frame
    MessageFrame output_frame = codec.deserialize(buffer, channel);
    
    // Verify payload
    ASSERT_TRUE(output_frame.payload_stream != nullptr);
    std::string output_payload = output_frame.payload_stream->str();
    EXPECT_EQ(output_payload, test_payload);
}

/**
 * @brief Tests error handling for invalid input/output streams
 * @details Verifies that:
 * - Codec properly handles invalid input streams
 * - Codec properly handles invalid output streams
 * Expected outcome: Appropriate exceptions are thrown
 */
TEST_F(CodecTest, ErrorHandling) {
    MessageFrame frame = createTestFrame();
    
    // Test invalid output stream
    std::stringstream bad_output;
    bad_output.setstate(std::ios::badbit);
    EXPECT_THROW(codec.serialize(frame, bad_output), std::runtime_error);
    
    // Test invalid input stream
    std::stringstream bad_input;
    bad_input.setstate(std::ios::badbit);
    EXPECT_THROW(codec.deserialize(bad_input, channel), std::runtime_error);
}
