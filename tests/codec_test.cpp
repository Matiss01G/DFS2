#include <gtest/gtest.h>
#include <sstream>
#include "network/codec.hpp"
#include "network/message_frame.hpp"
#include "channel/channel.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    Codec codec;
    Channel channel;
};

// Test basic serialization functionality
TEST_F(CodecTest, SerializeBasicFrame) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::DATA;
    input_frame.source_id = 12345;
    input_frame.filename_length = 0;
    input_frame.payload_size = 0;
    
    std::ostringstream output;
    std::size_t bytes_written = codec.serialize(input_frame, output);
    
    EXPECT_GT(bytes_written, 0);
    EXPECT_TRUE(output.good());
}

// Test serialization with payload
TEST_F(CodecTest, SerializeWithPayload) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::DATA;
    input_frame.source_id = 12345;
    input_frame.filename_length = 0;
    input_frame.payload_size = 5;
    
    std::string test_data = "Hello";
    auto payload_stream = std::make_shared<std::stringstream>(test_data);
    input_frame.payload_stream = payload_stream;
    
    std::ostringstream output;
    std::size_t bytes_written = codec.serialize(input_frame, output);
    
    EXPECT_GT(bytes_written, 0);
    EXPECT_TRUE(output.good());
}

// Test deserialization functionality
TEST_F(CodecTest, DeserializeBasicFrame) {
    // First serialize a frame
    MessageFrame input_frame;
    input_frame.message_type = MessageType::DATA;
    input_frame.source_id = 12345;
    input_frame.filename_length = 0;
    input_frame.payload_size = 0;
    
    std::ostringstream output;
    codec.serialize(input_frame, output);
    
    // Now deserialize it
    std::istringstream input(output.str());
    MessageFrame deserialized_frame = codec.deserialize(input, channel);
    
    EXPECT_EQ(deserialized_frame.message_type, input_frame.message_type);
    EXPECT_EQ(deserialized_frame.source_id, input_frame.source_id);
    EXPECT_EQ(deserialized_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(deserialized_frame.payload_size, input_frame.payload_size);
}

// Test channel integration and payload streaming
TEST_F(CodecTest, ChannelIntegrationWithPayload) {
    // Create and serialize a frame with payload
    MessageFrame input_frame;
    input_frame.message_type = MessageType::DATA;
    input_frame.source_id = 12345;
    input_frame.filename_length = 0;
    input_frame.payload_size = 13;
    
    std::string test_data = "Hello, World!";
    auto payload_stream = std::make_shared<std::stringstream>(test_data);
    input_frame.payload_stream = payload_stream;
    
    // Serialize the frame
    std::ostringstream serialized_data;
    codec.serialize(input_frame, serialized_data);
    
    // Deserialize the frame (this will automatically push to channel)
    std::istringstream input(serialized_data.str());
    MessageFrame deserialized_frame = codec.deserialize(input, channel);
    
    // Verify channel received the frame
    EXPECT_FALSE(channel.empty());
    EXPECT_EQ(channel.size(), 1);
    
    // Consume the frame from channel
    MessageFrame received_frame;
    EXPECT_TRUE(channel.consume(received_frame));
    
    // Verify frame headers
    EXPECT_EQ(received_frame.message_type, input_frame.message_type);
    EXPECT_EQ(received_frame.source_id, input_frame.source_id);
    EXPECT_EQ(received_frame.payload_size, input_frame.payload_size);
    
    // Read and verify payload data
    std::string received_data;
    if (received_frame.payload_stream) {
        std::stringstream& ss = *received_frame.payload_stream;
        received_data = ss.str();
    }
    
    EXPECT_EQ(received_data, test_data);
}

// Test error cases
TEST_F(CodecTest, ErrorCases) {
    // Test serialization with invalid output stream
    MessageFrame frame;
    std::stringstream bad_stream;
    bad_stream.setstate(std::ios::badbit);
    
    EXPECT_THROW(codec.serialize(frame, bad_stream), std::runtime_error);
    
    // Test deserialization with invalid input stream
    bad_stream.clear();
    bad_stream.setstate(std::ios::badbit);
    
    EXPECT_THROW(codec.deserialize(bad_stream, channel), std::runtime_error);
}
