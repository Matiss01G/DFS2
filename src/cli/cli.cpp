#include "cli/cli.hpp"
#include <iostream>
#include <sstream>
#include <boost/log/trivial.hpp>

namespace dfs {
namespace cli {

//==============================================
// CONSTRUCTOR AND DESTRUCTOR
//==============================================
  
CLI::CLI(store::Store& store, network::FileServer& file_server)
  : store_(store)
  , file_server_(file_server)
  , running_(false) {
  BOOST_LOG_TRIVIAL(info) << "CLI initialized";
}


//==============================================
// STARTUP
//==============================================
void CLI::run() {
  running_ = true;
  std::string line;
  
  BOOST_LOG_TRIVIAL(info) << "Starting CLI loop";
  std::cout << "DFS_Shell> " << std::flush;
  
  while (running_ && std::getline(std::cin, line)) {
  if (line == "quit") {
  running_ = false;
  continue;
  }

  std::istringstream iss(line);
  std::string command, filename;

  iss >> command;
  if (command == "pwd" || command == "ls" || command == "help") {
  process_command(command, "");
  } else if (iss >> filename) {
  process_command(command, filename);
  } else {
  std::cout << "Invalid input. Usage: <command> [filename]" << std::endl;
  }

  if (running_) {
  std::cout << "DFS_Shell> " << std::flush;
  }
  }
  
  BOOST_LOG_TRIVIAL(info) << "CLI loop ended";
}


//==============================================
// COMMAND PROCESSING 
//==============================================

void CLI::process_command(const std::string& command, const std::string& filename) {
  BOOST_LOG_TRIVIAL(debug) << "Processing command: " << command << " with filename: " << filename;

  if (command == "read") {
    handle_read_command(filename);
  }
  else if (command == "pwd" && filename.empty()) {
    store_.print_working_dir();
  }
  else if (command == "ls" && filename.empty()) {
    store_.list();
  }
  else if (command == "help" && filename.empty()) {
    handle_help_command();
  }
  else if (command == "store") {
    handle_store_command(filename);
  }
  else if (command == "cd") {
    try {
      store_.move_dir(filename);
    } catch (const std::exception& e) {
      log_and_display_error("Error changing directory", e.what());
    }
  }
  else if (command == "delete") {
    handle_delete_command(filename);
  }
  else if (command == "connect") {
    handle_connect_command(filename);
  }
  else {
    std::cout << "Unknown command or invalid arguments" << std::endl;
  }
}
void CLI::handle_read_command(const std::string& filename) {
  try {
    file_server_.get_file(filename);  
  } catch (const std::exception& e) {
    log_and_display_error("Error reading file", e.what());
  }
}

void CLI::handle_store_command(const std::string& filename) {
  std::ifstream file(filename);
  if (!file) {
    std::cout << "Error opening file: " << filename << std::endl;
    return;
  }
  file_server_.store_file(filename, file);
}

void CLI::handle_connect_command(const std::string& connection_string) {
  size_t colon_pos = connection_string.find(':');
  if (colon_pos == std::string::npos) {
    std::cout << "Invalid format. Usage: connect ip:port (e.g., connect 127.0.0.1:3002)" << std::endl;
    return;
  }

  std::string ip = connection_string.substr(0, colon_pos);
  std::string port_str = connection_string.substr(colon_pos + 1);

  try {
    uint16_t port = std::stoi(port_str);
    bool success = file_server_.connect(ip, port);
    std::cout << (success ? "Successfully connected to " : "Failed to connect to ")
         << ip << ":" << port << std::endl;
  } catch (const std::exception& e) {
    std::cout << "Invalid port number: " << port_str << std::endl;
  }
}

void CLI::handle_delete_command(const std::string& filename) {
  try {
    store_.delete_file(filename);
    std::cout << "File deleted successfully" << std::endl;
  } catch (const std::exception& e) {
    log_and_display_error("Error deleting file", e.what());
  }
}

void CLI::handle_help_command() {
  std::cout << "Available commands:" << std::endl;
  std::cout << "  help              Display this help message" << std::endl;
  std::cout << "  pwd               Print current working directory" << std::endl;
  std::cout << "  ls                List files in current directory" << std::endl;
  std::cout << "  cd <dir>          Change to directory <dir>" << std::endl;
  std::cout << "  read <file>       Read contents of <file>" << std::endl;
  std::cout << "  store <file>      Store local <file> in DFS" << std::endl;
  std::cout << "  delete <file>     Delete <file> from DFS" << std::endl;
  std::cout << "  connect <ip:port> Connect to DFS server at <ip:port>" << std::endl;
  std::cout << "  quit              Exit the DFS shell" << std::endl << std::endl;
}

void CLI::log_and_display_error(const std::string& message, const std::string& error) {
  BOOST_LOG_TRIVIAL(error) << message << ": " << error;
  std::cout << message << ": " << error << std::endl;
}

} // namespace cli
} // namespace dfs
