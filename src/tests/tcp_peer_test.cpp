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
                
                // Echo server implementation
                boost::asio::streambuf buffer;
                while (socket.is_open()) {
                    boost::system::error_code ec;
                    
                    // Read until newline
                    std::size_t n = boost::asio::read_until(socket, buffer, '\n', ec);
                    if (ec) {
                        if (ec != boost::asio::error::eof) {
                            BOOST_LOG_TRIVIAL(error) << "Echo server read error: " << ec.message();
                        }
                        break;
                    }
                    
                    if (n > 0) {
                        // Get the line from the buffer
                        std::string line(
                            boost::asio::buffers_begin(buffer.data()),
                            boost::asio::buffers_begin(buffer.data()) + n);
                        buffer.consume(n);  // Remove consumed data
                        
                        BOOST_LOG_TRIVIAL(debug) << "Echo server received: " << line;
                        
                        // Echo it back
                        boost::asio::write(socket, boost::asio::buffer(line), ec);
                        if (ec) {
                            BOOST_LOG_TRIVIAL(error) << "Echo server write error: " << ec.message();
                            break;
                        }
                    }
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
    const std::string test_data = "Hello,TCP!\n";  // Add newline as frame delimiter
    
    // Write data
    std::ostream* out = peer.get_output_stream();
    ASSERT_NE(out, nullptr);
    *out << test_data;  // Use operator<< for proper stream handling
    out->flush();
    
    // Read response with timeout
    std::istream* in = peer.get_input_stream();
    ASSERT_NE(in, nullptr);
    
    auto start = std::chrono::steady_clock::now();
    std::string received_data;
    std::string line;
    
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
        if (std::getline(*in, line)) {
            received_data = line + '\n';  // Re-add the newline that getline removes
            break;
        }
        
        if (in->fail() && !in->eof()) {
            in->clear();  // Clear error flags if not EOF
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(received_data, test_data) << "Data mismatch or timeout occurred";
    
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
    peer.set_stream_processor([&received_data](std::istream& stream) {
        std::string data;
        stream >> data;
        received_data = data;
    });
    
    // Test processing start/stop
    EXPECT_TRUE(peer.start_stream_processing());
    
    const std::string test_data = "ProcessThis\n";  // Add newline as frame delimiter
    std::ostream* out = peer.get_output_stream();
    ASSERT_NE(out, nullptr);
    *out << test_data;
    out->flush();
    
    // Wait for processor to receive data with timeout
    auto start = std::chrono::steady_clock::now();
    while (received_data.empty() && 
           std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Remove newline from received data for comparison
    if (!received_data.empty() && received_data.back() == '\n') {
        received_data.pop_back();
    }
    if (test_data.back() == '\n') {
        test_data.pop_back();
    }
    
    EXPECT_EQ(received_data, test_data) << "Stream processor did not receive expected data";
    
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
