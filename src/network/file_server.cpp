#include "network/file_server.hpp"
#include <boost/log/trivial.hpp>
#include <filesystem>

namespace dfs {
namespace network {

FileServer::FileServer(uint32_t server_id, const std::vector<uint8_t>& key)
    : server_id_(server_id)
    , key_(key) {
    
    BOOST_LOG_TRIVIAL(info) << "Initializing FileServer with ID: " << server_id_;
    
    // Create store directory based on server ID
    std::string store_path = "fileserver_" + std::to_string(server_id_);
    
    try {
        // Initialize store with the server-specific directory
        store_ = std::make_unique<dfs::store::Store>(store_path);
        
        // Initialize codec with the provided cryptographic key
        codec_ = std::make_unique<Codec>(key_);
        
        BOOST_LOG_TRIVIAL(info) << "FileServer initialization complete";
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to initialize FileServer: " << e.what();
        throw;
    }
}

} // namespace network
} // namespace dfs
