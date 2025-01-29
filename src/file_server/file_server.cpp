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

FileServer::FileServer(uint32_t ID, const std::vector<uint8_t>& key, PeerManager& peer_manager, Channel& channel)
  : ID_(ID)
  , key_(key)
  , channel_(channel)
  , peer_manager_(peer_manager) {  // Initialize PeerManager reference

  // Validate key size (32 bytes for AES-256)
  if (key_.empty() || key_.size() != 32) {
    BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << key_.size() << " bytes. Expected 32 bytes.";
    throw std::invalid_argument("Invalid cryptographic key size");
  }

  BOOST_LOG_TRIVIAL(info) << "Initializing FileServer with ID: " << ID_;

  try {
    // Create store directory based on server ID
    std::string store_path = "fileserver_" + std::to_string(ID_);

    // Initialize store with the server-specific directory
    store_ = std::make_unique<dfs::store::Store>(store_path);

    // Initialize codec with the provided cryptographic key and channel reference
    codec_ = std::make_unique<Codec>(key_, channel);

    // Start the channel listener thread
    listener_thread_ = std::make_unique<std::thread>(&FileServer::channel_listener, this);

    BOOST_LOG_TRIVIAL(info) << "FileServer initialization complete";
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to initialize FileServer: " << e.what();
    throw;
  }
}

FileServer::~FileServer() {
    running_ = false;
    if (listener_thread_ && listener_thread_->joinable()) {
        listener_thread_->join();
    }
}

std::string FileServer::extract_filename(const MessageFrame& frame) {
  if (!frame.payload_stream) {
    BOOST_LOG_TRIVIAL(error) << "Invalid payload stream in message frame";
    throw std::runtime_error("Invalid payload stream");
  }

  if (frame.filename_length == 0) {
    BOOST_LOG_TRIVIAL(error) << "Invalid filename length: 0";
    throw std::runtime_error("Invalid filename length");
  }

  try {
    // Create a buffer to hold the filename
    std::vector<char> filename_buffer(frame.filename_length);

    // Read the filename from the start of payload
    frame.payload_stream->seekg(0);
    if (!frame.payload_stream->read(filename_buffer.data(), frame.filename_length)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to read filename from payload stream";
      throw std::runtime_error("Failed to read filename");
    }

    // Convert to string
    std::string filename(filename_buffer.begin(), filename_buffer.end());

    BOOST_LOG_TRIVIAL(debug) << "Successfully extracted filename: " << filename;
    return filename;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error extracting filename: " << e.what();
    throw;
  }
}

bool FileServer::prepare_and_send(const std::string& filename, MessageType message_type, std::optional<uint8_t> peer_id) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Preparing file: " << filename 
                          << " for " << (peer_id ? "peer " + std::to_string(*peer_id) : "broadcast")
                          << " with message type: " << static_cast<int>(message_type);

    // Create message frame
    MessageFrame frame;
    frame.message_type = message_type;
    frame.source_id = ID_;
    frame.filename_length = filename.length();

    // Generate and set IV
    crypto::CryptoStream crypto_stream;
    auto iv = crypto_stream.generate_IV();
    frame.iv_.assign(iv.begin(), iv.end());

    // Register producer function for pipeline
    auto store_producer = [this, &filename, first_read = true](std::stringstream& output) mutable -> bool {
      if (!first_read) {
        return false;  // We've already read the file
      }
      try {
        // Write filename first
        output.write(filename.c_str(), filename.length());

        // Then write file content
        store_->get(filename, output);
        first_read = false;
        return output.good();
      } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error reading from store: " << e.what();
        return false;
      }
    };

    // Register transformer function for pipeline
    auto serialize_transform = [this, &frame](std::stringstream& input, std::stringstream& output) -> bool {
      frame.payload_stream = std::make_shared<std::stringstream>();
      *frame.payload_stream << input.rdbuf();

      // Get position to find size
      frame.payload_stream->seekp(0, std::ios::end);
      frame.payload_size = frame.payload_stream->tellp();
      frame.payload_stream->seekg(0);
      
      return codec_->serialize(frame, output);
    };

    // Create pipeline with producer and transformer defined above
    auto pipeline = utils::Pipeliner::create(store_producer)
                    ->transform(serialize_transform);
    pipeline->set_buffer_size(1024 * 1024); // 1MB buffer size

    // Flush the pipeline before sending to account for files smaller than 1MB
    pipeline->flush();

    // Send the pipeline data
    bool send_success;
    if (peer_id) {
      BOOST_LOG_TRIVIAL(debug) << "Sending file to peer: " << static_cast<int>(*peer_id);
      send_success = peer_manager_.send_to_peer(*peer_id, *pipeline);
    } else {
      BOOST_LOG_TRIVIAL(debug) << "Broadcasting file to all peers";
      send_success = peer_manager_.broadcast_stream(*pipeline);
    }

    if (!send_success) {
      BOOST_LOG_TRIVIAL(error) << "Failed to send file: " << filename;
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully prepared and sent file: " << filename;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in prepare_and_send: " << e.what();
    return false;
  }
}

