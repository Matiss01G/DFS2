#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "network/bootstrap.hpp"
#include "network/peer_manager.hpp"

using namespace dfs::network;

class BootstrapTest : public ::testing::Test {
protected:
    const std::string ADDRESS = "127.0.0.1";
    const std::vector<uint8_t> TEST_KEY = std::vector<uint8_t>(32, 0x42); // 32-byte test key

    bool wait_for_peer_connection(PeerManager& manager, uint8_t peer_id, int timeout_seconds = 5) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_seconds)) {
            if (manager.has_peer(peer_id) && manager.is_connected(peer_id)) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }
};

TEST_F(BootstrapTest, PeerConnection) {
    const uint8_t PEER1_ID = 1, PEER2_ID = 2;
    const uint16_t PEER1_PORT = 50001, PEER2_PORT = 50002;

    std::vector<std::string> peer1_bootstrap_nodes = {};
    std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":50001"};

    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    std::thread peer1_thread([&peer1]() {
        ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
    });

    // Wait for peer1 to fully start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::thread peer2_thread([&peer2]() {
        ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    });

    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    // Wait for peer connections with timeout
    ASSERT_TRUE(wait_for_peer_connection(peer1_manager, PEER2_ID)) 
        << "Peer 1 failed to connect to Peer 2";
    ASSERT_TRUE(wait_for_peer_connection(peer2_manager, PEER1_ID))
        << "Peer 2 failed to connect to Peer 1";

    // Additional connection status checks
    EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID)) 
        << "Peer 1 is not connected to Peer 2";
    EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID))
        << "Peer 2 is not connected to Peer 1";

    peer1_thread.join();
    peer2_thread.join();
}