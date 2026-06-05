/**
 * twap_algo.cpp — TWAP (Time Weighted Average Price) Execution Algorithm
 *                 Ultra-Low Latency Implementation
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CONCEPT
 * ═══════════════════════════════════════════════════════════════════════════
 * TWAP executes a parent order by dividing it into equal-size child orders
 * spread UNIFORMLY across a fixed time window — regardless of market volume.
 *
 * VWAP vs TWAP:
 *   VWAP  — child sizes proportional to HISTORICAL VOLUME (U-shaped schedule)
 *   TWAP  — child sizes proportional to TIME  (flat schedule, ignores volume)
 *
 * TWAP is preferred when:
 *   • Volume data is unreliable or unavailable (illiquid stocks, crypto).
 *   • Regulatory/compliance: uniform participation required.
 *   • Basket rebalancing with many legs having different volume curves.
 *   • Dark pool / iceberg strategy where predictability beats volume-weighting.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ALGO PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  symbol            8-char instrument ID.
 *  side              Buy / Sell.
 *  total_qty         Parent order size.
 *  start_time_ns     Window start.
 *  end_time_ns       Window end.
 *  num_slices        N equal-duration time slices. child_qty ≈ total_qty/N.
 *                    More slices → lower per-order impact, more messages.
 *                    Typical: 20–120 for a 1-hour window.
 *  limit_price       Worst price (0 = no limit).
 *  urgency           0=passive, 1=neutral, 2=aggressive.
 *  randomise_bp      ±basis-points size jitter to prevent pattern detection.
 *  timing_jitter_bp  ±basis-points fire-time jitter within each slice.
 *                    e.g., 2000 bp = ±20% of slice duration.
 *  catch_up_enabled  Carry unfilled residual into the next slice.
 *  max_clip_bp       Hard cap per child as bp of total_qty (e.g., 500 = 5%).
 *  min_order_size    Skip slice if resulting qty < this value.
 *  allow_moc_sweep   Send MOC if qty remains at end_time.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ULL DESIGN
 * ═══════════════════════════════════════════════════════════════════════════
 *  Hot path  (on_quote / on_timer / on_fill):
 *    ✓ Zero heap allocation — pre-computed slice schedule in fixed array
 *    ✓ next_slice_ index: O(1) lookup, no search
 *    ✓ Fire-time pre-randomised at start() with RDTSC PRNG — no RNG in hot path
 *    ✓ Integer price arithmetic only (midpoint via shift, no division)
 *    ✓ RDTSC submit timestamp for fill-latency measurement
 *    ✓ ALGO_LOG → SPSC LogRing — zero I/O in hot path
 *    ✓ carry_qty_ accumulates silently; applied on next slice send
 *
 *  Cold path (start / print_summary):
 *    ✓ Slice schedule built with float → stored as integers
 *    ✓ Timing jitter pre-applied at start() using TSC-seeded PRNG
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * BUILD
 * ═══════════════════════════════════════════════════════════════════════════
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *      twap_algo.cpp -lpthread -o twap_algo
 */

#include "algo_common.hpp"
#include <cmath>
#include <vector>   // MockRouter only
#include <cstdio>

// ════════════════════════════════════════════════════════════════════════════
// TWAP PARAMETERS
// ════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t TWAP_MAX_SLICES = 1440;  // 1440 × 1-min = 24h

struct TWAPParams {
    char     symbol[8]        = {'M','S','F','T',0,0,0,0};
    Side     side             = Side::Buy;
    Qty      total_qty        = 60'000;
    NsTime   start_time_ns    = hhmm_ns(10, 0);
    NsTime   end_time_ns      = hhmm_ns(15, 0);
    uint32_t num_slices       = 60;      // 60 × 5-min in a 5-hour window

    Price    limit_price      = 0;       // 0 = no outer limit
    uint8_t  urgency          = 1;       // 0=passive, 1=neutral, 2=aggressive
    uint32_t randomise_bp     = 1000;    // ±10% size jitter
    uint32_t timing_jitter_bp = 2000;    // ±20% fire-time jitter within slice
    bool     catch_up_enabled = true;
    uint32_t max_clip_bp      = 500;     // hard cap = 5% of total_qty per slice
    Qty      min_order_size   = 100;
    bool     allow_moc_sweep  = true;

    COLD_PATH bool validate() const noexcept {
        return total_qty > 0
            && start_time_ns < end_time_ns
            && num_slices >= 1
            && num_slices <= TWAP_MAX_SLICES
            && urgency <= 2;
    }
};

// ════════════════════════════════════════════════════════════════════════════
// SLICE RECORD  — pre-computed, never modified in hot path except filled_qty
// ════════════════════════════════════════════════════════════════════════════

