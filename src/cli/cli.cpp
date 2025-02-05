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
    std::cout << "Available commands: read <filename>" << std::endl;

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
            std::cout << "Available commands: read <filename>" << std::endl;
        }
    }

    BOOST_LOG_TRIVIAL(info) << "CLI loop ended";
}

void CLI::process_command(const std::string& command, const std::string& filename) {
    BOOST_LOG_TRIVIAL(debug) << "Processing command: " << command << " with filename: " << filename;

    try {
        if (command == "read") {
            BOOST_LOG_TRIVIAL(info) << "Executing read command for file: " << filename;
            store_.read_file(filename, 20); // Display 20 lines per page
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Available commands: read <filename>" << std::endl;
        }
    } catch (const store::StoreError& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing command: " << e.what();
        std::cout << "Error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Unexpected error processing command: " << e.what();
        std::cout << "Unexpected error: " << e.what() << std::endl;
    }
}

} // namespace cli
} // namespace dfs