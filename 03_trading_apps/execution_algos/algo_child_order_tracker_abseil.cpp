/*
 * ============================================================================
 * EXECUTION ALGO - ABSEIL CHILD-ORDER TRACKER VARIANT
 * ============================================================================
 *
 * Companion to `execution_algos/algo_common.hpp`. TWAP/VWAP/POV/Participate
 * algos slice a parent order into many child orders over time and must track
 * each child's state (sent/acked/partially-filled/filled/cancelled) with
 * frequent insert (new child order sent), update (fill/ack), and erase
 * (child fully filled or cancelled). This is exactly the high-churn,
 * lookup-by-id workload absl::flat_hash_map is built for.
 *
 * Build:
 *   brew install abseil
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG algo_child_order_tracker_abseil.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -labsl_raw_hash_set -labsl_hash -labsl_hashtablez_sampler \
 *       -labsl_synchronization -labsl_stacktrace -labsl_symbolize \
 *       -labsl_debugging_internal -labsl_demangle_internal -labsl_base \
 *       -labsl_spinlock_wait -labsl_throw_delegate -labsl_raw_logging_internal \
 *       -lpthread -o algo_child_order_tracker_abseil
 * ============================================================================
 */

#include "absl/container/flat_hash_map.h"

#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>

namespace ull::algos::abseil_variant {

enum class ChildStatus : uint8_t { SENT = 0, ACKED = 1, PARTIAL = 2, FILLED = 3, CANCELLED = 4 };

struct alignas(64) ChildOrderState {
    uint64_t    child_id;
    uint32_t    target_qty;
    uint32_t    filled_qty = 0;
    ChildStatus status = ChildStatus::SENT;
    uint8_t     _pad[64 - sizeof(uint64_t) - sizeof(uint32_t) * 2 - sizeof(ChildStatus)];
};
static_assert(sizeof(ChildOrderState) == 64, "ChildOrderState must be exactly one cache line");

// ----------------------------------------------------------------------
// Generic child-order tracker reusable across TWAP/VWAP/POV/Participate
// algos: absl::flat_hash_map<child_id, ChildOrderState>.
// ----------------------------------------------------------------------
class ChildOrderTracker {
public:
    void on_child_sent(uint64_t child_id, uint32_t target_qty) {
        children_.emplace(child_id, ChildOrderState{child_id, target_qty, 0, ChildStatus::SENT, {}});
    }

    void on_ack(uint64_t child_id) {
        auto it = children_.find(child_id);
        if (it != children_.end()) it->second.status = ChildStatus::ACKED;
    }

    void on_fill(uint64_t child_id, uint32_t fill_qty) {
        auto it = children_.find(child_id);
        if (it == children_.end()) return;
        auto& c = it->second;
        c.filled_qty += fill_qty;
        c.status = (c.filled_qty >= c.target_qty) ? ChildStatus::FILLED : ChildStatus::PARTIAL;
        if (c.status == ChildStatus::FILLED) children_.erase(it);
    }

    void on_cancel(uint64_t child_id) { children_.erase(child_id); }

    const ChildOrderState* get(uint64_t child_id) const {
        auto it = children_.find(child_id);
        return it == children_.end() ? nullptr : &it->second;
    }

    // Aggregate filled quantity across all still-open children -- used by
    // VWAP/TWAP algos to compute participation rate vs. schedule.
    uint64_t total_open_filled_qty() const {
        uint64_t sum = 0;
        for (auto& [id, c] : children_) sum += c.filled_qty;
        return sum;
    }

    size_t open_children() const { return children_.size(); }

private:
    absl::flat_hash_map<uint64_t, ChildOrderState> children_;
};

} // namespace ull::algos::abseil_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark: simulate a VWAP algo slicing a parent order into
// 400 one-minute-bucket child orders, each partially filled over several
// updates before completing.
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::algos::abseil_variant;
    using clock_t = std::chrono::high_resolution_clock;

    constexpr int NUM_CHILDREN = 400;
    constexpr int FILLS_PER_CHILD = 5;

    ChildOrderTracker tracker;
    std::mt19937_64 rng(99);
    std::uniform_int_distribution<uint32_t> qty_dist(50, 200);

    auto t0 = clock_t::now();
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        tracker.on_child_sent(i, qty_dist(rng) * FILLS_PER_CHILD);
        tracker.on_ack(i);
    }
    auto t1 = clock_t::now();

    for (int round = 0; round < FILLS_PER_CHILD; ++round) {
        for (int i = 0; i < NUM_CHILDREN; ++i) {
            tracker.on_fill(i, qty_dist(rng));
        }
    }
    auto t2 = clock_t::now();

    auto insert_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / NUM_CHILDREN;
    auto fill_ns   = std::chrono::duration<double, std::nano>(t2 - t1).count() / (NUM_CHILDREN * FILLS_PER_CHILD);

    std::cout << "=== Abseil Algo Child-Order Tracker ===\n";
    std::cout << "Child orders sent+acked: " << NUM_CHILDREN
              << ", avg " << std::fixed << std::setprecision(1) << insert_ns << " ns/op\n";
    std::cout << "Fill updates:             " << (NUM_CHILDREN * FILLS_PER_CHILD)
              << ", avg " << fill_ns << " ns/op\n";
    std::cout << "Children still open:      " << tracker.open_children() << "\n";
    std::cout << "Total open filled qty:    " << tracker.total_open_filled_qty() << "\n";
    return 0;
}
