
# Require CMake version 3.20 or newer.
cmake_minimum_required(VERSION 3.20)

# Define the project name and enable C++.
project(channel_backup LANGUAGES CXX)

# Use C++23 for OnnetJsonParser.
set(CMAKE_CXX_STANDARD 23)

# Require the selected C++ standard.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable compiler-specific C++ extensions.
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the existing Drogon installation.
set(Drogon_DIR
    "/home/vinayak/workspace/drogon-pg-install/lib64/cmake/Drogon"
    CACHE PATH "Drogon package directory" FORCE
)

# Load the Drogon CMake package.
find_package(Drogon CONFIG REQUIRED)

# Find threading support.
find_package(Threads REQUIRED)

# Define the custom parser directory.
set(ONNET_JSON_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnet_json_parser"
)

# Define the parser include directory.
set(ONNET_JSON_INCLUDE_DIR
    "${ONNET_JSON_ROOT}/include"
)

# Define the Glaze include directory.
set(GLAZE_INCLUDE_DIR
    "${ONNET_JSON_ROOT}/include/source/glaze/include"
)

# Define the parser static library.
set(ONNET_JSON_LIBRARY
    "${ONNET_JSON_ROOT}/lib/libonnet_json_parser.a"
)

# Define the schema directory.
set(CHANNEL_SCHEMA_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/config/schema"
)

# Define the Swagger documentation directory.
set(OPENAPI_DOCS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/docs"
)

# Check that the parser header exists.
if(NOT EXISTS "${ONNET_JSON_INCLUDE_DIR}/onnet/json/json_parser.hpp")
    message(FATAL_ERROR "OnnetJsonParser header not found")
endif()

# Check that the Glaze header exists.
if(NOT EXISTS "${GLAZE_INCLUDE_DIR}/glaze/glaze.hpp")
    message(FATAL_ERROR "Glaze header not found")
endif()

# Check that the parser library exists.
if(NOT EXISTS "${ONNET_JSON_LIBRARY}")
    message(FATAL_ERROR "OnnetJsonParser library not found")
endif()

# Check that the schema directory exists.
if(NOT IS_DIRECTORY "${CHANNEL_SCHEMA_DIR}")
    message(FATAL_ERROR "Schema directory not found")
endif()

# Check that the Swagger documentation directory exists.
if(NOT IS_DIRECTORY "${OPENAPI_DOCS_DIR}")
    message(FATAL_ERROR "Swagger documentation directory not found")
endif()

# Build the backend executable from its source files.
add_executable(channel_backup
    src/main.cpp
    src/controllers/ChannelController.cpp
    src/services/ChannelCommandService.cpp
    src/services/ChannelQueryService.cpp
)

# Add project and third-party include directories.
target_include_directories(channel_backup PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${ONNET_JSON_INCLUDE_DIR}"
    "${GLAZE_INCLUDE_DIR}"
)

# Make the schema directory available to C++ code.
target_compile_definitions(channel_backup PRIVATE
    CHANNEL_SCHEMA_DIR="${CHANNEL_SCHEMA_DIR}"
)

# Make the Swagger documentation directory available to C++ code.
target_compile_definitions(channel_backup PRIVATE
    OPENAPI_DOCS_DIR="${OPENAPI_DOCS_DIR}"
)

# Link Drogon, OnnetJsonParser, and threading support.
target_link_libraries(channel_backup PRIVATE
    Drogon::Drogon
    "${ONNET_JSON_LIBRARY}"
    Threads::Threads
)

# Enable common compiler warnings.
target_compile_options(channel_backup PRIVATE
    -Wall
    -Wextra
)

# Display the configured parser header path.
message(STATUS "OnnetJsonParser headers: ${ONNET_JSON_INCLUDE_DIR}")

# Display the configured Glaze header path.
message(STATUS "Glaze headers: ${GLAZE_INCLUDE_DIR}")

# Display the configured parser library path.
message(STATUS "OnnetJsonParser library: ${ONNET_JSON_LIBRARY}")

# Display the configured schema directory.
message(STATUS "Schema directory: ${CHANNEL_SCHEMA_DIR}")

# Display the configured Swagger documentation directory.
message(STATUS "Swagger documentation: ${OPENAPI_DOCS_DIR}")
