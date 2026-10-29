
# Require CMake version 3.20 or newer.
cmake_minimum_required(VERSION 3.20)

# Define the project and enable C++ compilation.
project(channel LANGUAGES CXX)

# Use the C++23 language standard required by OnnetJsonParser.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Drogon from the existing workspace installation.
set(Drogon_DIR
    "/home/vinayak/workspace/drogon-pg-install/lib64/cmake/Drogon"
    CACHE PATH "Drogon package directory" FORCE
)

find_package(Drogon CONFIG REQUIRED)

# Find the platform thread library.
find_package(Threads REQUIRED)

# Define the local third-party parser directory.
set(ONNET_JSON_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnet_json_parser"
)

# Define the parser header directory.
set(ONNET_JSON_INCLUDE_DIR
    "${ONNET_JSON_ROOT}/include"
)

# Define the Glaze header directory bundled with the parser.
set(GLAZE_INCLUDE_DIR
    "${ONNET_JSON_ROOT}/include/source/glaze/include"
)

# Define the prebuilt parser static library.
set(ONNET_JSON_LIBRARY
    "${ONNET_JSON_ROOT}/lib/libonnet_json_parser.a"
)

# Define the directory containing the JSON schema files.
set(CHANNEL_SCHEMA_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/config/schema"
)

# Stop configuration if the parser header is missing.
if(NOT EXISTS "${ONNET_JSON_INCLUDE_DIR}/onnet/json/json_parser.hpp")
    message(FATAL_ERROR
        "OnnetJsonParser header not found: ${ONNET_JSON_INCLUDE_DIR}/onnet/json/json_parser.hpp"
    )
endif()

# Stop configuration if the Glaze header is missing.
if(NOT EXISTS "${GLAZE_INCLUDE_DIR}/glaze/glaze.hpp")
    message(FATAL_ERROR
        "Glaze header not found: ${GLAZE_INCLUDE_DIR}/glaze/glaze.hpp"
    )
endif()

# Stop configuration if the static parser library is missing.
if(NOT EXISTS "${ONNET_JSON_LIBRARY}")
    message(FATAL_ERROR
        "OnnetJsonParser library not found: ${ONNET_JSON_LIBRARY}"
    )
endif()

# Stop configuration if the schema directory is missing.
if(NOT IS_DIRECTORY "${CHANNEL_SCHEMA_DIR}")
    message(FATAL_ERROR
        "Schema directory not found: ${CHANNEL_SCHEMA_DIR}"
    )
endif()

# Build the backend executable from its source files.
add_executable(channel
    src/main.cpp
    src/controllers/ChannelController.cpp
    src/services/ChannelService.cpp
)

# Add the backend headers, parser headers, and Glaze headers.
target_include_directories(channel PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${ONNET_JSON_INCLUDE_DIR}"
    "${GLAZE_INCLUDE_DIR}"
)

# Make the schema directory available to the C++ code.
target_compile_definitions(channel PRIVATE
    CHANNEL_SCHEMA_DIR="${CHANNEL_SCHEMA_DIR}"
)

# Link Drogon, the local parser library, and threading support.
target_link_libraries(channel PRIVATE
    Drogon::Drogon
    "${ONNET_JSON_LIBRARY}"
    Threads::Threads
)

# Enable common compiler warnings.
target_compile_options(channel PRIVATE
    -Wall
    -Wextra
)

# Print important configuration paths.
message(STATUS "OnnetJsonParser headers: ${ONNET_JSON_INCLUDE_DIR}")
message(STATUS "Glaze headers: ${GLAZE_INCLUDE_DIR}")
message(STATUS "OnnetJsonParser library: ${ONNET_JSON_LIBRARY}")
message(STATUS "Schema directory: ${CHANNEL_SCHEMA_DIR}")
