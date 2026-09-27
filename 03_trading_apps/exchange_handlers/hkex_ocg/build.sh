#!/bin/bash

# HKEX OCG-C Plugin Build Script
# Ultra-low latency compilation with optimization flags

set -e

echo "Building HKEX OCG-C Order Entry Plugin..."
echo "========================================"

# Compiler settings
CXX=g++
CXXFLAGS="-std=c++17 -O3 -march=native -Wall -Wextra -Wno-unused-parameter"
LDFLAGS="-pthread"

# Source files
PLUGIN_SRC="hkex_ocg_order_handler.cpp"
EXAMPLE_SRC="hkex_ocg_example_application.cpp"
PERF_TEST_SRC="hkex_ocg_performance_test.cpp"

# Build targets
echo "Compiling plugin library..."
$CXX $CXXFLAGS -c $PLUGIN_SRC -o hkex_ocg_order_handler.o

echo "Building example application..."
$CXX $CXXFLAGS $PLUGIN_SRC $EXAMPLE_SRC $LDFLAGS -o hkex_example

echo "Building performance test..."
$CXX $CXXFLAGS $PLUGIN_SRC $PERF_TEST_SRC $LDFLAGS -o hkex_perf_test

echo ""
echo "Build complete! Executables created:"
echo "  - hkex_example     (Example trading application)"
echo "  - hkex_perf_test   (Performance benchmarking)"
echo ""
echo "Run tests:"
echo "  ./hkex_example"
echo "  ./hkex_perf_test"
echo ""
