#!/bin/bash

# Latency Benchmarking Suite Runner
# Compiles and runs various latency benchmarks

echo "Latency Benchmarking Suite"
echo "========================="

# Check if compilation script exists
if [ ! -f "./compile_cpp20.sh" ]; then
    echo "Error: compile_cpp20.sh not found"
    exit 1
fi

# Compile benchmarks
echo "Compiling benchmarks..."

# Quick test
echo "1. Compiling quick latency test..."
g++ -std=c++2a -pthread -Wall -Wextra -O2 quick_latency_test.cpp -o quick_bench

if [ $? -eq 0 ]; then
    echo "✅ Quick benchmark compiled successfully"
    echo "Running quick benchmark..."
    ./quick_bench
else
    echo "❌ Quick benchmark compilation failed"
fi

echo ""

# Comprehensive test (with reduced intensity)
echo "2. Compiling comprehensive latency benchmark..."
g++ -std=c++2a -pthread -Wall -Wextra -O2 latency_benchmarking_examples.cpp -o latency_bench

if [ $? -eq 0 ]; then
    echo "✅ Comprehensive benchmark compiled successfully"
    echo "Note: The comprehensive benchmark includes many advanced features:"
    echo "  - High-resolution timing utilities"
    echo "  - Cache optimization benchmarks"
    echo "  - Lock-free data structure performance"
    echo "  - SIMD vectorization (where supported)"
    echo "  - Trading system specific benchmarks"
    echo "  - Memory allocation strategies"
    echo "  - Thread affinity optimization"
    echo ""
    echo "To run the comprehensive benchmark:"
    echo "  ./latency_bench"
    echo ""
    echo "Warning: This may take several minutes to complete all tests."
else
    echo "❌ Comprehensive benchmark compilation failed"
fi

echo ""

# Atomic memory orderings test
echo "3. Testing atomic memory orderings benchmark..."
if [ -f "atomic_memory_orderings_use_cases_examples.cpp" ]; then
    g++ -std=c++2a -pthread -Wall -Wextra -O2 atomic_memory_orderings_use_cases_examples.cpp -o atomic_bench

    if [ $? -eq 0 ]; then
        echo "✅ Atomic benchmark compiled successfully"
        echo "To run: ./atomic_bench"
    else
        echo "❌ Atomic benchmark compilation failed"
    fi
else
    echo "❌ Atomic benchmark source not found"
fi

echo ""
echo "=== Summary ==="
echo "Available benchmarks:"
if [ -f "./quick_bench" ]; then
    echo "  ✅ ./quick_bench - Quick latency tests"
fi
if [ -f "./latency_bench" ]; then
    echo "  ✅ ./latency_bench - Comprehensive latency suite"
fi
if [ -f "./atomic_bench" ]; then
    echo "  ✅ ./atomic_bench - Atomic memory orderings"
fi

echo ""
echo "Compilation tips:"
echo "  - Use -O3 for maximum optimization"
echo "  - Use -march=native for CPU-specific optimizations"
echo "  - Use -mavx or -mavx2 for SIMD optimizations (if supported)"
echo "  - Pin threads to CPU cores for consistent results"
echo "  - Disable CPU frequency scaling for stable measurements"

echo ""
echo "Benchmark suite setup completed!"
