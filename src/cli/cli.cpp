#include "cli/cli.hpp"
#include <iostream>
#include <sstream>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace cli {

CLI::CLI(store::Store& store, network::FileServer& file_server)
    : store_(store)
    , file_server_(file_server)
    , running_(false) {
    BOOST_LOG_TRIVIAL(info) << "CLI initialized";
}

void CLI::run() {
    running_ = true;
    std::string line;
    
    BOOST_LOG_TRIVIAL(info) << "Starting CLI loop";
    std::cout << "DFS CLI started. Enter command and filename (or 'quit' to exit):" << std::endl;
    
    while (running_ && std::getline(std::cin, line)) {
        if (line == "quit") {
            running_ = false;
            continue;
        }
        
        std::istringstream iss(line);
        std::string command, filename;
        
        if (iss >> command >> filename) {
            process_command(command, filename);
        } else {
            std::cout << "Invalid input. Usage: <command> <filename>" << std::endl;
        }
        
        if (running_) {
            std::cout << "Enter command and filename (or 'quit' to exit):" << std::endl;
        }
    }
    
    BOOST_LOG_TRIVIAL(info) << "CLI loop ended";
}

void CLI::process_command(const std::string& command, const std::string& filename) {
    BOOST_LOG_TRIVIAL(debug) << "Processing command: " << command << " with filename: " << filename;
    // Command processing will be implemented later
    std::cout << "Received command: " << command << " for file: " << filename << std::endl;
}

} // namespace cli
} // namespace dfs
