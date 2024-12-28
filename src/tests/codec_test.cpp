#include <gtest/gtest.h>
#include <sstream>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "network/message_frame.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    Codec codec;
    Channel channel;

    void SetUp() override {
        // Initialize Boost logging
        boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
        boost::log::add_console_log(
            std::cout,
            boost::log::keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
        );
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::debug
        );
        boost::log::add_common_attributes();

        BOOST_LOG_TRIVIAL(info) << "Setting up CodecTest fixture";
    }

    void TearDown() override {
        BOOST_LOG_TRIVIAL(info) << "Tearing down CodecTest fixture";
    }
};

TEST_F(CodecTest, BasicSerializeDeserialize) {
    // Create a message frame with all fields populated
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 12345;
    input_frame.filename_length = 8;  // Set a valid filename length

    // Create payload with known content
    const std::string test_data = "TestData123";
    input_frame.payload_size = test_data.length();

    // Create and prepare payload stream
    input_frame.payload_stream = std::make_shared<std::stringstream>();
    input_frame.payload_stream->write(test_data.c_str(), test_data.length());
    input_frame.payload_stream->seekg(0);

    // Prepare output stream for serialization
    std::stringstream output_stream;

    // Serialize the frame
    ASSERT_NO_THROW({
        std::size_t written = codec.serialize(input_frame, output_stream);
        ASSERT_GT(written, 0) << "No data was written during serialization";
    });

    // Reset stream position for reading
    output_stream.seekg(0);

    // Deserialize the frame
    MessageFrame output_frame;
    ASSERT_NO_THROW({
        output_frame = codec.deserialize(output_stream, channel);
    });

    // Verify all frame fields match
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);

    // Read and compare payload data
    std::string output_data;
    if (output_frame.payload_stream) {
        output_frame.payload_stream->seekg(0);
        std::stringstream ss;
        ss << output_frame.payload_stream->rdbuf();
        output_data = ss.str();
    }

    EXPECT_EQ(output_data, test_data) << "Payload content mismatch";
}

TEST_F(CodecTest, MinimalFrameSerializeDeserialize) {
    // Create a message frame with only required fields
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 54321;
    input_frame.payload_size = 0;
    input_frame.filename_length = 0;  // Set explicitly to 0

    // Serialize the frame
    std::stringstream output_stream;
    ASSERT_NO_THROW({
        std::size_t written = codec.serialize(input_frame, output_stream);
        ASSERT_GT(written, 0) << "No data was written during serialization";
    });

    // Reset stream position for reading
    output_stream.seekg(0);

    // Deserialize the frame
    MessageFrame output_frame = codec.deserialize(output_stream, channel);

    // Verify set fields match
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
}