#ifndef DFS_NETWORK_PEER_HPP
#define DFS_NETWORK_PEER_HPP

#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include <streambuf>
#include "message_frame.hpp"

namespace dfs {
namespace network {

class Peer {
public:
    // Stream callback type for processing received data
    using StreamProcessor = std::function<void(std::istream&)>;

    
    // ---- CONSTRUCTOR AND DESTRUCTOR ----
    virtual ~Peer() = default;    

    
    // ---- STREAM CONTROL OPERATIONS ----
    virtual bool start_stream_processing() = 0;
    virtual void stop_stream_processing() = 0;
    

    // ---- OUTGOING DATA STREAM PROCESSING ----
    virtual bool send_message(const std::string& message, std::size_t total_size) = 0;
    virtual bool send_stream(std::istream& input_stream, std::size_t total_size, std::size_t buffer_size = 8192) = 0;
    

    // ---- GETTERS AND SETTERS ----
    virtual std::istream* get_input_stream() = 0;
    virtual void set_stream_processor(StreamProcessor processor) = 0;

protected:
    Peer() = default;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_PEER_HPP