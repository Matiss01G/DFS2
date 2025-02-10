// ---- CRYPTO ----
// CipherContext Documentation
/*
DOCUMENTATION:
CLASS: CipherContext (RAII Wrapper)

VARIABLES:
  . EVP_CIPHER_CTX* ctx
      - OpenSSL cipher context pointer
      - Initialized to nullptr
      
CONSTRUCTOR:
  . CipherContext()
      - Creates new EVP cipher context
      - Throws runtime_error if context creation fails
      
METHODS:
  . ~CipherContext()
      - Destructor that frees the cipher context
  . EVP_CIPHER_CTX* get()
      - Returns the underlying cipher context pointer
*/

// CryptoStream Documentation
/*
DOCUMENTATION:
CLASS: CryptoStream

VARIABLES:
. static constexpr size_t KEY_SIZE = 32
    - 256 bits for AES-256 encryption
. static constexpr size_t IV_SIZE = 16  
    - 128 bits for CBC mode
. static constexpr size_t BLOCK_SIZE = 16
    - AES block size
. static constexpr size_t BUFFER_SIZE = 8192
    - Size of buffer for stream processing
. vector<uint8_t> key_
    - Stores encryption/decryption key
. vector<uint8_t> iv_
    - Stores initialization vector
. unique_ptr<CipherContext> context_
    - RAII managed cipher context
. bool is_initialized_ = false
    - Tracks initialization state
. Mode mode_ = Mode::Encrypt
    - Current operation mode
. istream* pending_input_ = nullptr
    - Tracks pending input stream

CONSTRUCTOR:
. CryptoStream()
    - Initializes OpenSSL algorithms
    - Creates cipher context
    - Sets up logging

METHODS:
Public:
  Initialization:
  . array<uint8_t, IV_SIZE> generate_IV() const
      - Generates random initialization vector
      - Returns 16-byte array for CBC mode
  . void initialize(const vector<uint8_t>& key, const vector<uint8_t>& iv)
      - Sets up encryption/decryption parameters
      - Validates key size
      - Stores key and IV

  Encryption/Decryption Operations:
  . ostream& encrypt(istream& input, ostream& output)
      - Encrypts input stream to output stream
      - Uses AES-256-CBC encryption
  . ostream& decrypt(istream& input, ostream& output)
      - Decrypts input stream to output stream
      - Uses AES-256-CBC decryption

  Getters/Setters:
  . void setMode(Mode mode)
      - Sets operation mode (Encrypt/Decrypt)
  . Mode getMode() const
      - Returns current operation mode

Private:
  Initialization:
  . void initializeCipher(bool encrypting)
      - Sets up cipher context for encryption/decryption
      - Configures AES-256-CBC mode

  Stream Processing:
  . void processStream(istream& input, ostream& output, bool encrypting)
      - Main stream processing loop
      - Handles both encryption and decryption
  . void saveStreamPos(istream& input, ostream& output, const function<void()>& operation)
      - Preserves stream positions
      - Provides exception safety
  . void processStreamData(istream& input, ostream& output, bool encrypting)
      - Processes data in chunks
      - Manages buffer operations
  . size_t processDataBlock(const uint8_t* inbuf, size_t bytes_read, uint8_t* outbuf, bool encrypting)
      - Processes individual data blocks
      - Handles encryption/decryption operations
  . void writeOutputBlock(ostream& output, const uint8_t* data, size_t length)
      - Writes processed data to output stream
      - Handles error checking
  . void processFinalBlock(uint8_t* outbuf, int& outlen, bool encrypting)
      - Handles final block with padding
      - Finalizes encryption/decryption

EXCEPTIONS:
. InitializationError
    - Thrown when initialization fails
. EncryptionError 
    - Thrown during encryption failures
. DecryptionError
    - Thrown during decryption failures
. runtime_error
    - Thrown for general stream operations failures
*/

