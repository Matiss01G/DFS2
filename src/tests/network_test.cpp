#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <memory>
#include <sstream>
#include <thread>
#include <chrono>
#include "../../include/network/network_manager.hpp"

using namespace dfs::network;
using namespace std::chrono_literals;

class NetworkTest : public ::testing::Test {
protected:
    boost::asio::io_context io_context_;
    std::unique_ptr<NetworkManager> server_;
    std::unique_ptr<NetworkManager> client_;
    
    void SetUp() override {
        server_ = std::make_unique<NetworkManager>();
        client_ = std::make_unique<NetworkManager>();
        
        // Start server
        server_->start(12345);
        std::this_thread::sleep_for(100ms);  // Allow server to start
    }
    
    void TearDown() override {
        if (client_) client_->stop();
        if (server_) server_->stop();
    }
};

TEST_F(NetworkTest, ConnectToPeer) {
    bool connected = false;
    std::mutex mtx;
    std::condition_variable cv;
    
    // Set up error handler to detect successful connection
    client_->set_error_handler([&](NetworkError error) {
        std::lock_guard<std::mutex> lock(mtx);
        if (error == NetworkError::SUCCESS) {
            connected = true;
            cv.notify_one();
        } else {
            std::cerr << "Connection error: " << to_string(error) << std::endl;
        }
    });
    
    // Connect client to server
    client_->start(12346);
    client_->connect_to_peer("127.0.0.1", 12345);
    
    // Wait for connection or timeout
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]{ return connected; }))
        << "Failed to establish connection within timeout period";
}

TEST_F(NetworkTest, StreamPacketTransfer) {
    std::string test_data = "Hello, World!";
    auto input_stream = std::make_shared<std::stringstream>(test_data);
    std::string received_data;
    std::mutex mtx;
    std::condition_variable cv;
    bool transfer_complete = false;
    
    // Start client
    client_->start(12346);
    client_->connect_to_peer("127.0.0.1", 12345);
    std::this_thread::sleep_for(100ms);  // Allow connection to establish
    
    // Set up packet handler
    server_->set_packet_handler(
        [&](const PacketHeader& header, std::shared_ptr<std::istream> payload) {
            if (header.type == PacketType::DATA) {
                std::stringstream ss;
                ss << payload->rdbuf();
                
                std::lock_guard<std::mutex> lock(mtx);
                received_data = ss.str();
                transfer_complete = true;
                cv.notify_one();
            }
        });
    
    // Send packet from client to server
    client_->send_packet(std::array<uint8_t, 16>(), PacketType::DATA, input_stream, 1);
    
    // Wait for transfer completion or timeout
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]{ return transfer_complete; }));
    EXPECT_EQ(received_data, test_data);
}

TEST_F(NetworkTest, ErrorHandling) {
    NetworkError last_error = NetworkError::SUCCESS;
    std::mutex mtx;
    std::condition_variable cv;
    
    client_->set_error_handler([&](NetworkError error) {
        std::lock_guard<std::mutex> lock(mtx);
        last_error = error;
        cv.notify_one();
    });
    
    // Try to connect to non-existent peer
    client_->start(12346);
    client_->connect_to_peer("127.0.0.1", 54321);
    
    // Wait for error or timeout
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]{ return last_error != NetworkError::SUCCESS; }));
    EXPECT_EQ(last_error, NetworkError::CONNECTION_FAILED);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
