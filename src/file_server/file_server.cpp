#include "file_server/file_server.hpp"
#include <boost/log/trivial.hpp>
#include <filesystem>
#include <optional>
#include <thread>
#include <chrono>
#include "network/peer_manager.hpp"
#include <memory>
#include <functional>
#include "utils/pipeliner.hpp"

namespace dfs {
namespace network {

FileServer::FileServer(uint32_t ID, const std::vector<uint8_t>& key, PeerManager& peer_manager, Channel& channel, TCP_Server& tcp_server)
  : ID_(ID)
  , key_(key)
  , channel_(channel)
  , peer_manager_(peer_manager) 
  , tcp_server_(tcp_server) {  

  // Validate key size (32 bytes for AES-256)
  if (key_.empty() || key_.size() != 32) {
    BOOST_LOG_TRIVIAL(error) << "File server: Invalid key size: " << key_.size() << " bytes. Expected 32 bytes.";
    throw std::invalid_argument("File server: Invalid cryptographic key size");
  }

  BOOST_LOG_TRIVIAL(info) << "File server: Initializing FileServer with ID: " << ID_;

  try {
    // Create store directory based on server ID
    std::string store_path = "File server: fileserver_" + std::to_string(ID_);

    // Initialize store with the server-specific directory
    store_ = std::make_unique<dfs::store::Store>(store_path);

    // Initialize codec with the provided cryptographic key and channel reference
    codec_ = std::make_unique<Codec>(key_, channel);

    // Start the channel listener thread
    listener_thread_ = std::make_unique<std::thread>(&FileServer::channel_listener, this);

    BOOST_LOG_TRIVIAL(info) << "File server: FileServer initialization complete";
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Failed to initialize FileServer: " << e.what();
    throw;
  }
}

FileServer::~FileServer() {
    running_ = false;
    if (listener_thread_ && listener_thread_->joinable()) {
        listener_thread_->join();
    }
}

bool FileServer::connect(const std::string& remote_address, uint16_t remote_port) {
  BOOST_LOG_TRIVIAL(info) << "File server: Initiating connection to " << remote_address << ":" << remote_port;

  try {
    if (!tcp_server_.connect(remote_address, remote_port)) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to connect to " << remote_address << ":" << remote_port;
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "File server: Successfully connected to " << remote_address << ":" << remote_port;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Connection error: " << e.what();
    return false;
  }
}

std::string FileServer::extract_filename(const MessageFrame& frame) {
  if (!frame.payload_stream) {
    BOOST_LOG_TRIVIAL(error) << "File server: Invalid payload stream in message frame";
    throw std::runtime_error("File server: Invalid payload stream");
  }

  if (frame.filename_length == 0) {
    BOOST_LOG_TRIVIAL(error) << "File server: Invalid filename length: 0";
    throw std::runtime_error("File server: Invalid filename length");
  }

  try {
    // Create a buffer to hold the filename
    std::vector<char> filename_buffer(frame.filename_length);

    // Read the filename from the start of payload
    frame.payload_stream->seekg(0);
    if (!frame.payload_stream->read(filename_buffer.data(), frame.filename_length)) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to read filename from payload stream";
      throw std::runtime_error("File server: Failed to read filename");
    }

    // Convert to string
    std::string filename(filename_buffer.begin(), filename_buffer.end());

    BOOST_LOG_TRIVIAL(debug) << "File server: Successfully extracted filename: " << filename;
    return filename;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Error extracting filename: " << e.what();
    throw;
  }
}

MessageFrame FileServer::create_message_frame(const std::string& filename, MessageType message_type) {
  // Initialize basic frame metadata
  MessageFrame frame;
  frame.message_type = message_type;
  frame.source_id = ID_;
  frame.filename_length = filename.length();

  // Generate cryptographic IV for this message
  crypto::CryptoStream crypto_stream;
  auto iv = crypto_stream.generate_IV();
  frame.iv_.assign(iv.begin(), iv.end());

  return frame;
}

std::function<bool(std::stringstream&)> FileServer::create_producer(
  const std::string& filename, MessageType message_type) {

  if (message_type == MessageType::GET_FILE) {
      // For GET_FILE, producer only writes filename (no file content needed)
      return [filename, first_read = true](std::stringstream& output) mutable -> bool {
          if (!first_read) return false;  // Only write once
          output.write(filename.c_str(), filename.length());
          first_read = false;
          return output.good();
      };
  }

  // For other types (e.g., STORE_FILE), producer writes both filename and file content
  return [this, filename, first_read = true](std::stringstream& output) mutable -> bool {
      if (!first_read) return false;  // Only read once
      output.write(filename.c_str(), filename.length());  // Write filename first
      store_->get(filename, output);   // Then append file content
      first_read = false;
      return output.good();
  };
}

std::function<bool(std::stringstream&, std::stringstream&)> FileServer::create_transform(
    MessageFrame& frame,
    utils::Pipeliner* pipeline) {
    // Capture pipeline by value since it's a pointer
    return [this, &frame, pipeline](std::stringstream& input, std::stringstream& output) -> bool {
        frame.payload_stream = std::make_shared<std::stringstream>();
        *frame.payload_stream << input.rdbuf();

        frame.payload_stream->seekp(0, std::ios::end);
        frame.payload_size = frame.payload_stream->tellp();
        frame.payload_stream->seekg(0);

        std::size_t total_serialized_size = codec_->serialize(frame, output);
        pipeline->set_total_size(total_serialized_size);

        return true;
    };
}
bool FileServer::send_pipeline(dfs::utils::Pipeliner* const& pipeline, std::optional<uint8_t> peer_id) {
    if (peer_id) {
        BOOST_LOG_TRIVIAL(debug) << "File server: Sending to peer: " << static_cast<int>(*peer_id);
        return peer_manager_.send_to_peer(*peer_id, *pipeline);
    }

    BOOST_LOG_TRIVIAL(debug) << "File server: Broadcasting to all peers";
    return peer_manager_.broadcast_stream(*pipeline);
}

bool FileServer::prepare_and_send(const std::string& filename, MessageType message_type, 
                              std::optional<uint8_t> peer_id) {
  try {
      BOOST_LOG_TRIVIAL(info) << "File server: Preparing file: " << filename 
                             << " for " << (peer_id ? "peer " + std::to_string(*peer_id) : "broadcast")
                             << " with message type: " << static_cast<int>(message_type);

      // Create pipeline and components
      auto frame = create_message_frame(filename, message_type);
      auto producer = create_producer(filename, message_type);
      auto pipeline = utils::Pipeliner::create(producer);
      auto transform = create_transform(frame, pipeline.get());

      // Configure pipeline with 1MB buffer
      pipeline->transform(transform);
      pipeline->set_buffer_size(1024 * 1024);  // 1MB buffer for efficient streaming
      pipeline->flush();  // Ensure all data is processed

      // Send data and handle any failures
      if (!send_pipeline(pipeline.get(), peer_id)) {
          BOOST_LOG_TRIVIAL(error) << "File server: Failed to send file: " << filename;
          return false;
      }

      BOOST_LOG_TRIVIAL(info) << "File server: Successfully sent file: " << filename;
      return true;
  }
  catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Error in prepare_and_send: " << e.what();
      return false;
  }
}

