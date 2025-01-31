// #include <gtest/gtest.h>
// #include <thread>
// #include <vector>
// #include <chrono>
// #include <atomic>
// #include <random>
// #include <sstream>
// #include "network/channel.hpp"
// #include "network/message_frame.hpp"

// using namespace dfs::network;

// class ChannelTest : public ::testing::Test {
// protected:
//     Channel channel;
// };

// TEST_F(ChannelTest, InitialState) {
//     EXPECT_TRUE(channel.empty());
//     EXPECT_EQ(channel.size(), 0);
// }

// TEST_F(ChannelTest, SingleProduceConsume) {
//     MessageFrame input_frame;
//     input_frame.message_type = MessageType::STORE_FILE;
//     input_frame.source_id = 123;  // Changed to uint8_t value
//     input_frame.payload_size = 5;

//     // Create a string stream for payload
//     auto payload = std::make_shared<std::stringstream>();
//     payload->write("Hello", 5);
//     input_frame.payload_stream = payload;

//     channel.produce(input_frame);

//     EXPECT_FALSE(channel.empty());
//     EXPECT_EQ(channel.size(), 1);

//     MessageFrame output_frame;
//     EXPECT_TRUE(channel.consume(output_frame));

//     EXPECT_EQ(output_frame.message_type, input_frame.message_type);
//     EXPECT_EQ(output_frame.source_id, input_frame.source_id);
//     EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);

//     // Compare payload streams
//     if (output_frame.payload_stream && input_frame.payload_stream) {
//         std::string input_data = input_frame.payload_stream->str();
//         std::string output_data = output_frame.payload_stream->str();
//         EXPECT_EQ(output_data, input_data);
//     }

//     EXPECT_TRUE(channel.empty());
//     EXPECT_EQ(channel.size(), 0);
// }

// TEST_F(ChannelTest, MultipleMessages) {
//     std::vector<MessageFrame> frames;
//     const int frame_count = 5;

//     // Produce multiple frames
//     for (int i = 0; i < frame_count; ++i) {
//         MessageFrame frame;
//         frame.message_type = MessageType::STORE_FILE;
//         frame.source_id = static_cast<uint8_t>((i * 100) % 256);  // Convert to uint8_t, handle overflow
//         frame.payload_size = 1;

//         auto payload = std::make_shared<std::stringstream>();
//         payload->put(static_cast<char>('A' + i));
//         frame.payload_stream = payload;

//         frames.push_back(frame);
//         channel.produce(frame);
//     }

//     EXPECT_EQ(channel.size(), frame_count);

//     // Consume and verify all frames in order
//     for (int i = 0; i < frame_count; ++i) {
//         MessageFrame output_frame;
//         EXPECT_TRUE(channel.consume(output_frame));
//         EXPECT_EQ(output_frame.message_type, frames[i].message_type);
//         EXPECT_EQ(output_frame.source_id, frames[i].source_id);

//         if (output_frame.payload_stream && frames[i].payload_stream) {
//             std::string output_data = output_frame.payload_stream->str();
//             std::string input_data = frames[i].payload_stream->str();
//             EXPECT_EQ(output_data, input_data);
//         }
//     }

//     EXPECT_TRUE(channel.empty());
// }

// TEST_F(ChannelTest, ConsumeEmptyChannel) {
//     MessageFrame frame;
//     EXPECT_FALSE(channel.consume(frame));
// }

// TEST_F(ChannelTest, ConcurrentProducersConsumers) {
//     const int num_producers = 4;
//     const int num_consumers = 4;
//     const int messages_per_producer = 50;
//     std::atomic<int> consumed_count{0};
//     std::atomic<bool> test_complete{false};

//     // Start producer threads
//     std::vector<std::thread> producers;
//     for (int i = 0; i < num_producers; ++i) {
//         producers.emplace_back([this, i, messages_per_producer]() {
//             std::random_device rd;
//             std::mt19937 gen(rd());
//             std::uniform_int_distribution<> delay(0, 100);

//             for (int j = 0; j < messages_per_producer; ++j) {
//                 MessageFrame frame;
//                 frame.message_type = MessageType::STORE_FILE;
//                 frame.source_id = static_cast<uint8_t>(j % 256);  // Convert to uint8_t, handle overflow
//                 frame.payload_size = sizeof(int);

//                 auto payload = std::make_shared<std::stringstream>();
//                 payload->put(static_cast<char>(j & 0xFF));
//                 frame.payload_stream = payload;

//                 channel.produce(frame);

//                 // Random delay to increase chance of thread interleaving
//                 std::this_thread::sleep_for(std::chrono::microseconds(delay(gen)));
//             }
//         });
//     }

//     // Start consumer threads
//     std::vector<std::thread> consumers;
//     for (int i = 0; i < num_consumers; ++i) {
//         consumers.emplace_back([this, &consumed_count, &test_complete]() {
//             MessageFrame frame;
//             while (!test_complete || !channel.empty()) {
//                 if (channel.consume(frame)) {
//                     consumed_count++;
//                 }
//                 std::this_thread::yield();
//             }
//         });
//     }

//     // Join producer threads
//     for (auto& producer : producers) {
//         producer.join();
//     }

//     // Wait for all messages to be consumed
//     while (consumed_count < (num_producers * messages_per_producer)) {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }

//     test_complete = true;

//     // Join consumer threads
//     for (auto& consumer : consumers) {
//         consumer.join();
//     }

//     EXPECT_EQ(consumed_count, num_producers * messages_per_producer);
//     EXPECT_TRUE(channel.empty());
// }

// TEST_F(ChannelTest, AlternatingProduceConsume) {
//     const int iterations = 50;
//     std::atomic<bool> producer_done{false};
//     std::atomic<int> consumed_count{0};

//     // Producer thread
//     std::thread producer([this, iterations, &producer_done]() {
//         for (int i = 0; i < iterations; ++i) {
//             MessageFrame frame;
//             frame.message_type = MessageType::STORE_FILE;
//             frame.source_id = static_cast<uint8_t>(i % 256);  // Convert to uint8_t, handle overflow
//             frame.payload_size = sizeof(int);

//             auto payload = std::make_shared<std::stringstream>();
//             payload->put(static_cast<char>(i & 0xFF));
//             frame.payload_stream = payload;

//             channel.produce(frame);
//             std::this_thread::yield(); // Allow consumer to interleave
//         }
//         producer_done = true;
//     });

//     // Consumer thread
//     std::thread consumer([this, &producer_done, &consumed_count]() {
//         MessageFrame frame;
//         while (!producer_done || !channel.empty()) {
//             if (channel.consume(frame)) {
//                 consumed_count++;
//             }
//             std::this_thread::yield();
//         }
//     });

//     producer.join();
//     consumer.join();

//     EXPECT_EQ(consumed_count, iterations);
//     EXPECT_TRUE(channel.empty());
// }