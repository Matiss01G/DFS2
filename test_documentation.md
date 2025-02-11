# Store Tests

## Overview

This test suite validates the Store's core operations, error handling, edge cases, and concurrent access behavior.

## Test Environment Setup

Each test case runs in an isolated environment with the following setup:

- Creates a unique temporary directory using system clock timestamp
- Instantiates a new Store instance pointing to this directory
- Automatically cleans up test directories after each test

## Test Cases

### Basic Operations (BasicOperations)

This test validates the fundamental operations of storing and retrieving data, including handling of empty data and non-existent keys.

**Key Assertions:**

1. Successfully stores and retrieves data with matching content
2. Successfully stores and retrieves empty data
3. Throws appropriate error when attempting to retrieve non-existent key
4. Correctly reports key existence status via has() method

### Multiple Files (MultipleFiles)

This test verifies the Store's ability to handle multiple distinct key-value pairs simultaneously, ensuring proper isolation and retrieval of each stored item.

**Key Assertions:**

1. Successfully stores multiple key-value pairs
2. Correctly retrieves each stored value without cross-contamination
3. Maintains data integrity for all stored pairs
4. All keys remain accessible throughout the test

### Error Handling (ErrorHandling)

This test verifies the Store's response to various error conditions and its ability to handle invalid inputs and cleanup operations.

**Key Assertions:**

1. Throws StoreError when attempting to store from an invalid stream
2. Successfully clears all stored data when clear() is called
3. Removes key accessibility after clear operation
4. Properly propagates errors without corrupting store state

### Edge Cases (EdgeCases)

This test validates the Store's handling of boundary conditions and potential security-sensitive inputs, ensuring robust operation under extreme conditions.

**Key Assertions:**

1. Properly handles or rejects empty keys
2. Safely handles path traversal attempts without security breaches
3. Correctly processes keys of maximum length (1024 characters)
4. Safely handles absolute path inputs
5. Properly manages Windows-style path separators

### Advanced Operations (AdvancedOperations)

This test validates the Store's performance with large files and verifies proper handling of file size tracking and content updates.

**Key Assertions:**

1. Successfully stores and retrieves 1MB file without data corruption
2. Accurately reports file size for stored content
3. Successfully overwrites existing key with new content
4. Correctly updates file size after content overwrite

### Concurrent Access (ConcurrentAccess)

This test verifies the Store's thread safety and ability to handle multiple simultaneous operations without data corruption or state inconsistency.

**Key Assertions:**

1. Successfully completes all 250 concurrent operations
2. Maintains data integrity across all concurrent operations
3. Each thread successfully stores and verifies its data
4. No operations fail due to race conditions
5. Final operation count matches expected total (5 threads × 50 operations)

## Helper Methods

- `void store_and_verify(const std::string& key, const std::string& data)` - A utility method that stores data, retrieves the data and compares for equality
- `void expect_retrieval_fails(const std::string& key)` - A utility method that verifies proper handling of non-existent or invalid keys.
- `static std::unique_ptrstd::stringstream create_test_stream(const std::string& content)` - A utility method that creates properly formatted test input streams.



# CryptoStream Tests

## Overview

This test suite validates the CryptoStream's encryption and decryption operations, error handling, initialization behavior, and performance with various data sizes.

## Test Environment Setup

Each test case runs with the following setup:

- Creates a CryptoStream instance with default configuration
- Initializes with test key (0x42) and IV (0x24) of appropriate sizes
- Uses Google Test framework for assertions and test organization
- Provides helper methods for stream comparison and validation

## Test Cases

### Basic Stream Operation (BasicStreamOperation)

This test validates the fundamental encryption and decryption operations using string streams.

**Key Assertions:**

1. Successfully encrypts plaintext data
2. Encrypted data differs from original plaintext
3. Successfully decrypts data back to original plaintext
4. Maintains data integrity through encryption/decryption cycle

### Uninitialized Error (UninitializedError)

This test verifies proper error handling when using an uninitialized CryptoStream.

**Key Assertions:**

1. Throws InitializationError when attempting encryption without initialization
2. Properly prevents operations on uninitialized streams
3. Reports appropriate error messages for initialization state

### Empty Stream (EmptyStream)

This test validates handling of empty input streams.

**Key Assertions:**

1. Successfully processes empty input streams
2. Produces non-empty encrypted output (due to padding)
3. Correctly recovers empty plaintext after decryption
4. Maintains consistency with empty data handling

### Large Stream (LargeStream)

This test verifies the CryptoStream's ability to handle large data volumes.

**Key Assertions:**

1. Successfully processes 1MB of sequential test data
2. Maintains data integrity for large streams
3. Correctly handles memory management for large data
4. Performs efficient streaming operations

