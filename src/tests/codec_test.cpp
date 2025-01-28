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
#include "network/stream_pipeline.hpp"  // Add the new header
#include "crypto/crypto_stream.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    // Initialize codec with a test key and channel
    std::vector<uint8_t> test_key = std::vector<uint8_t>(32, 0x42); // 32-byte test key
    Channel channel;
    Codec codec{test_key, channel};
    static bool logging_initialized;

    void SetUp() override {
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

    // Helper to create a test IV
    std::vector<uint8_t> generate_test_iv() {
        return std::vector<uint8_t>(dfs::crypto::CryptoStream::IV_SIZE, 0x42);
    }
};

bool CodecTest::logging_initialized = false;

// Test the StreamPipeline with small data
TEST_F(CodecTest, StreamPipelineBasicTest) {
    const std::string test_data = "Hello, StreamPipeline!";

    // Create producer function
    auto producer = [&test_data](std::stringstream& output) -> bool {
        output.write(test_data.c_str(), test_data.length());
        return output.good();
    };

    // Create pipeline
    StreamPipeline pipeline(producer);

    // Read from pipeline
    std::string result((std::istreambuf_iterator<char>(pipeline)),
                      std::istreambuf_iterator<char>());

    EXPECT_EQ(result, test_data) << "Pipeline output doesn't match input";
}

// Test StreamPipeline with codec serialization
TEST_F(CodecTest, StreamPipelineWithCodec) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.iv_ = generate_test_iv();
    input_frame.source_id = 1;

    const std::string test_data = "Test data for pipeline";
    input_frame.payload_size = test_data.length();

    // Create producer for original data
    auto data_producer = [&test_data](std::stringstream& output) -> bool {
        output.write(test_data.c_str(), test_data.length());
        return output.good();
    };

    // Set payload stream to pipeline
    input_frame.payload_stream = std::make_shared<StreamPipeline>(data_producer);

    // Create serialization producer
    auto serialize_producer = [this, &input_frame](std::stringstream& output) -> bool {
        try {
            codec.serialize(input_frame, output);
            return output.good();
        } catch (const std::exception&) {
            return false;
        }
    };

    // Create serialization pipeline
    StreamPipeline serialized_stream(serialize_producer);

    // Deserialize the data
    MessageFrame output_frame = codec.deserialize(serialized_stream);

    // Verify the frame
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.iv_, input_frame.iv_);

    // Read and compare payload
    std::string output_data((std::istreambuf_iterator<char>(*output_frame.payload_stream)),
                           std::istreambuf_iterator<char>());
    EXPECT_EQ(output_data, test_data) << "Pipeline payload doesn't match input";
}

TEST_F(CodecTest, MinimalFrameSerializeDeserialize) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.payload_size = 0;
    input_frame.iv_ = generate_test_iv();
    input_frame.source_id = 1;  // Set a numeric source ID

    std::stringstream output_stream;
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state before serialization";

    std::size_t written = 0;
    ASSERT_NO_THROW({
        written = codec.serialize(input_frame, output_stream);
    }) << "Serialization threw an exception";
    ASSERT_GT(written, 0) << "No data was written during serialization";
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state after serialization";

    std::string serialized_data = output_stream.str();
    ASSERT_FALSE(serialized_data.empty()) << "Serialized data is empty";
    ASSERT_GE(serialized_data.length(), written) << "Serialized data length mismatch";

    output_stream.seekg(0);
    ASSERT_TRUE(output_stream.good()) << "Failed to reset output stream position";

    MessageFrame deserialized_frame;
    ASSERT_NO_THROW({
        deserialized_frame = codec.deserialize(output_stream);
    }) << "Deserialization threw an exception";

    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume frame from channel";

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.iv_, input_frame.iv_);

    EXPECT_TRUE(channel.empty());
}

