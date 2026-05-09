/**
 * Abseil Containers Comprehensive Guide and Benchmarks
 *
 * Google's Abseil C++ Library - High-Performance Containers
 * Focus: Ultra-low latency, cache-friendly, production-ready
 * Platform: RHEL 8/9 (GCC 8+, glibc 2.28+) and macOS (development)
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CONTAINERS COVERED:
 * ═══════════════════════════════════════════════════════════════════════════
 *  Hash (Swiss Table - SIMD-accelerated open addressing):
 *    absl::flat_hash_map   — fastest lookups, best cache locality (15-60ns)
 *    absl::flat_hash_set   — set variant of flat_hash_map
 *    absl::node_hash_map   — pointer-stable (safe to store iterators)
 *    absl::node_hash_set   — set variant of node_hash_map
 *
 *  Ordered (B-tree - cache-friendly vs std::map red-black tree):
 *    absl::btree_map       — ordered, 2-3x faster than std::map (30-120ns)
 *    absl::btree_set       — ordered set
 *    absl::btree_multimap  — ordered multi-value map
 *    absl::btree_multiset  — ordered multi-value set
 *
 *  Sequential (SSO - avoid heap for small sizes):
 *    absl::InlinedVector   — SSO vector: N elements inline, spills to heap
 *    absl::FixedArray      — runtime-size, never resized
 *    absl::Span            — non-owning zero-copy view (like std::span)
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * TRADING USE CASES IN THIS FILE:
 * ═══════════════════════════════════════════════════════════════════════════
 *  1. Order Book price levels      → btree_map<Price, InlinedVector<Order,8>>
 *  2. Symbol → price cache         → flat_hash_map<uint32_t, double>
 *  3. Active orders by ID          → node_hash_map<uint64_t, Order>
 *  4. SOR routing table            → flat_hash_map<VenueID, VenueState>
 *  5. Dark pool crossing engine    → btree_multimap<Price, Order>
 *  6. ETF arb basket lookup        → flat_hash_map<BasketID, Span<Leg>>
 *  7. Index arb universe           → flat_hash_map<SymbolID, Weight>
 *  8. Principal internalizer       → flat_hash_map<OrderID, InternalMatch>
 *  9. Market making spread table   → flat_hash_map<InstrumentID, Quote>
 * 10. Risk limit table             → flat_hash_map<DeskID, RiskLimits>
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * SWISS TABLE INTERNALS (Why flat_hash_map is 2-3x faster):
 * ═══════════════════════════════════════════════════════════════════════════
 *  Memory layout per group (group = 16 slots):
 *    [ctrl[0..15]: 16 × 1 byte control][data[0..15]: 16 × sizeof(T) bytes]
 *    Control byte: 0xFF=empty, 0xFE=deleted, 0b0xxxxxxx=H2(7 low hash bits)
 *
 *  Lookup algo (SIMD, 1 cache line per probe group):
 *    1. H1 = hash >> 7  → slot group index
 *    2. H2 = hash & 127 → 7-bit fingerprint
 *    3. Load 16 ctrl bytes into SSE2/NEON register (1 cache line read)
 *    4. SIMD compare: find lanes where ctrl == H2 (16 candidates in parallel)
 *    5. For each match: compare full key. Usually 0-1 matches.
 *    → Result: avg 0-2 cache line reads per lookup vs O(1) avg but scattered
 *      memory for std::unordered_map (separate chaining = pointer chase)
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * RHEL BUILD INSTRUCTIONS:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  # Install deps:
 *  sudo dnf install -y gcc-c++ cmake git
 *
 *  # Option A: Build Abseil from source (recommended for RHEL):
 *  git clone https://github.com/abseil/abseil-cpp.git && cd abseil-cpp
 *  mkdir build && cd build
 *  cmake .. -DCMAKE_BUILD_TYPE=Release \
 *            -DCMAKE_INSTALL_PREFIX=/usr/local \
 *            -DCMAKE_CXX_STANDARD=17 \
 *            -DABSL_BUILD_TESTS=OFF
 *  make -j$(nproc) && sudo make install
 *  sudo ldconfig
 *
 *  # Option B: vcpkg (RHEL):
 *  vcpkg install abseil
 *
 *  # Compile (RHEL - standard):
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG -D_GNU_SOURCE \
 *      abseil_containers_comprehensive.cpp \
 *      -labsl_base -labsl_hash -labsl_raw_hash_set \
 *      -labsl_hashtablez_sampler -labsl_strings \
 *      -lpthread -o abseil_benchmark
 *
 *  # Compile (macOS - Homebrew):
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *      abseil_containers_comprehensive.cpp \
 *      -I/opt/homebrew/include -L/opt/homebrew/lib \
 *      -labsl_base -labsl_hash -labsl_raw_hash_set \
 *      -labsl_hashtablez_sampler -labsl_strings \
 *      -lpthread -o abseil_benchmark
 *
 *  # Run:
 *  ./abseil_benchmark
 *
 *  # RHEL tuning for ULL benchmarks:
 *  sudo tuned-adm profile latency-performance
 *  sudo cpupower frequency-set -g performance
 */

// _GNU_SOURCE required on RHEL for mlock, pthread_setaffinity_np, SCHED_FIFO
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <random>
#include <thread>
#include <atomic>
#include <cstring>
#include <numeric>
#include <cstdint>
#include <climits>

// Abseil Hash Containers (Swiss Tables)
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"

// Abseil Ordered Containers (B-tree — 2-3x faster than std::map)
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"

// Abseil Sequential Containers (SSO / fixed-size / zero-copy views)
#include "absl/container/inlined_vector.h"
#include "absl/container/fixed_array.h"

// absl::Span: non-owning, zero-copy view into contiguous data
// Use instead of (ptr, len) pairs — no extra allocation, no copy
#include "absl/types/span.h"

// Abseil Utilities
#include "absl/strings/string_view.h"
#include "absl/hash/hash.h"

//=============================================================================
// PERFORMANCE MEASUREMENT UTILITIES
//=============================================================================

class LatencyStats {
public:
    std::vector<uint64_t> measurements;

    void add(uint64_t ns) {
        measurements.push_back(ns);
    }

    void reset() {
        measurements.clear();
    }

    void print(const std::string& name) const {
        if (measurements.empty()) return;

        auto sorted = measurements;
        std::sort(sorted.begin(), sorted.end());

        uint64_t sum = 0;
        for (auto m : sorted) sum += m;

        std::cout << std::left << std::setw(55) << name
                  << " | Avg: " << std::setw(8) << (sum / sorted.size()) << " ns"
                  << " | P50: " << std::setw(8) << sorted[sorted.size() * 50 / 100] << " ns"
                  << " | P99: " << std::setw(8) << sorted[sorted.size() * 99 / 100] << " ns"
                  << " | P99.9: " << std::setw(8) << sorted[sorted.size() * 999 / 1000] << " ns\n";
    }
};

