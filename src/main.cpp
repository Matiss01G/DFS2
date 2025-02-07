#include "network/bootstrap.hpp"
#include <vector>
#include <iostream>
#include <string>
#include <random>
#include <unordered_map>

struct ProgramOptions {
  std::string host;
  uint16_t port{0};
  bool valid{false};
};

uint32_t generate_random_peer_id() {
  std::random_device rd;  // Used to obtain a seed for the random number engine
  std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<uint32_t> dis(1, std::numeric_limits<uint32_t>::max());
  return dis(gen);
}

void print_usage(const std::string& program_name) {
  std::cerr << "Usage: " << program_name << " -h <host> -p <port>\n"
        << "Required arguments:\n"
        << "  -h, --host    Host address\n"
        << "  -p, --port    Port number\n"
        << "Example: " << program_name << " -h 127.0.0.1 -p 3001\n";
}

ProgramOptions parse_command_line(int argc, char* argv[]) {
  const std::unordered_map<std::string, std::string*> flag_map = {
    {"-h", nullptr},
    {"--host", nullptr},
    {"-p", nullptr},
    {"--port", nullptr}
  };

  ProgramOptions options;
  std::string host;
  std::string port_str;

  for (int i = 1; i < argc - 1; i += 2) {
    const std::string flag(argv[i]);
    const std::string value(argv[i + 1]);

    if (flag_map.count(flag) == 0) {
      std::cerr << "Error: Unknown argument: " << flag << '\n';
      print_usage(argv[0]);
      return options;
    }

    if (flag == "-h" || flag == "--host") {
      options.host = value;
    } else if (flag == "-p" || flag == "--port") {
      try {
        options.port = static_cast<uint16_t>(std::stoi(value));
      } catch (...) {
        std::cerr << "Error: Invalid port number\n";
        print_usage(argv[0]);
        return options;
      }
    }
  }

  if (options.host.empty() || options.port == 0) {
    std::cerr << "Error: Both host and port are required\n";
    print_usage(argv[0]);
    return options;
  }

  options.valid = true;
  return options;
}

bool run_bootstrap(const std::string& host, uint16_t port) {
  try {
    std::vector<uint8_t> KEY(32, 0x42);
    uint32_t peer_id = generate_random_peer_id();
    dfs::network::Bootstrap peer(host, port, KEY, peer_id, {});  // Using random peer_id instead of 1
    dfs::cli::CLI cli(peer.get_file_server().get_store(), peer.get_file_server());

    if (!peer.start()) {
      std::cerr << "Error: Failed to start bootstrap\n";
      return false;
    }

    cli.run();
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Error: Failed to start bootstrap: " << e.what() << '\n';
    return false;
  }
}

int main(int argc, char* argv[]) {
  if (const auto options = parse_command_line(argc, argv); !options.valid) {
    return 1;
  } else if (!run_bootstrap(options.host, options.port)) {
    return 1;
  }
  return 0;
}