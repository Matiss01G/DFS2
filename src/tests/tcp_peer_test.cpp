#include <gtest/gtest.h>
#include "network/tcp_peer.hpp"
#include "network/channel.hpp"
#include "network/codec.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <limits>
#include <sstream>
#include "crypto/crypto_stream.hpp"


namespace dfs {
namespace network {
namespace test {

class TCP_PeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create cryptographic key (32 bytes for AES-256)
        key_ = std::vector<uint8_t>(32, 0x42);  // Initialize with dummy value

        // Create channel
        channel_ = std::make_unique<Channel>();

        // Setup networking
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context_,
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address("127.0.0.1"), 
                0  // Let OS choose port
            )
        );

        // Get the assigned port
        uint16_t port = acceptor_->local_endpoint().port();

        // Create and connect peers
        setupPeers(port);
    }

    void TearDown() override {
        if (peer1_) peer1_->stop_stream_processing();
        if (peer2_) peer2_->stop_stream_processing();
        acceptor_->close();
        io_context_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    void setupPeers(uint16_t port) {
        // Create peers
        peer1_ = std::make_unique<TCP_Peer>(1, *channel_, key_);
        peer2_ = std::make_unique<TCP_Peer>(2, *channel_, key_);

        // Start acceptor
        acceptor_->async_accept(peer2_->get_socket(),
            [this](const boost::system::error_code& ec) {
                ASSERT_FALSE(ec) << "Accept error: " << ec.message();
            });

        // Connect peer1 to peer2
        peer1_->get_socket().connect(
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address("127.0.0.1"), 
                port
            )
        );

        // Run io_context in a separate thread
        io_thread_ = std::thread([this]() {
            io_context_.run();
        });

        // Give time for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Helper to generate test IV
    std::vector<uint8_t> generate_test_iv() {
        return std::vector<uint8_t>(crypto::CryptoStream::IV_SIZE, 0x42);
    }

    // Protected members accessible by test cases
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<Channel> channel_;
    std::vector<uint8_t> key_;
    std::unique_ptr<TCP_Peer> peer1_;
    std::unique_ptr<TCP_Peer> peer2_;
    std::thread io_thread_;
};

TEST_F(TCP_PeerTest, BasicConnection) {
    ASSERT_TRUE(peer1_->get_socket().is_open()) << "Peer 1 socket not open";
    ASSERT_TRUE(peer2_->get_socket().is_open()) << "Peer 2 socket not open";

    EXPECT_TRUE(peer1_->get_socket().remote_endpoint().port() == 
                peer2_->get_socket().local_endpoint().port())
        << "Peers not connected to each other";
}

TEST_F(TCP_PeerTest, SendData) {
    // Create test data
    std::string test_data = "Hello, TCP Peer!";
    MessageFrame frame;
    frame.message_type = MessageType::STORE_FILE;
    frame.source_id = 1;
    frame.payload_size = test_data.length();
    frame.iv_ = generate_test_iv();  // Initialize IV

    // Create stream for payload
    auto payload = std::make_shared<std::stringstream>();
    payload->write(test_data.c_str(), test_data.length());
    payload->seekg(0);
    frame.payload_stream = payload;

    // Create codec for the receiving peer
    auto recv_codec = std::make_shared<Codec>(key_, *channel_);

    // Start stream processing on receiving peer
    peer2_->set_stream_processor(
        [recv_codec](std::istream& stream) {
            recv_codec->deserialize(stream);
        }
    );
    ASSERT_TRUE(peer2_->start_stream_processing());

    // Serialize and send the frame
    std::stringstream serialized_data;
    Codec codec(key_, *channel_);
    codec.serialize(frame, serialized_data);
    ASSERT_TRUE(peer1_->send_stream(serialized_data));

    // Wait for data to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get the received frame from channel
    MessageFrame received_frame;
    ASSERT_TRUE(channel_->consume(received_frame)) << "No frame received in channel";

    // Verify frame contents
    EXPECT_EQ(received_frame.message_type, frame.message_type);
    EXPECT_EQ(received_frame.source_id, frame.source_id);
    EXPECT_EQ(received_frame.payload_size, frame.payload_size);

    ASSERT_TRUE(received_frame.payload_stream != nullptr);
    std::string received_data((std::istreambuf_iterator<char>(*received_frame.payload_stream)),
                             std::istreambuf_iterator<char>());
    EXPECT_EQ(received_data, test_data) << "Received data doesn't match sent data";
}

} // namespace test
} // namespace network
} // namespace dfs