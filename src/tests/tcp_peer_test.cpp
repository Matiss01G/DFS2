#include <gtest/gtest.h>
#include "network/tcp_peer.hpp"
#include "network/channel.hpp"
#include "network/codec.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <chrono>

namespace dfs {
namespace network {
namespace test {

class TCPPeerTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    Channel channel_;
    std::vector<uint8_t> test_key_;
    
    void SetUp() override {
        // Generate a 32-byte test key
        test_key_.resize(32, 0x42); // Fill with arbitrary value
    }

    void TearDown() override {
        io_context_.stop();
    }

    // Helper to create connected TCP peer pairs
    std::pair<std::shared_ptr<TCP_Peer>, std::shared_ptr<TCP_Peer>> create_connected_peers() {
        // Create socket pair
        boost::asio::ip::tcp::acceptor acceptor(io_context_, 
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
        
        auto client_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        auto server_socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        
        // Start accept
        acceptor.async_accept(*server_socket, [](const boost::system::error_code&) {});
        
        // Connect client
        client_socket->connect(acceptor.local_endpoint());
        
        // Wait for accept to complete
        io_context_.run_one();
        
        // Create peers
        auto peer1 = std::make_shared<TCP_Peer>(1, channel_, test_key_);
        auto peer2 = std::make_shared<TCP_Peer>(2, channel_, test_key_);
        
        // Move sockets to peers
        peer1->get_socket() = std::move(*client_socket);
        peer2->get_socket() = std::move(*server_socket);
        
        return {peer1, peer2};
    }
};

TEST_F(TCPPeerTest, SendReceiveMessageFrame) {
    // Create connected peers
    auto [sender, receiver] = create_connected_peers();
    
    // Create message frame
    MessageFrame test_frame;
    test_frame.message_type = MessageType::STORE_FILE;
    test_frame.source_id = 1;
    test_frame.filename_length = 8;
    std::string test_data = "test.txt";
    test_frame.payload_stream = std::make_shared<std::stringstream>();
    *test_frame.payload_stream << test_data;

    // Create codec for serialization
    Codec codec(test_key_, channel_);

    // Setup stream processor for receiver
    bool frame_received = false;
    receiver->set_stream_processor(
        [&](std::istream& stream) {
            codec.deserialize(stream);
            frame_received = true;
        }
    );

    // Start receiver's stream processing
    receiver->start_stream_processing();

    // Serialize and send frame
    std::stringstream serialized_stream;
    ASSERT_TRUE(codec.serialize(test_frame, serialized_stream));
    ASSERT_TRUE(sender->send_stream(serialized_stream));

    // Wait for processing (up to 1 second)
    for (int i = 0; i < 100 && !frame_received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        io_context_.poll_one();
    }

    ASSERT_TRUE(frame_received);

    // Consume and verify frame from channel
    MessageFrame received_frame;
    ASSERT_TRUE(channel_.consume(received_frame));
    ASSERT_EQ(received_frame.message_type, test_frame.message_type);
    ASSERT_EQ(received_frame.source_id, test_frame.source_id);
    ASSERT_EQ(received_frame.filename_length, test_frame.filename_length);

    // Verify payload content
    std::stringstream received_data;
    *received_frame.payload_stream >> received_data.rdbuf();
    ASSERT_EQ(received_data.str(), test_data);

    // Cleanup
    sender->stop_stream_processing();
    receiver->stop_stream_processing();
}

} // namespace test
} // namespace network
} // namespace dfs
