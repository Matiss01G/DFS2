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
  const std::vector<uint8_t> TEST_KEY = std::vector<uint8_t>(32, 0x42); // 32-byte test key
};

// TEST_F(BootstrapTest, PeerConnection) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;

//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(3));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID));
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID));
//   EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID));
//   EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID));

//   peer1_thread.join();
//   peer2_thread.join();
// }

// TEST_F(BootstrapTest, DuplicatePeerConnection) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;

//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Initial connection failed - Peer 1 doesn't see Peer 2";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Initial connection failed - Peer 2 doesn't see Peer 1";

//   EXPECT_FALSE(peer2.connect_to_bootstrap_nodes()) 
//     << "Duplicate connection attempt should return false";

//   EXPECT_TRUE(peer1_manager.has_peer(PEER2_ID)) 
//     << "Original connection lost after duplicate attempt - Peer 1 lost Peer 2";
//   EXPECT_TRUE(peer2_manager.has_peer(PEER1_ID))
//     << "Original connection lost after duplicate attempt - Peer 2 lost Peer 1";
//   EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID))
//     << "Original connection no longer active - Peer 1 to Peer 2";
//   EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID))
//     << "Original connection no longer active - Peer 2 to Peer 1";

//   peer1_thread.join();
//   peer2_thread.join();
// }

// TEST_F(BootstrapTest, FileSharing) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;
//   const std::string TEST_FILE_CONTENT = "Test file content";
//   const std::string TEST_FILENAME = "test.txt";

//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(4));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer2 should be connected to Peer1";

//   std::stringstream file_content;
//   file_content << TEST_FILE_CONTENT;

//   auto& file_server1 = peer1.get_file_server();
//   ASSERT_TRUE(file_server1.store_file(TEST_FILENAME, file_content)) 
//     << "Failed to store file in peer1";

//   std::this_thread::sleep_for(std::chrono::seconds(2));

//   // Verify file exists in peer1's store
//   ASSERT_TRUE(file_server1.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer1's store";

//   auto& file_server2 = peer2.get_file_server();
//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server2.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer2's store after sharing";

//   // Verify file content matches in peer2's store
//   std::stringstream retrieved_content;
//   file_server2.get_store().get(TEST_FILENAME, retrieved_content);
//   ASSERT_TRUE(retrieved_content.good()) << "Failed to retrieve file from peer2's store";
//   ASSERT_EQ(retrieved_content.str(), TEST_FILE_CONTENT)
//     << "Retrieved file content should match original content";

//   peer1_thread.join();
//   peer2_thread.join();
// }

// TEST_F(BootstrapTest, LargeFileSharing) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;
//   const std::string TEST_FILENAME = "large_test.txt";

//   // Create a ~2MB file with meaningful content
//   std::stringstream file_content;
//   const size_t target_size = 2 * 1024 * 1024; // 2MB
//   const size_t chunk_size = 1024; // 1KB chunks
//   size_t current_size = 0;
//   size_t chunk_number = 0;

//   while (current_size < target_size) {
//     std::stringstream chunk;
  
//     // Calculate header size first
//     std::string header = "Chunk[" + std::to_string(chunk_number) + 
//               "] Position:" + std::to_string(current_size) + " Data:";
//     size_t header_size = header.size();
  
//     // Calculate remaining space for data
//     size_t data_space = chunk_size - header_size - 1; // -1 for newline
  
//     // Write header
//     chunk << header;
  
//     // Fill remaining space with identifiable but size-controlled data
//     for (size_t i = 0; i < data_space; ++i) {
//       chunk << static_cast<char>((chunk_number + i) % 256);
//     }
  
//     chunk << '\n';
  
//     std::string chunk_str = chunk.str();
//     assert(chunk_str.size() == chunk_size && "Chunk size mismatch!");
  
//     file_content << chunk_str;
//     current_size += chunk_size;
//     chunk_number++;
//   }

//   // Verify final size
//   assert(current_size == target_size && "Final file size mismatch!");
//   std::vector<std::string> peer1_bootstrap_nodes = {};  
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(4));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer2 should be connected to Peer1";

//   auto& file_server1 = peer1.get_file_server();
//   ASSERT_TRUE(file_server1.store_file(TEST_FILENAME, file_content)) 
//     << "Failed to store large file in peer1";

//   std::this_thread::sleep_for(std::chrono::seconds(5)); // Increased wait time for large file

