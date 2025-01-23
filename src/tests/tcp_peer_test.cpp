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
    std::shared_ptr<Channel> channel1_;
    std::shared_ptr<Channel> channel2_;
    std::vector<uint8_t> test_key_;
    std::unique_ptr<TCP_Peer> peer1_;
    std::unique_ptr<TCP_Peer> peer2_;
    
    void SetUp() override {
        // Create a 32-byte test key (for AES-256)
        test_key_ = std::vector<uint8_t>(32, 0x42);
        
        // Initialize channels
        channel1_ = std::make_shared<Channel>();
        channel2_ = std::make_shared<Channel>();
        
        // Create peers with different IDs (1 and 2)
        peer1_ = std::make_unique<TCP_Peer>(1, *channel1_, test_key_);
        peer2_ = std::make_unique<TCP_Peer>(2, *channel2_, test_key_);
    }
    
    void TearDown() override {
        peer1_.reset();
        peer2_.reset();
        channel1_.reset();
        channel2_.reset();
    }
    
    // Helper function to create a connected pair of sockets
    std::pair<std::shared_ptr<boost::asio::ip::tcp::socket>, 
              std::shared_ptr<boost::asio::ip::tcp::socket>> 
    create_connected_sockets() {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address("127.0.0.1"), 0);
        boost::asio::ip::tcp::acceptor acceptor(io_context_, ep);
        
        auto socket1 = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        auto socket2 = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        
        acceptor.listen();
        
        bool connected = false;
        std::thread connect_thread([&]() {
            socket2->connect(acceptor.local_endpoint());
            connected = true;
        });
        
        acceptor.accept(*socket1);
        connect_thread.join();
        
        EXPECT_TRUE(connected);
        return {socket1, socket2};
    }
};

// Test basic connection between two peers
TEST_F(TCP_PeerTest, BasicConnection) {
    // Create connected socket pair
    auto [socket1, socket2] = create_connected_sockets();
    
    // Assign sockets to peers
    peer1_->get_socket() = std::move(*socket1);
    peer2_->get_socket() = std::move(*socket2);
    
    // Start stream processing for both peers
    ASSERT_TRUE(peer1_->start_stream_processing());
    ASSERT_TRUE(peer2_->start_stream_processing());
    
    // Verify both peers are connected
    EXPECT_TRUE(peer1_->get_socket().is_open());
    EXPECT_TRUE(peer2_->get_socket().is_open());
    
    // Stop processing
    peer1_->stop_stream_processing();
    peer2_->stop_stream_processing();
}

// Test sending data between peers using MessageFrame
TEST_F(TCP_PeerTest, SendData) {
    // Create connected socket pair
    auto [socket1, socket2] = create_connected_sockets();
    
    // Assign sockets to peers
    peer1_->get_socket() = std::move(*socket1);
    peer2_->get_socket() = std::move(*socket2);
    
    // Create test data in a MessageFrame
    MessageFrame test_frame;
    test_frame.message_type = MessageType::STORE_FILE;
    test_frame.source_id = 1;
    test_frame.filename_length = 8;
    test_frame.payload_stream = std::make_shared<std::stringstream>();
    *test_frame.payload_stream << "testfile";
    
    // Set up receiver to process incoming data
    peer2_->set_stream_processor([&](std::istream& stream) {
        peer2_->codec_->deserialize(stream);
    });
    
    // Start stream processing for both peers
    ASSERT_TRUE(peer1_->start_stream_processing());
    ASSERT_TRUE(peer2_->start_stream_processing());
    
    // Serialize and send the frame
    std::stringstream serialized_data;
    ASSERT_TRUE(peer1_->codec_->serialize(test_frame, serialized_data));
    ASSERT_TRUE(peer1_->send_stream(serialized_data));
    
    // Allow time for data transmission and processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify received data
    MessageFrame received_frame;
    ASSERT_TRUE(channel2_->consume(received_frame));
    
    // Compare the received frame with the sent frame
    EXPECT_EQ(received_frame.message_type, test_frame.message_type);
    EXPECT_EQ(received_frame.source_id, test_frame.source_id);
    EXPECT_EQ(received_frame.filename_length, test_frame.filename_length);
    
    // Compare payload contents
    std::string received_payload;
    received_frame.payload_stream->seekg(0);
    std::getline(*received_frame.payload_stream, received_payload);
    EXPECT_EQ(received_payload, "testfile");
    
    // Stop processing
    peer1_->stop_stream_processing();
    peer2_->stop_stream_processing();
}

} // namespace test
} // namespace network
} // namespace dfs
