#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include "network/bootstrap.hpp"
#include "network/peer_manager.hpp"
#include <boost/log/trivial.hpp>

using namespace dfs::network;

class BootstrapTest : public ::testing::Test {
protected:
    const std::string ADDRESS = "127.0.0.1";
    const std::vector<uint8_t> TEST_KEY = std::vector<uint8_t>(32, 0x42); // 32-byte test key
    std::atomic<bool> connection_verified{false};
    std::atomic<bool> verification_complete{false};
};

TEST_F(BootstrapTest, PeerConnection) {
    const uint8_t PEER1_ID = 1, PEER2_ID = 2;
    const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;

    std::vector<std::string> peer1_bootstrap_nodes = {};
    std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

    // Create bootstrap instances
    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    // Start peer1 as a background task
    std::thread peer1_thread([&]() {
        peer1.start();
    });

    // Wait for peer1 to start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Start peer2 as a background task
    std::thread peer2_thread([&]() {
        peer2.start();
    });

    // Wait for peer2 to start and connect
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Get peer managers for verification
    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    // Verify connections in a separate thread to avoid blocking
    std::thread verification_thread([&]() {
        try {
            BOOST_LOG_TRIVIAL(info) << "Starting connection verification";

            // Wait for connections to be fully established
            std::this_thread::sleep_for(std::chrono::seconds(2));

            BOOST_LOG_TRIVIAL(debug) << "Verifying peer1 has peer2";
            ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer 1 should have Peer 2";

            BOOST_LOG_TRIVIAL(debug) << "Verifying peer2 has peer1";
            ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer 2 should have Peer 1";

            BOOST_LOG_TRIVIAL(debug) << "Verifying peer1 is connected to peer2";
            EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID)) << "Peer 1 should be connected to Peer 2";

            BOOST_LOG_TRIVIAL(debug) << "Verifying peer2 is connected to peer1";
            EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID)) << "Peer 2 should be connected to Peer 1";

            connection_verified = true;
            BOOST_LOG_TRIVIAL(info) << "Connection verification successful";
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Verification failed with error: " << e.what();
        }
        verification_complete = true;
    });

    // Wait for verification to complete with timeout
    const int VERIFICATION_TIMEOUT_SECONDS = 10;
    auto start_time = std::chrono::steady_clock::now();

    while (!verification_complete) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

        if (elapsed_time >= VERIFICATION_TIMEOUT_SECONDS) {
            BOOST_LOG_TRIVIAL(error) << "Verification timed out after " << VERIFICATION_TIMEOUT_SECONDS << " seconds";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Shutdown peers
    BOOST_LOG_TRIVIAL(info) << "Initiating peer shutdown";
    peer1.shutdown();
    peer2.shutdown();

    // Join all threads
    if (verification_thread.joinable()) {
        BOOST_LOG_TRIVIAL(debug) << "Joining verification thread";
        verification_thread.join();
    }

    if (peer1_thread.joinable()) {
        BOOST_LOG_TRIVIAL(debug) << "Joining peer1 thread";
        peer1_thread.join();
    }

    if (peer2_thread.joinable()) {
        BOOST_LOG_TRIVIAL(debug) << "Joining peer2 thread";
        peer2_thread.join();
    }

    BOOST_LOG_TRIVIAL(info) << "Test cleanup complete";
    ASSERT_TRUE(connection_verified) << "Connection verification failed or timed out";
}