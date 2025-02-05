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

# Create network library
add_library(dfs_network
    src/network/channel.cpp
    src/network/codec.cpp
    src/network/peer_manager.cpp
    src/network/tcp_peer.cpp
    src/network/tcp_server.cpp
    src/network/bootstrap.cpp
    src/file_server/file_server.cpp
    src/utils/pipeliner.cpp
)
target_include_directories(dfs_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dfs_network PUBLIC
    dfs_crypto
    dfs_store
    dfs_cli
    Boost::system
    Boost::thread
    Boost::log
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

# Create CLI library
add_library(dfs_cli
    src/cli/cli.cpp
)
target_include_directories(dfs_cli PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dfs_cli PUBLIC
    dfs_store
    dfs_network
    Boost::log
)

# Tests
enable_testing()

# Crypto tests
add_executable(crypto_tests
    src/tests/crypto_stream_test.cpp)
target_link_libraries(crypto_tests
    PRIVATE
    dfs_crypto
    GTest::GTest
    GTest::Main
)

# Store tests
add_executable(store_tests
    src/tests/store_test.cpp)
target_link_libraries(store_tests
    PRIVATE
    dfs_store
    GTest::GTest
    GTest::Main
)

# Channel tests
add_executable(channel_tests
    src/tests/channel_test.cpp
    src/network/channel.cpp)
target_include_directories(channel_tests PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(channel_tests
    PRIVATE
    dfs_network
    GTest::GTest
    GTest::Main
)

# TCP Peer tests
add_executable(tcp_peer_tests
    src/tests/tcp_peer_tests.cpp
    src/network/tcp_peer.cpp)
target_include_directories(tcp_peer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
)
target_link_libraries(tcp_peer_tests
    PRIVATE
    dfs_network
    dfs_crypto
    dfs_store
    GTest::GTest
    GTest::Main
)

# Bootstrap tests
add_executable(bootstrap_tests
    src/tests/bootstrap_test.cpp)
target_include_directories(bootstrap_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
)
target_link_libraries(bootstrap_tests
    PRIVATE
    dfs_network
    dfs_crypto
    dfs_store
    GTest::GTest
    GTest::Main
)

# Network tests (from network)
add_executable(codec_tests
    src/tests/codec_test.cpp
    src/network/codec.cpp)
target_include_directories(codec_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
)
target_link_libraries(codec_tests
    PRIVATE
    dfs_network
    dfs_crypto
    dfs_store
    GTest::GTest
    GTest::Main
)

# Update test discovery and run_tests sections
include(GoogleTest)
gtest_discover_tests(crypto_tests)
gtest_discover_tests(store_tests)
gtest_discover_tests(channel_tests)
gtest_discover_tests(codec_tests)
gtest_discover_tests(tcp_peer_tests)
gtest_discover_tests(bootstrap_tests)

# Update run_tests target
add_custom_target(run_tests ALL
    COMMAND crypto_tests
    COMMAND store_tests
    COMMAND channel_tests
    COMMAND codec_tests
    COMMAND tcp_peer_tests
    COMMAND bootstrap_tests
    DEPENDS crypto_tests store_tests channel_tests codec_tests tcp_peer_tests bootstrap_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Install rules
install(TARGETS dfs_crypto dfs_store dfs_network dfs_cli
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