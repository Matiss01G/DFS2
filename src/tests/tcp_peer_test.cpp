#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <array>
#include <queue>
#include "network/tcp_peer.hpp"

using namespace dfs::network;

class MockServer {
public:
    explicit MockServer() 
        : acceptor_(io_context_), port_(0) {
        start();
    }

    void start() {
        boost::system::error_code ec;
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::tcp::v4(), port_);
        
        acceptor_.open(endpoint.protocol(), ec);
        if (!ec) {
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint, ec);
            if (!ec) {
                acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
                // Get the actual port number assigned by the system
                endpoint = acceptor_.local_endpoint(ec);
                port_ = endpoint.port();
                BOOST_LOG_TRIVIAL(info) << "Mock server listening on port " << port_;
            }
        }
        
        if (ec) {
            BOOST_LOG_TRIVIAL(error) << "Server setup failed: " << ec.message();
            return;
        }

        run_thread_ = std::thread([this]() { accept_connections(); });
    }

    void stop() {
        try {
            stopping_ = true;
            
            // First cancel any pending operations
            if (acceptor_.is_open()) {
                boost::system::error_code ec;
                acceptor_.cancel(ec);
                if (ec) {
                    BOOST_LOG_TRIVIAL(error) << "Acceptor cancel error: " << ec.message();
                }
            }
            
            // Stop IO context to interrupt operations
            io_context_.stop();
            
            // Wait for processing thread
            if (run_thread_.joinable()) {
                run_thread_.join();
            }
            
            // Clean up sockets
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                for (auto& socket : active_sockets_) {
                    if (socket && socket->is_open()) {
                        try {
                            boost::system::error_code ec;
                            
                            socket->cancel(ec);
                            if (ec && ec != boost::asio::error::not_connected) {
                                BOOST_LOG_TRIVIAL(error) << "Socket cancel error: " << ec.message();
                            }
                            
                            socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                            if (ec && ec != boost::asio::error::not_connected) {
                                BOOST_LOG_TRIVIAL(error) << "Socket shutdown error: " << ec.message();
                            }
                            
                            socket->close(ec);
                            if (ec) {
                                BOOST_LOG_TRIVIAL(error) << "Socket close error: " << ec.message();
                            }
                        } catch (const std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "Socket cleanup error: " << e.what();
                        }
                    }
                }
                active_sockets_.clear();
            }
            
            // Close acceptor
            if (acceptor_.is_open()) {
                boost::system::error_code ec;
                acceptor_.close(ec);
                if (ec) {
                    BOOST_LOG_TRIVIAL(error) << "Acceptor close error: " << ec.message();
                }
            }
            
            // Reset IO context
            io_context_.reset();
            
            BOOST_LOG_TRIVIAL(debug) << "Mock server stopped cleanly";
            
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Server stop error: " << e.what();
            throw; // Re-throw to notify test framework
        }
    }

    ~MockServer() {
        stop();
    }

    uint16_t get_port() const { return port_; }

