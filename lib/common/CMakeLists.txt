cmake_minimum_required(VERSION 3.15)

# Common Library (header-only)
add_library(common_lib INTERFACE)

target_include_directories(common_lib
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link nlohmann_json
target_link_libraries(common_lib INTERFACE nlohmann_json::nlohmann_json)

# Set C++20 standard
target_compile_features(common_lib INTERFACE cxx_std_20)
