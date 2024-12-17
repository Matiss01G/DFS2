#include <gtest/gtest.h>
#include "network/connection_state.hpp"
#include "network/peer.hpp"

using namespace dfs::network;

// Mock implementation of Peer for testing
class MockPeer : public Peer {
public:
    bool connect(const std::string& address, uint16_t port) override { return true; }
    bool disconnect() override { return true; }
    bool send_data(const MessageFrame& frame) override { return true; }
    void set_receive_callback(DataCallback callback) override {}
    void start_read_loop() override {}
    void stop_read_loop() override {}
    ConnectionState::State get_connection_state() const override {
        return ConnectionState::State::CONNECTED;
    }
};

// Test Peer interface methods
TEST(NetworkTest, PeerInterface) {
    MockPeer peer;
    
    // Test connection operations
    EXPECT_TRUE(peer.connect("localhost", 8080));
    EXPECT_TRUE(peer.disconnect());
    
    // Test connection state
    EXPECT_EQ(peer.get_connection_state(), ConnectionState::State::CONNECTED);
    EXPECT_TRUE(peer.is_connected());
    
    // Test data operations
    MessageFrame frame{};
    EXPECT_TRUE(peer.send_data(frame));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
