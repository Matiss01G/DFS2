#include <gtest/gtest.h>
#include <sstream>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "network/message_frame.hpp"
#include "crypto/crypto_stream.hpp"

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
  std::vector<uint8_t> test_key = std::vector<uint8_t>(32, 0x42);
  Channel channel;
  Codec codec{test_key, channel};

  std::string generate_random_data(size_t size) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result(size, 0);
    for (size_t i = 0; i < size; ++i) {
      result[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    return result;
  }

  std::vector<uint8_t> generate_test_iv() {
    return std::vector<uint8_t>(dfs::crypto::CryptoStream::IV_SIZE, 0x42);
  }

  // Helper to create a basic message frame with common settings
  MessageFrame createBasicFrame(uint32_t source_id = 1, size_t payload_size = 0, size_t filename_length = 0) {
    MessageFrame frame;
    frame.message_type = MessageType::STORE_FILE;
    frame.source_id = source_id;
    frame.payload_size = payload_size;
    frame.filename_length = filename_length;
    frame.iv_ = generate_test_iv();
    return frame;
  }

  // Helper to add payload to a frame
  void addPayload(MessageFrame& frame, const std::string& data) {
    auto payload = std::make_shared<std::stringstream>();
    ASSERT_TRUE(payload->good()) << "Failed to create payload stream";
    payload->write(data.c_str(), data.length());
    payload->seekg(0);
    payload->seekp(0);
    frame.payload_stream = payload;
    frame.payload_size = data.length();
  }

  // Helper to verify frame serialization and deserialization
  void verifySerializeDeserialize(const MessageFrame& input_frame) {
    std::stringstream output_stream;
    ASSERT_TRUE(output_stream.good()) << "Output stream not in good state";

    std::size_t written = 0;
    ASSERT_NO_THROW({
      written = codec.serialize(input_frame, output_stream);
    }) << "Serialization failed";
    ASSERT_GT(written, 0) << "No data written";

    output_stream.seekg(0);
    ASSERT_TRUE(output_stream.good()) << "Failed to reset stream position";

    ASSERT_NO_THROW({
      codec.deserialize(output_stream);
    }) << "Deserialization failed";

    MessageFrame output_frame;
    ASSERT_TRUE(channel.consume(output_frame)) << "Failed to consume frame";
    verifyFramesMatch(input_frame, output_frame);
    EXPECT_TRUE(channel.empty());
  }

  // Helper to verify two frames match
  void verifyFramesMatch(const MessageFrame& input_frame, const MessageFrame& output_frame) {
    EXPECT_EQ(output_frame.message_type, input_frame.message_type);
    EXPECT_EQ(output_frame.source_id, input_frame.source_id);
    EXPECT_EQ(output_frame.payload_size, input_frame.payload_size);
    EXPECT_EQ(output_frame.filename_length, input_frame.filename_length);
    EXPECT_EQ(output_frame.iv_, input_frame.iv_);

    if (input_frame.payload_stream && output_frame.payload_stream) {
      input_frame.payload_stream->seekg(0);
      output_frame.payload_stream->seekg(0);

      std::string input_data((std::istreambuf_iterator<char>(*input_frame.payload_stream)),
                              std::istreambuf_iterator<char>());
      std::string output_data((std::istreambuf_iterator<char>(*output_frame.payload_stream)),
                               std::istreambuf_iterator<char>());

      EXPECT_EQ(output_data.length(), input_data.length()) << "Payload size mismatch";
      EXPECT_EQ(output_data, input_data) << "Payload data mismatch";
    }
  }
};



TEST_F(CodecTest, MinimalFrameSerializeDeserialize) {
  MessageFrame frame = createBasicFrame(1);
  verifySerializeDeserialize(frame);
}

TEST_F(CodecTest, BasicSerializeDeserialize) {
  MessageFrame frame = createBasicFrame(2, 0, 8);
  const std::string test_data = "TestData123";
  addPayload(frame, test_data);
  verifySerializeDeserialize(frame);
}

TEST_F(CodecTest, LargePayloadChunkedProcessing) {
  MessageFrame frame = createBasicFrame(3, 0, 12);
  const size_t payload_size = 10 * 1024 * 1024;  // 10MB
  std::string large_data = generate_random_data(payload_size);
  addPayload(frame, large_data);
  verifySerializeDeserialize(frame);
}

TEST_F(CodecTest, EmptySourceId) {
  MessageFrame frame = createBasicFrame(0);
  verifySerializeDeserialize(frame);
}