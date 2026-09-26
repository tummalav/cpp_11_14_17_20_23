/*
 * ============================================================================
 * ORDER BOOK - FOLLY CONTAINER VARIANT
 * ============================================================================
 *
 * Companion implementation to `ull_orderbook.cpp` (in-house) and
 * `orderbook_abseil_variant.cpp` (Abseil). This variant uses Folly containers
 * for the same two hot spots, so all three can be benchmarked head-to-head:
 *
 *  1. Order-ID -> Order* lookup (cancel/replace path)
 *     folly::F14FastMap<uint64_t, Order*>  -- F14 hashing, typically the
 *     fastest raw hash map benchmark numbers of the three approaches, at the
 *     cost of a heavier dependency footprint (glog/gflags/fmt/double-conversion).
 *
 *  2. Reference/rarely-mutated price ladder snapshot (e.g. a bulk-loaded
 *     book snapshot used for backtesting/replay, not the live mutating book)
 *     folly::sorted_vector_map<int64_t, PriceLevel>  -- flat sorted vector,
 *     O(n) insert but the fastest possible read/iterate once built; ideal
 *     when the ladder is built once per replay tick and read many times.
 *
 * Build:
 *   brew install folly
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG orderbook_folly_variant.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
 *       -lboost_context -lboost_filesystem -lpthread -o orderbook_folly_variant
 * ============================================================================
 */

#include <folly/container/F14Map.h>
#include <folly/sorted_vector_types.h>

#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>

namespace ull::orderbook::folly_variant {

enum class Side : uint8_t { BUY = 0, SELL = 1 };

struct alignas(64) Order {
    uint64_t order_id;
    int64_t  price_ticks;
    uint32_t qty;
    Side     side;
    uint8_t  _pad[64 - sizeof(uint64_t) - sizeof(int64_t) - sizeof(uint32_t) - sizeof(Side)];
};
static_assert(sizeof(Order) == 64, "Order must be exactly one cache line");

struct PriceLevel {
    uint64_t total_qty = 0;
    std::vector<Order*> fifo;
};

// ----------------------------------------------------------------------
// 1. Order-ID lookup: folly::F14FastMap
// ----------------------------------------------------------------------
class FollyOrderIndex {
public:
    void add(Order* o) { index_.emplace(o->order_id, o); }

    Order* find(uint64_t order_id) const {
        auto it = index_.find(order_id);
        return it == index_.end() ? nullptr : it->second;
    }

    bool cancel(uint64_t order_id) { return index_.erase(order_id) > 0; }

    size_t size() const { return index_.size(); }

private:
    folly::F14FastMap<uint64_t, Order*> index_;
};

// ----------------------------------------------------------------------
// 2. Read-mostly snapshot ladder: folly::sorted_vector_map
//    Build once (bulk insert), then read/iterate many times -- ideal for
//    backtesting/replay where the book snapshot doesn't mutate per read.
// ----------------------------------------------------------------------
class FollySnapshotLadder {
public:
    void add_order(Order* o) {
        auto& level = levels_[o->price_ticks];
        level.total_qty += o->qty;
        level.fifo.push_back(o);
    }

    const PriceLevel* best(Side side) const {
        if (levels_.empty()) return nullptr;
        return side == Side::BUY ? &(--levels_.end())->second
                                  : &levels_.begin()->second;
    }

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
    folly::sorted_vector_map<int64_t, PriceLevel> levels_;
};

} // namespace ull::orderbook::folly_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::orderbook::folly_variant;
    using clock_t = std::chrono::high_resolution_clock;

    constexpr int N = 200'000;
    std::vector<Order> pool(N);

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint64_t> id_dist(1, 1ull << 40);
    std::uniform_int_distribution<int64_t> price_dist(-50'000, 50'000);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 1000);

    for (int i = 0; i < N; ++i) {
        pool[i] = Order{id_dist(rng), price_dist(rng), qty_dist(rng),
                         (i % 2 == 0) ? Side::BUY : Side::SELL, {}};
    }

    FollyOrderIndex index;
    FollySnapshotLadder ladder;

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

    std::cout << "=== Folly Order Book Variant ===\n";
    std::cout << "Orders inserted:      " << N << "\n";
    std::cout << "Distinct price levels:" << ladder.level_count() << "\n";
    std::cout << "F14FastMap insert:    " << std::fixed << std::setprecision(1) << insert_ns << " ns/op (avg)\n";
    std::cout << "F14FastMap lookup:    " << lookup_ns << " ns/op (avg), found=" << found << "\n";

    auto best_bid = ladder.best(Side::BUY);
    auto best_ask = ladder.best(Side::SELL);
    std::cout << "Best bid qty: " << (best_bid ? best_bid->total_qty : 0) << "\n";
    std::cout << "Best ask qty: " << (best_ask ? best_ask->total_qty : 0) << "\n";
    return 0;
}
