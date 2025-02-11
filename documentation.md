# **CryptoStream**

### Overview

CryptoStream provides stream-based encryption and decryption using AES-256 in CBC mode. It handles large data streams efficiently with proper memory management and PKCS7 padding.

### Constants

- `static constexpr size_t KEY_SIZE = 32` - Required size for AES-256 encryption key
- `static constexpr size_t IV_SIZE = 16` - Required initialization vector size for CBC mode
- `static constexpr size_t BLOCK_SIZE = 16` - Standard AES block size for encryption/decryption
- `static constexpr size_t BUFFER_SIZE = 8192` - Optimal buffer size for stream processing

### Variables

- `std::vector<uint8_t> key_` - Stores the encryption/decryption key as a byte vector
- `std::vector<uint8_t> iv_` - Holds the initialization vector for CBC mode operations
- `std::unique_ptr<CipherContext> context_` - Smart pointer to OpenSSL cipher context wrapper
- `bool is_initialized_ = false` - Tracks whether crypto parameters are properly set
- `Mode mode_ = Mode::Encrypt` - Current operation mode (Encrypt/Decrypt)
- `std::istream* pending_input_ = nullptr` - Points to stream currently being processed

### Public Methods

**Constructor/Destructor**

- `CryptoStream()` - Initializes OpenSSL algorithms and creates new cipher context. Sets up RAII resources
- `~CryptoStream()` - Performs cleanup of OpenSSL resources and context memory

**Initialization**

- `void initialize(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv)` - Validates and sets encryption key and IV. Throws InitializationError if key size is invalid

**Encryption/Decryption**

- `std::ostream& encrypt(std::istream& input, std::ostream& output)` - Encrypts entire input stream using AES-256-CBC. Returns reference to output stream
- `std::ostream& decrypt(std::istream& input, std::ostream& output)` - Decrypts entire input stream using AES-256-CBC. Returns reference to output stream

**Getters/Setters**

- `Mode getMode() const` - Retrieves the current operation mode setting
- `void setMode(Mode mode)` - Updates the current operation mode between Encrypt/Decrypt

**Utilities**

- `std::array<uint8_t, IV_SIZE> generate_IV() const` - Creates cryptographically secure random IV using OpenSSL's RAND_bytes

### Private Methods

**Initialization**

- `void initializeCipher(bool encrypting)` - Prepares OpenSSL context for encryption/decryption with current parameters. Sets up CBC mode

**Stream Processing**

- `void processStream(std::istream& input, std::ostream& output, bool encrypting)` - Main entry point for stream operations. Validates streams and initiates processing pipeline
- `void saveStreamPos(std::istream& input, std::ostream& output, const std::function<void()>& operation)` - Maintains stream integrity by saving positions and restoring on errors. Ensures consistent state
- `void processStreamData(std::istream& input, std::ostream& output, bool encrypting)` - Handles the core encryption/decryption loop. Processes input in chunks through cipher
- `size_t processDataBlock(const uint8_t* inbuf, size_t bytes_read, uint8_t* outbuf, bool encrypting)` - Encrypts/decrypts a single data block using OpenSSL EVP functions
- `void writeOutputBlock(std::ostream& output, const uint8_t* data, size_t length)` - Safely writes processed data to output stream. Handles write errors
- `void processFinalBlock(uint8_t* outbuf, int& outlen, bool encrypting)` - Handles the final block with PKCS7 padding. Ensures proper stream termination



# **ByteOrder**

### Overview

ByteOrder provides utility functions for handling endianness conversions in cross-platform applications. It offers seamless conversion between host byte order and network byte order (big endian), with automatic detection of system endianness.

### Constants

None defined

### Variables

None defined - Class uses only static methods

### Public Methods

### Endianness Detection

`static bool isLittleEndian()` - Determines if the current system uses little endian byte ordering

- Returns: true if system is little endian, false if big endian
- Implementation uses a compile-time check of byte representation

### Byte Order Conversion

`static T toNetworkOrder(T value)` - Converts a value from host byte order to network byte order (big endian)

- Template parameter T: Type of value to convert
- Parameters:
    - value: The value to convert to network byte order
- Returns: Value in network byte order (big endian)
- Performs conversion only if system is little endian

`static T fromNetworkOrder(T value)` - Converts a value from network byte order (big endian) back to host byte order

