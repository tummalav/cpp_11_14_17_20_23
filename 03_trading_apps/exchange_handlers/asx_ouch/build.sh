#!/bin/bash

# ASX OUCH Plugin Build Script
# Ultra-low latency compilation with optimization flags

set -e

echo "Building ASX OUCH Order Entry Plugin..."
echo "======================================"

# Compiler settings
CXX=g++
CXXFLAGS="-std=c++17 -O3 -march=native -Wall -Wextra -Wno-unused-parameter"
LDFLAGS="-pthread"

# Source files
PLUGIN_SRC="ouch_asx_order_handler.cpp"
EXAMPLE_SRC="ouch_example_application.cpp"
PERF_TEST_SRC="ouch_performance_test.cpp"

# Build targets
echo "Compiling plugin library..."
$CXX $CXXFLAGS -c $PLUGIN_SRC -o ouch_asx_order_handler.o

echo "Building example application..."
$CXX $CXXFLAGS $PLUGIN_SRC $EXAMPLE_SRC $LDFLAGS -o asx_example

echo "Building performance test..."
$CXX $CXXFLAGS $PLUGIN_SRC $PERF_TEST_SRC $LDFLAGS -o asx_perf_test

echo ""
echo "Build complete! Executables created:"
echo "  - asx_example      (Example trading application)"
echo "  - asx_perf_test    (Performance benchmarking)"
echo ""
echo "Run tests:"
echo "  ./asx_example"
echo "  ./asx_perf_test"
echo ""