template<typename Func>
uint64_t measure_latency_ns(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

//=============================================================================
// TEST DATA STRUCTURES
// All structs use fixed-size integer keys (uint64_t/uint32_t) for HFT.
// String keys require hashing variable-length data → 2-5x slower.
// Use symbol_id (uint32_t) or packed_symbol (8-char → uint64_t) instead.
//=============================================================================

// ── Fixed-point price arithmetic (no float in hot path) ──────────────────
// Prices stored as int64_t × PRICE_SCALE to avoid floating-point rounding.
static constexpr int64_t PRICE_SCALE = 1'000'000; // 6 decimal places
inline int64_t to_fp(double price) noexcept {
    return static_cast<int64_t>(price * PRICE_SCALE);
}
inline double from_fp(int64_t fp) noexcept {
    return static_cast<double>(fp) / PRICE_SCALE;
}

// ── Hot data first, cold data after first cache line ─────────────────────
struct alignas(64) Order {
    // CL0 hot: fields read on every match/cancel
    uint64_t order_id;
    uint64_t symbol_key;    // packed 8-char symbol as uint64_t
    int64_t  price_fp;      // fixed-point price (no float)
    uint32_t quantity;
    uint32_t remaining_qty;
    uint8_t  side;          // 'B'=buy, 'S'=sell
    uint8_t  order_type;    // 'L'=limit, 'M'=market
    uint8_t  tif;           // 'D'=day, 'I'=IOC, 'G'=GTC
    uint8_t  venue_id;      // destination venue for SOR
    uint32_t strategy_id;

    // CL0 cold: timestamp (only read for audit/logging)
    uint64_t recv_tsc;

    Order() noexcept { std::memset(this, 0, sizeof(*this)); }
    Order(uint64_t id, double price, uint32_t qty, char s, uint8_t venue = 0) noexcept
        : order_id(id), symbol_key(0), price_fp(to_fp(price))
        , quantity(qty), remaining_qty(qty)
        , side(static_cast<uint8_t>(s)), order_type('L'), tif('D')
        , venue_id(venue), strategy_id(0), recv_tsc(0) {}

    bool operator==(const Order& o) const noexcept { return order_id == o.order_id; }
    bool operator<(const Order& o)  const noexcept { return order_id < o.order_id; }
};

// Abseil hash support for Order (use order_id as key)
template <typename H>
H AbslHashValue(H h, const Order& o) {
    return H::combine(std::move(h), o.order_id);
}

// ── Market data tick ──────────────────────────────────────────────────────
struct alignas(64) MarketData {
    uint64_t recv_tsc;
    uint32_t symbol_id;
    int64_t  bid_fp;        // fixed-point
    int64_t  ask_fp;
    uint32_t bid_size;
    uint32_t ask_size;
    uint64_t seq;

    MarketData() noexcept { std::memset(this, 0, sizeof(*this)); }
    MarketData(uint64_t ts, uint32_t sym, double bid, double ask,
               uint32_t bs, uint32_t as) noexcept
        : recv_tsc(ts), symbol_id(sym)
        , bid_fp(to_fp(bid)), ask_fp(to_fp(ask))
        , bid_size(bs), ask_size(as), seq(0) {}
};

// ── SOR venue state ───────────────────────────────────────────────────────
struct alignas(64) VenueState {
    uint32_t venue_id;
    uint32_t avail_qty;     // current available liquidity
    int64_t  best_bid_fp;
    int64_t  best_ask_fp;
    uint32_t fill_rate_bps; // fill probability in basis points (0-10000)
    uint32_t latency_us;    // round-trip latency to venue in microseconds
    uint8_t  active;        // 1=connected, 0=disconnected
    char     venue_code[7]; // e.g. "NYSE   ", "NASDAQ ", "ARCA   "

    VenueState() noexcept { std::memset(this, 0, sizeof(*this)); }
};

// ── ETF basket leg ────────────────────────────────────────────────────────
struct BasketLeg {
    uint32_t symbol_id;
    int32_t  shares;        // positive=long, negative=short
    double   weight;        // portfolio weight
};

// ── Risk limits ───────────────────────────────────────────────────────────
struct RiskLimits {
    int64_t  max_position_fp;   // max net position in notional (fixed-point)
    int64_t  max_pnl_loss_fp;   // max daily loss
    uint32_t max_order_qty;     // max single order size
    uint32_t max_order_rate;    // max orders per second
    uint8_t  hard_kill;         // 1=kill switch activated
    char     desk_name[31];
};

// ── Internal match (principal internalizer) ───────────────────────────────
struct InternalMatch {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    int64_t  match_price_fp;
    uint32_t match_qty;
    uint64_t match_tsc;         // timestamp of match
};

// ── Market making quote ───────────────────────────────────────────────────
struct alignas(64) MMQuote {
    uint32_t instrument_id;
    int64_t  bid_fp;
    int64_t  ask_fp;
    uint32_t bid_qty;
    uint32_t ask_qty;
    uint64_t update_tsc;
    uint16_t spread_bps;        // current spread in basis points
    uint8_t  active;
    char     _pad[5];
};

//=============================================================================
// 1. ABSEIL HASH CONTAINERS (Swiss Tables)
//=============================================================================

void benchmark_abseil_hash_containers() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ABSEIL HASH CONTAINERS (Swiss Tables)                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Swiss Tables: Google's optimized hash tables with:\n";
    std::cout << "  • Open addressing with quadratic probing\n";
    std::cout << "  • SIMD-based parallel probing (SSE2/NEON)\n";
    std::cout << "  • Excellent cache locality\n";
    std::cout << "  • 15-60ns lookup (2-3x faster than std::unordered_map)\n\n";

    constexpr size_t NUM_ELEMENTS = 10000;
    constexpr size_t ITERATIONS = 100;

    // absl::flat_hash_map - Most common, best performance
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "absl::flat_hash_map<uint64_t, Order>\n";
        std::cout << "  • Inline storage: values stored directly in table\n";
        std::cout << "  • Best cache performance\n";
        std::cout << "  • Invalidates references on rehash\n\n";

        LatencyStats insert_stats, lookup_stats, erase_stats;

        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            absl::flat_hash_map<uint64_t, Order> map;
            map.reserve(NUM_ELEMENTS);

            // Insert benchmark
            auto ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    map.emplace(i, Order(i, 100.0 + i, 100, 'B'));
                }
            });
            insert_stats.add(ns);

            // Lookup benchmark
            ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    volatile auto it = map.find(i);
                }
            });
            lookup_stats.add(ns);

            // Erase benchmark
            ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS / 10; ++i) {
                    map.erase(i);
                }
            });
            erase_stats.add(ns);
        }

        insert_stats.print("  INSERT (10K elements)");
        lookup_stats.print("  LOOKUP (10K elements)");
        erase_stats.print("  ERASE (1K elements)");
    }

    // absl::flat_hash_set
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "absl::flat_hash_set<uint64_t>\n";
        std::cout << "  • Set version of flat_hash_map\n";
        std::cout << "  • Same performance characteristics\n\n";

        LatencyStats insert_stats, lookup_stats;

        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            absl::flat_hash_set<uint64_t> set;
            set.reserve(NUM_ELEMENTS);

            auto ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    set.insert(i);
                }
            });
            insert_stats.add(ns);

            ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    volatile auto it = set.find(i);
                }
            });
            lookup_stats.add(ns);
        }

        insert_stats.print("  INSERT (10K elements)");
        lookup_stats.print("  LOOKUP (10K elements)");
    }

    // absl::node_hash_map - Stable pointers
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "absl::node_hash_map<uint64_t, Order>\n";
        std::cout << "  • Node-based storage (like std::unordered_map)\n";
        std::cout << "  • Stable pointers/references (never invalidated)\n";
        std::cout << "  • Slightly slower than flat_hash_map\n";
        std::cout << "  • Use when iterator/pointer stability needed\n\n";

        LatencyStats insert_stats, lookup_stats;

        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            absl::node_hash_map<uint64_t, Order> map;
            map.reserve(NUM_ELEMENTS);

            auto ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    map.emplace(i, Order(i, 100.0 + i, 100, 'B'));
                }
            });
            insert_stats.add(ns);

            ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    volatile auto it = map.find(i);
                }
            });
            lookup_stats.add(ns);
        }

        insert_stats.print("  INSERT (10K elements)");
        lookup_stats.print("  LOOKUP (10K elements)");
    }

    std::cout << "\n💡 Recommendation:\n";
    std::cout << "  • Use flat_hash_map for best performance (15-60ns lookup)\n";
    std::cout << "  • Use node_hash_map when pointer stability required\n";
    std::cout << "  • Always call reserve() to avoid rehashing\n";
}

