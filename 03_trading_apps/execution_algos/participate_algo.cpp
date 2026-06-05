/**
 * participate_algo.cpp — Participate / POV (Percentage of Volume) Algorithm
 *                        Ultra-Low Latency Implementation
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CONCEPT
 * ═══════════════════════════════════════════════════════════════════════════
 * The Participate algo (also called POV — Percentage Of Volume) tracks
 * real-time market volume and sends child orders so the algo's cumulative
 * filled quantity stays at a constant fraction of total market turnover.
 *
 * Unlike VWAP (pre-scheduled) or TWAP (time-uniform), POV is purely REACTIVE:
 *   • In a fast, high-volume market → sends more
 *   • In a slow, illiquid period    → sends less or nothing
 *   • No historical volume profile required
 *
 * Best for: illiquid or newly-listed instruments, risk-reduction trades
 * where predictable price impact is more important than finishing by a
 * specific time, or when volume data quality is poor.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ALGO PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  symbol              8-char instrument ID.
 *  side                Buy / Sell.
 *  total_qty           Parent order size.
 *  start_time_ns       Window start.
 *  end_time_ns         Hard deadline. If reached: remainder optionally swept.
 *  target_pov_bp       Target participation rate in basis points.
 *                      e.g., 1000 bp = 10%. Algo aims to fill 10% of
 *                      all market trades in the window.
 *  min_pov_bp          Floor participation (don't go below even in quiet mkt).
 *  max_pov_bp          Ceiling participation (cap to avoid price impact).
 *  check_interval_ns   How often to re-evaluate and potentially send.
 *                      Typical: 1–5 seconds. Shorter = more responsive.
 *  volume_window_ns    Rolling window for market volume measurement.
 *                      Typical: 30–60 seconds.
 *  min_order_size      Minimum child order. Accumulate deficit if below this.
 *  max_order_size      Hard clip per child order.
 *  limit_price         Worst price (0 = no limit).
 *  urgency             0=passive, 1=neutral, 2=aggressive.
 *  self_fill_dedupe    Subtract own fills from market vol denominator.
 *                      Prevents the algo from thinking it is "behind" its
 *                      own fills. Critical for accurate participation tracking.
 *  allow_end_sweep     Aggressive market sweep at end_time if qty remains.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ULL DESIGN
 * ═══════════════════════════════════════════════════════════════════════════
 *  Hot path (on_trade / on_quote — called per tick):
 *    ✓ on_trade: single RollingVolBuf::push() — no alloc, no branch
 *    ✓ on_quote: two atomic stores (relaxed) — ~2 cycles
 *    ✓ RollingVolBuf<65536>: power-of-2 capacity, bitmask index, no deque
 *
 *  Semi-hot path (on_timer — called every check_interval_ns):
 *    ✓ RollingVolBuf::query() — evicts stale entries, returns integer sum
 *    ✓ Participation deficit computed in integer basis-points (no float div)
 *      deficit_qty = (mkt_vol_window × target_pov_bp / 10000) - filled_in_window
 *    ✓ OrderPool<64>::alloc() — O(N) scan of N=64 cache-local orders
 *    ✓ RDTSC timestamp on submit
 *
 *  fill_in_window estimation:
 *    Uses a second RollingVolBuf<65536> tracking algo's own fills, so the
 *    participation rate is computed over the same time window as market vol.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * BUILD
 * ═══════════════════════════════════════════════════════════════════════════
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *      participate_algo.cpp -lpthread -o participate_algo
 */

#include "algo_common.hpp"
#include <vector>  // MockRouter only
#include <cstdio>

// ════════════════════════════════════════════════════════════════════════════
// PARTICIPATE PARAMETERS
// ════════════════════════════════════════════════════════════════════════════

struct ParticipateParams {
    char     symbol[8]         = {'S','P','Y',0,0,0,0,0};
    Side     side              = Side::Buy;
    Qty      total_qty         = 20'000;
    NsTime   start_time_ns     = hhmm_ns(9, 30);
    NsTime   end_time_ns       = hhmm_ns(15, 30);

    uint32_t target_pov_bp     = 1000;   // 10%  target participation
    uint32_t min_pov_bp        =  500;   //  5%  floor
    uint32_t max_pov_bp        = 2500;   // 25%  ceiling

    NsTime   check_interval_ns = 5'000'000'000ULL;   // evaluate every 5s
    NsTime   volume_window_ns  = 60'000'000'000ULL;  // rolling 60s window

