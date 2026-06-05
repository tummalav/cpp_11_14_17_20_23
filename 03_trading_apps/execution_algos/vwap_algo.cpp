/**
 * vwap_algo.cpp — VWAP (Volume Weighted Average Price) Execution Algorithm
 *                 Ultra-Low Latency Implementation
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CONCEPT
 * ═══════════════════════════════════════════════════════════════════════════
 * VWAP = Σ(price × volume) / Σ(volume)  over a time window.
 *
 * The VWAP algo splits a parent order into children whose SIZE mirrors the
 * EXPECTED market volume in each time slice, sourced from a historical
 * intraday volume profile (the "U-shaped" curve: heavy open, light midday,
 * heavy close on US equities).
 *
 * Goal: achieve an average execution price ≤ (buy) or ≥ (sell) the day's
 * VWAP benchmark, minimising market impact by participating with market rhythm.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ALGO PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  symbol            8-char instrument ID.
 *  side              Buy / Sell.
 *  total_qty         Parent order quantity.
 *  start_time_ns     Window start (ns since midnight).
 *  end_time_ns       Window end.
 *  num_buckets       Time slices (default 13 × 30-min = 6.5h US session).
 *  volume_profile[]  Historical % of daily volume per bucket. Must sum to 1.0.
 *                    Classic U-curve: 12%, 8.5%, 7%, ... , 12%.
 *  limit_price       Worst acceptable price (0 = no outer limit / market).
 *  max_participation Max % of real-time market volume per bucket (cap to
 *                    avoid moving price). Typical 15–25%.
 *  min_order_size    Minimum child order size. Skip bucket if below this.
 *  urgency           0=passive (near-touch), 1=neutral (mid), 2=aggressive (take).
 *  catch_up_enabled  Re-spread unfilled residual across remaining buckets.
 *  randomise_bp      ±N basis-points size jitter (1000 bp = ±10%).
 *                    Prevents algo fingerprinting by HFT observers.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ULL DESIGN
 * ═══════════════════════════════════════════════════════════════════════════
 *  Hot path  (on_trade / on_quote / on_timer):
 *    ✓ Zero heap allocation — fixed OrderPool<64> and VWAPBucket[48]
 *    ✓ Integer price arithmetic — no float on critical path
 *    ✓ RDTSC timestamp on every order submit (fill latency measurement)
 *    ✓ ALGO_LOG → SPSC LogRing — never blocks producer thread
 *    ✓ Atomic metrics with relaxed ordering — no mutex, no I/O
 *    ✓ alignas(64) on all hot state — zero false sharing
 *    ✓ __builtin_expect on all unlikely paths
 *
 *  Cold path (start / cancel / summary):
 *    ✓ Float-point schedule build once at start()
 *    ✓ Log ring flush to stdout
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * BUILD
 * ═══════════════════════════════════════════════════════════════════════════
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *      vwap_algo.cpp -lpthread -o vwap_algo
 */

#include "algo_common.hpp"
#include <cmath>
#include <vector>   // only used in MockRouter (cold path)
#include <cstdio>

// ════════════════════════════════════════════════════════════════════════════
// VWAP PARAMETERS
// ════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t VWAP_MAX_BUCKETS = 48;

struct VWAPParams {
    char     symbol[8]       = {'A','A','P','L',0,0,0,0};
    Side     side            = Side::Buy;
    Qty      total_qty       = 100'000;
    NsTime   start_time_ns   = hhmm_ns(9, 30);
    NsTime   end_time_ns     = hhmm_ns(16, 0);
    uint32_t num_buckets     = 13;

    // Classic US equity U-shaped volume curve (13 × 30-min buckets, 9:30–16:00)
    // Front and back weighted heavier; midday lighter.
    double   volume_profile[VWAP_MAX_BUCKETS] = {
        0.120, 0.085, 0.070, 0.060, 0.055, 0.050,
        0.050, 0.055, 0.060, 0.065, 0.070, 0.090, 0.120
    };