//   // Verify file exists in peer1's store
//   ASSERT_TRUE(file_server1.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer1's store";

//   auto& file_server2 = peer2.get_file_server();
//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server2.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer2's store after sharing";

//   // Verify file content matches in peer2's store
//   std::stringstream retrieved_content;
//   file_server2.get_store().get(TEST_FILENAME, retrieved_content);
//   ASSERT_TRUE(retrieved_content.good()) << "Failed to retrieve large file from peer2's store";
//   ASSERT_EQ(retrieved_content.str().size(), file_content.str().size())
//     << "Retrieved file size should match original size";
//   ASSERT_EQ(retrieved_content.str(), file_content.str())
//     << "Retrieved large file content should match original content";

//   peer1_thread.join();
//   peer2_thread.join();
// }

// TEST_F(BootstrapTest, BroadcastFileSharing) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2, PEER3_ID = 3;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002, PEER3_PORT = 3003;
//   const std::string TEST_FILE_CONTENT = "Test file content";
//   const std::string TEST_FILENAME = "test.txt";

//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};
//   std::vector<std::string> peer3_bootstrap_nodes = {ADDRESS + ":3001", ADDRESS + ":3002"};


//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);
//   Bootstrap peer3(ADDRESS, PEER3_PORT, TEST_KEY, PEER3_ID, peer3_bootstrap_nodes);


//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer3_thread([&peer3]() {
//     ASSERT_TRUE(peer3.start()) << "Failed to start peer 3";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(4));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();
//   auto& peer3_manager = peer3.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
//   ASSERT_TRUE(peer1_manager.has_peer(PEER3_ID)) << "Peer1 should be connected to Peer3";

//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer1 should be connected to Peer1";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER3_ID)) << "Peer2 should be connected to Peer2";

//   ASSERT_TRUE(peer3_manager.has_peer(PEER1_ID)) << "Peer1 should be connected to Peer1";
//   ASSERT_TRUE(peer3_manager.has_peer(PEER2_ID)) << "Peer2 should be connected to Peer2";

//   // Get file server references first
//   auto& file_server1 = peer1.get_file_server();
//   auto& file_server2 = peer2.get_file_server();
//   auto& file_server3 = peer3.get_file_server();

//   // Prepare and store the file
//   std::stringstream file_content;
//   file_content << TEST_FILE_CONTENT;

//   file_server1.store_file(TEST_FILENAME, file_content);

//   std::this_thread::sleep_for(std::chrono::seconds(10));

//   // Verify file exists in peer1's store
//   ASSERT_TRUE(file_server1.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer1's store";

//   std::this_thread::sleep_for(std::chrono::seconds(4));

//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server2.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer2's store after sharing";

//   std::this_thread::sleep_for(std::chrono::seconds(4));

//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server3.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer3's store after sharing";

//   // Verify file content matches in peer2's store
//   std::stringstream retrieved_content;
//   file_server3.get_store().get(TEST_FILENAME, retrieved_content);
//   ASSERT_TRUE(retrieved_content.good()) << "Failed to retrieve file from peer2's store";
//   ASSERT_EQ(retrieved_content.str(), TEST_FILE_CONTENT)
//     << "Retrieved file content should match original content";

//   std::stringstream retrieved_content2;
//   file_server2.get_store().get(TEST_FILENAME, retrieved_content2);
//   ASSERT_TRUE(retrieved_content2.good()) << "Failed to retrieve file from peer2's store";
//   ASSERT_EQ(retrieved_content2.str(), TEST_FILE_CONTENT)
//     << "Retrieved file content should match original content";

//   peer1_thread.join();
//   peer2_thread.join();
//   peer3_thread.join();
// }

// TEST_F(BootstrapTest, LargeFileBroadcast) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2, PEER3_ID = 3;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002, PEER3_PORT = 3003;
//   const std::string TEST_FILENAME = "large_broadcast_test.txt";

//   // Create a ~2MB file with meaningful content
//   std::stringstream file_content;
//   const size_t target_size = 2 * 1024 * 1024; // 2MB
//   const size_t chunk_size = 1024; // 1KB chunks
//   size_t current_size = 0;
//   size_t chunk_number = 0;

//   while (current_size < target_size) {
//     std::stringstream chunk;

//     // Calculate header size first
//     std::string header = "Chunk[" + std::to_string(chunk_number) + 
//                         "] Position:" + std::to_string(current_size) + " Data:";
//     size_t header_size = header.size();

