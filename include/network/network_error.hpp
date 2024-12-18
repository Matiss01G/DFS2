#ifndef DFS_NETWORK_ERROR_HPP
#define DFS_NETWORK_ERROR_HPP

namespace dfs {
namespace network {

enum class NetworkError {
    SUCCESS = 0,
    CONNECTION_FAILED,
    CONNECTION_LOST,
    INVALID_PEER,
    TIMEOUT,
    TRANSFER_FAILED,
    UNKNOWN_ERROR
};

inline const char* network_error_to_string(NetworkError error) {
    switch (error) {
        case NetworkError::SUCCESS: return "Success";
        case NetworkError::CONNECTION_FAILED: return "Connection failed";
        case NetworkError::CONNECTION_LOST: return "Connection lost";
        case NetworkError::INVALID_PEER: return "Invalid peer";
        case NetworkError::TIMEOUT: return "Timeout";
        case NetworkError::TRANSFER_FAILED: return "Transfer failed";
        case NetworkError::UNKNOWN_ERROR: return "Unknown error";
        default: return "Undefined error";
    }
}

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_ERROR_HPP
