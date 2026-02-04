#!/bin/bash

# Build script for Folly Containers Benchmark

set -e

echo "════════════════════════════════════════════════"
echo "  Building Folly Containers Benchmark"
echo "════════════════════════════════════════════════"
echo ""

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "✓ Detected macOS"

    # Check for Folly
    if brew list folly &> /dev/null; then
        echo "✓ Folly installed"
    else
        echo "✗ Folly not found. Installing..."
        brew install folly
    fi

    # Compile
    echo ""
    echo "Compiling with optimizations..."
    g++ -std=c++17 -O3 -march=native -DNDEBUG \
        folly_containers_comprehensive.cpp \
        -I/opt/homebrew/include -L/opt/homebrew/lib \
        -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
        -lboost_context -lboost_filesystem -lboost_program_options \
        -lpthread -o folly_benchmark

elif [[ -f /etc/redhat-release ]]; then
    echo "✓ Detected RHEL/CentOS"

    # Check for Folly
    if [ -f /usr/local/lib/libfolly.so ] || [ -f /usr/local/lib64/libfolly.so ]; then
        echo "✓ Folly installed"
    else
        echo "✗ Folly not found"
        echo "  Please build from source:"
        echo "    sudo yum install -y double-conversion-devel gflags-devel \\"
        echo "        glog-devel libevent-devel openssl-devel fmt-devel"
        echo "    git clone https://github.com/facebook/folly.git"
        echo "    cd folly && mkdir build && cd build"
        echo "    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local"
        echo "    make -j\$(nproc) && sudo make install"
        exit 1
    fi

    # Compile
    echo ""
    echo "Compiling with optimizations..."
    g++ -std=c++17 -O3 -march=native -mavx2 -DNDEBUG \
        folly_containers_comprehensive.cpp \
        -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
        -lpthread -o folly_benchmark
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
    echo "  ./folly_benchmark"
    echo ""
    echo "For production HFT:"
    echo "  taskset -c 2,3,4,5 ./folly_benchmark"
    echo ""
    echo "Profile with perf:"
    echo "  perf stat -e cache-misses,cache-references ./folly_benchmark"
    echo ""
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi

