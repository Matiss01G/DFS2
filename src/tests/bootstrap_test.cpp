#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <sstream>
#include "network/bootstrap.hpp"
#include "network/peer_manager.hpp"
#include "file_server/file_server.hpp"

using namespace dfs::network;

class BootstrapTest : public ::testing::Test {
protected:
  const std::string ADDRESS = "127.0.0.1";
  const std::vector<uint8_t> TEST_KEY = std::vector<uint8_t>(32, 0x42);
  const std::string TEST_FILE_CONTENT = "Test file content";
  const std::string TEST_FILENAME = "test.txt";
  const size_t LARGE_FILE_SIZE = 2 * 1024 * 1024;
  const size_t CHUNK_SIZE = 1024;
  
  struct Peer {
    uint8_t id;
    uint16_t port;
    std::vector<std::string> bootstrap_nodes;
    std::unique_ptr<Bootstrap> bootstrap;
    std::thread thread;

    Peer(uint8_t id, uint16_t port, std::vector<std::string> nodes)
      : id(id), port(port), bootstrap_nodes(std::move(nodes)) {}
  };
    
  std::vector<std::unique_ptr<Peer>> peers;
  
  void SetUp() override {
    // Clean up any leftover files and ensure we start fresh
    cleanup_all_files();
  }
  
  void TearDown() override {
    // First cleanup files while peers are still valid
    cleanup_all_files();
    // Then shutdown peers
    shutdown_peers();
    // Sleep briefly to ensure file handles are released
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
    
  void cleanup_all_files() {
    for (const auto& peer : peers) {
      if (!peer || !peer->bootstrap) {
        continue;
      }

      try {
        auto& store = peer->bootstrap->get_file_server().get_store();
        store.clear();
      } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to cleanup files for peer " 
                                 << static_cast<int>(peer->id) << ": " << e.what();
      }
    }
  }
    
  void shutdown_peers() {
    for (auto& peer : peers) {
      if (!peer) {
        continue;
      }

      try {
        if (peer->thread.joinable()) {
          peer->thread.join();
        }
      } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to join thread for peer " 
                                 << static_cast<int>(peer->id) << ": " << e.what();
      }
    }
    peers.clear();
  }

  Peer* create_peer(uint8_t id, uint16_t port, std::vector<std::string> bootstrap_nodes = {}) {
    auto peer = std::make_unique<Peer>(id, port, std::move(bootstrap_nodes));
    peer->bootstrap = std::make_unique<Bootstrap>(
      ADDRESS, port, TEST_KEY, id, peer->bootstrap_nodes
    );
    peers.push_back(std::move(peer));
    return peers.back().get();
  }

  void start_peer(Peer* peer, bool wait = true) {
    peer->thread = std::thread([peer]() {
      ASSERT_TRUE(peer->bootstrap->start()) << "Failed to start peer " << static_cast<int>(peer->id);
    });
    if (wait) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  std::stringstream create_large_file(size_t target_size = 0) {
    if (target_size == 0) target_size = LARGE_FILE_SIZE;

    std::stringstream file_content;
    size_t current_size = 0;
    size_t chunk_number = 0;

    while (current_size < target_size) {
      std::stringstream chunk;
      std::string header = "Chunk[" + std::to_string(chunk_number) + 
                         "] Position:" + std::to_string(current_size) + " Data:";
      size_t data_space = CHUNK_SIZE - header.size() - 1; // -1 for newline

      chunk << header;
      for (size_t i = 0; i < data_space; ++i) {
        chunk << static_cast<char>((chunk_number + i) % 256);
      }
      chunk << '\n';

      std::string chunk_str = chunk.str();
      EXPECT_EQ(chunk_str.size(), CHUNK_SIZE) << "Chunk size mismatch!";

      file_content << chunk_str;
      current_size += CHUNK_SIZE;
      chunk_number++;
    }

    EXPECT_EQ(current_size, target_size) << "Final file size mismatch!";
    return file_content;
  }

  void verify_peer_connections(const std::vector<Peer*>& test_peers) {
    for (size_t i = 0; i < test_peers.size(); i++) {
      for (size_t j = 0; j < test_peers.size(); j++) {
        if (i != j) {
          auto& manager = test_peers[i]->bootstrap->get_peer_manager();
          ASSERT_TRUE(manager.has_peer(test_peers[j]->id)) 
              << "Peer " << static_cast<int>(test_peers[i]->id) 
              << " should be connected to Peer " << static_cast<int>(test_peers[j]->id);
        }
      }
    }
  }

  void verify_file_content(const std::string& filename, const std::string& expected_content, 
                           const std::vector<Peer*>& test_peers) {
    for (const auto& peer : test_peers) {
      auto& file_server = peer->bootstrap->get_file_server();
      ASSERT_TRUE(file_server.get_store().has(filename)) 
        << "File should exist in peer " << static_cast<int>(peer->id) << "'s store";

      std::stringstream retrieved_content;
      file_server.get_store().get(filename, retrieved_content);
      ASSERT_TRUE(retrieved_content.good()) 
        << "Failed to retrieve file from peer " << static_cast<int>(peer->id) << "'s store";
      ASSERT_EQ(retrieved_content.str(), expected_content)
        << "Retrieved file content should match original content in peer " 
        << static_cast<int>(peer->id);
    }
  }
};

