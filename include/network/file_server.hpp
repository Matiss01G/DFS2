#ifndef DFS_NETWORK_FILE_SERVER_HPP
#define DFS_NETWORK_FILE_SERVER_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include "store/store.hpp"
#include "network/codec.hpp"

namespace dfs {
namespace network {

class FileServer {
public:
    // Constructor takes server ID and encryption key
    FileServer(uint32_t server_id, const std::vector<uint8_t>& key);

    // Virtual destructor for proper cleanup
    virtual ~FileServer() = default;

private:
    uint32_t server_id_;
    std::vector<uint8_t> key_;
    std::unique_ptr<dfs::store::Store> store_;
    std::unique_ptr<Codec> codec_;
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_FILE_SERVER_HPP