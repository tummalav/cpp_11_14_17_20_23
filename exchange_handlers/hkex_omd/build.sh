#!/bin/bash

# HKEX OMD Market Data Feed Handler Build Script
# Ultra-low latency compilation with optimization flags

set -e

echo "Building HKEX OMD Market Data Feed Handler..."
echo "============================================="

# Compiler settings
CXX=g++
CXXFLAGS="-std=c++17 -O3 -march=native -Wall -Wextra -Wno-unused-parameter"
LDFLAGS="-pthread"

# Source files
PLUGIN_SRC="hkex_omd_feed_handler.cpp"
EXAMPLE_SRC="hkex_omd_example_application.cpp"
PERF_TEST_SRC="hkex_omd_performance_test.cpp"

# Build targets
echo "Compiling plugin library..."
$CXX $CXXFLAGS -c $PLUGIN_SRC -o hkex_omd_feed_handler.o

echo "Building example application..."
$CXX $CXXFLAGS $PLUGIN_SRC $EXAMPLE_SRC $LDFLAGS -o hkex_omd_example

echo "Building performance test..."
$CXX $CXXFLAGS $PLUGIN_SRC $PERF_TEST_SRC $LDFLAGS -o hkex_omd_perf_test

echo ""
echo "Build complete! Executables created:"
echo "  - hkex_omd_example     (Example market data application)"
echo "  - hkex_omd_perf_test   (Performance benchmarking)"
echo ""
echo "Run tests:"
echo "  ./hkex_omd_example"
echo "  ./hkex_omd_perf_test"
echo ""
