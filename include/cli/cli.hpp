#pragma once

#include <string>
#include "store/store.hpp"
#include "file_server/file_server.hpp"

namespace dfs {
namespace cli {

class CLI {
public:
    // ---- CONSTRUCTOR AND DESTRUCTOR ----
    CLI(store::Store& store, network::FileServer& file_server);


    // ---- STARTUP ----
    void run();

private:
    // ---- PARAMETERS ----
    bool running_;
    // System components
    store::Store& store_;
    network::FileServer& file_server_;


    // ---- COMMAND PROCESSING ----
    void process_command(const std::string& command, const std::string& filename);
    void handle_read_command(const std::string& filename);
    void handle_store_command(const std::string& filename);
    void handle_connect_command(const std::string& connection_string);
    void handle_delete_command(const std::string& filename);
    void log_and_display_error(const std::string& message, const std::string& error);

    
};

} // namespace cli
} // namespace dfs