- Template parameter T: Type of value to convert
- Parameters:
    - value: The value to convert from network byte order
- Returns: Value in host byte order
- Performs conversion only if system is little endian

### Private Methods

### Byte Manipulation

`static T byteSwap(T value)` - Internal helper function that reverses byte order of any sized value

- Template parameter T: Type of value to byte swap
- Parameters:
    - value: The value whose bytes need to be swapped
- Returns: Value with reversed byte order
- Implementation:
    - Copies value to byte array
    - Reverses byte array
    - Copies back to result value
    - Uses std::memcpy for safe type punning



# **CryptoError Classes**

### Overview

Provides a hierarchy of exception classes for handling various cryptographic operation errors. All crypto-specific exceptions inherit from a common base class `CryptoError`, which itself derives from `std::runtime_error`. This ensures consistent error handling and reporting across the cryptographic system.

## Base Class:

### CryptoError

`class CryptoError : public std::runtime_error`

- Base class for all cryptographic exceptions
- Inherits from: std::runtime_error
- Purpose: Provides common functionality for all crypto-related errors

**Constructor**

`explicit CryptoError(const std::string& message)`

- Parameters:
    - message: Descriptive error message
- Behavior: Forwards message to std::runtime_error constructor

## Derived Classes:

### InitializationError

`class InitializationError : public CryptoError`

- Purpose: Represents errors during cryptographic system initialization
- Use cases: Invalid key sizes, failed algorithm initialization

**Constructor**

`explicit InitializationError(const std::string& message)`

- Parameters:
    - message: Specific initialization error details
- Behavior: Prepends "Initialization error: " to message

### EncryptionError

`class EncryptionError : public CryptoError`

- Purpose: Represents errors during encryption operations
- Use cases: Buffer errors, algorithm failures during encryption

**Constructor**

`explicit EncryptionError(const std::string& message)`

- Parameters:
    - message: Specific encryption error details
- Behavior: Prepends "Encryption error: " to message

### DecryptionError

`class DecryptionError : public CryptoError`

- Purpose: Represents errors during decryption operations
- Use cases: Invalid ciphertext, padding errors, algorithm failures during decryption

**Constructor** 

`explicit DecryptionError(const std::string& message)`

- Parameters:
    - message: Specific decryption error details
- Behavior: Prepends "Decryption error: " to message



# **File Server**

### Overview

FileServer provides a distributed file storage and retrieval system with encryption support. It handles peer-to-peer file sharing using AES-256 encryption in CBC mode, managing both local storage and network distribution of files. It is the core of this distributed file system implementation

### Constants

None defined in class (constants are inherited from dependent classes)

### Variables

- `uint32_t ID_` - Unique identifier for this file server instance
- `std::vector<uint8_t> key_` - AES-256 encryption key used for secure file transfers
- `std::unique_ptr<dfs::store::Store> store_` - Manages local file storage operations
- `std::unique_ptr<Codec> codec_` - Handles message encoding/decoding with encryption
- `Channel& channel_` - Reference to communication channel for message passing
- `PeerManager& peer_manager_` - Manages peer connections and message routing
- `TCP_Server& tcp_server_` - Handles TCP network connections
- `std::mutex mutex_` - Synchronizes access to shared resources
- `std::atomic<bool> running_{true}` - Controls the lifecycle of background threads
- `std::unique_ptr<std::thread> listener_thread_` - Background thread for processing incoming messages

### Public Methods

**Constructor/Destructor**

- `FileServer(uint32_t ID, const std::vector<uint8_t>& key, PeerManager& peer_manager, Channel& channel, TCP_Server& tcp_server)` - Initializes file server with ID, encryption key, and network components. Validates key size and sets up storage
- `virtual ~FileServer()` - Cleans up resources and stops background threads

**Initialization**

- `bool connect(const std::string& remote_address, uint16_t remote_port)` - Establishes connection to remote peer at specified address and port. Returns success status

**File Operations**

- `bool store_file(const std::string& filename, std::istream& input)` - Stores file locally and broadcasts to network peers. Returns success status
- `bool get_file(const std::string& filename)` - Retrieves file from local storage or network peers. Returns success status

**Getters/Setters**

- `dfs::store::Store& get_store()` - Returns reference to local file storage manager

