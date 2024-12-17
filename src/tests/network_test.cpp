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
            processing = false;
            return true;
        }
        return false;
    }
    
    std::ostream* get_output_stream() override {
        if (!is_connected()) return nullptr;
        return &output_stream;
    }
    
    std::istream* get_input_stream() override {
        if (!is_connected()) return nullptr;
        return &input_stream;
    }
    
    void set_stream_processor(StreamProcessor processor) override {
        if (validate_state(ConnectionState::State::CONNECTED)) {
            stream_processor = processor;
        }
    }
    
    bool start_stream_processing() override {
        if (!is_connected() || processing || !stream_processor) return false;
        
        // Simulate stream data
        input_stream.str("");
        input_stream.clear();
        for (const auto& chunk : {"test_data\n", "more_test_data\n", "final_chunk"}) {
            input_stream << chunk;
            if (input_stream.fail()) {
                state = ConnectionState::State::ERROR;
                return false;
            }
        }
        
        processing = true;
        if (stream_processor) {
            stream_processor(input_stream);
        }
        return true;
    }
    
    void stop_stream_processing() override {
        processing = false;
    }
    
    ConnectionState::State get_connection_state() const override {
        return state;
    }

    bool is_processing() const { return processing; }

private:
    ConnectionState::State state = ConnectionState::State::INITIAL;
    bool processing = false;
    StreamProcessor stream_processor;
    std::stringstream input_stream;
    std::stringstream output_stream;
};

TEST(NetworkTest, StreamDataProcessing) {
    MockPeer peer;
    std::string received_data;
    
    // Connect and set up stream processor
    EXPECT_TRUE(peer.connect("localhost", 8080));
    peer.set_stream_processor([&received_data](std::istream& stream) {
        std::string line;
        while (std::getline(stream, line)) {
            if (!stream.good() && !stream.eof()) {
                throw std::runtime_error("Stream read error");
            }
            received_data += line + ",";
        }
    });
    
    // Start processing and verify data
    EXPECT_TRUE(peer.start_stream_processing());
    EXPECT_TRUE(peer.is_processing());
    EXPECT_EQ(received_data, "test_data,more_test_data,final_chunk,");
    
    // Clean up
    peer.stop_stream_processing();
    EXPECT_FALSE(peer.is_processing());
}

TEST(NetworkTest, StreamOperations) {
    MockPeer peer;
    
    // Test stream access before connection
    EXPECT_EQ(peer.get_input_stream(), nullptr);
    EXPECT_EQ(peer.get_output_stream(), nullptr);
    
    // Connect and verify streams
    EXPECT_TRUE(peer.connect("localhost", 8080));
    EXPECT_NE(peer.get_input_stream(), nullptr);
    EXPECT_NE(peer.get_output_stream(), nullptr);
    
    // Write to output stream
    auto* out = peer.get_output_stream();
    *out << "test_output" << std::endl;
    EXPECT_TRUE(out->good());
    
    // Clean up
    EXPECT_TRUE(peer.disconnect());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
