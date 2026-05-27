cmake_minimum_required(VERSION 3.16)
project(AlgoTrader VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

# Find ALL source files recursively
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.cxx"
    "src/*.cc"
)

# Create executable
add_executable(algo_trader ${SOURCES})

# Link libraries
target_link_libraries(algo_trader
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

# Windows specific
if(WIN32)
    target_link_libraries(algo_trader ws2_32)
endif()

message(STATUS "Build configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  CURL Found: ${CURL_FOUND}")
message(STATUS "  OpenSSL Found: ${OPENSSL_FOUND}")
message(STATUS "  Source files: ${SOURCES}")