//=============================================================================
// 2. ABSEIL B-TREE CONTAINERS
//=============================================================================

void benchmark_abseil_btree_containers() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ABSEIL B-TREE CONTAINERS                                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "B-tree Containers: Cache-friendly ordered containers\n";
    std::cout << "  • Better cache locality than std::map (red-black tree)\n";
    std::cout << "  • 30-120ns lookup (vs std::map 50-200ns)\n";
    std::cout << "  • Maintains sorted order\n";
    std::cout << "  • Ideal for range queries\n\n";

    constexpr size_t NUM_ELEMENTS = 10000;
    constexpr size_t ITERATIONS = 100;

    // absl::btree_map
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "absl::btree_map<uint64_t, Order>\n";
        std::cout << "  • Ordered associative container\n";
        std::cout << "  • Better than std::map for cache locality\n";
        std::cout << "  • Efficient range queries\n\n";

        LatencyStats insert_stats, lookup_stats, range_stats;

        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            absl::btree_map<uint64_t, Order> map;

            // Insert in random order
            std::vector<uint64_t> keys(NUM_ELEMENTS);
            std::iota(keys.begin(), keys.end(), 0);
            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(keys.begin(), keys.end(), gen);

            auto ns = measure_latency_ns([&]() {
                for (auto key : keys) {
                    map.emplace(key, Order(key, 100.0 + key, 100, 'B'));
                }
            });
            insert_stats.add(ns);

            // Lookup benchmark
            ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    volatile auto it = map.find(i);
                }
            });
            lookup_stats.add(ns);

            // Range query benchmark
            ns = measure_latency_ns([&]() {
                auto it_lower = map.lower_bound(1000);
                auto it_upper = map.upper_bound(2000);
                uint64_t count = 0;
                for (auto it = it_lower; it != it_upper; ++it) {
                    count++;
                }
                volatile auto result = count;
            });
            range_stats.add(ns);
        }

        insert_stats.print("  INSERT (10K random order)");
        lookup_stats.print("  LOOKUP (10K elements)");
        range_stats.print("  RANGE QUERY (1K elements)");
    }

    // absl::btree_set
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "absl::btree_set<uint64_t>\n";
        std::cout << "  • Ordered set container\n";
        std::cout << "  • Efficient for sorted unique elements\n\n";

        LatencyStats insert_stats, lookup_stats;

        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            absl::btree_set<uint64_t> set;

            auto ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    set.insert(i);
                }
            });
            insert_stats.add(ns);

            ns = measure_latency_ns([&]() {
                for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                    volatile auto it = set.find(i);
                }
            });
            lookup_stats.add(ns);
        }

        insert_stats.print("  INSERT (10K elements)");
        lookup_stats.print("  LOOKUP (10K elements)");
    }

    std::cout << "\n💡 Recommendation:\n";
    std::cout << "  • Use btree_map when you need sorted/ordered data\n";
    std::cout << "  • 2-3x faster than std::map for lookups\n";
    std::cout << "  • Excellent for range queries (orderbook price levels)\n";
}

//=============================================================================
// 3. ABSEIL SEQUENTIAL CONTAINERS
//=============================================================================

void benchmark_abseil_sequential_containers() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ABSEIL SEQUENTIAL CONTAINERS                              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    constexpr size_t NUM_ELEMENTS = 1000;
    constexpr size_t ITERATIONS = 1000;

    // absl::InlinedVector - SSO for vectors
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "absl::InlinedVector<Order, N>\n";
        std::cout << "  • Small Size Optimization (SSO)\n";
        std::cout << "  • N elements stored inline (stack/object)\n";
        std::cout << "  • ZERO heap allocation for small sizes\n";
        std::cout << "  • Spills to heap when size > N\n\n";

        // Small size (inline storage)
        {
            std::cout << "InlinedVector<Order, 32> - Small size (≤32):\n";
            LatencyStats stats;

            for (size_t iter = 0; iter < ITERATIONS; ++iter) {
                auto ns = measure_latency_ns([&]() {
                    absl::InlinedVector<Order, 32> vec;
                    for (size_t i = 0; i < 32; ++i) {
                        vec.emplace_back(i, 100.0 + i, 100, 'B');
                    }

                    // Read back
                    uint64_t sum = 0;
                    for (const auto& order : vec) {
                        sum += order.order_id;
                    }
                    volatile auto result = sum;
                });
                stats.add(ns);
            }

            stats.print("  32 elements (inline, ZERO heap)");
        }

        // Large size (heap storage)
        {
            std::cout << "\nInlinedVector<Order, 32> - Large size (>32):\n";
            LatencyStats stats;

            for (size_t iter = 0; iter < ITERATIONS; ++iter) {
                auto ns = measure_latency_ns([&]() {
                    absl::InlinedVector<Order, 32> vec;
                    vec.reserve(NUM_ELEMENTS);
                    for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                        vec.emplace_back(i, 100.0 + i, 100, 'B');
                    }

                    uint64_t sum = 0;
                    for (const auto& order : vec) {
                        sum += order.order_id;
                    }
                    volatile auto result = sum;
                });
                stats.add(ns);
            }

            stats.print("  1000 elements (heap allocated)");
        }
    }

    // absl::FixedArray - Stack allocation with runtime size
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "absl::FixedArray<Order>\n";
        std::cout << "  • Runtime-sized array\n";
        std::cout << "  • Small sizes use stack (typically ≤256 bytes)\n";
        std::cout << "  • Large sizes use heap (one-time allocation)\n";
        std::cout << "  • Size cannot change after construction\n\n";

        {
            std::cout << "FixedArray - Small size (stack):\n";
            LatencyStats stats;

            for (size_t iter = 0; iter < ITERATIONS; ++iter) {
                auto ns = measure_latency_ns([&]() {
                    absl::FixedArray<Order> arr(10);  // Small, likely stack
                    for (size_t i = 0; i < 10; ++i) {
                        arr[i] = Order(i, 100.0 + i, 100, 'B');
                    }

                    uint64_t sum = 0;
                    for (const auto& order : arr) {
                        sum += order.order_id;
                    }
                    volatile auto result = sum;
                });
                stats.add(ns);
            }

            stats.print("  10 elements");
        }

        {
            std::cout << "\nFixedArray - Large size (heap):\n";
            LatencyStats stats;

            for (size_t iter = 0; iter < ITERATIONS; ++iter) {
                auto ns = measure_latency_ns([&]() {
                    absl::FixedArray<Order> arr(NUM_ELEMENTS);
                    for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
                        arr[i] = Order(i, 100.0 + i, 100, 'B');
                    }

                    uint64_t sum = 0;
                    for (const auto& order : arr) {
                        sum += order.order_id;
                    }
                    volatile auto result = sum;
                });
                stats.add(ns);
            }

            stats.print("  1000 elements");
        }
    }

    std::cout << "\n💡 Recommendation:\n";
    std::cout << "  • Use InlinedVector<T, N> for frequently created small vectors\n";
    std::cout << "  • Use FixedArray when size is runtime but doesn't change\n";
    std::cout << "  • Both avoid heap allocation for small sizes\n";
}

