cmake_minimum_required(VERSION 3.15)
project(algo_trading_terminal VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -mtune=native -flto -ffast-math -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

# Check if src directory exists before adding
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_subdirectory(src)
else()
    message(WARNING "src directory not found, creating minimal setup...")
    
    # Create minimal executable directly
    add_executable(algo_trader src/main.cpp)
    target_include_directories(algo_trader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

# Install target (only if target exists)
if(TARGET algo_trader)
    install(TARGETS algo_trader DESTINATION bin)
endif()