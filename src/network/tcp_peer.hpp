#pragma once

#include "network/channel.hpp"
#include "network/codec.hpp"
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>
#include <netinet/in.h>  // For htonl/ntohl

namespace dfs {
namespace network {

class TCP_Peer {
public:
    using StreamProcessor = std::function<void(std::istream&)>;

    TCP_Peer(uint8_t peer_id, Channel& channel, const std::vector<uint8_t>& key);
    ~TCP_Peer();

    void set_stream_processor(StreamProcessor processor);
    bool start_stream_processing();
    void stop_stream_processing();
    bool send_stream(std::istream& input_stream, std::size_t buffer_size = 8192);
    bool send_message(const std::string& message);
    void cleanup_connection();
    uint8_t get_peer_id() const;
    boost::asio::ip::tcp::socket& get_socket();

private:
    void initialize_streams();
    std::istream* get_input_stream();
    void process_stream();
    void async_read_next();

    uint8_t peer_id_;
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    std::unique_ptr<boost::asio::streambuf> input_buffer_;
    std::unique_ptr<std::istream> input_stream_;
    std::unique_ptr<Codec> codec_;
    StreamProcessor stream_processor_;
    std::unique_ptr<std::thread> processing_thread_;
    std::atomic<bool> processing_active_{false};
    std::mutex io_mutex_;
    std::optional<boost::asio::ip::tcp::endpoint> endpoint_;
    uint32_t message_size_{0};  // Added for message framing
};

} // namespace network
} // namespace dfs
