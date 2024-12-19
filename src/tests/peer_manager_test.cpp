#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/peer_manager.hpp"
#include "network/tcp_peer.hpp"
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <future>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

using namespace dfs::network;

class PeerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set logging level to INFO for cleaner test output
        boost::log::core::get()->set_filter(
            boost::log::expressions::is_in_range(
                boost::log::trivial::severity,
                boost::log::trivial::info,
                boost::log::trivial::fatal
            )
        );
        manager = std::make_unique<PeerManager>();
    }

    void TearDown() override {
        manager->shutdown();
    }

    // Helper method to create a test peer
    std::shared_ptr<TCP_Peer> create_test_peer(const std::string& id) {
        return std::make_shared<TCP_Peer>(id);
    }

    std::unique_ptr<PeerManager> manager;
};

// Test basic initialization
TEST_F(PeerManagerTest, Initialization) {
    ASSERT_NE(manager, nullptr);
}

// Test adding a valid peer
TEST_F(PeerManagerTest, AddValidPeer) {
    auto peer = create_test_peer("test_peer1");
    manager->add_peer(peer);
    
    auto retrieved_peer = manager->get_peer("test_peer1");
    ASSERT_NE(retrieved_peer, nullptr);
    EXPECT_EQ(retrieved_peer->get_peer_id(), "test_peer1");
}

// Test adding a null peer
TEST_F(PeerManagerTest, AddNullPeer) {
    std::shared_ptr<TCP_Peer> null_peer;
    manager->add_peer(null_peer);
    
    auto retrieved_peer = manager->get_peer("");
    EXPECT_EQ(retrieved_peer, nullptr);
}

// Test adding duplicate peers
TEST_F(PeerManagerTest, AddDuplicatePeer) {
    auto peer1 = create_test_peer("test_peer1");
    auto peer2 = create_test_peer("test_peer1"); // Same ID
    
    manager->add_peer(peer1);
    manager->add_peer(peer2); // Should not override existing peer
    
    auto retrieved_peer = manager->get_peer("test_peer1");
    ASSERT_NE(retrieved_peer, nullptr);
    EXPECT_EQ(retrieved_peer, peer1); // Should still be the first peer
}

// Test removing an existing peer
TEST_F(PeerManagerTest, RemoveExistingPeer) {
    auto peer = create_test_peer("test_peer1");
    manager->add_peer(peer);
    
    manager->remove_peer("test_peer1");
    auto retrieved_peer = manager->get_peer("test_peer1");
    EXPECT_EQ(retrieved_peer, nullptr);
}

// Test removing a non-existent peer
TEST_F(PeerManagerTest, RemoveNonExistentPeer) {
    manager->remove_peer("non_existent_peer");
    auto retrieved_peer = manager->get_peer("non_existent_peer");
    EXPECT_EQ(retrieved_peer, nullptr);
}

// Test shutdown functionality
TEST_F(PeerManagerTest, ShutdownTest) {
    auto peer1 = create_test_peer("test_peer1");
    auto peer2 = create_test_peer("test_peer2");
    
    manager->add_peer(peer1);
    manager->add_peer(peer2);
    
    manager->shutdown();
    
    // After shutdown, all peers should be removed
    EXPECT_EQ(manager->get_peer("test_peer1"), nullptr);
    EXPECT_EQ(manager->get_peer("test_peer2"), nullptr);
}

// Test multiple peer operations
TEST_F(PeerManagerTest, MultiplePeerOperations) {
    auto peer1 = create_test_peer("test_peer1");
    auto peer2 = create_test_peer("test_peer2");
    auto peer3 = create_test_peer("test_peer3");
    
    // Add multiple peers
    manager->add_peer(peer1);
    manager->add_peer(peer2);
    manager->add_peer(peer3);
    
    // Verify all peers are added
    EXPECT_NE(manager->get_peer("test_peer1"), nullptr);
    EXPECT_NE(manager->get_peer("test_peer2"), nullptr);
    EXPECT_NE(manager->get_peer("test_peer3"), nullptr);
    
    // Remove middle peer
    manager->remove_peer("test_peer2");
    
    // Verify correct peer was removed
    EXPECT_NE(manager->get_peer("test_peer1"), nullptr);
    EXPECT_EQ(manager->get_peer("test_peer2"), nullptr);
    EXPECT_NE(manager->get_peer("test_peer3"), nullptr);
}

// Test concurrent operations
TEST_F(PeerManagerTest, ConcurrentOperations) {
    const int num_threads = 2;
    const int peers_per_thread = 2; // Reduced number of peers per thread
    std::vector<std::thread> threads;
    
    // Launch threads to add peers
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, peers_per_thread]() {
            for (int j = 0; j < peers_per_thread; ++j) {
                std::string peer_id = "peer_" + std::to_string(i) + "_" + std::to_string(j);
                auto peer = create_test_peer(peer_id);
                manager->add_peer(peer);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all peers were added successfully
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < peers_per_thread; ++j) {
            std::string peer_id = "peer_" + std::to_string(i) + "_" + std::to_string(j);
            auto peer = manager->get_peer(peer_id);
            EXPECT_NE(peer, nullptr) << "Peer " << peer_id << " not found";
            if (peer) {
                EXPECT_EQ(peer->get_peer_id(), peer_id);
            }
        }
    }
}

// Test concurrent add and remove operations
TEST_F(PeerManagerTest, ConcurrentAddRemove) {
    const int num_operations = 10; // Reduced number of operations
    
    // Create a future for adding peers
    auto add_future = std::async(std::launch::async, [this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            auto peer = create_test_peer("test_peer_" + std::to_string(i));
            manager->add_peer(peer);
            std::this_thread::yield(); // Allow other threads to run
        }
    });
    
    // Create a future for removing peers
    auto remove_future = std::async(std::launch::async, [this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            manager->remove_peer("test_peer_" + std::to_string(i));
            std::this_thread::yield(); // Allow other threads to run
        }
    });
    
    // Wait for both operations to complete
    add_future.wait();
    remove_future.wait();
    
    // Verify final state
    for (int i = 0; i < num_operations; ++i) {
        auto peer = manager->get_peer("test_peer_" + std::to_string(i));
        // Due to race conditions, we can't predict exactly which peers will exist
        // But the manager should remain stable (no crashes or invalid states)
        if (peer) {
            EXPECT_EQ(peer->get_peer_id(), "test_peer_" + std::to_string(i));
        }
    }
}