    Price    limit_price       = 0;      // 0 = no outer limit
    double   max_participation = 0.20;   // cap at 20% of real-time market vol
    Qty      min_order_size    = 100;    // 1 round lot
    uint8_t  urgency           = 1;      // 0=passive, 1=neutral, 2=aggressive
    bool     catch_up_enabled  = true;
    uint32_t randomise_bp      = 1000;   // ±10% (1000 bp = ±10%)

    COLD_PATH bool validate() const noexcept {
        if (total_qty == 0 || start_time_ns >= end_time_ns) return false;
        if (num_buckets < 1 || num_buckets > VWAP_MAX_BUCKETS) return false;
        double sum = 0.0;
        for (uint32_t i = 0; i < num_buckets; ++i) sum += volume_profile[i];
        if (std::fabs(sum - 1.0) > 0.01) return false;
        if (max_participation <= 0.0 || max_participation > 1.0) return false;
        return true;
    }
};

// ════════════════════════════════════════════════════════════════════════════
// BUCKET SCHEDULE  — pre-computed at start(), never touched again in hot path
// ════════════════════════════════════════════════════════════════════════════

struct alignas(CACHE_LINE) VWAPBucket {
    NsTime   fire_ns;       // start nanosecond of this bucket
    NsTime   end_ns;        // end nanosecond of this bucket
    Qty      target_qty;    // pre-computed shares for this bucket
    Qty      filled_qty;    // updated on fills
    Qty      market_vol;    // real-time market volume seen in this bucket
    bool     order_sent;    // true once we've submitted a child for this bucket
    uint8_t  _pad[3];
};

// ════════════════════════════════════════════════════════════════════════════
// VWAP ALGO
// ════════════════════════════════════════════════════════════════════════════

class VWAPAlgo {
public:
    explicit VWAPAlgo(const VWAPParams& p, IOrderRouter& router) noexcept
        : params_(p), router_(router)
    {
        assert(p.validate());
        metrics_.reset();
        std::memset(buckets_, 0, sizeof(buckets_));
    }

    // ── Lifecycle (cold path) ────────────────────────────────────────────────

    COLD_PATH void start() noexcept {
        if (state_.load(std::memory_order_relaxed) != AlgoState::Idle) return;
        build_schedule_();
        state_.store(AlgoState::Active, std::memory_order_release);
        ALGO_LOG(log_, "[VWAP][%.8s] STARTED  qty=%llu  buckets=%u  urgency=%u",
            params_.symbol, (unsigned long long)params_.total_qty,
            params_.num_buckets, params_.urgency);
    }

    COLD_PATH void pause() noexcept {
        AlgoState exp = AlgoState::Active;
        if (state_.compare_exchange_strong(exp, AlgoState::Paused,
                std::memory_order_acq_rel)) {
            cancel_all_live_();
            ALGO_LOG(log_, "[VWAP][%.8s] PAUSED", params_.symbol);
        }
    }

    COLD_PATH void resume() noexcept {
        AlgoState exp = AlgoState::Paused;
        if (state_.compare_exchange_strong(exp, AlgoState::Active,
                std::memory_order_acq_rel))
            ALGO_LOG(log_, "[VWAP][%.8s] RESUMED", params_.symbol);
    }

    COLD_PATH void cancel() noexcept {
        state_.store(AlgoState::Cancelled, std::memory_order_release);
        cancel_all_live_();
        ALGO_LOG(log_, "[VWAP][%.8s] CANCELLED", params_.symbol);
        log_.flush();
    }

    // ── Market data (hot path) ───────────────────────────────────────────────

    // Called per public trade print — sub-microsecond budget
    FORCE_INLINE HOT_PATH void on_trade(const MarketTrade& t) noexcept {
        if (__builtin_expect(
                state_.load(std::memory_order_relaxed) != AlgoState::Active, 0))
            return;
        metrics_.ticks_processed.fetch_add(1, std::memory_order_relaxed);
        const int b = bucket_idx_(t.timestamp_ns);
        if (__builtin_expect(b >= 0, 1))
            buckets_[b].market_vol += t.qty;  // feed is single-threaded
    }

    // Called per quote update — just write two atomics
    FORCE_INLINE HOT_PATH void on_quote(const Quote& q) noexcept {
        last_bid_.store(q.bid_px, std::memory_order_relaxed);
        last_ask_.store(q.ask_px, std::memory_order_relaxed);
    }

    // ── Timer (hot path) — call every ~100 ms from scheduler thread ──────────

    FORCE_INLINE HOT_PATH void on_timer(NsTime now_ns) noexcept {
        if (__builtin_expect(
                state_.load(std::memory_order_relaxed) != AlgoState::Active, 0))
            return;
        if (__builtin_expect(now_ns >= params_.end_time_ns, 0)) {
            finish_();
            return;
        }

        const int b = bucket_idx_(now_ns);
        if (__builtin_expect(b < 0, 0)) return;

        if (b != cur_bucket_) {
            on_bucket_change_(b);
            return;
        }

        // Only one live child at a time; only one order per bucket
        if (orders_.live_count() > 0 || buckets_[b].order_sent) return;

        send_bucket_order_(b, now_ns);
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

        if (cur_bucket_ >= 0)
            buckets_[cur_bucket_].filled_qty += f.fill_qty;

        ALGO_LOG(log_, "[VWAP][%.8s] FILL  qty=%llu  px=%.4f  cum=%llu/%llu",
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
            "║  VWAP ALGO SUMMARY                                   ║\n"
            "╚══════════════════════════════════════════════════════╝\n");
        std::printf("  Symbol    : %.8s\n"
                    "  Side      : %s\n"
                    "  Target    : %llu\n"
                    "  State     : %s\n",
            params_.symbol, side_str(params_.side),
            (unsigned long long)params_.total_qty,
            state_str(state_.load(std::memory_order_relaxed)));
        metrics_.print("VWAP", g_ns_per_tick);

        std::printf("\n  %6s  %10s  %10s  %10s  %6s\n",
                    "Bucket", "Target", "Filled", "MktVol", "Fill%");
        for (uint32_t i = 0; i < params_.num_buckets; ++i) {
            const auto& b = buckets_[i];
            std::printf("  %6u  %10llu  %10llu  %10llu  %5.1f%%\n",
                i,
                (unsigned long long)b.target_qty,
                (unsigned long long)b.filled_qty,
                (unsigned long long)b.market_vol,
                b.target_qty > 0 ? 100.0 * b.filled_qty / b.target_qty : 0.0);
        }
        std::putchar('\n');
    }

    COLD_PATH void flush_log() noexcept { log_.flush(); }
    AlgoState state() const noexcept { return state_.load(std::memory_order_acquire); }

private:
    // ── Schedule build — float OK, called once on cold path ─────────────────
    COLD_PATH void build_schedule_() noexcept {
        const NsTime window = params_.end_time_ns - params_.start_time_ns;
        bucket_dur_ns_ = window / params_.num_buckets;
        Qty assigned = 0;
        for (uint32_t i = 0; i < params_.num_buckets; ++i) {
            auto& b    = buckets_[i];
            b.fire_ns  = params_.start_time_ns + i * bucket_dur_ns_;
            b.end_ns   = b.fire_ns + bucket_dur_ns_;
            b.filled_qty  = 0;
            b.market_vol  = 0;
            b.order_sent  = false;
            if (i == params_.num_buckets - 1) {
                b.target_qty = params_.total_qty - assigned;
            } else {
                b.target_qty = static_cast<Qty>(
                    std::llround(params_.volume_profile[i] * params_.total_qty));
                b.target_qty = std::max(b.target_qty, params_.min_order_size);
                assigned += b.target_qty;
            }
        }
    }

    // Map nanosecond timestamp → bucket index (-1 if outside window)
    FORCE_INLINE int bucket_idx_(NsTime now_ns) const noexcept {
        if (__builtin_expect(now_ns < params_.start_time_ns || bucket_dur_ns_ == 0, 0))
            return -1;
        const int b = static_cast<int>((now_ns - params_.start_time_ns) / bucket_dur_ns_);
        const int max_b = static_cast<int>(params_.num_buckets) - 1;
        return (b <= max_b) ? b : max_b;
    }

