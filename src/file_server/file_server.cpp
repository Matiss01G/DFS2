#include "file_server/file_server.hpp"
#include <boost/log/trivial.hpp>
#include <filesystem>
#include <optional>
#include <thread>
#include <chrono>
#include "network/peer_manager.hpp"  // Added include for PeerManager

namespace dfs {
namespace network {

FileServer::FileServer(uint32_t server_id, const std::vector<uint8_t>& key, PeerManager& peer_manager, Channel& channel)
  : server_id_(server_id)
  , key_(key)
  , channel_(channel)
  , peer_manager_(peer_manager) {  // Initialize PeerManager reference

  // Validate key size (32 bytes for AES-256)
  if (key_.empty() || key_.size() != 32) {
    BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << key_.size() << " bytes. Expected 32 bytes.";
    throw std::invalid_argument("Invalid cryptographic key size");
  }

  BOOST_LOG_TRIVIAL(info) << "Initializing FileServer with ID: " << server_id_;

  try {
    // Create store directory based on server ID
    std::string store_path = "fileserver_" + std::to_string(server_id_);

    // Initialize store with the server-specific directory
    store_ = std::make_unique<dfs::store::Store>(store_path);

    // Initialize codec with the provided cryptographic key
    codec_ = std::make_unique<Codec>(key_);

    BOOST_LOG_TRIVIAL(info) << "FileServer initialization complete";
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Failed to initialize FileServer: " << e.what();
    throw;
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

    // Save current position
    std::streampos original_pos = frame.payload_stream->tellg();

    // Read the filename from the start of payload
    frame.payload_stream->seekg(0);
    if (!frame.payload_stream->read(filename_buffer.data(), frame.filename_length)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to read filename from payload stream";
      throw std::runtime_error("Failed to read filename");
    }

    // Restore original position
    frame.payload_stream->seekg(original_pos);

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

bool FileServer::prepare_and_send(const std::string& filename, std::optional<std::string> peer_id, MessageType message_type) {
  try {
    BOOST_LOG_TRIVIAL(info) << "Preparing file: " << filename 
                          << " for " << (peer_id ? "peer " + *peer_id : "broadcast")
                          << " with message type: " << static_cast<int>(message_type);

    // Create message frame
    MessageFrame frame;
    frame.message_type = message_type;
    frame.payload_stream = std::make_shared<std::stringstream>();
    frame.filename_length = filename.length();

    // Generate and set IV
    crypto::CryptoStream crypto_stream;
    auto iv = crypto_stream.generate_IV();
    frame.iv_.assign(iv.begin(), iv.end());

    // Only get file data for STORE_FILE type
    if (message_type == MessageType::STORE_FILE) {
      try {
        store_->get(filename, *frame.payload_stream);
        if (!frame.payload_stream->good()) {
          BOOST_LOG_TRIVIAL(error) << "Failed to get file from store: " << filename;
          return false;
        }
      } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error getting file from store: " << e.what();
        return false;
      }
    } else {
      // For GET_FILE, we only need the filename in the payload
      (*frame.payload_stream) << filename;
    }

    // Create stream for serialized data
    std::stringstream serialized_stream;

    // Serialize the frame
    if (!codec_->serialize(frame, serialized_stream)) {
      BOOST_LOG_TRIVIAL(error) << "Failed to serialize message frame";
      return false;
    }

    // Send the serialized data
    bool send_success;
    if (peer_id) {
      // Send to specific peer
      BOOST_LOG_TRIVIAL(debug) << "Sending to peer: " << *peer_id;
      send_success = peer_manager_.send_stream(*peer_id, serialized_stream);
    } else {
      // Broadcast to all peers
      BOOST_LOG_TRIVIAL(debug) << "Broadcasting to all peers";
      send_success = peer_manager_.broadcast_stream(serialized_stream);
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

    // Broadcast the stored file to all peers with STORE_FILE type
    if (!prepare_and_send(filename, std::nullopt, MessageType::STORE_FILE)) {
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

    // Prepare and send file with GET_FILE type
    if (!prepare_and_send(filename, frame.source_id, MessageType::GET_FILE)) {
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

  while (true) {
    try {
      MessageFrame frame;
      // Try to consume a message from the channel
      if (channel_.consume(frame)) {
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