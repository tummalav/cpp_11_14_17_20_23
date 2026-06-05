/**
 * moc_algo.cpp — MOC (Market On Close) / LOC (Limit On Close) Execution Algorithm
 *                Ultra-Low Latency Implementation
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CONCEPT
 * ═══════════════════════════════════════════════════════════════════════════
 * A MOC order executes AT the closing auction print — the single clearing
 * price at which the exchange matches all accumulated closing interest at 16:00.
 *
 * Key mechanics (US equities — NYSE / NASDAQ):
 *   NYSE:
 *     • MOC/LOC submission deadline: 15:45 (orders locked after this)
 *     • D-Order / MOC cancel/replace: until 15:58
 *     • Closing Auction: 16:00:00
 *     • "Closing Imbalance" published: 15:45, then every 5–30s until 15:59
 *
 *   NASDAQ:
 *     • MOC submission: until 15:55
 *     • Cancel/replace: until 15:59:55
 *     • Closing Cross: 16:00:00
 *     • Imbalance message: from 15:55, every 5s
 *
 * Why use MOC:
 *   • Index funds / ETFs MUST execute at NAV-based close (benchmark tracking)
 *   • Guaranteed participation in closing auction — institutional standard
 *   • Often the highest-volume print of the day (>10% of daily volume)
 *   • Avoid end-of-day spread widening in continuous session
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ALGO PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  symbol                  8-char instrument ID.
 *  side                    Buy / Sell.
 *  total_qty               Parent order quantity.
 *  order_type              MOC (market at close) or LOC (limit at close).
 *  limit_price             LOC limit price. Ignored for MOC.
 *  submit_deadline_ns      Must submit BEFORE this time.
 *                          NYSE: 15:45. NASDAQ: 15:55.
 *  cancel_deadline_ns      Last time to cancel/replace.
 *                          NYSE: 15:58. NASDAQ: 15:59:55.
 *  close_time_ns           Closing auction time (16:00:00).
 *  post_close_timeout_ns   Timeout waiting for fill after close (16:00:30).
 *
 *  pre_close_participation Whether to participate in continuous session
 *  pre_close_start_ns      before submitting MOC, to reduce auction risk.
 *  pre_close_pov_bp        POV rate for pre-close continuous session.
 *                          Typical: 500–1000 bp (5–10%).
 *  pre_close_target_pct    Fraction of total_qty to fill pre-close (e.g., 0.3 = 30%).
 *                          Remainder submitted as MOC.
 *
 *  imbalance_action        What to do if closing imbalance is adverse:
 *                            DoNothing  = always fill at close (MOC semantics)
 *                            SwitchToLOC= replace MOC with LOC at ref_price
 *                            CancelOrder= cancel; keep pre-close fills only
 *  imbalance_threshold     Act if imbalance_qty on our side >= N × remaining_qty.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * STATE MACHINE
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  IDLE → (start())
 *  PRE_CLOSE_PARTICIPATING → (if pre_close enabled, POV in continuous session)
 *  AWAITING_SUBMIT → (waiting for submit_deadline_ns)
 *  ORDER_LIVE → (MOC/LOC submitted; monitoring imbalance; can cancel)
 *  AUCTION_PENDING → (past cancel_deadline_ns; locked)
 *  COMPLETE / CANCELLED
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ULL DESIGN
 * ═══════════════════════════════════════════════════════════════════════════
 *  ✓ Pre-staged MOC order: fully built at construction, submit_tsc stamped
 *    at fire time. Zero allocation on hot path.
 *  ✓ Pre-close POV sub-algo: embedded RollingVolBuf + atomic metrics.
 *    Runs inline in on_trade() / on_timer() until pre_close end.
 *  ✓ Imbalance processing: integer qty comparison, atomic state update.
 *  ✓ All state transitions: std::atomic<MOCState> — lock-free.
 *  ✓ RDTSC timestamps on every order event.
 *  ✓ ALGO_LOG → SPSC LogRing — zero I/O in hot path.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * BUILD
 * ═══════════════════════════════════════════════════════════════════════════
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *      moc_algo.cpp -lpthread -o moc_algo
 */

#include "algo_common.hpp"
#include <vector>  // MockRouter only
#include <cstdio>

// ════════════════════════════════════════════════════════════════════════════
// MOC STATE MACHINE
// ════════════════════════════════════════════════════════════════════════════