### Private Methods

**Outgoing Data Processing**

- `bool prepare_and_send(const std::string& filename, MessageType message_type, std::optional<uint8_t> peer_id)` - Prepares file data and sends to specified peer or broadcasts
- `MessageFrame create_message_frame(const std::string& filename, MessageType message_type)` - Creates message frame with metadata and initialization vector
- `std::function<bool(std::stringstream&)> create_producer(const std::string& filename, MessageType message_type)` - Creates data streaming function based on message type
- `std::function<bool(std::stringstream&, std::stringstream&)> create_transform(MessageFrame& frame, utils::Pipeliner* pipeline)` - Creates transformation function for message serialization
- `bool send_pipeline(dfs::utils::Pipeliner* const& pipeline, std::optional<uint8_t> peer_id)` - Handles pipeline data transmission to peers

**Incoming Data Processing**

- `void channel_listener()` - Background thread monitoring channel for incoming messages
- `void message_handler(const MessageFrame& frame)` - Routes incoming messages to appropriate handlers
- `bool handle_store(const MessageFrame& frame)` - Processes incoming store file requests
- `bool handle_get(const MessageFrame& frame)` - Processes incoming get file requests
- `std::string extract_filename(const MessageFrame& frame)` - Extracts filename from message frame payload

**Helper Methods**

- `bool read_from_local_store(const std::string& filename)` - Attempts to read file from local storage
- `bool retrieve_from_network(const std::string& filename)` - Attempts to retrieve file from network peers



# **Logger**

### Overview

Logger provides a centralized logging facility for the distributed file system using Boost.Log. It implements a singleton pattern for system-wide logging with configurable severity levels and file output.

### Constants

- `DFS_LOG_TRACE` - Macro for trace-level logging
- `DFS_LOG_DEBUG` - Macro for debug-level logging
- `DFS_LOG_INFO` - Macro for info-level logging
- `DFS_LOG_WARN` - Macro for warning-level logging
- `DFS_LOG_ERROR` - Macro for error-level logging
- `DFS_LOG_FATAL` - Macro for fatal-level logging

### Variables

- `static boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger` - Static instance of the severity logger

### Public Methods

**Initialization**

- `static void init(const std::string& log_file = "dfs_crypto.log")` - Initializes the logging system with specified configuration:
    - Sets up file sink with specified log file
    - Configures timestamp formatting
    - Sets minimum severity level to trace
    - Adds common attributes for logging
    - Enables auto-flush for immediate writing

**Getters/Setters**

- `static boost::log::sources::severity_logger<boost::log::trivial::severity_level>& get_logger()` - Returns reference to the singleton logger instance

### Private Methods

- None

### Log Format

Each log entry follows the format:
`[YYYY-MM-DD HH:MM:SS] [SEVERITY] message`

Example:
`[2024-02-10 14:30:45] [info] System startup complete`



# **MessageFrame**

## Overview

MessageFrame defines the structure for network messages in the distributed file system. It provides an enumeration for message types and a data structure that holds message content including initialization vectors, metadata, and payload data.

## Constants

- `MessageType::STORE_FILE = 0` - Enumeration value for file storage requests
- `MessageType::GET_FILE = 1` - Enumeration value for file retrieval requests

## Variables

- `std::vector<uint8_t> iv_` - Initialization vector for cryptographic operations
- `MessageType message_type` - Type of the message (STORE_FILE or GET_FILE)
- `uint8_t source_id` - Identifier of the message sender
- `uint64_t payload_size` - Size of the message payload in bytes
- `uint32_t filename_length` - Length of the filename in the payload
- `std::shared_ptr<std::stringstream> payload_stream` - Stream containing the message payload data

## Public Methods

None defined in class.

## Private Methods

None defined in class.



# **Peer**

## Overview

Peer is an abstract base class that defines the interface for network peers in the distributed file system. It provides virtual methods for stream processing, message sending, and stream control operations.

## Constants

None defined in class scope.

## Variables

- `using StreamProcessor = std::function<void(std::istream&)>` - Type definition for stream processing callback function

## Public Methods

**Constructor/Destructor**

- `virtual ~Peer() = default` - Virtual destructor for proper cleanup in derived classes

**Stream Control Operations**

