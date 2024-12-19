#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "network/peer_manager.hpp"
#include "network/tcp_peer.hpp"

namespace dfs {
namespace network {
namespace test {

class PeerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<PeerManager>();
    }

    void TearDown() override {
        manager->shutdown();
        manager.reset();
    }

    std::unique_ptr<PeerManager> manager;
};

TEST_F(PeerManagerTest, AddPeer) {
    auto peer = std::make_shared<TCP_Peer>("test_peer");
    manager->add_peer(peer);
    
    auto retrieved_peer = manager->get_peer("test_peer");
    ASSERT_NE(retrieved_peer, nullptr);
}

TEST_F(PeerManagerTest, AddDuplicatePeer) {
    auto peer1 = std::make_shared<TCP_Peer>("test_peer");
    auto peer2 = std::make_shared<TCP_Peer>("test_peer");
    
    manager->add_peer(peer1);
    manager->add_peer(peer2);  // Should be ignored
    
    auto retrieved_peer = manager->get_peer("test_peer");
    ASSERT_NE(retrieved_peer, nullptr);
    EXPECT_EQ(retrieved_peer.get(), peer1.get());
}

TEST_F(PeerManagerTest, RemovePeer) {
    auto peer = std::make_shared<TCP_Peer>("test_peer");
    manager->add_peer(peer);
    manager->remove_peer("test_peer");
    
    auto retrieved_peer = manager->get_peer("test_peer");
    EXPECT_EQ(retrieved_peer, nullptr);
}

TEST_F(PeerManagerTest, RemoveNonExistentPeer) {
    manager->remove_peer("non_existent_peer");  // Should not crash
    auto retrieved_peer = manager->get_peer("non_existent_peer");
    EXPECT_EQ(retrieved_peer, nullptr);
}

TEST_F(PeerManagerTest, ThreadSafety) {
    const int num_threads = 10;
    const int peers_per_thread = 100;
    std::vector<std::thread> threads;
    
    // Add peers from multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, peers_per_thread]() {
            for (int j = 0; j < peers_per_thread; ++j) {
                std::string peer_id = "peer_" + std::to_string(i) + "_" + std::to_string(j);
                auto peer = std::make_shared<TCP_Peer>(peer_id);
                manager->add_peer(peer);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all peers were added correctly
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < peers_per_thread; ++j) {
            std::string peer_id = "peer_" + std::to_string(i) + "_" + std::to_string(j);
            auto peer = manager->get_peer(peer_id);
            EXPECT_NE(peer, nullptr);
        }
    }
}

TEST_F(PeerManagerTest, ConcurrentAddRemove) {
    const int num_operations = 1000;
    std::thread add_thread([this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            auto peer = std::make_shared<TCP_Peer>("peer_" + std::to_string(i));
            manager->add_peer(peer);
        }
    });
    
    std::thread remove_thread([this, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            manager->remove_peer("peer_" + std::to_string(i));
        }
    });
    
    add_thread.join();
    remove_thread.join();
}

TEST_F(PeerManagerTest, ShutdownBehavior) {
    // Add some peers
    for (int i = 0; i < 5; ++i) {
        auto peer = std::make_shared<TCP_Peer>("peer_" + std::to_string(i));
        manager->add_peer(peer);
    }
    
    // Shutdown should disconnect all peers and clear the map
    manager->shutdown();
    
    // Verify all peers are removed
    for (int i = 0; i < 5; ++i) {
        auto peer = manager->get_peer("peer_" + std::to_string(i));
        EXPECT_EQ(peer, nullptr);
    }
}

TEST_F(PeerManagerTest, NullPeerHandling) {
    std::shared_ptr<TCP_Peer> null_peer;
    manager->add_peer(null_peer);  // Should be handled gracefully
    
    auto peer = std::make_shared<TCP_Peer>("valid_peer");
    manager->add_peer(peer);
    
    auto retrieved_peer = manager->get_peer("valid_peer");
    EXPECT_NE(retrieved_peer, nullptr);
}

} // namespace test
} // namespace network
} // namespace dfs