bool FileServer::store_file(const std::string& filename, std::stringstream& input) {
  std::lock_guard<std::mutex> lock(mutex_);

  try {
    BOOST_LOG_TRIVIAL(info) << "File server: Storing file with filename: " << filename;

    // Validate input stream
    if (!input.good()) {
      BOOST_LOG_TRIVIAL(error) << "File server: Invalid input stream for file: " << filename;
      return false;
    }

    // Store file locally
    try {
      store_->store(filename, input);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to store file locally: " << e.what();
      return false;
    }

    // Reset stream position after store operation
    input.clear();
    input.seekg(0);

    // Broadcast the stored file to all peers with STORE_FILE message type
    if (!prepare_and_send(filename, MessageType::STORE_FILE)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to broadcast file: " << filename;
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "File server: Successfully stored and broadcasted file: " << filename;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Error in store_file: " << e.what();
    return false;
  }
}

bool FileServer::store_file(const std::string& filename, std::istream& input) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    BOOST_LOG_TRIVIAL(info) << "File server: Storing file with filename: " << filename;
    // Validate input stream
    if (!input.good()) {
      BOOST_LOG_TRIVIAL(error) << "File server: Invalid input stream for file: " << filename;
      return false;
    }
    // Store file locally
    try {
      store_->store(filename, input);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to store file locally: " << e.what();
      return false;
    }
    // Reset stream position after store operation
    input.clear();
    input.seekg(0);
    // Broadcast the stored file to all peers with STORE_FILE message type
    if (!prepare_and_send(filename, MessageType::STORE_FILE)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to broadcast file: " << filename;
      return false;
    }
    BOOST_LOG_TRIVIAL(info) << "File server: Successfully stored and broadcasted file: " << filename;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Error in store_file: " << e.what();
    return false;
  }
}

bool FileServer::get_file(const std::string& filename) {
  std::lock_guard<std::mutex> lock(mutex_);
  BOOST_LOG_TRIVIAL(info) << "File server: Attempting to get file: " << filename;

  // Try reading from local store first
  if (read_from_local_store(filename)) {
      return true;  
  }

  // If local read failed, try network retrieval
  return retrieve_from_network(filename);
}

