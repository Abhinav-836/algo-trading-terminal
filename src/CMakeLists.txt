# Source directory CMakeLists.txt
set(CMAKE_CXX_STANDARD 20)

# Find all source files
file(GLOB_RECURSE SRC_FILES 
    "*.cpp"
    "*.cxx"
    "*.cc"
)

# Find all header files
file(GLOB_RECURSE HEADER_FILES 
    "*.hpp"
    "*.h"
    "*.hxx"
)

# Add executable
add_executable(algo_trader ${SRC_FILES})

# Include directories
target_include_directories(algo_trader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/exchange
    ${CMAKE_CURRENT_SOURCE_DIR}/strategies
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_SOURCE_DIR}/live
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Link libraries
target_link_libraries(algo_trader)