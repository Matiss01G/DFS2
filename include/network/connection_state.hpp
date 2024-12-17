#ifndef DFS_CONNECTION_STATE_HPP
#define DFS_CONNECTION_STATE_HPP

#include <string>
#include <stdexcept>

namespace dfs {
namespace network {

class ConnectionState {
public:
    enum class State {
        INITIAL,
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        DISCONNECTED,
        ERROR
    };

    ConnectionState() : current_state_(State::INITIAL) {}

    // Get current state
    State get_state() const { return current_state_; }

    // Attempt to transition to a new state
    bool transition_to(State new_state) {
        if (!is_valid_transition(new_state)) {
            return false;
        }
        current_state_ = new_state;
        return true;
    }

    // Check if a transition is valid
    bool is_valid_transition(State new_state) const {
        switch (current_state_) {
            case State::INITIAL:
                return new_state == State::CONNECTING || 
                       new_state == State::ERROR;

            case State::CONNECTING:
                return new_state == State::CONNECTED || 
                       new_state == State::DISCONNECTED ||
                       new_state == State::ERROR;

            case State::CONNECTED:
                return new_state == State::DISCONNECTING || 
                       new_state == State::ERROR;

            case State::DISCONNECTING:
                return new_state == State::DISCONNECTED || 
                       new_state == State::ERROR;

            case State::DISCONNECTED:
                return new_state == State::CONNECTING || 
                       new_state == State::ERROR;

            case State::ERROR:
                return new_state == State::DISCONNECTED;
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

} // namespace network
} // namespace dfs

#endif // DFS_CONNECTION_STATE_HPP
