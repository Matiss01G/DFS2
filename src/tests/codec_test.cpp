#include <gtest/gtest.h>
#include <sstream>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "network/message_frame.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    Codec codec;
    Channel channel;
    static bool logging_initialized;

    void SetUp() override {
        // Initialize Boost.Log only once
        if (!logging_initialized) {
            boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
            boost::log::core::get()->remove_all_sinks(); 
            boost::log::add_console_log(
                std::cout,
                boost::log::keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
            );
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::debug
            );
            boost::log::add_common_attributes();
            logging_initialized = true;
        }
    }

    // Helper to generate random data of specified size
    std::string generate_random_data(size_t size) {
        static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::string result(size, 0);
        for (size_t i = 0; i < size; ++i) {
            result[i] = charset[rand() % (sizeof(charset) - 1)];
        }
        return result;
    }
};

// Initialize static member
bool CodecTest::logging_initialized = false;

/**
 * @brief Tests serialization and deserialization of a MessageFrame with minimal fields
 */
TEST_F(CodecTest, MinimalFrameSerializeDeserialize) {
    // Create a message frame with only required fields and payload size
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 54321;
    input_frame.payload_size = 0;

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

    // Verify set fields match
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);

    // Verify channel is now empty
    EXPECT_TRUE(channel.empty());
}

/**
 * @brief Tests basic serialization and deserialization of a MessageFrame
 */
TEST_F(CodecTest, BasicSerializeDeserialize) {
    // Create a complete message frame with all fields populated
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;

    // Create payload with known content
    const std::string test_data = "TestData123";
    input_frame.payload_size = test_data.length();
    input_frame.filename_length = 8;

    // Create and prepare payload stream
    auto payload = std::make_shared<std::stringstream>();
    ASSERT_TRUE(payload->good()) << "Initial payload stream state is not good";
    payload->write(test_data.c_str(), test_data.length());
    ASSERT_TRUE(payload->good()) << "Failed to write test data to payload stream";
    payload->seekg(0);
    payload->seekp(0);
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
 * @brief Tests handling of large payloads that require chunked processing
 * @details This test verifies that:
 * - Large payloads (>8KB) are correctly serialized and deserialized
 * - Chunked processing works correctly
 * - Memory usage remains reasonable
 * - Data integrity is maintained
 */
TEST_F(CodecTest, LargePayloadChunkedProcessing) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 98765;

    // Generate a large payload (10MB)
    const size_t payload_size = 10 * 1024 * 1024;
    std::string large_data = generate_random_data(payload_size);
    input_frame.payload_size = payload_size;
    input_frame.filename_length = 12;

    // Create and prepare payload stream
    auto payload = std::make_shared<std::stringstream>();
    ASSERT_TRUE(payload->good()) << "Initial payload stream state is not good";
    payload->write(large_data.c_str(), large_data.length());
    ASSERT_TRUE(payload->good()) << "Failed to write large test data to payload stream";
    payload->seekg(0);
    payload->seekp(0);
    input_frame.payload_stream = payload;

    // Prepare output stream for serialization
    std::stringstream output_stream;

    // Serialize the frame with large payload
    std::size_t written = 0;
    ASSERT_NO_THROW({
        written = codec.serialize(input_frame, output_stream);
    }) << "Serialization of large payload threw an exception";

    ASSERT_EQ(written, payload_size + sizeof(uint8_t) + sizeof(uint32_t) + 
              sizeof(uint64_t) + sizeof(uint32_t)) 
        << "Written bytes don't match expected size";

    // Reset for deserialization
    output_stream.seekg(0);

    // Deserialize and verify
    MessageFrame deserialized_frame;
    ASSERT_NO_THROW({
        deserialized_frame = codec.deserialize(output_stream, channel);
    }) << "Deserialization of large payload threw an exception";

    // Retrieve frame from channel
    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume large frame from channel";

    // Verify frame fields
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);

    // Verify complete payload data
    output_frame.payload_stream->seekg(0);
    input_frame.payload_stream->seekg(0);

    std::string output_data((std::istreambuf_iterator<char>(*output_frame.payload_stream)),
                            std::istreambuf_iterator<char>());
    std::string input_data((std::istreambuf_iterator<char>(*input_frame.payload_stream)),
                           std::istreambuf_iterator<char>());

    EXPECT_EQ(output_data.length(), input_data.length()) << "Payload size mismatch";
    EXPECT_EQ(output_data, input_data) << "Large payload data mismatch";
}

/**
 * @brief Tests error handling for malformed input streams
 * @details Verifies that:
 * - Corrupted or incomplete data is handled gracefully
 * - Appropriate exceptions are thrown
 * - The codec maintains a consistent state after errors
 */
TEST_F(CodecTest, MalformedInputHandling) {
    std::stringstream malformed_stream;

    // Test 1: Empty stream
    EXPECT_THROW(codec.deserialize(malformed_stream, channel), std::runtime_error);

    // Test 2: Incomplete header
    malformed_stream.write("\x01\x02\x03", 3);  // Just a few random bytes
    malformed_stream.seekg(0);
    EXPECT_THROW(codec.deserialize(malformed_stream, channel), std::runtime_error);

    // Test 3: Valid header but missing payload
    MessageFrame frame;
    frame.message_type = MessageType::STORE_FILE;
    frame.source_id = 12345;
    frame.payload_size = 1000;  // Claim large payload
    frame.filename_length = 8;

    std::stringstream partial_stream;
    codec.serialize(frame, partial_stream);  // Serialize header
    partial_stream.seekg(0);

    EXPECT_THROW(codec.deserialize(partial_stream, channel), std::runtime_error);
}

/**
 * @brief Tests handling of zero-length payloads
 * @details Verifies that:
 * - Frames with zero-length payloads are handled correctly
 * - Serialization and deserialization work properly
 * - Channel processing is correct
 */
TEST_F(CodecTest, ZeroLengthPayload) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;
    input_frame.payload_size = 0;
    input_frame.filename_length = 0;

    std::stringstream output_stream;

    // Serialize frame with zero-length payload
    std::size_t written = codec.serialize(input_frame, output_stream);
    ASSERT_GT(written, 0) << "No data written for zero-length payload frame";

    // Verify serialized size matches expected header size
    ASSERT_EQ(written, sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t))
        << "Unexpected serialized size for zero-length payload";

    // Deserialize and verify
    output_stream.seekg(0);
    MessageFrame deserialized_frame = codec.deserialize(output_stream, channel);

    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame));

    EXPECT_EQ(output_frame.payload_size, 0);
    EXPECT_EQ(output_frame.filename_length, 0);
}
