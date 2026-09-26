#!/bin/bash
#
# Build script for the ULL Order Book / SOR / Feed Handler / Algo
# data-structure reference implementations described in:
#   03_trading_apps/DATA_STRUCTURES_FOR_ULL_ORDERBOOK_SOR_FEEDS_ALGOS.md
#
# Builds all in-house / Abseil / Folly / moodycamel variants for the four
# subsystems and runs each as a quick smoke test + micro-benchmark.
#
# Requirements:
#   - macOS: brew install abseil folly (moodycamel is vendored under
#            third_party/moodycamel/ -- no install needed)
#   - A C++20-capable compiler. On this repo's dev machine, Apple's bundled
#     clang (Command Line Tools) is too old for modern Abseil/Folly headers;
#     use Homebrew's llvm (`brew install llvm`) instead -- this script
#     auto-detects it.

set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRADING_APPS="$ROOT_DIR/03_trading_apps"
MOODYCAMEL_INC="$ROOT_DIR/third_party/moodycamel"
OUT_DIR="$ROOT_DIR/build/data_structures_variants"
mkdir -p "$OUT_DIR"

echo "════════════════════════════════════════════════════════════════"
echo "  Building ULL Data Structures Reference Implementations"
echo "════════════════════════════════════════════════════════════════"

# --- Compiler selection -----------------------------------------------
CXX="g++"
SYSROOT_FLAGS=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    BREW_LLVM_CLANGXX="$(ls /usr/local/Cellar/llvm/*/bin/clang++ 2>/dev/null | tail -1)"
    if [[ -z "$BREW_LLVM_CLANGXX" ]]; then
        BREW_LLVM_CLANGXX="$(ls /opt/homebrew/Cellar/llvm/*/bin/clang++ 2>/dev/null | tail -1)"
    fi
    if [[ -n "$BREW_LLVM_CLANGXX" ]]; then
        CXX="$BREW_LLVM_CLANGXX"
        SYSROOT_FLAGS="-isysroot $(xcrun --show-sdk-path)"
        echo "✓ Using Homebrew LLVM clang++: $CXX"
    else
        echo "! Homebrew llvm not found (brew install llvm); falling back to system compiler."
        echo "  Modern Abseil/Folly headers may fail to compile with an old Apple clang."
    fi
fi

ABSL_LIBS="-labsl_raw_hash_set -labsl_hash -labsl_hashtablez_sampler -labsl_synchronization \
-labsl_stacktrace -labsl_symbolize -labsl_debugging_internal -labsl_demangle_internal \
-labsl_base -labsl_spinlock_wait -labsl_throw_delegate -labsl_raw_logging_internal"

FOLLY_LIBS="-lfolly -lglog -lgflags -lfmt -ldouble-conversion -lboost_context -lboost_filesystem"

build_inhouse() {
    local name="$1" src="$2"
    echo "→ [in-house]    $name"
    $CXX -std=c++17 -O3 -march=native -DNDEBUG $SYSROOT_FLAGS "$src" -lpthread -o "$OUT_DIR/$name"
}

build_abseil() {
    local name="$1" src="$2"
    echo "→ [abseil]      $name"
    $CXX -std=c++17 -O3 -march=native -DNDEBUG $SYSROOT_FLAGS "$src" \
        -I/usr/local/include -L/usr/local/lib $ABSL_LIBS -lpthread -o "$OUT_DIR/$name"
}

build_folly() {
    local name="$1" src="$2"
    echo "→ [folly]       $name"
    $CXX -std=c++20 -O3 -march=native -DNDEBUG $SYSROOT_FLAGS "$src" \
        -I/usr/local/include -L/usr/local/lib $FOLLY_LIBS -lpthread -o "$OUT_DIR/$name"
}

build_moodycamel() {
    local name="$1" src="$2"
    echo "→ [moodycamel]  $name"
    $CXX -std=c++17 -O3 -march=native -DNDEBUG $SYSROOT_FLAGS "$src" -I"$MOODYCAMEL_INC" -lpthread -o "$OUT_DIR/$name"
}

# --- Order Book ---------------------------------------------------------
build_abseil orderbook_abseil_variant "$TRADING_APPS/orderbook/orderbook_abseil_variant.cpp"
build_folly  orderbook_folly_variant  "$TRADING_APPS/orderbook/orderbook_folly_variant.cpp"

# --- Smart Order Router (SOR) --------------------------------------------
build_inhouse   sor_inhouse           "$TRADING_APPS/sor/sor_inhouse.cpp"
build_abseil    sor_abseil_variant    "$TRADING_APPS/sor/sor_abseil_variant.cpp"
build_folly     sor_folly_variant     "$TRADING_APPS/sor/sor_folly_variant.cpp"
build_moodycamel sor_moodycamel_queue "$TRADING_APPS/sor/sor_moodycamel_queue.cpp"

# --- Feed Handlers --------------------------------------------------------
build_moodycamel feed_handler_moodycamel_queue     "$TRADING_APPS/feed_handlers/feed_handler_moodycamel_queue.cpp"
build_abseil     feed_handler_abseil_symbol_table   "$TRADING_APPS/feed_handlers/feed_handler_abseil_symbol_table.cpp"

# --- Execution Algos --------------------------------------------------------
build_abseil algo_child_order_tracker_abseil "$TRADING_APPS/execution_algos/algo_child_order_tracker_abseil.cpp"
build_folly  algo_rolling_window_folly       "$TRADING_APPS/execution_algos/algo_rolling_window_folly.cpp"

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "✓ Build complete. Binaries in: $OUT_DIR"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "Run all smoke tests / micro-benchmarks:"
echo "  for b in $OUT_DIR/*; do echo \"--- \$b ---\"; \"\$b\"; echo; done"