private:
    void accept_connections() {
        while (!stopping_) {
            try {
                auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
                boost::system::error_code ec;
                
                acceptor_.accept(*socket, ec);
                if (ec) {
                    if (stopping_) break;
                    BOOST_LOG_TRIVIAL(error) << "Accept error: " << ec.message();
                    continue;
                }
                
                {
                    std::lock_guard<std::mutex> lock(socket_mutex_);
                    active_sockets_.push_back(socket);
                }
                
                std::thread([this, socket]() {
                    handle_connection(socket);
                }).detach();
            } catch (const std::exception& e) {
                if (stopping_) break;
                BOOST_LOG_TRIVIAL(error) << "Accept thread error: " << e.what();
            }
        }
    }

    void handle_connection(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
        try {
            boost::asio::streambuf buffer;
            
            while (socket->is_open() && !stopping_) {
                try {
                    boost::system::error_code ec;
                    
                    // Check if data is available
                    if (socket->available(ec) > 0 || !ec) {
                        try {
                            // Configure socket for optimal performance
                            socket->set_option(boost::asio::ip::tcp::no_delay(true));
                            socket->set_option(boost::asio::socket_base::keep_alive(true));
                            socket->set_option(boost::asio::socket_base::receive_buffer_size(8192));
                            socket->set_option(boost::asio::socket_base::send_buffer_size(8192));
                            socket->set_option(boost::asio::socket_base::reuse_address(true));
                            socket->set_option(boost::asio::socket_base::linger(true, 0));
                            
                            // Read until newline
                            std::size_t n = boost::asio::read_until(
                                *socket,
                                buffer,
                                '\n',
                                ec
                            );

                            if (!ec && n > 0) {
                                // Process complete message
                                std::string message;
                                {
                                    std::istream is(&buffer);
                                    std::getline(is, message);
                                    message += '\n';
                                }
                                buffer.consume(n);

                                BOOST_LOG_TRIVIAL(debug) << "Server received: '" 
                                    << message.substr(0, message.length()-1) << "'";
                                
                                // Echo back immediately with detailed error handling
                                try {
                                    std::size_t bytes_written = boost::asio::write(
                                        *socket,
                                        boost::asio::buffer(message),
                                        boost::asio::transfer_exactly(message.length()),
                                        ec
                                    );
                                    
                                    if (!ec && bytes_written == message.length()) {
                                        BOOST_LOG_TRIVIAL(debug) << "Server echoed: '" 
                                            << message.substr(0, message.length()-1) << "'";
                                    } else {
                                        BOOST_LOG_TRIVIAL(error) << "Write error: " 
                                            << (ec ? ec.message() : "Incomplete write");
                                        break;
                                    }
                                } catch (const std::exception& e) {
                                    BOOST_LOG_TRIVIAL(error) << "Write operation error: " << e.what();
                                    break;
                                }
                            } else if (ec == boost::asio::error::operation_aborted ||
                                     ec == boost::asio::error::try_again ||
                                     ec == boost::asio::error::timed_out) {
                                // Expected errors, continue
                                continue;
                            } else if (ec == boost::asio::error::eof || 
                                     ec == boost::asio::error::connection_reset) {
                                // Client disconnected
                                BOOST_LOG_TRIVIAL(debug) << "Client disconnected";
                                break;
                            } else {
                                BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                                break;
                            }
                        } catch (const std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "Stream processing error: " << e.what();
                            break;
                        }
                    }
                    
                    // Small delay to prevent busy-waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    
                } catch (const std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "Socket operation error: " << e.what();
                    break;
                }
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Connection handler error: " << e.what();
        }
        
        try {
            if (socket && socket->is_open()) {
                boost::system::error_code ec;
                socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                if (ec && ec != boost::asio::error::not_connected) {
                    BOOST_LOG_TRIVIAL(error) << "Socket shutdown error: " << ec.message();
                }
                
                socket->close(ec);
                if (ec) {
                    BOOST_LOG_TRIVIAL(error) << "Socket close error: " << ec.message();
                }
            }
            
            std::lock_guard<std::mutex> lock(socket_mutex_);
            active_sockets_.erase(
                std::remove(active_sockets_.begin(), active_sockets_.end(), socket),
                active_sockets_.end()
            );
            
            BOOST_LOG_TRIVIAL(debug) << "Connection handler cleaned up";
            
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Connection cleanup error: " << e.what();
        }
    }

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    uint16_t port_;
    std::thread run_thread_;
    std::mutex socket_mutex_;
    std::vector<std::shared_ptr<boost::asio::ip::tcp::socket>> active_sockets_;
    std::atomic<bool> stopping_{false};
};

class TCP_PeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        try {
            server_ = std::make_unique<MockServer>();
            
            // Wait for server to be ready
            bool server_ready = wait_for_server();
            ASSERT_TRUE(server_ready) << "Failed to start mock server";
            