//=============================================================================
// 4. PRACTICAL TRADING EXAMPLES
//=============================================================================

void practical_trading_examples() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PRACTICAL TRADING SYSTEM EXAMPLES                         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // Example 1: Order Book Price Levels
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "Example 1: Order Book Price Levels\n";
        std::cout << "  Use Case: Store orders at each price level\n";
        std::cout << "  Container: absl::btree_map<Price, OrderQueue>\n\n";

        using Price = double;
        using OrderQueue = absl::InlinedVector<Order, 8>;  // Most levels have <8 orders

        absl::btree_map<Price, OrderQueue> bid_levels;
        absl::btree_map<Price, OrderQueue> ask_levels;

        // Add some orders
        LatencyStats add_stats;
        for (size_t i = 0; i < 1000; ++i) {
            auto ns = measure_latency_ns([&]() {
                Price price = 100.0 + (i % 100) * 0.01;
                bid_levels[price].emplace_back(i, price, 100, 'B');
            });
            add_stats.add(ns);
        }

        // Best bid/ask lookup
        LatencyStats lookup_stats;
        for (size_t i = 0; i < 1000; ++i) {
            auto ns = measure_latency_ns([&]() {
                if (!bid_levels.empty()) {
                    volatile auto best_bid = bid_levels.rbegin()->first;
                }
                if (!ask_levels.empty()) {
                    volatile auto best_ask = ask_levels.begin()->first;
                }
            });
            lookup_stats.add(ns);
        }

        add_stats.print("  Add order to price level");
        lookup_stats.print("  Get best bid/ask");

        std::cout << "  ✅ Benefits: Sorted prices, fast range queries, cache-friendly\n";
    }

    // Example 2: Symbol to Last Price Mapping
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "Example 2: Symbol → Last Price Cache\n";
        std::cout << "  Use Case: Fast lookup of last traded price by symbol\n";
        std::cout << "  Container: absl::flat_hash_map<SymbolID, Price>\n\n";

        absl::flat_hash_map<uint32_t, double> last_prices;
        last_prices.reserve(10000);  // Pre-allocate for 10K symbols

        // Update prices
        LatencyStats update_stats;
        for (size_t i = 0; i < 10000; ++i) {
            auto ns = measure_latency_ns([&]() {
                last_prices[i % 5000] = 100.0 + (i % 100);
            });
            update_stats.add(ns);
        }

        // Lookup prices
        LatencyStats lookup_stats;
        for (size_t i = 0; i < 10000; ++i) {
            auto ns = measure_latency_ns([&]() {
                volatile auto price = last_prices[i % 5000];
            });
            lookup_stats.add(ns);
        }

        update_stats.print("  Update price");
        lookup_stats.print("  Lookup price");

        std::cout << "  ✅ Benefits: 15-60ns lookup, excellent for hot data\n";
    }

    // Example 3: Order ID to Order Mapping
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "Example 3: Order ID → Order Details\n";
        std::cout << "  Use Case: Quickly find order by ID for modifications/cancels\n";
        std::cout << "  Container: absl::node_hash_map<OrderID, Order>\n";
        std::cout << "  Why node_hash_map: Pointers never invalidate\n\n";

        absl::node_hash_map<uint64_t, Order> active_orders;
        active_orders.reserve(50000);

        // Add orders
        LatencyStats add_stats;
        for (size_t i = 0; i < 10000; ++i) {
            auto ns = measure_latency_ns([&]() {
                active_orders.emplace(i, Order(i, 100.0 + i * 0.01, 100, 'B'));
            });
            add_stats.add(ns);
        }

        // Modify order (pointer stability matters)
        LatencyStats modify_stats;
        for (size_t i = 0; i < 10000; ++i) {
            auto ns = measure_latency_ns([&]() {
                auto it = active_orders.find(i % 5000);
                if (it != active_orders.end()) {
                    it->second.quantity = 200;  // Modify in place
                }
            });
            modify_stats.add(ns);
        }

        add_stats.print("  Add order");
        modify_stats.print("  Find and modify order");

        std::cout << "  ✅ Benefits: Stable pointers, safe to store iterators\n";
    }

    // Example 4: Recent Trades Buffer
    {
        std::cout << "\n──────────────────────────────────────────────────────────\n";
        std::cout << "Example 4: Recent Trades Buffer (Circular)\n";
        std::cout << "  Use Case: Keep last N trades for VWAP calculation\n";
        std::cout << "  Container: absl::FixedArray<Trade> or InlinedVector\n\n";

        struct Trade {
            uint64_t timestamp;
            double price;
            uint32_t quantity;
        };

        constexpr size_t BUFFER_SIZE = 1000;
        absl::FixedArray<Trade> recent_trades(BUFFER_SIZE);
        size_t write_index = 0;

        LatencyStats add_stats, vwap_stats;

        // Add trades (circular buffer)
        for (size_t i = 0; i < 10000; ++i) {
            auto ns = measure_latency_ns([&]() {
                recent_trades[write_index] = {i, 100.0 + (i % 100) * 0.01, 100};
                write_index = (write_index + 1) % BUFFER_SIZE;
            });
            add_stats.add(ns);

            // Calculate VWAP from buffer
            if (i % 100 == 0) {
                ns = measure_latency_ns([&]() {
                    double total_value = 0.0;
                    uint64_t total_volume = 0;
                    for (const auto& trade : recent_trades) {
                        total_value += trade.price * trade.quantity;
                        total_volume += trade.quantity;
                    }
                    volatile double vwap = total_value / total_volume;
                });
                vwap_stats.add(ns);
            }
        }

        add_stats.print("  Add trade to buffer");
        vwap_stats.print("  Calculate VWAP (1000 trades)");

        std::cout << "  ✅ Benefits: Fixed memory, cache-friendly iteration\n";
    }
}

//=============================================================================
// 5. COMPARISON TABLE
//=============================================================================

