#pragma once

#include <iostream>
#include <mutex>
#include <boost/endian/conversion.hpp>
#include "network/message_frame.hpp"
#include "channel/channel.hpp"

namespace dfs {
namespace network {

class Codec {
public:
    /**
     * Serializes a MessageFrame to an output stream in network byte order
     * 
     * @param frame The MessageFrame to serialize
     * @param output The output stream to write to
     * @return Number of bytes written
     * @throws std::runtime_error if serialization fails
     */
    std::size_t serialize(const MessageFrame& frame, std::ostream& output);

    /**
     * Deserializes a MessageFrame from an input stream in network byte order
     * 
     * @param input The input stream to read from
     * @param channel The channel to push the deserialized frame to
     * @return The deserialized MessageFrame
     * @throws std::runtime_error if deserialization fails
     */
    MessageFrame deserialize(std::istream& input, Channel& channel);

private:
    std::mutex mutex_; // Protects channel access during deserialization

    // Network byte order conversion helpers
    template<typename T>
    T to_network_order(T host_value) {
        return boost::endian::native_to_big(host_value);
    }

    template<typename T>
    T from_network_order(T network_value) {
        return boost::endian::big_to_native(network_value);
    }

    // Stream helpers
    void write_bytes(std::ostream& output, const void* data, std::size_t size);
    void read_bytes(std::istream& input, void* data, std::size_t size);
};

} // namespace network
} // namespace dfs
