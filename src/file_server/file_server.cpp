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

bool FileServer::prepare_and_send(const std::string& filename, std::optional<std::string> peer_id) {
    try {
        BOOST_LOG_TRIVIAL(info) << "Preparing file: " << filename 
                               << " for " << (peer_id ? "peer " + *peer_id : "broadcast");

        // Create message frame for header
        MessageFrame frame;
        frame.message_type = MessageType::STORE_FILE;
        frame.filename_length = filename.length();

        // Generate and set IV
        crypto::CryptoStream crypto_stream;
        auto iv = crypto_stream.generate_IV();
        frame.iv_.assign(iv.begin(), iv.end());

        // Get file size from store
        frame.payload_size = store_->get_file_size(filename);
        if (frame.payload_size == 0) {
            BOOST_LOG_TRIVIAL(error) << "Failed to get file size from store: " << filename;
            return false;
        }

        // Create temporary stream for header serialization
        std::stringstream header_stream;

        // Serialize only the header
        if (!codec_->serialize_header(frame, header_stream)) {
            BOOST_LOG_TRIVIAL(error) << "Failed to serialize message frame header";
            return false;
        }

        // Get input stream from store without loading entire file
        std::unique_ptr<std::istream> file_stream = store_->get_stream(filename);
        if (!file_stream || !file_stream->good()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to get file stream from store: " << filename;
            return false;
        }

        // Send the header and stream the encrypted payload
        bool send_success;
        if (peer_id) {
            // Send to specific peer
            BOOST_LOG_TRIVIAL(debug) << "Sending file to peer: " << *peer_id;

            // Send header first
            send_success = peer_manager_.send_stream(*peer_id, header_stream);
            if (!send_success) {
                BOOST_LOG_TRIVIAL(error) << "Failed to send header to peer: " << *peer_id;
                return false;
            }

            // Now stream the encrypted payload
            std::stringstream encrypted_stream;
            codec_->stream_encrypt_payload(*file_stream, encrypted_stream, frame.iv_);
            send_success = peer_manager_.send_stream(*peer_id, encrypted_stream);
        } else {
            // Broadcast to all peers
            BOOST_LOG_TRIVIAL(debug) << "Broadcasting file to all peers";

            // Send header first
            send_success = peer_manager_.broadcast_stream(header_stream);
            if (!send_success) {
                BOOST_LOG_TRIVIAL(error) << "Failed to broadcast header";
                return false;
            }

            // Now stream the encrypted payload
            std::stringstream encrypted_stream;
            codec_->stream_encrypt_payload(*file_stream, encrypted_stream, frame.iv_);
            send_success = peer_manager_.broadcast_stream(encrypted_stream);
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

    BOOST_LOG_TRIVIAL(info) << "Successfully stored file: " << filename;
    return true;
  }
  catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Error in store_file: " << e.what();
    return false;
  }
}

std::optional<std::stringstream> FileServer::get_file(const std::string& filename) {
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

    // Prepare file
    if (!prepare_and_send(filename, frame.source_id)) {
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