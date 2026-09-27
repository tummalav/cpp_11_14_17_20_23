/**
 * moo_algo.cpp — MOO (Market On Open) / LOO (Limit On Open) Execution Algorithm
 *                Ultra-Low Latency Implementation
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * CONCEPT
 * ═══════════════════════════════════════════════════════════════════════════
 * A MOO order executes AT the opening auction print — the single clearing
 * price at which the exchange matches all accumulated opening interest.
 *
 * Key mechanics (US equities — NYSE / NASDAQ):
 *   • Pre-market (08:00–09:28):  brokers accept MOO/LOO orders from clients.
 *   • Submission window (~09:28–09:29:50 NYSE, 09:28–09:29:55 NASDAQ):
 *       Orders entered into the exchange's opening auction book.
 *   • Cancel/replace window:
 *       NYSE:   until ~09:29:50 (10s before open)
 *       NASDAQ: until ~09:29:55
 *   • Opening Auction (09:30:00 sharp):
 *       Exchange calculates single clearing price maximising paired volume.
 *       All MOO orders fill at that price (if a price exists).
 *       LOO orders fill only if clearing price ≤ limit (buy) or ≥ limit (sell).
 *   • NOII (Net Order Imbalance Indicator) / Opening Indication:
 *       Published every 5s from ~09:28. Shows: paired qty, imbalance qty,
 *       imbalance side, reference price, near price, far price.
 *       Used to decide whether to cancel/replace or add contra-side liquidity.
 *
 * Why use MOO:
 *   • Need guaranteed participation in the opening auction (index rebalance,
 *     earnings day repositioning, large institutional fill).
 *   • Avoid pre-market bid/ask spreads (often 5–50× wider than regular hours).
 *   • Guaranteed single price — no market-impact slippage within the auction.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ALGO PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  symbol                8-char instrument ID.
 *  side                  Buy / Sell.
 *  total_qty             Parent order quantity.
 *  order_type            MOO (market at open) or LOO (limit at open).
 *  limit_price           LOO limit price. Ignored for MOO.
 *  submit_time_ns        When to submit the order to the exchange auction book.
 *                        Must be >= exchange submission open (typ. 09:28).
 *                        Must be < cancel_deadline_ns.
 *  cancel_deadline_ns    Last time the order can be cancelled/replaced.
 *                        NYSE: ~09:29:50.  NASDAQ: ~09:29:55.
 *  auction_time_ns       Expected auction execution time (09:30:00).
 *                        Used to confirm fills and transition to COMPLETE.
 *  max_imbalance_action  What to do if imbalance is against us:
 *                          0 = do nothing (MOO always fills at opening price)
 *                          1 = switch to LOO at ref_price (cap downside)
 *                          2 = cancel and reroute to continuous session
 *  imbalance_qty_threshold  Minimum imbalance qty on our side (as % of order)
 *                            to trigger max_imbalance_action. e.g., 2 = 200%
 *                            means the imbalance is 2× our order size on our side.
 *  pre_open_pov_bp       Optional: participate in pre-open cross (ATS/dark)
 *                        at this POV rate before the auction. 0 = disabled.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * STATE MACHINE
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  IDLE → (on start())
 *  AWAITING_SUBMIT → (at submit_time_ns, order sent to exchange)
 *  ORDER_LIVE → (monitoring NOII, can cancel before cancel_deadline_ns)
 *  AUCTION_PENDING → (after cancel_deadline_ns, cannot modify)
 *  COMPLETE / CANCELLED
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * ULL DESIGN
 * ═══════════════════════════════════════════════════════════════════════════
 *  ✓ Pre-staged order: ChildOrder fully built at start(), stored in pool.
 *    At submit_time_ns, on_timer() fires a single router_.submit() — minimum
 *    latency from "time to send" to "wire" (no allocation, no format at fire).
 *  ✓ Imbalance handler: atomic compare of imbalance_side vs our side,
 *    integer qty comparison — no float in hot path.
 *  ✓ State machine: std::atomic<MOOState> — lock-free transitions.
 *  ✓ RDTSC on every order event for latency measurement.
 *  ✓ ALGO_LOG → SPSC LogRing — no I/O in hot path.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * BUILD
 * ═══════════════════════════════════════════════════════════════════════════
 *  g++ -std=c++17 -O3 -march=native -DNDEBUG \
 *      moo_algo.cpp -lpthread -o moo_algo
 */

#include "algo_common.hpp"
#include <vector>  // MockRouter only
#include <cstdio>

// ════════════════════════════════════════════════════════════════════════════
// MOO STATE MACHINE
// ════════════════════════════════════════════════════════════════════════════