void print_comparison_table() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ABSEIL CONTAINERS COMPARISON SUMMARY                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "┌────────────────────────────┬─────────────┬──────────────┬────────────────────────┐\n";
    std::cout << "│ Container                  │ Lookup      │ Insert       │ Best Use Case          │\n";
    std::cout << "├────────────────────────────┼─────────────┼──────────────┼────────────────────────┤\n";
    std::cout << "│ HASH CONTAINERS (Unordered)                                                      │\n";
    std::cout << "├────────────────────────────┼─────────────┼──────────────┼────────────────────────┤\n";
    std::cout << "│ flat_hash_map              │ 15-60ns ✅  │ 30-100ns     │ Fast lookups, general  │\n";
    std::cout << "│ flat_hash_set              │ 15-60ns ✅  │ 30-100ns     │ Unique elements        │\n";
    std::cout << "│ node_hash_map              │ 20-80ns     │ 40-120ns     │ Stable pointers needed │\n";
    std::cout << "│ node_hash_set              │ 20-80ns     │ 40-120ns     │ Stable pointers needed │\n";
    std::cout << "├────────────────────────────┼─────────────┼──────────────┼────────────────────────┤\n";
    std::cout << "│ ORDERED CONTAINERS (B-tree)                                                      │\n";
    std::cout << "├────────────────────────────┼─────────────┼──────────────┼────────────────────────┤\n";
    std::cout << "│ btree_map                  │ 30-120ns ✅ │ 50-180ns     │ Sorted data, ranges    │\n";
    std::cout << "│ btree_set                  │ 30-120ns ✅ │ 50-180ns     │ Sorted unique elements │\n";
    std::cout << "├────────────────────────────┼─────────────┼──────────────┼────────────────────────┤\n";
    std::cout << "│ SEQUENTIAL CONTAINERS                                                            │\n";
    std::cout << "├────────────────────────────┼─────────────┼──────────────┼────────────────────────┤\n";
    std::cout << "│ InlinedVector<T, N>        │ Array-like  │ 35-90ns ✅   │ Small vectors, SSO     │\n";
    std::cout << "│ FixedArray<T>              │ Array-like  │ 40-100ns ✅  │ Runtime size, no grow  │\n";
    std::cout << "└────────────────────────────┴─────────────┴──────────────┴────────────────────────┘\n";

    std::cout << "\n┌─────────────────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ COMPARISON WITH STL                                                     │\n";
    std::cout << "├─────────────────────────────────────────────────────────────────────────┤\n";
    std::cout << "│ absl::flat_hash_map  vs  std::unordered_map                             │\n";
    std::cout << "│   • 2-3x faster lookups (15-60ns vs 30-100ns)                           │\n";
    std::cout << "│   • Better cache locality (Swiss table)                                 │\n";
    std::cout << "│   • SIMD-optimized probing                                              │\n";
    std::cout << "│                                                                         │\n";
    std::cout << "│ absl::btree_map  vs  std::map                                           │\n";
    std::cout << "│   • 2-3x faster lookups (30-120ns vs 50-200ns)                          │\n";
    std::cout << "│   • Better cache locality (B-tree vs red-black tree)                    │\n";
    std::cout << "│   • Lower memory overhead                                               │\n";
    std::cout << "│                                                                         │\n";
    std::cout << "│ absl::InlinedVector  vs  std::vector                                    │\n";
    std::cout << "│   • Zero heap allocation for small sizes (≤N)                           │\n";
    std::cout << "│   • 35-90ns vs 100-200ns for small vectors                              │\n";
    std::cout << "│   • Same performance for large vectors                                  │\n";
    std::cout << "└─────────────────────────────────────────────────────────────────────────┘\n";
}

//=============================================================================
// 6. BEST PRACTICES AND RECOMMENDATIONS
//=============================================================================

void print_best_practices() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ABSEIL CONTAINERS - BEST PRACTICES FOR HFT               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "🎯 CRITICAL PATH (<500ns)\n";
    std::cout << "────────────────────────────────────────────────────────────\n\n";

    std::cout << "1. Symbol → Price Lookups:\n";
    std::cout << "   ✅ absl::flat_hash_map<SymbolID, Price>\n";
    std::cout << "   • 15-60ns lookup (best performance)\n";
    std::cout << "   • Always call reserve() upfront\n\n";

    std::cout << "2. Order Book Price Levels:\n";
    std::cout << "   ✅ absl::btree_map<Price, OrderQueue>\n";
    std::cout << "   • 30-120ns lookup\n";
    std::cout << "   • Fast best bid/ask (rbegin/begin)\n";
    std::cout << "   • Efficient range queries\n\n";

    std::cout << "3. Active Orders by ID:\n";
    std::cout << "   ✅ absl::node_hash_map<OrderID, Order>\n";
    std::cout << "   • Stable pointers (safe to store iterators)\n";
    std::cout << "   • 20-80ns lookup\n\n";

    std::cout << "4. Small Temporary Buffers:\n";
    std::cout << "   ✅ absl::InlinedVector<Order, 16>\n";
    std::cout << "   • ZERO heap for ≤16 elements\n";
    std::cout << "   • 35-90ns creation time\n\n";

    std::cout << "5. Fixed-Size Buffers:\n";
    std::cout << "   ✅ absl::FixedArray<Trade>\n";
    std::cout << "   • Runtime size, single allocation\n";
    std::cout << "   • Stack for small, heap for large\n\n";

    std::cout << "⚠️  COMMON MISTAKES TO AVOID\n";
    std::cout << "────────────────────────────────────────────────────────────\n\n";

    std::cout << "❌ NOT calling reserve() on hash containers\n";
    std::cout << "   → Rehashing is expensive (can take microseconds)\n";
    std::cout << "   ✅ Always: map.reserve(expected_size);\n\n";

    std::cout << "❌ Using flat_hash_map when you need stable pointers\n";
    std::cout << "   → Rehashing invalidates all iterators/pointers\n";
    std::cout << "   ✅ Use node_hash_map if you store iterators\n\n";

    std::cout << "❌ Using std::map when you don't need ordering\n";
    std::cout << "   → 2-3x slower than flat_hash_map\n";
    std::cout << "   ✅ Use flat_hash_map for unordered, btree_map for ordered\n\n";

    std::cout << "❌ Using std::vector for small temporary buffers\n";
    std::cout << "   → Heap allocation every time\n";
    std::cout << "   ✅ Use InlinedVector<T, N> for frequently created small vectors\n\n";

    std::cout << "💡 PERFORMANCE TIPS\n";
    std::cout << "────────────────────────────────────────────────────────────\n\n";

    std::cout << "1. Pre-allocate at startup:\n";
    std::cout << "   market_data_cache.reserve(100000);  // All symbols\n\n";

    std::cout << "2. Use node_hash_map sparingly:\n";
    std::cout << "   • Only when pointer stability is required\n";
    std::cout << "   • Slightly slower than flat_hash_map\n\n";

    std::cout << "3. Choose InlinedVector size wisely:\n";
    std::cout << "   • Profile to find typical sizes\n";
    std::cout << "   • InlinedVector<Order, 8> if usually ≤8 orders\n";
    std::cout << "   • Don't make N too large (wasted stack space)\n\n";

    std::cout << "4. Prefer btree over flat for large sorted data:\n";
    std::cout << "   • flat_map insert is O(n) - slow for large maps\n";
    std::cout << "   • btree_map insert is O(log n)\n\n";

    std::cout << "5. Compile with optimizations:\n";
    std::cout << "   g++ -O3 -march=native -DNDEBUG\n";
    std::cout << "   • Enables SIMD optimizations in Swiss tables\n";
}

//=============================================================================
// 7. ADVANCED TRADING USE CASES — SOR, DARK POOL, ETF ARB, INDEX ARB,
//    MARKET MAKING, PRINCIPAL INTERNALIZER, RISK
//=============================================================================

