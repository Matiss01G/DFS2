#pragma once

#include "network/message_frame.hpp"
#include "network/channel.hpp"
#include <vector>
#include <string>
#include <memory>
#include <istream>
#include <ostream>

namespace dfs {
namespace network {

class Codec {
public:
    explicit Codec(const std::vector<uint8_t>& key) : key_(key) {}
    Codec(const std::vector<uint8_t>& key, Channel& channel) : key_(key) {}

    std::size_t serialize(const MessageFrame& frame, std::ostream& output);
    MessageFrame deserialize(std::istream& input, Channel& channel, const std::string& source_id);

private:
    void write_bytes(std::ostream& output, const void* data, std::size_t size);
    void read_bytes(std::istream& input, void* data, std::size_t size);

    std::vector<uint8_t> key_;
};

} // namespace network
} // namespace dfs