#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "network/tcp_peer.hpp"

using namespace dfs::network;

class TCPPeerTest : public ::testing::Test {
protected:
    // Test server components
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::thread server_thread_;
    bool server_running_{false};
    
    // Test configuration
    const uint16_t TEST_PORT = 12345;
    const std::string TEST_ADDRESS = "127.0.0.1";
    const std::string PEER_ID = "test_peer";
    
    void SetUp() override {
        start_test_server();
    }
    
    void TearDown() override {
        stop_test_server();
    }
    
    void start_test_server() {
        try {
            acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
                io_context_,
                boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::tcp::v4(), TEST_PORT));
            
            server_running_ = true;
            server_thread_ = std::thread([this]() {
                while (server_running_) {
                    try {
                        if (acceptor_->is_open()) {
                            boost::asio::ip::tcp::socket socket(io_context_);
                            acceptor_->accept(socket);
                        }
                    }
                    catch (const std::exception&) {
                        // Ignore errors during shutdown
                    }
                }
            });
        }
        catch (const std::exception& e) {
            FAIL() << "Failed to start test server: " << e.what();
        }
    }
    
    void stop_test_server() {
        server_running_ = false;
        if (acceptor_) {
            acceptor_->close();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
};

// Test connection lifecycle
TEST_F(TCPPeerTest, ConnectionLifecycle) {
    TCP_Peer peer(PEER_ID);
    
    // Initial state
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::INITIAL);
    
    // Connect
    EXPECT_TRUE(peer.connect(TEST_ADDRESS, TEST_PORT));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    
    // Disconnect
    EXPECT_TRUE(peer.disconnect());
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
}

// Test invalid connection attempts
TEST_F(TCPPeerTest, InvalidConnectionAttempts) {
    TCP_Peer peer(PEER_ID);
    
    // Try connecting to invalid port
    EXPECT_FALSE(peer.connect(TEST_ADDRESS, 54321));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
}

// Test stream operations
TEST_F(TCPPeerTest, StreamOperations) {
    TCP_Peer peer(PEER_ID);
    
    // Before connection
    EXPECT_EQ(peer.get_input_stream(), nullptr);
    EXPECT_EQ(peer.get_output_stream(), nullptr);
    
    // After connection
    EXPECT_TRUE(peer.connect(TEST_ADDRESS, TEST_PORT));
    EXPECT_NE(peer.get_input_stream(), nullptr);
    EXPECT_NE(peer.get_output_stream(), nullptr);
    
    // After disconnection
    peer.disconnect();
    EXPECT_EQ(peer.get_input_stream(), nullptr);
    EXPECT_EQ(peer.get_output_stream(), nullptr);
}

// Test stream processor
TEST_F(TCPPeerTest, StreamProcessor) {
    TCP_Peer peer(PEER_ID);
    std::atomic<bool> processor_called{false};
    
    // Set up stream processor
    peer.set_stream_processor([&processor_called](std::istream& stream) {
        processor_called = true;
    });
    
    // Try starting processor before connection
    EXPECT_FALSE(peer.start_stream_processing());
    
    // Connect and start processor
    EXPECT_TRUE(peer.connect(TEST_ADDRESS, TEST_PORT));
    EXPECT_TRUE(peer.start_stream_processing());
    
    // Stop processor
    peer.stop_stream_processing();
    peer.disconnect();
}

// Test multiple connection attempts
TEST_F(TCPPeerTest, MultipleConnectionAttempts) {
    TCP_Peer peer(PEER_ID);
    
    // First connection
    EXPECT_TRUE(peer.connect(TEST_ADDRESS, TEST_PORT));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    
    // Try connecting while already connected
    EXPECT_FALSE(peer.connect(TEST_ADDRESS, TEST_PORT));
    
    // Disconnect and try again
    EXPECT_TRUE(peer.disconnect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(peer.connect(TEST_ADDRESS, TEST_PORT)); // Should fail as we're in DISCONNECTED state
}

// Test peer ID
TEST_F(TCPPeerTest, PeerIdentification) {
    TCP_Peer peer(PEER_ID);
    EXPECT_EQ(peer.get_peer_id(), PEER_ID);
}

// Main function is defined in network_test.cpp
