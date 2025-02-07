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

  // Helper to create a message frame with given parameters
  MessageFrame createFrame(uint8_t source_id, const std::string& payload) {
  MessageFrame frame;
  frame.message_type = MessageType::STORE_FILE;
  frame.source_id = source_id;
  frame.payload_size = payload.size();

  auto payload_stream = std::make_shared<std::stringstream>();
  payload_stream->write(payload.c_str(), payload.size());
  frame.payload_stream = payload_stream;

  return frame;
  }

  // Helper to verify frame contents match
  void verifyFrameEquals(const MessageFrame& actual, const MessageFrame& expected) {
  EXPECT_EQ(actual.message_type, expected.message_type);
  EXPECT_EQ(actual.source_id, expected.source_id);
  EXPECT_EQ(actual.payload_size, expected.payload_size);

  if (actual.payload_stream && expected.payload_stream) {
    EXPECT_EQ(actual.payload_stream->str(), expected.payload_stream->str());
  } else {
    FAIL() << "Payload streams are not both valid";
  }
  }

  // Helper to run producer thread
  void runProducer(int start_id, int count, std::chrono::microseconds delay = std::chrono::microseconds(0)) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> jitter(0, delay.count());

  for (int i = 0; i < count; ++i) {
    auto frame = createFrame(static_cast<uint8_t>(i % 256), std::string(1, static_cast<char>('A' + i % 26)));
    channel.produce(frame);

    if (delay.count() > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(jitter(gen)));
    }
  }
  }

  // Helper to run consumer thread
  void runConsumer(std::atomic<int>& consumed_count, std::atomic<bool>& done) {
  MessageFrame frame;
  while (!done || !channel.empty()) {
    if (channel.consume(frame)) {
      consumed_count++;
    }
    std::this_thread::yield();
  }
  }
};

TEST_F(ChannelTest, InitialState) {
  EXPECT_TRUE(channel.empty());
  EXPECT_EQ(channel.size(), 0);
}

TEST_F(ChannelTest, SingleProduceConsume) {
  auto input_frame = createFrame(123, "Hello");
  channel.produce(input_frame);

  EXPECT_FALSE(channel.empty());
  EXPECT_EQ(channel.size(), 1);

  MessageFrame output_frame;
  EXPECT_TRUE(channel.consume(output_frame));
  verifyFrameEquals(output_frame, input_frame);

  EXPECT_TRUE(channel.empty());
  EXPECT_EQ(channel.size(), 0);
}

TEST_F(ChannelTest, MultipleMessages) {
  std::vector<MessageFrame> frames;
  const int frame_count = 5;

  // Produce multiple frames
  for (int i = 0; i < frame_count; ++i) {
    auto frame = createFrame(static_cast<uint8_t>(i * 100), std::string(1, static_cast<char>('A' + i)));
    frames.push_back(frame);
    channel.produce(frame);
  }

  EXPECT_EQ(channel.size(), frame_count);

  // Consume and verify all frames in order
  for (int i = 0; i < frame_count; ++i) {
    MessageFrame output_frame;
    EXPECT_TRUE(channel.consume(output_frame));
    verifyFrameEquals(output_frame, frames[i]);
  }

  EXPECT_TRUE(channel.empty());
}

TEST_F(ChannelTest, ConsumeEmptyChannel) {
  MessageFrame frame;
  EXPECT_FALSE(channel.consume(frame));
}

TEST_F(ChannelTest, ConcurrentProducersConsumers) {
  const int num_producers = 4;
  const int num_consumers = 4;
  const int messages_per_producer = 50;
  std::atomic<int> consumed_count{0};
  std::atomic<bool> test_complete{false};

  // Start producer threads
  std::vector<std::thread> producers;
  for (int i = 0; i < num_producers; ++i) {
    producers.emplace_back([this, i, messages_per_producer]() {
      runProducer(i * messages_per_producer, messages_per_producer, std::chrono::microseconds(100));
    });
  }

  // Start consumer threads
  std::vector<std::thread> consumers;
  for (int i = 0; i < num_consumers; ++i) {
    consumers.emplace_back([this, &consumed_count, &test_complete]() {
      runConsumer(consumed_count, test_complete);
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

TEST_F(ChannelTest, AlternatingProduceConsume) {
  const int iterations = 50;
  std::atomic<bool> producer_done{false};
  std::atomic<int> consumed_count{0};

  // Producer thread
  std::thread producer([this, iterations, &producer_done]() {
    runProducer(0, iterations);
    producer_done = true;
  });

  // Consumer thread
  std::thread consumer([this, &producer_done, &consumed_count]() {
    runConsumer(consumed_count, producer_done);
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(consumed_count, iterations);
  EXPECT_TRUE(channel.empty());
}