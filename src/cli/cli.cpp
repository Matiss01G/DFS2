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
      store_.read_file(filename);  
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Error reading file: " << e.what();
      std::cout << "Error reading file: " << e.what() << std::endl;
    }
  } else if (command == "pwd" && filename.empty()) {
    store_.print_working_dir();
  } else if (command == "ls" && filename.empty()) {
    store_.list();
  } else {
    std::cout << "Unknown command or invalid arguments" << std::endl;
  }
}

} // namespace cli
} // namespace dfs
