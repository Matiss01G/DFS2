cmake_minimum_required(VERSION 3.14)
project(dfs_crypto CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS log log_setup system thread)
find_package(GTest REQUIRED)

# Set source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create crypto library
add_library(dfs_crypto
    ${SRC_DIR}/crypto/crypto_stream.cpp
)
target_include_directories(dfs_crypto PUBLIC
    ${INCLUDE_DIR}
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
    ${SRC_DIR}/store/store.cpp
)
target_include_directories(dfs_store PUBLIC
    ${INCLUDE_DIR}
)
target_link_libraries(dfs_store PUBLIC
    OpenSSL::Crypto
    Boost::log
    Boost::log_setup
    Boost::system
)

# Create network library
add_library(dfs_network
    ${SRC_DIR}/network/tcp_peer.cpp
    ${SRC_DIR}/network/peer_manager.cpp
    ${SRC_DIR}/network/codec.cpp
    ${SRC_DIR}/network/channel.cpp
    ${SRC_DIR}/network/message_handler.cpp
)
target_include_directories(dfs_network PUBLIC
    ${INCLUDE_DIR}
)
target_link_libraries(dfs_network PUBLIC
    dfs_crypto
    Boost::system
    Boost::thread
    Boost::log
)

# Tests
enable_testing()

# Crypto tests
add_executable(crypto_tests
    ${SRC_DIR}/tests/crypto_stream_test.cpp
)
target_link_libraries(crypto_tests PRIVATE
    dfs_crypto
    GTest::GTest 
    GTest::Main
)

# Store tests
add_executable(store_tests
    ${SRC_DIR}/tests/store_test.cpp
)
target_link_libraries(store_tests PRIVATE
    dfs_store
    GTest::GTest
    GTest::Main
)

# Network tests
add_executable(network_tests
    ${SRC_DIR}/tests/tcp_peer_test.cpp
    ${SRC_DIR}/tests/peer_manager_test.cpp
    ${SRC_DIR}/tests/codec_test.cpp
)
target_include_directories(network_tests PRIVATE
    ${INCLUDE_DIR}
    ${SRC_DIR}/tests
)
target_link_libraries(network_tests PRIVATE
    dfs_network
    dfs_crypto
    GTest::GTest
    GTest::Main
)

# Channel tests
add_executable(channel_tests
    ${SRC_DIR}/tests/channel_test.cpp
)
target_include_directories(channel_tests PUBLIC
    ${INCLUDE_DIR}
)
target_link_libraries(channel_tests PRIVATE
    dfs_network
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(crypto_tests)
gtest_discover_tests(store_tests)
gtest_discover_tests(network_tests)
gtest_discover_tests(channel_tests)

# Add custom target to run tests
add_custom_target(run_tests ALL
    COMMAND crypto_tests
    COMMAND store_tests
    COMMAND network_tests
    COMMAND channel_tests
    DEPENDS crypto_tests store_tests network_tests channel_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
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
    DESTINATION lib/cmake/dfs
)