enum class MOOState : uint8_t {
    Idle            = 0,
    AwaitingSubmit  = 1,   // waiting for submit_time_ns
    OrderLive       = 2,   // order sent; monitoring NOII; can still cancel
    AuctionPending  = 3,   // past cancel deadline; auction will fill/reject
    Complete        = 4,
    Cancelled       = 5,
};

static inline const char* moo_state_str(MOOState s) {
    switch(s) {
        case MOOState::Idle:           return "IDLE";
        case MOOState::AwaitingSubmit: return "AWAITING_SUBMIT";
        case MOOState::OrderLive:      return "ORDER_LIVE";
        case MOOState::AuctionPending: return "AUCTION_PENDING";
        case MOOState::Complete:       return "COMPLETE";
        case MOOState::Cancelled:      return "CANCELLED";
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════════════
// MOO PARAMETERS
// ════════════════════════════════════════════════════════════════════════════

enum class MOOImbalanceAction : uint8_t {
    DoNothing    = 0,   // fill at whatever opening price (MOO semantics)
    SwitchToLOO  = 1,   // replace with LOO at ref_price to cap downside
    CancelOrder  = 2,   // cancel; reroute to continuous session
};

struct MOOParams {
    char     symbol[8]              = {'G','O','O','G',0,0,0,0};
    Side     side                   = Side::Buy;
    Qty      total_qty              = 5'000;
    OrderType order_type            = OrderType::MOO;  // MOO or LOO
    Price    limit_price            = 0;               // LOO only (0 = MOO)

    // Timing — all in nanoseconds since midnight
    NsTime   submit_time_ns         = hhmm_ns(9, 28);        // submit at 9:28
    NsTime   cancel_deadline_ns     = hhmm_ns(9, 29, 50);    // NYSE deadline
    NsTime   auction_time_ns        = hhmm_ns(9, 30, 0);     // expected fill time
    NsTime   post_open_timeout_ns   = hhmm_ns(9, 30, 30);    // give up waiting

    // Imbalance response
    MOOImbalanceAction imbalance_action    = MOOImbalanceAction::DoNothing;
    uint32_t           imbalance_threshold = 200;  // act if imbalance >= 2× our qty

    // Optional pre-open POV in continuous pre-market session (0 = disabled)
    uint32_t pre_open_pov_bp        = 0;

    COLD_PATH bool validate() const noexcept {
        return total_qty > 0
            && submit_time_ns < cancel_deadline_ns
            && cancel_deadline_ns < auction_time_ns
            && auction_time_ns < post_open_timeout_ns;
    }
};

// ════════════════════════════════════════════════════════════════════════════
// MOO ALGO
// ════════════════════════════════════════════════════════════════════════════

class MOOAlgo {
public:
    explicit MOOAlgo(const MOOParams& p, IOrderRouter& router) noexcept
        : params_(p), router_(router)
    {
        assert(p.validate());
        metrics_.reset();
        // Pre-build the auction order at construction — zero latency at fire time
        prebuild_auction_order_();
    }

    // ── Lifecycle (cold path) ────────────────────────────────────────────────

    COLD_PATH void start() noexcept {
        if (moo_state_.load(std::memory_order_relaxed) != MOOState::Idle) return;
        moo_state_.store(MOOState::AwaitingSubmit, std::memory_order_release);
        ALGO_LOG(log_,
            "[MOO][%.8s] STARTED  qty=%llu  type=%s  submit_at=09:%02llu:%02llu",
            params_.symbol,
            (unsigned long long)params_.total_qty,
            params_.order_type == OrderType::MOO ? "MOO" : "LOO",
            (unsigned long long)((params_.submit_time_ns / 60'000'000'000ULL) % 60),
            (unsigned long long)((params_.submit_time_ns /  1'000'000'000ULL) % 60));
    }

    COLD_PATH void cancel() noexcept {
        const MOOState cur = moo_state_.load(std::memory_order_acquire);
        if (cur == MOOState::OrderLive) {
            // Can still cancel before auction_deadline
            router_.cancel(auction_order_id_);
            metrics_.orders_cancelled.fetch_add(1, std::memory_order_relaxed);
        }
        moo_state_.store(MOOState::Cancelled, std::memory_order_release);
        ALGO_LOG(log_, "[MOO][%.8s] CANCELLED", params_.symbol);
        log_.flush();
    }

    // ── Quote update (hot path) ──────────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_quote(const Quote& q) noexcept {
        last_bid_.store(q.bid_px, std::memory_order_relaxed);
        last_ask_.store(q.ask_px, std::memory_order_relaxed);
    }

    // ── NOII / Auction imbalance (hot path) ──────────────────────────────────
    //
    // Published every 5s from ~09:28 by NYSE/NASDAQ.
    // ref_price:   indicative clearing price
    // imbalance_qty: excess on imbalance_side
    // imbalance_side: which side has surplus supply
    //
    FORCE_INLINE HOT_PATH void on_imbalance(const AuctionImbalance& ai) noexcept {
        const MOOState cur = moo_state_.load(std::memory_order_relaxed);
        if (__builtin_expect(cur != MOOState::OrderLive, 0)) return;
        if (__builtin_expect(ai.timestamp_ns >= params_.cancel_deadline_ns, 0)) return;

        // Update our cached view of imbalance
        last_ref_price_.store(ai.ref_price,    std::memory_order_relaxed);
        last_imb_qty_  .store(ai.imbalance_qty, std::memory_order_relaxed);
        last_imb_side_ .store(static_cast<uint8_t>(ai.imbalance_side),
                               std::memory_order_relaxed);

        // React to imbalance if threshold exceeded
        if (params_.imbalance_action == MOOImbalanceAction::DoNothing) return;

        // Is the imbalance on the SAME side as us? (our side has excess supply)
        // That means there are more buyers (if we buy) than sellers → price will clear higher
        // Or: same side means we're competing with excess same-direction flow.
        const bool imb_on_our_side = (ai.imbalance_side == params_.side);
        const Qty  threshold_qty   = params_.total_qty * params_.imbalance_threshold / 100;

        if (imb_on_our_side && ai.imbalance_qty >= threshold_qty) {
            ALGO_LOG(log_,
                "[MOO][%.8s] IMBALANCE same-side qty=%llu threshold=%llu  ACTION",
                params_.symbol,
                (unsigned long long)ai.imbalance_qty,
                (unsigned long long)threshold_qty);
            execute_imbalance_action_(ai);
        }
    }

    // ── Timer (hot path) — call every ~100ms ─────────────────────────────────

    FORCE_INLINE HOT_PATH void on_timer(NsTime now_ns) noexcept {
        const MOOState cur = moo_state_.load(std::memory_order_relaxed);

        switch (cur) {
            case MOOState::AwaitingSubmit:
                if (__builtin_expect(now_ns >= params_.submit_time_ns, 0))
                    submit_auction_order_(now_ns);
                break;

            case MOOState::OrderLive:
                if (__builtin_expect(now_ns >= params_.cancel_deadline_ns, 0)) {
                    moo_state_.store(MOOState::AuctionPending,
                                     std::memory_order_release);
                    ALGO_LOG(log_,
                        "[MOO][%.8s] CANCEL DEADLINE PASSED — auction locked",
                        params_.symbol);
                }
                break;

            case MOOState::AuctionPending:
                if (__builtin_expect(now_ns >= params_.post_open_timeout_ns, 0)) {
                    // Auction fill not received within timeout — algo is stale
                    ALGO_LOG(log_,
                        "[MOO][%.8s] POST-OPEN TIMEOUT: no fill received",
                        params_.symbol);
                    moo_state_.store(MOOState::Cancelled, std::memory_order_release);
                    log_.flush();
                }
                break;

            default:
                break;
        }
    }

    // ── Fill report (hot path) ───────────────────────────────────────────────

    FORCE_INLINE HOT_PATH void on_fill(const FillReport& f) noexcept {
        if (__builtin_expect(f.order_id != auction_order_id_, 0)) return;

        metrics_.fills_count .fetch_add(1,          std::memory_order_relaxed);
        metrics_.filled_qty  .fetch_add(f.fill_qty, std::memory_order_relaxed);
        metrics_.filled_value.fetch_add(
            static_cast<uint64_t>(f.fill_price) * f.fill_qty,
            std::memory_order_relaxed);

        if (f.fill_tsc > auction_order_.submit_tsc)
            metrics_.sum_latency_tsc.fetch_add(
                f.fill_tsc - auction_order_.submit_tsc,
                std::memory_order_relaxed);

        auction_order_.filled_qty += f.fill_qty;
        if (auction_order_.filled_qty >= auction_order_.qty)
            auction_order_.live = false;

        ALGO_LOG(log_,
            "[MOO][%.8s] FILL  qty=%llu  px=%.4f  cum=%llu/%llu",
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
            "║  MOO / LOO ALGO SUMMARY                              ║\n"
            "╚══════════════════════════════════════════════════════╝\n");
        std::printf("  Symbol    : %.8s\n"
                    "  Side      : %s\n"
                    "  Type      : %s\n"
                    "  Target    : %llu\n"
                    "  State     : %s\n",
            params_.symbol,
            side_str(params_.side),
            params_.order_type == OrderType::MOO ? "MOO" : "LOO",
            (unsigned long long)params_.total_qty,
            moo_state_str(moo_state_.load(std::memory_order_relaxed)));
        const Price ref = last_ref_price_.load(std::memory_order_relaxed);
        if (ref) std::printf("  Last ref px: $%.4f\n", px_to_dbl(ref));
        metrics_.print("MOO", g_ns_per_tick);
        std::putchar('\n');
    }

    COLD_PATH void flush_log() noexcept { log_.flush(); }
    MOOState state() const noexcept { return moo_state_.load(std::memory_order_acquire); }

private:
    // ── Pre-build the auction order at construction (cold path) ─────────────
    COLD_PATH void prebuild_auction_order_() noexcept {
        std::memcpy(auction_order_.symbol, params_.symbol, 8);
        auction_order_.qty         = params_.total_qty;
        auction_order_.side        = params_.side;
        auction_order_.type        = params_.order_type;
        auction_order_.tif         = TimeInForce::AUC;
        auction_order_.limit_price = params_.limit_price;
        auction_order_.filled_qty  = 0;
        auction_order_.submit_tsc  = 0;
        auction_order_.live        = false;
        auction_order_.order_id    = 0;
    }

    // ── Submit — called once from on_timer when submit_time_ns arrives ────────
    FORCE_INLINE void submit_auction_order_(NsTime /*now_ns*/) noexcept {
        auction_order_.submit_tsc = rdtsc_now();  // stamp BEFORE submit() call
        auction_order_.live       = true;
        auction_order_.order_id   = router_.submit(auction_order_);
        auction_order_id_         = auction_order_.order_id;
        metrics_.orders_sent.fetch_add(1, std::memory_order_relaxed);
        moo_state_.store(MOOState::OrderLive, std::memory_order_release);
        ALGO_LOG(log_,
            "[MOO][%.8s] SUBMITTED  order_id=%llu  qty=%llu  type=%s  lim=%.4f",
            params_.symbol,
            (unsigned long long)auction_order_id_,
            (unsigned long long)params_.total_qty,
            params_.order_type == OrderType::MOO ? "MOO" : "LOO",
            px_to_dbl(params_.limit_price));
    }

    // ── React to adverse imbalance ───────────────────────────────────────────
    COLD_PATH void execute_imbalance_action_(const AuctionImbalance& ai) noexcept {
        switch (params_.imbalance_action) {
            case MOOImbalanceAction::DoNothing:
                break;
            case MOOImbalanceAction::SwitchToLOO: {
                // Replace MOO with LOO at current reference price
                const Price new_limit = ai.ref_price;
                const bool ok = router_.replace(
                    auction_order_id_, auction_order_.qty, new_limit);
                if (ok) {
                    auction_order_.limit_price = new_limit;
                    auction_order_.type        = OrderType::LOC;
                    ALGO_LOG(log_,
                        "[MOO][%.8s] REPLACED MOO→LOO  new_limit=%.4f",
                        params_.symbol, px_to_dbl(new_limit));
                }
                break;
            }
            case MOOImbalanceAction::CancelOrder: {
                router_.cancel(auction_order_id_);
                auction_order_.live = false;
                metrics_.orders_cancelled.fetch_add(1, std::memory_order_relaxed);
                moo_state_.store(MOOState::Cancelled, std::memory_order_release);
                ALGO_LOG(log_,
                    "[MOO][%.8s] CANCELLED due to adverse imbalance", params_.symbol);
                log_.flush();
                break;
            }
        }
    }

    COLD_PATH void finish_() noexcept {
        auction_order_.live = false;
        moo_state_.store(MOOState::Complete, std::memory_order_release);
        ALGO_LOG(log_, "[MOO][%.8s] COMPLETE  filled=%llu/%llu  px=%.4f",
            params_.symbol,
            (unsigned long long)metrics_.filled_qty.load(std::memory_order_relaxed),
            (unsigned long long)params_.total_qty,
            metrics_.filled_qty > 0
                ? static_cast<double>(metrics_.filled_value.load(std::memory_order_relaxed))
                  / (static_cast<double>(metrics_.filled_qty.load(std::memory_order_relaxed)) * PRICE_SCALE)
                : 0.0);
        log_.flush();
    }

    // ── Data members ─────────────────────────────────────────────────────────
    MOOParams                        params_;
    IOrderRouter&                    router_;
    std::atomic<MOOState>            moo_state_       {MOOState::Idle};
    uint64_t                         auction_order_id_{0};
    ChildOrder                       auction_order_{};

    CACHE_ALIGN std::atomic<Price>   last_bid_        {0};
    CACHE_ALIGN std::atomic<Price>   last_ask_        {0};
    CACHE_ALIGN std::atomic<Price>   last_ref_price_  {0};
    CACHE_ALIGN std::atomic<Qty>     last_imb_qty_    {0};
    CACHE_ALIGN std::atomic<uint8_t> last_imb_side_   {0};
    CACHE_ALIGN AlgoMetrics          metrics_;
    mutable     LogRing              log_;
};

// ════════════════════════════════════════════════════════════════════════════
// SIMULATION HARNESS
// ════════════════════════════════════════════════════════════════════════════

struct MockRouter final : public IOrderRouter {
    explicit MockRouter(MOOAlgo* a) : algo(a) {}
    uint64_t submit(const ChildOrder& o) override {
        const uint64_t id = ++next_id;
        std::printf("  [EXCHANGE] MOO order_id=%llu  %s  qty=%llu  type=%s  lim=%.4f\n",
            (unsigned long long)id,
            side_str(o.side),
            (unsigned long long)o.qty,
            o.type == OrderType::MOO ? "MOO" :
            o.type == OrderType::LOC ? "LOO" : "OTHER",
            px_to_dbl(o.limit_price));
        // Simulate fill at opening price at auction time
        fills.push_back({id,
            dbl_to_px(175.20),   // simulated opening print
            o.qty,               // fully filled at open
            rdtsc_now(),
            now_midnight_ns()});
        return id;
    }
    bool cancel(uint64_t id) override {
        std::printf("  [EXCHANGE] CANCEL order_id=%llu\n", (unsigned long long)id);
        return true;
    }
    bool replace(uint64_t id, Qty qty, Price px) override {
        std::printf("  [EXCHANGE] REPLACE order_id=%llu  qty=%llu  limit=%.4f\n",
            (unsigned long long)id, (unsigned long long)qty, px_to_dbl(px));
        return true;
    }
    void deliver() { for (auto& f : fills) algo->on_fill(f); fills.clear(); }

    MOOAlgo*                algo;
    uint64_t                next_id = 0;
    std::vector<FillReport> fills;
};

int main() {
    std::puts(
        "╔══════════════════════════════════════════════════════════╗\n"
        "║  MOO / LOO Execution Algorithm — ULL Simulation          ║\n"
        "╚══════════════════════════════════════════════════════════╝");
    calibrate_tsc();

    MOOParams p;
    p.side               = Side::Buy;
    p.total_qty          = 5'000;
    p.order_type         = OrderType::MOO;
    p.limit_price        = 0;
    p.imbalance_action   = MOOImbalanceAction::SwitchToLOO;
    p.imbalance_threshold= 150;   // act if imbalance >= 1.5× our qty

    // Use "now + offset" for demo (doesn't need to be real 9:28)
    const NsTime base = now_midnight_ns();
    p.submit_time_ns      = base + 1'000'000'000ULL;   // 1s from now
    p.cancel_deadline_ns  = base + 3'000'000'000ULL;   // 3s from now
    p.auction_time_ns     = base + 4'000'000'000ULL;   // 4s from now
    p.post_open_timeout_ns= base + 5'000'000'000ULL;   // 5s from now

    MockRouter router(nullptr);
    MOOAlgo    algo(p, router);
    router.algo = &algo;

    Quote q{}; std::memcpy(q.symbol, p.symbol, 8);
    q.bid_px = dbl_to_px(174.80); q.ask_px = dbl_to_px(175.20);
    algo.on_quote(q);
    algo.start();

    // Tick sequence: timer checks + one NOII at t=2s
    for (int tick = 0; tick <= 50; ++tick) {
        const NsTime t = base + static_cast<NsTime>(tick) * 100'000'000ULL; // 100ms ticks

        // Inject NOII at 2 seconds (buy-side imbalance — we are buying)
        if (tick == 20) {
            AuctionImbalance ai{};
            std::memcpy(ai.symbol, p.symbol, 8);
            ai.timestamp_ns   = t;
            ai.ref_price      = dbl_to_px(175.50);
            ai.imbalance_qty  = 8'000;   // 8K on buy side (1.6× our 5K)
            ai.imbalance_side = Side::Buy;
            std::puts("  [NOII] Buy-side imbalance 8000 @ ref=175.50");
            algo.on_imbalance(ai);
        }

        algo.on_timer(t);

        // Deliver fill when auction fires at t=4s
        if (tick == 40) router.deliver();
    }

    algo.print_summary();
    return 0;
}