//     // Calculate remaining space for data
//     size_t data_space = chunk_size - header_size - 1; // -1 for newline

//     // Write header
//     chunk << header;

//     // Fill remaining space with identifiable but size-controlled data
//     for (size_t i = 0; i < data_space; ++i) {
//       chunk << static_cast<char>((chunk_number + i) % 256);
//     }

//     chunk << '\n';

//     std::string chunk_str = chunk.str();
//     assert(chunk_str.size() == chunk_size && "Chunk size mismatch!");

//     file_content << chunk_str;
//     current_size += chunk_size;
//     chunk_number++;
//   }

//   // Verify final size
//   assert(current_size == target_size && "Final file size mismatch!");

//   // Set up bootstrap nodes
//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};
//   std::vector<std::string> peer3_bootstrap_nodes = {ADDRESS + ":3001", ADDRESS + ":3002"};

//   // Create peers
//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);
//   Bootstrap peer3(ADDRESS, PEER3_PORT, TEST_KEY, PEER3_ID, peer3_bootstrap_nodes);

//   // Start peers with appropriate delays
//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });
//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });
//   std::this_thread::sleep_for(std::chrono::seconds(1));

//   std::thread peer3_thread([&peer3]() {
//     ASSERT_TRUE(peer3.start()) << "Failed to start peer 3";
//   });
//   std::this_thread::sleep_for(std::chrono::seconds(4));

//   // Verify peer connections
//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();
//   auto& peer3_manager = peer3.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
//   ASSERT_TRUE(peer1_manager.has_peer(PEER3_ID)) << "Peer1 should be connected to Peer3";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer2 should be connected to Peer1";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER3_ID)) << "Peer2 should be connected to Peer3";
//   ASSERT_TRUE(peer3_manager.has_peer(PEER1_ID)) << "Peer3 should be connected to Peer1";
//   ASSERT_TRUE(peer3_manager.has_peer(PEER2_ID)) << "Peer3 should be connected to Peer2";

//   // Get file server references
//   auto& file_server1 = peer1.get_file_server();
//   auto& file_server2 = peer2.get_file_server();
//   auto& file_server3 = peer3.get_file_server();

//   // Store the large file in peer1
//   ASSERT_TRUE(file_server1.store_file(TEST_FILENAME, file_content)) 
//     << "Failed to store large file in peer1";

//   // Increased wait times for large file transmission
//   std::this_thread::sleep_for(std::chrono::seconds(10));

//   // Verify file exists in peer1's store
//   ASSERT_TRUE(file_server1.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer1's store";

//   // Wait for file propagation to peer2
//   std::this_thread::sleep_for(std::chrono::seconds(8));

//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server2.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer2's store after sharing";

//   // Wait for file propagation to peer3
//   std::this_thread::sleep_for(std::chrono::seconds(8));

//   // Verify file exists in peer3's store after sharing
//   ASSERT_TRUE(file_server3.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer3's store after sharing";

//   // Verify file content matches in peer2's store
//   std::stringstream retrieved_content2;
//   file_server2.get_store().get(TEST_FILENAME, retrieved_content2);
//   ASSERT_TRUE(retrieved_content2.good()) << "Failed to retrieve large file from peer2's store";
//   ASSERT_EQ(retrieved_content2.str().size(), file_content.str().size())
//     << "Retrieved file size should match original size in peer2";
//   ASSERT_EQ(retrieved_content2.str(), file_content.str())
//     << "Retrieved large file content should match original content in peer2";

//   // Verify file content matches in peer3's store
//   std::stringstream retrieved_content3;
//   file_server3.get_store().get(TEST_FILENAME, retrieved_content3);
//   ASSERT_TRUE(retrieved_content3.good()) << "Failed to retrieve large file from peer3's store";
//   ASSERT_EQ(retrieved_content3.str().size(), file_content.str().size())
//     << "Retrieved file size should match original size in peer3";
//   ASSERT_EQ(retrieved_content3.str(), file_content.str())
//     << "Retrieved large file content should match original content in peer3";

//   peer1_thread.join();
//   peer2_thread.join();
//   peer3_thread.join();
// }

// TEST_F(BootstrapTest, LargeGetFile) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;
//   const std::string TEST_FILENAME = "large_test.txt";

//   // Create a ~2MB file with meaningful content
//   std::stringstream file_content;
//   const size_t target_size = 2 * 1024 * 1024; // 2MB
//   const size_t chunk_size = 1024; // 1KB chunks
//   size_t current_size = 0;
//   size_t chunk_number = 0;
//   while (current_size < target_size) {
//     std::stringstream chunk;