void advanced_trading_use_cases() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ADVANCED CAPITAL MARKETS USE CASES                        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    // ── UC1: Smart Order Router (SOR) ────────────────────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC1: Smart Order Router (SOR) — Venue Routing Table\n";
        std::cout << "  Container: flat_hash_map<uint32_t, VenueState>\n";
        std::cout << "  Use: Given order qty+side, find cheapest venue with liquidity\n";
        std::cout << "  Why flat_hash_map: O(1) venue lookup by venue_id, 15-60ns\n\n";

        // Pre-size at startup — NO rehashing in hot path
        absl::flat_hash_map<uint32_t, VenueState> sor_table;
        sor_table.reserve(64); // max 64 venues

        // Startup: populate venues
        const char* venues[] = {"NYSE   ", "NASDAQ ", "ARCA   ", "BATS   ",
                                 "CBOE   ", "IEX    ", "LTSE   ", "MEMX   "};
        for (uint32_t i = 0; i < 8; ++i) {
            VenueState vs{};
            vs.venue_id      = i;
            vs.avail_qty     = 10000;
            vs.best_bid_fp   = to_fp(99.95 - i * 0.01);
            vs.best_ask_fp   = to_fp(100.05 + i * 0.01);
            vs.fill_rate_bps = 8000 - i * 100; // 80% fill rate
            vs.latency_us    = 50 + i * 10;
            vs.active        = 1;
            std::memcpy(vs.venue_code, venues[i], 7);
            sor_table.emplace(i, vs);
        }

        // Hot path: find best venue for order (minimize price × latency)
        LatencyStats route_stats;
        for (int iter = 0; iter < 100000; ++iter) {
            auto ns = measure_latency_ns([&]() {
                uint32_t best_venue = UINT32_MAX;
                int64_t  best_ask   = INT64_MAX;
                for (const auto& [vid, vs] : sor_table) {
                    if (!vs.active || vs.avail_qty == 0) continue;
                    if (vs.best_ask_fp < best_ask) {
                        best_ask   = vs.best_ask_fp;
                        best_venue = vid;
                    }
                }
                volatile auto r = best_venue;
            });
            route_stats.add(ns);
        }
        route_stats.print("  SOR: find best venue (8 venues)");
        std::cout << "  ✅ 8-venue scan in flat_hash_map: cache-resident hot path\n\n";
    }

    // ── UC2: Dark Pool Crossing Engine ───────────────────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC2: Dark Pool Crossing Engine\n";
        std::cout << "  Container: btree_multimap<int64_t, Order> (buy + sell books)\n";
        std::cout << "  Use: Match buy orders >= ask price with sell orders\n";
        std::cout << "  Why btree_multimap: sorted by price, O(log N) crossing point\n\n";

        // Buy side: sorted DESCENDING (highest bid first) → negate price
        // Sell side: sorted ASCENDING (lowest ask first)
        absl::btree_multimap<int64_t, Order> dark_buy;   // key = -price_fp
        absl::btree_multimap<int64_t, Order> dark_sell;  // key = +price_fp

        // Populate dark pool with resting orders
        std::mt19937 rng(42);
        for (uint64_t i = 0; i < 1000; ++i) {
            double price = 99.0 + (rng() % 200) * 0.01;
            Order buy(i, price, 100, 'B');
            dark_buy.emplace(-to_fp(price), buy);

            Order sell(10000 + i, price + 0.02, 100, 'S');
            dark_sell.emplace(to_fp(price + 0.02), sell);
        }

        // Hot path: find crossing orders (buy_price >= sell_price)
        LatencyStats cross_stats;
        for (int iter = 0; iter < 10000; ++iter) {
            auto ns = measure_latency_ns([&]() {
                uint32_t crosses = 0;
                // Iterate: highest buy bid vs lowest sell ask
                auto buy_it  = dark_buy.begin();  // highest bid (negated)
                auto sell_it = dark_sell.begin();  // lowest ask
                while (buy_it != dark_buy.end() && sell_it != dark_sell.end()) {
                    int64_t bid_fp = -buy_it->first;
                    int64_t ask_fp =  sell_it->first;
                    if (bid_fp < ask_fp) break; // no cross
                    ++crosses;
                    ++buy_it; ++sell_it;
                    if (crosses >= 10) break; // max N crosses per cycle
                }
                volatile auto r = crosses;
            });
            cross_stats.add(ns);
        }
        cross_stats.print("  Dark pool: find crossing orders (1K each side)");
        std::cout << "  ✅ btree sorted by price: O(log N) to crossing point\n\n";
    }

    // ── UC3: ETF Arbitrage — Basket Lookup ───────────────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC3: ETF Arb — Basket Component Lookup\n";
        std::cout << "  Container: flat_hash_map<uint32_t, absl::Span<BasketLeg>>\n";
        std::cout << "  Use: Given ETF trade, instantly find all N component legs\n";
        std::cout << "  Why Span: zero-copy view into pre-allocated leg array\n\n";

        // Pre-allocate basket leg storage at startup (no heap in hot path)
        constexpr size_t MAX_BASKETS  = 500;
        constexpr size_t MAX_LEGS     = 50;
        // BSS: legs stored in flat array, span = view into it
        static std::array<BasketLeg, MAX_BASKETS * MAX_LEGS> leg_pool{};

        absl::flat_hash_map<uint32_t, absl::Span<BasketLeg>> etf_baskets;
        etf_baskets.reserve(MAX_BASKETS);

        // Startup: populate SPY, QQQ, IWM components
        for (uint32_t etf_id = 0; etf_id < 10; ++etf_id) {
            uint32_t n_legs = 20 + etf_id * 5; // 20-65 components
            if (n_legs > MAX_LEGS) n_legs = MAX_LEGS;
            BasketLeg* base = &leg_pool[etf_id * MAX_LEGS];
            for (uint32_t j = 0; j < n_legs; ++j) {
                base[j].symbol_id = etf_id * 100 + j;
                base[j].shares    = static_cast<int32_t>(100 - j);
                base[j].weight    = 1.0 / n_legs;
            }
            etf_baskets.emplace(etf_id,
                absl::Span<BasketLeg>(base, n_legs));
        }

        // Hot path: ETF trade arrives → get basket → compute delta hedge
        LatencyStats hedge_stats;
        for (int iter = 0; iter < 100000; ++iter) {
            auto ns = measure_latency_ns([&]() {
                auto it = etf_baskets.find(iter % 10); // find basket
                if (it == etf_baskets.end()) return;
                // Iterate legs (zero-copy span — no allocation)
                double total_delta = 0.0;
                for (const auto& leg : it->second) {
                    total_delta += leg.shares * leg.weight;
                }
                volatile double r = total_delta;
            });
            hedge_stats.add(ns);
        }
        hedge_stats.print("  ETF arb: lookup basket + sum delta (avg 35 legs)");
        std::cout << "  ✅ Span = zero-copy: no allocation in hot path\n\n";
    }

    // ── UC4: Index Arbitrage — Universe Weight Table ──────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC4: Index Arb — Index Component Weight Lookup\n";
        std::cout << "  Container: flat_hash_map<uint32_t, double> (symbol→weight)\n";
        std::cout << "  Use: Given market tick on S&P500 member, find its index weight\n";
        std::cout << "  Why flat_hash_map: 500 symbols, sub-30ns lookup needed\n\n";

        absl::flat_hash_map<uint32_t, double> index_weights;
        index_weights.reserve(512); // 512 = next power of 2 after 500 (SP500)

        for (uint32_t sym = 0; sym < 500; ++sym) {
            index_weights.emplace(sym, 1.0 / 500.0);
        }

        LatencyStats weight_stats;
        std::mt19937 rng2(999);
        for (int iter = 0; iter < 100000; ++iter) {
            uint32_t sym_id = rng2() % 500;
            auto ns = measure_latency_ns([&]() {
                auto it = index_weights.find(sym_id);
                volatile double w = (it != index_weights.end()) ? it->second : 0.0;
            });
            weight_stats.add(ns);
        }
        weight_stats.print("  Index arb: weight lookup (500-member index)");
        std::cout << "  ✅ Integer key (uint32_t): hashes in 1 cycle, no string hash\n\n";
    }

    // ── UC5: Market Making — Spread/Quote Table ───────────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC5: Market Making — Active Quote State\n";
        std::cout << "  Container: flat_hash_map<uint32_t, MMQuote>\n";
        std::cout << "  Use: Feed tick → lookup existing quote → decide to reprice\n";
        std::cout << "  Hot path: ~1M ticks/sec × N instruments\n\n";

        absl::flat_hash_map<uint32_t, MMQuote> mm_quotes;
        mm_quotes.reserve(4096); // 4K instruments

        // Startup: populate quotes for all instruments
        for (uint32_t inst = 0; inst < 1000; ++inst) {
            MMQuote q{};
            q.instrument_id = inst;
            q.bid_fp  = to_fp(99.95);
            q.ask_fp  = to_fp(100.05);
            q.bid_qty = 100;
            q.ask_qty = 100;
            q.active  = 1;
            q.spread_bps = 10;
            mm_quotes.emplace(inst, q);
        }

        // Hot path: tick arrives → update quote (typical MM hot path)
        LatencyStats mm_stats;
        for (int iter = 0; iter < 200000; ++iter) {
            uint32_t inst = iter % 1000;
            auto ns = measure_latency_ns([&]() {
                auto it = mm_quotes.find(inst);
                if (it != mm_quotes.end()) {
                    it->second.bid_fp  = to_fp(99.94 + (iter % 10) * 0.01);
                    it->second.ask_fp  = to_fp(100.06 + (iter % 10) * 0.01);
                    it->second.update_tsc = static_cast<uint64_t>(iter);
                }
            });
            mm_stats.add(ns);
        }
        mm_stats.print("  MM: hit quote state + update prices (1K instruments)");
        std::cout << "  ✅ Pre-reserved map: zero rehash in hot path\n\n";
    }

    // ── UC6: Principal Internalizer ──────────────────────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC6: Principal Internalizer — Client vs Firm Book Matching\n";
        std::cout << "  Container: node_hash_map<uint64_t, Order> (firm resting orders)\n";
        std::cout << "  Container: btree_multimap<int64_t, uint64_t> (price → order_id)\n";
        std::cout << "  Why node_hash_map: stable pointers → safe to keep references\n\n";

        // Firm's resting inventory (node_hash_map = stable references)
        absl::node_hash_map<uint64_t, Order> firm_book;
        firm_book.reserve(10000);

        // Price index for fast crossing
        absl::btree_multimap<int64_t, uint64_t> price_idx; // price_fp → order_id

        // Populate firm book with resting orders
        for (uint64_t i = 0; i < 500; ++i) {
            Order o(i, 100.0 + (i % 100) * 0.01, 500, 'S');
            auto [it, ok] = firm_book.emplace(i, o);
            if (ok) price_idx.emplace(o.price_fp, i);
        }

        // Hot path: client buy order arrives → find firm sell at best price
        LatencyStats intern_stats;
        for (int iter = 0; iter < 50000; ++iter) {
            int64_t client_bid = to_fp(100.50);
            auto ns = measure_latency_ns([&]() {
                // Find lowest firm ask <= client bid
                auto it = price_idx.begin();
                uint32_t matched = 0;
                while (it != price_idx.end() && it->first <= client_bid) {
                    auto firm_it = firm_book.find(it->second);
                    if (firm_it != firm_book.end()) ++matched;
                    ++it;
                    if (matched >= 3) break;
                }
                volatile auto r = matched;
            });
            intern_stats.add(ns);
        }
        intern_stats.print("  Internalizer: client order → find firm matches");
        std::cout << "  ✅ btree price index + node_hash_map for stable Order refs\n\n";
    }

    // ── UC7: Risk Limits Table ────────────────────────────────────────────
    {
        std::cout << "──────────────────────────────────────────────────────────\n";
        std::cout << "UC7: Pre-Trade Risk — Limits Check\n";
        std::cout << "  Container: flat_hash_map<uint32_t, RiskLimits>\n";
        std::cout << "  Use: Every order goes through risk check before routing\n";
        std::cout << "  Requirement: ADD <50ns to order path\n\n";

        absl::flat_hash_map<uint32_t, RiskLimits> risk_table;
        risk_table.reserve(256); // 256 desks

        for (uint32_t desk = 0; desk < 32; ++desk) {
            RiskLimits rl{};
            rl.max_position_fp  = to_fp(10'000'000.0); // $10M notional
            rl.max_pnl_loss_fp  = to_fp(-500'000.0);   // -$500K daily loss limit
            rl.max_order_qty    = 10000;
            rl.max_order_rate   = 1000; // 1K orders/sec
            rl.hard_kill        = 0;
            risk_table.emplace(desk, rl);
        }

        // Hot path: pre-trade risk check (called on EVERY order)
        LatencyStats risk_stats;
        for (int iter = 0; iter < 500000; ++iter) {
            uint32_t desk_id = iter % 32;
            uint32_t order_qty = 100 + (iter % 1000);
            auto ns = measure_latency_ns([&]() {
                auto it = risk_table.find(desk_id);
                if (it == risk_table.end()) return;
                const auto& lim = it->second;
                volatile bool ok = (lim.hard_kill == 0 &&
                                    order_qty <= lim.max_order_qty);
            });
            risk_stats.add(ns);
        }
        risk_stats.print("  Pre-trade risk: limit lookup + check (32 desks)");
        std::cout << "  ✅ flat_hash_map: risk check adds <30ns to order path\n";
    }
}

