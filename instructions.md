# Distributed File System (DFS)

**Version**: 1.0

**Author**: Marin Gallien

**Copyright**: 2025 Marin Gallien

This program showcases a basic C++ implementation  of a Distributed File System(). The instructions below will guide the user on how to install required dependencies and set up the DFS for local use. The instructions will also outline how to run the provided test suits, should the user wish to do so.

## Dependencies

Before building and running the Distributed File System, ensure you have the following dependencies installed:

- boost (1.81.0)
- cmake (3.29.2)
- gcc (13.2.0)
- gtest (1.14.0)
- openssl (3.0.13)
- pkg-config (0.29.2)

### Installing Dependencies

### Ubuntu/Debian

```bash
# Install build essentials (gcc, g++, make)
sudo apt-get install build-essential

# Install pkg-config
sudo apt-get install pkg-config

# Install CMake
sudo apt-get install cmake

# Install OpenSSL
sudo apt-get install libssl-dev

# Install Boost libraries
sudo apt-get install libboost-all-dev

# Install Google Test
sudo apt-get install libgtest-dev

```

### macOS (using Homebrew)

```bash
# Install gcc (macOS will also need this even though it has Clang)
brew install gcc

# Install pkg-config
brew install pkg-config

# Install CMake
brew install cmake

# Install OpenSSL
brew install openssl

# Install Boost libraries
brew install boost

# Install Google Test
brew install googletest

```

## Building the Project

1. Create a build directory:

```bash
mkdir build
cd build

```

1. Generate build files with CMake:

```bash
cmake ..

```

1. Build the project:

```bash
make

```

## Forming and Running the DFS

- The instructions below will walk you through how to:
    1. Create peers to participate in the DFS
    2. Connect these peers to each other
- This design allows for any number of peers to join the DFS.
- It is important to understand that running the main executable will **NOT** automatically create a DFS. Instead, it will create a participating peer node which can then be connected to other nodes in the DFS network, if there are any. Thus, this project requires **AT LEAST 2** participating peers to be created

### Creating a Single Peer

The main executable `dfs_main` requires the following command-line options:

```bash
./dfs_main [OPTIONS]

Options:
-h, --host <IP>        Local IP address (required)
-p, --port <number>    Port number to listen on (required)

```

Example usage:

```bash
# Start a server with IP 127.0.0.1 on port 3000
./dfs_main --host 127.0.0.1 --port 3000 

# Start a server with IP 127.0.0.1 on port 3000
./dfs_main -h 127.0.0.1 -p 3000

```

- **ATTENTION!**
    - Peers can only be created with **LOCALHOST** IP address
    - Creating 1 peer does NOT create a DFS
    - To form the DFS, you must open several shell tabs and in each, create a peer with localhost IP and a different port number, as shown below:

```bash
# Tab 1 - Peer 1:
./dfs_main -h 127.0.0.1 -p 3001
```

```bash
# Tab 2 - Peer 2:
./dfs_main -h 127.0.0.1 -p 3002
```

- You have now created 2 peers, but they are **NOT** connected.
- Please follow the instructions below to connect the peers

### Connecting to Peers

- Once **AT LEAST 2** peers have been created:
    - From either peer, attempt to connect to the other:

```bash
# Tab 1 - Peer 1
DFS_Shell> connect 127.0.0.1:3002
```

- This command will establish a connection between both peers.
- You have now created a DFS of 2 nodes. Any files stored in 1 peer will be shared with the other.
- You may add as many nodes as you wish.

## CLI Commands

Once the program is running, you can interact with it using the following CLI commands:

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

The project includes several test suites that can be run individually or all at once:

### Running Individual Test Suites

```bash
# Run codec tests
./codec_tests

# Run bootstrap tests
./bootstrap_tests

# Run channel tests
./channel_tests

# Run store tests
./store_tests

# Run crypto tests
./crypto_tests

```

### Running All Tests

```bash
# Run all test suites
./all_tests

```