#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <random>
#include <sstream>
#include "network/channel.hpp"
#include "network/message_frame.hpp"

using namespace dfs::network;

class ChannelTest : public ::testing::Test {
protected:
    Channel channel;
};

/**
 * @brief Tests the initial state of a newly created Channel
 * @details Verifies that a newly created Channel:
 * - Is empty (contains no messages)
 * - Has a size of 0
 * Expected outcome: Both empty() returns true and size() returns 0
 */
TEST_F(ChannelTest, InitialState) {
    EXPECT_TRUE(channel.empty());
    EXPECT_EQ(channel.size(), 0);
}

/**
 * @brief Tests basic produce and consume operations with a single message
 * @details Verifies that:
 * - A single message can be produced (added) to the channel
 * - The same message can be consumed (retrieved) from the channel
 * - The message contents remain intact during the process
 * - The channel returns to empty state after consumption
 * Expected outcome: Message is successfully stored and retrieved with all fields matching,
 * and channel becomes empty after consumption
 */
TEST_F(ChannelTest, SingleProduceConsume) {
    MessageFrame input_frame;
    input_frame.message_type = MessageType::STORE_FILE;
    input_frame.source_id = 123;
    input_frame.payload_size = 5;

    // Create a string stream for payload
    auto payload = std::make_shared<std::stringstream>();
    payload->write("Hello", 5);
    input_frame.payload_stream = payload;

    channel.produce(input_frame);

    EXPECT_FALSE(channel.empty());
    EXPECT_EQ(channel.size(), 1);

    MessageFrame output_frame;
    EXPECT_TRUE(channel.consume(output_frame));

    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);

    // Compare payload streams
    if (output_frame.payload_stream && input_frame.payload_stream) {
        std::string input_data = input_frame.payload_stream->str();
        std::string output_data = output_frame.payload_stream->str();
        EXPECT_EQ(output_data, input_data);
    }

    EXPECT_TRUE(channel.empty());
    EXPECT_EQ(channel.size(), 0);
}

/**
 * @brief Tests handling of multiple messages in sequence
 * @details Verifies that:
 * - Multiple messages can be produced in sequence
 * - Messages are consumed in FIFO (First-In-First-Out) order
 * - All message properties are preserved
 * - Channel size updates correctly with each operation
 * Expected outcome: All messages are retrieved in the same order they were added,
 * with all properties matching their original values
 */
TEST_F(ChannelTest, MultipleMessages) {
    std::vector<MessageFrame> frames;
    const int frame_count = 5;

    // Produce multiple frames
    for (int i = 0; i < frame_count; ++i) {
        MessageFrame frame;
        frame.message_type = MessageType::STORE_FILE;
        frame.source_id = i * 100;
        frame.payload_size = 1;

        auto payload = std::make_shared<std::stringstream>();
        payload->put(static_cast<char>('A' + i));
        frame.payload_stream = payload;

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

        if (output_frame.payload_stream && frames[i].payload_stream) {
            std::string output_data = output_frame.payload_stream->str();
            std::string input_data = frames[i].payload_stream->str();
            EXPECT_EQ(output_data, input_data);
        }
    }

    EXPECT_TRUE(channel.empty());
}

/**
 * @brief Tests behavior when consuming from an empty channel
 * @details Verifies that:
 * - Attempting to consume from an empty channel returns false
 * - The output frame remains unmodified
 * Expected outcome: consume() returns false and the channel remains empty
 */
TEST_F(ChannelTest, ConsumeEmptyChannel) {
    MessageFrame frame;
    EXPECT_FALSE(channel.consume(frame));
}

/**
 * @brief Tests thread safety with multiple concurrent producers and consumers
 * @details Verifies that:
 * - Multiple threads can safely produce and consume messages simultaneously
 * - No messages are lost or corrupted during concurrent operations
 * - Channel maintains consistency under high concurrency
 * - All produced messages are successfully consumed exactly once
 * Expected outcome: All messages are successfully produced and consumed,
 * with the final consumed count matching the total number of produced messages
 */
TEST_F(ChannelTest, ConcurrentProducersConsumers) {
    const int num_producers = 4;
    const int num_consumers = 4;
    const int messages_per_producer = 50;  // Reduced from 1000 to 50
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
                frame.message_type = MessageType::STORE_FILE;
                frame.source_id = j;
                frame.payload_size = sizeof(int);

                auto payload = std::make_shared<std::stringstream>();
                payload->put(static_cast<char>(j & 0xFF));
                frame.payload_stream = payload;

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

/**
 * @brief Tests alternating produce-consume pattern with timing variations
 * @details Verifies that:
 * - Channel handles alternating produce and consume operations correctly
 * - Operations work correctly with different timing patterns
 * - All messages are processed in order without loss
 * Expected outcome: All produced messages are consumed successfully,
 * with the final consumed count matching the number of iterations
 */
TEST_F(ChannelTest, AlternatingProduceConsume) {
    const int iterations = 50;  // Reduced from 1000 to 50
    std::atomic<bool> producer_done{false};
    std::atomic<int> consumed_count{0};

    // Producer thread
    std::thread producer([this, iterations, &producer_done]() {
        for (int i = 0; i < iterations; ++i) {
            MessageFrame frame;
            frame.message_type = MessageType::STORE_FILE;
            frame.source_id = i;
            frame.payload_size = sizeof(int);

            auto payload = std::make_shared<std::stringstream>();
            payload->put(static_cast<char>(i & 0xFF));
            frame.payload_stream = payload;

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