//=============================================================================
// 8. ABSEIL SPAN — ZERO-COPY MARKET DATA VIEWS
//=============================================================================

void demo_absl_span() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ABSL::SPAN — ZERO-COPY DATA VIEWS                         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "absl::Span<T>: non-owning view into contiguous data.\n";
    std::cout << "  • No allocation, no copy — just (ptr, length)\n";
    std::cout << "  • Use instead of (T*, size_t) pairs everywhere\n";
    std::cout << "  • Identical to std::span (C++20) but works on C++17+RHEL\n\n";

    std::cout << "Trading uses:\n";
    std::cout << "  • Span<BasketLeg>: ETF component slice into pre-alloc array\n";
    std::cout << "  • Span<Order>: view into ring buffer without copy\n";
    std::cout << "  • Span<uint8_t>: raw packet bytes from NIC — zero copy\n";
    std::cout << "  • Span<MarketData>: market data snapshot window\n\n";

    // Demo: process market data packet as Span (zero-copy from ring buffer)
    static std::array<MarketData, 256> tick_buffer{};
    for (size_t i = 0; i < 256; ++i) {
        tick_buffer[i] = MarketData(i, i % 100, 99.95, 100.05, 100, 100);
    }

    // Span = view into tick_buffer[64..128] — no copy, no allocation
    absl::Span<MarketData> window(tick_buffer.data() + 64, 64);

    LatencyStats span_stats;
    for (int iter = 0; iter < 100000; ++iter) {
        auto ns = measure_latency_ns([&]() {
            int64_t sum_bid = 0;
            for (const auto& md : window) {  // iterates pointer range - zero overhead
                sum_bid += md.bid_fp;
            }
            volatile int64_t r = sum_bid;
        });
        span_stats.add(ns);
    }
    span_stats.print("  Span<MarketData>: iterate 64 ticks (zero copy)");

    std::cout << "\n  Code pattern:\n";
    std::cout << "    // Pre-allocated pool at startup:\n";
    std::cout << "    static std::array<BasketLeg, 50000> leg_pool{};\n";
    std::cout << "    // Span = view into pool slice:\n";
    std::cout << "    absl::Span<BasketLeg> spy_legs(leg_pool.data(), 503);\n";
    std::cout << "    // Store in map: zero copy, no extra allocation:\n";
    std::cout << "    baskets.emplace(SPY_ID, spy_legs);\n";
}

