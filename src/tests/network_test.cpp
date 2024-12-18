#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_peer.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <sstream>

using namespace dfs::network;

class TCPPeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_peer = std::make_unique<TCP_Peer>("test_peer");
        server_endpoint.address(boost::asio::ip::make_address("127.0.0.1"));
        server_endpoint.port(12345);
        
        // Set up test server
        acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context, server_endpoint
        );
        acceptor->listen();
    }

    void TearDown() override {
        if (test_peer && test_peer->is_connected()) {
            test_peer->disconnect();
        }
        if (server_socket && server_socket->is_open()) {
            server_socket->close();
        }
        acceptor->close();
    }

    // Helper method to accept connection in server
    void accept_connection() {
        server_socket = std::make_unique<boost::asio::ip::tcp::socket>(io_context);
        acceptor->accept(*server_socket);
    }

    // Helper method to run server operations in background
    void start_server_thread() {
        server_thread = std::thread([this]() {
            accept_connection();
        });
    }

    std::unique_ptr<TCP_Peer> test_peer;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::endpoint server_endpoint;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
    std::unique_ptr<boost::asio::ip::tcp::socket> server_socket;
    std::thread server_thread;
};

// Test basic connection functionality
TEST_F(TCPPeerTest, ConnectionTest) {
    start_server_thread();
    
    // Test connection
    EXPECT_TRUE(test_peer->connect("127.0.0.1", 12345));
    EXPECT_TRUE(test_peer->is_connected());
    
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Test disconnection functionality
TEST_F(TCPPeerTest, DisconnectionTest) {
    start_server_thread();
    
    ASSERT_TRUE(test_peer->connect("127.0.0.1", 12345));
    EXPECT_TRUE(test_peer->disconnect());
    EXPECT_FALSE(test_peer->is_connected());
    
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Test sending data through socket using send_stream
TEST_F(TCPPeerTest, SendStreamTest) {
    start_server_thread();
    
    ASSERT_TRUE(test_peer->connect("127.0.0.1", 12345));
    
    // Wait for server to accept connection
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    // Prepare test data
    std::string test_data = "Hello, TCP Peer!\n";
    std::istringstream input_stream(test_data);
    
    // Send data using send_stream
    EXPECT_TRUE(test_peer->send_stream(input_stream));
    
    // Verify data received on server side
    std::vector<char> received_data(test_data.size());
    boost::system::error_code ec;
    size_t bytes_read = boost::asio::read(*server_socket,
        boost::asio::buffer(received_data),
        boost::asio::transfer_exactly(test_data.size()),
        ec
    );
    
    EXPECT_FALSE(ec);
    EXPECT_EQ(bytes_read, test_data.size());
    EXPECT_EQ(std::string(received_data.begin(), received_data.end()), test_data);
}

// Test receiving data through input stream
TEST_F(TCPPeerTest, ReceiveStreamTest) {
    start_server_thread();
    
    ASSERT_TRUE(test_peer->connect("127.0.0.1", 12345));
    
    // Wait for server to accept connection
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    // Send test data from server
    std::string test_data = "Server response!\n";
    boost::system::error_code ec;
    boost::asio::write(*server_socket, 
        boost::asio::buffer(test_data),
        boost::asio::transfer_exactly(test_data.size()),
        ec
    );
    ASSERT_FALSE(ec);
    
    // Setup synchronization primitives
    std::mutex mtx;
    std::condition_variable cv;
    std::string received_data;
    bool data_received = false;

    // Setup stream processor with synchronization
    test_peer->set_stream_processor([&](std::istream& stream) {
        std::string line;
        std::getline(stream, line);
        {
            std::lock_guard<std::mutex> lock(mtx);
            received_data = line + '\n';  // Add back the newline that getline removes
            data_received = true;
        }
        cv.notify_one();
    });
    
    // Start stream processing
    ASSERT_TRUE(test_peer->start_stream_processing());
    
    // Wait for data with timeout
    {
        std::unique_lock<std::mutex> lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2), [&]{ return data_received; }));
        EXPECT_EQ(received_data, test_data);
    }
    
    // Stop processing and cleanup
    test_peer->stop_stream_processing();
}

// Test edge cases and error conditions
TEST_F(TCPPeerTest, EdgeCasesTest) {
    // Test connecting to non-existent server
    EXPECT_FALSE(test_peer->connect("127.0.0.1", 54321));
    EXPECT_FALSE(test_peer->is_connected());
    
    // Test disconnecting when not connected
    EXPECT_FALSE(test_peer->disconnect());
    
    // Test sending when not connected
    std::istringstream input_stream("Test message\n");
    EXPECT_FALSE(test_peer->send_stream(input_stream));
    
    // Test getting input stream when not connected
    EXPECT_EQ(test_peer->get_input_stream(), nullptr);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