struct alignas(CACHE_LINE) TWAPSlice {
    NsTime   fire_ns;       // actual fire time (after jitter), pre-computed
    Qty      target_qty;    // pre-computed base qty for this slice
    Qty      filled_qty;    // updated on fills
    bool     sent;          // true once a child order has been submitted
    uint8_t  _pad[7];
};

// ════════════════════════════════════════════════════════════════════════════
// TWAP ALGO
// ════════════════════════════════════════════════════════════════════════════

class TWAPAlgo {
public:
    explicit TWAPAlgo(const TWAPParams& p, IOrderRouter& router) noexcept
        : params_(p), router_(router)
    {
        assert(p.validate());
        metrics_.reset();
        std::memset(slices_, 0, sizeof(slices_));
    }

    // ── Lifecycle (cold path) ────────────────────────────────────────────────

    COLD_PATH void start() noexcept {
        if (state_.load(std::memory_order_relaxed) != AlgoState::Idle) return;
        build_schedule_();
        state_.store(AlgoState::Active, std::memory_order_release);
        ALGO_LOG(log_, "[TWAP][%.8s] STARTED  qty=%llu  slices=%u  urgency=%u  moc=%d",
            params_.symbol,
            (unsigned long long)params_.total_qty,
            params_.num_slices,
            params_.urgency,
            (int)params_.allow_moc_sweep);
    }

    COLD_PATH void pause() noexcept {
        AlgoState exp = AlgoState::Active;
        if (state_.compare_exchange_strong(exp, AlgoState::Paused,
                std::memory_order_acq_rel)) {
            cancel_all_live_();
            ALGO_LOG(log_, "[TWAP][%.8s] PAUSED", params_.symbol);
        }
    }

    COLD_PATH void resume() noexcept {
        AlgoState exp = AlgoState::Paused;
        if (state_.compare_exchange_strong(exp, AlgoState::Active,
                std::memory_order_acq_rel))
            ALGO_LOG(log_, "[TWAP][%.8s] RESUMED", params_.symbol);
    }

    COLD_PATH void cancel() noexcept {
        state_.store(AlgoState::Cancelled, std::memory_order_release);
        cancel_all_live_();
        ALGO_LOG(log_, "[TWAP][%.8s] CANCELLED", params_.symbol);
        log_.flush();
    }

    // ── Market data (hot path) ───────────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_quote(const Quote& q) noexcept {
        last_bid_.store(q.bid_px, std::memory_order_relaxed);
        last_ask_.store(q.ask_px, std::memory_order_relaxed);
    }

    // ── Timer (hot path) — call every ~50 ms from scheduler thread ───────────

    FORCE_INLINE HOT_PATH void on_timer(NsTime now_ns) noexcept {
        if (__builtin_expect(
                state_.load(std::memory_order_relaxed) != AlgoState::Active, 0))
            return;
        if (__builtin_expect(now_ns >= params_.end_time_ns, 0)) {
            on_window_end_(now_ns);
            return;
        }

        // Advance through all slices whose fire_ns has arrived
        // next_slice_ avoids rescanning already-sent slices (O(1) in steady state)
        while (next_slice_ < params_.num_slices) {
            TWAPSlice& sl = slices_[next_slice_];
            if (__builtin_expect(now_ns < sl.fire_ns, 1)) break;  // not yet
            if (!sl.sent) {
                execute_slice_(next_slice_, now_ns);
            }
            ++next_slice_;
        }
    }

    // ── Fill report (hot path) ───────────────────────────────────────────────

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

        // Attribute fill to current slice
        const uint32_t sl_idx = (next_slice_ > 0) ? next_slice_ - 1 : 0;
        if (sl_idx < params_.num_slices)
            slices_[sl_idx].filled_qty += f.fill_qty;

        ALGO_LOG(log_, "[TWAP][%.8s] FILL  qty=%llu  px=%.4f  cum=%llu/%llu",
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
            "║  TWAP ALGO SUMMARY                                   ║\n"
            "╚══════════════════════════════════════════════════════╝\n");
        std::printf("  Symbol    : %.8s\n"
                    "  Side      : %s\n"
                    "  Target    : %llu\n"
                    "  State     : %s\n",
            params_.symbol, side_str(params_.side),
            (unsigned long long)params_.total_qty,
            state_str(state_.load(std::memory_order_relaxed)));
        metrics_.print("TWAP", g_ns_per_tick);

        uint32_t sent_count = 0;
        for (uint32_t i = 0; i < params_.num_slices; ++i)
            if (slices_[i].sent) ++sent_count;
        std::printf("  Slices sent: %u / %u\n", sent_count, params_.num_slices);

        std::printf("\n  %5s  %10s  %10s  %5s\n",
                    "Slice","Target","Filled","Sent");
        for (uint32_t i = 0; i < params_.num_slices; ++i) {
            const auto& sl = slices_[i];
            std::printf("  %5u  %10llu  %10llu  %5s\n",
                i,
                (unsigned long long)sl.target_qty,
                (unsigned long long)sl.filled_qty,
                sl.sent ? "Y" : "N");
        }
        std::putchar('\n');
    }

    COLD_PATH void flush_log() noexcept { log_.flush(); }
    AlgoState state() const noexcept { return state_.load(std::memory_order_acquire); }

