#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <map>
#include <sstream>
#include <mutex>
#include "network/bootstrap.hpp"
#include "network/peer_manager.hpp"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions/attr.hpp>

using namespace dfs::network;

// Custom sink that buffers logs by thread ID
class thread_grouped_sink : public boost::log::sinks::basic_formatted_sink_backend<
    char,
    boost::log::sinks::concurrent_feeding
> {
    mutable std::mutex mutex_;
    std::map<std::string, std::stringstream> thread_buffers_;

public:
    void consume(boost::log::record_view const& rec, string_type const& formatted_message) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Get thread ID using the correct attribute name
        auto thread_id_attr = rec.attribute_values()["ThreadID"].extract<boost::log::attributes::current_thread_id::value_type>();
        if (thread_id_attr) {
            std::string thread_id = boost::lexical_cast<std::string>(thread_id_attr.get());
            thread_buffers_[thread_id] << formatted_message << std::endl;
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [thread_id, buffer] : thread_buffers_) {
            std::cout << "\n=== Logs for Thread " << thread_id << " ===" << std::endl;
            std::cout << buffer.str();
        }
        thread_buffers_.clear();
    }
};

using sink_t = boost::log::sinks::synchronous_sink<thread_grouped_sink>;
boost::shared_ptr<sink_t> g_sink;

class BootstrapTest : public ::testing::Test {
protected:
    const std::string ADDRESS = "127.0.0.1";
    const std::vector<uint8_t> TEST_KEY = std::vector<uint8_t>(32, 0x42); // 32-byte test key

    void SetUp() override {
        if (!g_sink) {
            auto backend = boost::make_shared<thread_grouped_sink>();
            g_sink = boost::make_shared<sink_t>(backend);

            g_sink->set_formatter(
                boost::log::expressions::stream
                    << "["
                    << boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity")
                    << "] "
                    << boost::log::expressions::smessage
            );

            boost::log::core::get()->add_sink(g_sink);
            boost::log::core::get()->add_global_attribute(
                "ThreadID",
                boost::log::attributes::current_thread_id()
            );
        }
    }

    void TearDown() override {
        if (g_sink) {
            g_sink->flush();
        }
    }
};

TEST_F(BootstrapTest, PeerConnection) {
    const uint8_t PEER1_ID = 1, PEER2_ID = 2;
    const uint16_t PEER1_PORT = 3001, PEER2_PORT = 3002;

    std::vector<std::string> peer1_bootstrap_nodes = {};
    std::vector<std::string> peer2_bootstrap_nodes = {ADDRESS + ":3001"};

    Bootstrap peer1(ADDRESS, PEER1_PORT, TEST_KEY, PEER1_ID, peer1_bootstrap_nodes);
    Bootstrap peer2(ADDRESS, PEER2_PORT, TEST_KEY, PEER2_ID, peer2_bootstrap_nodes);

    std::thread peer1_thread([&peer1]() {
        ASSERT_TRUE(peer1.start()) << "Failed to start peer 1";
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::thread peer2_thread([&peer2]() {
        ASSERT_TRUE(peer2.start()) << "Failed to start peer 2";
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));

    BOOST_LOG_TRIVIAL(info) << "Getting peer managers";

    auto& peer1_manager = peer1.get_peer_manager();
    auto& peer2_manager = peer2.get_peer_manager();

    BOOST_LOG_TRIVIAL(info) << "Checking for peer existence in peers map";
    ASSERT_TRUE(peer1_manager.has_peer(PEER2_ID));
    ASSERT_TRUE(peer2_manager.has_peer(PEER1_ID));
    EXPECT_TRUE(peer1_manager.is_connected(PEER2_ID));
    EXPECT_TRUE(peer2_manager.is_connected(PEER1_ID));

    peer1_thread.join();
    peer2_thread.join();
}