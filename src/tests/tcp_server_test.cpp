#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_server.hpp"
#include "network/peer_manager.hpp"
#include <thread>
#include <chrono>

using namespace dfs::network;
using ::testing::_;

class TCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test components
        peer_manager = std::make_unique<PeerManager>();
    }

    void TearDown() override {
        if (server) {
            server->shutdown();
        }
    }

    std::unique_ptr<PeerManager> peer_manager;
    std::unique_ptr<TCP_Server> server;
};

TEST_F(TCPServerTest, InitializationTest) {
    server = std::make_unique<TCP_Server>(12345, "127.0.0.1", *peer_manager);
    ASSERT_NE(server, nullptr);
}

TEST_F(TCPServerTest, StartListenerTest) {
    server = std::make_unique<TCP_Server>(12345, "127.0.0.1", *peer_manager);
    ASSERT_TRUE(server->start_listener());

    // Allow some time for the server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server->shutdown();
}

TEST_F(TCPServerTest, MultipleStartTest) {
    server = std::make_unique<TCP_Server>(12345, "127.0.0.1", *peer_manager);

    ASSERT_TRUE(server->start_listener());
    ASSERT_FALSE(server->start_listener()); // Second start should fail

    server->shutdown();
}

TEST_F(TCPServerTest, ShutdownTest) {
    server = std::make_unique<TCP_Server>(12345, "127.0.0.1", *peer_manager);

    ASSERT_TRUE(server->start_listener());
    server->shutdown();

    // Should be able to start again after shutdown
    ASSERT_TRUE(server->start_listener());
}

TEST_F(TCPServerTest, ConnectionTest) {
    server = std::make_unique<TCP_Server>(12345, "127.0.0.1", *peer_manager);
    ASSERT_TRUE(server->start_listener());

    // Create a client socket
    boost::asio::io_context client_io_context;
    boost::asio::ip::tcp::socket client_socket(client_io_context);

    try {
        // Connect to server
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address("127.0.0.1"), 12345);
        client_socket.connect(endpoint);

        // Allow some time for connection processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        EXPECT_GT(peer_manager->size(), 0);

        client_socket.close();
    } catch (const std::exception& e) {
        FAIL() << "Connection failed: " << e.what();
    }
}