/*
 * ============================================================================
 * SMART ORDER ROUTER (SOR) - IN-HOUSE VENUE AGGREGATOR
 * ============================================================================
 *
 * Design Principles:
 *  1. N (venues) is small (tens, not millions) -> fixed-size array beats a
 *     hash map or heap for top-of-book scans; SIMD/cache-friendly linear scan.
 *  2. Zero heap allocation on the routing decision hot path.
 *  3. Cache-line aligned VenueQuote so an entire venue table fits in a
 *     handful of cache lines.
 *  4. When N grows large (>~32, e.g. 100+ dark pools), a binary min/max-heap
 *     over the same array (see `best_venue_heap()`) turns best-price
 *     extraction into O(log N) instead of O(N) -- switch strategy based on N.
 *
 * This is the "in-house" baseline; compare against sor_abseil_variant.cpp,
 * sor_folly_variant.cpp and sor_moodycamel_queue.cpp for the same subsystem
 * built with library containers/queues.
 * ============================================================================
 */

#include <array>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>
#include <limits>

namespace ull::sor::inhouse {

constexpr size_t MAX_VENUES = 32;

enum class Side : uint8_t { BUY = 0, SELL = 1 };

struct alignas(64) VenueQuote {
    int64_t  price_ticks = 0;   // 8 bytes, placed first to avoid internal padding
    uint32_t venue_id    = 0;   // 4
    uint32_t qty         = 0;   // 4
    bool     active      = false; // 1
    uint8_t  _pad[64 - sizeof(int64_t) - sizeof(uint32_t) * 2 - sizeof(bool)];
};
static_assert(sizeof(VenueQuote) == 64, "VenueQuote must be exactly one cache line");

// ----------------------------------------------------------------------
// Fixed-size array aggregator: linear scan for best price across venues.
// Fastest option while N <= ~32 (fits in 2 cache lines, no branch misprediction
// from a general heap/tree traversal).
// ----------------------------------------------------------------------
class VenueAggregator {
public:
    void update(uint32_t venue_id, int64_t price_ticks, uint32_t qty) {
        for (auto& v : quotes_) {
            if (v.active && v.venue_id == venue_id) {
                v.price_ticks = price_ticks;
                v.qty = qty;
                return;
            }
        }
        for (auto& v : quotes_) {
            if (!v.active) {
                v = VenueQuote{price_ticks, venue_id, qty, true, {}};
                ++count_;
                return;
            }
        }
        // Table full -- in production, log/alert; here we silently ignore.
    }

    // Best price across all active venues. BUY side routes to lowest ask,
    // SELL side routes to highest bid (SOR is routing an outbound order, so
    // it wants the best *available* liquidity on the opposite side).
    const VenueQuote* best_venue(Side side) const {
        const VenueQuote* best = nullptr;
        for (auto& v : quotes_) {
            if (!v.active) continue;
            if (!best) { best = &v; continue; }
            if (side == Side::BUY) {
                if (v.price_ticks < best->price_ticks) best = &v;
            } else {
                if (v.price_ticks > best->price_ticks) best = &v;
            }
        }
        return best;
    }

    // Depth-aware slicing: sorted view of all active venues, best-first.
    std::vector<VenueQuote> ranked(Side side) const {
        std::vector<VenueQuote> out;
        out.reserve(count_);
        for (auto& v : quotes_) if (v.active) out.push_back(v);
        std::sort(out.begin(), out.end(), [side](const VenueQuote& a, const VenueQuote& b) {
            return side == Side::BUY ? a.price_ticks < b.price_ticks
                                      : a.price_ticks > b.price_ticks;
        });
        return out;
    }

    size_t venue_count() const { return count_; }

private:
    std::array<VenueQuote, MAX_VENUES> quotes_{};
    size_t count_ = 0;
};

// ----------------------------------------------------------------------
// Binary min-heap variant over a flat array -- use when N grows beyond
// ~32 venues (e.g. broker aggregating 100+ liquidity pools/dark pools).
// Keeps best-price extraction O(log N) instead of the O(N) linear scan above.
// ----------------------------------------------------------------------
class VenueHeap {
public:
    explicit VenueHeap(Side side) : side_(side) {}

    void push(VenueQuote q) {
        heap_.push_back(q);
        std::push_heap(heap_.begin(), heap_.end(), Cmp{side_});
    }

    const VenueQuote* top() const { return heap_.empty() ? nullptr : &heap_.front(); }

    void pop() {
        if (heap_.empty()) return;
        std::pop_heap(heap_.begin(), heap_.end(), Cmp{side_});
        heap_.pop_back();
    }

    size_t size() const { return heap_.size(); }

private:
    // std::push_heap/pop_heap build a max-heap w.r.t. the comparator;
    // for BUY (want lowest ask) we invert the comparison to get a min-heap.
    struct Cmp {
        Side side;
        bool operator()(const VenueQuote& a, const VenueQuote& b) const {
            return side == Side::BUY ? a.price_ticks > b.price_ticks
                                      : a.price_ticks < b.price_ticks;
        }
    };

    Side side_;
    std::vector<VenueQuote> heap_;
};

} // namespace ull::sor::inhouse

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::sor::inhouse;
    using clock_t = std::chrono::high_resolution_clock;

    constexpr int VENUES = 20;
    constexpr int ITERS = 2'000'000;

    std::mt19937_64 rng(7);
    std::uniform_int_distribution<int64_t> price_dist(9'900, 10'100);
    std::uniform_int_distribution<uint32_t> qty_dist(100, 5000);

    VenueAggregator agg;
    for (int v = 0; v < VENUES; ++v) agg.update(v, price_dist(rng), qty_dist(rng));

    auto t0 = clock_t::now();
    int64_t best_sum = 0;
    for (int i = 0; i < ITERS; ++i) {
        agg.update(i % VENUES, price_dist(rng), qty_dist(rng));
        auto best = agg.best_venue(Side::BUY);
        if (best) best_sum += best->price_ticks;
    }
    auto t1 = clock_t::now();

    auto ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / ITERS;

    std::cout << "=== In-house SOR Venue Aggregator ===\n";
    std::cout << "Venues:                " << agg.venue_count() << "\n";
    std::cout << "update+best_venue avg:  " << std::fixed << std::setprecision(1) << ns << " ns/op\n";
    std::cout << "checksum (unused):      " << best_sum << "\n";

    auto ranked = agg.ranked(Side::BUY);
    std::cout << "Top-3 venues (best ask first): ";
    for (size_t i = 0; i < std::min<size_t>(3, ranked.size()); ++i)
        std::cout << "[v" << ranked[i].venue_id << "@" << ranked[i].price_ticks << "] ";
    std::cout << "\n";

    // Heap variant demo for large N.
    VenueHeap heap(Side::BUY);
    for (int v = 0; v < VENUES; ++v) heap.push(VenueQuote{price_dist(rng), (uint32_t)v, qty_dist(rng), true, {}});
    std::cout << "Heap best venue: v" << heap.top()->venue_id << " @ " << heap.top()->price_ticks << "\n";
    return 0;
}
