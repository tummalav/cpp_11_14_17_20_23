#!/bin/bash

# Smart C++ Compilation Script
# Automatically detects and uses the best available C++ standard
# Prefers C++20, falls back to C++17 if C++20 has issues

set -e

if [ $# -eq 0 ]; then
    echo "Usage: $0 <source_file.cpp> [output_name]"
    echo "Example: $0 my_program.cpp my_program"
    echo ""
    echo "This script automatically detects the best C++ standard to use:"
    echo "  1. Tries C++20 (c++20 or c++2a)"
    echo "  2. Falls back to C++17 if C++20 has issues"
    echo "  3. Falls back to C++14 if C++17 is not available"
    exit 1
fi

SOURCE_FILE="$1"
OUTPUT_NAME="${2:-${SOURCE_FILE%.*}}"

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file '$SOURCE_FILE' not found"
    exit 1
fi

echo "Smart C++ Compilation for: $SOURCE_FILE"
echo "========================================="

# Function to test compilation with specific standard
test_compilation() {
    local std_flag="$1"
    local test_file="${SOURCE_FILE%.*}_test_$$"

    echo "Testing compilation with $std_flag..."

    # Try to compile
    if g++ "$std_flag" -pthread -Wall -Wextra -O2 "$SOURCE_FILE" -o "$test_file" 2>/dev/null; then
        # Test if the binary runs without hanging (for 3 seconds max)
        if timeout 3s "./$test_file" >/dev/null 2>&1; then
            rm -f "$test_file"
            echo "✅ $std_flag compilation and execution successful"
            return 0
        else
            rm -f "$test_file"
            echo "⚠️  $std_flag compilation successful but execution failed/hung"
            return 1
        fi
    else
        rm -f "$test_file"
        echo "❌ $std_flag compilation failed"
        return 1
    fi
}

# Array of C++ standards to try (in order of preference)
STANDARDS=(
    "-std=c++20"
    "-std=c++2a"
    "-std=c++17"
    "-std=c++14"
)

SELECTED_STANDARD=""

# Try each standard until one works
for std in "${STANDARDS[@]}"; do
    if test_compilation "$std"; then
        SELECTED_STANDARD="$std"
        break
    fi
done

if [ -z "$SELECTED_STANDARD" ]; then
    echo ""
    echo "❌ Error: No working C++ standard found"
    echo "Tried: ${STANDARDS[*]}"
    exit 1
fi

echo ""
echo "🎯 Selected standard: $SELECTED_STANDARD"
echo "Compiling final version..."

# Compile with selected standard
g++ "$SELECTED_STANDARD" -pthread -Wall -Wextra -O2 "$SOURCE_FILE" -o "$OUTPUT_NAME"

if [ $? -eq 0 ]; then
    echo "✅ Final compilation successful: $OUTPUT_NAME"
    echo "📋 Standard used: $SELECTED_STANDARD"
    echo "🚀 Run with: ./$OUTPUT_NAME"

    # Display some info about the compiled binary
    if command -v file >/dev/null 2>&1; then
        echo "📄 Binary info: $(file "$OUTPUT_NAME")"
    fi

    if command -v du >/dev/null 2>&1; then
        echo "📏 Binary size: $(du -h "$OUTPUT_NAME" | cut -f1)"
    fi
else
    echo "❌ Final compilation failed"
    exit 1
fi
