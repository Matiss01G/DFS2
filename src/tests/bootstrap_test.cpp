#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "network/bootstrap.hpp"
#include "network/peer_manager.hpp"
#include <sstream>

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

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::thread peer2_thread([&peer2]() {
        ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID));
    ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID));
    EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID));
    EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID));

    peer1_thread.join();
    peer2_thread.join();
}

// New test case to verify handling of duplicate peer connections
TEST_F(BootstrapTest, DuplicatePeerConnection) {
    const uint8_t PEER1_ID = 1, PEER2_ID = 2;
    const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;

    std::vector<std::string> peer1_bootstrap_nodes = {};
    std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    // Start the first peer
    std::thread peer1_thread([&peer1]() {
        ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Start the second peer
    std::thread peer2_thread([&peer2]() {
        ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Verify initial connection was successful
    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Initial connection failed - Peer 1 doesn't see Peer 2";
    ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Initial connection failed - Peer 2 doesn't see Peer 1";

    // Attempt duplicate connection
    EXPECT_FALSE(peer2.connect_to_bootstrap_nodes())
        << "Duplicate connection attempt should return false";

    // Verify original connections are still intact
    EXPECT_TRUE(peer1_manager.has_peer(PEER2_ID))
        << "Original connection lost after duplicate attempt - Peer 1 lost Peer 2";
    EXPECT_TRUE(peer2_manager.has_peer(PEER1_ID))
        << "Original connection lost after duplicate attempt - Peer 2 lost Peer 1";
    EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID))
        << "Original connection no longer active - Peer 1 to Peer 2";
    EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID))
        << "Original connection no longer active - Peer 2 to Peer 1";

    peer1_thread.join();
    peer2_thread.join();
}

/**
 * Test: FileSharing
 * Purpose: Verifies that files stored on one peer are properly shared with connected peers.
 * 
 * Methodology:
 * 1. Creates and connects two peers
 * 2. Stores a test file on peer1
 * 3. Verifies the file exists locally on peer1
 * 4. Verifies the file is shared and exists on peer2
 * 
 * Expected Results:
 * - Peers should connect successfully
 * - File should be stored successfully on peer1
 * - File should be automatically shared and available on peer2
 * - File contents should match on both peers
 */
TEST_F(BootstrapTest, FileSharing) {
    const uint8_t PEER1_ID = 1, PEER2_ID = 2;
    const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;
    const std::string TEST_FILE_KEY = "test_shared_file";
    const std::string TEST_FILE_CONTENT = "Hello, distributed file system!";

    std::vector<std::string> peer1_bootstrap_nodes = {};
    std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

    // Create and start peers
    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    // Start peer1 (bootstrap node)
    std::thread peer1_thread([&peer1]() {
        ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Start peer2 and connect to peer1
    std::thread peer2_thread([&peer2]() {
        ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Verify peers are connected
    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();
    ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
    ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer2 should be connected to Peer1";

    // Create and store test file on peer1
    std::stringstream test_data(TEST_FILE_CONTENT);
    auto& file_server1 = peer1.get_file_server();
    ASSERT_NO_THROW({
        file_server1.store_file(TEST_FILE_KEY, test_data);
    }) << "Failed to store file on peer1";

    // Wait for file sharing to complete
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Verify file exists on peer1
    ASSERT_TRUE(file_server1.has_file(TEST_FILE_KEY))
        << "File should exist locally on peer1";

    // Verify file exists and matches on peer2
    auto& file_server2 = peer2.get_file_server();
    ASSERT_TRUE(file_server2.has_file(TEST_FILE_KEY))
        << "File should be shared with peer2";

    // Verify file contents on peer2
    std::stringstream retrieved_data;
    ASSERT_NO_THROW({
        file_server2.get_file(TEST_FILE_KEY, retrieved_data);
    }) << "Failed to retrieve file from peer2";
    ASSERT_EQ(retrieved_data.str(), TEST_FILE_CONTENT)
        << "File content should match on peer2";

    peer1_thread.join();
    peer2_thread.join();
}