// ByteOrder Documentation
/*
DOCUMENTATION:
CLASS: ByteOrder

VARIABLES:
  . None
CONSTRUCTOR:
  . None (all static methods)
METHODS:
  . static bool isLittleEndian()
      - Detects system endianness
      - Returns true if system is little endian
      - Uses bit pattern testing for detection

  . static T toNetworkOrder<T>(T value)
      - Converts host byte order to network byte order (big endian)
      - Template function that works with any numeric type T
      - Only swaps bytes if system is little endian
      - Returns the converted value

  . static T fromNetworkOrder<T>(T value)
      - Converts network byte order (big endian) to host byte order
      - Template function that works with any numeric type T
      - Only swaps bytes if system is little endian
      - Returns the converted value

  Private:
    . static T byteSwap<T>(T value)
        - Generic byte swap implementation
        - Works with any size T
        - Uses memcpy for safe type punning
        - Reverses byte order using std::reverse
        - Returns byte-swapped value
*/

// CryptoError Documentation
/*
DOCUMENTATION:
CLASS: CryptoError

VARIABLES:
 None (Inherits from std::runtime_error)
CONSTRUCTOR:
 . explicit CryptoError(const string& message)
     - Creates base crypto error with specified message
     - Inherits from std::runtime_error
METHODS:
 None (Uses inherited std::runtime_error functionality)

CLASS: InitializationError
VARIABLES:
 None (Inherits from CryptoError)
CONSTRUCTOR:
 . explicit InitializationError(const string& message)
     - Creates initialization error
     - Prepends "Initialization error: " to message
     - Inherits from CryptoError
METHODS:
 None (Uses inherited CryptoError functionality)

CLASS: EncryptionError
VARIABLES:
 None (Inherits from CryptoError)
CONSTRUCTOR:
 . explicit EncryptionError(const string& message)
     - Creates encryption error
     - Prepends "Encryption error: " to message
     - Inherits from CryptoError
METHODS:
 None (Uses inherited CryptoError functionality)

CLASS: DecryptionError
VARIABLES:
 None (Inherits from CryptoError)
CONSTRUCTOR:
 . explicit DecryptionError(const string& message)
     - Creates decryption error
     - Prepends "Decryption error: " to message
     - Inherits from CryptoError
METHODS:
 None (Uses inherited CryptoError functionality)
*/


// ---- FILE SERVER ----
// FileServer Documentation
/*
DOCUMENTATION:
CLASS: FileServer

VARIABLES:
  . uint32_t ID_
      - Unique identifier for the file server instance
  . vector<uint8_t> key_
      - Cryptographic key for secure communications (32 bytes)
  . unique_ptr<Store> store_
      - Manages local file storage operations
  . unique_ptr<Codec> codec_
      - Handles message encoding/decoding
  . Channel& channel_
      - Reference to communication channel
  . PeerManager& peer_manager_
      - Manages peer connections and communications
  . TCP_Server& tcp_server_
      - Handles TCP connections
  . mutex mutex_
      - Synchronizes access to shared resources
  . atomic<bool> running_{true}
      - Controls listener thread lifecycle
  . unique_ptr<thread> listener_thread_
      - Background thread for message processing

CONSTRUCTOR:
  . FileServer(uint32_t ID, const vector<uint8_t>& key, PeerManager& peer_manager, 
               Channel& channel, TCP_Server& tcp_server)
      - Initializes server with ID and crypto key
      - Sets up store directory and codec
      - Starts listener thread
      - Throws invalid_argument if key size invalid
      - Throws runtime_error if initialization fails

METHODS:
  Public:
    . bool connect(const string& remote_address, uint16_t remote_port)
        - Establishes connection to remote endpoint
        - Returns true if connection successful
    . bool store_file(const string& filename, istream& input)
        - Stores file locally and broadcasts to peers
        - Thread-safe operation
        - Returns true if store and broadcast successful
    . bool get_file(const string& filename)
        - Retrieves file from local store or network
        - Thread-safe operation
        - Returns true if file retrieved successfully
    . Store& get_store()
        - Returns reference to local store instance

  Private:
    Processing Outgoing Data:
    . bool prepare_and_send(const string& filename, MessageType message_type, optional<uint8_t> peer_id)
        - Prepares and sends file data to peers
        - Handles both targeted and broadcast sending
    . MessageFrame create_message_frame(const string& filename, MessageType message_type)
        - Creates frame with metadata and crypto IV
    . function<bool(stringstream&)> create_producer(const string& filename, MessageType message_type)
        - Creates data streaming function based on message type
    . function<bool(stringstream&, stringstream&)> create_transform(MessageFrame& frame, Pipeliner* pipeline)
        - Creates transformation function for frame serialization
    . bool send_pipeline(Pipeliner* const& pipeline, optional<uint8_t> peer_id)
        - Sends pipeline data to specific peer or broadcasts

    Processing Incoming Data:
    . void channel_listener()
        - Background thread monitoring channel for messages
    . void message_handler(const MessageFrame& frame)
        - Routes messages to appropriate handlers
    . bool handle_store(const MessageFrame& frame)
        - Processes incoming store requests
    . bool handle_get(const MessageFrame& frame)
        - Processes incoming get requests
    . string extract_filename(const MessageFrame& frame)
        - Extracts filename from message frame payload

    File Operations:
    . bool read_from_local_store(const string& filename)
        - Attempts to read file from local storage
    . bool retrieve_from_network(const string& filename)
        - Attempts to retrieve file from network peers
*/


