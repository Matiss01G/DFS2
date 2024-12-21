#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include "channel/channel.hpp"
#include "network/message_frame.hpp"

using namespace dfs::network;

class ChannelTest : public ::testing::Test {
protected:
    Channel channel;
};

// Basic functionality tests
TEST_F(ChannelTest, InitialState) {
    EXPECT_TRUE(channel.empty());
    EXPECT_EQ(channel.size(), 0);
}

TEST_F(ChannelTest, SingleProduceConsume) {
    MessageFrame input_frame;
    input_frame.type = 1;
    input_frame.source_id = 123;
    input_frame.payload_size = 5;
    input_frame.payload_data = {'H', 'e', 'l', 'l', 'o'};

    channel.produce(input_frame);

    EXPECT_FALSE(channel.empty());
    EXPECT_EQ(channel.size(), 1);

    MessageFrame output_frame;
    EXPECT_TRUE(channel.consume(output_frame));

    EXPECT_EQ(output_frame.type, input_frame.type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.payload_data, input_frame.payload_data);

    EXPECT_TRUE(channel.empty());
    EXPECT_EQ(channel.size(), 0);
}

TEST_F(ChannelTest, MultipleMessages) {
    std::vector<MessageFrame> frames;
    const int frame_count = 5;

    // Produce multiple frames
    for (int i = 0; i < frame_count; ++i) {
        MessageFrame frame;
        frame.type = i;
        frame.source_id = i * 100;
        frame.payload_size = 1;
        frame.payload_data = {static_cast<char>('A' + i)};
        frames.push_back(frame);
        channel.produce(frame);
    }

    EXPECT_EQ(channel.size(), frame_count);

    // Consume and verify all frames in order
    for (int i = 0; i < frame_count; ++i) {
        MessageFrame output_frame;
        EXPECT_TRUE(channel.consume(output_frame));
        EXPECT_EQ(output_frame.type, frames[i].type);
        EXPECT_EQ(output_frame.source_id, frames[i].source_id);
        EXPECT_EQ(output_frame.payload_data, frames[i].payload_data);
    }

    EXPECT_TRUE(channel.empty());
}

TEST_F(ChannelTest, ConsumeEmptyChannel) {
    MessageFrame frame;
    EXPECT_FALSE(channel.consume(frame));
}

// Thread safety tests
TEST_F(ChannelTest, ConcurrentProducersConsumers) {
    const int num_producers = 4;
    const int num_consumers = 4;
    const int messages_per_producer = 1000;
    std::atomic<int> consumed_count{0};
    std::atomic<bool> test_complete{false};

    // Start producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([this, i, messages_per_producer]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delay(0, 100);

            for (int j = 0; j < messages_per_producer; ++j) {
                MessageFrame frame;
                frame.type = i;
                frame.source_id = j;
                frame.payload_size = sizeof(int);
                frame.payload_data = {static_cast<char>(j & 0xFF)};
                channel.produce(frame);

                // Random delay to increase chance of thread interleaving
                std::this_thread::sleep_for(std::chrono::microseconds(delay(gen)));
            }
        });
    }

    // Start consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([this, &consumed_count, &test_complete]() {
            MessageFrame frame;
            while (!test_complete || !channel.empty()) {
                if (channel.consume(frame)) {
                    consumed_count++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Join producer threads
    for (auto& producer : producers) {
        producer.join();
    }

    // Wait for all messages to be consumed
    while (consumed_count < (num_producers * messages_per_producer)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    test_complete = true;

    // Join consumer threads
    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_EQ(consumed_count, num_producers * messages_per_producer);
    EXPECT_TRUE(channel.empty());
}

// Edge cases and stress testing
TEST_F(ChannelTest, AlternatingProduceConsume) {
    const int iterations = 1000;
    std::atomic<bool> producer_done{false};
    std::atomic<int> consumed_count{0};

    // Producer thread
    std::thread producer([this, iterations, &producer_done]() {
        for (int i = 0; i < iterations; ++i) {
            MessageFrame frame;
            frame.type = 1;
            frame.source_id = i;
            frame.payload_size = sizeof(int);
            frame.payload_data = {static_cast<char>(i & 0xFF)};
            channel.produce(frame);
            std::this_thread::yield(); // Allow consumer to interleave
        }
        producer_done = true;
    });

    // Consumer thread
    std::thread consumer([this, &producer_done, &consumed_count]() {
        MessageFrame frame;
        while (!producer_done || !channel.empty()) {
            if (channel.consume(frame)) {
                consumed_count++;
            }
            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed_count, iterations);
    EXPECT_TRUE(channel.empty());
}

TEST_F(ChannelTest, BurstyTraffic) {
    const int burst_size = 100;
    const int num_bursts = 10;
    std::atomic<int> consumed_count{0};

    std::thread producer([this, burst_size, num_bursts]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay(1, 50);

        for (int burst = 0; burst < num_bursts; ++burst) {
            // Produce burst of messages
            for (int i = 0; i < burst_size; ++i) {
                MessageFrame frame;
                frame.type = burst;
                frame.source_id = i;
                channel.produce(frame);
            }

            // Random delay between bursts
            std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
        }
    });

    std::thread consumer([this, burst_size, num_bursts, &consumed_count]() {
        MessageFrame frame;
        const int total_messages = burst_size * num_bursts;

        while (consumed_count < total_messages) {
            if (channel.consume(frame)) {
                consumed_count++;
            }
            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed_count, burst_size * num_bursts);
    EXPECT_TRUE(channel.empty());
}

// Stress test with rapid produce/consume operations
TEST_F(ChannelTest, StressTest) {
    const int operations = 10000;
    std::atomic<bool> stop{false};
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    // Producer thread
    std::thread producer([this, &stop, &produced]() {
        while (produced < operations && !stop) {
            MessageFrame frame;
            frame.type = 1;
            frame.source_id = produced;
            channel.produce(frame);
            produced++;
        }
    });

    // Consumer thread
    std::thread consumer([this, &stop, &consumed]() {
        MessageFrame frame;
        while (consumed < operations && !stop) {
            if (channel.consume(frame)) {
                consumed++;
            }
            std::this_thread::yield();
        }
    });

    // Allow test to run for a maximum of 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));
    stop = true;

    producer.join();
    consumer.join();

    EXPECT_EQ(produced, consumed);
    EXPECT_TRUE(channel.empty());
}