#include <gtest/gtest.h>
#include "network/connection_state.hpp"

using namespace dfs::network;

class ConnectionStateTest : public ::testing::Test {
protected:
    ConnectionState state;
};

TEST_F(ConnectionStateTest, InitialState) {
    EXPECT_EQ(state.get_state(), ConnectionState::State::INITIAL);
    EXPECT_EQ(state.get_state_string(), "INITIAL");
}

TEST_F(ConnectionStateTest, ValidTransitions) {
    // Initial -> Connecting
    EXPECT_TRUE(state.transition_to(ConnectionState::State::CONNECTING));
    EXPECT_EQ(state.get_state(), ConnectionState::State::CONNECTING);

    // Connecting -> Connected
    EXPECT_TRUE(state.transition_to(ConnectionState::State::CONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::CONNECTED);

    // Connected -> Disconnecting
    EXPECT_TRUE(state.transition_to(ConnectionState::State::DISCONNECTING));
    EXPECT_EQ(state.get_state(), ConnectionState::State::DISCONNECTING);

    // Disconnecting -> Disconnected
    EXPECT_TRUE(state.transition_to(ConnectionState::State::DISCONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::DISCONNECTED);

    // Disconnected -> Connecting (reconnection)
    EXPECT_TRUE(state.transition_to(ConnectionState::State::CONNECTING));
    EXPECT_EQ(state.get_state(), ConnectionState::State::CONNECTING);
}

TEST_F(ConnectionStateTest, InvalidTransitions) {
    // Can't go from Initial to Connected directly
    EXPECT_FALSE(state.transition_to(ConnectionState::State::CONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::INITIAL);

    // Can't go from Initial to Disconnected directly
    EXPECT_FALSE(state.transition_to(ConnectionState::State::DISCONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::INITIAL);

    // Move to a valid state first
    EXPECT_TRUE(state.transition_to(ConnectionState::State::CONNECTING));
    
    // Can't go from Connecting to Disconnecting directly
    EXPECT_FALSE(state.transition_to(ConnectionState::State::DISCONNECTING));
    EXPECT_EQ(state.get_state(), ConnectionState::State::CONNECTING);
}

TEST_F(ConnectionStateTest, ErrorStateTransitions) {
    // Any state can transition to ERROR
    EXPECT_TRUE(state.transition_to(ConnectionState::State::ERROR));
    EXPECT_EQ(state.get_state(), ConnectionState::State::ERROR);

    // From ERROR, can only go to DISCONNECTED
    EXPECT_FALSE(state.transition_to(ConnectionState::State::CONNECTING));
    EXPECT_TRUE(state.transition_to(ConnectionState::State::DISCONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::DISCONNECTED);
}

TEST_F(ConnectionStateTest, StateToString) {
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::INITIAL), "INITIAL");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::CONNECTING), "CONNECTING");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::CONNECTED), "CONNECTED");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::DISCONNECTING), "DISCONNECTING");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::DISCONNECTED), "DISCONNECTED");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::ERROR), "ERROR");
}

// Main function is defined in network_test.cpp
