cmake_minimum_required(VERSION 3.15)

# Auth Library
set(AUTH_SOURCES
    src/AuthServer.cpp
    src/AuthManager.cpp
    src/AuthClient.cpp
    src/InMemoryUserRepository.cpp
    src/FileUserRepository.cpp
)

set(AUTH_HEADERS
    include/auth/AuthServer.h
    include/auth/AuthManager.h
    include/auth/AuthToken.h
    include/auth/AuthClient.h
    include/auth/IUserRepository.h
    include/auth/InMemoryUserRepository.h
    include/auth/FileUserRepository.h
)

add_library(auth_lib STATIC ${AUTH_SOURCES} ${AUTH_HEADERS})

target_include_directories(auth_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against threading library
find_package(Threads REQUIRED)
target_link_libraries(auth_lib PUBLIC Threads::Threads)

# Set C++20 standard
target_compile_features(auth_lib PUBLIC cxx_std_20)
