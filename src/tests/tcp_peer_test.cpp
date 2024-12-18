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
    uint16_t test_port = 12345;
    
    static const int MAX_RETRIES = 5;
    static const int RETRY_DELAY_MS = 100;

    void SetUp() override {
        // Set up a test server with retries
        boost::system::error_code ec;
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(io_context_);
        
        int retries = 0;
        bool success = false;
        
        while (!success && retries < MAX_RETRIES) {
            ec.clear();
            acceptor_->open(boost::asio::ip::tcp::v4(), ec);
            
            if (!ec) {
                // Enable address reuse and other socket options
                acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
                acceptor_->set_option(boost::asio::socket_base::linger(true, 0), ec);
                
                if (!ec) {
                    acceptor_->bind(boost::asio::ip::tcp::endpoint(
                        boost::asio::ip::tcp::v4(), test_port + retries), ec);
                    
                    if (!ec) {
                        acceptor_->listen(boost::asio::socket_base::max_listen_connections, ec);
                        if (!ec) {
                            test_port = test_port + retries;  // Update the port if we bound to a different one
                            success = true;
                            break;
                        }
                    }
                }
            }
            
            if (!success) {
                acceptor_->close(ec);
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                retries++;
            }
        }
        
        if (!success) {
            FAIL() << "Failed to set up test server after " << MAX_RETRIES 
                  << " retries. Last error: " << ec.message();
        }
    }

    void TearDown() override {
        if (acceptor_) {
            boost::system::error_code ec;
            
            // Cancel any pending operations
            acceptor_->cancel(ec);
            
            // Ensure the acceptor is closed properly
            if (acceptor_->is_open()) {
                acceptor_->close(ec);
            }
            
            // Reset the acceptor
            acceptor_.reset();
        }
        
        // Stop and reset io_context
        io_context_.stop();
        io_context_.reset();
        
        // Allow some time for socket cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
    }

    // Helper to run test server in background
    void run_test_server() {
        std::thread([this]() {
            try {
                boost::asio::ip::tcp::socket socket(io_context_);
                acceptor_->accept(socket);
                
                // Configure socket for optimal performance
                socket.set_option(boost::asio::ip::tcp::no_delay(true));
                socket.set_option(boost::asio::socket_base::keep_alive(true));
                socket.set_option(boost::asio::socket_base::send_buffer_size(8192));
                socket.set_option(boost::asio::socket_base::receive_buffer_size(8192));
                
                // Echo server implementation
                boost::asio::streambuf buffer;
                std::string response;
                
                while (socket.is_open()) {
                    boost::system::error_code ec;
                    
                    // Read until newline with proper error handling
                    std::size_t n = boost::asio::read_until(socket, buffer, '\n', ec);
                    
                    if (!ec && n > 0) {
                        // Extract complete line with newline
                        std::istream is(&buffer);
                        std::getline(is, response);
                        response += '\n';  // Add back the newline
                        
                        BOOST_LOG_TRIVIAL(debug) << "Echo server received: " << response;
                        
                        // Echo back with guaranteed delivery
                        std::size_t bytes_written = boost::asio::write(
                            socket, 
                            boost::asio::buffer(response),
                            boost::asio::transfer_exactly(response.length()),
                            ec
                        );
                        
                        if (!ec && bytes_written == response.length()) {
                            socket.lowest_layer().flush(ec);
                            if (!ec) {
                                BOOST_LOG_TRIVIAL(debug) << "Echo server sent response: " << response;
                            } else {
                                BOOST_LOG_TRIVIAL(error) << "Echo server flush error: " << ec.message();
                                break;
                            }
                        } else {
                            BOOST_LOG_TRIVIAL(error) << "Echo server write error: " << ec.message();
                            break;
                        }
                    } else if (ec == boost::asio::error::eof) {
                        BOOST_LOG_TRIVIAL(debug) << "Echo server: Client disconnected";
                        break;
                    } else if (ec) {
                        BOOST_LOG_TRIVIAL(error) << "Echo server read error: " << ec.message();
                        break;
                    }
                }
            }
            catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Test server error: " << e.what();
            }
        }).detach();
        
        // Allow time for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    
    // Test data transfer with multiple attempts
    const std::string test_data = "Hello,TCP!\n";  // Add newline as frame delimiter
    const int MAX_RETRIES = 3;
    
    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        // Write data
        std::ostream* out = peer.get_output_stream();
        ASSERT_NE(out, nullptr);
        BOOST_LOG_TRIVIAL(debug) << "Sending data: " << test_data;
        *out << test_data << std::flush;
        
        // Read response with timeout
        std::istream* in = peer.get_input_stream();
        ASSERT_NE(in, nullptr);
        
        auto start = std::chrono::steady_clock::now();
        std::string received_data;
        std::string line;
        bool received = false;
        
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
            if (std::getline(*in, line)) {
                received_data = line + '\n';
                received = true;
                BOOST_LOG_TRIVIAL(debug) << "Received data: " << received_data;
                break;
            }
            
            if (in->fail() && !in->eof()) {
                in->clear();  // Clear error flags if not EOF
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (received && received_data == test_data) {
            SUCCEED() << "Data transfer successful on attempt " << (retry + 1);
            return;
        }
        
        BOOST_LOG_TRIVIAL(warning) << "Data transfer attempt " << (retry + 1) 
                                  << " failed. Expected: " << test_data 
                                  << ", Received: " << received_data;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    FAIL() << "Data transfer failed after " << MAX_RETRIES << " attempts";
    
    // Cleanup
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
    std::mutex data_mutex;
    peer.set_stream_processor([&received_data, &data_mutex](std::istream& stream) {
        std::string line;
        if (std::getline(stream, line)) {
            std::lock_guard<std::mutex> lock(data_mutex);
            received_data = line;
        }
    });
    
    // Test processing start/stop
    EXPECT_TRUE(peer.start_stream_processing());
    
    const std::string test_input = "ProcessThis\n";  // Add newline as frame delimiter
    std::ostream* out = peer.get_output_stream();
    ASSERT_NE(out, nullptr);
    *out << test_input << std::flush;  // Make sure to flush
    
    // Wait for processor to receive data with timeout
    auto start = std::chrono::steady_clock::now();
    std::string received_copy;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            received_copy = received_data;
        }
        if (!received_copy.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Prepare test data for comparison (remove newlines)
    std::string expected_data = test_input;
    if (!expected_data.empty() && expected_data.back() == '\n') {
        expected_data.pop_back();
    }
    
    EXPECT_EQ(received_data, expected_data) << "Stream processor did not receive expected data";
    
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

// Main function is defined in network_test.cpp
