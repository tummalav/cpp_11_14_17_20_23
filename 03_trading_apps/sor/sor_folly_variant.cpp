/*
 * ============================================================================
 * SMART ORDER ROUTER (SOR) - FOLLY CONTAINER VARIANT
 * ============================================================================
 *
 * Companion to `sor_inhouse.cpp` / `sor_abseil_variant.cpp`. Uses Folly for
 * the venue routing/config table, which is read-mostly reference data
 * (fees, latency tiers, supported order types) rebuilt wholesale on config
 * reload but read on every single routing decision:
 *
 *   folly::sorted_vector_map<venue_id, VenueConfig> -- flat, contiguous,
 *   sorted vector. O(n) insert (acceptable: bulk-loaded once at reload),
 *   but the fastest possible read/iterate of the three approaches (in-house
 *   array linear scan, Abseil btree_map, Folly sorted_vector_map) once built,
 *   because it's a single contiguous allocation with zero pointer chasing.
 *
 * Build:
 *   brew install folly
 *   g++ -std=c++20 -O3 -march=native -DNDEBUG sor_folly_variant.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
 *       -lboost_context -lboost_filesystem -lpthread -o sor_folly_variant
 * ============================================================================
 */

#include <folly/sorted_vector_types.h>

#include <string>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>

namespace ull::sor::folly_variant {

struct VenueConfig {
    std::string name;
    double   fee_bps;
    uint32_t latency_tier;
    bool     supports_ioc;
};

class VenueRoutingTable {
public:
    // Bulk-load once at startup/config-reload; O(n log n) sort amortized
    // across the whole reload, not per-lookup.
    void configure(uint32_t venue_id, VenueConfig cfg) { venues_[venue_id] = std::move(cfg); }

    const VenueConfig* get(uint32_t venue_id) const {
        auto it = venues_.find(venue_id);
        return it == venues_.end() ? nullptr : &it->second;
    }

    // Fast contiguous iteration -- no pointer chasing, cache-friendly.
    std::vector<uint32_t> priority_order() const {
        std::vector<uint32_t> out;
        out.reserve(venues_.size());
        for (auto& [id, cfg] : venues_) out.push_back(id);
        return out;
    }

    size_t size() const { return venues_.size(); }

private:
    folly::sorted_vector_map<uint32_t, VenueConfig> venues_;
};

} // namespace ull::sor::folly_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::sor::folly_variant;
    using clock_t = std::chrono::high_resolution_clock;

    VenueRoutingTable routing;
    auto t0 = clock_t::now();
    for (uint32_t v = 0; v < 20; ++v) {
        routing.configure(v, VenueConfig{"VENUE_" + std::to_string(v), 0.1 * v, (v % 3) + 1, v % 2 == 0});
    }
    auto t1 = clock_t::now();

    // Simulate a hot routing-decision loop: many reads against the same
    // read-mostly table -- this is where sorted_vector_map shines.
    constexpr int READS = 5'000'000;
    uint64_t checksum = 0;
    for (int i = 0; i < READS; ++i) {
        auto* cfg = routing.get(i % 20);
        if (cfg) checksum += cfg->latency_tier;
    }
    auto t2 = clock_t::now();

    auto build_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / 20;
    auto read_ns  = std::chrono::duration<double, std::nano>(t2 - t1).count() / READS;

    std::cout << "=== Folly SOR Variant ===\n";
    std::cout << "Routing table venues:        " << routing.size() << "\n";
    std::cout << "sorted_vector_map build avg: " << std::fixed << std::setprecision(1) << build_ns << " ns/insert\n";
    std::cout << "sorted_vector_map read avg:  " << read_ns << " ns/op (checksum=" << checksum << ")\n";

    auto order = routing.priority_order();
    std::cout << "Venue priority order (first 5): ";
    for (size_t i = 0; i < std::min<size_t>(5, order.size()); ++i) std::cout << order[i] << " ";
    std::cout << "\n";
    return 0;
}
