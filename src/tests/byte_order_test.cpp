#include <gtest/gtest.h>
#include "crypto/byte_order.hpp"
#include "network/message_handler.hpp"
#include <sstream>
#include <vector>

using namespace dfs::crypto;
using namespace dfs::network;

TEST(ByteOrderTest, NetworkOrderConversion) {
    uint32_t original = 0x12345678;
    uint32_t network = ByteOrder::toNetworkOrder(original);
    uint32_t host = ByteOrder::fromNetworkOrder(network);

    EXPECT_EQ(host, original);
}

TEST(ByteOrderTest, EndianCheck) {
    bool isLittle = ByteOrder::isLittleEndian();
    // We can't test the actual value as it's platform-dependent,
    // but we can ensure the function returns consistently
    EXPECT_EQ(isLittle, ByteOrder::isLittleEndian());
}

TEST(ByteOrderTest, DifferentTypes) {
    uint16_t u16 = 0x1234;
    uint32_t u32 = 0x12345678;
    uint64_t u64 = 0x1234567890ABCDEF;

    EXPECT_EQ(ByteOrder::fromNetworkOrder(ByteOrder::toNetworkOrder(u16)), u16);
    EXPECT_EQ(ByteOrder::fromNetworkOrder(ByteOrder::toNetworkOrder(u32)), u32);
    EXPECT_EQ(ByteOrder::fromNetworkOrder(ByteOrder::toNetworkOrder(u64)), u64);
}

// New tests for MessageFrame network byte order handling
TEST(ByteOrderTest, MessageFrameSerialization) {
    MessageHandler handler;
    std::stringstream input("Test message");
    std::stringstream serialized;
    std::stringstream deserialized;

    // Test with different source_id values
    std::vector<uint32_t> test_source_ids = {
        0x12345678,  // Mixed endian test value
        0x00000001,  // Small value
        0xFFFFFFFF   // Max value
    };

    for (uint32_t source_id : test_source_ids) {
        serialized.str("");
        deserialized.str("");
        input.clear();
        input.seekg(0);

        // Set source_id and verify serialization/deserialization
        handler.set_source_id(source_id);
        ASSERT_TRUE(handler.serialize(input, serialized)) 
            << "Failed to serialize with source_id: " << std::hex << source_id;
        ASSERT_TRUE(handler.deserialize(serialized, deserialized))
            << "Failed to deserialize with source_id: " << std::hex << source_id;

        // Verify the deserialized source_id matches original
        EXPECT_EQ(handler.get_source_id(), source_id)
            << "Source ID mismatch for value: " << std::hex << source_id;
    }
}

TEST(ByteOrderTest, MessageFramePayloadSize) {
    MessageHandler handler;
    std::stringstream serialized;
    std::stringstream deserialized;

    // Test with different payload sizes
    std::vector<uint32_t> test_sizes = {
        0,           // Empty payload
        1024,        // Small payload
        0xFFFFFFFF   // Max payload size
    };

    for (uint32_t size : test_sizes) {
        // Create input stream with specified size
        std::string test_data(size, 'A');
        std::stringstream input(test_data);

        serialized.str("");
        deserialized.str("");

        ASSERT_TRUE(handler.serialize(input, serialized))
            << "Failed to serialize with payload size: " << size;
        ASSERT_TRUE(handler.deserialize(serialized, deserialized))
            << "Failed to deserialize with payload size: " << size;

        // Verify the deserialized content length matches original
        std::string result = deserialized.str();
        EXPECT_EQ(result.length(), test_data.length())
            << "Payload size mismatch for size: " << size;
        EXPECT_EQ(result, test_data)
            << "Payload content mismatch for size: " << size;
    }
}

TEST(ByteOrderTest, MessageTypePreservation) {
    MessageHandler handler;
    std::stringstream input("Test");
    std::stringstream serialized;
    std::stringstream deserialized;

    // Test with different message types
    std::vector<uint8_t> test_types = {
        0x00, 0x01, 0xFF  // Test different type values
    };

    for (uint8_t type : test_types) {
        serialized.str("");
        deserialized.str("");
        input.clear();
        input.seekg(0);

        // Set message type and verify preservation
        handler.set_message_type(type);
        ASSERT_TRUE(handler.serialize(input, serialized))
            << "Failed to serialize with type: " << std::hex << (int)type;
        ASSERT_TRUE(handler.deserialize(serialized, deserialized))
            << "Failed to deserialize with type: " << std::hex << (int)type;

        // Verify the type wasn't affected by byte order conversion
        EXPECT_EQ(handler.get_message_type(), type)
            << "Message type changed after serialization/deserialization";
    }
}