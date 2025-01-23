#include <gtest/gtest.h>
#include "network/tcp_peer.hpp"
#include "network/channel.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>

namespace dfs {
namespace network {
namespace test {

class TCP_PeerTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    std::vector<uint8_t> test_key_;
    Channel channel1_;
    Channel channel2_;
    
    void SetUp() override {
        // Create a 32-byte test key (AES-256)
        test_key_ = std::vector<uint8_t>(32, 0x42);
    }
    
    void TearDown() override {
        io_context_.stop();
    }
    
    // Helper to create connected peer pairs
    std::pair<std::shared_ptr<TCP_Peer>, std::shared_ptr<TCP_Peer>> create_connected_peers() {
        auto acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context_, 
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address("127.0.0.1"), 0));
                
        uint16_t port = acceptor->local_endpoint().port();
        
        // Create peers
        auto peer1 = std::make_shared<TCP_Peer>(1, channel1_, test_key_);
        auto peer2 = std::make_shared<TCP_Peer>(2, channel2_, test_key_);
        
        // Setup connection thread
        std::thread accept_thread([&]() {
            try {
                auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
                acceptor->accept(*socket);
                peer1->get_socket() = std::move(*socket);
            } catch (const std::exception& e) {
                ADD_FAILURE() << "Accept failed: " << e.what();
            }
        });
        
        // Connect peer2 to peer1
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        socket->connect(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address("127.0.0.1"), port));
        peer2->get_socket() = std::move(*socket);
        
        accept_thread.join();
        acceptor->close();
        
        return {peer1, peer2};
    }
};

// Test basic peer connection and message exchange
TEST_F(TCP_PeerTest, BasicFrameExchange) {
    auto [peer1, peer2] = create_connected_peers();
    
    bool message_received = false;
    std::string test_message = "Hello, Peer!";
    
    // Set up receiver
    peer2->set_stream_processor([&message_received, test_message](std::istream& stream) {
        std::string received;
        std::getline(stream, received);
        EXPECT_EQ(received, test_message);
        message_received = true;
    });
    
    // Start processing on both peers
    ASSERT_TRUE(peer1->start_stream_processing());
    ASSERT_TRUE(peer2->start_stream_processing());
    
    // Send message from peer1 to peer2
    ASSERT_TRUE(peer1->send_message(test_message));
    
    // Wait for message processing (with timeout)
    auto start_time = std::chrono::steady_clock::now();
    while (!message_received) {
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            FAIL() << "Timeout waiting for message";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(message_received);
    
    // Cleanup
    peer1->stop_stream_processing();
    peer2->stop_stream_processing();
}

// Test large message frame transmission
TEST_F(TCP_PeerTest, LargeFrameTransmission) {
    auto [peer1, peer2] = create_connected_peers();
    
    bool message_received = false;
    std::string large_message(1024 * 1024, 'A'); // 1MB message
    
    // Set up receiver
    peer2->set_stream_processor([&message_received, &large_message](std::istream& stream) {
        std::string received;
        std::getline(stream, received);
        EXPECT_EQ(received, large_message);
        message_received = true;
    });
    
    // Start processing
    ASSERT_TRUE(peer1->start_stream_processing());
    ASSERT_TRUE(peer2->start_stream_processing());
    
    // Send large message
    ASSERT_TRUE(peer1->send_message(large_message));
    
    // Wait for message processing
    auto start_time = std::chrono::steady_clock::now();
    while (!message_received) {
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10)) {
            FAIL() << "Timeout waiting for large message";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(message_received);
    
    // Cleanup
    peer1->stop_stream_processing();
    peer2->stop_stream_processing();
}

// Test bidirectional communication
TEST_F(TCP_PeerTest, BidirectionalCommunication) {
    auto [peer1, peer2] = create_connected_peers();
    
    bool peer1_received = false;
    bool peer2_received = false;
    std::string message1 = "Message from Peer 1";
    std::string message2 = "Message from Peer 2";
    
    // Set up peer1 receiver
    peer1->set_stream_processor([&peer1_received, message2](std::istream& stream) {
        std::string received;
        std::getline(stream, received);
        EXPECT_EQ(received, message2);
        peer1_received = true;
    });
    
    // Set up peer2 receiver
    peer2->set_stream_processor([&peer2_received, message1](std::istream& stream) {
        std::string received;
        std::getline(stream, received);
        EXPECT_EQ(received, message1);
        peer2_received = true;
    });
    
    // Start processing on both peers
    ASSERT_TRUE(peer1->start_stream_processing());
    ASSERT_TRUE(peer2->start_stream_processing());
    
    // Send messages in both directions
    ASSERT_TRUE(peer1->send_message(message1));
    ASSERT_TRUE(peer2->send_message(message2));
    
    // Wait for both messages to be received
    auto start_time = std::chrono::steady_clock::now();
    while (!peer1_received || !peer2_received) {
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            FAIL() << "Timeout waiting for bidirectional messages";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(peer1_received);
    EXPECT_TRUE(peer2_received);
    
    // Cleanup
    peer1->stop_stream_processing();
    peer2->stop_stream_processing();
}

// Test error handling for disconnected peer
TEST_F(TCP_PeerTest, DisconnectedPeerHandling) {
    auto [peer1, peer2] = create_connected_peers();
    
    // Start processing
    ASSERT_TRUE(peer1->start_stream_processing());
    ASSERT_TRUE(peer2->start_stream_processing());
    
    // Forcefully close peer2's socket
    peer2->get_socket().close();
    
    // Attempt to send message to disconnected peer
    std::string test_message = "This should fail";
    EXPECT_FALSE(peer1->send_message(test_message));
    
    // Cleanup
    peer1->stop_stream_processing();
    peer2->stop_stream_processing();
}

} // namespace test
} // namespace network
} // namespace dfs
