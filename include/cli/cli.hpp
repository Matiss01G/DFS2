#pragma once

#include <string>
#include "store/store.hpp"
#include "file_server/file_server.hpp"

namespace dfs {
namespace cli {

class CLI {
public:
    CLI(store::Store& store, network::FileServer& file_server);
    void run();
    
private:
    void process_command(const std::string& command, const std::string& filename);
    bool running_;
    store::Store& store_;
    network::FileServer& file_server_;
};

} // namespace cli
} // namespace dfs