TEST_F(CodecTest, BasicSerializeDeserialize) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.iv_ = generate_test_iv();
    input_frame.source_id = 2;  // Set a numeric source ID

    const std::string test_data = "TestData123";
    input_frame.payload_size = test_data.length();
    input_frame.filename_length = 8;

    auto payload = std::make_shared<std::stringstream>();
    ASSERT_TRUE(payload->good()) << "Initial payload stream state is not good";
    payload->write(test_data.c_str(), test_data.length());
    ASSERT_TRUE(payload->good()) << "Failed to write test data to payload stream";
    payload->seekg(0);
    payload->seekp(0);
    input_frame.payload_stream = payload;

    std::stringstream output_stream;
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state before serialization";

    std::size_t written = 0;
    ASSERT_NO_THROW({
        written = codec.serialize(input_frame, output_stream);
    }) << "Serialization threw an exception";
    ASSERT_GT(written, 0) << "No data was written during serialization";

    output_stream.seekg(0);
    ASSERT_TRUE(output_stream.good()) << "Failed to reset output stream position";

    MessageFrame deserialized_frame;
    ASSERT_NO_THROW({
        deserialized_frame = codec.deserialize(output_stream);
    }) << "Deserialization threw an exception";

    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume frame from channel";

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.iv_, input_frame.iv_);

    ASSERT_TRUE(output_frame.payload_stream != nullptr);
    ASSERT_TRUE(input_frame.payload_stream != nullptr);

    input_frame.payload_stream->seekg(0);
    output_frame.payload_stream->seekg(0);

    std::string input_data((std::istreambuf_iterator<char>(*input_frame.payload_stream)),
                           std::istreambuf_iterator<char>());
    std::string output_data((std::istreambuf_iterator<char>(*output_frame.payload_stream)),
                            std::istreambuf_iterator<char>());

    EXPECT_EQ(output_data.length(), input_data.length()) << "Decrypted payload size mismatch";
    EXPECT_EQ(output_data, input_data) << "Decrypted payload data mismatch";
    EXPECT_EQ(output_data, test_data) << "Decrypted payload doesn't match original test data";

    EXPECT_TRUE(channel.empty());
}

TEST_F(CodecTest, LargePayloadChunkedProcessing) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.iv_ = generate_test_iv();
    input_frame.source_id = 3;  // Set a numeric source ID

    const size_t payload_size = 10 * 1024 * 1024;  // 10MB
    std::string large_data = generate_random_data(payload_size);
    input_frame.payload_size = payload_size;
    input_frame.filename_length = 12;

    auto payload = std::make_shared<std::stringstream>();
    payload->write(large_data.c_str(), large_data.length());
    payload->seekg(0);
    payload->seekp(0);
    input_frame.payload_stream = payload;

    std::stringstream output_stream;

    std::size_t written = 0;
    ASSERT_NO_THROW({
        written = codec.serialize(input_frame, output_stream);
    }) << "Serialization of large payload threw an exception";

    output_stream.seekg(0);

    MessageFrame deserialized_frame;
    ASSERT_NO_THROW({
        deserialized_frame = codec.deserialize(output_stream);
    }) << "Deserialization of large payload threw an exception";

    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume large frame from channel";

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.iv_, input_frame.iv_);

    output_frame.payload_stream->seekg(0);
    input_frame.payload_stream->seekg(0);

    std::string output_data((std::istreambuf_iterator<char>(*output_frame.payload_stream)),
                            std::istreambuf_iterator<char>());
    std::string input_data((std::istreambuf_iterator<char>(*input_frame.payload_stream)),
                           std::istreambuf_iterator<char>());

    EXPECT_EQ(output_data.length(), input_data.length()) << "Decrypted payload size mismatch";
    EXPECT_EQ(output_data, input_data) << "Decrypted large payload data mismatch";

    EXPECT_TRUE(channel.empty());
}

TEST_F(CodecTest, MalformedInputHandling) {
    std::stringstream malformed_stream;

    // Test 1: Empty stream
    EXPECT_THROW(codec.deserialize(malformed_stream), std::runtime_error);

    // Test 2: Incomplete header (missing IV)
    malformed_stream.write("\x01\x02\x03", 3);
    malformed_stream.seekg(0);
    EXPECT_THROW(codec.deserialize(malformed_stream), std::runtime_error);

    // Test 3: Invalid IV size
    MessageFrame frame;
    frame.message_type = MessageType::STORE_FILE;
    frame.payload_size = 1000;
    frame.filename_length = 8;
    frame.source_id = 4;  // Set a numeric source ID
    frame.iv_ = std::vector<uint8_t>(dfs::crypto::CryptoStream::IV_SIZE - 1, 0x42); // Invalid IV size

    std::stringstream partial_stream;
    EXPECT_THROW(codec.serialize(frame, partial_stream), std::runtime_error);
}

TEST_F(CodecTest, EmptySourceId) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.payload_size = 0;
    input_frame.filename_length = 0;
    input_frame.iv_ = generate_test_iv();
    input_frame.source_id = 0;  // Set source ID to 0

    std::stringstream output_stream;

    std::size_t written = codec.serialize(input_frame, output_stream);
    ASSERT_GT(written, 0) << "No data written for frame with empty source_id";

    output_stream.seekg(0);
    MessageFrame deserialized_frame = codec.deserialize(output_stream);

    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame));

    EXPECT_EQ(output_frame.source_id, 0u);  // Using unsigned comparison
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.payload_size, 0u);  // Using unsigned comparison
    EXPECT_EQ(output_frame.filename_length, 0u);  // Using unsigned comparison
    EXPECT_EQ(output_frame.iv_, input_frame.iv_);
}