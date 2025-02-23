cmake_minimum_required(VERSION 3.10)
project(ProxyServer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Add compiler flags
add_compile_options(-Wall -Wextra -pthread)

# Create required directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# Collect source files
file(GLOB SOURCES "src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# Create main executable
add_executable(proxy_server src/main.cpp ${SOURCES})
target_include_directories(proxy_server PRIVATE src)
target_link_libraries(proxy_server pthread)

# Create client test executable
add_executable(client_test test/client_test.cpp)
target_include_directories(client_test PRIVATE src)
target_link_libraries(client_test pthread) 