- `virtual bool start_stream_processing() = 0` - Begins processing incoming data stream
- `virtual void stop_stream_processing() = 0` - Stops processing incoming data stream

**Outgoing Data Stream Processing**

- `virtual bool send_message(const std::string& message, std::size_t total_size) = 0` - Sends string message to peer
- `virtual bool send_stream(std::istream& input_stream, std::size_t total_size, std::size_t buffer_size = 8192) = 0` - Sends stream data to peer

**Getters and Setters**

- `virtual std::istream* get_input_stream() = 0` - Returns pointer to input stream
- `virtual void set_stream_processor(StreamProcessor processor) = 0` - Sets callback for processing received data

## Private Methods

None defined in class.



# **TCP_Peer**

## Overview

TCP_Peer implements the Peer interface using TCP/IP for network communication. It provides asynchronous stream processing, secure message transmission, and connection management functionality for peer-to-peer communication.

## Constants

None defined in class scope.

## Variables

### Public Types

- `using StreamProcessor = std::function<void(std::istream&)>` - Type definition for stream processing callback

### Variables

- `uint8_t peer_id_` - Unique identifier for this peer
- `StreamProcessor stream_processor_` - Callback for processing received data
- `std::size_t expected_size_` - Expected size of incoming data
- `std::unique_ptr<Codec> codec_` - Encryption/decryption handler

**Stream Buffers**

- `std::unique_ptr<boost::asio::streambuf> input_buffer_` - Buffer for incoming data
- `std::unique_ptr<std::istream> input_stream_` - Stream for reading input data

**Network Components**

- `boost::asio::io_context io_context_` - Asio I/O context for async operations
- `std::mutex io_mutex_` - Synchronization for I/O operations
- `std::unique_ptr<boost::asio::ip::tcp::socket> socket_` - TCP socket
- `std::unique_ptr<boost::asio::ip::tcp::endpoint> endpoint_` - Network endpoint

**Processing Thread**

- `std::unique_ptr<std::thread> processing_thread_` - Thread for async processing
- `std::atomic<bool> processing_active_` - Flag for processing state

## Public Methods

**Constructor/Destructor**

- `explicit TCP_Peer(uint8_t peer_id, Channel& channel, const std::vector<uint8_t>& key)` - Creates peer with ID, channel and crypto key
- `~TCP_Peer()` - Ensures proper cleanup of resources

**Stream Control Operations**

- `bool start_stream_processing()` - Initiates async stream processing thread
- `void stop_stream_processing()` - Stops processing and cleans up resources

**Outgoing Data Stream Processing**

- `bool send_stream(std::istream& input_stream, std::size_t total_size, std::size_t buffer_size = 8192)` - Sends data stream to peer
- `bool send_message(const std::string& message, std::size_t total_size)` - Sends string message to peer

**Getters and Setters**

- `std::istream* get_input_stream()` - Returns pointer to input stream
- `uint8_t get_peer_id() const` - Returns peer identifier
- `boost::asio::ip::tcp::socket& get_socket()` - Returns reference to socket
- `void set_stream_processor(StreamProcessor processor)` - Sets stream processing callback

## Private Methods

**Incoming Data Stream Processing**

- `void initialize_streams()` - Sets up input streams
- `void process_stream()` - Main stream processing loop
- `void handle_read_size()` - Handles size prefix reading
- `void handle_read_data()` - Handles data chunk reading
- `void process_received_data()` - Processes received data chunks
- `void async_read_next()` - Initiates next async read

**Outgoing Data Stream Processing**

- `bool send_size()` - Sends data size prefix

**Teardown**

- `void cleanup_connection()` - Cleans up connection resources



# **PeerManager**

## Overview

PeerManager handles peer connections in the distributed file system. It manages TCP peer creation, maintains peer state, handles message routing, and provides stream operations for communication between peers.

## Constants

None defined in class scope.

## Variables

- `Channel& channel_` - Reference to communication channel for message passing
- `TCP_Server& tcp_server_` - Reference to TCP server for connection handling
- `std::vector<uint8_t> key_` - Cryptographic key for secure peer communication
- `std::map<uint8_t, std::shared_ptr<TCP_Peer>> peers_` - Map of connected peers
- `mutable std::mutex mutex_` - Synchronization primitive for thread-safe peer access

