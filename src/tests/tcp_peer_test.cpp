#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "network/tcp_peer.hpp"

using namespace dfs::network;

class TCP_PeerTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    const uint16_t test_port = 12345;
    
    void SetUp() override {
        // Set up a test server
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context_,
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(),
                test_port
            )
        );
    }

    void TearDown() override {
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }
    }

    // Helper to run test server in background
    void run_test_server() {
        std::thread([this]() {
            try {
                boost::asio::ip::tcp::socket socket(io_context_);
                acceptor_->accept(socket);
                
                // Echo server implementation
                while (socket.is_open()) {
                    boost::asio::streambuf buf;
                    boost::system::error_code ec;
                    size_t n = boost::asio::read(socket, buf, 
                        boost::asio::transfer_at_least(1), ec);
                    
                    if (ec) break;
                    
                    boost::asio::write(socket, buf.data(), ec);
                    if (ec) break;
                }
            }
            catch (const std::exception& e) {
                // Test server error, can be ignored
            }
        }).detach();
    }
};

// Test connection lifecycle
TEST_F(TCP_PeerTest, ConnectionLifecycle) {
    TCP_Peer peer("test_peer");
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::INITIAL);
    EXPECT_EQ(peer.get_peer_id(), "test_peer");
    
    run_test_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test successful connection
    EXPECT_TRUE(peer.connect("127.0.0.1", test_port));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    EXPECT_TRUE(peer.is_connected());
    
    // Test graceful disconnection
    EXPECT_TRUE(peer.disconnect());
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
    EXPECT_FALSE(peer.is_connected());
}

// Test connection error handling
TEST_F(TCP_PeerTest, ConnectionErrorHandling) {
    TCP_Peer peer("error_test_peer");
    
    // Test connection to invalid port
    EXPECT_FALSE(peer.connect("127.0.0.1", 54321));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
    
    // Test connection to invalid address
    EXPECT_FALSE(peer.connect("invalid.local", test_port));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
    
    // Test disconnect from error state
    EXPECT_FALSE(peer.disconnect());
}

// Test stream operations
TEST_F(TCP_PeerTest, StreamOperations) {
    TCP_Peer peer("stream_test_peer");
    
    // Test streams before connection
    EXPECT_EQ(peer.get_output_stream(), nullptr);
    EXPECT_EQ(peer.get_input_stream(), nullptr);
    
    run_test_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port));
    
    // Test streams after connection
    EXPECT_NE(peer.get_output_stream(), nullptr);
    EXPECT_NE(peer.get_input_stream(), nullptr);
    
    // Test data transfer
    std::string test_data = "Hello, TCP!";
    *peer.get_output_stream() << test_data << std::flush;
    
    std::string received_data;
    *peer.get_input_stream() >> received_data;
    
    EXPECT_EQ(received_data, test_data);
    
    peer.disconnect();
}

// Test stream processor functionality
TEST_F(TCP_PeerTest, StreamProcessorOperations) {
    TCP_Peer peer("processor_test_peer");
    std::string received_data;
    
    run_test_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port));
    
    // Test processor setup and execution
    peer.set_stream_processor([&received_data](std::istream& stream) {
        std::string data;
        stream >> data;
        received_data = data;
    });
    
    // Test processing start/stop
    EXPECT_TRUE(peer.start_stream_processing());
    std::string test_data = "ProcessThis";
    *peer.get_output_stream() << test_data << std::flush;
    
    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(received_data, test_data);
    
    // Test cleanup
    peer.stop_stream_processing();
    peer.disconnect();
}

// Test concurrent operations
TEST_F(TCP_PeerTest, ConcurrentOperations) {
    TCP_Peer peer("concurrent_test_peer");
    std::atomic<int> process_count{0};
    
    run_test_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port));
    
    // Set up processor that counts processed messages
    peer.set_stream_processor([&process_count](std::istream& stream) {
        std::string data;
        stream >> data;
        process_count++;
    });
    
    EXPECT_TRUE(peer.start_stream_processing());
    
    // Send multiple messages concurrently
    const int message_count = 5;
    for(int i = 0; i < message_count; i++) {
        *peer.get_output_stream() << "Message" << i << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Allow time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(process_count, message_count);
    
    peer.stop_stream_processing();
    peer.disconnect();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