bool FileServer::store_file(const std::string& filename, std::stringstream& input) {
  std::lock_guard<std::mutex> lock(mutex_);

  try {
    BOOST_LOG_TRIVIAL(info) << "Storing file with filename: " << filename;

    // Validate input stream
    if (!input.good()) {
      BOOST_LOG_TRIVIAL(error) << "Invalid input stream for file: " << filename;
      return false;
    }

    // Store file locally
    try {
      store_->store(filename, input);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Failed to store file locally: " << e.what();
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

    BOOST_LOG_TRIVIAL(info) << "Successfully stored and broadcasted file: " << filename;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in store_file: " << e.what();
    return false;
  }
}

std::optional<std::stringstream> FileServer::get_file(const std::string& filename) {
  std::lock_guard<std::mutex> lock(mutex_);

  try {
    BOOST_LOG_TRIVIAL(info) << "Attempting to get file: " << filename;

    // Check local store
    std::stringstream local_result;
    try {
      store_->get(filename, local_result);
      if (local_result.good()) {
        BOOST_LOG_TRIVIAL(info) << "File found in local store: " << filename;
        return local_result;
      }
    } catch (const dfs::store::StoreError& e) {
      BOOST_LOG_TRIVIAL(debug) << "File not found in local store: " << e.what();
      // Try to get file from network by sending GET_FILE request
      if (!prepare_and_send(filename, MessageType::GET_FILE)) {
        BOOST_LOG_TRIVIAL(error) << "Failed to send GET_FILE request for: " << filename;
        return std::nullopt;
      }

      // Wait for potential network retrieval (5 seconds)
      BOOST_LOG_TRIVIAL(debug) << "Waiting for network retrieval of file: " << filename;
      std::this_thread::sleep_for(std::chrono::seconds(5));

      // Try to get the file from local store again
      try {
        std::stringstream retry_result;
        store_->get(filename, retry_result);
        if (retry_result.good()) {
          BOOST_LOG_TRIVIAL(info) << "File successfully retrieved from network: " << filename;
          return retry_result;
        }
      } catch (const dfs::store::StoreError& e) {
        BOOST_LOG_TRIVIAL(debug) << "File not found after network retrieval attempt: " << e.what();
      }
    }

    BOOST_LOG_TRIVIAL(info) << "File not found: " << filename;
    return std::nullopt;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in get_file: " << e.what();
    return std::nullopt;
  }
}

bool FileServer::handle_store(const MessageFrame& frame) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Handling store message frame";

    // Validate payload stream
    if (!frame.payload_stream || !frame.payload_stream->good()) {
      BOOST_LOG_TRIVIAL(error) << "Invalid payload stream in message frame";
      return false;
    }

    // Extract filename from frame
    std::string filename;
    try {
      filename = extract_filename(frame);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Failed to extract filename: " << e.what();
      return false;
    }

    // Store the file using the Store class
    try {
      store_->store(filename, *frame.payload_stream);
      BOOST_LOG_TRIVIAL(info) << "Successfully stored file: " << filename;
      return true;
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Failed to store file: " << e.what();
      return false;
    }
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in handle_store: " << e.what();
    return false;
  }
}

bool FileServer::handle_get(const MessageFrame& frame) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Handling get message frame";

    // Extract filename from frame
    std::string filename;
    try {
      filename = extract_filename(frame);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Failed to extract filename: " << e.what();
      return false;
    }

    // Check if file exists locally
    if (!store_->has(filename)) {
      BOOST_LOG_TRIVIAL(info) << "File not found locally: " << filename;
      return false;
    }

    // Prepare file with GET_FILE message type and send to requesting peer
    if (!prepare_and_send(filename, MessageType::GET_FILE, frame.source_id)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to prepare file: " << filename;
      return false;
    }

    BOOST_LOG_TRIVIAL(info) << "Successfully handled get request for file: " << filename;
    return true;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in handle_get: " << e.what();
    return false;
  }
}

void FileServer::channel_listener() {
  BOOST_LOG_TRIVIAL(info) << "Starting channel listener";

  while (running_) {
    try {
      MessageFrame frame;
      // Try to consume a message from the channel
      if (channel_.consume(frame)) {
        std::lock_guard<std::mutex> lock(mutex_);
        BOOST_LOG_TRIVIAL(debug) << "Retrieved message from channel, type: " 
                    << static_cast<int>(frame.message_type);

        // Handle the message
        message_handler(frame);
      }

      // Small sleep to prevent busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(error) << "Error in channel listener: " << e.what();
    }
  }
}

void FileServer::message_handler(const MessageFrame& frame) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Handling message of type: " << static_cast<int>(frame.message_type);

    switch (frame.message_type) {
      case MessageType::STORE_FILE:
        BOOST_LOG_TRIVIAL(debug) << "Forwarding to handle_store";
        if (!handle_store(frame)) {
          BOOST_LOG_TRIVIAL(error) << "Failed to handle store message";
        }
        break;

      case MessageType::GET_FILE:
        BOOST_LOG_TRIVIAL(debug) << "Forwarding to handle_get";
        if (!handle_get(frame)) {
          BOOST_LOG_TRIVIAL(error) << "Failed to handle get message";
        }
        break;

      default:
        BOOST_LOG_TRIVIAL(warning) << "Unknown message type: " << static_cast<int>(frame.message_type);
        break;
    }
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in message handler: " << e.what();
  }
}

} // namespace network
} // namespace dfs