## Public Methods

**Constructor/Destructor**

- `PeerManager(Channel& channel, TCP_Server& tcp_server, const std::vector<uint8_t>& key)` - Initializes manager with channel, server and crypto key
- `~PeerManager()` - Ensures clean shutdown of all peer connections

**Connection Management**

- `bool disconnect(uint8_t peer_id)` - Disconnects a specific peer from the network
- `bool is_connected(uint8_t peer_id)` - Checks if a specific peer is currently connected

**Peer Management**

- `void create_peer(std::shared_ptr<boost::asio::ip::tcp::socket> socket, uint8_t peer_id)` - Creates new peer from accepted connection
- `void add_peer(const std::shared_ptr<TCP_Peer> peer)` - Adds peer to managed peer collection
- `void remove_peer(uint8_t peer_id)` - Removes peer from managed collection
- `bool has_peer(uint8_t peer_id)` - Checks if peer exists in collection
- `std::shared_ptr<TCP_Peer> get_peer(uint8_t peer_id)` - Retrieves peer by ID

**Stream Operations**

- `bool send_to_peer(uint8_t peer_id, dfs::utils::Pipeliner& pipeline)` - Sends stream data to specific peer
- `bool broadcast_stream(dfs::utils::Pipeliner& pipeline)` - Sends stream data to all connected peers

**Utility Methods**

- `std::size_t size() const` - Returns number of managed peers
- `void shutdown()` - Terminates all peer connections and cleanup

## Private Methods

None defined in class.



# **Codec**

## Overview

Codec handles the serialization and deserialization of message frames for network transmission. It provides encryption for secure communication using AES-256-CBC, handles byte order conversion, and manages stream operations.

## Constants

None defined in class scope.

## Variables

- `std::vector<uint8_t> key_` - Encryption key used for securing message frames
- `Channel& channel_` - Reference to channel for message frame distribution

## Public Methods

**Constructor/Destructor**

- `explicit Codec(const std::vector<uint8_t>& key, Channel& channel)` - Initializes codec with encryption key and channel reference

**Serialization and Deserialization**

- `std::size_t serialize(const MessageFrame& frame, std::ostream& output)` - Encrypts and writes message frame to output stream. Returns total bytes written
- `MessageFrame deserialize(std::istream& input)` - Reads and decrypts message frame from input stream, adds to channel. Returns parsed frame

## Private Methods

**Stream Operations**

- `void write_bytes(std::ostream& output, const void* data, std::size_t size)` - Writes raw bytes to output stream
- `void read_bytes(std::istream& input, void* data, std::size_t size)` - Reads raw bytes from input stream

**Byte Order Conversion**

- `static uint32_t to_network_order(uint32_t host_value)` - Converts 32-bit value to network byte order
- `static uint64_t to_network_order(uint64_t host_value)` - Converts 64-bit value to network byte order
- `static uint32_t from_network_order(uint32_t network_value)` - Converts 32-bit value from network to host byte order
- `static uint64_t from_network_order(uint64_t network_value)` - Converts 64-bit value from network to host byte order

**Utility Methods**

- `static size_t get_padded_size(size_t original_size)` - Calculates total size including encryption padding



# **Channel**

## Overview

Channel provides a thread-safe message queue implementation for inter-component communication in the distributed file system. Messages are processed in FIFO order with proper synchronization for concurrent access.

## Constants

None defined in class scope.

## Variables

- `mutable std::mutex mutex_` - Synchronization primitive for thread-safe queue access
- `std::queue<MessageFrame> queue_` - Internal FIFO queue storing message frames

## Public Methods

**Constructor/Destructor**

- `Channel()` - Default constructor initializes empty message queue
- `~Channel()` - Default destructor cleans up queue resources

**Channel Control Methods**

- `void produce(const MessageFrame& frame)` - Adds a message frame to the back of the queue in thread-safe manner
- `bool consume(MessageFrame& frame)` - Retrieves and removes next message frame from queue. Returns false if empty, true if message retrieved

**Query Methods**

- `bool empty() const` - Returns true if the channel has no messages
- `std::size_t size() const` - Returns the number of messages currently in the channel

## Private Methods

- None



# **TCP_Server**

## Overview

