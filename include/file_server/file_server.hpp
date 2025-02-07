#ifndef DFS_NETWORK_FILE_SERVER_HPP
#define DFS_NETWORK_FILE_SERVER_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <sstream>
#include <optional>
#include "store/store.hpp"
#include "network/codec.hpp"
#include "network/message_frame.hpp"
#include "network/channel.hpp"
#include "crypto/crypto_stream.hpp"
#include "utils/pipeliner.hpp"
#include "network/tcp_server.hpp" 

namespace dfs {
namespace network {

// Forward declaration
class PeerManager;
class Pipeliner;

class FileServer {
public:
  // ---- CONSTRUCTOR AND DESTRUCTOR ----
  FileServer(uint32_t ID, const std::vector<uint8_t>& key, PeerManager& peer_manager, Channel& channel, TCP_Server& tcp_server);
  virtual ~FileServer();


  // Connects to a remote endpoint 
  bool connect(const std::string& remote_address, uint16_t remote_port);
  
  
  // ---- PROCESSING OF USER REQUESTS ----
  bool store_file(const std::string& filename, std::istream& input);
  bool get_file(const std::string& filename);

  
  // ---- GETTERS ----
  dfs::store::Store& get_store() { return *store_; }
  
private:
  // ---- PARAMETERS ----
  uint32_t ID_;
  std::vector<uint8_t> key_;
  std::unique_ptr<dfs::store::Store> store_;
  std::unique_ptr<Codec> codec_;
  Channel& channel_;
  PeerManager& peer_manager_;  
  TCP_Server& tcp_server_;
  std::mutex mutex_;
  std::atomic<bool> running_{true};
  std::unique_ptr<std::thread> listener_thread_;

  
  // ---- PROCESSING OF OUTGOING DATA ----
  // Prepare and send file to peers with specified message type
  bool prepare_and_send(const std::string& filename, MessageType message_type, std::optional<uint8_t> peer_id = std::nullopt);
  // Creates MessageFrame with appropriate metadata and IV
  MessageFrame create_message_frame(const std::string& filename, MessageType message_type);
  // Creates producer function to handle file content streaming based on message type
  std::function<bool(std::stringstream&)> create_producer(const std::string& filename, MessageType message_type);
  // Creates transform function to serialize message frame data
  std::function<bool(std::stringstream&, std::stringstream&)> create_transform(
    MessageFrame& frame, 
    utils::Pipeliner* pipeline);
  // Handles sending pipeline data to specific peer or broadcasting
  bool send_pipeline(dfs::utils::Pipeliner* const& pipeline, std::optional<uint8_t> peer_id);

  
  // ---- PROCESSING OF INCOMING DATA ----
  // Channel listener continuously checks for messages in the channel queue
  void channel_listener();
  // Message handler routes messages to appropriate handlers based on type
  void message_handler(const MessageFrame& frame);
  // Handle incoming store/get message frames
  bool handle_store(const MessageFrame& frame);
  bool handle_get(const MessageFrame& frame);
  // Extract filename from message frame's payload stream
  std::string extract_filename(const MessageFrame& frame);

  
  // Called by get_file to retrieve file from store/network
  bool read_from_local_store(const std::string& filename);
  bool retrieve_from_network(const std::string& filename);
};

} // namespace network
} // namespace dfs

#endif // DFS_NETWORK_FILE_SERVER_HPP