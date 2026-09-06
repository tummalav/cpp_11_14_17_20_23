/*
 * ============================================================================
 * SMART ORDER ROUTER (SOR) - ABSEIL CONTAINER VARIANT
 * ============================================================================
 *
 * Companion to `sor_inhouse.cpp`. Uses Abseil where the SOR's key-space is
 * NOT small/dense (unlike the fixed ~20-32 venue table, which stays in-house):
 *
 *  1. Child-order -> parent-order tracking (fan-out/fan-in). High churn:
 *     child orders are created and erased continuously as the SOR slices a
 *     parent order across venues. absl::flat_hash_map gives fast insert/
 *     erase/lookup (Swiss Tables, SIMD probing) without per-node allocation.
 *
 *  2. Venue routing/config table (fees, latency tier, supported order types).
 *     Read-mostly, rarely written (config reload). absl::btree_map keyed by
 *     venue name/id gives ordered iteration (e.g. iterate venues by priority
 *     tier) with better cache locality than std::map's red-black tree.
 *
 * Build:
 *   brew install abseil
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG sor_abseil_variant.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -labsl_raw_hash_set -labsl_hash -labsl_hashtablez_sampler \
 *       -labsl_synchronization -labsl_stacktrace -labsl_symbolize \
 *       -labsl_debugging_internal -labsl_demangle_internal -labsl_base \
 *       -labsl_spinlock_wait -labsl_throw_delegate -labsl_raw_logging_internal \
 *       -lpthread -o sor_abseil_variant
 * ============================================================================
 */

#include "absl/container/flat_hash_map.h"
#include "absl/container/btree_map.h"

#include <string>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>

namespace ull::sor::abseil_variant {

struct ParentOrder {
    uint64_t parent_id;
    uint32_t total_qty;
    uint32_t filled_qty = 0;
};

struct ChildOrderState {
    uint64_t child_id;
    uint64_t parent_id;
    uint32_t venue_id;
    uint32_t qty;
    bool     acked = false;
};

struct VenueConfig {
    std::string name;
    double   fee_bps;
    uint32_t latency_tier;   // 1 = fastest / co-lo, higher = slower
    bool     supports_ioc;
};

// ----------------------------------------------------------------------
// 1. Child-order tracking: absl::flat_hash_map<child_id, ChildOrderState>
//    High-churn insert/erase as the router slices/fills across venues.
// ----------------------------------------------------------------------
class ChildOrderTracker {
public:
    void on_child_sent(uint64_t child_id, uint64_t parent_id, uint32_t venue_id, uint32_t qty) {
        children_.emplace(child_id, ChildOrderState{child_id, parent_id, venue_id, qty, false});
    }

    void on_ack(uint64_t child_id) {
        auto it = children_.find(child_id);
        if (it != children_.end()) it->second.acked = true;
    }

    void on_fill_complete(uint64_t child_id) { children_.erase(child_id); }

    const ChildOrderState* get(uint64_t child_id) const {
        auto it = children_.find(child_id);
        return it == children_.end() ? nullptr : &it->second;
    }

    size_t active_children() const { return children_.size(); }

private:
    absl::flat_hash_map<uint64_t, ChildOrderState> children_;
};

// ----------------------------------------------------------------------
// 2. Venue routing table: absl::btree_map<venue_id, VenueConfig>
//    Read-mostly reference data; ordered iteration lets the router walk
//    venues by latency tier (best co-lo venues first) without a separate sort.
// ----------------------------------------------------------------------
class VenueRoutingTable {
public:
    void configure(uint32_t venue_id, VenueConfig cfg) { venues_[venue_id] = std::move(cfg); }

    const VenueConfig* get(uint32_t venue_id) const {
        auto it = venues_.find(venue_id);
        return it == venues_.end() ? nullptr : &it->second;
    }

    // Ordered by venue_id ascending -- in practice venue_id would be assigned
    // so lower ids = higher routing priority (fastest/cheapest venues first).
    std::vector<uint32_t> priority_order() const {
        std::vector<uint32_t> out;
        out.reserve(venues_.size());
        for (auto& [id, cfg] : venues_) out.push_back(id);
        return out;
    }

    size_t size() const { return venues_.size(); }

private:
    absl::btree_map<uint32_t, VenueConfig> venues_;
};

} // namespace ull::sor::abseil_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::sor::abseil_variant;
    using clock_t = std::chrono::high_resolution_clock;

    VenueRoutingTable routing;
    for (uint32_t v = 0; v < 20; ++v) {
        routing.configure(v, VenueConfig{"VENUE_" + std::to_string(v), 0.1 * v, (v % 3) + 1, v % 2 == 0});
    }

    ChildOrderTracker tracker;
    constexpr int N = 200'000;
    std::mt19937_64 rng(11);
    std::uniform_int_distribution<uint32_t> venue_dist(0, 19);
    std::uniform_int_distribution<uint32_t> qty_dist(10, 500);

    auto t0 = clock_t::now();
    for (int i = 0; i < N; ++i) {
        tracker.on_child_sent(i, i / 5, venue_dist(rng), qty_dist(rng));
    }
    auto t1 = clock_t::now();

    for (int i = 0; i < N; i += 2) tracker.on_ack(i);
    for (int i = 0; i < N; ++i) tracker.on_fill_complete(i);
    auto t2 = clock_t::now();

    auto insert_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    auto erase_ns  = std::chrono::duration<double, std::nano>(t2 - t1).count() / N;

    std::cout << "=== Abseil SOR Variant ===\n";
    std::cout << "Routing table venues:      " << routing.size() << "\n";
    std::cout << "flat_hash_map child insert: " << std::fixed << std::setprecision(1) << insert_ns << " ns/op\n";
    std::cout << "ack+erase avg:              " << erase_ns << " ns/op\n";
    std::cout << "active children remaining:  " << tracker.active_children() << "\n";

    auto order = routing.priority_order();
    std::cout << "Venue priority order (first 5): ";
    for (size_t i = 0; i < std::min<size_t>(5, order.size()); ++i) std::cout << order[i] << " ";
    std::cout << "\n";
    return 0;
}