### Invalid Stream State (InvalidStreamState)

This test validates error handling for streams in invalid states.

**Key Assertions:**

1. Throws runtime_error for bad input streams
2. Throws runtime_error for bad output streams
3. Properly handles stream state checking
4. Maintains consistent error reporting

### Block Alignment (BlockAlignment)

This test verifies proper handling of data with various sizes around block boundaries.

**Key Assertions:**

1. Correctly processes data sizes near block boundaries (15, 16, 17 bytes)
2. Correctly processes data sizes near double block boundaries (31, 32, 33 bytes)
3. Maintains data integrity regardless of block alignment
4. Properly handles padding for non-aligned data

### IV Generation (IVGeneration)

This test validates the initialization vector generation functionality.

**Key Assertions:**

1. Generates IVs of correct size
2. Produces unique IVs across multiple generations
3. Successfully initializes with generated IVs
4. Maintains encryption/decryption functionality with generated IVs

## Helper Methods

- `streamsEqual(std::istream& s1, std::istream& s2)` - A static helper method for comparing stream contents.



# Codec Tests

## Overview

This test suite validates the network Codec's serialization and deserialization operations, focusing on message frame handling, payload processing, and edge cases in a network communication context.

## Test Environment Setup

Each test case runs with the following setup:

- Creates a Codec instance with a test encryption key (32 bytes of 0x42)
- Uses a Channel instance for message handling
- Initializes with Google Test framework and Boost logging
- Provides comprehensive helper methods for frame creation and verification

## Test Cases

### Minimal Frame Serialize/Deserialize (MinimalFrameSerializeDeserialize)

This test validates the basic serialization and deserialization of a minimal message frame.

**Key Assertions:**

1. Successfully serializes minimal frame with basic attributes
2. Successfully deserializes frame back to original state
3. Maintains frame integrity through serialization cycle
4. Properly handles minimal frame configuration

### Basic Serialize/Deserialize (BasicSerializeDeserialize)

This test verifies standard frame processing with a small payload.

**Key Assertions:**

1. Successfully processes frames with payload data
2. Maintains data integrity for basic payloads
3. Correctly handles filename length parameters
4. Preserves all frame attributes during processing

### Large Payload Chunked Processing (LargePayloadChunkedProcessing)

This test validates the Codec's ability to handle large data payloads.

**Key Assertions:**

1. Successfully processes 10MB payload data
2. Maintains data integrity for large payloads
3. Properly handles chunked data processing
4. Manages memory efficiently for large data transfers

### Empty Source ID (EmptySourceId)

This test verifies handling of frames with zero source ID.

**Key Assertions:**

1. Successfully processes frames with empty source ID
2. Maintains proper frame structure with zero values
3. Correctly serializes and deserializes empty IDs
4. Handles edge case of zero identifier

## Helper Methods

- `generate_random_data(size_t size)` - Generates random test data of specified size.
- `generate_test_iv()` - Generates test initialization vector.
- `createBasicFrame(uint32_t source_id, size_t payload_size, size_t filename_length)` - Creates a message frame with standard test configuration.
- `addPayload(MessageFrame& frame, const std::string& data)` - Adds payload data to a message frame.
- `verifySerializeDeserialize(const MessageFrame& input_frame)` - Comprehensive frame verification through serialization cycle.
- `verifyFramesMatch(const MessageFrame& input_frame, const MessageFrame& output_frame)` - Detailed frame comparison verification.



# Channel Tests

## Overview

This test suite validates the Channel's message passing capabilities, focusing on thread-safe producer-consumer patterns, concurrent operations, and message ordering in a network communication context.

## Test Environment Setup

Each test case runs with the following setup:

- Creates a Channel instance for message passing
- Uses Google Test framework for assertions
- Provides helper methods for message frame creation and verification
- Implements thread-safe testing utilities

## Test Cases

### Initial State (InitialState)

This test validates the initial state of a newly created channel.

**Key Assertions:**

1. Channel is empty on creation
2. Initial size is zero
3. Proper initialization of internal state
4. Channel ready for operations

### Single Produce Consume (SingleProduceConsume)

This test verifies basic single-threaded produce and consume operations.

**Key Assertions:**

1. Successfully produces single message
2. Correctly updates channel size
3. Successfully consumes message with matching content
4. Returns to empty state after consumption

### Multiple Messages (MultipleMessages)

This test validates handling of multiple sequential messages.

**Key Assertions:**

1. Successfully produces multiple frames sequentially
2. Maintains correct message ordering
3. Accurately tracks channel size
4. Properly consumes all messages in FIFO order

### Consume Empty Channel (ConsumeEmptyChannel)

This test verifies behavior when attempting to consume from an empty channel.

**Key Assertions:**