bool FileServer::read_from_local_store(const std::string& filename) {
  try {
      if (!store_->has(filename)) {
          BOOST_LOG_TRIVIAL(debug) << "File server: File not found in local store";
          return false;
      }

      if (store_->read_file(filename, 20)) {
          BOOST_LOG_TRIVIAL(info) << "File server: File successfully read from local store: " << filename;
          return true;
      }
  } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(debug) << "File server: Error reading from local store: " << e.what();
  }
  return false;
}

bool FileServer::retrieve_from_network(const std::string& filename) {
  try {
      if (!prepare_and_send(filename, MessageType::GET_FILE)) {
          BOOST_LOG_TRIVIAL(error) << "File server: Failed to send GET_FILE request for: " << filename;
          return false;
      }

      BOOST_LOG_TRIVIAL(debug) << "File server: Waiting for network retrieval of file: " << filename;
      std::this_thread::sleep_for(std::chrono::seconds(5));

      if (store_->has(filename)) { 
          BOOST_LOG_TRIVIAL(info) << "File server: File successfully retrieved from network: " << filename;
          return true;  
      }
  } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Error in network retrieval: " << e.what();
  }

  BOOST_LOG_TRIVIAL(info) << "File server: File not found: " << filename;
  return false;
}

bool FileServer::handle_store(const MessageFrame& frame) {
  try {
    BOOST_LOG_TRIVIAL(info) << "File server: Handling store message frame";

    // Validate payload stream
    if (!frame.payload_stream || !frame.payload_stream->good()) {
      BOOST_LOG_TRIVIAL(error) << "File server: Invalid payload stream in message frame";
      return false;
    }

    // Extract filename from frame
    std::string filename;
    try {
      filename = extract_filename(frame);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to extract filename: " << e.what();
      return false;
    }

    // Store the file using the Store class
    try {
      store_->store(filename, *frame.payload_stream);
      BOOST_LOG_TRIVIAL(info) << "File server: Successfully stored file: " << filename;
      return true;
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to store file: " << e.what();
      return false;
    }
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Error in handle_store: " << e.what();
    return false;
  }
}

bool FileServer::handle_get(const MessageFrame& frame) {
  try {
    BOOST_LOG_TRIVIAL(info) << "File server: Handling get message frame";

    // Extract filename from frame
    std::string filename;
    try {
      filename = extract_filename(frame);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to extract filename: " << e.what();
      return false;
    }

    // Check if file exists locally
    if (!store_->has(filename)) {
      BOOST_LOG_TRIVIAL(info) << "File server: File not found locally: " << filename;
      return false;
    }

    // Prepare file with GET_FILE message type and send to requesting peer
    if (!prepare_and_send(filename, MessageType::STORE_FILE, frame.source_id)) {
      BOOST_LOG_TRIVIAL(error) << "File server: Failed to prepare file: " << filename;
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "File server: Successfully handled get request for file: " << filename;
    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Error in handle_get: " << e.what();
    return false;
  }
}

void FileServer::channel_listener() {
  BOOST_LOG_TRIVIAL(info) << "File server: Starting channel listener";

  while (running_) {
    try {
      MessageFrame frame;
      // Try to consume a message from the channel
      if (channel_.consume(frame)) {
        BOOST_LOG_TRIVIAL(debug) << "File server: Retrieved message from channel, type: " 
                    << static_cast<int>(frame.message_type);

        // Handle the message
        message_handler(frame);
      }

      // Small sleep to prevent busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "File server: Error in channel listener: " << e.what();
    }
  }
}

void FileServer::message_handler(const MessageFrame& frame) {
  try {
    BOOST_LOG_TRIVIAL(info) << "File server: Handling message of type: " << static_cast<int>(frame.message_type);

    switch (frame.message_type) {
      case MessageType::STORE_FILE:
        BOOST_LOG_TRIVIAL(debug) << "File server: Forwarding to handle_store";
        if (!handle_store(frame)) {
          BOOST_LOG_TRIVIAL(error) << "File server: Failed to handle store message";
        }
        break;

      case MessageType::GET_FILE:
        BOOST_LOG_TRIVIAL(debug) << "File server: Forwarding to handle_get";
        if (!handle_get(frame)) {
          BOOST_LOG_TRIVIAL(error) << "File server: Failed to handle get message";
        }
        break;

      default:
        BOOST_LOG_TRIVIAL(warning) << "File server: Unknown message type: " << static_cast<int>(frame.message_type);
        break;
    }
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "File server: Error in message handler: " << e.what();
  }
}

} // namespace network
} // namespace dfs