TEST_F(BootstrapTest, PeerConnection) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});

  start_peer(peer1);
  start_peer(peer2);

  std::this_thread::sleep_for(std::chrono::seconds(3));
  verify_peer_connections({peer1, peer2});
}

TEST_F(BootstrapTest, DuplicatePeerConnection) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});

  start_peer(peer1);
  start_peer(peer2);

  std::this_thread::sleep_for(std::chrono::seconds(2));
  verify_peer_connections({peer1, peer2});

  EXPECT_FALSE(peer2->bootstrap->connect_to_bootstrap_nodes()) 
      << "Duplicate connection attempt should return false";

  // Verify connections still active after duplicate attempt
  verify_peer_connections({peer1, peer2});
}

TEST_F(BootstrapTest, FileSharing) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});

  // Get both peers to connect
  start_peer(peer1);
  start_peer(peer2);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // Store file in peer1, which shares it with peer2
  std::stringstream file_content;
  file_content << TEST_FILE_CONTENT;
  peer1->bootstrap->get_file_server().store_file(TEST_FILENAME, file_content);

  // Check that peers are connect and that peer2 received the file
  std::this_thread::sleep_for(std::chrono::seconds(3));
  verify_peer_connections({peer1, peer2});
  verify_file_content(TEST_FILENAME, TEST_FILE_CONTENT, {peer1, peer2});
}

TEST_F(BootstrapTest, LargeFileSharing) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});

  // Get both peers to connect
  start_peer(peer1);
  start_peer(peer2);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // Store file in peer1, which shares it with peer2
  auto file_content = create_large_file();
  peer1->bootstrap->get_file_server().store_file("large_test.txt", file_content);

  // Check that peers are connect and that peer2 received the file
  std::this_thread::sleep_for(std::chrono::seconds(2));
  verify_peer_connections({peer1, peer2});
  verify_file_content("large_test.txt", file_content.str(), {peer1, peer2});
}

TEST_F(BootstrapTest, BroadcastFileSharing) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});
  auto peer3 = create_peer(3, 3003, {ADDRESS + ":3001", ADDRESS + ":3002"});

  start_peer(peer1);
  start_peer(peer2);
  start_peer(peer3);

  std::this_thread::sleep_for(std::chrono::seconds(3));
  verify_peer_connections({peer1, peer2, peer3});

  std::stringstream file_content;
  file_content << TEST_FILE_CONTENT;
  peer1->bootstrap->get_file_server().store_file(TEST_FILENAME, file_content);

  std::this_thread::sleep_for(std::chrono::seconds(3));
  verify_file_content(TEST_FILENAME, TEST_FILE_CONTENT, {peer1, peer2, peer3});
}

TEST_F(BootstrapTest, LargeFileBroadcast) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});
  auto peer3 = create_peer(3, 3003, {ADDRESS + ":3001", ADDRESS + ":3002"});

  start_peer(peer1);
  start_peer(peer2);
  start_peer(peer3);

  std::this_thread::sleep_for(std::chrono::seconds(3));
  verify_peer_connections({peer1, peer2, peer3});

  auto file_content = create_large_file();
  peer1->bootstrap->get_file_server().store_file("large_broadcast_test.txt", file_content);

  std::this_thread::sleep_for(std::chrono::seconds(10));
  verify_file_content("large_broadcast_test.txt", file_content.str(), {peer1, peer2, peer3});
}

TEST_F(BootstrapTest, GetFile) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});

  start_peer(peer1);

  std::stringstream file_content;
  file_content << TEST_FILE_CONTENT;
  peer1->bootstrap->get_file_server().store_file(TEST_FILENAME, file_content);

  start_peer(peer2);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  peer2->bootstrap->get_file_server().get_file(TEST_FILENAME);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  verify_peer_connections({peer1, peer2});
  verify_file_content(TEST_FILENAME, TEST_FILE_CONTENT, {peer1, peer2});
}

TEST_F(BootstrapTest, LargeGetFile) {
  auto peer1 = create_peer(1, 3001);
  auto peer2 = create_peer(2, 3002, {ADDRESS + ":3001"});

  start_peer(peer1);

  auto file_content = create_large_file();
  peer1->bootstrap->get_file_server().store_file("large_test.txt", file_content);

  start_peer(peer2);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  peer2->bootstrap->get_file_server().get_file("large_test.txt");
  std::this_thread::sleep_for(std::chrono::seconds(5));

  verify_peer_connections({peer1, peer2});
  verify_file_content("large_test.txt", file_content.str(), {peer1, peer2});
}