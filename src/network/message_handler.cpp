#include "network/message_handler.hpp"
#include <boost/log/trivial.hpp>
#include <sstream>
#include <vector>
#include <stdexcept>

namespace dfs {
namespace network {

bool MessageHandler::serialize(std::istream& input, std::ostream& output) {
    try {
        // Create message frame
        MessageFrame frame;

        // Read input data into buffer
        std::vector<char> data;
        char buffer[4096];
        while (input.read(buffer, sizeof(buffer))) {
            data.insert(data.end(), buffer, buffer + input.gcount());
        }
        if (input.gcount() > 0) {
            data.insert(data.end(), buffer, buffer + input.gcount());
        }

        // Set frame fields
        frame.type = ByteOrder::toNetworkOrder(frame.type);
        frame.source_id = ByteOrder::toNetworkOrder(frame.source_id);
        frame.payload_size = ByteOrder::toNetworkOrder(static_cast<uint32_t>(data.size()));

        // Write frame header
        output.write(reinterpret_cast<const char*>(&frame), sizeof(frame));

        // Write data payload
        if (!data.empty()) {
            output.write(data.data(), data.size());
        }

        return output.good();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Serialization error: " << e.what();
        return false;
    }
}

bool MessageHandler::deserialize(std::istream& input, std::ostream& output) {
    try {
        MessageFrame frame;

        // Read frame header
        input.read(reinterpret_cast<char*>(&frame), sizeof(frame));
        if (!input.good()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to read message frame";
            return false;
        }

        // Convert frame fields to host byte order
        frame.type = ByteOrder::fromNetworkOrder(frame.type);
        frame.source_id = ByteOrder::fromNetworkOrder(frame.source_id);
        uint32_t payload_size = ByteOrder::fromNetworkOrder(frame.payload_size);

        // Process payload in chunks
        std::vector<char> buffer(4096);
        uint32_t remaining = payload_size;

        while (remaining > 0 && input.good()) {
            size_t to_read = std::min(remaining, static_cast<uint32_t>(buffer.size()));
            input.read(buffer.data(), to_read);

            if (input.gcount() > 0) {
                output.write(buffer.data(), input.gcount());
                remaining -= input.gcount();
            }
        }

        return input.good() && output.good() && remaining == 0;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Deserialization error: " << e.what();
        return false;
    }
}

bool MessageHandler::process_incoming_data(std::istream& input, const std::array<uint8_t, IV_SIZE>& iv) {
    try {
        if (!input.good()) {
            BOOST_LOG_TRIVIAL(error) << "Invalid input stream";
            return false;
        }

        // Store IV for potential later use
        std::vector<uint8_t> initialization_vector(iv.begin(), iv.end());

        // Skip IV in input stream as it's provided separately
        input.ignore(IV_SIZE);
        if (!input.good()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to skip initialization vector";
            return false;
        }

        // Read message headers
        MessageHeader headers;
        if (!read_headers(input, headers)) {
            BOOST_LOG_TRIVIAL(error) << "Failed to read message headers";
            return false;
        }

        BOOST_LOG_TRIVIAL(debug) << "Processing message type: " << static_cast<uint32_t>(headers.type)
                                << ", file size: " << headers.file_size
                                << ", file name: " << headers.file_name;

        // Process based on message type
        switch (headers.type) {
            case MessageType::STORE_FILE: {
                if (headers.file_size > MAX_FILE_SIZE) {
                    BOOST_LOG_TRIVIAL(error) << "File size " << headers.file_size 
                                           << " exceeds maximum limit of " << MAX_FILE_SIZE;
                    return false;
                }

                // Create stream for file data
                auto stream = std::make_unique<std::stringstream>();
                if (!deserialize(input, *stream)) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to deserialize file data for " 
                                           << headers.file_name;
                    return false;
                }

                // TODO: FileServer Integration
                // Call fileserver.store_file with:
                // - headers.file_name: name of file to store
                // - headers.file_size: expected size of file
                // - std::move(stream): data stream containing file contents
                // - initialization_vector: IV for encryption

                BOOST_LOG_TRIVIAL(info) << "Successfully processed STORE_FILE request for " 
                                      << headers.file_name;
                break;
            }
            case MessageType::GET_FILE: {
                // TODO: FileServer Integration
                // Call fileserver.get_file with:
                // - headers.file_name: name of file to retrieve
                // - headers.file_size: expected size of file
                // - initialization_vector: IV for decryption

                BOOST_LOG_TRIVIAL(info) << "Successfully processed GET_FILE request for " 
                                      << headers.file_name;
                break;
            }
            default:
                BOOST_LOG_TRIVIAL(error) << "Unknown message type: " 
                                       << static_cast<uint32_t>(headers.type);
                return false;
        }

        return true;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing incoming data: " << e.what();
        return false;
    }
}

std::unique_ptr<std::stringstream> MessageHandler::process_outgoing_data(
    const MessageHeader& headers,
    std::istream& data
) {
    try {
        if (!data.good()) {
            BOOST_LOG_TRIVIAL(error) << "Invalid input data stream";
            return nullptr;
        }

        auto output = std::make_unique<std::stringstream>();
        if (!output) {
            BOOST_LOG_TRIVIAL(error) << "Failed to create output stream";
            return nullptr;
        }

        // Create message frame
        MessageFrame frame;
        frame.type = static_cast<uint32_t>(headers.type);
        frame.source_id = 0; // TODO: Set appropriate source ID
        frame.payload_size = headers.file_size;

        BOOST_LOG_TRIVIAL(debug) << "Creating outgoing message frame - Type: " 
                                << static_cast<uint32_t>(headers.type)
                                << ", Size: " << headers.file_size;

        // Write headers to output
        if (!write_headers(*output, headers)) {
            BOOST_LOG_TRIVIAL(error) << "Failed to write headers for " << headers.file_name;
            return nullptr;
        }

        // Serialize and write data
        if (!serialize(data, *output)) {
            BOOST_LOG_TRIVIAL(error) << "Failed to serialize data for " << headers.file_name;
            return nullptr;
        }

        BOOST_LOG_TRIVIAL(info) << "Successfully processed outgoing data for " << headers.file_name;
        return output;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error processing outgoing data: " << e.what();
        return nullptr;
    }
}

bool MessageHandler::read_headers(std::istream& input, MessageHeader& headers) {
    try {
        // Read message type (in network byte order)
        uint32_t type;
        input.read(reinterpret_cast<char*>(&type), sizeof(type));
        type = ByteOrder::fromNetworkOrder(type);
        headers.type = static_cast<MessageType>(type);

        // Read file size (in network byte order)
        uint64_t size;
        input.read(reinterpret_cast<char*>(&size), sizeof(size));
        headers.file_size = ByteOrder::fromNetworkOrder(size);

        // Read file name length (in network byte order)
        uint32_t name_length;
        input.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));
        name_length = ByteOrder::fromNetworkOrder(name_length);

