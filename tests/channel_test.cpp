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
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 123;
    input_frame.payload_size = 5;

    // Set initialization vector
    std::array<uint8_t, 16> iv = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    input_frame.initialization_vector = iv;

    channel.produce(input_frame);

    EXPECT_FALSE(channel.empty());
    EXPECT_EQ(channel.size(), 1);

    MessageFrame output_frame;
    EXPECT_TRUE(channel.consume(output_frame));

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.initialization_vector, input_frame.initialization_vector);

    EXPECT_TRUE(channel.empty());
    EXPECT_EQ(channel.size(), 0);
}

TEST_F(ChannelTest, MultipleMessages) {
    std::vector<MessageFrame> frames;
    const int frame_count = 5;

    // Produce multiple frames
    for (int i = 0; i < frame_count; ++i) {
        MessageFrame frame;
        frame.message_type = i % 2 == 0 ? MessageType::STORE_FILE : MessageType::GET_FILE;
        frame.source_id = i * 100;
        frame.payload_size = sizeof(int);

        std::array<uint8_t, 16> iv;
        for (size_t j = 0; j < 16; ++j) {
            iv[j] = static_cast<uint8_t>(i + j);
        }
        frame.initialization_vector = iv;

        frames.push_back(frame);
        channel.produce(frame);
    }

    EXPECT_EQ(channel.size(), frame_count);

    // Consume and verify all frames in order
    for (int i = 0; i < frame_count; ++i) {
        MessageFrame output_frame;
        EXPECT_TRUE(channel.consume(output_frame));
        EXPECT_EQ(output_frame.message_type, frames[i].message_type);
        EXPECT_EQ(output_frame.source_id, frames[i].source_id);
        EXPECT_EQ(output_frame.payload_size, frames[i].payload_size);
        EXPECT_EQ(output_frame.initialization_vector, frames[i].initialization_vector);
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
                frame.message_type = j % 2 == 0 ? MessageType::STORE_FILE : MessageType::GET_FILE;
                frame.source_id = j;
                frame.payload_size = sizeof(int);

                std::array<uint8_t, 16> iv;
                for (size_t k = 0; k < 16; ++k) {
                    iv[k] = static_cast<uint8_t>((j + k) & 0xFF);
                }
                frame.initialization_vector = iv;

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
TEST_F(ChannelTest, StressTest) {
    const int operations = 10000;
    std::atomic<bool> stop{false};
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    // Producer thread
    std::thread producer([this, &stop, &produced]() {
        while (produced < operations && !stop) {
            MessageFrame frame;
            frame.message_type = MessageType::STORE_FILE;
            frame.source_id = produced;
            frame.payload_size = sizeof(int);

            std::array<uint8_t, 16> iv;
            for (size_t i = 0; i < 16; ++i) {
                iv[i] = static_cast<uint8_t>((produced + i) & 0xFF);
            }
            frame.initialization_vector = iv;

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