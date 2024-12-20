#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_peer.hpp"
#include "test_utils.hpp"
#include <boost/asio.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>

using namespace dfs::network;
using ::testing::Return;
using ::testing::_;

class TCPPeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_logging(); // Initialize logging with reduced verbosity
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

    // Setup synchronization primitives
    std::mutex mtx;
    std::condition_variable cv;
    std::string received_data;
    bool data_received = false;

    // Setup stream processor with synchronization
    test_peer->set_stream_processor([&](std::istream& stream) {
        try {
            std::string line;
            std::getline(stream, line);
            {
                std::lock_guard<std::mutex> lock(mtx);
                received_data = line + '\n';  // Add back the newline that getline removes
                data_received = true;
            }
            cv.notify_one();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
        }
    });

    // Start stream processing before sending data
    ASSERT_TRUE(test_peer->start_stream_processing());

    // Send test data from server after stream processing is started
    std::string test_data = "Server response!\n";
    {
        boost::system::error_code ec;
        boost::asio::write(*server_socket,
            boost::asio::buffer(test_data),
            boost::asio::transfer_exactly(test_data.size()),
            ec
        );
        ASSERT_FALSE(ec) << "Failed to write test data: " << ec.message();
    }

    // Wait for data with a shorter timeout
    {
        std::unique_lock<std::mutex> lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::milliseconds(500), [&]{ return data_received; }))
            << "Timeout waiting for data reception";
        EXPECT_EQ(received_data, test_data);
    }

    // Stop processing and cleanup
    test_peer->stop_stream_processing();
}

// Test receiving multiple messages through same connection
TEST_F(TCPPeerTest, MultipleMessagesTest) {
    start_server_thread();

    ASSERT_TRUE(test_peer->connect("127.0.0.1", 12345));

    // Wait for server to accept connection
    if (server_thread.joinable()) {
        server_thread.join();
    }

    // Setup synchronization primitives
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<std::string> received_messages;
    std::atomic<int> messages_received{0};
    const int expected_messages = 3;

    // Setup stream processor with synchronization
    test_peer->set_stream_processor([&](std::istream& stream) {
        try {
            std::string line;
            std::getline(stream, line);
            {
                std::lock_guard<std::mutex> lock(mtx);
                received_messages.push_back(line + '\n');  // Add back the newline that getline removes
                messages_received++;
            }
            cv.notify_one();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
        }
    });

    // Start stream processing before sending data
    ASSERT_TRUE(test_peer->start_stream_processing());

    // Test messages to send
    std::vector<std::string> test_messages = {
        "First message!\n",
        "Second message!\n",
        "Third message!\n"
    };

    // Send messages with small delays between them
    for (const auto& msg : test_messages) {
        boost::system::error_code ec;
        boost::asio::write(*server_socket,
            boost::asio::buffer(msg),
            boost::asio::transfer_exactly(msg.size()),
            ec
        );
        ASSERT_FALSE(ec) << "Failed to write message: " << ec.message();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Small delay between messages
    }

    // Wait for all messages with timeout
    {
        std::unique_lock<std::mutex> lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::milliseconds(500),
            [&]{ return messages_received == expected_messages; }))
            << "Timeout waiting for messages, received " << messages_received << " of " << expected_messages;

        // Verify messages were received in order
        ASSERT_EQ(received_messages.size(), test_messages.size());
        for (size_t i = 0; i < test_messages.size(); ++i) {
            EXPECT_EQ(received_messages[i], test_messages[i])
                << "Message " << i << " does not match";
        }
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