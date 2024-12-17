#include <gtest/gtest.h>
#include "network/connection_state.hpp"

using namespace dfs::network;

class ConnectionStateTest : public ::testing::Test {
protected:
    ConnectionState state;
};

// Test initial state
TEST_F(ConnectionStateTest, InitialState) {
    EXPECT_EQ(state.get_state(), ConnectionState::State::INITIAL);
    EXPECT_EQ(state.get_state_string(), "INITIAL");
}

// Test valid state transitions
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

// Test invalid state transitions
TEST_F(ConnectionStateTest, InvalidTransitions) {
    // Can't go directly from Initial to Connected
    EXPECT_FALSE(state.transition_to(ConnectionState::State::CONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::INITIAL);
    
    // Can't go from Initial to Disconnected
    EXPECT_FALSE(state.transition_to(ConnectionState::State::DISCONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::INITIAL);
    
    // Move to Connecting first
    EXPECT_TRUE(state.transition_to(ConnectionState::State::CONNECTING));
    
    // Can't go from Connecting to Disconnecting
    EXPECT_FALSE(state.transition_to(ConnectionState::State::DISCONNECTING));
    EXPECT_EQ(state.get_state(), ConnectionState::State::CONNECTING);
}

// Test error state handling
TEST_F(ConnectionStateTest, ErrorStateHandling) {
    // Any state can transition to ERROR
    EXPECT_TRUE(state.transition_to(ConnectionState::State::ERROR));
    EXPECT_EQ(state.get_state(), ConnectionState::State::ERROR);
    
    // From ERROR, can only go to DISCONNECTED
    EXPECT_FALSE(state.transition_to(ConnectionState::State::CONNECTING));
    EXPECT_TRUE(state.transition_to(ConnectionState::State::DISCONNECTED));
    EXPECT_EQ(state.get_state(), ConnectionState::State::DISCONNECTED);
}

// Test terminal state detection
TEST_F(ConnectionStateTest, TerminalStates) {
    // Initial state is not terminal
    EXPECT_FALSE(state.is_terminal());
    
    // ERROR state is terminal
    EXPECT_TRUE(state.transition_to(ConnectionState::State::ERROR));
    EXPECT_TRUE(state.is_terminal());
    
    // DISCONNECTED state is terminal
    EXPECT_TRUE(state.transition_to(ConnectionState::State::DISCONNECTED));
    EXPECT_TRUE(state.is_terminal());
    
    // CONNECTED state is not terminal
    state.transition_to(ConnectionState::State::CONNECTING);
    state.transition_to(ConnectionState::State::CONNECTED);
    EXPECT_FALSE(state.is_terminal());
}

// Test state to string conversion
TEST_F(ConnectionStateTest, StateToString) {
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::INITIAL), "INITIAL");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::CONNECTING), "CONNECTING");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::CONNECTED), "CONNECTED");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::DISCONNECTING), "DISCONNECTING");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::DISCONNECTED), "DISCONNECTED");
    EXPECT_EQ(ConnectionState::state_to_string(ConnectionState::State::ERROR), "ERROR");
}

// Main function is defined in network_test.cpp
