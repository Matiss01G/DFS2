#ifndef DFS_NETWORK_FILE_SERVER_HPP
#define DFS_NETWORK_FILE_SERVER_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include "store/store.hpp"
#include "network/codec.hpp"
#include "network/message_frame.hpp"
#include "network/peer_manager.hpp"
#include "crypto/crypto_stream.hpp"

namespace dfs {
namespace network {

class FileServer {
public:
    // Constructor takes server ID and encryption key
    FileServer(uint32_t server_id, const std::vector<uint8_t>& key);

    // Virtual destructor for proper cleanup
    virtual ~FileServer() = default;

    // Handle incoming store message frame
    bool handle_store(const MessageFrame& frame);

    // Extract filename from message frame's payload stream
    std::string extract_filename(const MessageFrame& frame);

    // Prepare and send file to peers
    bool prepare_and_send(const std::string& filename, std::optional<uint32_t> peer_id = std::nullopt);

    // Store file locally and broadcast to peers
    bool store_file(const std::string& filename, std::stringstream& input);

    // Get file either from local store or network
    std::optional<std::stringstream> get_file(const std::string& filename);

private:
    uint32_t server_id_;
    std::vector<uint8_t> key_;
    std::unique_ptr<dfs::store::Store> store_;
    std::unique_ptr<Codec> codec_;
    std::shared_ptr<PeerManager> peer_manager_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_FILE_SERVER_HPP