TCP_Server manages TCP/IP network connections and handles peer handshaking in the distributed file system. It provides functionality for accepting incoming connections, establishing outgoing connections, and managing the lifecycle of network connections.

## Constants

None defined in class scope.

## Variables

- `PeerManager* peer_manager_` - Pointer to peer management system
- `const uint8_t ID_` - Unique identifier for this server

**Network Components**

- `const uint16_t port_` - Port number for listening
- `const std::string address_` - Network address to bind to

**Server State**

- `std::unique_ptr<std::thread> io_thread_` - Thread for async I/O operations
- `bool is_running_` - Server operational state flag

**Incoming Connection Handlers**

- `boost::asio::io_context io_context_` - Asio I/O context
- `std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_` - Connection acceptor=

## Public Methods

**Constructor/Destructor**

- `TCP_Server(const uint16_t port, const std::string& address, const uint8_t ID)` - Initializes server with port, address and ID
- `~TCP_Server()` - Ensures proper shutdown

**Initialization and Teardown**

- `bool start_listener()` - Starts the TCP server and begins accepting connections
- `void shutdown()` - Stops the server and cleans up resources

**Connection Initiation**

- `bool connect(const std::string& remote_address, uint16_t remote_port)` - Establishes connection to remote host

**Getters/Setters**

- `void set_peer_manager(PeerManager& peer_manager)` - Sets the peer management system

## Private Methods

**Initialization and Teardown**

- `void start_accept()` - Main listening loop for incoming connections

**Connection Initiation**

- `bool initiate_connection(const std::string& remote_address, uint16_t remote_port, std::shared_ptr<boost::asio::ip::tcp::socket>& socket)` - Creates socket connection to remote host

**Handshake Initiation**

- `bool initiate_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket)` - Performs ID exchange with remote peer
- `bool send_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket)` - Sends local ID to remote peer

**Handshake Reception**

- `void receive_handshake(std::shared_ptr<boost::asio::ip::tcp::socket> socket)` - Handles incoming handshake request
- `uint8_t read_ID(std::shared_ptr<boost::asio::ip::tcp::socket> socket)` - Reads peer ID from socket



# **Bootstrap**

## Overview

Bootstrap manages the initialization and lifecycle of a distributed file system node. It coordinates multiple components including TCP servers, peer management, and file services. The class handles network connections, peer discovery, and ensures proper startup and shutdown sequences.

## Constants

None defined in class scope.

## Variables

**Network Components**

- `std::string address_` - Network address for the node to bind to
- `uint16_t port_` - Port number for network communications
- `std::vector<std::string> bootstrap_nodes_` - List of known nodes to connect to on startup

**Crypto Key and Local ID**

- `std::vector<uint8_t> key_` - Cryptographic key for secure communications
- `uint8_t ID_` - Unique identifier for this node

**System Components**

- `std::unique_ptr<Channel> channel_` - Manages communication channels
- `std::unique_ptr<TCP_Server> tcp_server_` - Handles TCP connections
- `std::unique_ptr<PeerManager> peer_manager_` - Manages peer relationships
- `std::unique_ptr<FileServer> file_server_` - Handles file operations

## Public Methods

**Constructor/Destructor**

- `Bootstrap(const std::string& address, uint16_t port, const std::vector<uint8_t>& key, uint8_t ID, const std::vector<std::string>& bootstrap_nodes)` - Initializes all components in specific order with proper dependency management
- `~Bootstrap()` - Ensures clean shutdown of all components

**Initialization/Destruction**

- `bool start()` - Starts TCP server and connects to bootstrap nodes. Returns true on success
- `bool shutdown()` - Terminates all components in reverse dependency order. Returns true on success
- `bool connect_to_bootstrap_nodes()` - Attempts to connect to all configured bootstrap nodes. Returns true if all connections successful

**Getters/Setters**

- `PeerManager& get_peer_manager()` - Returns reference to peer management system
- `FileServer& get_file_server()` - Returns reference to file server component

## Private Methods

- None



# **Store**

## Overview

Store provides content-addressable storage functionality using SHA-256 hashing. It manages file storage, retrieval, and organization with a hierarchical directory structure based on content hashes.

## Constants

None defined in class scope.

## Variables

- `std::filesystem::path base_path_` - Root directory path for all stored files

## Public Methods

**Constructor/Destructor**

