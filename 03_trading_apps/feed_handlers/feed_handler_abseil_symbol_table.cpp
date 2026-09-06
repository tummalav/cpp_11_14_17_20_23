/*
 * ============================================================================
 * MARKET DATA FEED HANDLER - ABSEIL SYMBOL TABLE VARIANT
 * ============================================================================
 *
 * Exchange numeric instrument IDs (ITCH `stock_locate`, CME MDP3
 * `SecurityID`) are usually dense small integers, so the fastest lookup is a
 * flat array indexed directly by ID (see `dense_id_table` below). This file
 * demonstrates that in-house baseline alongside the case where you also need
 * to translate exchange symbol *strings* (e.g. "AAPL", "6758.T") to that
 * dense internal ID once per session -- that translation layer is exactly
 * where absl::flat_hash_map earns its keep (sparse, string-keyed, built once
 * at session start / symbol-list refresh, then read to resolve incoming
 * string-keyed reference/corporate-action messages).
 *
 * Build:
 *   brew install abseil
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG feed_handler_abseil_symbol_table.cpp \
 *       -I/usr/local/include -L/usr/local/lib \
 *       -labsl_raw_hash_set -labsl_hash -labsl_hashtablez_sampler \
 *       -labsl_synchronization -labsl_stacktrace -labsl_symbolize \
 *       -labsl_debugging_internal -labsl_demangle_internal -labsl_base \
 *       -labsl_spinlock_wait -labsl_throw_delegate -labsl_raw_logging_internal \
 *       -lpthread -o feed_handler_abseil_symbol_table
 * ============================================================================
 */

#include "absl/container/flat_hash_map.h"

#include <string>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>

namespace ull::feed_handlers::abseil_variant {

struct SymbolState {
    uint64_t last_trade_price_ticks = 0;
    uint32_t last_trade_qty = 0;
    uint64_t update_count = 0;
};

// ----------------------------------------------------------------------
// In-house baseline: exchange instrument IDs are dense small integers ->
// direct array index, O(1), no hashing at all. This is what the hot decode
// path should use once the string->id translation (below) has resolved the
// numeric ID at session start.
// ----------------------------------------------------------------------
class DenseIdSymbolTable {
public:
    explicit DenseIdSymbolTable(size_t max_symbols) : states_(max_symbols) {}

    SymbolState& get(uint32_t dense_id) { return states_[dense_id]; }
    size_t size() const { return states_.size(); }

private:
    std::vector<SymbolState> states_;
};

// ----------------------------------------------------------------------
// absl::flat_hash_map<string, dense_id>: symbol-name -> dense internal ID
// translation. Built once per session / symbol-list refresh (not on the
// per-message hot path), then used to resolve any string-keyed reference
// data (e.g. corporate-action feeds, FIX security definition messages)
// down to the dense ID used by DenseIdSymbolTable above.
// ----------------------------------------------------------------------
class SymbolNameResolver {
public:
    void register_symbol(const std::string& name, uint32_t dense_id) {
        name_to_id_.emplace(name, dense_id);
    }

    // Returns UINT32_MAX if not found.
    uint32_t resolve(const std::string& name) const {
        auto it = name_to_id_.find(name);
        return it == name_to_id_.end() ? UINT32_MAX : it->second;
    }

    size_t size() const { return name_to_id_.size(); }

private:
    absl::flat_hash_map<std::string, uint32_t> name_to_id_;
};

} // namespace ull::feed_handlers::abseil_variant

// ---------------------------------------------------------------------------
// Demo / micro-benchmark
// ---------------------------------------------------------------------------
int main() {
    using namespace ull::feed_handlers::abseil_variant;
    using clock_t = std::chrono::high_resolution_clock;

    constexpr uint32_t NUM_SYMBOLS = 8192;

    SymbolNameResolver resolver;
    DenseIdSymbolTable table(NUM_SYMBOLS);

    for (uint32_t i = 0; i < NUM_SYMBOLS; ++i) {
        resolver.register_symbol("SYM" + std::to_string(i), i);
    }

    // Session-start resolution: translate string -> dense id (rare, off hot path).
    auto t0 = clock_t::now();
    std::vector<uint32_t> resolved_ids(NUM_SYMBOLS);
    for (uint32_t i = 0; i < NUM_SYMBOLS; ++i) {
        resolved_ids[i] = resolver.resolve("SYM" + std::to_string(i));
    }
    auto t1 = clock_t::now();

    // Hot decode path: dense array index per incoming market-data message.
    constexpr int MESSAGES = 5'000'000;
    std::mt19937_64 rng(3);
    std::uniform_int_distribution<uint32_t> id_dist(0, NUM_SYMBOLS - 1);
    std::uniform_int_distribution<uint64_t> price_dist(9000, 11000);

    auto t2 = clock_t::now();
    for (int i = 0; i < MESSAGES; ++i) {
        uint32_t id = resolved_ids[id_dist(rng) % NUM_SYMBOLS];
        auto& s = table.get(id);
        s.last_trade_price_ticks = price_dist(rng);
        s.last_trade_qty += 1;
        ++s.update_count;
    }
    auto t3 = clock_t::now();

    auto resolve_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / NUM_SYMBOLS;
    auto decode_ns  = std::chrono::duration<double, std::nano>(t3 - t2).count() / MESSAGES;

    std::cout << "=== Abseil Feed Handler Symbol Table ===\n";
    std::cout << "Symbols registered:            " << resolver.size() << "\n";
    std::cout << "flat_hash_map resolve (1x/sym): " << std::fixed << std::setprecision(1) << resolve_ns << " ns/op\n";
    std::cout << "dense array hot-path update:    " << decode_ns << " ns/op (" << MESSAGES << " messages)\n";
    return 0;
}