// ---- LOGGER ----
// Logger Documentation
/*
DOCUMENTATION:
CLASS: Logger

VARIABLES:
  . None (static-only class)

CONSTRUCTOR:
  . None (static-only class)

METHODS:
  . static void init(const string& log_file = "dfs_crypto.log")
      - Initializes the logging system
      - Sets up file sink with specified log file
      - Configures timestamp and severity formatting
      - Establishes minimum severity level as trace
      - Parameters:
          - log_file: Path to log file (default: "dfs_crypto.log")

  . static boost::log::sources::severity_logger<boost::log::trivial::severity_level>& get_logger()
      - Returns reference to singleton severity logger
      - Thread-safe access to logger instance
      - Used by logging macros

LOGGING MACROS:
  . DFS_LOG_TRACE
      - Logs trace-level messages
      - Lowest severity level
      - Used for detailed debugging information

  . DFS_LOG_DEBUG
      - Logs debug-level messages
      - Used for debugging information
      - More selective than trace

  . DFS_LOG_INFO
      - Logs info-level messages
      - Used for general operational information
      - Normal application behavior

  . DFS_LOG_WARN
      - Logs warning-level messages
      - Used for potentially harmful situations
      - Application continues to function

  . DFS_LOG_ERROR
      - Logs error-level messages
      - Used for error conditions
      - May affect specific operations

  . DFS_LOG_FATAL
      - Logs fatal-level messages
      - Used for severe error conditions
      - May prevent application from functioning

LOG FORMAT:
  . Timestamp: [YYYY-MM-DD HH:MM:SS]
  . Severity Level: [trace/debug/info/warning/error/fatal]
  . Message: User-provided message text
*/


// ---- NETWORK ----
// Bootstrap Documentation
/*
DOCUMENTATION:
CLASS: Bootstrap

VARIABLES:
. string address_
    - Network address for the bootstrap node
. uint16_t port_
    - Port number for network communication
. vector<string> bootstrap_nodes_
    - List of remote bootstrap nodes to connect to
. vector<uint8_t> key_
    - Cryptographic key for secure communications
. uint8_t ID_
    - Unique identifier for this bootstrap node
. unique_ptr<Channel> channel_
    - Handles message passing between components
. unique_ptr<TCP_Server> tcp_server_
    - Manages TCP connections and networking
. unique_ptr<PeerManager> peer_manager_
    - Manages peer connections and communications
. unique_ptr<FileServer> file_server_
    - Handles file storage and retrieval operations

CONSTRUCTOR:
. Bootstrap(const string& address, uint16_t port, const vector<uint8_t>& key, 
           uint8_t ID, const vector<string>& bootstrap_nodes)
    - Initializes bootstrap node with network settings
    - Creates and initializes all system components
    - Components created in dependency order
    - Throws exception if initialization fails

METHODS:
Public:
  Initialization:
  . bool start()
      - Starts TCP server and connects to bootstrap nodes
      - Returns true if startup successful
  . bool shutdown()
      - Terminates all components in reverse dependency order
      - Returns true if shutdown successful
  . bool connect_to_bootstrap_nodes()
      - Attempts connection to all configured bootstrap nodes
      - Returns true if all connections successful

  Getters/Setters:
  . PeerManager& get_peer_manager()
      - Returns reference to peer manager instance
  . FileServer& get_file_server()
      - Returns reference to file server instance

Private:
  None
*/

