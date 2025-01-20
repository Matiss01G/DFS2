#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "file_server/file_server.hpp"
#include "network/peer_manager.hpp"
#include "network/channel.hpp"
#include <sstream>
#include <vector>

using namespace dfs::network;
using ::testing::Return;
using ::testing::_;

class FileServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test components
        std::vector<uint8_t> test_key(32, 0x42); // 32-byte test key
        file_server = std::make_unique<FileServer>(1, test_key, peer_manager, channel);
    }

    void TearDown() override {
        // Cleanup
    }

    PeerManager peer_manager;
    Channel channel;
    std::unique_ptr<FileServer> file_server;
};

TEST_F(FileServerTest, PrepareAndSendStoreFile) {
    // Test data
    std::string filename = "test_file.txt";
    std::stringstream input;
    input << "Test content";
    
    // Store file first
    ASSERT_TRUE(file_server->store_file(filename, input));

    // Test prepare_and_send with STORE_FILE
    EXPECT_TRUE(file_server->prepare_and_send(filename, std::nullopt, MessageType::STORE_FILE));
}

TEST_F(FileServerTest, PrepareAndSendGetFile) {
    // Test data
    std::string filename = "test_file.txt";
    std::stringstream input;
    input << "Test content";
    
    // Store file first
    ASSERT_TRUE(file_server->store_file(filename, input));

    // Test prepare_and_send with GET_FILE
    std::string peer_id = "test_peer";
    EXPECT_TRUE(file_server->prepare_and_send(filename, peer_id, MessageType::GET_FILE));
}

TEST_F(FileServerTest, PrepareAndSendWithDefaultMessageType) {
    // Test data
    std::string filename = "test_file.txt";
    std::stringstream input;
    input << "Test content";
    
    // Store file first
    ASSERT_TRUE(file_server->store_file(filename, input));

    // Test prepare_and_send with default message type (should be STORE_FILE)
    EXPECT_TRUE(file_server->prepare_and_send(filename));
}
