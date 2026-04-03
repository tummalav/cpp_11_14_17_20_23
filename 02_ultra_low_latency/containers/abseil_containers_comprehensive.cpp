/**
 * Abseil Containers Comprehensive Guide and Benchmarks
 *
 * Google's Abseil C++ Library - High-Performance Containers
 * Focus: Ultra-low latency, cache-friendly, production-ready
 *
 * Installation (macOS):
 *   brew install abseil
 *
 * Installation (RHEL):
 *   # Build from source: https://github.com/abseil/abseil-cpp
 *   git clone https://github.com/abseil/abseil-cpp.git
 *   cd abseil-cpp && mkdir build && cd build
 *   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
 *   make -j$(nproc) && sudo make install
 *
 * Compilation (macOS):
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *       abseil_containers_comprehensive.cpp \
 *       -I/opt/homebrew/include -L/opt/homebrew/lib \
 *       -labsl_base -labsl_hash -labsl_raw_hash_set -labsl_strings \
 *       -labsl_synchronization -labsl_time \
 *       -lpthread -o abseil_benchmark
 *
 * Compilation (RHEL):
 *   g++ -std=c++17 -O3 -march=native -mavx2 -DNDEBUG \
 *       abseil_containers_comprehensive.cpp \
 *       -labsl_base -labsl_hash -labsl_raw_hash_set -labsl_strings \
 *       -lpthread -o abseil_benchmark
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <random>
#include <thread>
#include <atomic>
#include <cstring>

// Abseil Hash Containers
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"

// Abseil Ordered Containers (B-tree based)
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"

// Abseil Sequential Containers
#include "absl/container/inlined_vector.h"
#include "absl/container/fixed_array.h"

// Abseil Utilities
#include "absl/strings/string_view.h"
#include "absl/hash/hash.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

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
//=============================================================================

struct Order {
    uint64_t order_id;
    double price;
    uint32_t quantity;
    char side;  // 'B' or 'S'
    uint8_t padding[3];

    Order() : order_id(0), price(0.0), quantity(0), side('B') {
        std::memset(padding, 0, sizeof(padding));
    }

    Order(uint64_t id, double p, uint32_t q, char s)
        : order_id(id), price(p), quantity(q), side(s) {
        std::memset(padding, 0, sizeof(padding));
    }

    bool operator==(const Order& other) const {
        return order_id == other.order_id;
    }

    bool operator<(const Order& other) const {
        return order_id < other.order_id;
    }
};

// Hash function for Order
template <typename H>
H AbslHashValue(H h, const Order& order) {
    return H::combine(std::move(h), order.order_id);
}

struct MarketData {
    uint64_t timestamp;
    uint32_t symbol_id;
    double bid_price;
    double ask_price;
    uint32_t bid_size;
    uint32_t ask_size;

    MarketData() : timestamp(0), symbol_id(0), bid_price(0.0),
                   ask_price(0.0), bid_size(0), ask_size(0) {}

    MarketData(uint64_t ts, uint32_t sym, double bid, double ask,
               uint32_t bsize, uint32_t asize)
        : timestamp(ts), symbol_id(sym), bid_price(bid), ask_price(ask),
          bid_size(bsize), ask_size(asize) {}
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
// MAIN BENCHMARK RUNNER
//=============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║      ABSEIL CONTAINERS COMPREHENSIVE BENCHMARK             ║\n";
    std::cout << "║      Google's High-Performance C++ Containers              ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    std::cout << "\nSystem Information:\n";
    std::cout << "  CPU Cores: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "  Date: February 2026\n";
    std::cout << "  Target: Sub-microsecond latency for HFT\n";

    benchmark_abseil_hash_containers();
    benchmark_abseil_btree_containers();
    benchmark_abseil_sequential_containers();
    practical_trading_examples();
    print_comparison_table();
    print_best_practices();

    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Benchmark Complete!                                       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "📚 Resources:\n";
    std::cout << "  • Abseil Docs: https://abseil.io/docs/cpp/guides/container\n";
    std::cout << "  • Swiss Tables Paper: https://abseil.io/about/design/swisstables\n";
    std::cout << "  • GitHub: https://github.com/abseil/abseil-cpp\n\n";

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

