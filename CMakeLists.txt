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
    src/crypto/logger.cpp
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

# Tests
enable_testing()

add_executable(crypto_tests 
    src/tests/crypto_stream_test.cpp
    src/tests/byte_order_test.cpp
    src/tests/logger_test.cpp)

target_link_libraries(crypto_tests
    PRIVATE
    dfs_crypto
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(crypto_tests)

# Install rules
install(TARGETS dfs_crypto
    EXPORT dfs_crypto-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT dfs_crypto-targets
    FILE dfs_crypto-targets.cmake
    NAMESPACE dfs::
    DESTINATION lib/cmake/dfs_crypto
)
