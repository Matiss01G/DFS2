#include <gtest/gtest.h>
#include "network/connection_state.hpp"

using namespace dfs::network;

// Basic test to ensure the test framework is working
TEST(NetworkTest, BasicTest) {
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
