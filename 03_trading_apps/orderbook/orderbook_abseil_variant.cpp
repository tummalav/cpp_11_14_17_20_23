/*
 * ============================================================================
 * ORDER BOOK - ABSEIL CONTAINER VARIANT
 * ============================================================================
 *
 * Companion implementation to `ull_orderbook.cpp` (in-house arrays/intrusive
 * lists). This variant swaps two specific hot spots for Abseil containers and
 * is intended to be benchmarked head-to-head against the in-house version:
 *
 *  1. Order-ID -> Order* lookup (cancel/replace path)
 *     in-house: direct array index (requires dense sequential internal IDs)
 *     here:     absl::flat_hash_map<uint64_t, Order*>  (Swiss Tables, SIMD
 *               probing) -- needed when IDs are sparse/exchange-assigned
 *               (e.g. FIX ClOrdID, exchange order tokens).
 *
 *  2. Sparse price ladder (e.g. an illiquid options chain with a huge strike
 *     range where a dense array would waste memory)
 *     in-house: dense array indexed by (price - base) / tick
 *     here:     absl::btree_map<int64_t, PriceLevel>  (ordered, cache-friendly
 *               B-tree -- supports O(log n) range queries for depth-of-book
 *               without allocating the full price range)
 *
 * Build:
 *   brew install abseil
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG orderbook_abseil_variant.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -labsl_raw_hash_set -labsl_hash -labsl_city -labsl_low_level_hash \
 *       -labsl_hashtablez_sampler -labsl_synchronization -labsl_stacktrace \
 *       -labsl_symbolize -labsl_debugging_internal -labsl_demangle_internal \
 *       -labsl_base -labsl_spinlock_wait -labsl_throw_delegate -labsl_raw_logging_internal \
 *       -lpthread -o orderbook_abseil_variant
 * ============================================================================
 */

#include "absl/container/flat_hash_map.h"
#include "absl/container/btree_map.h"

#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>

namespace ull::orderbook::abseil_variant {

enum class Side : uint8_t { BUY = 0, SELL = 1 };

// Hot-path order record. Kept POD-like and cache-line sized, matching the
// in-house `Order` struct so the two variants are apples-to-apples.
struct alignas(64) Order {
    uint64_t order_id;
    int64_t  price_ticks;   // fixed-point price (ticks from base)
    uint32_t qty;
    Side     side;
    uint8_t  _pad[64 - sizeof(uint64_t) - sizeof(int64_t) - sizeof(uint32_t) - sizeof(Side)];
};
static_assert(sizeof(Order) == 64, "Order must be exactly one cache line");

struct PriceLevel {
    uint64_t total_qty = 0;
    std::vector<Order*> fifo;   // time-priority queue at this price
};

// ----------------------------------------------------------------------
// 1. Order-ID lookup: absl::flat_hash_map instead of array indexing.
//    Use when internal/exchange order IDs are sparse (not dense sequential).
// ----------------------------------------------------------------------
class AbseilOrderIndex {
public:
    void add(Order* o) { index_.emplace(o->order_id, o); }

    Order* find(uint64_t order_id) const {
        auto it = index_.find(order_id);
        return it == index_.end() ? nullptr : it->second;
    }

    bool cancel(uint64_t order_id) { return index_.erase(order_id) > 0; }

    size_t size() const { return index_.size(); }

private:
    absl::flat_hash_map<uint64_t, Order*> index_;
};

// ----------------------------------------------------------------------
// 2. Sparse price ladder: absl::btree_map instead of a dense price array.
//    Use for wide/sparse price ranges (options chains, illiquid instruments)
//    where allocating a full dense array per tick would waste memory but you
//    still need ordered iteration for depth-of-book / best-price queries.
// ----------------------------------------------------------------------
class AbseilPriceLadder {
public:
    void add_order(Order* o) {
        auto& level = levels_[o->price_ticks];
        level.total_qty += o->qty;
        level.fifo.push_back(o);
    }

    // Best bid = highest price, best ask = lowest price.
    const PriceLevel* best(Side side) const {
        if (levels_.empty()) return nullptr;
        return side == Side::BUY ? &levels_.rbegin()->second
                                  : &levels_.begin()->second;
    }

    // Depth-of-book: top N levels, ordered by price priority for `side`.
    std::vector<std::pair<int64_t, uint64_t>> top_n(Side side, size_t n) const {
        std::vector<std::pair<int64_t, uint64_t>> out;
        out.reserve(n);
        if (side == Side::BUY) {
            for (auto it = levels_.rbegin(); it != levels_.rend() && out.size() < n; ++it)
                out.emplace_back(it->first, it->second.total_qty);
        } else {
            for (auto it = levels_.begin(); it != levels_.end() && out.size() < n; ++it)
                out.emplace_back(it->first, it->second.total_qty);
        }
        return out;
    }

    size_t level_count() const { return levels_.size(); }

private:
    absl::btree_map<int64_t, PriceLevel> levels_;
};

} // namespace ull::orderbook::abseil_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::orderbook::abseil_variant;
    using clock_t = std::chrono::high_resolution_clock;

    constexpr int N = 200'000;
    std::vector<Order> pool(N);

    std::mt19937_64 rng(42);
    // Sparse exchange-style order IDs (not dense/sequential).
    std::uniform_int_distribution<uint64_t> id_dist(1, 1ull << 40);
    std::uniform_int_distribution<int64_t> price_dist(-50'000, 50'000); // wide sparse strikes
    std::uniform_int_distribution<uint32_t> qty_dist(1, 1000);

    for (int i = 0; i < N; ++i) {
        pool[i] = Order{id_dist(rng), price_dist(rng), qty_dist(rng),
                         (i % 2 == 0) ? Side::BUY : Side::SELL, {}};
    }

    AbseilOrderIndex index;
    AbseilPriceLadder ladder;

    auto t0 = clock_t::now();
    for (auto& o : pool) {
        index.add(&o);
        ladder.add_order(&o);
    }
    auto t1 = clock_t::now();

    uint64_t found = 0;
    for (auto& o : pool) {
        if (index.find(o.order_id)) ++found;
    }
    auto t2 = clock_t::now();

    auto insert_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    auto lookup_ns = std::chrono::duration<double, std::nano>(t2 - t1).count() / N;

    std::cout << "=== Abseil Order Book Variant ===\n";
    std::cout << "Orders inserted:      " << N << "\n";
    std::cout << "Distinct price levels:" << ladder.level_count() << "\n";
    std::cout << "flat_hash_map insert: " << std::fixed << std::setprecision(1) << insert_ns << " ns/op (avg)\n";
    std::cout << "flat_hash_map lookup: " << lookup_ns << " ns/op (avg), found=" << found << "\n";

    auto best_bid = ladder.best(Side::BUY);
    auto best_ask = ladder.best(Side::SELL);
    std::cout << "Best bid qty: " << (best_bid ? best_bid->total_qty : 0) << "\n";
    std::cout << "Best ask qty: " << (best_ask ? best_ask->total_qty : 0) << "\n";
    return 0;
}
