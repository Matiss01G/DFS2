#include <gtest/gtest.h>
#include <sstream>
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
 * - A complete MessageFrame can be serialized with all fields populated
 * - The serialized data can be correctly deserialized
 * - All fields match between original and deserialized frames
 * - The Channel system correctly handles the deserialized frame
 * Expected outcome: All fields match between original and deserialized frames
 */
TEST_F(CodecTest, BasicSerializeDeserialize) {
    // Create a complete message frame with all fields populated
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;

    // Create payload with known content
    const std::string test_data = "TestData123";
    input_frame.payload_size = test_data.length(); // Set payload size to match data length
    input_frame.filename_length = 8;

    // Create and prepare payload stream
    auto payload = std::make_shared<std::stringstream>();
    ASSERT_TRUE(payload->good()) << "Initial payload stream state is not good";
    payload->write(test_data.c_str(), test_data.length());
    ASSERT_TRUE(payload->good()) << "Failed to write test data to payload stream";
    payload->seekg(0);
    input_frame.payload_stream = payload;

    // Prepare output stream for serialization
    std::stringstream output_stream;
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state before serialization";

    // Serialize the frame
    std::size_t written = 0;
    ASSERT_NO_THROW({
        written = codec.serialize(input_frame, output_stream);
    }) << "Serialization threw an exception";
    ASSERT_GT(written, 0) << "No data was written during serialization";
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state after serialization";

    // Verify the output stream contains data
    std::string serialized_data = output_stream.str();
    ASSERT_FALSE(serialized_data.empty()) << "Serialized data is empty";
    ASSERT_GE(serialized_data.length(), written) << "Serialized data length mismatch";

    // Reset stream position for reading
    output_stream.seekg(0);
    ASSERT_TRUE(output_stream.good()) << "Failed to reset output stream position";

    // Deserialize the frame
    MessageFrame deserialized_frame;
    ASSERT_NO_THROW({
        deserialized_frame = codec.deserialize(output_stream, channel);
    }) << "Deserialization threw an exception";

    // Retrieve frame from channel
    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume frame from channel";

    // Verify all frame fields match
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);

    // Verify payload streams exist
    ASSERT_TRUE(output_frame.payload_stream != nullptr) << "Output payload stream is null";
    ASSERT_TRUE(input_frame.payload_stream != nullptr) << "Input payload stream is null";

    // Reset stream positions for comparison
    input_frame.payload_stream->seekg(0);
    output_frame.payload_stream->seekg(0);

    // Read and compare payload data
    std::string input_data = input_frame.payload_stream->str();
    std::string output_data = output_frame.payload_stream->str();

    EXPECT_EQ(output_data, input_data) << "Payload data mismatch";
    EXPECT_EQ(output_data, test_data) << "Payload content doesn't match original test data";

    // Verify channel is now empty
    EXPECT_TRUE(channel.empty());
}

/**
 * @brief Tests serialization and deserialization of a minimal MessageFrame
 * @details Verifies that:
 * - A MessageFrame with only required fields (no payload or filename) can be serialized
 * - The serialized data can be correctly deserialized
 * - All fields match between original and deserialized frames
 * - The Channel system correctly handles the minimal frame
 * Expected outcome: All fields match between original and deserialized frames,
 * and payload-related fields are properly handled as empty/zero
 */
TEST_F(CodecTest, MinimalFrameSerializeDeserialize) {
    // Create a minimal message frame with only required fields
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 54321;
    input_frame.payload_size = 0;
    input_frame.filename_length = 0;
    input_frame.payload_stream = nullptr;

    // Prepare output stream for serialization
    std::stringstream output_stream;
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state before serialization";

    // Serialize the frame
    std::size_t written = 0;
    ASSERT_NO_THROW({
        written = codec.serialize(input_frame, output_stream);
    }) << "Serialization threw an exception";
    ASSERT_GT(written, 0) << "No data was written during serialization";
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state after serialization";

    // Verify minimal required data was written
    std::string serialized_data = output_stream.str();
    ASSERT_FALSE(serialized_data.empty()) << "Serialized data is empty";
    ASSERT_GE(serialized_data.length(), written) << "Serialized data length mismatch";

    // Reset stream position for reading
    output_stream.seekg(0);
    ASSERT_TRUE(output_stream.good()) << "Failed to reset output stream position";

    // Deserialize the frame
    MessageFrame deserialized_frame;
    ASSERT_NO_THROW({
        deserialized_frame = codec.deserialize(output_stream, channel);
    }) << "Deserialization threw an exception";

    // Retrieve frame from channel
    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume frame from channel";

    // Verify all frame fields match
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);

    // Verify payload stream is properly handled
    if (input_frame.payload_size == 0) {
        // For zero payload size, payload stream might be initialized as empty or null
        if (output_frame.payload_stream) {
            EXPECT_EQ(output_frame.payload_stream->str(), "") << "Empty payload stream should contain no data";
        }
    }

    // Verify channel is now empty
    EXPECT_TRUE(channel.empty());
}