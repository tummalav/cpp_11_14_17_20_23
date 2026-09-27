/*
 * ============================================================================
 * ULTRA LOW LATENCY RISK MANAGER - C++17
 * ============================================================================
 *
 * Design Goals:
 *  - Sub-microsecond risk check on hot path  (target: 50-200 ns end-to-end)
 *  - Zero heap allocation on hot path        (all limits pre-loaded at startup)
 *  - Lock-free reads of risk limits          (std::atomic, cache-line aligned)
 *  - Separate per-symbol and aggregate limits
 *  - Supports: position limits, loss limits, order-rate limits (token bucket),
 *              max-notional per order, max-qty per order, kill-switch
 *
 * Risk Checks (in hot-path order, fastest first):
 *  1. Global kill switch              (1 atomic load,  ~1 ns)
 *  2. Max order qty                   (1 compare,      ~2 ns)
 *  3. Max order notional              (1 multiply + compare, ~5 ns)
 *  4. Order rate limiter (token bucket)(atomic CAS,    ~10 ns)
 *  5. Per-symbol position limit       (atomic load,    ~5 ns)
 *  6. Per-symbol net loss limit       (atomic load,    ~5 ns)
 *  7. Aggregate gross position limit  (atomic load,    ~5 ns)
 *  8. Aggregate daily P&L limit       (atomic load,    ~5 ns)
 *
 * Total budget: ~40-50 ns for all checks combined.
 *
 * Thread Model:
 *  - Risk check runs on order-entry thread (single writer per symbol assumed)
 *  - Position/P&L updates also on order-entry thread after fill
 *  - Limit updates from risk management thread (rare, atomic stores)
 *  - Risk reporting runs on separate monitoring thread
 * ============================================================================
 */

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <random>
#include <thread>
#include <sstream>

namespace ull_risk {

// ============================================================================
// Constants
// ============================================================================
static constexpr uint32_t MAX_SYMBOLS          = 512;
static constexpr uint64_t PRICE_SCALE          = 1'000'000ULL;  // 6 decimal places
static constexpr int64_t  DEFAULT_MAX_QTY      = 1'000'000LL;
static constexpr int64_t  DEFAULT_MAX_NOTIONAL = 100'000'000LL; // $100M per order
static constexpr int64_t  DEFAULT_POS_LIMIT    = 10'000'000LL;  // ±10M shares
static constexpr int64_t  DEFAULT_LOSS_LIMIT   = -5'000'000LL * static_cast<int64_t>(PRICE_SCALE); // -$5M
static constexpr int64_t  DEFAULT_AGG_GROSS    = 500'000'000LL; // $500M gross
static constexpr int64_t  DEFAULT_DAILY_PNL    = -20'000'000LL * static_cast<int64_t>(PRICE_SCALE); // -$20M/day
static constexpr uint32_t TOKEN_BUCKET_RATE    = 10'000;        // orders/sec
static constexpr uint32_t TOKEN_BUCKET_BURST   = 200;           // burst capacity

// ============================================================================
// Result codes
// ============================================================================
enum class RiskResult : uint8_t {
    PASS            = 0,
    KILL_SWITCH     = 1,
    MAX_QTY         = 2,
    MAX_NOTIONAL    = 3,
    RATE_LIMIT      = 4,
    POS_LIMIT       = 5,
    LOSS_LIMIT      = 6,
    AGG_GROSS_LIMIT = 7,
    AGG_PNL_LIMIT   = 8,
    UNKNOWN_SYMBOL  = 9,
};

[[nodiscard]] inline const char* risk_result_name(RiskResult r) noexcept {
    switch (r) {
        case RiskResult::PASS:             return "PASS";
        case RiskResult::KILL_SWITCH:      return "KILL_SWITCH";
        case RiskResult::MAX_QTY:          return "MAX_QTY";
        case RiskResult::MAX_NOTIONAL:     return "MAX_NOTIONAL";
        case RiskResult::RATE_LIMIT:       return "RATE_LIMIT";
        case RiskResult::POS_LIMIT:        return "POS_LIMIT";
        case RiskResult::LOSS_LIMIT:       return "LOSS_LIMIT";
        case RiskResult::AGG_GROSS_LIMIT:  return "AGG_GROSS_LIMIT";
        case RiskResult::AGG_PNL_LIMIT:    return "AGG_PNL_LIMIT";
        case RiskResult::UNKNOWN_SYMBOL:   return "UNKNOWN_SYMBOL";
    }
    return "UNKNOWN";
}

// ============================================================================
// Fixed-point price
// ============================================================================
[[nodiscard]] inline int64_t to_fp(double p) noexcept {
    return static_cast<int64_t>(p * PRICE_SCALE + 0.5);
}
[[nodiscard]] inline double from_fp(int64_t fp) noexcept {
    return static_cast<double>(fp) / PRICE_SCALE;
}

// ============================================================================
// Order side
// ============================================================================
enum class Side : uint8_t { BUY = 0, SELL = 1 };

// ============================================================================
// Token Bucket Rate Limiter
// Lock-free, refills based on elapsed time.
// ============================================================================
struct alignas(64) TokenBucket {
    std::atomic<uint32_t>  tokens;           // current token count
    std::atomic<uint64_t>  last_refill_ns;   // last refill timestamp (ns since epoch)
    uint32_t               rate_per_sec;     // tokens added per second
    uint32_t               burst;            // maximum tokens
    char                   pad[64 - sizeof(std::atomic<uint32_t>)
                                 - sizeof(std::atomic<uint64_t>)
                                 - 2*sizeof(uint32_t)];

