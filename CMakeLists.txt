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
add_library(dfs_network INTERFACE)
target_include_directories(dfs_network INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dfs_network INTERFACE
    dfs_crypto
    Boost::system
    Boost::thread
)

# Tests
enable_testing()

add_executable(crypto_tests 
    src/tests/crypto_stream_test.cpp)

target_link_libraries(crypto_tests
    PRIVATE
    dfs_crypto
    GTest::GTest
    GTest::Main
)

add_executable(network_tests
    src/tests/network_test.cpp
    src/tests/connection_state_test.cpp)

target_link_libraries(network_tests
    PRIVATE
    dfs_network
    dfs_crypto
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(crypto_tests)
gtest_discover_tests(network_tests)

# Install rules
install(TARGETS dfs_crypto dfs_network
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
