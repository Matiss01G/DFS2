#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "network/bootstrap.hpp"
#include "network/peer_manager.hpp"  // Add explicit include for PeerManager

using namespace dfs::network;

class BootstrapTest : public ::testing::Test {
protected:
    const std::string ADDRESS = "127.0.0.1";
    const std::vector<uint8_t> TEST_KEY = std::vector<uint8_t>(32, 0x42); // 32-byte test key
};

TEST_F(BootstrapTest, PeerConnection) {
    const uint8_t PEER1_ID = 1, PEER2_ID = 2;
    const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;

    std::vector<std::string> peer1_bootstrap_nodes = {};
    std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    std::thread peer1_thread([&peer1]() {
        ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::thread peer2_thread([&peer2]() {
        ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID));
    ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID));
    EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID));
    EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID));

    peer1_thread.join();
    peer2_thread.join();
}