    FORCE_INLINE void on_bucket_change_(int new_b) noexcept {
        cancel_all_live_();
        if (params_.catch_up_enabled && cur_bucket_ >= 0) {
            const auto& prev = buckets_[cur_bucket_];
            if (prev.filled_qty < prev.target_qty) {
                const Qty residual = prev.target_qty - prev.filled_qty;
                const uint32_t left = params_.num_buckets - static_cast<uint32_t>(new_b);
                if (left > 0) {
                    const Qty extra = residual / left;
                    for (uint32_t k = static_cast<uint32_t>(new_b);
                         k < params_.num_buckets; ++k)
                        buckets_[k].target_qty += extra;
                    buckets_[params_.num_buckets - 1].target_qty += residual % left;
                    ALGO_LOG(log_,
                        "[VWAP][%.8s] CATCH-UP residual=%llu over %u buckets",
                        params_.symbol, (unsigned long long)residual, left);
                }
            }
        }
        cur_bucket_ = new_b;
    }

    FORCE_INLINE void send_bucket_order_(int b, NsTime /*now_ns*/) noexcept {
        auto& bkt = buckets_[b];
        const Qty cum   = metrics_.filled_qty.load(std::memory_order_relaxed);
        const Qty rem   = (params_.total_qty > cum) ? params_.total_qty - cum : 0;
        if (__builtin_expect(rem == 0, 0)) { finish_(); return; }

        Qty qty = std::min(bkt.target_qty - bkt.filled_qty, rem);
        if (__builtin_expect(qty == 0, 0)) return;

        // Participation cap against observed bucket market volume
        if (bkt.market_vol > 0) {
            // integer multiply: max_participation stored as basis points / 10000
            const Qty cap = static_cast<Qty>(bkt.market_vol * params_.max_participation);
            if (bkt.filled_qty >= cap) return;
            qty = std::min(qty, cap - bkt.filled_qty);
        }

        // ±randomise_bp jitter using low bits of RDTSC as cheap PRNG
        if (params_.randomise_bp > 0) {
            const uint32_t r = static_cast<uint32_t>(rdtsc_now() & 0xFFFFu);
            const int32_t jitter = static_cast<int32_t>(
                r * 2 * params_.randomise_bp / 65535u)
                - static_cast<int32_t>(params_.randomise_bp);
            const int64_t adj = static_cast<int64_t>(qty) * (10000 + jitter) / 10000;
            qty = static_cast<Qty>(adj > 0 ? adj : 0);
        }

        // Round to lot
        if (params_.min_order_size > 1)
            qty = (qty / params_.min_order_size) * params_.min_order_size;
        if (__builtin_expect(qty < params_.min_order_size, 0)) return;

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
            ALGO_LOG(log_, "[VWAP][%.8s] ERROR: order pool exhausted", params_.symbol);
            return;
        }
        std::memcpy(o->symbol, params_.symbol, 8);
        o->qty         = qty;
        o->side        = params_.side;
        o->type        = (limit_px == 0) ? OrderType::Market : OrderType::Limit;
        o->tif         = TimeInForce::IOC;
        o->limit_price = limit_px;
        o->submit_tsc  = rdtsc_now();  // RDTSC for fill-latency measurement
        o->live        = true;
        o->order_id    = router_.submit(*o);
        metrics_.orders_sent.fetch_add(1, std::memory_order_relaxed);
        bkt.order_sent = true;

