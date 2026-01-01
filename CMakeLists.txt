cmake_minimum_required(VERSION 3.10)
project(PeerReviewSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/include)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Storage library
add_library(storage
    src/storage/storage.cpp
    src/storage/bitmap.cpp
    src/storage/cache.cpp
)

# Filesystem library
add_library(filesystem
    src/filesystem/disk.cpp
    src/filesystem/block_manager.cpp
    src/filesystem/filesystem.cpp
    src/filesystem/backup.cpp
    src/filesystem/directory_ops.cpp
    src/filesystem/file_ops.cpp
)

target_link_libraries(filesystem storage)

# Protocol library
add_library(protocol
    src/protocol/protocol.cpp
)

# User library
add_library(user
    src/user/user_manager.cpp
)

target_link_libraries(user 
    OpenSSL::Crypto
)

# Review library
add_library(review
    src/review/review_system.cpp
)

target_link_libraries(review
    filesystem
    user
)

# Network library
add_library(network
    src/network/connection.cpp
    src/network/server.cpp
    src/network/client.cpp
)

target_link_libraries(network 
    protocol 
    filesystem
    user
    review
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads
)

# Server executable
add_executable(server apps/server_main.cpp)
target_link_libraries(server 
    network
    filesystem
    user
    review
    protocol
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads
)

# Client executable
add_executable(client apps/client_main.cpp)
target_link_libraries(client 
    network
    protocol
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads
)

# ✅ CLI 可执行文件 - 修复库名
add_executable(review_cli
    src/client/main.cpp
    src/client/cli.cpp
)

target_link_libraries(review_cli
    review
    user
    filesystem
    storage
)

# Tests
add_executable(test_filesystem tests/test_filesystem.cpp)
target_link_libraries(test_filesystem filesystem)

add_executable(test_backup tests/test_backup.cpp)
target_link_libraries(test_backup filesystem)

add_executable(test_directory tests/test_directory.cpp)
target_link_libraries(test_directory filesystem)

add_executable(test_file_ops tests/test_file_ops.cpp)
target_link_libraries(test_file_ops filesystem)

add_executable(test_file_ops_debug tests/test_file_ops_debug.cpp)
target_link_libraries(test_file_ops_debug filesystem)

add_executable(test_industrial tests/test_industrial.cpp)
target_link_libraries(test_industrial filesystem)

add_executable(test_persistence tests/test_persistence.cpp)
target_link_libraries(test_persistence review user filesystem storage OpenSSL::Crypto)

add_executable(test_comments tests/test_comments.cpp)
target_link_libraries(test_comments review user filesystem storage OpenSSL::Crypto)

# Install targets
install(TARGETS server client review_cli DESTINATION bin)
