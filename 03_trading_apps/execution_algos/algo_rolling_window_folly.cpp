/*
 * ============================================================================
 * EXECUTION ALGO - FOLLY ROLLING WINDOW / VWAP BUCKET VARIANT
 * ============================================================================
 *
 * Two Folly containers applied to common algo data-structure needs:
 *
 *  1. folly::small_vector<Fill, N> for an order's fill history. Most child
 *     orders fill in 1-3 partial fills, but occasionally (thin liquidity)
 *     an order can rack up dozens of small fills. small_vector keeps the
 *     common case (<=N fills) entirely inline (no heap allocation) and
 *     transparently spills to the heap for the rare large case -- ideal for
 *     "usually small, occasionally large" collections on a per-order basis.
 *
 *  2. folly::sorted_vector_map<int, VwapBucket> for the VWAP volume-curve
 *     buckets. Built once per trading day (390 one-minute buckets for a
 *     typical equity session) and read on every child-order slicing
 *     decision -- a flat sorted vector is the fastest possible read/iterate
 *     structure for this bulk-loaded, read-mostly reference curve.
 *
 * Build:
 *   brew install folly
 *   g++ -std=c++20 -O3 -march=native -DNDEBUG algo_rolling_window_folly.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
 *       -lboost_context -lboost_filesystem -lpthread -o algo_rolling_window_folly
 * ============================================================================
 */

#include <folly/small_vector.h>
#include <folly/sorted_vector_types.h>

#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>

namespace ull::algos::folly_variant {

struct Fill {
    uint64_t timestamp_ns;
    uint32_t qty;
    int64_t  price_ticks;
};

struct VwapBucket {
    int      bucket_index;   // minute-of-day bucket, 0..389 for a 6.5h session
    double   target_volume_pct;
    uint64_t executed_qty = 0;
};

// ----------------------------------------------------------------------
// 1. Per-order fill history: folly::small_vector<Fill, 4> -- inline storage
//    for the common case of <=4 fills, heap-spills for outliers. Avoids
//    per-order heap allocation for the vast majority of orders.
// ----------------------------------------------------------------------
class OrderFillHistory {
public:
    void add_fill(uint64_t ts, uint32_t qty, int64_t price_ticks) {
        fills_.push_back(Fill{ts, qty, price_ticks});
    }

    uint32_t total_qty() const {
        uint32_t sum = 0;
        for (auto& f : fills_) sum += f.qty;
        return sum;
    }

    double vwap_price() const {
        if (fills_.empty()) return 0.0;
        double notional = 0.0;
        uint64_t qty = 0;
        for (auto& f : fills_) { notional += (double)f.price_ticks * f.qty; qty += f.qty; }
        return qty ? notional / qty : 0.0;
    }

    size_t fill_count() const { return fills_.size(); }
    bool is_inline() const { return fills_.size() <= 4; } // heuristic for the demo

private:
    folly::small_vector<Fill, 4> fills_;
};

// ----------------------------------------------------------------------
// 2. VWAP volume-curve buckets: folly::sorted_vector_map<bucket_index, ...>
//    Bulk-loaded once at algo start (from historical intraday volume curve),
//    read on every slicing decision throughout the day.
// ----------------------------------------------------------------------
class VwapVolumeCurve {
public:
    void load_bucket(int bucket_index, double target_volume_pct) {
        buckets_[bucket_index] = VwapBucket{bucket_index, target_volume_pct, 0};
    }

    void record_execution(int bucket_index, uint64_t qty) {
        auto it = buckets_.find(bucket_index);
        if (it != buckets_.end()) it->second.executed_qty += qty;
    }

    const VwapBucket* get(int bucket_index) const {
        auto it = buckets_.find(bucket_index);
        return it == buckets_.end() ? nullptr : &it->second;
    }

    size_t bucket_count() const { return buckets_.size(); }

private:
    folly::sorted_vector_map<int, VwapBucket> buckets_;
};

} // namespace ull::algos::folly_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::algos::folly_variant;
    using clock_t = std::chrono::high_resolution_clock;

    // 1. Fill history: simulate 100,000 orders, most with 1-4 fills, a few
    //    (thin liquidity) with up to 30 fills.
    constexpr int NUM_ORDERS = 100'000;
    std::mt19937_64 rng(5);
    std::uniform_int_distribution<int> fill_count_dist(1, 100); // skewed below
    std::uniform_int_distribution<uint32_t> qty_dist(10, 500);
    std::uniform_int_distribution<int64_t> price_dist(9900, 10100);

    std::vector<OrderFillHistory> orders(NUM_ORDERS);
    auto t0 = clock_t::now();
    size_t heap_spilled = 0;
    for (auto& o : orders) {
        int roll = fill_count_dist(rng);
        int nfills = (roll <= 90) ? (roll % 4) + 1 : (roll % 30) + 5; // ~10% thin-liquidity outliers
        for (int f = 0; f < nfills; ++f) o.add_fill(f, qty_dist(rng), price_dist(rng));
        if (!o.is_inline()) ++heap_spilled;
    }
    auto t1 = clock_t::now();
    auto fill_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / NUM_ORDERS;

    std::cout << "=== Folly Algo Rolling Window (small_vector fill history) ===\n";
    std::cout << "Orders simulated:       " << NUM_ORDERS << "\n";
    std::cout << "Orders spilled to heap: " << heap_spilled << " ("
              << std::fixed << std::setprecision(1) << (100.0 * heap_spilled / NUM_ORDERS) << "%)\n";
    std::cout << "add_fill avg:           " << fill_ns << " ns/op\n";

    // 2. VWAP volume curve: 390 one-minute buckets, U-shaped intraday curve.
    VwapVolumeCurve curve;
    for (int b = 0; b < 390; ++b) {
        double pct = (b < 30 || b > 360) ? 0.6 : 0.15; // heavier at open/close
        curve.load_bucket(b, pct);
    }
    for (int b = 0; b < 390; ++b) curve.record_execution(b, 1000 + (b % 7) * 50);

    std::cout << "\n=== Folly VWAP Volume Curve (sorted_vector_map buckets) ===\n";
    std::cout << "Buckets loaded: " << curve.bucket_count() << "\n";
    auto* b0 = curve.get(0);
    auto* bMid = curve.get(195);
    std::cout << "Bucket 0 (open) target%=" << (b0 ? b0->target_volume_pct : 0)
              << " executed=" << (b0 ? b0->executed_qty : 0) << "\n";
    std::cout << "Bucket 195 (midday) target%=" << (bMid ? bMid->target_volume_pct : 0)
              << " executed=" << (bMid ? bMid->executed_qty : 0) << "\n";
    return 0;
}
