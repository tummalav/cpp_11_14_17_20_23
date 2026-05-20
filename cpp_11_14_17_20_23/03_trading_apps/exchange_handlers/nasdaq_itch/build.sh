#!/bin/bash

# NASDAQ ITCH Feed Handler Build Script
# Ultra-low latency compilation with optimization flags

set -e

echo "Building NASDAQ ITCH Feed Handler..."
echo "===================================="

# Compiler settings
CXX=g++
CXXFLAGS="-std=c++17 -O3 -march=native -Wall -Wextra -Wno-unused-parameter"
LDFLAGS="-pthread"

# Source files
PLUGIN_SRC="nasdaq_itch_feed_handler.cpp"
EXAMPLE_SRC="nasdaq_itch_example_application.cpp"
PERF_TEST_SRC="nasdaq_itch_performance_test.cpp"

# Build targets
echo "Compiling plugin library..."
$CXX $CXXFLAGS -c $PLUGIN_SRC -o nasdaq_itch_feed_handler.o

echo "Building example application..."
$CXX $CXXFLAGS $PLUGIN_SRC $EXAMPLE_SRC $LDFLAGS -o nasdaq_itch_example

echo "Building performance test..."
$CXX $CXXFLAGS $PLUGIN_SRC $PERF_TEST_SRC $LDFLAGS -o nasdaq_itch_perf_test

echo ""
echo "Build complete! Executables created:"
echo "  - nasdaq_itch_example     (Example market data application)"
echo "  - nasdaq_itch_perf_test   (Performance benchmarking)"
echo ""
echo "Run tests:"
echo "  ./nasdaq_itch_example"
echo "  ./nasdaq_itch_perf_test"
echo ""
