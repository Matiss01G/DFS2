#include <gtest/gtest.h>
#include "network/channel/channel.hpp"
#include "network/message_frame.hpp"
#include <thread>
#include <chrono>
#include <vector>

using namespace dfs::network;

class ChannelTest : public ::testing::Test {
protected:
    Channel channel;
    
    MessageFrame createTestFrame(uint32_t source_id = 1, uint64_t payload_size = 100) {
        MessageFrame frame;
        frame.source_id = source_id;
        frame.message_type = MessageType::DATA;
        frame.payload_size = payload_size;
        // Initialize IV with some test data
        std::fill(frame.initialization_vector.begin(), frame.initialization_vector.end(), 0x42);
        return frame;
    }
};

// Test basic produce and consume functionality
TEST_F(ChannelTest, BasicProduceConsume) {
    MessageFrame input_frame = createTestFrame();
    channel.produce(input_frame);
    
    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame));
    
    EXPECT_EQ(input_frame.source_id, output_frame.source_id);
    EXPECT_EQ(input_frame.message_type, output_frame.message_type);
    EXPECT_EQ(input_frame.payload_size, output_frame.payload_size);
    EXPECT_EQ(input_frame.initialization_vector, output_frame.initialization_vector);
}

// Test empty channel behavior
TEST_F(ChannelTest, EmptyChannel) {
    EXPECT_TRUE(channel.empty());
    EXPECT_EQ(channel.size(), 0);
    
    MessageFrame frame;
    EXPECT_FALSE(channel.consume(frame));
}

// Test size tracking
TEST_F(ChannelTest, SizeTracking) {
    EXPECT_EQ(channel.size(), 0);
    
    channel.produce(createTestFrame(1));
    EXPECT_EQ(channel.size(), 1);
    
    channel.produce(createTestFrame(2));
    EXPECT_EQ(channel.size(), 2);
    
    MessageFrame frame;
    channel.consume(frame);
    EXPECT_EQ(channel.size(), 1);
    
    channel.consume(frame);
    EXPECT_EQ(channel.size(), 0);
}

// Test FIFO ordering
TEST_F(ChannelTest, FIFOOrdering) {
    const int num_messages = 5;
    
    // Produce messages with sequential source IDs
    for (int i = 0; i < num_messages; ++i) {
        channel.produce(createTestFrame(i));
    }
    
    // Verify messages are consumed in the same order
    for (int i = 0; i < num_messages; ++i) {
        MessageFrame frame;
        ASSERT_TRUE(channel.consume(frame));
        EXPECT_EQ(frame.source_id, i);
    }
}

// Test thread safety with multiple producers and consumers
TEST_F(ChannelTest, ThreadSafety) {
    const int num_producers = 4;
    const int num_consumers = 4;
    const int messages_per_producer = 1000;
    
    std::atomic<int> total_consumed(0);
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    
    // Start producer threads
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([this, i, messages_per_producer]() {
            for (int j = 0; j < messages_per_producer; ++j) {
                channel.produce(createTestFrame(i * messages_per_producer + j));
            }
        });
    }
    
    // Start consumer threads
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([this, &total_consumed]() {
            MessageFrame frame;
            while (total_consumed < num_producers * messages_per_producer) {
                if (channel.consume(frame)) {
                    total_consumed++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& producer : producers) {
        producer.join();
    }
    for (auto& consumer : consumers) {
        consumer.join();
    }
    
    // Verify all messages were consumed
    EXPECT_EQ(total_consumed, num_producers * messages_per_producer);
    EXPECT_TRUE(channel.empty());
}

// Test channel with varying message sizes
TEST_F(ChannelTest, VaryingMessageSizes) {
    std::vector<uint64_t> sizes = {0, 100, 1024, 1024*1024};
    
    for (uint64_t size : sizes) {
        MessageFrame input_frame = createTestFrame(1, size);
        channel.produce(input_frame);
        
        MessageFrame output_frame;
        ASSERT_TRUE(channel.consume(output_frame));
        EXPECT_EQ(output_frame.payload_size, size);
    }
}