// Channel Documentation
/*
DOCUMENTATION:
CLASS: Channel

VARIABLES:
 . mutex mutex_
     - Synchronizes access to message queue
 . queue<MessageFrame> queue_
     - FIFO queue for message frames
     
CONSTRUCTOR:
 . Channel()
     - Default constructor
     - Initializes empty message queue
     
METHODS:
 Public:
   Channel Control:
   . void produce(const MessageFrame& frame)
       - Adds message frame to back of queue
       - Thread-safe operation
   . bool consume(MessageFrame& frame)
       - Retrieves next message frame from queue
       - Returns false if queue is empty
       - Thread-safe operation
       
   Query Methods:
   . bool empty() const
       - Checks if channel has no messages
       - Thread-safe operation
       - Returns true if queue is empty
   . size_t size() const
       - Gets number of messages in channel
       - Thread-safe operation
       - Returns current queue size
 Private:
   None
*/

// Codec Documentation
/*
DOCUMENTATION:
CLASS: Codec

VARIABLES:
 . vector<uint8_t> key_
     - Cryptographic key for encryption/decryption
 . Channel& channel_
     - Reference to channel for frame distribution
CONSTRUCTOR:
 . explicit Codec(const vector<uint8_t>& key, Channel& channel)
     - Initializes codec with crypto key and channel
     - Throws runtime_error if initialization fails
     
METHODS:
 Public:
   Serialization:
   . size_t serialize(const MessageFrame& frame, ostream& output) 
       - Encrypts and writes frame to output stream
       - Returns total bytes written
       - Throws runtime_error if serialization fails
   . MessageFrame deserialize(istream& input)
       - Reads and decrypts frame from input stream
       - Adds frame to channel queue
       - Returns deserialized message frame
       - Throws runtime_error if deserialization fails
       
 Private:
   Stream Operations:
   . void write_bytes(ostream& output, const void* data, size_t size)
       - Writes raw bytes to output stream
       - Throws runtime_error if write fails
   . void read_bytes(istream& input, void* data, size_t size)
       - Reads raw bytes from input stream
       - Throws runtime_error if read fails
       
   Byte Order Conversion:
   . static uint32_t to_network_order(uint32_t host_value)
       - Converts 32-bit value to network byte order
   . static uint64_t to_network_order(uint64_t host_value)
       - Converts 64-bit value to network byte order
   . static uint32_t from_network_order(uint32_t network_value)
       - Converts 32-bit value from network to host order
   . static uint64_t from_network_order(uint64_t network_value)
       - Converts 64-bit value from network to host order
       
   Utility Methods:
   . static size_t get_padded_size(size_t original_size)
       - Calculates size after encryption padding
*/

// Message_frame Documentation 
/*
DOCUMENTATION:
CLASS: MessageFrame

VARIABLES:
 . vector<uint8_t> iv_
     - Initialization vector for encryption
 . MessageType message_type
     - Type of message (STORE_FILE or GET_FILE)
 . uint8_t source_id
     - ID of sending peer
 . uint64_t payload_size
     - Size of payload data in bytes
 . uint32_t filename_length
     - Length of filename in payload
 . shared_ptr<stringstream> payload_stream
     - Stream containing payload data
     
CONSTRUCTOR:
 None (struct with default construction)
 
METHODS:
 None (Data-only structure)

ENUM: MessageType
VALUES:
 . STORE_FILE = 0
     - Message is a file storage request
 . GET_FILE = 1
     - Message is a file retrieval request
*/