            BOOST_LOG_TRIVIAL(info) << "Test setup complete, server ready on port " 
                << server_->get_port();
                
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Test setup error: " << e.what();
            throw; // Re-throw to fail the test
        }
    }

    void TearDown() override {
        try {
            if (server_) {
                server_->stop();
                server_.reset();
                
                // Brief pause to ensure cleanup is complete
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                BOOST_LOG_TRIVIAL(info) << "Test teardown complete";
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Test teardown error: " << e.what();
            throw; // Re-throw to fail the test
        }
    }

    std::unique_ptr<MockServer> server_;
    
    // Helper to wait for server readiness with shorter timeout and better error reporting
    bool wait_for_server(std::chrono::milliseconds timeout = std::chrono::milliseconds(50)) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            try {
                if (server_ && server_->get_port() != 0) {
                    return true;
                }
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Server check error: " << e.what();
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        BOOST_LOG_TRIVIAL(error) << "Server readiness timeout after " << timeout.count() << "ms";
        return false;
    }
};

TEST_F(TCP_PeerTest, BasicConnectivity) {
    TCP_Peer peer("test_peer");
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::INITIAL);
    
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    
    ASSERT_TRUE(peer.disconnect());
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
}

TEST_F(TCP_PeerTest, StreamOperations) {
    ASSERT_TRUE(wait_for_server()) << "Server failed to start and bind to port";
    
    TCP_Peer peer("stream_test_peer");
    
    // Test stream unavailability before connection
    EXPECT_EQ(peer.get_input_stream(), nullptr) << "Input stream should be null before connection";
    EXPECT_EQ(peer.get_output_stream(), nullptr) << "Output stream should be null before connection";
    
    // Connect and verify stream availability
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    EXPECT_NE(peer.get_input_stream(), nullptr) << "Input stream should be available after connection";
    EXPECT_NE(peer.get_output_stream(), nullptr) << "Output stream should be available after connection";
    
    // Test stream processor configuration
    std::string received_data;
    std::mutex data_mutex;
    std::condition_variable data_cv;
    bool data_received = false;
    
    peer.set_stream_processor([&](std::istream& stream) {
        try {
            std::string data;
            if (std::getline(stream, data)) {
                BOOST_LOG_TRIVIAL(debug) << "Stream processor received data: '" << data << "'";
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    received_data = data + '\n';
                    data_received = true;
                }
                data_cv.notify_one();
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
        }
    });
    
    // Start stream processing and verify
    ASSERT_TRUE(peer.start_stream_processing()) << "Failed to start stream processing";
    
    // Test data transmission
    const std::string test_data = "Hello,TCP!\n";
    auto* output_stream = peer.get_output_stream();
    ASSERT_NE(output_stream, nullptr) << "Failed to get output stream";
    
    BOOST_LOG_TRIVIAL(debug) << "Sending test data: '" << test_data << "'";
    *output_stream << test_data << std::flush;
    
    // Wait for data with timeout
    BOOST_LOG_TRIVIAL(debug) << "Waiting for data to be received...";
    {
        std::unique_lock<std::mutex> lock(data_mutex);
        ASSERT_TRUE(data_cv.wait_for(lock, std::chrono::milliseconds(100), [&]() {
            return data_received && received_data == test_data;
        })) << "Data reception timeout or mismatch:\n"
            << "Expected: '" << test_data << "'\n"
            << "Received: '" << (data_received ? received_data : "[nothing]") << "'";
    }
    
    BOOST_LOG_TRIVIAL(info) << "Data matched successfully";
    
    // Stop processing and disconnect
    peer.stop_stream_processing();
    peer.disconnect();
}