        if (name_length > 1024) { // Reasonable limit for filename length
            BOOST_LOG_TRIVIAL(error) << "File name length exceeds maximum allowed: " << name_length;
            return false;
        }

        // Read file name (raw bytes, no conversion needed)
        std::vector<char> name_buffer(name_length);
        input.read(name_buffer.data(), name_length);
        headers.file_name.assign(name_buffer.data(), name_length);

        return input.good();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error reading headers: " << e.what();
        return false;
    }
}

bool MessageHandler::write_headers(std::ostream& output, const MessageHeader& headers) {
    try {
        // Write message type in network byte order
        uint32_t type = ByteOrder::toNetworkOrder(static_cast<uint32_t>(headers.type));
        output.write(reinterpret_cast<const char*>(&type), sizeof(type));

        // Write file size in network byte order
        uint64_t size = ByteOrder::toNetworkOrder(headers.file_size);
        output.write(reinterpret_cast<const char*>(&size), sizeof(size));

        // Write file name length in network byte order
        uint32_t name_length = ByteOrder::toNetworkOrder(
            static_cast<uint32_t>(headers.file_name.length())
        );
        output.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));

        // Write file name (raw bytes, no conversion needed)
        output.write(headers.file_name.c_str(), headers.file_name.length());

        return output.good();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Error writing headers: " << e.what();
        return false;
    }
}

} // namespace network
} // namespace dfs