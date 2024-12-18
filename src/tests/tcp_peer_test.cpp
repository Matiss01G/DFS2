#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/tcp_peer.hpp"
#include <thread>
#include <chrono>

using namespace dfs::network;
using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

class TCP_PeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        peer_ = std::make_unique<TCP_Peer>("test_peer");
    }

    void TearDown() override {
        if (peer_ && peer_->is_connected()) {
            peer_->disconnect();
        }
        peer_.reset();
    }

    std::unique_ptr<TCP_Peer> peer_;
};

// Test constructor and initial state
TEST_F(TCP_PeerTest, ConstructorInitialization) {
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::INITIAL);
    EXPECT_FALSE(peer_->is_connected());
    EXPECT_EQ(peer_->get_input_stream(), nullptr);
    EXPECT_EQ(peer_->get_output_stream(), nullptr);
}

// Test invalid connection attempts
TEST_F(TCP_PeerTest, InvalidConnectionAttempts) {
    // Try connecting to invalid address
    EXPECT_FALSE(peer_->connect("invalid_address", 12345));
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::ERROR);
    
    // Create new peer for second test since we can't transition from ERROR
    peer_.reset(new TCP_Peer("test_peer"));
    
    // Try connecting to unreachable port
    EXPECT_FALSE(peer_->connect("127.0.0.1", 1));
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::ERROR);
}

// Test state validation
TEST_F(TCP_PeerTest, StateValidation) {
    // Cannot disconnect from INITIAL state
    EXPECT_FALSE(peer_->disconnect());
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::INITIAL);
    
    // Cannot get streams in INITIAL state
    EXPECT_EQ(peer_->get_input_stream(), nullptr);
    EXPECT_EQ(peer_->get_output_stream(), nullptr);
}

// Test stream processor
TEST_F(TCP_PeerTest, StreamProcessorHandling) {
    bool processor_called = false;
    auto processor = [&processor_called](std::istream& stream) {
        processor_called = true;
        std::string data;
        std::getline(stream, data);
        EXPECT_FALSE(data.empty());
    };
    
    peer_->set_stream_processor(processor);
    
    // Cannot start processing in INITIAL state
    EXPECT_FALSE(peer_->start_stream_processing());
}

// Test disconnection sequence
TEST_F(TCP_PeerTest, DisconnectionSequence) {
    // Setup mock server and connect first
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12345));
    
    std::thread server_thread([&io_context]() {
        io_context.run();
    });
    
    // Connect to local server
    EXPECT_TRUE(peer_->connect("127.0.0.1", 12345));
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::CONNECTED);
    
    // Test disconnection
    EXPECT_TRUE(peer_->disconnect());
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::DISCONNECTED);
    
    // Cleanup
    io_context.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Test buffer management and cleanup
TEST_F(TCP_PeerTest, BufferManagementAndCleanup) {
    // Setup mock server
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12348));
    
    std::thread server_thread([&]() {
        boost::asio::ip::tcp::socket socket(io_context);
        acceptor.accept(socket);
        
        // Send large test data
        std::string test_data(8192, 'x');  // 8KB of data
        test_data += "\n";
        boost::asio::write(socket, boost::asio::buffer(test_data));
        
        io_context.stop();
    });
    
    // Connect to mock server
    EXPECT_TRUE(peer_->connect("127.0.0.1", 12348));
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::CONNECTED);
    
    // Start stream processing
    bool data_received = false;
    peer_->set_stream_processor([&data_received](std::istream& stream) {
        std::string data;
        std::getline(stream, data);
        data_received = true;
        EXPECT_EQ(data.length(), 8192);  // Verify full data received
    });
    
    EXPECT_TRUE(peer_->start_stream_processing());
    
    // Wait for data processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify data was processed
    EXPECT_TRUE(data_received);
    
    // Cleanup
    peer_->disconnect();
    io_context.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Test multiple connect/disconnect cycles
TEST_F(TCP_PeerTest, MultipleConnectionCycles) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12349));
    
    std::thread server_thread([&]() {
        while (!io_context.stopped()) {
            try {
                boost::asio::ip::tcp::socket socket(io_context);
                acceptor.accept(socket);
            } catch (...) {
                break;
            }
        }
    });
    
    // Perform multiple connect/disconnect cycles
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(peer_->connect("127.0.0.1", 12349));
        EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::CONNECTED);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        EXPECT_TRUE(peer_->disconnect());
        EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::DISCONNECTED);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Cleanup
    io_context.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Test stream processor error handling
TEST_F(TCP_PeerTest, StreamProcessorErrorHandling) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12350));
    
    std::thread server_thread([&]() {
        boost::asio::ip::tcp::socket socket(io_context);
        acceptor.accept(socket);
        
        // Send malformed data
        std::string bad_data = "malformed\xFF\xFE\n";
        boost::asio::write(socket, boost::asio::buffer(bad_data));
        
        io_context.stop();
    });
    
    // Connect to mock server
    EXPECT_TRUE(peer_->connect("127.0.0.1", 12350));
    
    // Set up processor that throws an exception
    bool exception_caught = false;
    peer_->set_stream_processor([&exception_caught](std::istream& stream) {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_TRUE(peer_->start_stream_processing());
    
    // Wait for error processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Cleanup
    peer_->disconnect();
    io_context.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Test stream operations
TEST_F(TCP_PeerTest, StreamOperations) {
    // Setup mock server
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 12346));
    
    std::thread server_thread([&]() {
        boost::asio::ip::tcp::socket socket(io_context);
        acceptor.accept(socket);
        
        // Send test data
        std::string test_data = "test_message\n";
        boost::asio::write(socket, boost::asio::buffer(test_data));
        
        // Wait for response
        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, '\n');
        
        io_context.stop();
    });
    
    // Connect to mock server
    EXPECT_TRUE(peer_->connect("127.0.0.1", 12346));
    EXPECT_EQ(peer_->get_connection_state(), ConnectionState::State::CONNECTED);
    
    // Setup stream processor
    std::string received_data;
    peer_->set_stream_processor([&received_data](std::istream& stream) {
        std::getline(stream, received_data);
    });
    
    // Start processing
    EXPECT_TRUE(peer_->start_stream_processing());
    
    // Write test data
    auto output_stream = peer_->get_output_stream();
    ASSERT_NE(output_stream, nullptr);
    *output_stream << "response_message\n";
    output_stream->flush();
    
    // Wait for data processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify received data
    EXPECT_EQ(received_data, "test_message");
    
    // Cleanup
    peer_->disconnect();
    io_context.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