1. Returns false when consuming from empty channel
2. Maintains consistent empty state
3. Properly handles invalid consume attempts
4. No state corruption on empty consume

### Concurrent Producers Consumers (ConcurrentProducersConsumers)

This test validates concurrent multi-threaded operations.

**Key Assertions:**

1. Successfully handles 4 concurrent producer threads
2. Successfully handles 4 concurrent consumer threads
3. Processes 50 messages per producer (200 total)
4. Maintains data integrity under concurrent access
5. No messages lost during concurrent operations

### Alternating Produce Consume (AlternatingProduceConsume)

This test verifies behavior with alternating producer and consumer threads.

**Key Assertions:**

1. Successfully coordinates single producer and consumer
2. Processes 50 iterations of produce-consume cycles
3. Maintains message ordering during alternation
4. Properly handles thread synchronization

## Helper Methods

- `createFrame(uint8_t source_id, const std::string& payload)` - Creates a message frame with specified parameters.
- `verifyFrameEquals(const MessageFrame& actual, const MessageFrame& expected)` - Comprehensive frame comparison verification.
- `runProducer(int start_id, int count, std::chrono::microseconds delay)` - This method simulates real-world producer behavior with controlled timing and message generation.
- `runConsumer(std::atomic<int>& consumed_count, std::atomic<bool>& done)` - This method provides controlled message consumption with accurate tracking



# Bootstrap Tests

## Overview

This test suite validates the Bootstrap component's peer-to-peer networking capabilities, focusing on peer connection management, file sharing, and distributed file synchronization across multiple nodes.

## Test Environment Setup

Each test case runs with the following setup:

- Uses local network (127.0.0.1) for peer communication
- Standard test encryption key (32 bytes of 0x42)
- Configurable file sizes and chunk sizes for testing
- Automatic cleanup of test files and peer connections
- Google Test framework integration

## Test Cases

### Peer Connection (PeerConnection)

This test validates basic peer connection establishment.

**Key Assertions:**

1. Successfully establishes connection between two peers
2. Verifies bidirectional peer recognition
3. Confirms peer manager state consistency
4. Validates connection stability

### Duplicate Peer Connection (DuplicatePeerConnection)

This test verifies handling of repeated connection attempts.

**Key Assertions:**

1. Successfully establishes initial peer connection
2. Correctly handles duplicate connection attempts
3. Maintains existing connections after duplicate attempts
4. Returns appropriate status for duplicate connections

### File Sharing (FileSharing)

This test validates basic file sharing between peers.

**Key Assertions:**

1. Successfully stores file in source peer
2. Automatically propagates file to connected peer
3. Maintains file integrity during transfer
4. Verifies consistent file availability across peers

### Large File Sharing (LargeFileSharing)

This test verifies handling of large file transfers.

**Key Assertions:**

1. Successfully transfers 2MB test file
2. Maintains data integrity for large files
3. Handles chunked file transfer correctly
4. Verifies complete file reconstruction

### Broadcast File Sharing (BroadcastFileSharing)

This test validates file propagation across multiple peers.

**Key Assertions:**

1. Successfully connects three-peer network
2. Propagates files to all connected peers
3. Maintains file consistency across network
4. Verifies complete peer mesh connectivity

### Large File Broadcast (LargeFileBroadcast)

This test verifies large file distribution across multiple peers.

**Key Assertions:**

1. Successfully broadcasts 2MB file across network
2. Maintains data integrity during multi-peer transfer
3. Handles concurrent large file transfers
4. Verifies consistent file state across all peers

### Get File (GetFile)

This test validates on-demand file retrieval.

**Key Assertions:**

1. Successfully retrieves file from remote peer
2. Maintains file integrity during transfer
3. Properly stores retrieved file locally
4. Verifies file availability after retrieval

### Large Get File (LargeGetFile)

This test verifies retrieval of large files.

**Key Assertions:**

1. Successfully retrieves 2MB file from remote peer
2. Maintains data integrity for large transfers
3. Handles chunked file retrieval correctly
4. Verifies complete file reconstruction

## Helper Methods

- `create_peer(uint8_t id, uint16_t port, std::vectorstd::string bootstrap_nodes)` - Creates and initializes a new peer node in the network.
- `start_peer(Peer* peer, bool wait)` - Initiates peer network operations in a thread-safe manner.
- `create_large_file(size_t target_size)` - Generates large test files with verifiable content structure.
- `verify_peer_connections(const std::vector<Peer*>& test_peers)` - Verifies a peer is connected to a list of peers
- `verify_file_content(const std::string& filename, const std::string& expected_content, const std::vector<Peer*>& test_peers)` - Verifies file content of a file present in a peer’s store matches the expected contents. Repeats for list of peers.