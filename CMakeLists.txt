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
    src/network/tcp_peer.cpp
    src/network/peer_manager.cpp
)
target_include_directories(dfs_network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
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

# Network tests
add_executable(network_tests
    src/tests/tcp_peer_test.cpp
    src/tests/peer_manager_test.cpp
    tests/codec_test.cpp)

target_include_directories(network_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(network_tests
    PRIVATE
    dfs_network
    dfs_crypto
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(crypto_tests)
gtest_discover_tests(store_tests)
gtest_discover_tests(network_tests)

# Add custom target to run tests
add_custom_target(run_tests ALL
    COMMAND crypto_tests
    COMMAND store_tests
    COMMAND network_tests
    DEPENDS crypto_tests store_tests network_tests
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
    DESTINATION lib/cmake/dfs)