#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_peer.hpp"
#include "network/peer_manager.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>

using namespace dfs::network;
using ::testing::Return;
using ::testing::_;

class PeerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging with reduced verbosity
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::error
        );
        manager = std::make_unique<PeerManager>();
    }

    void TearDown() override {
        if (manager) {
            manager->shutdown();
        }
    }

    std::unique_ptr<PeerManager> manager;
};

// Test initialization
TEST_F(PeerManagerTest, InitializationTest) {
    ASSERT_NE(manager, nullptr);
}

// Test adding valid peer
TEST_F(PeerManagerTest, AddValidPeerTest) {
    auto peer = std::make_shared<TCP_Peer>("test_peer_1");
    manager->add_peer(peer);
    auto retrieved_peer = manager->get_peer("test_peer_1");
    ASSERT_NE(retrieved_peer, nullptr);
    EXPECT_EQ(retrieved_peer->get_peer_id(), "test_peer_1");
}

// Test adding null peer
TEST_F(PeerManagerTest, AddNullPeerTest) {
    std::shared_ptr<TCP_Peer> null_peer = nullptr;
    manager->add_peer(null_peer);
    // Should not crash and null peer should not be added
    auto retrieved_peer = manager->get_peer("");
    ASSERT_EQ(retrieved_peer, nullptr);
}

// Test adding duplicate peer
TEST_F(PeerManagerTest, AddDuplicatePeerTest) {
    auto peer1 = std::make_shared<TCP_Peer>("duplicate_id");
    auto peer2 = std::make_shared<TCP_Peer>("duplicate_id");

    manager->add_peer(peer1);
    manager->add_peer(peer2); 

    auto retrieved_peer = manager->get_peer("duplicate_id");
    ASSERT_NE(retrieved_peer, nullptr);
    EXPECT_EQ(retrieved_peer, peer1); 
}

// Test removing existing peer
TEST_F(PeerManagerTest, RemoveExistingPeerTest) {
    auto peer = std::make_shared<TCP_Peer>("remove_test");
    manager->add_peer(peer);

    ASSERT_NE(manager->get_peer("remove_test"), nullptr);
    manager->remove_peer("remove_test");
    EXPECT_EQ(manager->get_peer("remove_test"), nullptr);
}

// Test removing non-existent peer
TEST_F(PeerManagerTest, RemoveNonExistentPeerTest) {
    manager->remove_peer("non_existent"); 
    EXPECT_EQ(manager->get_peer("non_existent"), nullptr);
}

// Test getting non-existent peer
TEST_F(PeerManagerTest, GetNonExistentPeerTest) {
    EXPECT_EQ(manager->get_peer("non_existent"), nullptr);
}

// Test thread safety with multiple threads adding and removing peers
TEST_F(PeerManagerTest, ThreadSafetyTest) {
    const int num_threads = 3;  
    const int operations_per_thread = 5;  
    std::vector<std::thread> threads;

    // Create threads that add and remove peers
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string peer_id = "peer_" + std::to_string(i) + "_" + std::to_string(j);
                auto peer = std::make_shared<TCP_Peer>(peer_id);

                // Add peer
                manager->add_peer(peer);

                // Small delay to increase chance of thread interleaving
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                // Verify peer exists
                auto retrieved_peer = manager->get_peer(peer_id);
                EXPECT_NE(retrieved_peer, nullptr);

                // Remove peer
                manager->remove_peer(peer_id);

                // Verify peer was removed
                retrieved_peer = manager->get_peer(peer_id);
                EXPECT_EQ(retrieved_peer, nullptr);
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
}

// Test shutdown functionality
TEST_F(PeerManagerTest, ShutdownTest) {
    // Add several peers
    std::vector<std::string> peer_ids = {"peer1", "peer2"};  
    for (const auto& id : peer_ids) {
        manager->add_peer(std::make_shared<TCP_Peer>(id));
    }

    // Verify peers are added
    for (const auto& id : peer_ids) {
        EXPECT_NE(manager->get_peer(id), nullptr);
    }

    // Shutdown manager
    manager->shutdown();

    // Verify all peers are removed
    for (const auto& id : peer_ids) {
        EXPECT_EQ(manager->get_peer(id), nullptr);
    }
}

// Test broadcasting to multiple peers
TEST_F(PeerManagerTest, BroadcastStreamTest) {
    // Create multiple peers
    std::vector<std::shared_ptr<TCP_Peer>> test_peers;
    const int num_peers = 3;

    for (int i = 0; i < num_peers; ++i) {
        auto peer = std::make_shared<TCP_Peer>("test_peer_" + std::to_string(i));
        test_peers.push_back(peer);
        manager->add_peer(peer);
    }

    // Prepare test data
    std::string test_message = "Test broadcast message\n";
    std::istringstream input_stream(test_message);

    // Test broadcasting when peers are not connected
    EXPECT_FALSE(manager->broadcast_stream(input_stream));

    // Test broadcasting with invalid stream
    std::istringstream empty_stream("");
    empty_stream.setstate(std::ios::failbit);
    EXPECT_FALSE(manager->broadcast_stream(empty_stream));

    // Clean up
    manager->shutdown();
}

// Test broadcasting with mixed success/failure scenarios
TEST_F(PeerManagerTest, BroadcastMixedResultsTest) {
    auto peer1 = std::make_shared<TCP_Peer>("peer1");
    auto peer2 = std::make_shared<TCP_Peer>("peer2");

    manager->add_peer(peer1);
    manager->add_peer(peer2);

    std::string test_message = "Test broadcast message\n";
    std::istringstream input_stream(test_message);

    // Test broadcasting to disconnected peers
    EXPECT_FALSE(manager->broadcast_stream(input_stream));

    // Clean up
    manager->shutdown();
}

// Test send_to_peer functionality
TEST_F(PeerManagerTest, SendToPeerTest) {
    // Create test peer
    auto peer = std::make_shared<TCP_Peer>(std::to_string(1));
    manager->add_peer(peer);

    // Test sending to non-existent peer
    std::istringstream non_existent_stream("Test message\n");
    EXPECT_FALSE(manager->send_to_peer(999, non_existent_stream));

    // Test sending with invalid stream
    std::istringstream invalid_stream("");
    invalid_stream.setstate(std::ios::failbit);
    EXPECT_FALSE(manager->send_to_peer(1, invalid_stream));

    // Test sending to existing but disconnected peer
    std::istringstream disconnected_stream("Test message\n");
    EXPECT_FALSE(manager->send_to_peer(1, disconnected_stream));
}

// Test send_stream functionality with string-based peer ID
TEST_F(PeerManagerTest, SendStreamTest) {
    // Create test peer
    auto peer = std::make_shared<TCP_Peer>("test_peer_1");
    manager->add_peer(peer);

    // Test sending to non-existent peer
    std::istringstream non_existent_stream("Test message\n");
    EXPECT_FALSE(manager->send_stream("non_existent_peer", non_existent_stream));

    // Test sending with invalid stream
    std::istringstream invalid_stream("");
    invalid_stream.setstate(std::ios::failbit);
    EXPECT_FALSE(manager->send_stream("test_peer_1", invalid_stream));

    // Test sending to existing but disconnected peer
    std::istringstream disconnected_stream("Test message\n");
    EXPECT_FALSE(manager->send_stream("test_peer_1", disconnected_stream));
}