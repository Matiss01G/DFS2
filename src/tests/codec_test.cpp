#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <memory>
#include "network/codec.hpp"
#include "network/channel.hpp"
#include "crypto/crypto_stream.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/core/null_deleter.hpp>

using namespace dfs::network;

class CodecTest : public ::testing::Test {
protected:
    std::vector<uint8_t> key_;
    Codec* codec_;
    Channel* channel_;

    void SetUp() override {
        // Reset Boost.Log core
        boost::log::core::get()->remove_all_sinks();

        // Create and configure a new console sink
        typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> text_sink;
        boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

        // Add console as a destination
        boost::shared_ptr<std::ostream> stream(&std::cout, boost::null_deleter());
        sink->locked_backend()->add_stream(stream);

        // Set the formatter
        sink->set_formatter(
            boost::log::expressions::stream
                << "[" << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << "] [" << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
                << "] [" << boost::log::trivial::severity
                << "] " << boost::log::expressions::smessage
        );

        // Set the filter to debug level
        sink->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);

        // Add the sink to the core
        boost::log::core::get()->add_sink(sink);

        // Add common attributes once
        boost::log::add_common_attributes();

        BOOST_LOG_TRIVIAL(info) << "Setting up CodecTest fixture";

        // Initialize 256-bit (32 byte) encryption key
        key_.resize(32, 0x42);  // Test key filled with 0x42
        codec_ = new Codec(key_);
        channel_ = new Channel();

        BOOST_LOG_TRIVIAL(debug) << "Test fixture initialized with key size: " << key_.size();
    }

    void TearDown() override {
        BOOST_LOG_TRIVIAL(debug) << "Tearing down CodecTest fixture";
        delete codec_;
        delete channel_;

        // Clean up logging
        boost::log::core::get()->remove_all_sinks();
    }

    // Helper to create a test message frame
    MessageFrame createTestFrame(const std::string& payload = "test payload", 
                               MessageType type = MessageType::STORE_FILE) {
        BOOST_LOG_TRIVIAL(debug) << "Creating test frame with payload size: " << payload.size();
        MessageFrame frame;
        frame.message_type = type;
        frame.source_id = 1;
        frame.payload_size = payload.size();
        frame.filename_length = 8;  // Default test filename length

        // Generate test IV using std::fill instead of std::generate
        std::fill(frame.initialization_vector.begin(), 
                 frame.initialization_vector.end(), 
                 0x24);  // Test IV filled with 0x24

        // Set up payload stream
        frame.payload_stream = std::make_shared<std::stringstream>(payload);
        BOOST_LOG_TRIVIAL(debug) << "Test frame created with type: " << static_cast<int>(type);
        return frame;
    }

    // Helper to compare two message frames
    void verifyFramesEqual(const MessageFrame& original, const MessageFrame& deserialized) {
        BOOST_LOG_TRIVIAL(debug) << "Verifying frame equality";
        EXPECT_EQ(original.message_type, deserialized.message_type);
        EXPECT_EQ(original.source_id, deserialized.source_id);
        EXPECT_EQ(original.payload_size, deserialized.payload_size);
        EXPECT_EQ(original.filename_length, deserialized.filename_length);
        EXPECT_EQ(original.initialization_vector, deserialized.initialization_vector);

        if (original.payload_stream && deserialized.payload_stream) {
            std::string original_payload, deserialized_payload;
            original.payload_stream->seekg(0);
            deserialized.payload_stream->seekg(0);

            original_payload.resize(original.payload_size);
            deserialized_payload.resize(deserialized.payload_size);

            original.payload_stream->read(&original_payload[0], original.payload_size);
            deserialized.payload_stream->read(&deserialized_payload[0], deserialized.payload_size);

            EXPECT_EQ(original_payload, deserialized_payload);
            BOOST_LOG_TRIVIAL(debug) << "Payload comparison completed";
        } else {
            EXPECT_EQ(original.payload_stream == nullptr, deserialized.payload_stream == nullptr);
            BOOST_LOG_TRIVIAL(debug) << "One or both payloads are null";
        }
        BOOST_LOG_TRIVIAL(debug) << "Frame verification completed";
    }
};

// Test encryption functionality
TEST_F(CodecTest, EncryptionVerification) {
    BOOST_LOG_TRIVIAL(info) << "Starting EncryptionVerification test";
    MessageFrame original = createTestFrame("Sensitive Data");
    std::stringstream buffer;

    // Serialize (which encrypts)
    codec_->serialize(original, buffer);
    std::string serialized = buffer.str();
    BOOST_LOG_TRIVIAL(debug) << "Serialized data size: " << serialized.size();

    // Verify the sensitive data is not visible in plain text
    EXPECT_EQ(serialized.find("Sensitive Data"), std::string::npos);
    BOOST_LOG_TRIVIAL(debug) << "Verified sensitive data is not in plain text";

    // Deserialize and verify we can recover the original data
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);

    std::string recovered_payload;
    recovered_payload.resize(deserialized.payload_size);
    deserialized.payload_stream->read(&recovered_payload[0], deserialized.payload_size);
    BOOST_LOG_TRIVIAL(debug) << "Recovered payload size: " << recovered_payload.size();
    EXPECT_EQ(recovered_payload, "Sensitive Data");
    BOOST_LOG_TRIVIAL(info) << "EncryptionVerification test completed successfully";
}

// Test large message handling
TEST_F(CodecTest, LargeMessageHandling) {
    BOOST_LOG_TRIVIAL(info) << "Starting LargeMessageHandling test";
    // Create a large payload (1MB)
    std::string large_payload;
    large_payload.reserve(1024 * 1024);  // 1MB
    for (int i = 0; i < 1024 * 1024; i++) {
        large_payload += static_cast<char>(i % 256);
    }
    BOOST_LOG_TRIVIAL(debug) << "Created large payload of size: " << large_payload.size();

    MessageFrame original = createTestFrame(large_payload);
    std::stringstream buffer;

    // Serialize
    std::size_t bytes_written = codec_->serialize(original, buffer);
    BOOST_LOG_TRIVIAL(info) << "Serialized large message frame, bytes written: " << bytes_written;
    ASSERT_GT(bytes_written, large_payload.size());

    // Deserialize
    MessageFrame deserialized = codec_->deserialize(buffer, *channel_);
    BOOST_LOG_TRIVIAL(info) << "Deserialized large message frame, payload size: " << deserialized.payload_size;
    ASSERT_GT(deserialized.payload_size, 0);

    // Verify
    verifyFramesEqual(original, deserialized);
    BOOST_LOG_TRIVIAL(info) << "LargeMessageHandling test completed successfully";
}