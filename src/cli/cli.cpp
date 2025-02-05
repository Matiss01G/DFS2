#include "cli/cli.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace cli {

CLI::CLI(store::Store& store, network::FileServer& file_server)
    : running_(true)
    , store_(store)
    , file_server_(file_server) {
    BOOST_LOG_TRIVIAL(info) << "CLI initialized";
}

void CLI::run() {
    std::string line, command, filename;
    
    BOOST_LOG_TRIVIAL(info) << "Starting CLI. Enter commands (or 'quit' to exit)";
    
    while (running_) {
        std::cout << "dfs> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        std::istringstream iss(line);
        if (!(iss >> command >> filename)) {
            if (command == "quit") {
                running_ = false;
                break;
            }
            std::cout << "Invalid command format. Use: <command> <filename>" << std::endl;
            continue;
        }
        
        if (command == "quit") {
            running_ = false;
            break;
        }
        
        process_command(command, filename);
    }
    
    BOOST_LOG_TRIVIAL(info) << "CLI shutdown";
}

void CLI::process_command(const std::string& command, const std::string& filename) {
    // Command processing will be implemented later
    BOOST_LOG_TRIVIAL(info) << "Command received: " << command << " " << filename;
    std::cout << "Command '" << command << "' with file '" << filename << "' received" << std::endl;
}

} // namespace cli
} // namespace dfs