TEST_F(TCP_PeerTest, BasicMessageExchange) {
    ASSERT_TRUE(wait_for_server()) << "Server failed to start and bind to port";
    
    TCP_Peer peer("exchange_test");
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    
    std::string received_data;
    std::mutex data_mutex;
    std::condition_variable data_cv;
    bool data_received = false;
    
    peer.set_stream_processor([&](std::istream& stream) {
        try {
            std::string data;
            if (std::getline(stream, data)) {
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    received_data = data + '\n';
                    data_received = true;
                    BOOST_LOG_TRIVIAL(debug) << "Stream processor received: '" << data << "'";
                }
                data_cv.notify_one();
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
        }
    });
    
    ASSERT_TRUE(peer.start_stream_processing());
    
    const std::string test_message = "Test Message\n";
    auto* output_stream = peer.get_output_stream();
    ASSERT_NE(output_stream, nullptr);
    
    *output_stream << test_message << std::flush;
    
    bool received = false;
    {
        std::unique_lock<std::mutex> lock(data_mutex);
        received = data_cv.wait_for(lock, std::chrono::milliseconds(50), [&]() {
            return data_received && received_data == test_message;
        });
        
        if (!received) {
            BOOST_LOG_TRIVIAL(error) << "Failed to receive message:"
                << "\nExpected: '" << test_message << "'"
                << "\nReceived: '" << (data_received ? received_data : "[nothing]") << "'";
        }
    }
    
    ASSERT_TRUE(received);
    
    peer.stop_stream_processing();
    peer.disconnect();
}

TEST_F(TCP_PeerTest, ConnectionLifecycle) {
    ASSERT_TRUE(wait_for_server()) << "Server failed to start and bind to port";
    
    TCP_Peer peer("test_peer");
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::INITIAL);
    
    // Test successful connection
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    
    // Test stream availability
    EXPECT_NE(peer.get_input_stream(), nullptr);
    EXPECT_NE(peer.get_output_stream(), nullptr);
    
    // Test graceful disconnection
    ASSERT_TRUE(peer.disconnect());
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::DISCONNECTED);
}

TEST_F(TCP_PeerTest, ConnectionErrorHandling) {
    TCP_Peer peer("error_test_peer");
    
    // Test connection to invalid port
    EXPECT_FALSE(peer.connect("127.0.0.1", 54321));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
    
    // Test invalid state transitions after error
    EXPECT_FALSE(peer.connect("127.0.0.1", server_->get_port()));
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::ERROR);
    
    // Test stream unavailability in error state
    EXPECT_EQ(peer.get_input_stream(), nullptr);
    EXPECT_EQ(peer.get_output_stream(), nullptr);
    
    // Test disconnect from error state
    EXPECT_FALSE(peer.disconnect());
}

TEST_F(TCP_PeerTest, UnexpectedDisconnection) {
    ASSERT_TRUE(wait_for_server()) << "Server failed to start and bind to port";
    
    TCP_Peer peer("disconnect_test");
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    
    std::string received_data;
    std::mutex data_mutex;
    std::condition_variable data_cv;
    bool data_received = false;
    
    peer.set_stream_processor([&](std::istream& stream) {
        try {
            std::string data;
            if (std::getline(stream, data)) {
                std::lock_guard<std::mutex> lock(data_mutex);
                received_data = data + '\n';
                data_received = true;
                data_cv.notify_one();
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
        }
    });
    
    ASSERT_TRUE(peer.start_stream_processing());
    
    // Send initial message
    const std::string test_message = "Before disconnect\n";
    auto* output_stream = peer.get_output_stream();
    ASSERT_NE(output_stream, nullptr);
    *output_stream << test_message << std::flush;
    
    // Wait for echo
    {
        std::unique_lock<std::mutex> lock(data_mutex);
        ASSERT_TRUE(data_cv.wait_for(lock, std::chrono::milliseconds(100),
            [&]() { return data_received && received_data == test_message; }))
            << "Failed to receive initial message";
    }
    
    // Stop the server to simulate unexpected disconnection
    server_->stop();
    
    // Try to send message after server stopped
    data_received = false;
    const std::string post_disconnect_message = "After disconnect\n";
    *output_stream << post_disconnect_message << std::flush;
    
    // Verify no message received
    {
        std::unique_lock<std::mutex> lock(data_mutex);
        EXPECT_FALSE(data_cv.wait_for(lock, std::chrono::milliseconds(100),
            [&]() { return data_received; }))
            << "Unexpectedly received data after server stop";
    }
    
    peer.stop_stream_processing();
    peer.disconnect();
}

TEST_F(TCP_PeerTest, MultipleMessages) {
    ASSERT_TRUE(wait_for_server()) << "Server failed to start and bind to port";
    
    TCP_Peer peer("multiple_test");
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    
    std::vector<std::string> received_messages;
    std::queue<std::string> message_queue;
    std::mutex messages_mutex;
    std::condition_variable messages_cv;
    std::atomic<bool> processing_error{false};
    std::string error_message;
    
    const size_t message_count = 5;
    std::atomic<size_t> processed_count{0};
    
    peer.set_stream_processor([&](std::istream& stream) {
        try {
            std::string data;
            if (std::getline(stream, data)) {
                std::lock_guard<std::mutex> lock(messages_mutex);
                received_messages.push_back(data + '\n');
                processed_count++;
                BOOST_LOG_TRIVIAL(debug) << "Received message " << processed_count 
                                       << "/" << message_count << ": '" << data << "'";
                messages_cv.notify_one();
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(messages_mutex);
            processing_error = true;
            error_message = e.what();
            BOOST_LOG_TRIVIAL(error) << "Stream processor error: " << e.what();
            messages_cv.notify_one();
        }
    });
    
    ASSERT_TRUE(peer.start_stream_processing());
    
    auto* output_stream = peer.get_output_stream();
    ASSERT_NE(output_stream, nullptr) << "Failed to get output stream";
    
    std::vector<std::string> test_messages;
    for (size_t i = 0; i < message_count; ++i) {
        std::string msg = "Message " + std::to_string(i) + "\n";
        test_messages.push_back(msg);
        message_queue.push(msg);
        BOOST_LOG_TRIVIAL(debug) << "Queued message " << (i+1) << "/" << message_count 
                                << ": '" << msg.substr(0, msg.length()-1) << "'";
    }
    
    // Send messages with better error handling and progress tracking
    size_t sent_count = 0;
    while (!message_queue.empty() && !processing_error) {
        try {
            const std::string& msg = message_queue.front();
            BOOST_LOG_TRIVIAL(debug) << "Sending message " << (sent_count+1) 
                                   << "/" << message_count << ": '" 
                                   << msg.substr(0, msg.length()-1) << "'";
            
            *output_stream << msg << std::flush;
            message_queue.pop();
            sent_count++;
            
            // Brief pause between messages to prevent overwhelming
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error sending message: " << e.what();
            FAIL() << "Failed to send message: " << e.what();
        }
    }
    
    ASSERT_FALSE(processing_error) << "Stream processor error: " << error_message;
    
    // Wait for all messages with progress updates
    bool received_all = false;
    auto start_time = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(500);
    
    while (!received_all && std::chrono::steady_clock::now() - start_time < timeout) {
        std::unique_lock<std::mutex> lock(messages_mutex);
        received_all = messages_cv.wait_for(lock, std::chrono::milliseconds(50), [&]() {
            if (processing_error) return true;
            if (received_messages.size() != test_messages.size()) {
                BOOST_LOG_TRIVIAL(debug) << "Waiting for messages... " 
                    << received_messages.size() << "/" << test_messages.size();
                return false;
            }
            return true;
        });
        
        if (processing_error) break;
    }
    
    ASSERT_FALSE(processing_error) << "Stream processor error: " << error_message;
    
    ASSERT_TRUE(received_all) << "Timeout waiting for messages. Received " 
        << received_messages.size() << "/" << message_count << " messages";
    
    // Verify message contents
    {
        std::lock_guard<std::mutex> lock(messages_mutex);
        ASSERT_EQ(received_messages.size(), test_messages.size()) 
            << "Message count mismatch";
        
        for (size_t i = 0; i < test_messages.size(); ++i) {
            EXPECT_EQ(received_messages[i], test_messages[i]) 
                << "Message " << i << " content mismatch:\n"
                << "Expected: '" << test_messages[i] << "'\n"
                << "Received: '" << received_messages[i] << "'";
        }
    }
    
    peer.stop_stream_processing();
    peer.disconnect();
}