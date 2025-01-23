#include <gtest/gtest.h>
#include "network/tcp_peer.hpp"
#include "network/channel.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>

namespace dfs {
namespace network {
namespace test {

class TCP_PeerTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    std::shared_ptr<Channel> channel1_;
    std::shared_ptr<Channel> channel2_;
    std::vector<uint8_t> test_key_;
    std::unique_ptr<TCP_Peer> peer1_;
    std::unique_ptr<TCP_Peer> peer2_;
    uint16_t test_port_;

    void SetUp() override {
        // Initialize test key (32 bytes for AES-256)
        test_key_ = std::vector<uint8_t>(32, 0x42);  // Fill with test pattern
        test_port_ = 12345;
        
        // Create channels
        channel1_ = std::make_shared<Channel>();
        channel2_ = std::make_shared<Channel>();
        
        // Create TCP endpoints
        auto acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(
            io_context_,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), test_port_)
        );

        // Create socket for peer2 (client)
        auto socket2 = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);

        // Accept incoming connection asynchronously
        acceptor->async_accept(
            [this, acceptor](const boost::system::error_code& error, boost::asio::ip::tcp::socket peer_socket) {
                if (!error) {
                    // Create peer1 (server) with accepted socket
                    peer1_ = std::make_unique<TCP_Peer>(1, *channel1_, test_key_);
                    peer1_->get_socket() = std::move(peer_socket);
                }
            });

        // Connect peer2 to peer1
        socket2->async_connect(
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::address::from_string("127.0.0.1"), 
                test_port_
            ),
            [this, socket2](const boost::system::error_code& error) {
                if (!error) {
                    // Create peer2 with connected socket
                    peer2_ = std::make_unique<TCP_Peer>(2, *channel2_, test_key_);
                    peer2_->get_socket() = std::move(*socket2);
                }
            });

        // Run the io_context to complete the connection
        io_context_.run();
    }

    void TearDown() override {
        if (peer1_) peer1_->stop_stream_processing();
        if (peer2_) peer2_->stop_stream_processing();
        io_context_.stop();
    }
};

TEST_F(TCP_PeerTest, BasicConnection) {
    ASSERT_TRUE(peer1_) << "Peer1 was not created successfully";
    ASSERT_TRUE(peer2_) << "Peer2 was not created successfully";
    
    // Verify both sockets are open
    EXPECT_TRUE(peer1_->get_socket().is_open()) << "Peer1 socket is not open";
    EXPECT_TRUE(peer2_->get_socket().is_open()) << "Peer2 socket is not open";
    
    // Verify endpoints are connected
    auto endpoint1 = peer1_->get_socket().remote_endpoint();
    auto endpoint2 = peer2_->get_socket().remote_endpoint();
    
    EXPECT_EQ(endpoint1.address().to_string(), "127.0.0.1");
    EXPECT_EQ(endpoint2.address().to_string(), "127.0.0.1");
}

TEST_F(TCP_PeerTest, SendData) {
    ASSERT_TRUE(peer1_) << "Peer1 was not created successfully";
    ASSERT_TRUE(peer2_) << "Peer2 was not created successfully";

    // Start stream processing on both peers
    ASSERT_TRUE(peer1_->start_stream_processing());
    ASSERT_TRUE(peer2_->start_stream_processing());

    // Create test data
    std::string test_data = "Hello, peer!";
    MessageFrame send_frame(MessageType::DATA, 1, test_data);
    
    // Create streams for serialization
    std::stringstream serialized_stream;
    peer1_->codec_->serialize(serialized_stream, send_frame);

    // Send data from peer1 to peer2
    ASSERT_TRUE(peer1_->send_stream(serialized_stream));

    // Wait for data to be processed (max 5 seconds)
    auto start_time = std::chrono::steady_clock::now();
    MessageFrame received_frame;
    bool frame_received = false;

    while (!frame_received && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        if (channel2_->try_get_frame(received_frame)) {
            frame_received = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(frame_received) << "Did not receive frame within timeout";
    
    // Verify received data matches sent data
    EXPECT_EQ(received_frame.get_type(), MessageType::DATA);
    EXPECT_EQ(received_frame.get_source_id(), 1);
    EXPECT_EQ(received_frame.get_payload(), test_data);
}

} // namespace test
} // namespace network
} // namespace dfs
