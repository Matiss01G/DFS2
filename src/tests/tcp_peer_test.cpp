#include <boost/test/unit_test.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include "network/tcp_peer.hpp"
#include "network/connection_state.hpp"

using namespace dfs::network;

// Test fixture for TCP_Peer tests
struct TCP_PeerTest {
    TCP_PeerTest() {
        // Set logging level to debug for testing
        boost::log::core::get()->set_filter(
            boost::log::trivial::severity >= boost::log::trivial::debug
        );
    }
    
    ~TCP_PeerTest() {
        // Reset logging level
        boost::log::core::get()->reset_filter();
    }
};

BOOST_FIXTURE_TEST_SUITE(tcp_peer_tests, TCP_PeerTest)

// Test TCP_Peer construction and initial state
BOOST_AUTO_TEST_CASE(test_construction) {
    TCP_Peer peer("test_peer");
    BOOST_CHECK_EQUAL(peer.get_connection_state(), ConnectionState::State::INITIAL);
}

// Test invalid state transitions
BOOST_AUTO_TEST_CASE(test_invalid_state_transitions) {
    TCP_Peer peer("test_peer");
    
    // Cannot connect without being in INITIAL state
    BOOST_CHECK_EQUAL(peer.get_connection_state(), ConnectionState::State::INITIAL);
    
    // Should not be able to disconnect from INITIAL state
    BOOST_CHECK(!peer.disconnect());
    BOOST_CHECK_EQUAL(peer.get_connection_state(), ConnectionState::State::INITIAL);
    
    // Should not be able to get streams when not connected
    BOOST_CHECK(!peer.get_input_stream());
    BOOST_CHECK(!peer.get_output_stream());
}

// Test connection failure logging
BOOST_AUTO_TEST_CASE(test_connection_failure_logging) {
    TCP_Peer peer("test_peer");
    
    // Attempt connection to invalid address to trigger error logging
    bool result = peer.connect("invalid.host.local", 12345);
    BOOST_CHECK(!result);
    
    // Should transition to ERROR state on connection failure
    BOOST_CHECK_EQUAL(peer.get_connection_state(), ConnectionState::State::ERROR);
}

// Test stream processor setting
BOOST_AUTO_TEST_CASE(test_stream_processor) {
    TCP_Peer peer("test_peer");
    
    // Set a dummy stream processor
    bool processor_called = false;
    peer.set_stream_processor([&processor_called](std::istream&) {
        processor_called = true;
    });
    
    // Cannot start processing without being connected
    BOOST_CHECK(!peer.start_stream_processing());
}

BOOST_AUTO_TEST_SUITE_END()
