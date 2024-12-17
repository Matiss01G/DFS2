#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "network/tcp_peer.hpp"

using namespace dfs::network;
using boost::asio::ip::tcp;

class TcpPeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_running_ = true;
        // Start test server in a separate thread
        server_thread_ = std::thread([this]() {
            try {
                boost::asio::io_context io_context;
                tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), test_port_));
                
                while (server_running_) {
                    // Accept connections
                    boost::system::error_code accept_ec;
                    acceptor.accept(server_socket_, accept_ec);
                    if (accept_ec) {
                        continue;
                    }
                    
                    // Echo received data back
                    std::vector<char> buffer(test_buffer_size);
                    while (server_running_ && server_socket_.is_open()) {
                        boost::system::error_code ec;
                        size_t len = server_socket_.read_some(boost::asio::buffer(buffer), ec);
                        if (ec) {
                            break;
                        }
                        boost::asio::write(server_socket_, boost::asio::buffer(buffer, len), ec);
                        if (ec) {
                            break;
                        }
                    }
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Test server error: " << e.what();
            }
        });
        
        // Allow server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void TearDown() override {
        if (server_thread_.joinable()) {
            server_socket_.close();
            server_thread_.join();
        }
    }
    
    static constexpr uint16_t test_port_ = 12345;
    static constexpr size_t test_buffer_size = 8192;
    boost::asio::io_context server_io_context_;
    tcp::socket server_socket_{server_io_context_};
    std::thread server_thread_;
    std::atomic<bool> server_running_{false};
};

// Test basic connection functionality
TEST_F(TcpPeerTest, ConnectionLifecycle) {
    TcpPeer peer;
    
    // Test initial state
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::INITIAL);
    
    // Test successful connection
    EXPECT_TRUE(peer.connect("127.0.0.1", test_port_));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    EXPECT_TRUE(peer.is_connected());
    
    // Test stream availability
    EXPECT_NE(peer.get_input_stream(), nullptr);
    EXPECT_NE(peer.get_output_stream(), nullptr);
    
    // Test disconnection
    EXPECT_TRUE(peer.disconnect());
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
    EXPECT_FALSE(peer.is_connected());
}

// Test stream operations
TEST_F(TcpPeerTest, StreamOperations) {
    TcpPeer peer;
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port_));
    
    // Test writing to output stream
    const std::string test_data = "Hello, TCP!";
    auto* out_stream = peer.get_output_stream();
    ASSERT_NE(out_stream, nullptr);
    *out_stream << test_data << std::endl;
    out_stream->flush();
    
    // Set up stream processor to capture received data
    std::string received_data;
    peer.set_stream_processor([&received_data](std::istream& stream) {
        std::string line;
        std::getline(stream, line);
        received_data = line;
    });
    
    // Start processing and wait for data
    EXPECT_TRUE(peer.start_stream_processing());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify received data
    EXPECT_EQ(received_data, test_data);
    
    // Clean up
    peer.stop_stream_processing();
    peer.disconnect();
}

// Test connection error handling
TEST_F(TcpPeerTest, ConnectionErrors) {
    TcpPeer peer;
    
    // Test connection to non-existent server
    EXPECT_FALSE(peer.connect("127.0.0.1", 54321));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
    
    // Test automatic transition to DISCONNECTED state
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
    
    // Test reconnection after error
    EXPECT_TRUE(peer.connect("127.0.0.1", test_port_));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
}

// Test stream processing control
TEST_F(TcpPeerTest, StreamProcessingControl) {
    TcpPeer peer;
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port_));
    
    // Test starting without processor
    EXPECT_FALSE(peer.start_stream_processing());
    
    // Set processor and start
    bool processor_called = false;
    peer.set_stream_processor([&processor_called](std::istream&) {
        processor_called = true;
    });
    
    EXPECT_TRUE(peer.start_stream_processing());
    
    // Write some data to trigger processing
    auto* out = peer.get_output_stream();
    *out << "Test data" << std::endl;
    out->flush();
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop processing
    peer.stop_stream_processing();
    EXPECT_TRUE(processor_called);
    
    // Clean up
    peer.disconnect();
}

// Test multiple connect/disconnect cycles
TEST_F(TcpPeerTest, ConnectionCycles) {
    TcpPeer peer;
    
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(peer.connect("127.0.0.1", test_port_))
            << "Connection failed on cycle " << i;
        EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED)
            << "Wrong state on cycle " << i;
            
        EXPECT_TRUE(peer.disconnect())
            << "Disconnection failed on cycle " << i;
        EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED)
            << "Wrong state after disconnect on cycle " << i;
    }
}

// Test large data transfer and buffer management
TEST_F(TcpPeerTest, LargeDataTransfer) {
    TcpPeer peer;
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port_));
    
    // Generate large test data
    const size_t data_size = test_buffer_size * 2;
    std::string large_data;
    large_data.reserve(data_size);
    for (size_t i = 0; i < data_size; ++i) {
        large_data.push_back(static_cast<char>('A' + (i % 26)));
    }
    
    // Set up stream processor to capture received data
    std::string received_data;
    peer.set_stream_processor([&received_data](std::istream& stream) {
        std::string chunk;
        chunk.resize(1024);
        while (stream.read(&chunk[0], chunk.size())) {
            received_data.append(chunk);
        }
        if (stream.gcount() > 0) {
            received_data.append(chunk, 0, stream.gcount());
        }
    });
    
    // Start processing
    ASSERT_TRUE(peer.start_stream_processing());
    
    // Send large data
    auto* out_stream = peer.get_output_stream();
    ASSERT_NE(out_stream, nullptr);
    *out_stream << large_data;
    out_stream->flush();
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify received data
    EXPECT_EQ(received_data, large_data);
    
    // Clean up
    peer.stop_stream_processing();
    peer.disconnect();
}

// Test timeout handling
TEST_F(TcpPeerTest, TimeoutHandling) {
    TcpPeer peer;
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port_));
    
    // Set up stream processor that simulates slow processing
    peer.set_stream_processor([](std::istream& stream) {
        std::string data;
        stream >> data;
        std::this_thread::sleep_for(std::chrono::seconds(6));  // Longer than socket timeout
    });
    
    // Start processing
    ASSERT_TRUE(peer.start_stream_processing());
    
    // Send data
    auto* out = peer.get_output_stream();
    ASSERT_NE(out, nullptr);
    *out << "test data" << std::endl;
    out->flush();
    
    // Wait for timeout to occur
    std::this_thread::sleep_for(std::chrono::seconds(7));
    
    // Verify peer entered error state
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
    
    // Clean up
    peer.stop_stream_processing();
    peer.disconnect();
}
