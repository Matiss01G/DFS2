cmake_minimum_required(VERSION 3.14)
project(dfs_crypto CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS log log_setup system thread)
find_package(GTest REQUIRED)

# Enable testing
enable_testing()

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

# Test executables
add_executable(tcp_peer_test src/tests/tcp_peer_test.cpp)
target_link_libraries(tcp_peer_test PRIVATE
    dfs_network
    GTest::GTest
    GTest::Main
)
add_test(NAME tcp_peer_test COMMAND tcp_peer_test)

# Add other test executables
add_executable(channel_test src/tests/channel_test.cpp)
target_link_libraries(channel_test PRIVATE
    dfs_network
    GTest::GTest
    GTest::Main
)
add_test(NAME channel_test COMMAND channel_test)

add_executable(codec_test src/tests/codec_test.cpp)
target_link_libraries(codec_test PRIVATE
    dfs_network
    GTest::GTest
    GTest::Main
)
add_test(NAME codec_test COMMAND codec_test)

add_executable(crypto_stream_test src/tests/crypto_stream_test.cpp)
target_link_libraries(crypto_stream_test PRIVATE
    dfs_crypto
    GTest::GTest
    GTest::Main
)
add_test(NAME crypto_stream_test COMMAND crypto_stream_test)

add_executable(peer_manager_test src/tests/peer_manager_test.cpp)
target_link_libraries(peer_manager_test PRIVATE
    dfs_network
    GTest::GTest
    GTest::Main
)
add_test(NAME peer_manager_test COMMAND peer_manager_test)

add_executable(store_test src/tests/store_test.cpp)
target_link_libraries(store_test PRIVATE
    dfs_store
    GTest::GTest
    GTest::Main
)
add_test(NAME store_test COMMAND store_test)

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