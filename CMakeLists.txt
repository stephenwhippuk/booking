cmake_minimum_required(VERSION 3.16)
project(BookingProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set source and build directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")

# Set output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

# Enable testing
enable_testing()

# Fetch Google Test and nlohmann/json
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(googletest json)

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add UI library subdirectory
add_subdirectory(lib/ui)

# Add Common library subdirectory
add_subdirectory(lib/common)

# Add Auth library subdirectory
add_subdirectory(lib/auth)

# Client executable (queue-based architecture)
add_executable(client
    ${SRC_DIR}/client/client.cpp
    ${SRC_DIR}/client/NetworkManager.cpp
    ${SRC_DIR}/client/ApplicationManager.cpp
    ${SRC_DIR}/client/ApplicationState.cpp
    ${SRC_DIR}/client/UIManager.cpp
)
target_include_directories(client PRIVATE ${INCLUDE_DIR})
target_link_libraries(client ncurses_ui auth_lib common_lib ncurses pthread)

# Server executable with socket and client management classes
add_executable(server 
    ${SRC_DIR}/server/server.cpp
    ${SRC_DIR}/server/ServerSocket.cpp
    ${SRC_DIR}/server/ClientManager.cpp
    ${SRC_DIR}/server/ChatRoom.cpp
)
target_include_directories(server PRIVATE ${INCLUDE_DIR})
target_link_libraries(server auth_lib common_lib pthread)

# Auth server executable
add_executable(auth_server
    ${SRC_DIR}/auth_server.cpp
)
target_include_directories(auth_server PRIVATE ${INCLUDE_DIR})
target_link_libraries(auth_server auth_lib pthread)

# Test executable
add_executable(tests
    tests/ThreadSafeQueueTest.cpp
    tests/NetworkManagerTest.cpp
    tests/ApplicationManagerTest.cpp
    ${SRC_DIR}/client/NetworkManager.cpp
    ${SRC_DIR}/client/ApplicationManager.cpp
    ${SRC_DIR}/client/ApplicationState.cpp
)
target_include_directories(tests PRIVATE ${INCLUDE_DIR})
target_link_libraries(tests 
    GTest::gtest_main
    auth_lib
    common_lib
    pthread
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(tests)
