#include "network/bootstrap.hpp"
#include <vector>

int main() {
  std::string ADDRESS = "127.0.0.1";
  std::vector<uint8_t> KEY(32, 0x42);  
  
  uint16_t port = 3000;
  uint8_t id = 1;
  std::vector<std::string> bootstrap_nodes;  

  dfs::network::Bootstrap peer1(ADDRESS, port, KEY, id, bootstrap_nodes);

  // Create CLI instance with store and file server from peer1
  dfs::cli::CLI cli(peer1.get_file_server().get_store(), peer1.get_file_server());

  peer1.start();
  cli.run();

  return 0;
}