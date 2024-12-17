#include <gtest/gtest.h>
#include <sstream>
#include "network/connection_state.hpp"
#include "network/peer.hpp"

using namespace dfs::network;

class MockPeer : public Peer {
public:
    bool connect(const std::string& address, uint16_t port) override { 
        if (validate_state(ConnectionState::State::INITIAL) || 
            validate_state(ConnectionState::State::DISCONNECTED)) {
            state = ConnectionState::State::CONNECTED;
            return true;
        }
        return false;
    }
    
    bool disconnect() override { 
        if (validate_state(ConnectionState::State::CONNECTED)) {
            state = ConnectionState::State::DISCONNECTED;
            read_active = false;
            return true;
        }
        return false;
    }
    
    bool send_data(const MessageFrame& frame) override { 
        return validate_state(ConnectionState::State::CONNECTED); 
    }
    
    void set_receive_callback(OnDataReceived callback) override {
        if (callback && validate_state(ConnectionState::State::CONNECTED)) {
            std::stringstream test_stream;
            // Simulate buffered data stream
            for (const auto& chunk : {"test_data\n", "more_test_data\n", "final_chunk"}) {
                test_stream << chunk;
                if (test_stream.fail()) {
                    state = ConnectionState::State::ERROR;
                    return;
                }
            }
            callback(test_stream);
        }
    }
    
    void start_read_loop() override {
        if (validate_state(ConnectionState::State::CONNECTED)) {
            read_active = true;
        }
    }
    
    void stop_read_loop() override {
        read_active = false;
    }
    
    ConnectionState::State get_connection_state() const override {
        return state;
    }

    bool is_reading() const { return read_active; }

private:
    ConnectionState::State state = ConnectionState::State::INITIAL;
    bool read_active = false;
};

TEST(NetworkTest, PeerStreamHandling) {
    MockPeer peer;
    std::string received_data;
    
    // Set up stream receive callback
    peer.set_receive_callback([&received_data](std::istream& stream) {
        std::string line;
        while (std::getline(stream, line)) {
            if (!stream.good() && !stream.eof()) {
                throw std::runtime_error("Stream read error");
            }
            received_data += line + ",";
        }
    });
    
    // Verify received stream data
    EXPECT_EQ(received_data, "test_data,more_test_data,final_chunk,");
}

TEST(NetworkTest, PeerStateTransitions) {
    MockPeer peer;
    
    // Initial connection
    EXPECT_TRUE(peer.connect("localhost", 8080));
    EXPECT_TRUE(peer.is_connected());
    
    // Start reading loop
    peer.start_read_loop();
    EXPECT_TRUE(peer.is_reading());
    
    // Stop reading and disconnect
    peer.stop_read_loop();
    EXPECT_FALSE(peer.is_reading());
    EXPECT_TRUE(peer.disconnect());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
