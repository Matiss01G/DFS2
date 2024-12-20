#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_peer.hpp"
#include "network/peer_manager.hpp"
#include "test_utils.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <thread>
#include <chrono>
#include <vector>

using namespace dfs::network;
using ::testing::Return;
using ::testing::_;

class PeerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_logging(); 
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

// Additional test cases can be added here