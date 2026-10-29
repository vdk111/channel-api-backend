
cmake_minimum_required(VERSION 3.20)

project(channel_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)

add_executable(channel_backend
    src/main.cpp
    src/controllers/ChannelController.cpp
)

target_include_directories(channel_backend PRIVATE include)

target_link_libraries(channel_backend PRIVATE Drogon::Drogon)

target_compile_options(channel_backend PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

