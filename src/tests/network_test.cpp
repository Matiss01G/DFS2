#include <gtest/gtest.h>
#include <sstream>
#include "network/connection_state.hpp"
#include "network/peer.hpp"

using namespace dfs::network;

class NetworkTest : public ::testing::Test {
protected:
    std::condition_variable cv;
    std::mutex mtx;
    bool connected = false;
    bool transfer_complete = false;
    NetworkError last_error = NetworkError::SUCCESS;
    
    void SetUp() override {
        // Reset test state
        connected = false;
        transfer_complete = false;
        last_error = NetworkError::SUCCESS;
    }

    void TearDown() override {
        connected = false;
        transfer_complete = false;
    }

    static void connection_callback(const std::string& peer_id, bool success) {
        if (success) {
            std::unique_lock<std::mutex> lock(mtx);
            connected = true;
            cv.notify_one();
        }
    }

    static void transfer_callback(const std::string& peer_id, bool success) {
        if (success) {
            std::unique_lock<std::mutex> lock(mtx);
            transfer_complete = true;
            cv.notify_one();
        }
    }

    static void error_callback(const std::string& peer_id, NetworkError error) {
        std::unique_lock<std::mutex> lock(mtx);
        last_error = error;
        cv.notify_one();
    }
};

TEST_F(NetworkTest, ConnectToPeer) {
    NetworkManager manager1(12345);
    NetworkManager manager2(12346);
    
    // Start both network managers
    manager1.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow first manager to initialize
    manager2.start();
    
    // Set up connection callback
    manager2.set_connection_callback(std::bind(&NetworkTest::connection_callback, 
        std::placeholders::_1, std::placeholders::_2));
    
    // Add peer to manager2 and initiate connection
    const std::string peer_addr = "127.0.0.1:12345";
    EXPECT_TRUE(manager2.add_peer(peer_addr)) << "Failed to add peer " << peer_addr;
    
    // Wait for connection with timeout
    {
        std::unique_lock<std::mutex> lock(mtx);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return connected; }))
            << "Failed to establish connection within timeout period";
    }
    
    // Verify connection state
    EXPECT_TRUE(manager2.is_peer_connected(peer_addr)) 
        << "Peer connection state verification failed";
    
    // Clean up
    manager2.stop();
    manager1.stop();
}

TEST_F(NetworkTest, StreamFileTransfer) {
    NetworkManager manager1(12345);
    NetworkManager manager2(12346);
    
    // Start both network managers with initialization delay
    manager1.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    manager2.start();
    
    // Set up callbacks
    manager2.set_connection_callback(std::bind(&NetworkTest::connection_callback,
        std::placeholders::_1, std::placeholders::_2));
    manager2.set_transfer_callback(std::bind(&NetworkTest::transfer_callback,
        std::placeholders::_1, std::placeholders::_2));
    
    // Add peer and connect
    const std::string peer_id = "127.0.0.1:12345";
    EXPECT_TRUE(manager2.add_peer(peer_id)) << "Failed to add peer " << peer_id;
    
    // Wait for connection with detailed error message
    {
        std::unique_lock<std::mutex> lock(mtx);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return connected; }))
            << "Failed to establish connection for file transfer within timeout period";
    }
    
    // Verify connection before transfer
    EXPECT_TRUE(manager2.is_peer_connected(peer_id)) 
        << "Peer not connected before file transfer";
    
    // Create test data and initiate transfer
    std::string test_data = "Test file content";
    EXPECT_TRUE(manager2.send_to_peer(peer_id, test_data))
        << "Failed to initiate file transfer";
    
    // Wait for transfer completion with timeout
    {
        std::unique_lock<std::mutex> lock(mtx);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]{ return transfer_complete; }))
            << "File transfer did not complete within timeout period";
    }
    
    // Clean up in reverse order of creation
    manager2.stop();
    manager1.stop();
}

TEST_F(NetworkTest, ErrorHandling) {
    NetworkManager manager1(12345);
    NetworkManager manager2(12346);
    
    // Start managers with initialization delay
    manager1.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    manager2.start();
    
    // Set up error callback
    manager2.set_error_callback(std::bind(&NetworkTest::error_callback,
        std::placeholders::_1, std::placeholders::_2));
    
    // Try to connect to non-existent peer with invalid port
    const std::string invalid_peer = "127.0.0.1:54321";
    EXPECT_TRUE(manager2.add_peer(invalid_peer)) 
        << "Failed to add invalid peer for error testing";
    
    // Wait for error with timeout and verification
    {
        std::unique_lock<std::mutex> lock(mtx);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), 
            [&]{ return last_error != NetworkError::SUCCESS; }))
            << "No error received for invalid peer connection attempt";
    }
    
    // Verify error state
    EXPECT_FALSE(manager2.is_peer_connected(invalid_peer))
        << "Invalid peer showing as connected";
    EXPECT_NE(last_error, NetworkError::SUCCESS)
        << "Expected error state not set for invalid peer";
    
    // Clean up in reverse order
    manager2.stop();
    manager1.stop();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
