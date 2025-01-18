#include "file_server/file_server.hpp"
#include <boost/log/trivial.hpp>
#include <filesystem>
#include <sstream>
#include "../crypto/crypto_utils.hpp"

namespace dfs {
namespace network {

FileServer::FileServer(uint32_t server_id, const std::vector<uint8_t>& key)
    : server_id_(server_id)
    , key_(key) {

    // Validate key size (32 bytes for AES-256)
    if (key_.empty() || key_.size() != 32) {
        BOOST_LOG_TRIVIAL(error) << "Invalid key size: " << key_.size() << " bytes. Expected 32 bytes.";
        throw std::invalid_argument("Invalid cryptographic key size");
    }

    BOOST_LOG_TRIVIAL(info) << "Initializing FileServer with ID: " << server_id_;

    // Create store directory based on server ID
    std::string store_path = "fileserver_" + std::to_string(server_id_);

    try {
        // Initialize store with the server-specific directory
        store_ = std::make_unique<dfs::store::Store>(store_path);

        // Initialize codec with the provided cryptographic key
        codec_ = std::make_unique<Codec>(key_);

        // Initialize peer manager
        peer_manager_ = std::make_unique<PeerManager>();

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

bool FileServer::prepare_and_send(const std::string& filename, std::optional<uint32_t> peer_id) {
    try {
        BOOST_LOG_TRIVIAL(info) << "Preparing to send file: " << filename;

        // Create message frame
        MessageFrame frame;
        frame.message_type = MessageType::STORE_FILE;
        frame.source_id = server_id_;
        frame.payload_stream = std::make_shared<std::stringstream>();

        // Generate and set IV
        frame.initialization_vector = crypto::generate_IV();
        BOOST_LOG_TRIVIAL(debug) << "Generated IV for file: " << filename;

        // Get file content from store into payload stream
        store_->get(filename, *dynamic_cast<std::stringstream*>(frame.payload_stream.get()));
        frame.payload_size = frame.payload_stream->tellg();
        frame.payload_stream->seekg(0);

        // Create empty stream for serialized data
        std::stringstream serialized_stream;

        // Serialize the frame
        codec_->serialize(frame, *frame.payload_stream, serialized_stream);
        serialized_stream.seekg(0);

        bool success = false;
        if (peer_id.has_value()) {
            // Send to specific peer
            BOOST_LOG_TRIVIAL(debug) << "Sending file to peer: " << peer_id.value();
            success = peer_manager_->send_to_peer(peer_id.value(), serialized_stream);
        } else {
            // Broadcast to all peers
            BOOST_LOG_TRIVIAL(debug) << "Broadcasting file to all peers";
            success = peer_manager_->broadcast_stream(serialized_stream);
        }

        if (success) {
            BOOST_LOG_TRIVIAL(info) << "Successfully sent file: " << filename;
        } else {
            BOOST_LOG_TRIVIAL(error) << "Failed to send file: " << filename;
        }

        return success;
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error in prepare_and_send: " << e.what();
        return false;
    }
}

} // namespace network
} // namespace dfs