//     // Calculate header size first
//     std::string header = "Chunk[" + std::to_string(chunk_number) + 
//               "] Position:" + std::to_string(current_size) + " Data:";
//     size_t header_size = header.size();

//     // Calculate remaining space for data
//     size_t data_space = chunk_size - header_size - 1; // -1 for newline

//     // Write header
//     chunk << header;

//     // Fill remaining space with identifiable but size-controlled data
//     for (size_t i = 0; i < data_space; ++i) {
//       chunk << static_cast<char>((chunk_number + i) % 256);
//     }

//     chunk << '\n';

//     std::string chunk_str = chunk.str();
//     assert(chunk_str.size() == chunk_size && "Chunk size mismatch!");

//     file_content << chunk_str;
//     current_size += chunk_size;
//     chunk_number++;
//   }
//   // Verify final size
//   assert(current_size == target_size && "Final file size mismatch!");

//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};
//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });
//   auto& file_server1 = peer1.get_file_server();
//   // Store the large file
//   file_server1.store_file(TEST_FILENAME, file_content);

//   std::this_thread::sleep_for(std::chrono::seconds(3));

//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });
//   std::this_thread::sleep_for(std::chrono::seconds(3));

//   auto& file_server2 = peer2.get_file_server();
//   file_server2.get_file(TEST_FILENAME);

//   std::this_thread::sleep_for(std::chrono::seconds(5));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();
//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer2 should be connected to Peer1";

//   // Verify file exists in peer1's store
//   ASSERT_TRUE(file_server1.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer1's store";

//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server2.get_store().has(TEST_FILENAME)) 
//     << "Large file should exist in peer2's store after sharing";

//   // Verify file content matches in peer2's store
//   std::stringstream retrieved_content;
//   file_server2.get_store().get(TEST_FILENAME, retrieved_content);
//   ASSERT_TRUE(retrieved_content.good()) << "Failed to retrieve large file from peer2's store";
//   ASSERT_EQ(retrieved_content.str().size(), file_content.str().size())
//     << "Retrieved file size should match original size";
//   ASSERT_EQ(retrieved_content.str(), file_content.str())
//     << "Retrieved large file content should match original content";

//   peer1_thread.join();
//   peer2_thread.join();
// }

// TEST_F(BootstrapTest, GetFile) {
//   const uint8_t PEER1_ID = 1, PEER2_ID = 2;
//   const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;
//   const std::string TEST_FILE_CONTENT = "Test file content";
//   const std::string TEST_FILENAME = "test.txt";

//   std::vector<std::string> peer1_bootstrap_nodes = {};
//   std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

//   Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
//   Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

//   std::thread peer1_thread([&peer1]() {
//     ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
//   });
//   auto& file_server1 = peer1.get_file_server();
//   // Prepare and store the file
//   std::stringstream file_content;
//   file_content << TEST_FILE_CONTENT;
//   file_server1.store_file(TEST_FILENAME, file_content);


//   std::this_thread::sleep_for(std::chrono::seconds(3));


//   std::thread peer2_thread([&peer2]() {
//     ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
//   });

//   std::this_thread::sleep_for(std::chrono::seconds(3));

//   auto& file_server2 = peer2.get_file_server();
//   file_server2.get_file(TEST_FILENAME);

//   std::this_thread::sleep_for(std::chrono::seconds(5));

//   auto& peer1_manager = peer1.get_peer_manager();
//   auto& peer2_manager = peer2.get_peer_manager();

//   ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID)) << "Peer1 should be connected to Peer2";
//   ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID)) << "Peer1 should be connected to Peer1";

//   // Verify file exists in peer1's store
//   ASSERT_TRUE(file_server1.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer1's store";

//   // Verify file exists in peer2's store after sharing
//   ASSERT_TRUE(file_server2.get_store().has(TEST_FILENAME)) 
//     << "File should exist in peer2's store after sharing";

//   // Verify file content matches in peer2's store
//   std::stringstream retrieved_content;
//   file_server2.get_store().get(TEST_FILENAME, retrieved_content);
//   ASSERT_TRUE(retrieved_content.good()) << "Failed to retrieve file from peer2's store";
//   ASSERT_EQ(retrieved_content.str(), TEST_FILE_CONTENT)
//     << "Retrieved file content should match original content";

//   peer1_thread.join();
//   peer2_thread.join();
// }