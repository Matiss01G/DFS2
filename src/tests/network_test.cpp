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
        connected = (error == NetworkError::SUCCESS);
        cv.notify_one();
    });
    
    // Connect client to server
    client_->start(12346);
    client_->connect_to_peer("127.0.0.1", 12345);
    
    // Wait for connection or timeout
    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]{ return connected; }));
}

TEST_F(NetworkTest, StreamFileTransfer) {
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
    
    // Set up file received handler
    server_->set_file_received_handler(
        [&](const std::array<uint8_t, 32>& source_id,
            const std::vector<uint8_t>& file_key,
            std::shared_ptr<std::istream> payload) {
            std::stringstream ss;
            ss << payload->rdbuf();
            
            std::lock_guard<std::mutex> lock(mtx);
            received_data = ss.str();
            transfer_complete = true;
            cv.notify_one();
        });
    
    // Generate random file key
    std::vector<uint8_t> file_key(32);
    std::generate(file_key.begin(), file_key.end(), std::rand);
    
    // Send file from client to server
    client_->send_file(std::array<uint8_t, 32>(), input_stream, file_key);
    
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
