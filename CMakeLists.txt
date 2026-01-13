cmake_minimum_required(VERSION 3.16)
project(BookingProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set source and build directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")

# Set output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

# Client executable with UI and connection classes
add_executable(client 
    ${SRC_DIR}/client.cpp 
    ${SRC_DIR}/ChatUI.cpp
    ${SRC_DIR}/ServerConnection.cpp
)
target_link_libraries(client ncurses pthread)

# Server executable with socket and client management classes
add_executable(server 
    ${SRC_DIR}/server.cpp
    ${SRC_DIR}/ServerSocket.cpp
    ${SRC_DIR}/ClientManager.cpp
)
target_link_libraries(server pthread)