// Peer_Manager Documentation
/*
DOCUMENTATION:
CLASS: PeerManager

VARIABLES:
 . Channel& channel_
     - Reference to channel for message distribution
 . TCP_Server& tcp_server_
     - Reference to TCP server handling connections
 . vector<uint8_t> key_
     - Cryptographic key for secure communications (32 bytes)
 . map<uint8_t, shared_ptr<TCP_Peer>> peers_
     - Map of connected peers indexed by peer ID
 . mutex mutex_
     - Synchronizes access to peers map
     
CONSTRUCTOR:
 . PeerManager(Channel& channel, TCP_Server& tcp_server, const vector<uint8_t>& key)
     - Initializes manager with channel, server and crypto key
     - Validates key size (must be 32 bytes)
     - Throws invalid_argument if key size invalid
     - Copy constructor and assignment operator deleted
     
METHODS:
 Public:
   Connection Management:
   . bool disconnect(uint8_t peer_id)
       - Disconnects peer and closes socket
       - Returns true if disconnect successful
   . bool is_connected(uint8_t peer_id)
       - Checks if peer's socket is open
       - Returns true if peer connected 
       
   Peer Management:
   . void create_peer(shared_ptr<tcp::socket> socket, uint8_t peer_id)
       - Creates new TCP peer with given socket and ID
       - Sets up stream processing for peer
       - Restarts TCP server accept after creation
   . void add_peer(shared_ptr<TCP_Peer> peer)
       - Adds peer to managed peers map
       - Thread-safe operation
   . void remove_peer(uint8_t peer_id)
       - Removes and disconnects peer
       - Thread-safe operation
   . bool has_peer(uint8_t peer_id)
       - Checks if peer exists in map
       - Thread-safe operation
   . shared_ptr<TCP_Peer> get_peer(uint8_t peer_id)
       - Returns pointer to peer if exists, nullptr otherwise
       - Thread-safe operation
       
   Stream Operations:
   . bool send_to_peer(uint8_t peer_id, Pipeliner& pipeline)
       - Sends pipeline data to specific peer
       - Returns true if send successful
       - Validates pipeline and peer status
   . bool broadcast_stream(Pipeliner& pipeline)
       - Sends pipeline data to all connected peers
       - Returns true if all sends successful
       - Handles disconnected peers and errors
       
   Utility Methods:
   . size_t size() const
       - Returns number of managed peers
       - Thread-safe operation
   . void shutdown()
       - Disconnects all peers and cleans up
       - Thread-safe operation
 Private:
   None
*/

// Peer Documentation
/*
DOCUMENTATION:
CLASS: Peer

VARIABLES:
 . using StreamProcessor = function<void(istream&)>
     - Function type for stream data processing
     
CONSTRUCTOR:
 . Peer()
     - Protected default constructor
     - Virtual destructor for inheritance
     
METHODS:
 Public:
   Stream Control:
   . virtual bool start_stream_processing() = 0
       - Starts processing incoming stream data
       - Returns true if processing started successfully
   . virtual void stop_stream_processing() = 0
       - Stops processing incoming stream data
       
   Stream Operations:
   . virtual bool send_message(const string& message, size_t total_size) = 0
       - Sends text message to peer
       - Returns true if send successful
   . virtual bool send_stream(istream& input_stream, size_t total_size, size_t buffer_size = 8192) = 0
       - Sends input stream data to peer
       - Uses specified buffer size for transfers
       - Returns true if send successful
       
   Getters and Setters:
   . virtual istream* get_input_stream() = 0
       - Returns pointer to input stream
   . virtual void set_stream_processor(StreamProcessor processor) = 0
       - Sets callback for processing received data
       
 Protected:
   None
   
 Private:
   None
*/

