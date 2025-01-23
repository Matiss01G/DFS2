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
    // Set up peer 1 (listening on port 8001)
    const uint8_t PEER1_ID = 1;
    const uint16_t PEER1_PORT = 8001;
    std::vector<std::string> peer1_bootstrap_nodes = {
        ADDRESS + ":8002"  // Connect to peer 2
    };

    // Set up peer 2 (listening on port 8002)
    const uint8_t PEER2_ID = 2;
    const uint16_t PEER2_PORT = 8002;
    std::vector<std::string> peer2_bootstrap_nodes = {
        ADDRESS + ":8001"  // Connect to peer 1
    };

    // Create bootstrap instances
    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    // Start both peers
    ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";

    // Allow time for connection establishment
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Get peer managers from both bootstrap instances
    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    // Verify peer 2 is connected to peer 1
    ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) 
        << "Peer 1 does not have Peer 2 (ID: " << static_cast<int>(PEER2_ID) << ") in its peer map";

    // Verify peer 1 is connected to peer 2
    ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID))
        << "Peer 2 does not have Peer 1 (ID: " << static_cast<int>(PEER1_ID) << ") in its peer map";

    // Additional connection state verification
    EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID))
        << "Peer 1's connection to Peer 2 is not active";
    EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID))
        << "Peer 2's connection to Peer 1 is not active";
}