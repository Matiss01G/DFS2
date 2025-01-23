#include <gtest/gtest.h>
#include "network/tcp_peer.hpp"
#include "network/channel.hpp"
#include "network/codec.hpp"
#include "crypto/crypto_stream.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <memory>
#include <vector>
#include <sstream>

namespace dfs {
namespace network {
namespace test {

class TCP_PeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test key (32 bytes for AES-256)
        key_ = std::vector<uint8_t>(32, 0x42);  // Fill with arbitrary value 0x42

        // Create channel for communication
        channel_ = std::make_shared<Channel>();

        // Create io_context for network operations
        io_context_ = std::make_shared<boost::asio::io_context>();

        // Create acceptor for server socket
        acceptor_ = std::make_shared<boost::asio::ip::tcp::acceptor>(
            *io_context_,
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(),
                0  // Let OS choose port
            )
        );
    }

    void TearDown() override {
        // Stop stream processing for peers if they exist
        if (peer1_) peer1_->stop_stream_processing();
        if (peer2_) peer2_->stop_stream_processing();

        // Close sockets and cleanup
        if (peer1_) peer1_->get_socket().close();
        if (peer2_) peer2_->get_socket().close();

        io_context_->stop();
    }

    // Helper method to create connected peer pair
    void create_connected_peers() {
        // Create client socket
        auto client_socket = std::make_shared<boost::asio::ip::tcp::socket>(*io_context_);

        // Create server socket by accepting connection
        std::thread accept_thread([this]() {
            auto server_socket = std::make_shared<boost::asio::ip::tcp::socket>(*io_context_);
            acceptor_->accept(*server_socket);
            peer2_ = std::make_shared<TCP_Peer>(2, *channel_, key_);
            peer2_->get_socket() = std::move(*server_socket);
        });

        // Connect client socket
        client_socket->connect(acceptor_->local_endpoint());

        // Create peer1 with client socket
        peer1_ = std::make_shared<TCP_Peer>(1, *channel_, key_);
        peer1_->get_socket() = std::move(*client_socket);

        accept_thread.join();

        // Set up stream processors for both peers
        peer1_->set_stream_processor([this](std::istream& stream) {
            Codec codec(key_, *channel_);
            auto frame = codec.deserialize(stream);
            channel_->produce(frame);
        });

        peer2_->set_stream_processor([this](std::istream& stream) {
            Codec codec(key_, *channel_);
            auto frame = codec.deserialize(stream);
            channel_->produce(frame);
        });

        // Start stream processing
        peer1_->start_stream_processing();
        peer2_->start_stream_processing();
    }

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::shared_ptr<Channel> channel_;
    std::vector<uint8_t> key_;
    std::shared_ptr<TCP_Peer> peer1_;
    std::shared_ptr<TCP_Peer> peer2_;
};

TEST_F(TCP_PeerTest, BasicConnection) {
    create_connected_peers();
    ASSERT_TRUE(peer1_->get_socket().is_open());
    ASSERT_TRUE(peer2_->get_socket().is_open());
}

TEST_F(TCP_PeerTest, SendReceiveSimpleMessage) {
    create_connected_peers();

    std::string test_message = "Hello, peer!";
    std::istringstream iss(test_message);

    ASSERT_TRUE(peer1_->send_stream(iss));

    // Wait briefly for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(TCP_PeerTest, SerializeDeserializeMessageFrame) {
    create_connected_peers();

    // Create test message frame
    MessageFrame send_frame;
    send_frame.message_type = MessageType::STORE_FILE;
    send_frame.source_id = 1;
    std::string test_data = "Test payload data";
    send_frame.payload_stream = std::make_shared<std::stringstream>(test_data);

    // Generate IV using CryptoStream
    crypto::CryptoStream crypto_stream;
    auto iv_array = crypto_stream.generate_IV();
    send_frame.iv_.assign(iv_array.begin(), iv_array.end());

    // Create serialized stream
    std::stringstream serialized_stream;
    Codec send_codec(key_, *channel_);
    ASSERT_TRUE(send_codec.serialize(send_frame, serialized_stream));

    // Send serialized data
    ASSERT_TRUE(peer1_->send_stream(serialized_stream));

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Try to consume the frame from channel
    MessageFrame received_frame;
    ASSERT_TRUE(channel_->consume(received_frame));

    // Verify received frame contents
    EXPECT_EQ(received_frame.message_type, send_frame.message_type);
    EXPECT_EQ(received_frame.source_id, send_frame.source_id);

    // Compare payload data
    std::stringstream received_data;
    received_data << received_frame.payload_stream->rdbuf();
    EXPECT_EQ(received_data.str(), test_data);
}

} // namespace test
} // namespace network
} // namespace dfs