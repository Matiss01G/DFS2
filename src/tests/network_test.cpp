#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_peer.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