// TCP_Peer Documentation
/*
DOCUMENTATION:
CLASS: TCP_Peer

VARIABLES:
 . uint8_t peer_id_
     - Unique identifier for this peer
 . StreamProcessor stream_processor_
     - Callback function for processing received data
 . size_t expected_size_
     - Size of next incoming data chunk
 . unique_ptr<streambuf> input_buffer_
     - Buffer for incoming data
 . unique_ptr<istream> input_stream_
     - Stream for reading buffered data
 . io_context io_context_
     - Manages asynchronous operations
 . mutex io_mutex_
     - Synchronizes socket I/O operations
 . unique_ptr<tcp::socket> socket_
     - TCP socket for peer connection
 . unique_ptr<tcp::endpoint> endpoint_
     - Remote endpoint information
 . unique_ptr<thread> processing_thread_
     - Thread for asynchronous data processing
 . atomic<bool> processing_active_{false}
     - Controls processing thread lifecycle
 . unique_ptr<Codec> codec_
     - Handles message encoding/decoding
     
CONSTRUCTOR:
 . explicit TCP_Peer(uint8_t peer_id, Channel& channel, const vector<uint8_t>& key)
     - Initializes peer with ID, channel and crypto key
     - Sets up socket and streams
     - Copy constructor and assignment deleted
     
METHODS:
 Public:
   Stream Control:
   . bool start_stream_processing() override
       - Starts async processing thread
       - Returns false if socket closed or no processor
   . void stop_stream_processing() override
       - Stops processing and joins thread
       - Cancels pending operations
       
   Stream Operations:
   . bool send_stream(istream& input_stream, size_t total_size, size_t buffer_size = 8192)
       - Sends stream data to peer in chunks
       - Returns true if all data sent successfully
   . bool send_message(const string& message, size_t total_size)
       - Convenience method to send string data
       - Returns true if send successful
       
   Getters/Setters:
   . istream* get_input_stream() override
       - Returns input stream if connected
   . uint8_t get_peer_id() const
       - Returns peer's unique ID
   . tcp::socket& get_socket()
       - Returns reference to TCP socket
   . void set_stream_processor(StreamProcessor processor)
       - Sets callback for processing received data
       
 Private:
   Stream Control:
   . void initialize_streams()
       - Sets up input buffer and stream
       
   Incoming Data:
   . void process_stream()
       - Main async processing loop
   . void handle_read_size(error_code& ec, size_t bytes)
       - Handles size prefix read completion
   . void handle_read_data(error_code& ec, size_t bytes)  
       - Handles data chunk read completion
   . void process_received_data()
       - Processes data using stream processor
   . void async_read_next()
       - Initiates next async read operation
       
   Outgoing Data:
   . bool send_size(size_t total_size)
       - Sends size prefix for framing
       
   Cleanup:
   . void cleanup_connection()
       - Closes socket and resets state
*/

// TCP_Server Documentation
/*
DOCUMENTATION:
CLASS: TCP_Server

VARIABLES:
 . const uint8_t ID_
     - Unique identifier for this server instance
 . const uint16_t port_
     - Port number server listens on
 . const string address_
     - Network address server binds to
 . unique_ptr<thread> io_thread_
     - Thread running io_context event loop
 . bool is_running_
     - Indicates if server is actively running
 . io_context io_context_
     - Manages asynchronous operations
 . unique_ptr<tcp::acceptor> acceptor_
     - Accepts incoming TCP connections
 . PeerManager* peer_manager_
     - Pointer to peer management system
     
CONSTRUCTOR:
 . TCP_Server(uint16_t port, const string& address, uint8_t ID)
     - Initializes server with network settings and ID
     - Does not start listening immediately
     
METHODS:
 Public:
   Initialization:
   . bool start_listener()
       - Starts accepting connections
       - Creates acceptor and io_thread
       - Returns true if startup successful
   . void shutdown()
       - Stops accepting connections
       - Cleans up resources and joins thread
       
   Connection:
   . bool connect(const string& remote_address, uint16_t remote_port)
       - Establishes connection to remote server
       - Performs handshake exchange
       - Returns true if connection successful
       
   Setters:
   . void set_peer_manager(PeerManager& peer_manager)
       - Sets peer management system
       
 Private:
   Initialization:
   . void start_accept()
       - Sets up async accept for new connections
       - Continues accepting while running
       
   Connection:
   . bool initiate_connection(const string& address, uint16_t port, shared_ptr<tcp::socket>& socket)
       - Resolves address and connects socket
       - Returns true if connection established
       
   Handshake:
   . bool initiate_handshake(shared_ptr<tcp::socket> socket)
       - Exchanges IDs with remote peer
       - Creates new peer if successful
   . bool send_ID(shared_ptr<tcp::socket> socket)
       - Sends local ID to remote peer
   . void receive_handshake(shared_ptr<tcp::socket> socket)  
       - Handles incoming handshake request
       - Creates peer after ID exchange
   . uint8_t read_ID(shared_ptr<tcp::socket> socket)
       - Reads peer ID from socket
*/


