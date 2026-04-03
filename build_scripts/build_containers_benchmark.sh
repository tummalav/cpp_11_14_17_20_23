#!/bin/bash

# Build script for Ultra Low Latency Containers Benchmark
# Works on both macOS and RHEL

set -e

echo "=========================================="
echo "Building Ultra Low Latency Containers Benchmark"
echo "=========================================="

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS"
    OS="macos"
elif [[ -f /etc/redhat-release ]]; then
    echo "Detected RHEL"
    OS="rhel"
else
    echo "Unknown OS: $OSTYPE"
    OS="unknown"
fi

# Check for required libraries
echo ""
echo "Checking dependencies..."

if [[ "$OS" == "macos" ]]; then
    # macOS with Homebrew
    INCLUDE_PATH="/opt/homebrew/include"
    LIB_PATH="/opt/homebrew/lib"

    if ! command -v brew &> /dev/null; then
        echo "Error: Homebrew not found. Install from https://brew.sh"
        exit 1
    fi

    # Check for libraries
    for lib in boost abseil folly; do
        if brew list $lib &> /dev/null; then
            echo "✓ $lib installed"
        else
            echo "✗ $lib not found. Installing..."
            brew install $lib
        fi
    done

    COMPILE_CMD="g++ -std=c++20 -O3 -march=native -DNDEBUG \
        ultra_low_latency_containers_comparison.cpp \
        -I$INCLUDE_PATH -L$LIB_PATH \
        -lboost_system -labsl_base -labsl_hash -labsl_raw_hash_set \
        -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
        -lpthread -o containers_benchmark"

elif [[ "$OS" == "rhel" ]]; then
    # RHEL
    INCLUDE_PATH="/usr/include"
    LIB_PATH="/usr/lib64"

    # Check for boost
    if ! rpm -q boost-devel &> /dev/null; then
        echo "✗ boost-devel not found. Install with: sudo yum install boost-devel"
        exit 1
    else
        echo "✓ boost-devel installed"
    fi

    COMPILE_CMD="g++ -std=c++20 -O3 -march=native -mavx2 -DNDEBUG \
        ultra_low_latency_containers_comparison.cpp \
        -lboost_system -lpthread -o containers_benchmark"

    echo "Note: Abseil and Folly benchmarks may be disabled on RHEL"
else
    echo "Error: Unsupported OS"
    exit 1
fi

# Compile
echo ""
echo "Compiling with optimization flags..."
echo "Command: $COMPILE_CMD"
echo ""

eval $COMPILE_CMD

if [ $? -eq 0 ]; then
    echo ""
    echo "=========================================="
    echo "✓ Build successful!"
    echo "=========================================="
    echo ""
    echo "Run the benchmark with:"
    echo "  ./containers_benchmark"
    echo ""
    echo "For production HFT systems, also consider:"
    echo "  • CPU pinning: taskset -c 0-3 ./containers_benchmark"
    echo "  • Set process priority: sudo nice -n -20 ./containers_benchmark"
    echo "  • Disable frequency scaling: sudo cpupower frequency-set -g performance"
    echo ""
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi

