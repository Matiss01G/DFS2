#include <gtest/gtest.h>
#include "network/codec.hpp"
#include <sstream>
#include <vector>
#include <random>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    Codec codec;
    void SetUp() override {
        // Configure logging to reduce verbosity during tests
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::warning
        );
    }
};

// Test basic serialization and deserialization with header verification
TEST_F(CodecTest, BasicSerializationTest) {
    // Create a test message frame
    MessageFrame original;
    original.message_type = MessageType::STORE_FILE;
    original.source_id = 12345;
    original.payload_size = 13;  // "Hello, World!" length
    original.filename_length = 0;

    // Create payload stream
    original.payload_stream = std::make_shared<std::stringstream>();
    *original.payload_stream << "Hello, World!";

    // Serialize
    std::stringstream serialized;
    size_t written = codec.serialize(original, serialized);
    ASSERT_GT(written, 0);

    // Create a channel for deserialization
    Channel channel;

    // Deserialize
    MessageFrame deserialized = codec.deserialize(serialized, channel);

    // Verify headers
    EXPECT_EQ(deserialized.message_type, original.message_type);
    EXPECT_EQ(deserialized.source_id, original.source_id);
    EXPECT_EQ(deserialized.payload_size, original.payload_size);
    EXPECT_EQ(deserialized.filename_length, original.filename_length);

    // Verify payload
    std::string original_payload, deserialized_payload;
    original.payload_stream->seekg(0);
    std::getline(*original.payload_stream, original_payload);

    deserialized.payload_stream->seekg(0);
    std::getline(*deserialized.payload_stream, deserialized_payload);

    EXPECT_EQ(original_payload, deserialized_payload);
}

// Test serialization of large data that requires multiple chunks
TEST_F(CodecTest, LargeDataSerializationTest) {
    const size_t large_size = 1024 * 1024; // 1MB

    // Create large test data
    std::vector<char> test_data(large_size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(32, 126); // Printable ASCII

    for (size_t i = 0; i < large_size; ++i) {
        test_data[i] = static_cast<char>(dis(gen));
    }

    // Create message frame with large payload
    MessageFrame original;
    original.message_type = MessageType::STORE_FILE;
    original.source_id = 98765;
    original.payload_size = large_size;
    original.filename_length = 0;

    original.payload_stream = std::make_shared<std::stringstream>();
    original.payload_stream->write(test_data.data(), large_size);

    // Serialize
    std::stringstream serialized;
    size_t written = codec.serialize(original, serialized);
    ASSERT_EQ(written, large_size + sizeof(uint8_t) + sizeof(uint32_t) + 
                      sizeof(uint64_t) + sizeof(uint32_t));

    // Create channel for deserialization
    Channel channel;

    // Deserialize
    MessageFrame deserialized = codec.deserialize(serialized, channel);

    // Verify headers
    EXPECT_EQ(deserialized.message_type, original.message_type);
    EXPECT_EQ(deserialized.source_id, original.source_id);
    EXPECT_EQ(deserialized.payload_size, original.payload_size);

    // Verify large payload
    std::vector<char> deserialized_data(large_size);
    deserialized.payload_stream->seekg(0);
    deserialized.payload_stream->read(deserialized_data.data(), large_size);

    EXPECT_EQ(deserialized.payload_stream->gcount(), large_size);
    EXPECT_EQ(std::memcmp(test_data.data(), deserialized_data.data(), large_size), 0);
}

// Test serialization with channel integration
TEST_F(CodecTest, ChannelIntegrationTest) {
    // Create test messages
    const int num_messages = 5;
    std::vector<MessageFrame> original_frames;
    Channel channel;

    for (int i = 0; i < num_messages; ++i) {
        MessageFrame frame;
        frame.message_type = MessageType::STORE_FILE;
        frame.source_id = 1000 + i;
        frame.payload_size = 10;
        frame.filename_length = 0;

        frame.payload_stream = std::make_shared<std::stringstream>();
        *frame.payload_stream << "Message " << i;
        frame.payload_stream->seekg(0);

        original_frames.push_back(frame);
    }

    // Serialize and deserialize through channel
    std::vector<std::stringstream> serialized_streams(num_messages);
    for (int i = 0; i < num_messages; ++i) {
        codec.serialize(original_frames[i], serialized_streams[i]);
        codec.deserialize(serialized_streams[i], channel);
    }

    // Verify frames from channel
    MessageFrame frame;
    for (int i = 0; i < num_messages; ++i) {
        ASSERT_TRUE(channel.consume(frame));

        // Verify headers
        EXPECT_EQ(frame.message_type, original_frames[i].message_type);
        EXPECT_EQ(frame.source_id, original_frames[i].source_id);
        EXPECT_EQ(frame.payload_size, original_frames[i].payload_size);

        // Verify payload
        std::string expected_payload = "Message " + std::to_string(i);
        std::string actual_payload;
        frame.payload_stream->seekg(0);
        std::getline(*frame.payload_stream, actual_payload);

        EXPECT_EQ(actual_payload, expected_payload);
    }

    // Verify channel is empty
    EXPECT_TRUE(channel.empty());
}