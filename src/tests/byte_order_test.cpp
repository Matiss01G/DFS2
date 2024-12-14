#include <gtest/gtest.h>
#include "crypto/byte_order.hpp"

using namespace dfs::crypto;

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