// ---- STORE ----
// Store Documentation
/*
DOCUMENTATION:
CLASS: Store

VARIABLES:
. std::filesystem::path base_path_
    - Root directory path for all stored files
    - Stores files in content-addressed subdirectories

CONSTRUCTOR:
. explicit Store(const std::string& base_path)
    - Initializes store with base directory path
    - Creates directory if it doesn't exist
    - Throws StoreError if directory creation fails

METHODS:
Public:
  Core Storage:
  . void store(const std::string& key, std::istream& data)
      - Stores input stream data under given key
      - Creates content-addressed file path using key hash
      - Handles empty streams and large files efficiently
      - Throws StoreError on I/O failures
  . void get(const std::string& key, std::stringstream& output) 
      - Retrieves data associated with key into output stream
      - Handles empty files and large files in chunks
      - Throws StoreError if file not found or I/O errors
  . void remove(const std::string& key)
      - Deletes file associated with key
      - Throws StoreError if file not found or deletion fails
  . void clear()
      - Removes all stored files and resets store
      - Recreates empty base directory

  Query Operations:
  . bool has(const std::string& key) const
      - Checks if data exists for given key
      - Returns true if file exists, false otherwise
  . std::uintmax_t get_file_size(const std::string& key) const
      - Returns size of stored file in bytes
      - Throws StoreError if file not found

  CLI Support:
  . bool read_file(const std::string& key, size_t lines_per_page) const
      - Displays file contents with pagination
      - Returns false if file not found or read error
  . void print_working_dir() const
      - Displays current working and store directories
  . void list() const
      - Lists files in working and store directories
  . void move_dir(const std::string& path)
      - Changes store base directory
      - Throws StoreError if path invalid or inaccessible
  . void delete_file(const std::string& filename)
      - Removes file and cleans up empty directories
      - Throws StoreError if file not found or deletion fails

Private:
  Content-Addressed Storage:
  . std::string hash_key(const std::string& key) const
      - Generates SHA-256 hash of key using OpenSSL
      - Returns hex string representation
      - Throws StoreError on hashing failures
  . std::filesystem::path get_path_for_hash(const std::string& hash) const
      - Creates hierarchical path from hash
      - Format: base_path/hash[0:2]/hash[2:4]/hash[4:6]/remaining_hash

  Utility Methods:
  . void check_directory_exists(const std::filesystem::path& path) const
      - Creates directory if it doesn't exist
      - Creates parent directories as needed
  . std::filesystem::path resolve_key_path(const std::string& key) const
      - Converts key to content-addressed file path
      - Combines hash_key and get_path_for_hash
  . void verify_file_exists(const std::filesystem::path& file_path) const
      - Checks if file exists at path
      - Throws StoreError if not found

  CLI Support:
  . bool display_file_contents(std::ifstream& file, const std::string& key, size_t lines_per_page) const
      - Implements paginated file display
      - Handles user interaction for navigation
*/