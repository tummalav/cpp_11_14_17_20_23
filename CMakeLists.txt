cmake_minimum_required(VERSION 3.20)
project(cpp_modern_features)

# Set C++ standard based on target
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++11 Examples
add_executable(cpp11_examples cpp11_examples.cpp)
set_target_properties(cpp11_examples PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

# C++14 Examples
add_executable(cpp14_examples cpp14_examples.cpp)
set_target_properties(cpp14_examples PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)

# C++17 Examples
add_executable(cpp17_examples cpp17_examples.cpp)
set_target_properties(cpp17_examples PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# C++20 Examples
add_executable(cpp20_examples cpp20_examples.cpp)
set_target_properties(cpp20_examples PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# C++23 Examples
add_executable(cpp23_examples cpp23_examples.cpp)
set_target_properties(cpp23_examples PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# Enable threading support for C++11 examples
find_package(Threads REQUIRED)
target_link_libraries(cpp11_examples Threads::Threads)

# Print build configuration
message(STATUS "===========================================")
message(STATUS "Modern C++ Features Examples Build Configuration")
message(STATUS "===========================================")
message(STATUS "C++11 examples: cpp11_examples")
message(STATUS "C++14 examples: cpp14_examples")
message(STATUS "C++17 examples: cpp17_examples")
message(STATUS "C++20 examples: cpp20_examples")
message(STATUS "C++23 examples: cpp23_examples")
message(STATUS "===========================================")