- `explicit Store(const std::string& base_path)` - Initializes store with specified base directory path

**Core Storage Operations**

- `void store(const std::string& key, std::istream& data)` - Stores data stream under given key
- `void get(const std::string& key, std::stringstream& output)` - Retrieves data for key into output stream
- `void remove(const std::string& key)` - Removes data associated with key
- `void clear()` - Removes all stored data and resets store

**Query Operations**

- `bool has(const std::string& key) const` - Checks if data exists for key
- `std::uintmax_t get_file_size(const std::string& key) const` - Returns file size in bytes

**CLI Command Support**

- `bool read_file(const std::string& key, size_t lines_per_page) const` - Displays file contents with pagination
- `void print_working_dir() const` - Displays current working directory
- `void list() const` - Lists store contents
- `void move_dir(const std::string& path)` - Changes working directory
- `void delete_file(const std::string& filename)` - Deletes specified file

## Private Methods

**CLI Command Support**

- `bool display_file_contents(std::ifstream& file, const std::string& key, size_t lines_per_page) const` - Handles paginated display

**CAS Storage Support**

- `std::string hash_key(const std::string& key) const` - Generates SHA-256 hash
- `std::filesystem::path get_path_for_hash(const std::string& hash) const` - Creates path from hash

**Utility Methods**

- `void check_directory_exists(const std::filesystem::path& path) const` - Ensures directory exists
- `std::filesystem::path resolve_key_path(const std::string& key) const` - Converts key to filesystem path
- `void verify_file_exists(const std::filesystem::path& file_path) const` - Checks file existence



# **Pipeliner**

## Overview

Pipeliner implements a stream processing pipeline that allows data transformation through producer and transformer functions. It extends std::stringstream to provide buffered stream processing capabilities.

## Constants

None defined in class scope.

## Variables

- `ProducerFn producer_` - Function that produces input data
- `std::vector<TransformFn> transforms_` - List of transformation functions
- `size_t buffer_size_` - Size of processing buffer (default 8192)
- `bool produced_` - Flag indicating if pipeline has been executed
- `bool eof_` - End of file indicator
- `std::stringstream buffer_` - Internal buffer for data processing
- `std::size_t total_size_` - Total size of processed data

## Public Methods

**Constructor/Destructor**

- `explicit Pipeliner(ProducerFn producer)` - Initializes pipeline with producer function

**Pipeline Construction Methods**

- `static PipelinerPtr create(ProducerFn producer)` - Creates pipeline with producer function
- `PipelinerPtr transform(TransformFn transform)` - Adds transformation function to pipeline

**Pipeline Execution and Control Methods**

- `void flush()` - Processes remaining data in pipeline

**Getters and Setters**

- `std::size_t get_total_size() const` - Returns total processed data size
- `void set_buffer_size(size_t size)` - Sets processing buffer size
- `void set_total_size(std::size_t size)` - Sets total data size

## Private Methods

**Pipeline Execution and Control Methods**

- `virtual int sync()` - Synchronizes pipeline processing
- `bool process_pipeline()` - Processes data through pipeline until buffer full
- `bool process_next_chunk()` - Processes single chunk through pipeline



# **CLI**

## Overview

CLI provides a command-line interface for interacting with the distributed file system. It processes user commands for file operations, directory navigation, and network connections.

## Constants

None defined in class scope.

## Variables

- `bool running_` - Flag indicating if CLI is active
- `store::Store& store_` - Reference to storage system
- `network::FileServer& file_server_` - Reference to file server component

## Public Methods

**Constructor/Destructor**

- `CLI(store::Store& store, network::FileServer& file_server)` - Initializes CLI with store and file server references

**Startup**

- `void run()` - Starts CLI command processing loop

## Private Methods

**Command Processing**

- `void process_command(const std::string& command, const std::string& filename)` - Routes commands to appropriate handlers
- `void handle_read_command(const std::string& filename)` - Processes file read requests
- `void handle_store_command(const std::string& filename)` - Processes file storage requests
- `void handle_connect_command(const std::string& connection_string)` - Processes network connection requests
- `void handle_delete_command(const std::string& filename)` - Processes file deletion requests
- `void handle_help_command()` - Displays help information
- `void log_and_display_error(const std::string& message, const std::string& error)` - Handles error logging and display