private:
    // ── Schedule build — pre-randomise fire times using TSC PRNG ────────────
    COLD_PATH void build_schedule_() noexcept {
        const NsTime window   = params_.end_time_ns - params_.start_time_ns;
        slice_dur_ns_         = window / params_.num_slices;
        const Qty base_qty    = params_.total_qty / params_.num_slices;
        const Qty remainder   = params_.total_qty % params_.num_slices;
        const Qty clip        = params_.max_clip_bp > 0
            ? params_.total_qty * params_.max_clip_bp / 10000 : Qty(-1);

        // Use TSC-seeded LCG for deterministic but non-predictable jitter
        uint64_t rng = rdtsc_now() ^ 0xDEADBEEFCAFEBABEULL;

        for (uint32_t i = 0; i < params_.num_slices; ++i) {
            auto& sl = slices_[i];
            const NsTime nominal = params_.start_time_ns + i * slice_dur_ns_;

            // LCG step — cheap, no heap
            rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;

            // ±timing_jitter_bp fire-time shift
            if (params_.timing_jitter_bp > 0) {
                const uint32_t r = static_cast<uint32_t>(rng >> 33);
                const int64_t  jitter_ns = static_cast<int64_t>(
                    slice_dur_ns_ * params_.timing_jitter_bp / 10000);
                const int64_t shift = static_cast<int64_t>(r % (2 * jitter_ns + 1))
                                    - jitter_ns;
                sl.fire_ns = static_cast<NsTime>(
                    static_cast<int64_t>(nominal) + shift);
            } else {
                sl.fire_ns = nominal;
            }

            sl.target_qty = base_qty + (i == params_.num_slices - 1 ? remainder : 0);
            sl.target_qty = std::min(sl.target_qty, clip);
            sl.filled_qty = 0;
            sl.sent       = false;
        }
    }

    FORCE_INLINE void execute_slice_(uint32_t idx, NsTime now_ns) noexcept {
        auto& sl = slices_[idx];
        sl.sent  = true;

        const Qty cum     = metrics_.filled_qty.load(std::memory_order_relaxed);
        const Qty rem     = (params_.total_qty > cum) ? params_.total_qty - cum : 0;
        if (__builtin_expect(rem == 0, 0)) { finish_(); return; }

        // Base qty + catch-up carry from previous unfilled slices
        Qty qty = sl.target_qty + carry_qty_;
        carry_qty_ = 0;
        qty = std::min(qty, rem);

        // ±randomise_bp size jitter — LCG PRNG, no heap
        if (params_.randomise_bp > 0) {
            lcg_state_ = lcg_state_ * 6364136223846793005ULL + 1442695040888963407ULL;
            const uint32_t r = static_cast<uint32_t>(lcg_state_ >> 33);
            const int32_t jitter = static_cast<int32_t>(
                r * 2 * params_.randomise_bp / 0xFFFFFFFFu)
                - static_cast<int32_t>(params_.randomise_bp);
            const int64_t adj = static_cast<int64_t>(qty) * (10000 + jitter) / 10000;
            qty = static_cast<Qty>(adj > 0 ? adj : 0);
        }

        // Round to lot
        if (params_.min_order_size > 1)
            qty = (qty / params_.min_order_size) * params_.min_order_size;
        if (__builtin_expect(qty < params_.min_order_size, 0)) {
            // Accumulate as carry rather than dropping entirely
            carry_qty_ += sl.target_qty;
            return;
        }

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
            ALGO_LOG(log_, "[TWAP][%.8s] ERROR: order pool exhausted", params_.symbol);
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

        ALGO_LOG(log_, "[TWAP][%.8s] SEND  slice=%u  qty=%llu  px=%.4f  carry=%llu",
            params_.symbol, idx,
            (unsigned long long)qty,
            px_to_dbl(limit_px),
            (unsigned long long)carry_qty_);
        (void)now_ns;
    }

    COLD_PATH void on_window_end_(NsTime now_ns) noexcept {
        cancel_all_live_();
        const Qty cum = metrics_.filled_qty.load(std::memory_order_relaxed);
        const Qty rem = (params_.total_qty > cum) ? params_.total_qty - cum : 0;
        if (rem >= params_.min_order_size && params_.allow_moc_sweep) {
            ALGO_LOG(log_, "[TWAP][%.8s] END: MOC sweep qty=%llu",
                params_.symbol, (unsigned long long)rem);
            ChildOrder* o = orders_.alloc();
            if (o) {
                std::memcpy(o->symbol, params_.symbol, 8);
                o->qty        = rem;
                o->side       = params_.side;
                o->type       = OrderType::MOC;
                o->tif        = TimeInForce::AUC;
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

    COLD_PATH void cancel_all_live_() noexcept {
        orders_.cancel_all(router_);
    }

    COLD_PATH void finish_() noexcept {
        cancel_all_live_();
        state_.store(AlgoState::Complete, std::memory_order_release);
        ALGO_LOG(log_, "[TWAP][%.8s] COMPLETE  filled=%llu/%llu",
            params_.symbol,
            (unsigned long long)metrics_.filled_qty.load(std::memory_order_relaxed),
            (unsigned long long)params_.total_qty);
        log_.flush();
    }

    // ── Data members ─────────────────────────────────────────────────────────
    TWAPParams                       params_;
    IOrderRouter&                    router_;
    std::atomic<AlgoState>           state_         {AlgoState::Idle};
    uint32_t                         next_slice_    {0};
    NsTime                           slice_dur_ns_  {0};
    Qty                              carry_qty_     {0};  // unfilled residual
    uint64_t                         lcg_state_     {0};  // inline PRNG state

    CACHE_ALIGN std::atomic<Price>   last_bid_      {0};
    CACHE_ALIGN std::atomic<Price>   last_ask_      {0};
    CACHE_ALIGN AlgoMetrics          metrics_;
    CACHE_ALIGN TWAPSlice            slices_[TWAP_MAX_SLICES];
    CACHE_ALIGN OrderPool<64>        orders_;
    mutable     LogRing              log_;
};

// ════════════════════════════════════════════════════════════════════════════
// SIMULATION HARNESS
// ════════════════════════════════════════════════════════════════════════════

struct MockRouter final : public IOrderRouter {
    explicit MockRouter(TWAPAlgo* a) : algo(a) {}
    uint64_t submit(const ChildOrder& o) override {
        const uint64_t id = ++next_id;
        fills.push_back({id,
            o.limit_price ? o.limit_price : dbl_to_px(420.0),
            (o.qty * 4) / 5,
            rdtsc_now(),
            now_midnight_ns()});
        return id;
    }
    bool cancel(uint64_t)           override { return true; }
    bool replace(uint64_t,Qty,Price)override { return true; }
    void deliver() { for (auto& f : fills) algo->on_fill(f); fills.clear(); }

    TWAPAlgo*               algo;
    uint64_t                next_id = 0;
    std::vector<FillReport> fills;
};

int main() {
    std::puts(
        "╔══════════════════════════════════════════════════════════╗\n"
        "║  TWAP Execution Algorithm — ULL Simulation               ║\n"
        "╚══════════════════════════════════════════════════════════╝");
    calibrate_tsc();

    TWAPParams p;
    p.side              = Side::Sell;
    p.total_qty         = 30'000;
    p.num_slices        = 10;
    p.urgency           = 1;
    p.randomise_bp      = 1000;
    p.timing_jitter_bp  = 1500;
    p.catch_up_enabled  = true;
    p.max_clip_bp       = 1500;   // max 15% per slice
    p.min_order_size    = 100;
    p.allow_moc_sweep   = true;

    const NsTime now = now_midnight_ns();
    p.start_time_ns  = now;
    p.end_time_ns    = now + p.num_slices * 1'000'000'000ULL;  // 1s/slice demo

    MockRouter router(nullptr);
    TWAPAlgo   algo(p, router);
    router.algo = &algo;

    Quote q{}; std::memcpy(q.symbol, p.symbol, 8);
    q.bid_px = dbl_to_px(419.95); q.ask_px = dbl_to_px(420.05);
    algo.on_quote(q);
    algo.start();

    for (uint32_t i = 0; i <= p.num_slices && algo.state() == AlgoState::Active; ++i) {
        const NsTime t = p.start_time_ns + i * 1'000'000'000ULL + 50'000ULL;
        q.bid_px = dbl_to_px(419.95 + 0.02 * i);
        q.ask_px = dbl_to_px(420.05 + 0.02 * i);
        algo.on_quote(q);
        algo.on_timer(t);
        router.deliver();
    }

    algo.print_summary();
    return 0;
}

