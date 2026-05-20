#!/bin/bash

# Build script for Abseil Containers Benchmark

set -e

echo "════════════════════════════════════════════════"
echo "  Building Abseil Containers Benchmark"
echo "════════════════════════════════════════════════"
echo ""

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "✓ Detected macOS"

    # Check for Abseil
    if brew list abseil &> /dev/null; then
        echo "✓ Abseil installed"
    else
        echo "✗ Abseil not found. Installing..."
        brew install abseil
    fi

    # Compile
    echo ""
    echo "Compiling with optimizations..."
    g++ -std=c++17 -O3 -march=native -DNDEBUG \
        abseil_containers_comprehensive.cpp \
        -I/opt/homebrew/include -L/opt/homebrew/lib \
        -labsl_base -labsl_hash -labsl_raw_hash_set -labsl_strings \
        -labsl_synchronization -labsl_time \
        -lpthread -o abseil_benchmark

elif [[ -f /etc/redhat-release ]]; then
    echo "✓ Detected RHEL/CentOS"

    # Check for Abseil
    if [ -f /usr/local/lib/libabsl_base.a ]; then
        echo "✓ Abseil installed"
    else
        echo "✗ Abseil not found"
        echo "  Please build from source:"
        echo "    git clone https://github.com/abseil/abseil-cpp.git"
        echo "    cd abseil-cpp && mkdir build && cd build"
        echo "    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local"
        echo "    make -j\$(nproc) && sudo make install"
        exit 1
    fi

    # Compile
    echo ""
    echo "Compiling with optimizations..."
    g++ -std=c++17 -O3 -march=native -mavx2 -DNDEBUG \
        abseil_containers_comprehensive.cpp \
        -labsl_base -labsl_hash -labsl_raw_hash_set -labsl_strings \
        -lpthread -o abseil_benchmark
else
    echo "✗ Unknown OS: $OSTYPE"
    exit 1
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "════════════════════════════════════════════════"
    echo "✓ Build successful!"
    echo "════════════════════════════════════════════════"
    echo ""
    echo "Run the benchmark:"
    echo "  ./abseil_benchmark"
    echo ""
    echo "For production HFT:"
    echo "  taskset -c 2,3,4,5 ./abseil_benchmark"
    echo ""
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi

