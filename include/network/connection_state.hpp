#ifndef DFS_CONNECTION_STATE_HPP
#define DFS_CONNECTION_STATE_HPP

#include <string>

namespace dfs {
namespace network {

/**
 * ConnectionState class manages network connection states and their transitions.
 * Implements a state machine that enforces valid state transitions and prevents invalid ones.
 */
class ConnectionState {
public:
    /**
     * Network connection states:
     * INITIAL      - Starting state when object is created
     * CONNECTING   - Attempting to establish connection
     * CONNECTED    - Successfully connected
     * DISCONNECTING- In process of disconnecting
     * DISCONNECTED - Not connected, but can transition to CONNECTING
     * ERROR       - Error state, can only transition to DISCONNECTED
     */
    enum class State {
        INITIAL,
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        DISCONNECTED,
        ERROR
    };

    /**
     * Initialize connection state to INITIAL.
     */
    ConnectionState() : current_state_(State::INITIAL) {}

    /**
     * Get the current connection state.
     * @return Current state
     */
    State get_state() const { return current_state_; }

    /**
     * Check if the current state is a terminal state (ERROR or DISCONNECTED).
     * @return true if in terminal state
     */
    bool is_terminal() const {
        return current_state_ == State::ERROR || 
               current_state_ == State::DISCONNECTED;
    }

    /**
     * Attempt to transition to a new state.
     * @param new_state The target state
     * @return true if transition was successful, false if invalid
     */
    bool transition_to(State new_state) {
        if (!is_valid_transition(current_state_, new_state)) {
            return false;
        }
        current_state_ = new_state;
        return true;
    }

    // Check if a transition is valid
    static bool is_valid_transition(State from, State to) {
        switch (from) {
            case State::INITIAL:
                return to == State::CONNECTING || 
                       to == State::ERROR;

            case State::CONNECTING:
                return to == State::CONNECTED || 
                       to == State::DISCONNECTED ||
                       to == State::ERROR;

            case State::CONNECTED:
                return to == State::DISCONNECTING || 
                       to == State::ERROR;

            case State::DISCONNECTING:
                return to == State::DISCONNECTED || 
                       to == State::ERROR;

            case State::DISCONNECTED:
                return to == State::CONNECTING || 
                       to == State::ERROR;

            case State::ERROR:
                return to == State::DISCONNECTED;
        }
        return false;
    }

    // Convert state to string for logging
    static std::string state_to_string(State state) {
        switch (state) {
            case State::INITIAL:      return "INITIAL";
            case State::CONNECTING:   return "CONNECTING";
            case State::CONNECTED:    return "CONNECTED";
            case State::DISCONNECTING: return "DISCONNECTING";
            case State::DISCONNECTED: return "DISCONNECTED";
            case State::ERROR:        return "ERROR";
            default:                  return "UNKNOWN";
        }
    }

    // Get current state as string
    std::string get_state_string() const {
        return state_to_string(current_state_);
    }

private:
    State current_state_;
};

// Stream operator for ConnectionState::State to support test assertions and logging
inline std::ostream& operator<<(std::ostream& os, const ConnectionState::State& state) {
    os << ConnectionState::state_to_string(state);
    return os;
}

} // namespace network
} // namespace dfs

#endif // DFS_CONNECTION_STATE_HPP
