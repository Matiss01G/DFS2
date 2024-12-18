#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include "network/tcp_peer.hpp"

using namespace dfs::network;
using boost::asio::ip::tcp;

class TcpPeerTest : public ::testing::Test {
protected:
    std::condition_variable server_cv_;
    std::mutex server_mutex_;
    std::atomic<bool> server_ready_{false};
    
    void SetUp() override {
        server_running_ = true;
        server_ready_ = false;
        
        // Start test server in a separate thread
        server_thread_ = std::thread([this]() {
            try {
                boost::asio::io_context io_context;
                tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), test_port_));
                
                // Signal that server is ready to accept connections
                server_ready_ = true;
                server_cv_.notify_one();
                
                while (server_running_) {
                    // Accept connections with timeout
                    boost::system::error_code accept_ec;
                    boost::asio::socket_base::keep_alive option(true);
                    acceptor.accept(server_socket_, accept_ec);
                    
                    if (accept_ec) {
                        if (accept_ec != boost::asio::error::operation_aborted) {
                            BOOST_LOG_TRIVIAL(error) << "Accept error: " << accept_ec.message();
                        }
                        continue;
                    }
                    
                    // Configure accepted socket
                    server_socket_.set_option(option);
                    
                    // Echo received data back
                    std::vector<char> buffer(test_buffer_size);
                    while (server_running_ && server_socket_.is_open()) {
                        boost::system::error_code ec;
                        size_t len = server_socket_.read_some(boost::asio::buffer(buffer), ec);
                        if (ec) {
                            if (ec != boost::asio::error::operation_aborted) {
                                BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                            }
                            break;
                        }
                        
                        boost::asio::write(server_socket_, boost::asio::buffer(buffer, len), ec);
                        if (ec) {
                            BOOST_LOG_TRIVIAL(error) << "Write error: " << ec.message();
                            break;
                        }
                    }
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Test server error: " << e.what();
            }
        });
        
        // Wait for server to be ready with timeout
        {
            std::unique_lock<std::mutex> lock(server_mutex_);
            if (!server_cv_.wait_for(lock, std::chrono::seconds(5), [this] { return server_ready_; })) {
                FAIL() << "Test server failed to start within timeout";
            }
        }
        
        // Additional setup delay to ensure server is fully operational
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void TearDown() override {
        server_running_ = false;
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
    
    // Test stream availability
    EXPECT_NE(peer.get_input_stream(), nullptr);
    EXPECT_NE(peer.get_output_stream(), nullptr);
    
    // Test disconnection
    EXPECT_TRUE(peer.disconnect());
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
}

// Test comprehensive stream operations
TEST_F(TcpPeerTest, StreamOperations) {
    TcpPeer peer;
    ASSERT_TRUE(peer.connect("127.0.0.1", test_port_));
    
    // Test basic stream availability
    auto* out_stream = peer.get_output_stream();
    auto* in_stream = peer.get_input_stream();
    ASSERT_NE(out_stream, nullptr);
    ASSERT_NE(in_stream, nullptr);
    
    // Test stream state before processing
    EXPECT_TRUE(out_stream->good());
    EXPECT_TRUE(in_stream->good());
    
    // Test writing multiple chunks with varying sizes
    const std::vector<std::string> test_chunks = {
        "Small chunk",
        std::string(1024, 'A'),  // 1KB chunk
        "Another small chunk",
        std::string(4096, 'B')   // 4KB chunk
    };
    
    std::string expected_data;
    for (const auto& chunk : test_chunks) {
        *out_stream << chunk << std::endl;
        expected_data += chunk + "\n";
    }
    out_stream->flush();
    
    // Test concurrent read/write with stream processor
    std::string received_data;
    std::atomic<bool> processing_complete{false};
    
    peer.set_stream_processor([&](std::istream& stream) {
        std::string line;
        while (std::getline(stream, line)) {
            received_data += line + "\n";
            // Simulate processing delay
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        processing_complete = true;
    });
    
    // Start processing and continue writing
    EXPECT_TRUE(peer.start_stream_processing());
    
    // Write additional data while processing
    const std::string additional_data = "Concurrent write test\n";
    *out_stream << additional_data;
    out_stream->flush();
    expected_data += additional_data;
    
    // Wait for processing completion with timeout
    auto start_time = std::chrono::steady_clock::now();
    while (!processing_complete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(2)) {
            FAIL() << "Stream processing timeout";
            break;
        }
    }
    
    // Verify all data was received correctly
    EXPECT_EQ(received_data, expected_data);
    
    // Test stream state after processing
    EXPECT_TRUE(out_stream->good());
    EXPECT_TRUE(peer.get_input_stream()->good());
    
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

// Test large data transfer
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

// Added test for connection cycles beyond the initial 3.
TEST_F(TcpPeerTest, ExtendedConnectionCycles) {
    TcpPeer peer;
    for (int i = 0; i < 10; ++i) { // Increased cycle count for more robust testing.
        EXPECT_TRUE(peer.connect("127.0.0.1", test_port_)) << "Connection failed on cycle " << i;
        EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED) << "Wrong state on cycle " << i;
        EXPECT_TRUE(peer.disconnect()) << "Disconnection failed on cycle " << i;
        EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED) << "Wrong state after disconnect on cycle " << i;
    }
}

// Added test for connection errors beyond the initial connection error.
TEST_F(TcpPeerTest, AdditionalConnectionErrors) {
    TcpPeer peer;
    for (int i = 0; i < 5; ++i) { // Attempt multiple connections to invalid ports.
        int invalidPort = 60000 + i; // Generate a range of invalid ports.
        EXPECT_FALSE(peer.connect("127.0.0.1", invalidPort)) << "Unexpected connection success on port " << invalidPort;
        EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR) << "Wrong state after failed connection on port " << invalidPort;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); //Allow time for state transition.
        EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED) << "Failed to transition to DISCONNECTED state after failed connection on port " << invalidPort;
    }
}