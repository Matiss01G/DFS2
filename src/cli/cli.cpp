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

    iss >> command;
    if (command == "pwd" || command == "ls") {
      process_command(command, "");
    } else if (iss >> filename) {
      process_command(command, filename);
    } else {
      std::cout << "Invalid input. Usage: <command> [filename]" << std::endl;
    }

    if (running_) {
      std::cout << "Enter command and filename (or 'quit' to exit):" << std::endl;
    }
  }
  
  BOOST_LOG_TRIVIAL(info) << "CLI loop ended";
}

void CLI::process_command(const std::string& command, const std::string& filename) {
    BOOST_LOG_TRIVIAL(debug) << "Processing command: " << command << " with filename: " << filename;

    if (command == "read") {
        try {
            file_server_.get_file(filename);  
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error reading file: " << e.what();
            std::cout << "Error reading file: " << e.what() << std::endl;
        }
    }
    else if (command == "pwd" && filename.empty()) {
        store_.print_working_dir();
    }
    else if (command == "ls" && filename.empty()) {
        store_.list();
    }
    else if (command == "store") {
        std::ifstream file(filename);
        if (!file) {
            std::cout << "Error opening file: " << filename << std::endl;
            return;
        }
        file_server_.store_file(filename, file);
    }   
    else if (command == "cd") {
        try {
            store_.move_dir(filename);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error changing directory: " << e.what();
            std::cout << "Error changing directory: " << e.what() << std::endl;
        }
    }
    else if (command == "delete") {
        try {
            store_.delete_file(filename);
            std::cout << "File deleted successfully" << std::endl;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Error deleting file: " << e.what();
            std::cout << "Error deleting file: " << e.what() << std::endl;
        }
    } 
    else if (command == "connect") {
        // Parse IP and port from the filename parameter
        size_t colon_pos = filename.find(':');
        if (colon_pos == std::string::npos) {
            std::cout << "Invalid format. Usage: connect ip:port (e.g., connect 127.0.0.1:3002)" << std::endl;
            return;
        }

        std::string ip = filename.substr(0, colon_pos);
        std::string port_str = filename.substr(colon_pos + 1);

        try {
            uint16_t port = std::stoi(port_str);
            if (file_server_.connect(ip, port)) {
                std::cout << "Successfully connected to " << ip << ":" << port << std::endl;
            } else {
                std::cout << "Failed to connect to " << ip << ":" << port << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Invalid port number: " << port_str << std::endl;
            return;
        }
    }
    else {
        std::cout << "Unknown command or invalid arguments" << std::endl;
    }
}

} // namespace cli
} // namespace dfs