//=============================================================================
// 9. CUSTOM HASH FOR ULL — INTEGER KEY BEST PRACTICES
//=============================================================================

void demo_custom_hash() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CUSTOM HASH FUNCTIONS FOR ULL                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Key insight: hash performance depends on KEY TYPE:\n";
    std::cout << "  uint32_t/uint64_t : 1 cycle (multiply + shift)\n";
    std::cout << "  double             : 1 cycle (reinterpret_cast to uint64_t)\n";
    std::cout << "  std::string        : N cycles (strlen + loop) — AVOID on hot path\n";
    std::cout << "  struct             : ~sizeof(struct)/8 cycles\n\n";

    std::cout << "HFT recommendation: ALWAYS use integer symbol IDs, never strings\n";
    std::cout << "  • Assign uint32_t IDs at startup from symbol list file\n";
    std::cout << "  • Or pack 8-char ticker into uint64_t:\n";
    std::cout << "      uint64_t pack = 0;\n";
    std::cout << "      memcpy(&pack, \"AAPL    \", 8);  // zero-padded\n\n";

    // Benchmark: integer key vs packed-string key
    constexpr size_t N = 500; // typical index universe
    absl::flat_hash_map<uint32_t, double> int_map;
    int_map.reserve(512);

    absl::flat_hash_map<uint64_t, double> packed_map;
    packed_map.reserve(512);

    // Populate both
    for (uint32_t i = 0; i < N; ++i) {
        int_map.emplace(i, 1.0 / N);
        uint64_t packed = static_cast<uint64_t>(i) * 0x100000001ULL; // fake packed
        packed_map.emplace(packed, 1.0 / N);
    }

    LatencyStats int_stats, packed_stats;
    std::mt19937 rng(123);
    for (int iter = 0; iter < 200000; ++iter) {
        uint32_t key = rng() % N;
        auto ns = measure_latency_ns([&]() {
            volatile auto it = int_map.find(key);
        });
        int_stats.add(ns);

        uint64_t pk = static_cast<uint64_t>(key) * 0x100000001ULL;
        ns = measure_latency_ns([&]() {
            volatile auto it = packed_map.find(pk);
        });
        packed_stats.add(ns);
    }

    int_stats.print("    flat_hash_map<uint32_t>: lookup");
    packed_stats.print("    flat_hash_map<uint64_t>: lookup (packed symbol)");
    std::cout << "  ✅ Both are fast. Integer key is king in HFT hot paths.\n";
}

//=============================================================================
// MAIN BENCHMARK RUNNER
//=============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║      ABSEIL CONTAINERS COMPREHENSIVE BENCHMARK             ║\n";
    std::cout << "║      Google High-Performance C++ Containers                ║\n";
    std::cout << "║      RHEL 8/9 (GCC 8+) + macOS Compatible                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    std::cout << "\nSystem Information:\n";
    std::cout << "  CPU Cores : " << std::thread::hardware_concurrency() << "\n";
    std::cout << "  Platform  : "
#ifdef __linux__
              << "Linux/RHEL\n";
#elif defined(__APPLE__)
              << "macOS (dev)\n";
#else
              << "Other\n";
#endif
    std::cout << "  Target    : Sub-microsecond latency — HFT front-office\n\n";

    std::cout << "Swiss Table SIMD internals (flat_hash_map):\n";
    std::cout << "  • 16 slots per group, ctrl bytes = 16×1B in one cache line\n";
    std::cout << "  • Lookup: H1→group, SIMD(ctrl, H2) → match mask, then key cmp\n";
    std::cout << "  • Avg 0-1 cache line reads per lookup (vs 2-3 for unordered_map)\n";
    std::cout << "  • SIMD: SSE2/NEON 16-way parallel search\n\n";

    benchmark_abseil_hash_containers();
    benchmark_abseil_btree_containers();
    benchmark_abseil_sequential_containers();
    practical_trading_examples();
    advanced_trading_use_cases();
    demo_absl_span();
    demo_custom_hash();
    print_comparison_table();
    print_best_practices();

    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Benchmark Complete! All use cases covered.                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "📚 Resources:\n";
    std::cout << "  Abseil Containers  : https://abseil.io/docs/cpp/guides/container\n";
    std::cout << "  Swiss Tables paper : https://abseil.io/about/design/swisstables\n";
    std::cout << "  B-tree design      : https://abseil.io/about/design/btree\n";
    std::cout << "  GitHub             : https://github.com/abseil/abseil-cpp\n";
    std::cout << "  RHEL build         : See file header for cmake + dnf instructions\n\n";

    std::cout << "Container selection cheat-sheet for HFT:\n";
    std::cout << "  flat_hash_map  → symbol/order/venue lookup (15-60ns, no ordering)\n";
    std::cout << "  node_hash_map  → orders where you store iterators/pointers\n";
    std::cout << "  btree_map      → price levels, dark pool book, range queries\n";
    std::cout << "  btree_multimap → multi-order at same price (dark pool, queue)\n";
    std::cout << "  InlinedVector  → orders at a price level (usually <8 orders)\n";
    std::cout << "  FixedArray     → VWAP rolling buffer, fixed-size time windows\n";
    std::cout << "  Span           → zero-copy ETF basket, raw NIC packet view\n\n";

    return 0;
}

/**
 * ═══════════════════════════════════════════════════════════════════════════
 * EXPECTED PERFORMANCE (Intel Xeon, macOS/RHEL)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Hash Containers:
 *   flat_hash_map lookup:    15-60ns   (P99: 120ns)
 *   flat_hash_set lookup:    15-60ns   (P99: 120ns)
 *   node_hash_map lookup:    20-80ns   (P99: 150ns)
 *
 * Ordered Containers:
 *   btree_map lookup:        30-120ns  (P99: 250ns)
 *   btree_set lookup:        30-120ns  (P99: 250ns)
 *
 * Sequential Containers:
 *   InlinedVector<T, N>:     35-90ns   (N elements, ZERO heap)
 *   FixedArray<T>:           40-100ns  (one allocation)
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * WHY ABSEIL FOR HFT?
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 1. Swiss Tables (flat_hash_map)
 *    • 2-3x faster than std::unordered_map
 *    • SIMD-optimized parallel probing
 *    • Excellent cache locality
 *
 * 2. B-tree Containers (btree_map)
 *    • 2-3x faster than std::map
 *    • Better cache locality than red-black trees
 *    • Lower memory overhead
 *
 * 3. Small Size Optimization (InlinedVector)
 *    • Zero heap allocation for small vectors
 *    • Perfect for temporary buffers
 *
 * 4. Production Quality
 *    • Used in Google's infrastructure
 *    • Well-tested, battle-proven
 *    • Active development and support
 *
 * ═══════════════════════════════════════════════════════════════════════════
 */