    Qty      min_order_size    = 100;
    Qty      max_order_size    = 5'000;
    Price    limit_price       = 0;
    uint8_t  urgency           = 1;
    bool     self_fill_dedupe  = true;
    bool     allow_end_sweep   = true;

    COLD_PATH bool validate() const noexcept {
        return total_qty > 0
            && start_time_ns < end_time_ns
            && target_pov_bp > 0 && target_pov_bp <= 10000
            && min_pov_bp <= target_pov_bp
            && target_pov_bp <= max_pov_bp
            && check_interval_ns > 0
            && urgency <= 2;
    }
};

// ════════════════════════════════════════════════════════════════════════════
// PARTICIPATE ALGO
// ════════════════════════════════════════════════════════════════════════════

class ParticipateAlgo {
    // Rolling volume buffers — 65536 entries each (power-of-2 for bitmask index)
    // At 10K trades/sec with a 60s window → max 600K entries; 64K is sufficient
    // for typical liquid US equity. For ultra-high-frequency instruments, increase.
    using VolBuf = RollingVolBuf<65536>;

public:
    explicit ParticipateAlgo(const ParticipateParams& p, IOrderRouter& router) noexcept
        : params_(p), router_(router),
          mkt_vol_buf_(p.volume_window_ns),
          algo_fill_buf_(p.volume_window_ns)
    {
        assert(p.validate());
        metrics_.reset();
    }

    // ── Lifecycle (cold path) ────────────────────────────────────────────────