enum class MOCState : uint8_t {
    Idle                 = 0,
    PreCloseParticipating= 1,  // POV in continuous session before MOC submit
    AwaitingSubmit       = 2,  // waiting for submit_deadline_ns
    OrderLive            = 3,  // MOC/LOC on exchange; can still cancel
    AuctionPending       = 4,  // past cancel deadline; auction locked
    Complete             = 5,
    Cancelled            = 6,
};

static inline const char* moc_state_str(MOCState s) {
    switch(s) {
        case MOCState::Idle:                  return "IDLE";
        case MOCState::PreCloseParticipating: return "PRE_CLOSE_PARTICIPATING";
        case MOCState::AwaitingSubmit:        return "AWAITING_SUBMIT";
        case MOCState::OrderLive:             return "ORDER_LIVE";
        case MOCState::AuctionPending:        return "AUCTION_PENDING";
        case MOCState::Complete:              return "COMPLETE";
        case MOCState::Cancelled:             return "CANCELLED";
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════════════
// MOC PARAMETERS
// ════════════════════════════════════════════════════════════════════════════

enum class MOCImbalanceAction : uint8_t {
    DoNothing  = 0,
    SwitchToLOC= 1,
    CancelOrder= 2,
};

struct MOCParams {
    char     symbol[8]               = {'Q','Q','Q',0,0,0,0,0};
    Side     side                    = Side::Buy;
    Qty      total_qty               = 8'000;
    OrderType order_type             = OrderType::MOC;
    Price    limit_price             = 0;       // LOC only

    // Timing (nanoseconds since midnight — NYSE defaults shown)
    NsTime   submit_deadline_ns      = hhmm_ns(15, 45);     // NYSE: submit by 15:45
    NsTime   cancel_deadline_ns      = hhmm_ns(15, 58);     // NYSE: cancel by 15:58
    NsTime   close_time_ns           = hhmm_ns(16,  0);     // closing auction 16:00
    NsTime   post_close_timeout_ns   = hhmm_ns(16,  0, 30); // 30s post-close timeout

    // Pre-close participation in continuous session (optional)
    bool     pre_close_participation = false;
    NsTime   pre_close_start_ns      = hhmm_ns(15, 30);     // start POV at 15:30
    uint32_t pre_close_pov_bp        = 500;    // 5% POV pre-close
    double   pre_close_target_pct    = 0.30;   // fill 30% pre-close; 70% at close

    // Imbalance response
    MOCImbalanceAction imbalance_action     = MOCImbalanceAction::DoNothing;
    uint32_t           imbalance_threshold  = 200;   // act if imbalance >= 2× remaining

    COLD_PATH bool validate() const noexcept {
        if (total_qty == 0) return false;
        if (submit_deadline_ns >= cancel_deadline_ns) return false;
        if (cancel_deadline_ns >= close_time_ns) return false;
        if (close_time_ns >= post_close_timeout_ns) return false;
        if (pre_close_participation &&
            pre_close_start_ns >= submit_deadline_ns) return false;
        return true;
    }
};

// ════════════════════════════════════════════════════════════════════════════
// MOC ALGO
// ════════════════════════════════════════════════════════════════════════════

class MOCAlgo {
    using VolBuf = RollingVolBuf<65536>;

public:
    explicit MOCAlgo(const MOCParams& p, IOrderRouter& router) noexcept
        : params_(p), router_(router),
          pre_close_vol_buf_(60'000'000'000ULL)  // 60s rolling window for pre-close POV
    {
        assert(p.validate());
        metrics_.reset();

        // Compute pre-close target qty (round to min lot of 100)
        pre_close_target_qty_ = p.pre_close_participation
            ? static_cast<Qty>(p.total_qty * p.pre_close_target_pct / 100.0) * 100
            : 0;
        moc_qty_ = p.total_qty - pre_close_target_qty_;

        // Pre-build the MOC order
        prebuild_moc_order_();
    }

    // ── Lifecycle (cold path) ────────────────────────────────────────────────

    COLD_PATH void start() noexcept {
        if (moc_state_.load(std::memory_order_relaxed) != MOCState::Idle) return;
        const MOCState first = params_.pre_close_participation
            ? MOCState::PreCloseParticipating
            : MOCState::AwaitingSubmit;
        moc_state_.store(first, std::memory_order_release);
        ALGO_LOG(log_,
            "[MOC][%.8s] STARTED  total=%llu  moc_qty=%llu  pre_close_qty=%llu  type=%s",
            params_.symbol,
            (unsigned long long)params_.total_qty,
            (unsigned long long)moc_qty_,
            (unsigned long long)pre_close_target_qty_,
            params_.order_type == OrderType::MOC ? "MOC" : "LOC");
    }

    COLD_PATH void cancel() noexcept {
        const MOCState cur = moc_state_.load(std::memory_order_acquire);
        if (cur == MOCState::OrderLive) {
            router_.cancel(moc_order_id_);
            metrics_.orders_cancelled.fetch_add(1, std::memory_order_relaxed);
        }
        cancel_all_pre_close_();
        moc_state_.store(MOCState::Cancelled, std::memory_order_release);
        ALGO_LOG(log_, "[MOC][%.8s] CANCELLED", params_.symbol);
        log_.flush();
    }

    // ── Market data (hot path) ───────────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_trade(const MarketTrade& t) noexcept {
        if (__builtin_expect(
                moc_state_.load(std::memory_order_relaxed)
                != MOCState::PreCloseParticipating, 0))
            return;
        metrics_.ticks_processed.fetch_add(1, std::memory_order_relaxed);
        pre_close_vol_buf_.push(t.timestamp_ns, t.qty);
    }

    FORCE_INLINE HOT_PATH void on_quote(const Quote& q) noexcept {
        last_bid_.store(q.bid_px, std::memory_order_relaxed);
        last_ask_.store(q.ask_px, std::memory_order_relaxed);
    }

    // ── Closing imbalance (hot path) ─────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_imbalance(const AuctionImbalance& ai) noexcept {
        const MOCState cur = moc_state_.load(std::memory_order_relaxed);
        if (__builtin_expect(cur != MOCState::OrderLive, 0)) return;
        if (__builtin_expect(ai.timestamp_ns >= params_.cancel_deadline_ns, 0)) return;

        last_ref_price_.store(ai.ref_price,    std::memory_order_relaxed);
        last_imb_qty_  .store(ai.imbalance_qty, std::memory_order_relaxed);
        last_imb_side_ .store(static_cast<uint8_t>(ai.imbalance_side),
                               std::memory_order_relaxed);

        if (params_.imbalance_action == MOCImbalanceAction::DoNothing) return;

        // Adverse imbalance: same side as us has excess → price pressure against us
        const bool same_side = (ai.imbalance_side == params_.side);
        const Qty  remaining  = moc_qty_ - pre_close_filled_;
        const Qty  threshold  = remaining * params_.imbalance_threshold / 100;

        if (same_side && ai.imbalance_qty >= threshold) {
            ALGO_LOG(log_,
                "[MOC][%.8s] ADVERSE IMBALANCE same-side=%llu >= threshold=%llu  ACTION",
                params_.symbol,
                (unsigned long long)ai.imbalance_qty,
                (unsigned long long)threshold);
            execute_imbalance_action_(ai);
        }
    }

    // ── Timer (hot path) — call every ~100ms ─────────────────────────────────

    FORCE_INLINE HOT_PATH void on_timer(NsTime now_ns) noexcept {
        const MOCState cur = moc_state_.load(std::memory_order_relaxed);

        switch (cur) {
            // Phase 1: pre-close continuous session POV
            case MOCState::PreCloseParticipating: {
                if (__builtin_expect(now_ns >= params_.submit_deadline_ns, 0)) {
                    // Pre-close window over — transition to submit MOC
                    cancel_all_pre_close_();
                    moc_state_.store(MOCState::AwaitingSubmit,
                                     std::memory_order_release);
                    ALGO_LOG(log_,
                        "[MOC][%.8s] PRE-CLOSE ENDED  pre_filled=%llu  moc_remaining=%llu",
                        params_.symbol,
                        (unsigned long long)pre_close_filled_,
                        (unsigned long long)(moc_qty_ + pre_close_target_qty_
                                             - metrics_.filled_qty.load(std::memory_order_relaxed)));
                    // Recompute MOC qty = total - already_filled
                    const Qty already = metrics_.filled_qty.load(std::memory_order_relaxed);
                    moc_order_.qty = (params_.total_qty > already)
                                    ? params_.total_qty - already : 0;
                    return;
                }
                // Still in pre-close window — run POV evaluation
                if (now_ns >= params_.pre_close_start_ns)
                    evaluate_pre_close_pov_(now_ns);
                break;
            }

            // Phase 2: submit MOC order
            case MOCState::AwaitingSubmit: {
                if (__builtin_expect(now_ns >= params_.submit_deadline_ns, 0)) {
                    if (moc_order_.qty > 0)
                        submit_moc_order_(now_ns);
                    else
                        finish_();   // already fully filled pre-close
                }
                break;
            }

            // Phase 3: order live — monitor, watch for cancel deadline
            case MOCState::OrderLive: {
                if (__builtin_expect(now_ns >= params_.cancel_deadline_ns, 0)) {
                    moc_state_.store(MOCState::AuctionPending,
                                     std::memory_order_release);
                    ALGO_LOG(log_,
                        "[MOC][%.8s] CANCEL DEADLINE PASSED — auction locked",
                        params_.symbol);
                }
                break;
            }

            // Phase 4: waiting for fill after close
            case MOCState::AuctionPending: {
                if (__builtin_expect(now_ns >= params_.post_close_timeout_ns, 0)) {
                    ALGO_LOG(log_,
                        "[MOC][%.8s] POST-CLOSE TIMEOUT: no fill received",
                        params_.symbol);
                    moc_state_.store(MOCState::Cancelled, std::memory_order_release);
                    log_.flush();
                }
                break;
            }

            default:
                break;
        }
    }

    // ── Fill report (hot path) ───────────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_fill(const FillReport& f) noexcept {
        metrics_.fills_count .fetch_add(1,          std::memory_order_relaxed);
        metrics_.filled_qty  .fetch_add(f.fill_qty, std::memory_order_relaxed);
        metrics_.filled_value.fetch_add(
            static_cast<uint64_t>(f.fill_price) * f.fill_qty,
            std::memory_order_relaxed);

        if (f.order_id == moc_order_id_) {
            // MOC/LOC fill
            if (f.fill_tsc > moc_order_.submit_tsc)
                metrics_.sum_latency_tsc.fetch_add(
                    f.fill_tsc - moc_order_.submit_tsc,
                    std::memory_order_relaxed);
            moc_order_.filled_qty += f.fill_qty;
            if (moc_order_.filled_qty >= moc_order_.qty)
                moc_order_.live = false;
            ALGO_LOG(log_,
                "[MOC][%.8s] MOC FILL  qty=%llu  px=%.4f  cum=%llu/%llu",
                params_.symbol,
                (unsigned long long)f.fill_qty,
                px_to_dbl(f.fill_price),
                (unsigned long long)metrics_.filled_qty.load(std::memory_order_relaxed),
                (unsigned long long)params_.total_qty);
        } else {
            // Pre-close POV fill
            pre_close_filled_ += f.fill_qty;
            ChildOrder* o = pre_close_orders_.find(f.order_id);
            if (o) { o->filled_qty += f.fill_qty; if (o->filled_qty >= o->qty) o->live = false; }
            ALGO_LOG(log_,
                "[MOC][%.8s] PRE-CLOSE FILL  qty=%llu  px=%.4f  cum=%llu/%llu",
                params_.symbol,
                (unsigned long long)f.fill_qty,
                px_to_dbl(f.fill_price),
                (unsigned long long)pre_close_filled_,
                (unsigned long long)pre_close_target_qty_);
        }

        if (metrics_.filled_qty.load(std::memory_order_relaxed) >= params_.total_qty)
            finish_();
    }

    // ── Reporting (cold path) ────────────────────────────────────────────────

    COLD_PATH void print_summary() const noexcept {
        log_.flush();
        std::printf(
            "\n╔══════════════════════════════════════════════════════╗\n"
            "║  MOC / LOC ALGO SUMMARY                              ║\n"
            "╚══════════════════════════════════════════════════════╝\n");
        std::printf("  Symbol         : %.8s\n"
                    "  Side           : %s\n"
                    "  Type           : %s\n"
                    "  Total qty      : %llu\n"
                    "  MOC qty        : %llu\n"
                    "  Pre-close qty  : %llu (filled=%llu)\n"
                    "  State          : %s\n",
            params_.symbol,
            side_str(params_.side),
            params_.order_type == OrderType::MOC ? "MOC" : "LOC",
            (unsigned long long)params_.total_qty,
            (unsigned long long)moc_qty_,
            (unsigned long long)pre_close_target_qty_,
            (unsigned long long)pre_close_filled_,
            moc_state_str(moc_state_.load(std::memory_order_relaxed)));
        const Price ref = last_ref_price_.load(std::memory_order_relaxed);
        if (ref) std::printf("  Last ref px    : $%.4f\n", px_to_dbl(ref));
        metrics_.print("MOC", g_ns_per_tick);
        std::putchar('\n');
    }

    COLD_PATH void flush_log() noexcept { log_.flush(); }
    MOCState state() const noexcept { return moc_state_.load(std::memory_order_acquire); }

private:
    COLD_PATH void prebuild_moc_order_() noexcept {
        std::memcpy(moc_order_.symbol, params_.symbol, 8);
        moc_order_.qty         = moc_qty_;
        moc_order_.side        = params_.side;
        moc_order_.type        = params_.order_type;
        moc_order_.tif         = TimeInForce::AUC;
        moc_order_.limit_price = params_.limit_price;
        moc_order_.filled_qty  = 0;
        moc_order_.submit_tsc  = 0;
        moc_order_.live        = false;
        moc_order_.order_id    = 0;
    }

    // ── Pre-close POV evaluation ─────────────────────────────────────────────
    FORCE_INLINE void evaluate_pre_close_pov_(NsTime now_ns) noexcept {
        // Rate-gate
        if (now_ns - pre_close_last_check_ < 5'000'000'000ULL) return;  // 5s interval
        pre_close_last_check_ = now_ns;
        if (pre_close_orders_.live_count() > 0) return;

        const Qty pre_close_rem = (pre_close_target_qty_ > pre_close_filled_)
                                  ? pre_close_target_qty_ - pre_close_filled_ : 0;
        if (pre_close_rem == 0) return;

        const Qty mkt_vol = pre_close_vol_buf_.query(now_ns);
        if (mkt_vol == 0) return;

        Qty qty = mkt_vol * params_.pre_close_pov_bp / 10000;
        qty = std::min(qty, pre_close_rem);
        qty = std::min(qty, Qty(5000));   // hard cap pre-close child
        qty = (qty / 100) * 100;          // round to lot
        if (qty < 100) return;

        const Price bid = last_bid_.load(std::memory_order_relaxed);
        const Price ask = last_ask_.load(std::memory_order_relaxed);
        const Price lim = midpoint(bid, ask);

        ChildOrder* o = pre_close_orders_.alloc();
        if (__builtin_expect(o == nullptr, 0)) return;
        std::memcpy(o->symbol, params_.symbol, 8);
        o->qty         = qty;
        o->side        = params_.side;
        o->type        = OrderType::Limit;
        o->tif         = TimeInForce::IOC;
        o->limit_price = lim;
        o->submit_tsc  = rdtsc_now();
        o->live        = true;
        o->order_id    = router_.submit(*o);
        metrics_.orders_sent.fetch_add(1, std::memory_order_relaxed);

        ALGO_LOG(log_,
            "[MOC][%.8s] PRE-CLOSE SEND  qty=%llu  px=%.4f  pov=%ubp",
            params_.symbol,
            (unsigned long long)qty,
            px_to_dbl(lim),
            params_.pre_close_pov_bp);
    }

    FORCE_INLINE void submit_moc_order_(NsTime /*now_ns*/) noexcept {
        moc_order_.submit_tsc = rdtsc_now();
        moc_order_.live       = true;
        moc_order_.order_id   = router_.submit(moc_order_);
        moc_order_id_         = moc_order_.order_id;
        metrics_.orders_sent.fetch_add(1, std::memory_order_relaxed);
        moc_state_.store(MOCState::OrderLive, std::memory_order_release);
        ALGO_LOG(log_,
            "[MOC][%.8s] SUBMITTED  order_id=%llu  qty=%llu  type=%s  lim=%.4f",
            params_.symbol,
            (unsigned long long)moc_order_id_,
            (unsigned long long)moc_order_.qty,
            params_.order_type == OrderType::MOC ? "MOC" : "LOC",
            px_to_dbl(params_.limit_price));
    }

    COLD_PATH void execute_imbalance_action_(const AuctionImbalance& ai) noexcept {
        switch (params_.imbalance_action) {
            case MOCImbalanceAction::DoNothing: break;
            case MOCImbalanceAction::SwitchToLOC: {
                const bool ok = router_.replace(
                    moc_order_id_, moc_order_.qty, ai.ref_price);
                if (ok) {
                    moc_order_.limit_price = ai.ref_price;
                    moc_order_.type        = OrderType::LOC;
                    ALGO_LOG(log_,
                        "[MOC][%.8s] REPLACED MOC→LOC  limit=%.4f",
                        params_.symbol, px_to_dbl(ai.ref_price));
                }
                break;
            }
            case MOCImbalanceAction::CancelOrder: {
                router_.cancel(moc_order_id_);
                moc_order_.live = false;
                metrics_.orders_cancelled.fetch_add(1, std::memory_order_relaxed);
                moc_state_.store(MOCState::Cancelled, std::memory_order_release);
                ALGO_LOG(log_,
                    "[MOC][%.8s] CANCELLED due to adverse closing imbalance",
                    params_.symbol);
                log_.flush();
                break;
            }
        }
    }

    COLD_PATH void cancel_all_pre_close_() noexcept {
        pre_close_orders_.cancel_all(router_);
    }

    COLD_PATH void finish_() noexcept {
        moc_order_.live = false;
        cancel_all_pre_close_();
        moc_state_.store(MOCState::Complete, std::memory_order_release);
        const Qty fq = metrics_.filled_qty.load(std::memory_order_relaxed);
        const double avg = fq > 0
            ? static_cast<double>(metrics_.filled_value.load(std::memory_order_relaxed))
              / (static_cast<double>(fq) * PRICE_SCALE)
            : 0.0;
        ALGO_LOG(log_, "[MOC][%.8s] COMPLETE  filled=%llu/%llu  avg_px=%.4f",
            params_.symbol,
            (unsigned long long)fq,
            (unsigned long long)params_.total_qty,
            avg);
        log_.flush();
    }

    // ── Data members ─────────────────────────────────────────────────────────
    MOCParams                        params_;
    IOrderRouter&                    router_;
    std::atomic<MOCState>            moc_state_           {MOCState::Idle};
    Qty                              moc_qty_             {0};
    Qty                              pre_close_target_qty_{0};
    Qty                              pre_close_filled_    {0};
    uint64_t                         moc_order_id_        {0};
    NsTime                           pre_close_last_check_{0};
    ChildOrder                       moc_order_{};

    CACHE_ALIGN std::atomic<Price>   last_bid_        {0};
    CACHE_ALIGN std::atomic<Price>   last_ask_        {0};
    CACHE_ALIGN std::atomic<Price>   last_ref_price_  {0};
    CACHE_ALIGN std::atomic<Qty>     last_imb_qty_    {0};
    CACHE_ALIGN std::atomic<uint8_t> last_imb_side_   {0};
    CACHE_ALIGN AlgoMetrics          metrics_;
    CACHE_ALIGN VolBuf               pre_close_vol_buf_;
    CACHE_ALIGN OrderPool<32>        pre_close_orders_;
    mutable     LogRing              log_;
};

// ════════════════════════════════════════════════════════════════════════════
// SIMULATION HARNESS
// ════════════════════════════════════════════════════════════════════════════

struct MockRouter final : public IOrderRouter {
    explicit MockRouter(MOCAlgo* a) : algo(a) {}
    uint64_t submit(const ChildOrder& o) override {
        const uint64_t id = ++next_id;
        const char* type_str = o.type == OrderType::MOC ? "MOC" :
                               o.type == OrderType::LOC ? "LOC" :
                               o.type == OrderType::Limit ? "LMT" : "MKT";
        std::printf("  [EXCHANGE] %s order_id=%llu  %s  qty=%llu  lim=%.4f\n",
            type_str,
            (unsigned long long)id,
            side_str(o.side),
            (unsigned long long)o.qty,
            px_to_dbl(o.limit_price));
        const Price fill_px = (o.type == OrderType::MOC || o.type == OrderType::LOC)
            ? dbl_to_px(480.25)   // simulated closing print
            : (o.limit_price ? o.limit_price : dbl_to_px(479.80));
        fills.push_back({id, fill_px, o.qty, rdtsc_now(), now_midnight_ns()});
        return id;
    }
    bool cancel(uint64_t id) override {
        std::printf("  [EXCHANGE] CANCEL %llu\n", (unsigned long long)id);
        return true;
    }
    bool replace(uint64_t id, Qty qty, Price px) override {
        std::printf("  [EXCHANGE] REPLACE %llu  qty=%llu  lim=%.4f\n",
            (unsigned long long)id, (unsigned long long)qty, px_to_dbl(px));
        return true;
    }
    void deliver() { for (auto& f : fills) algo->on_fill(f); fills.clear(); }

    MOCAlgo*                algo;
    uint64_t                next_id = 0;
    std::vector<FillReport> fills;
};

int main() {
    std::puts(
        "╔══════════════════════════════════════════════════════════╗\n"
        "║  MOC / LOC Execution Algorithm — ULL Simulation          ║\n"
        "╚══════════════════════════════════════════════════════════╝");
    calibrate_tsc();

    MOCParams p;
    p.side                    = Side::Buy;
    p.total_qty               = 8'000;
    p.order_type              = OrderType::MOC;
    p.limit_price             = 0;
    p.imbalance_action        = MOCImbalanceAction::SwitchToLOC;
    p.imbalance_threshold     = 150;   // act if imbalance >= 1.5× remaining
    p.pre_close_participation = true;
    p.pre_close_pov_bp        = 800;   // 8% pre-close POV
    p.pre_close_target_pct    = 0.25;  // fill 25% in pre-close

    // Compress timeline for demo
    const NsTime base = now_midnight_ns();
    p.pre_close_start_ns    = base + 1'000'000'000ULL;   // 1s → pre-close POV starts
    p.submit_deadline_ns    = base + 5'000'000'000ULL;   // 5s → submit MOC
    p.cancel_deadline_ns    = base + 7'000'000'000ULL;   // 7s → cancel deadline
    p.close_time_ns         = base + 8'000'000'000ULL;   // 8s → auction
    p.post_close_timeout_ns = base + 9'000'000'000ULL;   // 9s → timeout

    MockRouter router(nullptr);
    MOCAlgo    algo(p, router);
    router.algo = &algo;

    Quote q{}; std::memcpy(q.symbol, p.symbol, 8);
    q.bid_px = dbl_to_px(479.70); q.ask_px = dbl_to_px(479.90);
    algo.on_quote(q);
    algo.start();

    MarketTrade tr{}; std::memcpy(tr.symbol, p.symbol, 8);
    for (int tick = 0; tick <= 90; ++tick) {
        const NsTime t = base + static_cast<NsTime>(tick) * 100'000'000ULL;

        // Market trades during pre-close POV window
        if (tick >= 10 && tick <= 50) {
            tr.timestamp_ns = t;
            tr.price        = dbl_to_px(479.80);
            tr.qty          = static_cast<Qty>(1500 + tick * 50);
            algo.on_trade(tr);
        }

        // Drift quote
        q.bid_px = dbl_to_px(479.70 + 0.02 * tick);
        q.ask_px = dbl_to_px(479.90 + 0.02 * tick);
        algo.on_quote(q);

        // Inject closing imbalance at tick=62 (~6.2s = in ORDER_LIVE window)
        if (tick == 62) {
            AuctionImbalance ai{};
            std::memcpy(ai.symbol, p.symbol, 8);
            ai.timestamp_ns   = t;
            ai.ref_price      = dbl_to_px(480.50);
            ai.imbalance_qty  = 15'000;   // 15K on buy side
            ai.imbalance_side = Side::Buy;  // same as us
            std::puts("  [NOII] Closing: Buy-side imbalance 15000 @ ref=480.50");
            algo.on_imbalance(ai);
        }

        algo.on_timer(t);

        // Deliver pre-close fills at tick 15, 25, 35
        if (tick == 15 || tick == 25 || tick == 35) router.deliver();

        // Deliver MOC fill at tick 80 (auction)
        if (tick == 80) {
            std::puts("  [MARKET] CLOSING AUCTION EXECUTED");
            router.deliver();
        }
    }

    algo.print_summary();
    return 0;
}

