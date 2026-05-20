#!/bin/bash

# Build script for Lock-Free Ring Buffers

set -e

echo "════════════════════════════════════════════════"
echo "  Building Lock-Free Ring Buffers Benchmark"
echo "════════════════════════════════════════════════"
echo ""

echo "Compiling with optimizations..."
g++ -std=c++17 -O3 -march=native -DNDEBUG \
    lockfree_ring_buffers_trading.cpp \
    -lpthread -o lockfree_benchmark

if [ $? -eq 0 ]; then
    echo ""
    echo "════════════════════════════════════════════════"
    echo "✓ Build successful!"
    echo "════════════════════════════════════════════════"
    echo ""
    echo "Run the benchmark:"
    echo "  ./lockfree_benchmark"
    echo ""
    echo "For production HFT (with CPU pinning):"
    echo "  taskset -c 2,3,4,5 ./lockfree_benchmark"
    echo ""
    echo "Profile with perf:"
    echo "  perf stat -e cache-misses,cache-references ./lockfree_benchmark"
    echo ""
else
    echo ""
    echo "✗ Build failed"
    exit 1
fi

