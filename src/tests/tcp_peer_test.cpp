#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <array>
#include <queue>
#include <future>
#include "network/tcp_peer.hpp"

using namespace dfs::network;

class MockServer {
public:
    explicit MockServer() 
        : io_context_(),
          work_guard_(boost::asio::make_work_guard(io_context_)),
          acceptor_(io_context_),
          port_(0),
          stopping_(false) {
        start();
    }

    void start() {
        try {
            boost::system::error_code ec;
            boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::tcp::v4(), 0);
            
            acceptor_.open(endpoint.protocol(), ec);
            if (!ec) {
                acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
                acceptor_.bind(endpoint, ec);
                if (!ec) {
                    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
                    endpoint = acceptor_.local_endpoint(ec);
                    port_ = endpoint.port();
                    BOOST_LOG_TRIVIAL(info) << "Mock server listening on port " << port_;
                }
            }
            
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "Server setup failed: " << ec.message();
                return;
            }

            run_thread_ = std::thread([this]() { 
                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    BOOST_LOG_TRIVIAL(error) << "IO context error: " << e.what();
                }
                BOOST_LOG_TRIVIAL(debug) << "IO context finished";
            });

            post_accept();
            BOOST_LOG_TRIVIAL(info) << "Mock server started successfully";
        }
        catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Server start error: " << e.what();
            throw;
        }
    }

    void stop() {
        try {
            if (stopping_) return;
            stopping_ = true;
            BOOST_LOG_TRIVIAL(debug) << "Initiating server shutdown...";
            
            // Cancel pending operations
            if (acceptor_.is_open()) {
                boost::system::error_code ec;
                acceptor_.cancel(ec);
                acceptor_.close(ec);
                BOOST_LOG_TRIVIAL(debug) << "Acceptor closed";
            }

            // Clean up sockets
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                for (auto& socket : active_sockets_) {
                    if (socket && socket->is_open()) {
                        try {
                            boost::system::error_code ec;
                            socket->cancel(ec);
                            socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                            socket->close(ec);
                        } catch (const std::exception& e) {
                            BOOST_LOG_TRIVIAL(error) << "Socket cleanup error: " << e.what();
                        }
                    }
                }
                active_sockets_.clear();
                BOOST_LOG_TRIVIAL(debug) << "All sockets cleaned up";
            }

            // Stop io_context and cleanup
            work_guard_.reset();
            io_context_.stop();

            // Wait for run thread with timeout
            if (run_thread_.joinable()) {
                std::future<void> future = std::async(std::launch::async, [this]() {
                    run_thread_.join();
                });

                if (future.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout) {
                    BOOST_LOG_TRIVIAL(warning) << "IO context shutdown timeout";
                }
            }

            BOOST_LOG_TRIVIAL(debug) << "Server stopped cleanly";
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Server stop error: " << e.what();
        }
    }

    ~MockServer() {
        try {
            stop();
        } catch (...) {
            // Suppress destruction errors
        }
    }

    uint16_t get_port() const { return port_; }

private:
    void post_accept() {
        if (stopping_) return;

        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        acceptor_.async_accept(*socket,
            [this, socket](const boost::system::error_code& ec) {
                if (!ec) {
                    handle_connection(socket);
                    post_accept();  // Continue accepting
                } else if (!stopping_) {
                    BOOST_LOG_TRIVIAL(error) << "Accept error: " << ec.message();
                }
            });
    }

    void handle_connection(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
        if (stopping_) return;

        try {
            socket->set_option(boost::asio::ip::tcp::no_delay(true));
            socket->set_option(boost::asio::socket_base::keep_alive(true));
            socket->set_option(boost::asio::socket_base::receive_buffer_size(8192));
            socket->set_option(boost::asio::socket_base::send_buffer_size(8192));
            
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                active_sockets_.push_back(socket);
            }

            auto buffer = std::make_shared<boost::asio::streambuf>();
            async_read(socket, buffer);

        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Connection setup error: " << e.what();
            cleanup_socket(socket);
        }
    }

    void async_read(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                   std::shared_ptr<boost::asio::streambuf> buffer) {
        if (stopping_) return;

        boost::asio::async_read_until(*socket, *buffer, '\n',
            [this, socket, buffer](const boost::system::error_code& ec, std::size_t n) {
                if (!ec && n > 0 && !stopping_) {
                    std::string data;
                    {
                        std::istream is(buffer.get());
                        std::getline(is, data);
                    }
                    buffer->consume(n);

                    BOOST_LOG_TRIVIAL(debug) << "Server received: '" << data << "'";
                    
                    // Echo back immediately
                    data += '\n';
                    auto write_buffer = std::make_shared<std::string>(data);
                    
                    boost::asio::async_write(*socket,
                        boost::asio::buffer(*write_buffer),
                        [this, socket, write_buffer](const boost::system::error_code& ec, std::size_t n) {
                            if (!ec && n > 0) {
                                BOOST_LOG_TRIVIAL(debug) << "Server echoed: '" 
                                    << write_buffer->substr(0, write_buffer->length()-1) << "'";
                                // Only continue reading after successful write
                                auto new_buffer = std::make_shared<boost::asio::streambuf>();
                                async_read(socket, new_buffer);
                            } else if (!stopping_) {
                                BOOST_LOG_TRIVIAL(error) << "Write error: " << ec.message();
                                cleanup_socket(socket);
                            }
                        });
                }
                else if (!stopping_) {
                    if (ec != boost::asio::error::eof) {
                        BOOST_LOG_TRIVIAL(error) << "Read error: " << ec.message();
                    }
                    cleanup_socket(socket);
                }
            });
    }

    void cleanup_socket(std::shared_ptr<boost::asio::ip::tcp::socket> socket) {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        auto it = std::find(active_sockets_.begin(), active_sockets_.end(), socket);
        if (it != active_sockets_.end()) {
            if (socket->is_open()) {
                boost::system::error_code ec;
                socket->cancel(ec);
                socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket->close(ec);
            }
            active_sockets_.erase(it);
        }
    }

    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
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
            BOOST_LOG_TRIVIAL(info) << "Test setup complete, server ready on port " 
                << server_->get_port();
                
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Test setup error: " << e.what();
            throw;
        }
    }

    void TearDown() override {
        try {
            if (server_) {
                server_->stop();
                server_.reset();
                BOOST_LOG_TRIVIAL(info) << "Test teardown complete";
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Test teardown error: " << e.what();
            throw;
        }
    }

    std::unique_ptr<MockServer> server_;
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
    TCP_Peer peer("stream_test_peer");
    
    // Test stream unavailability before connection
    EXPECT_EQ(peer.get_input_stream(), nullptr);
    EXPECT_EQ(peer.get_output_stream(), nullptr);
    
    // Connect and verify stream availability
    ASSERT_TRUE(peer.connect("127.0.0.1", server_->get_port()));
    EXPECT_NE(peer.get_input_stream(), nullptr);
    EXPECT_NE(peer.get_output_stream(), nullptr);
    
    // Configure stream processor
    std::promise<std::string> data_promise;
    auto data_future = data_promise.get_future();
    
    peer.set_stream_processor([&data_promise](std::istream& stream) {
        std::string data;
        if (std::getline(stream, data)) {
            BOOST_LOG_TRIVIAL(debug) << "Stream processor received data: '" << data << "'";
            data_promise.set_value(data);
        }
    });
    
    // Start stream processing
    ASSERT_TRUE(peer.start_stream_processing());
    
    // Test data transmission
    const std::string test_data = "Hello,TCP!\n";
    auto* output_stream = peer.get_output_stream();
    ASSERT_NE(output_stream, nullptr);
    
    BOOST_LOG_TRIVIAL(debug) << "Sending test data: '" << test_data << "'";
    *output_stream << test_data << std::flush;
    
    // Wait for data with timeout
    auto status = data_future.wait_for(std::chrono::milliseconds(500));
    ASSERT_EQ(status, std::future_status::ready) << "Timeout waiting for data";
    
    auto received_data = data_future.get();
    EXPECT_EQ(received_data + '\n', test_data) << 
        "Data mismatch. Expected: '" << test_data << "', Got: '" << received_data + '\n' << "'";
    
    // Cleanup
    peer.stop_stream_processing();
    peer.disconnect();
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
