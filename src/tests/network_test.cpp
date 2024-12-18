#include <gtest/gtest.h>
#include "network/connection_state.hpp"
#include "network/peer.hpp"

// Main function for running all tests
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
