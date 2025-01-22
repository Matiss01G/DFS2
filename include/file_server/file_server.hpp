#ifndef DFS_NETWORK_FILE_SERVER_HPP
#define DFS_NETWORK_FILE_SERVER_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <optional>
#include "store/store.hpp"
#include "network/codec.hpp"
#include "network/message_frame.hpp"
#include "network/channel.hpp"
#include "crypto/crypto_stream.hpp"

namespace dfs {
namespace network {

// Forward declaration
class PeerManager;

class FileServer {
public:
    // Constructor takes server ID, encryption key, PeerManager reference and reference to shared channel
    FileServer(uint32_t ID, const std::vector<uint8_t>& key, PeerManager& peer_manager, Channel& channel);

    // Virtual destructor for proper cleanup
    virtual ~FileServer() = default;

    // Extract filename from message frame's payload stream
    std::string extract_filename(const MessageFrame& frame);

    // Prepare and send file to peers with specified message type
    bool prepare_and_send(const std::string& filename, MessageType message_type, std::optional<std::string> peer_id = std::nullopt);

    // Store file locally and broadcast to peers
    bool store_file(const std::string& filename, std::stringstream& input);

    // Get file either from local store or network
    std::optional<std::stringstream> get_file(const std::string& filename);

    // Handle incoming store message frame
    bool handle_store(const MessageFrame& frame);

    // Handle incoming get message frame
    bool handle_get(const MessageFrame& frame);

private:
    uint32_t ID_;
    std::vector<uint8_t> key_;
    std::unique_ptr<dfs::store::Store> store_;
    std::unique_ptr<Codec> codec_;
    Channel& channel_;
    PeerManager& peer_manager_;  // Added PeerManager reference

    // Channel listener continuously checks for messages in the channel queue
    void channel_listener();

    // Message handler routes messages to appropriate handlers based on type
    void message_handler(const MessageFrame& frame);
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_FILE_SERVER_HPP