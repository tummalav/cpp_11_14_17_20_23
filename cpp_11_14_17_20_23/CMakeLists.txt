cmake_minimum_required(VERSION 3.16)
project(ASX_OUCH_Handler VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for ultra-low latency
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Add compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -ffast-math")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -ffast-math")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# OUCH Handler Library (as shared library plugin)
add_library(ouch_asx_plugin SHARED
    ouch_asx_order_handler.cpp
)

target_link_libraries(ouch_asx_plugin
    Threads::Threads
    dl
)

# Set plugin properties
set_target_properties(ouch_asx_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Example Application
add_executable(ouch_example
    ouch_example_application.cpp
)

target_link_libraries(ouch_example
    Threads::Threads
    dl
)

# Performance test executable
add_executable(ouch_performance_test
    ouch_performance_test.cpp
)

target_link_libraries(ouch_performance_test
    Threads::Threads
    dl
)

# Install targets
install(TARGETS ouch_asx_plugin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS ouch_example ouch_performance_test
    RUNTIME DESTINATION bin
)

install(FILES
    ouch_asx_order_handler.hpp
    ouch_plugin_manager.hpp
    DESTINATION include
)

# Create config file for production use
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/ouch_config.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ouch_config.json"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ouch_config.json"
    DESTINATION etc
)
