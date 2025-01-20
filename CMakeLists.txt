cmake_minimum_required(VERSION 3.14)
project(dfs_crypto CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS log log_setup system thread)
find_package(GTest REQUIRED)

# Create crypto library
add_library(dfs_crypto
    src/crypto/crypto_stream.cpp
)
target_include_directories(dfs_crypto PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dfs_crypto PUBLIC
    OpenSSL::Crypto
    Boost::log
    Boost::log_setup
    Boost::system
    Boost::thread
)

# Create store library
add_library(dfs_store
    src/store/store.cpp
)
target_include_directories(dfs_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dfs_store PUBLIC
    OpenSSL::Crypto
    Boost::log
    Boost::log_setup
    Boost::system
)

# Create network library
add_library(dfs_network
    src/network/tcp_server.cpp
    src/network/tcp_peer.cpp
    src/network/peer_manager.cpp
    src/network/channel.cpp
    src/network/codec.cpp
    src/file_server/file_server.cpp
)
target_include_directories(dfs_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dfs_network PUBLIC
    dfs_crypto
    dfs_store
    Boost::system
    Boost::thread
    Boost::log
)

# Enable testing
enable_testing()

# Add tests executable
add_executable(unit_tests
    src/tests/tcp_peer_test.cpp
    src/tests/tcp_server_test.cpp
    src/tests/file_server_test.cpp
)
target_link_libraries(unit_tests PRIVATE
    GTest::GTest
    GTest::Main
    dfs_network
    dfs_crypto
    dfs_store
)

# Install rules
install(TARGETS dfs_crypto dfs_store dfs_network
    EXPORT dfs-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT dfs-targets
    FILE dfs-targets.cmake
    NAMESPACE dfs::
    DESTINATION lib/cmake/dfs)