    COLD_PATH void start() noexcept {
        if (state_.load(std::memory_order_relaxed) != AlgoState::Idle) return;
        last_check_ns_ = params_.start_time_ns;
        state_.store(AlgoState::Active, std::memory_order_release);
        ALGO_LOG(log_,
            "[POV][%.8s] STARTED  qty=%llu  target=%ubp  window=%llus",
            params_.symbol,
            (unsigned long long)params_.total_qty,
            params_.target_pov_bp,
            (unsigned long long)(params_.volume_window_ns / 1'000'000'000ULL));
    }

    COLD_PATH void pause() noexcept {
        AlgoState exp = AlgoState::Active;
        if (state_.compare_exchange_strong(exp, AlgoState::Paused,
                std::memory_order_acq_rel)) {
            cancel_all_live_();
            ALGO_LOG(log_, "[POV][%.8s] PAUSED", params_.symbol);
        }
    }

    COLD_PATH void resume() noexcept {
        AlgoState exp = AlgoState::Paused;
        if (state_.compare_exchange_strong(exp, AlgoState::Active,
                std::memory_order_acq_rel))
            ALGO_LOG(log_, "[POV][%.8s] RESUMED", params_.symbol);
    }

    COLD_PATH void cancel() noexcept {
        state_.store(AlgoState::Cancelled, std::memory_order_release);
        cancel_all_live_();
        ALGO_LOG(log_, "[POV][%.8s] CANCELLED", params_.symbol);
        log_.flush();
    }

    // ── Market data hot path ─────────────────────────────────────────────────

    // Called per public trade print — budget: <200ns
    FORCE_INLINE HOT_PATH void on_trade(const MarketTrade& t) noexcept {
        if (__builtin_expect(
                state_.load(std::memory_order_relaxed) != AlgoState::Active, 0))
            return;
        metrics_.ticks_processed.fetch_add(1, std::memory_order_relaxed);
        mkt_vol_buf_.push(t.timestamp_ns, t.qty);  // zero-alloc push
    }

    // Called per quote update — budget: <50ns
    FORCE_INLINE HOT_PATH void on_quote(const Quote& q) noexcept {
        last_bid_.store(q.bid_px, std::memory_order_relaxed);
        last_ask_.store(q.ask_px, std::memory_order_relaxed);
    }

    // ── Timer semi-hot path — called every ~check_interval_ns ───────────────

    FORCE_INLINE HOT_PATH void on_timer(NsTime now_ns) noexcept {
        if (__builtin_expect(
                state_.load(std::memory_order_relaxed) != AlgoState::Active, 0))
            return;
        if (__builtin_expect(now_ns >= params_.end_time_ns, 0)) {
            on_deadline_(now_ns);
            return;
        }

        // Rate-gate: don't evaluate more often than check_interval_ns
        if (__builtin_expect(now_ns - last_check_ns_ < params_.check_interval_ns, 1))
            return;
        last_check_ns_ = now_ns;

        // Skip if a live order is still working (avoid double-stacking)
        if (orders_.live_count() > 0) return;

        evaluate_and_send_(now_ns);
    }

    // ── Fill report hot path ─────────────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_fill(const FillReport& f) noexcept {
        metrics_.fills_count .fetch_add(1,          std::memory_order_relaxed);
        metrics_.filled_qty  .fetch_add(f.fill_qty, std::memory_order_relaxed);
        metrics_.filled_value.fetch_add(
            static_cast<uint64_t>(f.fill_price) * f.fill_qty,
            std::memory_order_relaxed);

        ChildOrder* o = orders_.find(f.order_id);
        if (__builtin_expect(o != nullptr, 1)) {
            if (f.fill_tsc > o->submit_tsc)
                metrics_.sum_latency_tsc.fetch_add(
                    f.fill_tsc - o->submit_tsc, std::memory_order_relaxed);
            o->filled_qty += f.fill_qty;
            if (o->filled_qty >= o->qty) o->live = false;
        }

        // Track algo fills in rolling window (for POV denominator correction)
        algo_fill_buf_.push(f.fill_time_ns, f.fill_qty);

        // Self-trade dedup: remove our fill from market vol window
        // so we don't count ourselves in the market vol denominator
        if (params_.self_fill_dedupe)
            mkt_vol_buf_.subtract(f.fill_qty);

        ALGO_LOG(log_,
            "[POV][%.8s] FILL  qty=%llu  px=%.4f  cum=%llu/%llu",
            params_.symbol,
            (unsigned long long)f.fill_qty,
            px_to_dbl(f.fill_price),
            (unsigned long long)metrics_.filled_qty.load(std::memory_order_relaxed),
            (unsigned long long)params_.total_qty);

        if (metrics_.filled_qty.load(std::memory_order_relaxed) >= params_.total_qty)
            finish_();
    }

    // ── Reporting (cold path) ────────────────────────────────────────────────

    COLD_PATH void print_summary() const noexcept {
        log_.flush();
        std::printf(
            "\n╔══════════════════════════════════════════════════════╗\n"
            "║  PARTICIPATE / POV ALGO SUMMARY                      ║\n"
            "╚══════════════════════════════════════════════════════╝\n");
        std::printf("  Symbol    : %.8s\n"
                    "  Side      : %s\n"
                    "  Target    : %llu\n"
                    "  State     : %s\n",
            params_.symbol, side_str(params_.side),
            (unsigned long long)params_.total_qty,
            state_str(state_.load(std::memory_order_relaxed)));
        std::printf("  Target POV: %u bp (%.1f%%)\n",
            params_.target_pov_bp, params_.target_pov_bp / 100.0);
        metrics_.print("POV", g_ns_per_tick);

        const Qty filled = metrics_.filled_qty.load(std::memory_order_relaxed);
        if (total_mkt_vol_seen_ > 0) {
            std::printf("  Market vol seen  : %llu\n",
                (unsigned long long)total_mkt_vol_seen_);
            std::printf("  Actual POV       : %.2f%% (target=%.1f%%)\n",
                100.0 * filled / total_mkt_vol_seen_,
                params_.target_pov_bp / 100.0);
        }
        std::putchar('\n');
    }

    COLD_PATH void flush_log() noexcept { log_.flush(); }
    AlgoState state() const noexcept { return state_.load(std::memory_order_acquire); }

private:
    // ── Core participation evaluation — runs at check_interval cadence ───────
    FORCE_INLINE void evaluate_and_send_(NsTime now_ns) noexcept {
        const Qty cum = metrics_.filled_qty.load(std::memory_order_relaxed);
        const Qty rem = (params_.total_qty > cum) ? params_.total_qty - cum : 0;
        if (__builtin_expect(rem == 0, 0)) { finish_(); return; }

        // Total market volume in the rolling window (evicts stale)
        const Qty mkt_vol = mkt_vol_buf_.query(now_ns);
        total_mkt_vol_seen_ += mkt_vol;  // accumulate for summary (cold only)

        if (__builtin_expect(mkt_vol == 0, 0)) {
            ALGO_LOG(log_, "[POV][%.8s] SKIP: no market vol in window", params_.symbol);
            return;
        }

        // Algo fills in the same window
        const Qty algo_vol_window = algo_fill_buf_.query(now_ns);

        // Integer participation deficit calculation (no float division):
        //   target_vol_window = mkt_vol × target_pov_bp / 10000
        //   deficit = target_vol_window - algo_vol_window
        // We multiply first to avoid integer truncation:
        const Qty target_vol = mkt_vol * params_.target_pov_bp / 10000;
        const Qty deficit    = (target_vol > algo_vol_window)
                               ? target_vol - algo_vol_window : 0;

        // Apply participation band clamping
        const Qty vol_max = mkt_vol * params_.max_pov_bp / 10000;
        const Qty vol_min = mkt_vol * params_.min_pov_bp / 10000;

        Qty qty = deficit;
        qty = std::min(qty, vol_max);
        qty = std::max(qty, vol_min);

        // Hard order size limits
        qty = std::min(qty, params_.max_order_size);
        qty = std::min(qty, rem);

        // Round to lot
        if (params_.min_order_size > 1)
            qty = (qty / params_.min_order_size) * params_.min_order_size;

        if (__builtin_expect(qty < params_.min_order_size, 0)) {
            deficit_carry_ += deficit;  // carry small deficit to next interval
            ALGO_LOG(log_,
                "[POV][%.8s] SKIP qty=%llu < min=%llu  carry=%llu",
                params_.symbol,
                (unsigned long long)qty,
                (unsigned long long)params_.min_order_size,
                (unsigned long long)deficit_carry_);
            return;
        }

        // Add any carried deficit
        qty = std::min(qty + deficit_carry_, rem);
        qty = std::min(qty, params_.max_order_size);
        if (params_.min_order_size > 1)
            qty = (qty / params_.min_order_size) * params_.min_order_size;
        deficit_carry_ = 0;

        const Price bid = last_bid_.load(std::memory_order_relaxed);
        const Price ask = last_ask_.load(std::memory_order_relaxed);
        Price limit_px = 0;
        if (params_.limit_price != 0) {
            limit_px = params_.limit_price;
        } else {
            switch (params_.urgency) {
                case 0: limit_px = (params_.side == Side::Buy) ? bid : ask; break;
                case 1: limit_px = midpoint(bid, ask); break;
                case 2: limit_px = (params_.side == Side::Buy) ? ask : bid; break;
            }
        }

        ChildOrder* o = orders_.alloc();
        if (__builtin_expect(o == nullptr, 0)) {
            ALGO_LOG(log_, "[POV][%.8s] ERROR: order pool exhausted", params_.symbol);
            return;
        }
        std::memcpy(o->symbol, params_.symbol, 8);
        o->qty         = qty;
        o->side        = params_.side;
        o->type        = (limit_px == 0) ? OrderType::Market : OrderType::Limit;
        o->tif         = TimeInForce::IOC;
        o->limit_price = limit_px;
        o->submit_tsc  = rdtsc_now();
        o->live        = true;
        o->order_id    = router_.submit(*o);
        metrics_.orders_sent.fetch_add(1, std::memory_order_relaxed);

        ALGO_LOG(log_,
            "[POV][%.8s] SEND  qty=%llu  px=%.4f  mkt_vol=%llu  pov=%ubp",
            params_.symbol,
            (unsigned long long)qty,
            px_to_dbl(limit_px),
            (unsigned long long)mkt_vol,
            params_.target_pov_bp);
    }

    COLD_PATH void on_deadline_(NsTime now_ns) noexcept {
        cancel_all_live_();
        const Qty cum = metrics_.filled_qty.load(std::memory_order_relaxed);
        const Qty rem = (params_.total_qty > cum) ? params_.total_qty - cum : 0;
        if (rem >= params_.min_order_size && params_.allow_end_sweep) {
            ALGO_LOG(log_,
                "[POV][%.8s] DEADLINE: sweeping remaining=%llu as market order",
                params_.symbol, (unsigned long long)rem);
            ChildOrder* o = orders_.alloc();
            if (o) {
                std::memcpy(o->symbol, params_.symbol, 8);
                o->qty        = rem;
                o->side       = params_.side;
                o->type       = OrderType::Market;
                o->tif        = TimeInForce::IOC;
                o->limit_price= 0;
                o->submit_tsc = rdtsc_now();
                o->live       = true;
                o->order_id   = router_.submit(*o);
                metrics_.orders_sent.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            finish_();
        }
        (void)now_ns;
    }

    COLD_PATH void cancel_all_live_() noexcept { orders_.cancel_all(router_); }

    COLD_PATH void finish_() noexcept {
        cancel_all_live_();
        state_.store(AlgoState::Complete, std::memory_order_release);
        ALGO_LOG(log_, "[POV][%.8s] COMPLETE  filled=%llu/%llu",
            params_.symbol,
            (unsigned long long)metrics_.filled_qty.load(std::memory_order_relaxed),
            (unsigned long long)params_.total_qty);
        log_.flush();
    }

    // ── Data members ─────────────────────────────────────────────────────────
    ParticipateParams                params_;
    IOrderRouter&                    router_;
    std::atomic<AlgoState>           state_            {AlgoState::Idle};
    NsTime                           last_check_ns_    {0};
    Qty                              deficit_carry_    {0};
    Qty                              total_mkt_vol_seen_{0};  // cold path only

    CACHE_ALIGN std::atomic<Price>   last_bid_         {0};
    CACHE_ALIGN std::atomic<Price>   last_ask_         {0};
    CACHE_ALIGN AlgoMetrics          metrics_;
    CACHE_ALIGN VolBuf               mkt_vol_buf_;   // market volume rolling window
    CACHE_ALIGN VolBuf               algo_fill_buf_; // algo fill rolling window
    CACHE_ALIGN OrderPool<64>        orders_;
    mutable     LogRing              log_;
};

// ════════════════════════════════════════════════════════════════════════════
// SIMULATION HARNESS
// ════════════════════════════════════════════════════════════════════════════

struct MockRouter final : public IOrderRouter {
    explicit MockRouter(ParticipateAlgo* a) : algo(a) {}
    uint64_t submit(const ChildOrder& o) override {
        const uint64_t id = ++next_id;
        fills.push_back({id,
            o.limit_price ? o.limit_price : dbl_to_px(525.50),
            (o.qty * 9) / 10,   // 90% fill rate
            rdtsc_now(),
            now_midnight_ns()});
        return id;
    }
    bool cancel(uint64_t)           override { return true; }
    bool replace(uint64_t,Qty,Price)override { return true; }
    void deliver() { for (auto& f : fills) algo->on_fill(f); fills.clear(); }

    ParticipateAlgo*        algo;
    uint64_t                next_id = 0;
    std::vector<FillReport> fills;
};

int main() {
    std::puts(
        "╔══════════════════════════════════════════════════════════╗\n"
        "║  Participate / POV Algorithm — ULL Simulation            ║\n"
        "╚══════════════════════════════════════════════════════════╝");
    calibrate_tsc();

    ParticipateParams p;
    p.side              = Side::Buy;
    p.total_qty         = 10'000;
    p.target_pov_bp     = 1000;   // 10%
    p.min_pov_bp        =  500;   //  5%
    p.max_pov_bp        = 2000;   // 20%
    p.check_interval_ns = 500'000'000ULL;   // 500ms demo
    p.volume_window_ns  = 2'000'000'000ULL; // 2s rolling demo
    p.min_order_size    = 100;
    p.max_order_size    = 2'000;
    p.self_fill_dedupe  = true;
    p.allow_end_sweep   = true;

    const NsTime now = now_midnight_ns();
    p.start_time_ns  = now;
    p.end_time_ns    = now + 10'000'000'000ULL;  // 10s demo

    MockRouter      router(nullptr);
    ParticipateAlgo algo(p, router);
    router.algo = &algo;

    Quote q{}; std::memcpy(q.symbol, p.symbol, 8);
    q.bid_px = dbl_to_px(525.45); q.ask_px = dbl_to_px(525.55);
    algo.on_quote(q);
    algo.start();

    MarketTrade tr{}; std::memcpy(tr.symbol, p.symbol, 8);
    // Simulate: one trade + one timer check every 500ms
    for (int tick = 0; tick < 20 && algo.state() == AlgoState::Active; ++tick) {
        const NsTime t = p.start_time_ns
                       + static_cast<NsTime>(tick) * 500'000'000ULL;

        // Market trade arriving (varying volume)
        tr.timestamp_ns = t;
        tr.price = dbl_to_px(525.50 + 0.01 * tick);
        tr.qty   = static_cast<Qty>(2000 + tick * 300);
        algo.on_trade(tr);

        // Drift quote
        q.bid_px = dbl_to_px(525.45 + 0.01 * tick);
        q.ask_px = dbl_to_px(525.55 + 0.01 * tick);
        algo.on_quote(q);

        // Timer check
        algo.on_timer(t);
        router.deliver();
    }

    algo.print_summary();
    return 0;
}