        ALGO_LOG(log_, "[VWAP][%.8s] SEND  b=%d  qty=%llu  px=%.4f  urgency=%u",
            params_.symbol, b,
            (unsigned long long)qty,
            px_to_dbl(limit_px),
            params_.urgency);
    }

    COLD_PATH void cancel_all_live_() noexcept {
        orders_.cancel_all(router_);
    }

    COLD_PATH void finish_() noexcept {
        cancel_all_live_();
        state_.store(AlgoState::Complete, std::memory_order_release);
        ALGO_LOG(log_, "[VWAP][%.8s] COMPLETE  filled=%llu/%llu",
            params_.symbol,
            (unsigned long long)metrics_.filled_qty.load(std::memory_order_relaxed),
            (unsigned long long)params_.total_qty);
        log_.flush();
    }

    // ── Data layout: hot atomics on separate cache lines ────────────────────
    VWAPParams                       params_;
    IOrderRouter&                    router_;
    std::atomic<AlgoState>           state_         {AlgoState::Idle};
    int                              cur_bucket_    {-1};
    NsTime                           bucket_dur_ns_ {0};

    CACHE_ALIGN std::atomic<Price>   last_bid_      {0};
    CACHE_ALIGN std::atomic<Price>   last_ask_      {0};
    CACHE_ALIGN AlgoMetrics          metrics_;
    CACHE_ALIGN VWAPBucket           buckets_[VWAP_MAX_BUCKETS];
    CACHE_ALIGN OrderPool<64>        orders_;
    mutable     LogRing              log_;
};

// ════════════════════════════════════════════════════════════════════════════
// SIMULATION HARNESS
// ════════════════════════════════════════════════════════════════════════════

struct MockRouter final : public IOrderRouter {
    explicit MockRouter(VWAPAlgo* a) : algo(a) {}
    uint64_t submit(const ChildOrder& o) override {
        const uint64_t id = ++next_id;
        fills.push_back({id,
            o.limit_price ? o.limit_price : dbl_to_px(185.0),
            (o.qty * 4) / 5,   // 80% fill simulated
            rdtsc_now(),
            now_midnight_ns()});
        return id;
    }
    bool cancel(uint64_t)          override { return true; }
    bool replace(uint64_t,Qty,Price) override { return true; }
    void deliver() { for (auto& f : fills) algo->on_fill(f); fills.clear(); }

    VWAPAlgo*               algo;
    uint64_t                next_id = 0;
    std::vector<FillReport> fills;
};

int main() {
    std::puts(
        "╔══════════════════════════════════════════════════════════╗\n"
        "║  VWAP Execution Algorithm — ULL Simulation               ║\n"
        "╚══════════════════════════════════════════════════════════╝");
    calibrate_tsc();

    VWAPParams p;
    p.side              = Side::Buy;
    p.total_qty         = 50'000;
    p.num_buckets       = 13;
    p.urgency           = 1;
    p.randomise_bp      = 1000;
    p.max_participation = 0.20;
    p.min_order_size    = 100;

    const NsTime now  = now_midnight_ns();
    p.start_time_ns   = now;
    p.end_time_ns     = now + p.num_buckets * 1'000'000'000ULL;  // 1s/bucket in demo

    MockRouter router(nullptr);
    VWAPAlgo   algo(p, router);
    router.algo = &algo;

    Quote q{}; std::memcpy(q.symbol, p.symbol, 8);
    q.bid_px = dbl_to_px(184.95); q.ask_px = dbl_to_px(185.05);
    algo.on_quote(q);
    algo.start();

    MarketTrade tr{}; std::memcpy(tr.symbol, p.symbol, 8);
    for (uint32_t i = 0; i < p.num_buckets && algo.state() == AlgoState::Active; ++i) {
        // t0 = bucket boundary: triggers on_bucket_change_ (returns early)
        // t1 = mid-bucket:      triggers send_bucket_order_ (order sent)
        const NsTime t0 = p.start_time_ns + i * 1'000'000'000ULL;
        const NsTime t1 = t0 + 500'000'000ULL;  // 500ms into bucket
        tr.timestamp_ns = t1;
        tr.price = dbl_to_px(185.00 + 0.01 * i);
        tr.qty   = 3'000 + i * 500;
        algo.on_trade(tr);
        q.bid_px = dbl_to_px(184.95 + 0.01 * i);
        q.ask_px = dbl_to_px(185.05 + 0.01 * i);
        algo.on_quote(q);
        algo.on_timer(t0);   // bucket boundary: triggers on_bucket_change_
        algo.on_timer(t1);   // mid-bucket: fires the child order
        router.deliver();
    }

    algo.print_summary();
    return 0;
}

