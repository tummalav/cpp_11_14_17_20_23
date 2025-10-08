# Quick Start Guide

This guide will help you quickly get started with the C++ modern features examples.

## What's Inside

This repository contains comprehensive examples of C++ features from C++11 through C++23:

- **README.md** - Detailed documentation of all features with explanations
- **cpp11_examples.cpp** - C++11 features (auto, lambdas, smart pointers, threading, etc.)
- **cpp14_examples.cpp** - C++14 features (generic lambdas, binary literals, make_unique, etc.)
- **cpp17_examples.cpp** - C++17 features (structured bindings, optional, variant, etc.)
- **cpp20_examples.cpp** - C++20 features (concepts, ranges, coroutines, modules, etc.)
- **cpp23_examples.cpp** - C++23 features (expected, deducing this, std::print, etc.)

## Quick Build & Run

### Option 1: Using CMake (Recommended)

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build all examples
cmake --build .

# Run examples
./cpp11_examples
./cpp14_examples
./cpp17_examples
./cpp20_examples
./cpp23_examples
```

### Option 2: Direct Compilation

```bash
# C++11
g++ -std=c++11 -o cpp11_examples cpp11_examples.cpp -pthread

# C++14
g++ -std=c++14 -o cpp14_examples cpp14_examples.cpp

# C++17
g++ -std=c++17 -o cpp17_examples cpp17_examples.cpp

# C++20
g++ -std=c++20 -o cpp20_examples cpp20_examples.cpp

# C++23
g++ -std=c++23 -o cpp23_examples cpp23_examples.cpp

# Run any example
./cpp17_examples
```

## Requirements

- **Compiler**: GCC 13+, Clang 16+, or MSVC 2022+
- **CMake**: 3.20 or higher (for CMake builds)

## Example Output

When you run an example, you'll see output like:

```
========================================
     C++17 Feature Examples
========================================

=== Structured Bindings ===
John is 25 years old
Tuple: 1, 2.5, hello
Point: x=10, y=20
...
```

## Learning Path

1. Start with **README.md** for in-depth explanations
2. Run the examples to see features in action
3. Modify the code to experiment with different scenarios
4. Build your own projects using these modern features

## Key Features by Version

### C++11
- Auto type deduction
- Lambda expressions
- Smart pointers
- Move semantics
- Range-based for loops

### C++14
- Generic lambdas
- Return type deduction
- Binary literals
- make_unique

### C++17
- Structured bindings
- std::optional
- std::variant
- Filesystem library
- constexpr if

### C++20
- Concepts
- Ranges
- Coroutines
- Modules
- Three-way comparison

### C++23
- std::expected
- std::print
- Deducing this
- if consteval
- Enhanced ranges

## Getting Help

For detailed explanations of each feature, see the main **README.md** file.

## License

This project is provided for educational purposes.
