# Distributed File System (DFS)

## Overview

This project is a basic implementation of a Distributed File System (DFS) in C++, designed as a learning exercise to explore and gain hands-on experience with:

- **Networking**: Implementation of peer-to-peer communication using TCP/IP
- **Multithreading**: Managing concurrent operations and thread synchronization
- **Pipelining**: Stream processing and data transformation
- **Modern C++**: Practical application of C++ programming concepts
- **Security**: Basic implementation of encrypted file transfer using AES-256

While this implementation includes essential features like encrypted file transfer and peer-to-peer sharing, it's important to note that this is an educational project rather than a production-ready system. The focus was on learning and understanding the underlying concepts rather than creating a fully optimized DFS. 

## Prerequisites

Before building the DFS, ensure your system meets the following requirements:

### System Requirements

- Linux (Ubuntu/Debian) or macOS
- At least 1GB of free disk space
- Minimum 2GB RAM
- Internet connection for downloading dependencies

### Required Software Versions

- C++17 compatible compiler (GCC 13.2.0 or higher)
- CMake 3.29.2 or higher
- Git (for cloning the repository)

## Quick Start

```bash
# Clone the repository
git clone https://github.com/username/dfs.git
cd dfs

# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential pkg-config cmake libssl-dev libboost-all-dev libgtest-dev

# Build the project
mkdir build && cd build
cmake ..
make

# Start first peer node
./dfs_main -h 127.0.0.1 -p 3001

# In a new terminal, start second peer node
./dfs_main -h 127.0.0.1 -p 3002

# Connect peers (in either terminal's DFS shell)
DFS_Shell> connect 127.0.0.1:3002

# Use the DFS
```

## Features

- **Peer-to-Peer Architecture**: Decentralized file sharing between multiple nodes
- **Secure File Transfer**: AES-256-CBC encryption for all file transfers
- **Automatic File Synchronization**: Stored files automatically propagate across connected peers
- **Content-Addressable Storage**: SHA-256 based content addressing for efficient storage
- **Command-Line Interface**: Easy-to-use CLI for system interaction

## Architecture Overview

The DFS consists of several key components:

- **FileServer**: Core component handling file operations and peer-to-peer sharing
- **CryptoStream**: Provides AES-256-CBC encryption for secure file transfers
- **Store**: Manages content-addressable storage using SHA-256 hashing
- **TCP_Peer**: Handles the exchange of data between 2 connected peers
- **PeerManager**: Handles peer connections
- **Channel**: Provides message queuing for communication between codec and FileServer
- **TCP_Server**: Manages network connections and peer handshaking
- **Bootstrap**: Coordinates initialization and lifecycle of DFS nodes for testing purposes

## Documentation

Detailed documentation for this project can be found in the following files:

- [documentation.md](./documentation.md): Contains comprehensive documentation about the system architecture, components, and implementation details
- [test_documentation.md](./test_documentation.md): Contains detailed information about the test suite and test case documentation

## Dependencies

- boost (1.81.0)
- cmake (3.29.2)
- gcc (13.2.0)
- gtest (1.14.0)
- openssl (3.0.13)
- pkg-config (0.29.2)

## Installing Dependencies

### Ubuntu/Debian

```bash
# Install build essentials
sudo apt-get install build-essential

# Install required packages
sudo apt-get install pkg-config cmake libssl-dev libboost-all-dev libgtest-dev

```

### macOS (using Homebrew)

```bash
# Install required packages
brew install gcc pkg-config cmake openssl boost googletest

```

## Building the Project

1. Create and enter build directory:

```bash
mkdir build
cd build

```

1. Generate build files:

```bash
cmake ..

```

1. Build the project:

```bash
make

```

## Running the DFS

### Starting a Peer Node

The DFS requires AT LEAST 2 participating peers to form a network. Each peer must be started with unique port numbers. 

This DFS is only tested to function locally, thus each peer must be created with an IP address of 127.0.0.1

```bash
./dfs_main [OPTIONS]

Options:
-h, --host <IP>        Local IP address (required)
-p, --port <number>    Port number to listen on (required)

```

Example: Starting two peers in different terminal windows:

```bash
# Terminal 1 - Peer 1
./dfs_main -h 127.0.0.1 -p 3001

# Terminal 2 - Peer 2
./dfs_main -h 127.0.0.1 -p 3002

```

### Connecting Peers

After starting multiple peers, establish connections between them using the CLI:

```bash
DFS_Shell> connect 127.0.0.1:3002

```

## CLI Commands

The DFS provides a command-line interface with the following commands:

```
help              Display help message
pwd               Print current working directory
ls                List files in current directory
cd <dir>          Change to directory <dir>
read <file>       Read contents of <file>
store <file>      Store local <file> in DFS
delete <file>     Delete <file> from DFS
connect <ip:port> Connect to DFS server at <ip:port>
quit              Exit the DFS shell

```

## Running Tests

The project includes comprehensive test suites that can be run individually or together:

```bash
# Run individual test suites
./codec_tests
./bootstrap_tests
./channel_tests
./store_tests
./crypto_tests

# Run all tests
./all_tests

```

## Project Information

- **Date**: 11/02/2025
- **Version**: 1.0
- **Author**: Marin Gallien
- **Copyright**: © 2025 Marin Gallien
- **License**: MIT License

## License

MIT License

Copyright (c) 2025 Marin Gallien

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