    void init(uint32_t rate, uint32_t burst_cap) noexcept {
        rate_per_sec   = rate;
        burst          = burst_cap;
        tokens.store(burst_cap, std::memory_order_relaxed);
        last_refill_ns.store(now_ns(), std::memory_order_relaxed);
    }

    [[nodiscard]] static uint64_t now_ns() noexcept {
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    // Returns true if a token was consumed, false if rate-limited.
    [[nodiscard]] bool try_consume() noexcept {
        refill();
        uint32_t cur = tokens.load(std::memory_order_relaxed);
        while (cur > 0) {
            if (tokens.compare_exchange_weak(cur, cur - 1,
                    std::memory_order_acquire, std::memory_order_relaxed))
                return true;
        }
        return false;
    }

private:
    void refill() noexcept {
        uint64_t now   = now_ns();
        uint64_t last  = last_refill_ns.load(std::memory_order_relaxed);
        uint64_t delta = now - last;
        if (delta < 1'000'000ULL) return;  // < 1ms, skip

        uint32_t add = static_cast<uint32_t>(delta * rate_per_sec / 1'000'000'000ULL);
        if (add == 0) return;

        if (!last_refill_ns.compare_exchange_strong(last, last + add * 1'000'000'000ULL / rate_per_sec,
                std::memory_order_relaxed, std::memory_order_relaxed))
            return;

        uint32_t cur = tokens.load(std::memory_order_relaxed);
        uint32_t new_tokens = std::min(cur + add, burst);
        tokens.store(new_tokens, std::memory_order_relaxed);
    }
};

// ============================================================================
// Per-Symbol Risk State  (one cache line)
// ============================================================================
struct alignas(64) SymbolRisk {
    // Mutable state (updated on fills)
    std::atomic<int64_t>  net_position;      // signed, in shares
    std::atomic<int64_t>  gross_position;    // always >= 0, in shares * price (notional)
    std::atomic<int64_t>  realized_pnl_fp;   // fixed-point PnL

    // Limits (rarely changed, written by risk manager thread)
    std::atomic<int64_t>  max_long_qty;      // max net long position
    std::atomic<int64_t>  max_short_qty;     // max net short (stored positive)
    std::atomic<int64_t>  max_loss_fp;       // per-symbol loss limit (negative)

    // Metadata
    char                  symbol[8];         // null-terminated symbol name
    bool                  active;            // false = symbol not initialized
    char                  pad[3];

    void init(const char* sym, int64_t max_long, int64_t max_short, int64_t max_loss) noexcept {
        net_position.store(0,         std::memory_order_relaxed);
        gross_position.store(0,       std::memory_order_relaxed);
        realized_pnl_fp.store(0,      std::memory_order_relaxed);
        max_long_qty.store(max_long,  std::memory_order_relaxed);
        max_short_qty.store(max_short,std::memory_order_relaxed);
        max_loss_fp.store(max_loss,   std::memory_order_relaxed);
        std::strncpy(symbol, sym, 7);
        symbol[7] = '\0';
        active = true;
    }

    // Called after fill to update position and PnL
    void on_fill(int64_t qty, int64_t price_fp, bool is_buy) noexcept {
        // Positive qty = buy, negative = sell
        int64_t signed_qty = is_buy ? qty : -qty;
        int64_t old_pos    = net_position.fetch_add(signed_qty, std::memory_order_relaxed);

        // Simplified PnL update: realized when reducing position
        int64_t new_pos = old_pos + signed_qty;
        if ((old_pos > 0 && !is_buy) || (old_pos < 0 && is_buy)) {
            // Reducing/flipping position: book realized PnL
            int64_t reducing = std::min(std::abs(old_pos), std::abs(qty));
            int64_t avg_cost_fp = price_fp;  // simplified: use current price
            int64_t pnl_delta   = (is_buy ? -1 : 1) * reducing * avg_cost_fp / PRICE_SCALE;
            realized_pnl_fp.fetch_add(pnl_delta, std::memory_order_relaxed);
        }

        // Update gross notional
        int64_t notional = qty * price_fp / static_cast<int64_t>(PRICE_SCALE);
        gross_position.fetch_add(notional, std::memory_order_relaxed);
        (void)new_pos;
    }
};
static_assert(sizeof(SymbolRisk)       == 64, "SymbolRisk must be 64 bytes");

// ============================================================================
// Aggregate Risk State  (one cache line)
// ============================================================================
struct alignas(64) AggregateRisk {
    std::atomic<int64_t>  gross_notional_fp;    // total absolute notional (fixed-point)
    std::atomic<int64_t>  daily_pnl_fp;         // aggregate daily P&L (fixed-point)
    std::atomic<int64_t>  order_count_today;     // total orders submitted today
    std::atomic<int64_t>  fill_count_today;

    std::atomic<int64_t>  max_gross_notional_fp; // limit
    std::atomic<int64_t>  max_daily_loss_fp;     // limit (negative)
    char                  pad[64 - 6 * sizeof(std::atomic<int64_t>)];

    void init(int64_t max_gross, int64_t max_loss) noexcept {
        gross_notional_fp.store(0,         std::memory_order_relaxed);
        daily_pnl_fp.store(0,              std::memory_order_relaxed);
        order_count_today.store(0,         std::memory_order_relaxed);
        fill_count_today.store(0,          std::memory_order_relaxed);
        max_gross_notional_fp.store(max_gross, std::memory_order_relaxed);
        max_daily_loss_fp.store(max_loss,  std::memory_order_relaxed);
    }
};
static_assert(sizeof(AggregateRisk) == 64, "AggregateRisk must be 64 bytes");

// ============================================================================
// Risk Manager
// ============================================================================
class RiskManager {
public:
    RiskManager() {
        kill_switch_.store(false, std::memory_order_relaxed);
        agg_.init(DEFAULT_AGG_GROSS * static_cast<int64_t>(PRICE_SCALE),
                  DEFAULT_DAILY_PNL);
        rate_limiter_.init(TOKEN_BUCKET_RATE, TOKEN_BUCKET_BURST);
        std::memset(symbols_.data(), 0, sizeof(SymbolRisk) * MAX_SYMBOLS);
        std::memset(symbol_index_, -1, sizeof(symbol_index_));
    }

    // ----- Symbol registration (called at startup) ------
    bool register_symbol(const char* sym,
                         int64_t max_long  = DEFAULT_POS_LIMIT,
                         int64_t max_short = DEFAULT_POS_LIMIT,
                         int64_t max_loss  = DEFAULT_LOSS_LIMIT) {
        uint32_t idx = next_symbol_slot_++;
        if (idx >= MAX_SYMBOLS) return false;
        symbols_[idx].init(sym, max_long, max_short, max_loss);
        uint32_t h = symbol_hash(sym) % HASH_SIZE;
        // Open addressing
        while (symbol_index_[h] != -1) h = (h + 1) % HASH_SIZE;
        symbol_index_[h] = static_cast<int32_t>(idx);
        std::strncpy(symbol_hash_keys_[h], sym, 7);
        return true;
    }

    // ----- Hot path: pre-order risk check ------
    // Returns RiskResult::PASS if order is acceptable.
    // ~50-100 ns total on isolated core.
    [[nodiscard]] __attribute__((hot, always_inline))
    RiskResult check_order(const char* sym, Side side,
                           int64_t qty, int64_t price_fp) noexcept {
        // 1. Kill switch (fastest check)
        if (__builtin_expect(kill_switch_.load(std::memory_order_relaxed), 0))
            return RiskResult::KILL_SWITCH;

        // 2. Max order qty
        if (__builtin_expect(qty <= 0 || qty > DEFAULT_MAX_QTY, 0))
            return RiskResult::MAX_QTY;

        // 3. Max order notional
        int64_t notional_fp = qty * price_fp / static_cast<int64_t>(PRICE_SCALE);
        if (__builtin_expect(notional_fp > DEFAULT_MAX_NOTIONAL * static_cast<int64_t>(PRICE_SCALE), 0))
            return RiskResult::MAX_NOTIONAL;

        // 4. Rate limiter
        if (__builtin_expect(!rate_limiter_.try_consume(), 0))
            return RiskResult::RATE_LIMIT;

        // 5. Symbol lookup
        SymbolRisk* sr = find_symbol(sym);
        if (__builtin_expect(sr == nullptr, 0))
            return RiskResult::UNKNOWN_SYMBOL;

        // 6. Per-symbol position limit
        int64_t cur_pos  = sr->net_position.load(std::memory_order_relaxed);
        int64_t new_pos  = cur_pos + (side == Side::BUY ? qty : -qty);
        int64_t max_long = sr->max_long_qty.load(std::memory_order_relaxed);
        int64_t max_shrt = sr->max_short_qty.load(std::memory_order_relaxed);
        if (__builtin_expect(new_pos > max_long || new_pos < -max_shrt, 0))
            return RiskResult::POS_LIMIT;

        // 7. Per-symbol loss limit
        int64_t cur_loss = sr->realized_pnl_fp.load(std::memory_order_relaxed);
        int64_t max_loss = sr->max_loss_fp.load(std::memory_order_relaxed);
        if (__builtin_expect(cur_loss < max_loss, 0))
            return RiskResult::LOSS_LIMIT;

        // 8. Aggregate gross notional
        int64_t cur_gross   = agg_.gross_notional_fp.load(std::memory_order_relaxed);
        int64_t max_gross   = agg_.max_gross_notional_fp.load(std::memory_order_relaxed);
        if (__builtin_expect(cur_gross + notional_fp > max_gross, 0))
            return RiskResult::AGG_GROSS_LIMIT;

        // 9. Aggregate daily PnL
        int64_t cur_pnl     = agg_.daily_pnl_fp.load(std::memory_order_relaxed);
        int64_t max_pnl     = agg_.max_daily_loss_fp.load(std::memory_order_relaxed);
        if (__builtin_expect(cur_pnl < max_pnl, 0))
            return RiskResult::AGG_PNL_LIMIT;

        agg_.order_count_today.fetch_add(1, std::memory_order_relaxed);
        return RiskResult::PASS;
    }

    // ----- Post-fill update (called after execution report) ------
    void on_fill(const char* sym, Side side, int64_t qty, int64_t price_fp) noexcept {
        SymbolRisk* sr = find_symbol(sym);
        if (!sr) return;
        sr->on_fill(qty, price_fp, side == Side::BUY);
        int64_t notional = qty * price_fp / static_cast<int64_t>(PRICE_SCALE);
        agg_.gross_notional_fp.fetch_add(notional, std::memory_order_relaxed);
        agg_.fill_count_today.fetch_add(1, std::memory_order_relaxed);
    }

    // ----- Kill switch ------
    void set_kill_switch(bool active) noexcept {
        kill_switch_.store(active, std::memory_order_seq_cst);
        if (active)
            std::cout << "[RISK] *** KILL SWITCH ACTIVATED ***\n";
        else
            std::cout << "[RISK] Kill switch deactivated\n";
    }
    [[nodiscard]] bool is_killed() const noexcept {
        return kill_switch_.load(std::memory_order_relaxed);
    }

    // ----- Limit updates (rare, from risk management thread) ------
    bool update_position_limit(const char* sym, int64_t max_long, int64_t max_short) noexcept {
        SymbolRisk* sr = find_symbol(sym);
        if (!sr) return false;
        sr->max_long_qty.store(max_long,   std::memory_order_relaxed);
        sr->max_short_qty.store(max_short, std::memory_order_relaxed);
        return true;
    }
    void update_daily_pnl_limit(int64_t max_loss_usd) noexcept {
        agg_.max_daily_loss_fp.store(max_loss_usd * static_cast<int64_t>(PRICE_SCALE),
                                      std::memory_order_relaxed);
    }

    // ----- Reporting ------
    void print_risk_report() const {
        std::cout << "\n===== Risk Manager Report =====\n";
        std::cout << "Kill switch:      " << (kill_switch_.load() ? "ACTIVE" : "off") << "\n";
        std::cout << "Orders today:     " << agg_.order_count_today.load() << "\n";
        std::cout << "Fills today:      " << agg_.fill_count_today.load()  << "\n";
        std::cout << "Gross notional:   $" << std::fixed << std::setprecision(2)
                  << from_fp(agg_.gross_notional_fp.load()) << "\n";
        std::cout << "Daily PnL:        $" << from_fp(agg_.daily_pnl_fp.load()) << "\n";

        std::cout << "\nPer-Symbol Positions:\n";
        std::cout << std::left
                  << std::setw(8) << "Symbol"
                  << std::setw(14) << "Net Position"
                  << std::setw(16) << "Gross Notional"
                  << std::setw(14) << "Realized PnL"
                  << "\n";
        std::cout << std::string(52, '-') << "\n";

        for (uint32_t i = 0; i < next_symbol_slot_; ++i) {
            const auto& sr = symbols_[i];
            if (!sr.active) continue;
            std::cout << std::setw(8) << sr.symbol
                      << std::setw(14) << sr.net_position.load()
                      << std::setw(16) << std::fixed << std::setprecision(2)
                      << from_fp(sr.gross_position.load())
                      << std::setw(14) << from_fp(sr.realized_pnl_fp.load())
                      << "\n";
        }
        std::cout << "================================\n";
    }

    // Counts for testing
    int reject_count(RiskResult r) const { return reject_counts_[static_cast<int>(r)]; }

private:
    // ---- Symbol hash table ----
    static constexpr uint32_t HASH_SIZE = MAX_SYMBOLS * 2;

    [[nodiscard]] static uint32_t symbol_hash(const char* sym) noexcept {
        uint64_t h = 14695981039346656037ULL;
        for (int i = 0; sym[i] != '\0' && i < 8; ++i) {
            h ^= static_cast<uint8_t>(sym[i]);
            h *= 1099511628211ULL;
        }
        return static_cast<uint32_t>(h);
    }

    [[nodiscard]] SymbolRisk* find_symbol(const char* sym) noexcept {
        uint32_t h = symbol_hash(sym) % HASH_SIZE;
        for (int attempt = 0; attempt < 8; ++attempt, h = (h + 1) % HASH_SIZE) {
            int32_t idx = symbol_index_[h];
            if (idx == -1) return nullptr;
            if (std::strncmp(symbol_hash_keys_[h], sym, 7) == 0)
                return &symbols_[idx];
        }
        return nullptr;
    }

    // State
    alignas(64) std::atomic<bool>                   kill_switch_;
    alignas(64) AggregateRisk                        agg_;
    alignas(64) TokenBucket                          rate_limiter_;
    alignas(64) std::array<SymbolRisk, MAX_SYMBOLS>  symbols_;

    // Symbol index
    int32_t  symbol_index_[HASH_SIZE];
    char     symbol_hash_keys_[HASH_SIZE][8];
    uint32_t next_symbol_slot_{0};

    // Reject counters (diagnostic, not on hot path)
    mutable int reject_counts_[10]{};
};

// ============================================================================
// Latency Benchmark
// ============================================================================
struct BenchStats {
    double min_ns, max_ns, mean_ns, p50_ns, p99_ns, p999_ns;
};

BenchStats benchmark_risk_check(RiskManager& rm, const char* sym,
                                  int iterations = 1'000'000) {
    std::vector<double> latencies;
    latencies.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        Side     side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        int64_t  qty  = 100 + (i % 50) * 10;
        int64_t  px   = to_fp(150.0 + (i % 100) * 0.01);

        auto t0 = std::chrono::steady_clock::now();
        volatile RiskResult r = rm.check_order(sym, side, qty, px);
        auto t1 = std::chrono::steady_clock::now();

        latencies.push_back(
            std::chrono::duration<double, std::nano>(t1 - t0).count()
        );
        (void)r;
    }

    std::sort(latencies.begin(), latencies.end());
    double sum = 0;
    for (auto v : latencies) sum += v;

    return {
        latencies.front(),
        latencies.back(),
        sum / iterations,
        latencies[iterations / 2],
        latencies[static_cast<size_t>(iterations * 0.99)],
        latencies[static_cast<size_t>(iterations * 0.999)],
    };
}

// ============================================================================
// Demo
// ============================================================================

void run_demo() {
    std::cout << "============================================\n";
    std::cout << "   Ultra Low Latency Risk Manager Demo\n";
    std::cout << "============================================\n";

    RiskManager rm;

    // Register symbols
    rm.register_symbol("AAPL",
        5'000'000LL,     // max long  5M shares
        5'000'000LL,     // max short 5M shares
        to_fp(-1'000'000.0)  // max loss -$1M
    );
    rm.register_symbol("GOOG",
        1'000'000LL,
        1'000'000LL,
        to_fp(-2'000'000.0)
    );
    rm.register_symbol("MSFT",
        3'000'000LL,
        3'000'000LL,
        to_fp(-500'000.0)
    );

    // ---- Test individual risk checks ----
    std::cout << "\n--- Individual Risk Checks ---\n";

    struct TestCase {
        const char* sym;
        Side        side;
        int64_t     qty;
        double      price;
        const char* description;
    };

    std::vector<TestCase> tests = {
        {"AAPL", Side::BUY,  1000, 150.50,     "Normal BUY - should PASS"},
        {"AAPL", Side::SELL, 500,  151.00,     "Normal SELL - should PASS"},
        {"AAPL", Side::BUY,  2'000'000LL, 150.50, "Exceeds max qty - MAX_QTY"},
        {"AAPL", Side::BUY,  1000, 200'000.0,  "Huge price -> notional breach"},
        {"ZZZZ", Side::BUY,  100,  50.0,       "Unknown symbol"},
    };

    for (auto& tc : tests) {
        RiskResult r = rm.check_order(tc.sym, tc.side, tc.qty, to_fp(tc.price));
        std::cout << std::setw(12) << tc.sym
                  << " " << (tc.side == Side::BUY ? "BUY " : "SELL")
                  << " qty=" << std::setw(10) << tc.qty
                  << " px=" << std::setw(8) << std::fixed << std::setprecision(2) << tc.price
                  << "  -> " << std::setw(16) << risk_result_name(r)
                  << "  [" << tc.description << "]\n";
    }

    // ---- Simulate order flow ----
    std::cout << "\n--- Simulating 10,000 orders ---\n";
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> qty_dist(100, 5000);
    std::uniform_real_distribution<double> px_dist(149.0, 152.0);
    std::array<const char*, 3> syms = {"AAPL", "GOOG", "MSFT"};

    int pass = 0, fail = 0;
    for (int i = 0; i < 10'000; ++i) {
        const char* sym  = syms[i % 3];
        Side        side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        int64_t     qty  = qty_dist(rng);
        int64_t     px   = to_fp(px_dist(rng));

        RiskResult r = rm.check_order(sym, side, qty, px);
        if (r == RiskResult::PASS) {
            rm.on_fill(sym, side, qty, px);
            ++pass;
        } else {
            ++fail;
        }
    }
    std::cout << "Passed: " << pass << "  Failed: " << fail << "\n";

    // ---- Kill switch test ----
    std::cout << "\n--- Kill Switch Test ---\n";
    rm.set_kill_switch(true);
    RiskResult r = rm.check_order("AAPL", Side::BUY, 100, to_fp(150.0));
    std::cout << "Order after kill switch: " << risk_result_name(r) << "\n";
    rm.set_kill_switch(false);

    // ---- Risk report ----
    rm.print_risk_report();

    // ---- Latency benchmark ----
    std::cout << "\n--- Risk Check Latency Benchmark (1M iterations) ---\n";
    auto stats = benchmark_risk_check(rm, "AAPL", 500'000);
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  min   : " << stats.min_ns   << " ns\n";
    std::cout << "  mean  : " << stats.mean_ns  << " ns\n";
    std::cout << "  p50   : " << stats.p50_ns   << " ns\n";
    std::cout << "  p99   : " << stats.p99_ns   << " ns\n";
    std::cout << "  p99.9 : " << stats.p999_ns  << " ns\n";
    std::cout << "  max   : " << stats.max_ns   << " ns\n";
}

}  // namespace ull_risk

int main() {
    ull_risk::run_demo();
    return 0;
}

