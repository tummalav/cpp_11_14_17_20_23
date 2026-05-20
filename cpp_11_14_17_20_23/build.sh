#!/bin/bash

# Build script for ASX OUCH Order Handler
# Usage: ./build.sh [clean|debug|release|performance]

set -e

BUILD_TYPE="Release"
CLEAN_BUILD=false
PERFORMANCE_BUILD=false

# Parse command line arguments
case "${1:-}" in
    clean)
        CLEAN_BUILD=true
        ;;
    debug)
        BUILD_TYPE="Debug"
        ;;
    release)
        BUILD_TYPE="Release"
        ;;
    performance)
        BUILD_TYPE="Release"
        PERFORMANCE_BUILD=true
        ;;
    *)
        echo "Usage: $0 [clean|debug|release|performance]"
        echo "  clean       - Clean build directory"
        echo "  debug       - Debug build"
        echo "  release     - Release build (default)"
        echo "  performance - Performance optimized build"
        exit 1
        ;;
esac

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

echo "ASX OUCH Order Handler Build Script"
echo "=================================="
echo "Project Root: ${PROJECT_ROOT}"
echo "Build Type: ${BUILD_TYPE}"
echo "Build Directory: ${BUILD_DIR}"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure CMake
echo "Configuring CMake..."
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

# Performance optimizations
if [ "$PERFORMANCE_BUILD" = true ]; then
    echo "Enabling performance optimizations..."
    CMAKE_ARGS+=(
        -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native -flto -ffast-math -funroll-loops"
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
    )
fi

cmake "${CMAKE_ARGS[@]}" "${PROJECT_ROOT}"

# Build
echo "Building..."
CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
make -j"${CPU_COUNT}"

echo ""
echo "Build completed successfully!"
echo ""
echo "Executables:"
echo "  ${BUILD_DIR}/ouch_example - Example application"
echo "  ${BUILD_DIR}/ouch_performance_test - Performance test"
echo ""
echo "Library:"
echo "  ${BUILD_DIR}/ouch_asx_plugin.so - OUCH plugin library"
echo ""

# Run basic validation
echo "Running basic validation..."
if [ -f "${BUILD_DIR}/ouch_example" ]; then
    echo "✓ Example application built successfully"
else
    echo "✗ Example application not found"
    exit 1
fi

if [ -f "${BUILD_DIR}/ouch_performance_test" ]; then
    echo "✓ Performance test built successfully"
else
    echo "✗ Performance test not found"
    exit 1
fi

if [ -f "${BUILD_DIR}/ouch_asx_plugin.so" ]; then
    echo "✓ Plugin library built successfully"
else
    echo "✗ Plugin library not found"
    exit 1
fi

echo ""
echo "All builds validated successfully!"
echo ""
echo "To run the example application:"
echo "  cd ${BUILD_DIR} && ./ouch_example"
echo ""
echo "To run performance tests:"
echo "  cd ${BUILD_DIR} && ./ouch_performance_test [num_orders] [orders_per_sec] [num_threads]"
echo ""
