#include <gtest/gtest.h>
#include <sstream>
#include <boost/endian/conversion.hpp>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "network/message_frame.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    Codec codec;
    Channel channel;
};

/**
 * @brief Tests basic serialization and deserialization of a MessageFrame
 * @details Verifies that:
 * - A message frame can be serialized to a stream
 * - The same message can be deserialized back
 * - All fields maintain their values through the process
 */
TEST_F(CodecTest, BasicSerializeDeserialize) {
    // Create a test message frame
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;
    input_frame.filename_length = 8;  // "test.txt"
    input_frame.payload_size = 5;

    // Set initialization vector (for AES-256-CBC)
    for (size_t i = 0; i < input_frame.initialization_vector.size(); ++i) {
        input_frame.initialization_vector[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Create payload
    auto payload = std::make_shared<std::stringstream>();
    payload->write("Hello", 5);
    input_frame.payload_stream = payload;

    // Serialize to stringstream
    std::stringstream buffer;
    std::size_t written_bytes = codec.serialize(input_frame, buffer);

    // Should write more than header size
    EXPECT_GT(written_bytes, 
        input_frame.initialization_vector.size() + 
        sizeof(MessageType) + 
        sizeof(uint32_t) + // source_id
        sizeof(uint32_t) + // filename_length
        sizeof(uint64_t)   // payload_size
    );

    // Deserialize and verify
    MessageFrame output_frame = codec.deserialize(buffer, channel);

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);

    // Verify initialization vector
    EXPECT_TRUE(std::equal(
        output_frame.initialization_vector.begin(),
        output_frame.initialization_vector.end(),
        input_frame.initialization_vector.begin()
    ));

    // Verify payload
    if (output_frame.payload_stream && input_frame.payload_stream) {
        std::string input_data = input_frame.payload_stream->str();
        std::string output_data = output_frame.payload_stream->str();
        EXPECT_EQ(output_data, input_data);
    } else {
        FAIL() << "Payload streams not properly handled";
    }
}

/**
 * @brief Tests serialization and deserialization of a MessageFrame with large payload
 * @details Verifies that:
 * - A message frame with large payload can be serialized correctly
 * - The payload is properly chunked during serialization
 * - The message can be deserialized back with exact payload contents
 * - All fields maintain their values through the process
 */
TEST_F(CodecTest, LargePayloadSerializeDeserialize) {
    // Create a test message frame
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 54321;
    input_frame.filename_length = 12;  // "testfile.dat"

    // Create a large payload (50KB) with varied content
    const size_t payload_size = 50 * 1024;  // 50KB
    auto payload = std::make_shared<std::stringstream>();
    std::string test_pattern;
    test_pattern.reserve(1024);  // 1KB pattern

    // Create a repeating but varied pattern
    for (size_t i = 0; i < 256; ++i) {
        test_pattern += static_cast<char>(i & 0xFF);
        if (i % 2 == 0) {
            test_pattern += "testing";
        } else {
            test_pattern += "payload";
        }
    }

    // Repeat the pattern to fill the payload
    for (size_t i = 0; i < (payload_size / test_pattern.size()) + 1; ++i) {
        payload->write(test_pattern.c_str(), test_pattern.size());
    }

    input_frame.payload_size = payload->tellp();
    input_frame.payload_stream = payload;

    // Set initialization vector
    for (size_t i = 0; i < input_frame.initialization_vector.size(); ++i) {
        input_frame.initialization_vector[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Serialize to stringstream
    std::stringstream buffer;
    std::size_t written_bytes = codec.serialize(input_frame, buffer);

    // Verify written bytes matches expected size
    std::size_t expected_size = 
        input_frame.initialization_vector.size() + 
        sizeof(MessageType) + 
        sizeof(uint32_t) + // source_id
        sizeof(uint32_t) + // filename_length
        sizeof(uint64_t) + // payload_size
        input_frame.payload_size;

    EXPECT_EQ(written_bytes, expected_size);

    // Deserialize and verify
    MessageFrame output_frame = codec.deserialize(buffer, channel);

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);

    // Verify initialization vector
    EXPECT_TRUE(std::equal(
        output_frame.initialization_vector.begin(),
        output_frame.initialization_vector.end(),
        input_frame.initialization_vector.begin()
    ));

    // Verify payload content
    ASSERT_TRUE(output_frame.payload_stream) << "Output payload stream is null";
    ASSERT_TRUE(input_frame.payload_stream) << "Input payload stream is null";

    std::string input_data = input_frame.payload_stream->str();
    std::string output_data = output_frame.payload_stream->str();

    EXPECT_EQ(output_data.size(), input_data.size()) 
        << "Payload sizes don't match";
    EXPECT_EQ(output_data, input_data) 
        << "Payload contents don't match";
}

/**
 * @brief Tests handling of empty payload
 * @details Verifies that the Codec correctly handles MessageFrames with no payload
 */
TEST_F(CodecTest, EmptyPayload) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;
    input_frame.filename_length = 0;
    input_frame.payload_size = 0;

    // Set initialization vector
    for (size_t i = 0; i < input_frame.initialization_vector.size(); ++i) {
        input_frame.initialization_vector[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Serialize to stringstream
    std::stringstream buffer;
    std::size_t written_bytes = codec.serialize(input_frame, buffer);

    // Should write exactly header size
    std::size_t expected_size = 
        input_frame.initialization_vector.size() + 
        sizeof(MessageType) + 
        sizeof(uint32_t) + // source_id
        sizeof(uint32_t) + // filename_length
        sizeof(uint64_t);  // payload_size
    EXPECT_EQ(written_bytes, expected_size);

    // Deserialize and verify
    MessageFrame output_frame = codec.deserialize(buffer, channel);

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.payload_size, 0);
    EXPECT_FALSE(output_frame.payload_stream);

    // Verify initialization vector
    EXPECT_TRUE(std::equal(
        output_frame.initialization_vector.begin(),
        output_frame.initialization_vector.end(),
        input_frame.initialization_vector.begin()
    ));
}

/**
 * @brief Tests error handling for invalid input streams
 * @details Verifies that the Codec properly handles error cases
 */
TEST_F(CodecTest, ErrorHandling) {
    std::stringstream bad_stream;
    bad_stream.setstate(std::ios::badbit);

    MessageFrame frame;
    EXPECT_THROW(codec.serialize(frame, bad_stream), std::runtime_error);
    EXPECT_THROW(codec.deserialize(bad_stream, channel), std::runtime_error);
}

/**
 * @brief Tests Channel integration
 * @details Verifies that:
 * - Deserialized messages are properly added to the channel
 * - Channel size increases appropriately
 * - Messages can be retrieved from the channel
 */
TEST_F(CodecTest, ChannelIntegration) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;
    input_frame.filename_length = 0;
    input_frame.payload_size = 0;

    // Set initialization vector
    for (size_t i = 0; i < input_frame.initialization_vector.size(); ++i) {
        input_frame.initialization_vector[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Serialize message
    std::stringstream buffer;
    codec.serialize(input_frame, buffer);

    EXPECT_TRUE(channel.empty());

    // Deserialize should add to channel
    MessageFrame output_frame = codec.deserialize(buffer, channel);

    EXPECT_FALSE(channel.empty());
    EXPECT_EQ(channel.size(), 1);

    // Verify message in channel
    MessageFrame retrieved_frame;
    EXPECT_TRUE(channel.consume(retrieved_frame));

    EXPECT_EQ(retrieved_frame.message_type, input_frame.message_type);
    EXPECT_EQ(retrieved_frame.source_id, input_frame.source_id);
    EXPECT_EQ(retrieved_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(retrieved_frame.payload_size, input_frame.payload_size);

    // Verify initialization vector
    EXPECT_TRUE(std::equal(
        retrieved_frame.initialization_vector.begin(),
        retrieved_frame.initialization_vector.end(),
        input_frame.initialization_vector.begin()
    ));
}