/**
 * ull_pair_dualcounter_strategies.cpp
 *
 * Extension of ull_etf_index_strategies.cpp — adds:
 *   SEC A : IOPC / iNAV / Index Future Fair Value — all formula variants
 *   SEC B : Strategy Parameters — full reference with importance ratings
 *   SEC C : Dual Counter Market Making strategy (CRTP, no virtual)
 *   SEC D : All major Pair Trading strategies (8 variants)
 *
 * BUILD (macOS):
 *   g++ -std=c++20 -O3 -march=native -DNDEBUG \
 *       ull_pair_dualcounter_strategies.cpp \
 *       -I/opt/homebrew/include -L/opt/homebrew/lib \
 *       -labsl_base -labsl_hash -labsl_raw_hash_set \
 *       -labsl_hashtablez_sampler -labsl_strings \
 *       -lpthread -lm -o ull_pair_strategies
 *
 * BUILD (RHEL 8/9):
 *   g++ -std=c++20 -O3 -march=native -DNDEBUG -D_GNU_SOURCE \
 *       ull_pair_dualcounter_strategies.cpp \
 *       -labsl_base -labsl_hash -labsl_raw_hash_set \
 *       -labsl_hashtablez_sampler -labsl_strings \
 *       -lpthread -lm -o ull_pair_strategies
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <thread>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <climits>
#include <deque>
#include <type_traits>
#include <pthread.h>
#include <sched.h>
#ifdef __linux__
#  include <sys/mman.h>
#  include <sys/resource.h>
#endif
#include "absl/container/flat_hash_map.h"
#include "absl/container/btree_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"

// ── Platform ─────────────────────────────────────────────────────────────────
#if defined(__x86_64__)
#  include <immintrin.h>
#  define CPU_PAUSE()       _mm_pause()
#  define PREFETCH_R(p)     __builtin_prefetch((p),0,3)
#  define PREFETCH_W(p)     __builtin_prefetch((p),1,3)
   inline uint64_t rdtsc() noexcept {
       uint32_t lo,hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
       return (uint64_t(hi)<<32)|lo;
   }
#elif defined(__aarch64__)
#  define CPU_PAUSE()       __asm__ volatile("yield":::"memory")
#  define PREFETCH_R(p)     __builtin_prefetch((p),0,3)
#  define PREFETCH_W(p)     __builtin_prefetch((p),1,3)
   inline uint64_t rdtsc() noexcept {
       uint64_t t; __asm__ volatile("mrs %0,cntvct_el0":"=r"(t)); return t;
   }
#else
#  define CPU_PAUSE()
#  define PREFETCH_R(p)
#  define PREFETCH_W(p)
   inline uint64_t rdtsc() noexcept { return 0; }
#endif

#define CACHE_LINE    64
#define CACHE_ALIGN   alignas(CACHE_LINE)
#define FORCE_INLINE  __attribute__((always_inline)) inline
#define HOT           __attribute__((hot))
#define COLD          __attribute__((cold))

// Fixed-point
static constexpr int64_t PRICE_SCALE = 1'000'000'000LL;
static constexpr int64_t BPS_SCALE   = 10'000LL;
constexpr int64_t to_fp(double p)    noexcept { return static_cast<int64_t>(p*PRICE_SCALE); }
constexpr double  from_fp(int64_t p) noexcept { return static_cast<double>(p)/PRICE_SCALE; }

// ── Shared structures (same as main file) ────────────────────────────────────
struct alignas(CACHE_LINE) Tick {
    uint64_t recv_tsc; uint32_t symbol_id; uint32_t seq;
    int64_t bid_fp; int64_t ask_fp; int64_t last_fp;
    uint32_t bid_qty; uint32_t ask_qty; uint32_t last_qty;
    uint8_t venue_id; uint8_t msg_type; char _pad[2];
    Tick() noexcept { std::memset(this,0,sizeof(*this)); }
};
struct alignas(CACHE_LINE) Order {
    uint64_t order_id; uint64_t strategy_id; uint64_t send_tsc;
    uint32_t instrument_id; int64_t price_fp; uint32_t qty;
    uint32_t remaining_qty; uint8_t side; uint8_t tif;
    uint8_t order_type; uint8_t venue_id; uint32_t _pad;
    Order() noexcept { std::memset(this,0,sizeof(*this)); }
};

// SeqLock (duplicated for self-contained file)
template<typename T>
class alignas(CACHE_LINE) SeqLock {
    CACHE_ALIGN std::atomic<uint64_t> seq_{0};
    CACHE_ALIGN T data_{};
public:
    FORCE_INLINE HOT void write(const T& v) noexcept {
        const uint64_t s=seq_.load(std::memory_order_relaxed);
        seq_.store(s+1,std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_release);
        data_=v;
        std::atomic_thread_fence(std::memory_order_release);
        seq_.store(s+2,std::memory_order_release);
    }
    FORCE_INLINE HOT bool read(T& out) const noexcept {
        for(int i=0;i<1024;++i){
            uint64_t s1=seq_.load(std::memory_order_acquire);
            if(s1&1){CPU_PAUSE();continue;}
            std::atomic_thread_fence(std::memory_order_acquire);
            out=data_;
            std::atomic_thread_fence(std::memory_order_acquire);
            uint64_t s2=seq_.load(std::memory_order_acquire);
            if(__builtin_expect(s1==s2,1))return true;
            CPU_PAUSE();
        }
        return false;
    }
};

template<typename T, size_t Cap>
class alignas(CACHE_LINE) SpscRing {
    static constexpr uint64_t MASK=Cap-1;
    struct alignas(CACHE_LINE) Cell {
        std::atomic<uint64_t> seq{0}; T data{};
        static constexpr size_t USED=sizeof(std::atomic<uint64_t>)+sizeof(T);
        static constexpr size_t PAD=(USED%CACHE_LINE)?(CACHE_LINE-USED%CACHE_LINE):0;
        char _pad[PAD];
    };
    CACHE_ALIGN std::atomic<uint64_t> enq_{0};
    CACHE_ALIGN std::atomic<uint64_t> deq_{0};
    CACHE_ALIGN std::array<Cell,Cap>   ring_;
public:
    SpscRing() noexcept { for(size_t i=0;i<Cap;++i) ring_[i].seq.store(i,std::memory_order_relaxed); }
    FORCE_INLINE HOT bool push(const T& item) noexcept {
        const uint64_t pos=enq_.load(std::memory_order_relaxed);
        Cell& c=ring_[pos&MASK];
        if(c.seq.load(std::memory_order_acquire)!=pos)return false;
        c.data=item;
        c.seq.store(pos+1,std::memory_order_release);
        enq_.store(pos+1,std::memory_order_relaxed);
        return true;
    }
    FORCE_INLINE HOT bool pop(T& out) noexcept {
        const uint64_t pos=deq_.load(std::memory_order_relaxed);
        Cell& c=ring_[pos&MASK];
        if(c.seq.load(std::memory_order_acquire)!=pos+1)return false;
        out=c.data;
        c.seq.store(pos+Cap,std::memory_order_release);
        deq_.store(pos+1,std::memory_order_relaxed);
        return true;
    }
    bool empty() const noexcept { return deq_.load(std::memory_order_acquire)==enq_.load(std::memory_order_acquire); }
};

// ============================================================================
// SECTION A — IOPC / iNAV / INDEX FUTURE FAIR VALUE — ALL FORMULAS
// ============================================================================
/*
 ┌──────────────────────────────────────────────────────────────────────────┐
 │ IOPC — Indicative Optimized Portfolio Composition                        │
 │                                                                          │
 │ Purpose: Tells APs (Authorized Participants) exactly WHICH stocks and    │
 │          HOW MANY shares to deliver for ETF creation/redemption.         │
 │          Published daily before market open by the ETF issuer.           │
 │                                                                          │
 │ Why important for trading:                                               │
 │  • IOPC reveals ETF's exact basket composition for the day              │
 │  • APs use it to construct creation/redemption baskets                  │
 │  • Market makers use it to compute precise iNAV (not approximate)       │
 │  • Deviations between IOPC and actual weights signal rebalance trades   │
 │                                                                          │
 │ IOPC fields (per constituent):                                           │
 │  symbol, shares_per_creation_unit, estimated_cash_component,            │
 │  accrued_interest (bonds), fx_rate (for international ETFs)             │
 │                                                                          │
 │ Creation Unit (CU): minimum block for AP creation/redemption            │
 │  • Equity ETF: typically 25,000–100,000 ETF shares per CU               │
 │  • Bond ETF:   typically 100,000–500,000 ETF shares per CU              │
 └──────────────────────────────────────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────┐
 │ iNAV — Indicative (Intraday) NAV                                         │
 │                                                                          │
 │ Published every 15 seconds by exchanges (NYSE Arca: INAV ticker)        │
 │ Market makers compute it MORE frequently internally (<100ms)            │
 │                                                                          │
 │ FORMULA VARIANTS:                                                        │
 │                                                                          │
 │ 1. BOTTOM-UP (full constituent sum) — most accurate:                    │
 │    iNAV = [Σᵢ (shares_i × price_i × fx_i) + cash_component]            │
 │           ──────────────────────────────────────────────────            │
 │                    ETF_shares_per_CU                                     │
 │                                                                          │
 │ 2. TOP-DOWN (index proxy) — fastest, for large ETFs (SP500, R3000):    │
 │    iNAV = Index_Level × ETF_Divisor × FX − accrued_fee_per_share        │
 │    ETF_Divisor = 1/divisor (e.g. SPY=1/10: SP500=5000 → SPY≈500)       │
 │    Avoids summing 500 stocks. <5ns per update vs ~100us bottom-up.      │
 │                                                                          │
 │ 3. DELTA UPDATE (production hot path):                                  │
 │    iNAV_new = iNAV_prev + weight_i × Δprice_i × fx_i                   │
 │    One multiply+add per tick = <5ns.                                    │
 │    Periodic full recalc every 1s to prevent drift.                      │
 │                                                                          │
 │ PRICE INPUTS (which price to use per leg):                              │
 │  iNAV_mid  = Σ(w × mid × fx)   ← general fair value                   │
 │  iNAV_bid  = Σ(w × bid × fx)   ← worst case if we SELL basket          │
 │              → use for ETF ASK quote (we sell ETF → buy basket at ask)  │
 │  iNAV_ask  = Σ(w × ask × fx)   ← worst case if we BUY basket           │
 │              → use for ETF BID quote (we buy ETF → sell basket at bid)  │
 │                                                                          │
 │ FX TREATMENT (for international ETFs):                                  │
 │  Spot FX updated continuously. iNAV in ETF base currency (usually USD). │
 │  FX hedge cost adds ~0.5-2 bps to effective spread.                    │
 │                                                                          │
 │ ACCRUED MANAGEMENT FEE:                                                 │
 │  Daily fee_bps = TER_bps / 252    (TER = Total Expense Ratio)           │
 │  Accrued = fee_bps × days_since_last_dist / 10000 × iNAV               │
 │  iNAV_adj = iNAV - accrued_fee                                         │
 └──────────────────────────────────────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────┐
 │ INDEX FUTURE FAIR VALUE — COST OF CARRY MODEL                           │
 │                                                                          │
 │ F_fair = S × exp((r − d) × T)                                           │
 │                                                                          │
 │ Components:                                                              │
 │  S = Spot index level (e.g. SP500 = 5280.00)                            │
 │  r = Risk-free rate (SOFR overnight, annualized e.g. 0.045 = 4.5%)     │
 │  d = Dividend yield (annualized, e.g. SP500 ≈ 0.015 = 1.5%)            │
 │  T = Time to expiry in years (e.g. 45 days = 45/252 = 0.1786)          │
 │                                                                          │
 │ DISCRETE CARRY WITH KNOWN DIVIDENDS (most accurate):                    │
 │  PV_Divs = Σ[ Div_i × exp(−r × t_i) ]  (PV of all dividends before T)  │
 │  F_fair  = (S − PV_Divs) × exp(r × T)                                  │
 │                                                                          │
 │ FAIR VALUE BASIS (FVB):                                                 │
 │  FVB = F_fair − S ≈ S × (r − d) × T                                    │
 │  FVB > 0: futures at PREMIUM  (typical: r > d, e.g. US rates > yield)  │
 │  FVB < 0: futures at DISCOUNT (high-div markets: APAC, UK)             │
 │                                                                          │
 │ MARKET BASIS:                                                           │
 │  Basis_bps = (F_market_mid − F_fair) / F_fair × 10000                  │
 │  Basis > 0: future RICH  → sell future, buy cash (Cash & Carry)        │
 │  Basis < 0: future CHEAP → buy future, sell cash (Reverse C&C)         │
 │                                                                          │
 │ IMPLIED REPO RATE:                                                      │
 │  repo_impl = ln(F_market / S) / T + d                                  │
 │  If repo_impl > actual_repo → future is RICH (expensive to finance)    │
 │  If repo_impl < actual_repo → future is CHEAP                          │
 │                                                                          │
 │ ROLL COST (calendar spread):                                            │
 │  Roll = F(T2) − F(T1) = S × [exp((r−d)×T2) − exp((r−d)×T1)]          │
 │  Negative roll = contango (typical for equity futures)                 │
 │  Positive roll = backwardation (rare for equity, common for commodities)│
 │                                                                          │
 │ TRADING THRESHOLDS (practical):                                         │
 │  Enter arb if |Basis_bps| > transaction_cost_bps + slippage_bps        │
 │  Typical cost: 2-4 bps total (commission + market impact each side)    │
 │  Minimum edge needed: 5-10 bps net                                     │
 └──────────────────────────────────────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────┐
 │ INDEX / BASKET FAIR VALUE                                                │
 │                                                                          │
 │ Index Level (reference):                                                │
 │  Price-weighted  (DJIA): Index = Σ(price_i) / divisor                  │
 │  Value-weighted  (SPX):  Index = Σ(price_i × shares_i) / base_value   │
 │  Equal-weighted  (RSP):  Index = Σ(price_i) / N × scale_factor         │
 │                                                                          │
 │ Custom Basket Fair Value:                                               │
 │  BV_mid = Σ(w_i × mid_i × fx_i)    ← delta-neutral hedging reference  │
 │  BV_bid = Σ(w_i × bid_i × fx_i)    ← worst-case liquidation value     │
 │  BV_ask = Σ(w_i × ask_i × fx_i)    ← worst-case entry cost            │
 │  Tracking_Error = BV − Benchmark   (minimize this via rebalancing)     │
 │                                                                          │
 │ Theoretical Value (with carry adjustments):                             │
 │  TV_i = price_i × exp((r_i − d_i) × T) × fx_i                         │
 │  Basket_TV = Σ(w_i × TV_i)                                             │
 │                                                                          │
 │ SPREAD CONSTRUCTION (for ETF quotes):                                   │
 │  ETF_bid = iNAV_ask − half_spread − inventory_skew − vol_premium       │
 │  ETF_ask = iNAV_bid + half_spread − inventory_skew + vol_premium       │
 │                                                                          │
 │  half_spread = max(MIN_SPREAD, constituent_spread/2 + our_edge)        │
 │  inventory_skew = SKEW_BPS_PER_LOT × net_inventory / 100              │
 │  vol_premium = EWMA_vol × vol_scale_factor                             │
 └──────────────────────────────────────────────────────────────────────────┘
*/

// ── Compile-time fair value engine (all formula variants) ────────────────────
struct FairValueEngine {
    // Cost of carry: F_fair = S × exp((r-d)×T)
    // T pre-computed at compile time for known expiry (e.g. quarterly options)
    template<int DaysToExpiry>
    static constexpr double carry_factor(double r, double d) noexcept {
        constexpr double T = DaysToExpiry / 252.0;
        // Note: std::exp is not constexpr in C++20; computed at init time
        return std::exp((r - d) * T);
    }

    // ── Full fair value (runtime) ─────────────────────────────────────────
    static double index_future_fair(double spot, double r, double d, double T) noexcept {
        return spot * std::exp((r - d) * T);
    }

    // ── Fair value with discrete known dividends ──────────────────────────
    // divs = vector of (days_to_div, div_amount)
    static double index_future_fair_div(double spot, double r, double T,
        absl::Span<const std::pair<double,double>> divs) noexcept {
        double pv_divs = 0.0;
        for (const auto& [t, div] : divs)
            pv_divs += div * std::exp(-r * t);
        return (spot - pv_divs) * std::exp(r * T);
    }

    // ── Implied repo rate from futures price ──────────────────────────────
    static double implied_repo(double futures_px, double spot, double d, double T) noexcept {
        return std::log(futures_px / spot) / T + d;
    }

    // ── Basis in bps ──────────────────────────────────────────────────────
    static int64_t basis_bps(int64_t futures_fp, int64_t fair_fp) noexcept {
        return (futures_fp - fair_fp) * BPS_SCALE / (fair_fp ? fair_fp : 1);
    }

    // ── Roll cost (calendar spread fair value) ────────────────────────────
    static double roll_fair(double spot, double r, double d, double T1, double T2) noexcept {
        return spot * (std::exp((r-d)*T2) - std::exp((r-d)*T1));
    }

    // ── iNAV bottom-up (full sum) ──────────────────────────────────────────
    static double inav(absl::Span<const double> weights,
                       absl::Span<const double> mids,
                       absl::Span<const double> fx_rates,
                       double cash, double etf_shares_per_cu) noexcept {
        const size_t n = std::min({weights.size(), mids.size(), fx_rates.size()});
        double sum = cash;
        for (size_t i = 0; i < n; ++i) sum += weights[i] * mids[i] * fx_rates[i];
        return sum / etf_shares_per_cu;
    }

    // ── iNAV top-down (index proxy — <5ns) ────────────────────────────────
    static double inav_topdown(double index_level, double etf_divisor,
                                double fx_usd, double accrued_fee) noexcept {
        return index_level * etf_divisor * fx_usd - accrued_fee;
    }

    // ── Accrued management fee ─────────────────────────────────────────────
    static double accrued_fee(double ter_bps, int days_since_dist, double inav_ref) noexcept {
        return ter_bps / 252.0 * days_since_dist / BPS_SCALE * inav_ref;
    }
};

// ============================================================================
// SECTION B — STRATEGY PARAMETERS: FULL REFERENCE TABLE WITH IMPORTANCE
// ============================================================================
/*
 ┌���─────────────────────────────────────────────────────────────────────────────
 │ ETF MARKET MAKING PARAMETERS
 ├────────────────────┬─────────────┬────────────────────────────────────────┤
 │ Parameter          │ Typical     │ Impact / Importance                    │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ BASE_SPREAD_BPS    │ 3-10 bps    │ ★★★★★ Primary revenue driver.          │
 │                    │             │   Too tight → losses from adverse sel. │
 │                    │             │   Too wide → order flow goes elsewhere │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ MIN_EDGE_BPS       │ 1-3 bps     │ ★★★★★ Don't quote if constituent spread │
 │                    │             │   > our ETF spread. Avoids free option. │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ SKEW_BPS_PER_LOT   │ 0.5-2 bps   │ ★★★★★ Inventory control. Reduces mean │
 │                    │             │   reversion delay. Must calibrate to   │
 │                    │             │   ADV of ETF.                          │
 ├────────────────────┼───���─────────┼────────────────────────────────────────┤
 │ HEDGE_THRESHOLD    │ 500-5000    │ ★★★★★ When to hedge. Smaller = lower   │
 │                    │             │   delta risk but higher transaction     │
 │                    │             │   cost. Balance with futures tick size. │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ VOL_SCALE_FACTOR   │ 10-50       │ ★★★★☆ Widen spread during volatile     │
 │                    │             │   markets. Prevents losses during       │
 │                    │             │   gap moves.                           │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ FADE_FACTOR_BPS    │ 1-5 bps     │ ★★★☆☆ Widen on order imbalance. Reduces│
 │                    │             │   adverse selection from informed flow. │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ QUOTE_SIZE         │ 500-10K     │ ★★★★☆ Market share driver. Larger size │
 │                    │             │   attracts more flow but increases risk.│
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ REQUOTE_COOLDOWN   │ 50-500 µs   │ ★★★☆☆ Min time between requotes.       │
 │                    │             │   Too fast → excessive order traffic.  │
 │                    │             │   Too slow → quotes stale in fast mkt. │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ HEDGEINSTRUMENT    │ F,B,N       │ ★★★★☆ F=futures (fast,liquid,cheap).   │
 │                    │             │   B=basket (exact,slow,expensive).     │
 │                    │             │   F preferred for intraday hedging.    │
 └────────────────────┴─────────────┴────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────────
 │ ETF ARBITRAGE PARAMETERS
 ├────────────────────┬─────────────┬────────────────────────────────────────┤
 │ ENTRY_PREMIUM_BPS  │ 5-20 bps    │ ★���★★★ Net alpha after all costs.       │
 │                    │             │   Must exceed: creation_fee +          │
 │                    │             │   transaction_cost + market_impact.    │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ CREATION_COST_BPS  │ 1-5 bps     │ ★★★★★ AP creation/redemption fee.      │
 │                    │             │   Fixed cost from ETF prospectus.      │
 ├────────────────────┼─────────────┼────────────────────────────────────────┤
 │ FILL_TIMEOUT_US    │ 100-2000 µs │ ★★★★☆ Cancel basket leg if unfilled.   │
 │                    │             │   Prevents legged risk (one side done, │
 │                    │             │   other not → directional exposure).   │
 └────────────────────┴─────────────┴────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────────
 │ INDEX ARBITRAGE PARAMETERS
 ├────────────────────┬─────────────┬────────────────────────────────────────┤
 │ ENTRY_BASIS_BPS    │ 3-10 bps    │ ★★★★★ Minimum net basis after costs.   │
 │ RISK_FREE_RATE     │ 4-5.5%      │ ★★★★★ Drives fair value. Update daily  │
 │                    │             │   from SOFR/OIS fixings.               │
 │ DIVIDEND_YIELD     │ 1-3%        │ ★★★★★ Reduces fair futures price.      │
 │                    │             │   Update from IBES consensus forecasts.│
 │ DAYS_TO_EXPIRY     │ 1-90 days   │ ★★★★★ Time decay of carry. Basis       │
 │                    │             │   converges to 0 at expiry.            │
 │ ROLL_DAYS_BEFORE   │ 3-7 days    │ ★★★☆☆ Start rolling to next contract.  │
 └────────────────────┴─────────────┴────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────────
 │ PAIR TRADING PARAMETERS
 ├────────────────────┬─────────────┬─────────���──────────────────────────────┤
 │ ZSCORE_ENTRY       │ 2.0-3.0 σ   │ ★★★★★ Entry signal threshold.          │
 │                    │             │   2σ: more trades, lower P&L per trade │
 │                    │             │   3σ: fewer trades, higher P&L/trade   │
 │ ZSCORE_EXIT        │ 0.0-0.5 σ   │ ★★★★★ Exit when spread mean-reverts.   │
 │ ZSCORE_STOP        │ 4.0-5.0 σ   │ ★★★★★ Stop-loss if spread diverges.    │
 │ LOOKBACK_WINDOW    │ 20-120 days │ ★★★★☆ Rolling window for mean/std.     │
 │                    │             │   Shorter: faster adapt, more noise.   │
 │ HEDGE_RATIO        │ 0.5-2.0 β   │ ★★★★★ Cointegration vector (OLS beta). │
 │                    │             │   Ensures market-neutral position.     │
 │ HALFLIFE_DAYS      │ 5-30 days   │ ★★★★☆ Mean-reversion speed (OU model). │
 │                    │             │   Trade only if halflife < position_T.  │
 │ MAX_HOLDING_DAYS   │ 5-20 days   │ ★★★☆☆ Force close after N days.        │
 └────────────────────┴─────────────┴────────────────────────────────────────┘

 ┌──────────────────────────────────────────────────────────────────────────────
 │ DUAL COUNTER MM PARAMETERS
 ├────────────────────┬─────────────┬────────────────────────────────────────┤
 │ COUNTER_A_VENUE    │ Primary     │ ★★★★★ Main execution venue (lowest bps)│
 │ COUNTER_B_VENUE    │ Dark/ATS    │ ★★���★★ Dark pool or ATS for size trades │
 │ CROSS_THRESHOLD    │ 2-5 bps     │ ★★★★★ When to net A vs B (reduce hedge)│
 │ COUNTER_SKEW_BPS   │ 0.5-2 bps   │ ★★★★☆ Widen B-side to encourage netting│
 │ MAX_NET_EXPOSURE   │ 500-5K      │ ★★★★★ Net of A+B inventory before hedge │
 └────────────────────┴─────────────┴────────────────────────────────────────┘
*/

// ============================================================================
// SECTION C — DUAL COUNTER MARKET MAKING STRATEGY
// ============================================================================
//
// CONCEPT:
//   Run TWO independent quote engines on the SAME instrument simultaneously:
//   Counter A — Lit venue (NYSE/NASDAQ): tight spread, visible quotes
//   Counter B — Dark pool/ATS (IEX/Liquidnet): wider spread, hidden size
//
// KEY INSIGHT:
//   If Counter A fills a BUY and Counter B fills a SELL at the same time:
//   → Net inventory change = 0 (fully crossed internally)
//   → No external hedge needed → saves 1-3 bps transaction cost
//   → Called "internal crossing" or "natural netting"
//
// MECHANICS:
//   1. Both counters quote independently (separate SPSC rings to gateways)
//   2. After each fill: check if A.inventory + B.inventory crosses threshold
//   3. If net exposure < CROSS_THRESHOLD: don't hedge (natural netting)
//   4. If net exposure > HEDGE_THRESHOLD: hedge the accumulated net
//
// DUAL COUNTER SPREAD CONSTRUCTION:
//   A_bid = iNAV_ask - SPREAD_A/2 - skew  (tight, compete on lit)
//   A_ask = iNAV_bid + SPREAD_A/2 - skew
//   B_bid = iNAV_ask - SPREAD_B/2 - skew  (wider, attract large natural flow)
//   B_ask = iNAV_bid + SPREAD_B/2 - skew
//   B-spread is WIDER → attracts natural, patient block flow (not HFTs)
//
// BENEFIT:
//   By maintaining two counters, you capture:
//   - HFT/algo flow on lit venue (Counter A)
//   - Institutional/natural flow on dark venue (Counter B)
//   Net exposure is frequently crossed internally → lower hedge cost.

struct DualCounterConfig {
    // Counter A (lit venue) parameters
    static constexpr int64_t  A_SPREAD_BPS       = 4;    // 0.4 bps each side
    static constexpr uint32_t A_QUOTE_SIZE        = 1000;
    static constexpr uint8_t  A_VENUE_ID          = 1;   // e.g. NYSE
    // Counter B (dark pool) parameters
    static constexpr int64_t  B_SPREAD_BPS        = 10;  // 1 bps each side
    static constexpr uint32_t B_QUOTE_SIZE        = 5000; // larger block size
    static constexpr uint8_t  B_VENUE_ID          = 2;   // e.g. IEX dark
    // Net position management
    static constexpr int64_t  MAX_LONG_QTY        = 100000;
    static constexpr int64_t  MAX_SHORT_QTY       = 100000;
    static constexpr int64_t  CROSS_THRESHOLD_QTY = 500;  // net below this: don't hedge
    static constexpr int64_t  HEDGE_THRESHOLD_QTY = 5000; // hedge when abs(net) > 5K
    static constexpr int64_t  SKEW_BPS_PER_LOT    = 1;
    static constexpr uint32_t FUTURES_INST_ID      = 9999;
    static constexpr bool     ENABLE_INTERNAL_CROSS = true;
};

template<typename Config = DualCounterConfig>
class DualCounterMM {
    // iNAV reference
    const SeqLock<struct iNavData>* inav_{nullptr};

    // ── Per-counter quote state (each on its own cache line) ─────────────
    struct alignas(CACHE_LINE) CounterState {
        int64_t  bid_fp{0};
        int64_t  ask_fp{0};
        int64_t  net_qty{0};     // net position from THIS counter's fills
        int64_t  pending_buys{0};
        int64_t  pending_sells{0};
        uint64_t last_quote_tsc{0};
        uint32_t quote_id{0};
        char     _pad[28];
    };

    // iNAV data for this file (self-contained)
    struct iNavData { int64_t mid_fp{0}; int64_t bid_fp{0}; int64_t ask_fp{0}; };

    CACHE_ALIGN CounterState ca_;   // Counter A: lit venue
    CACHE_ALIGN CounterState cb_;   // Counter B: dark pool
    CACHE_ALIGN struct {
        int64_t  total_net_qty{0};  // ca_.net_qty + cb_.net_qty
        int64_t  internally_crossed{0};
        int64_t  hedges_sent{0};
        uint64_t ticks_processed{0};
        char     _pad[32];
    } stats_;

    SpscRing<Order, 256>* ring_a_{nullptr};  // orders to lit venue
    SpscRing<Order, 256>* ring_b_{nullptr};  // orders to dark venue
    uint64_t strategy_id_{0};
    uint32_t inst_id_{0};
    int64_t  inav_mid_fp_{0};
    int64_t  inav_bid_fp_{0};
    int64_t  inav_ask_fp_{0};

public:
    COLD void init(SpscRing<Order,256>* ring_a, SpscRing<Order,256>* ring_b,
                   uint64_t sid, uint32_t inst) noexcept {
        ring_a_ = ring_a;
        ring_b_ = ring_b;
        strategy_id_ = sid;
        inst_id_ = inst;
    }

    // ── Hot path: tick arrives → update both counters ─────────────────────
    FORCE_INLINE HOT void on_tick(const Tick& t) noexcept {
        if (__builtin_expect(t.symbol_id != inst_id_, 0)) return;
        ++stats_.ticks_processed;

        // Use latest iNAV (or ETF bid/ask as proxy)
        inav_mid_fp_ = (t.bid_fp + t.ask_fp) / 2;
        inav_bid_fp_ = t.bid_fp;
        inav_ask_fp_ = t.ask_fp;

        // ── Counter A: tight spread on lit venue ──────────────────────────
        const int64_t net_qty = ca_.net_qty + cb_.net_qty;
        const int64_t skew_fp = net_qty / 100 * Config::SKEW_BPS_PER_LOT *
                                 inav_mid_fp_ / BPS_SCALE;

        // A-side quotes (tight, lit market)
        const int64_t a_half = inav_mid_fp_ * Config::A_SPREAD_BPS / BPS_SCALE / 2;
        const int64_t a_bid  = inav_ask_fp_ - a_half - skew_fp;
        const int64_t a_ask  = inav_bid_fp_ + a_half - skew_fp;

        // ── Counter B: wider spread on dark pool ──────────────────────────
        const int64_t b_half = inav_mid_fp_ * Config::B_SPREAD_BPS / BPS_SCALE / 2;
        const int64_t b_bid  = inav_ask_fp_ - b_half - skew_fp;
        const int64_t b_ask  = inav_bid_fp_ + b_half - skew_fp;

        // Send quotes if changed materially
        constexpr int64_t MIN_MOVE = PRICE_SCALE / 10000;
        if (std::abs(a_bid - ca_.bid_fp) > MIN_MOVE || std::abs(a_ask - ca_.ask_fp) > MIN_MOVE) {
            send_quote(*ring_a_, a_bid, a_ask, Config::A_QUOTE_SIZE, Config::A_VENUE_ID);
            ca_.bid_fp = a_bid; ca_.ask_fp = a_ask;
        }
        if (std::abs(b_bid - cb_.bid_fp) > MIN_MOVE || std::abs(b_ask - cb_.ask_fp) > MIN_MOVE) {
            send_quote(*ring_b_, b_bid, b_ask, Config::B_QUOTE_SIZE, Config::B_VENUE_ID);
            cb_.bid_fp = b_bid; cb_.ask_fp = b_ask;
        }

        // ── Check if net exposure needs hedging ───────────────────────────
        manage_net_exposure(net_qty, inav_mid_fp_);
    }

    // ── Fill handler: called when Counter A or B fills ────────────────────
    FORCE_INLINE HOT void on_fill_a(const Order& o) noexcept {
        if (o.side == 'B') ca_.net_qty += o.qty;
        else               ca_.net_qty -= o.qty;
        check_internal_cross();
    }

    FORCE_INLINE HOT void on_fill_b(const Order& o) noexcept {
        if (o.side == 'B') cb_.net_qty += o.qty;
        else               cb_.net_qty -= o.qty;
        check_internal_cross();
    }

    COLD void print_stats() const noexcept {
        std::cout << "  [DualCounterMM id=" << strategy_id_ << "]\n"
                  << "    ticks=" << stats_.ticks_processed
                  << " A_net=" << ca_.net_qty << " B_net=" << cb_.net_qty
                  << " total_net=" << (ca_.net_qty + cb_.net_qty)
                  << " internally_crossed=" << stats_.internally_crossed
                  << " hedges=" << stats_.hedges_sent << "\n";
    }

private:
    // ── Internal crossing: if A and B have opposing positions → net them ──
    // Example: CA filled BUY 1000, CB filled SELL 800 → net only 200 long
    //          No hedge needed for the crossed 800 → saves ~3 bps
    FORCE_INLINE HOT void check_internal_cross() noexcept {
        if constexpr (!Config::ENABLE_INTERNAL_CROSS) return;
        const int64_t net = ca_.net_qty + cb_.net_qty;
        const int64_t a_abs = std::abs(ca_.net_qty);
        const int64_t b_abs = std::abs(cb_.net_qty);
        // Opposing positions → crossing opportunity
        if ((ca_.net_qty > 0 && cb_.net_qty < 0) ||
            (ca_.net_qty < 0 && cb_.net_qty > 0)) {
            const int64_t cross_qty = std::min(a_abs, b_abs);
            stats_.internally_crossed += cross_qty;
            // In production: generate internal fill report, update P&L
            // The net remaining (|a| - |b|) still needs external hedge if large
        }
        (void)net;
    }

    // ── External hedge when combined net exceeds threshold ─────────────────
    FORCE_INLINE HOT void manage_net_exposure(int64_t net_qty, int64_t fair_fp) noexcept {
        const int64_t abs_net = std::abs(net_qty);
        if (__builtin_expect(abs_net <= Config::CROSS_THRESHOLD_QTY, 1)) return; // no hedge
        if (abs_net > Config::HEDGE_THRESHOLD_QTY) {
            // Send futures hedge
            const char side = (net_qty > 0) ? 'S' : 'B';
            Order o{};
            o.order_id      = ++stats_.hedges_sent;
            o.strategy_id   = strategy_id_;
            o.instrument_id = Config::FUTURES_INST_ID;
            o.price_fp      = fair_fp;
            o.qty           = static_cast<uint32_t>(std::min(abs_net, Config::HEDGE_THRESHOLD_QTY));
            o.side          = static_cast<uint8_t>(side);
            o.tif           = 'I';
            o.order_type    = 'L';
            ring_a_->push(o); // route hedge via Counter A (better liquidity)
        }
    }

    FORCE_INLINE void send_quote(SpscRing<Order,256>& ring, int64_t bid_fp,
                                  int64_t ask_fp, uint32_t qty, uint8_t venue) noexcept {
        // Bid side
        Order bid_o{};
        bid_o.strategy_id   = strategy_id_;
        bid_o.instrument_id = inst_id_;
        bid_o.price_fp = bid_fp; bid_o.qty = qty;
        bid_o.side = 'B'; bid_o.tif = 'D'; bid_o.order_type = 'L';
        bid_o.venue_id = venue;
        ring.push(bid_o);
        // Ask side
        Order ask_o{};
        ask_o.strategy_id   = strategy_id_;
        ask_o.instrument_id = inst_id_;
        ask_o.price_fp = ask_fp; ask_o.qty = qty;
        ask_o.side = 'S'; ask_o.tif = 'D'; ask_o.order_type = 'L';
        ask_o.venue_id = venue;
        ring.push(ask_o);
    }
};

// ============================================================================
// SECTION D — PAIR TRADING STRATEGIES (8 VARIANTS)
// ============================================================================
//
// COMMON FRAMEWORK:
//   All pair strategies use the same Ornstein-Uhlenbeck spread model:
//   dX_t = κ(μ - X_t)dt + σ dW_t
//   where X_t = spread between leg A and leg B (after hedge ratio adjustment)
//   κ = mean-reversion speed (halflife = ln(2)/κ)
//   μ = long-run mean of spread
//   σ = instantaneous volatility of spread
//
// Z-SCORE DRIVEN ENTRY/EXIT:
//   z = (X_t - μ) / σ_rolling
//   Enter LONG spread:   z < -ZSCORE_ENTRY  (spread too far below mean)
//   Enter SHORT spread:  z >  ZSCORE_ENTRY  (spread too far above mean)
//   Exit:                |z| < ZSCORE_EXIT
//   Stop-loss:           |z| > ZSCORE_STOP (spread diverging, not reverting)

// ── Generic rolling statistics (no heap alloc — fixed window using array) ──
template<size_t Window>
class RollingStats {
    std::array<double, Window> buf_{};
    size_t   head_{0};
    size_t   count_{0};
    double   sum_{0.0};
    double   sum_sq_{0.0};
public:
    FORCE_INLINE void push(double x) noexcept {
        if (count_ == Window) {
            // Remove oldest
            const double old = buf_[head_];
            sum_    -= old;
            sum_sq_ -= old * old;
        } else {
            ++count_;
        }
        buf_[head_] = x;
        sum_    += x;
        sum_sq_ += x * x;
        head_ = (head_ + 1) % Window;
    }
    double mean() const noexcept { return count_ > 0 ? sum_ / count_ : 0.0; }
    double variance() const noexcept {
        if (count_ < 2) return 0.0;
        const double m = mean();
        return sum_sq_ / count_ - m * m;
    }
    double std_dev() const noexcept { return std::sqrt(std::max(0.0, variance())); }
    double zscore(double x) const noexcept {
        const double sd = std_dev();
        return sd > 1e-12 ? (x - mean()) / sd : 0.0;
    }
    size_t count() const noexcept { return count_; }
    bool   warm()  const noexcept { return count_ == Window; }
};

// ── Base CRTP pair strategy ───────────────────────────────────────────────────
template<typename Derived, typename Config>
class PairBase {
protected:
    uint32_t  inst_a_{0};   // Leg A instrument ID (e.g. SPY)
    uint32_t  inst_b_{0};   // Leg B instrument ID (e.g. IVV)
    int64_t   mid_a_{0};    // latest mid price leg A
    int64_t   mid_b_{0};    // latest mid price leg B

    // Spread = mid_a - hedge_ratio × mid_b
    double   hedge_ratio_{1.0};  // β (from OLS regression) — keeps position market-neutral
    double   spread_{0.0};

    RollingStats<Config::LOOKBACK_WINDOW> stats_;

    SpscRing<Order,256>* ring_{nullptr};
    uint64_t strategy_id_{0};

    enum class PairPos { FLAT, LONG_SPREAD, SHORT_SPREAD };
    PairPos pos_{PairPos::FLAT};

    uint64_t orders_{0};
    uint64_t hedges_{0};
    uint64_t ticks_{0};

public:
    COLD void init(SpscRing<Order,256>* ring, uint64_t sid,
                   uint32_t a, uint32_t b, double hr = 1.0) noexcept {
        ring_ = ring; strategy_id_ = sid;
        inst_a_ = a; inst_b_ = b; hedge_ratio_ = hr;
    }

    FORCE_INLINE HOT void on_tick(const Tick& t) noexcept {
        ++ticks_;
        if      (t.symbol_id == inst_a_) mid_a_ = (t.bid_fp + t.ask_fp) / 2;
        else if (t.symbol_id == inst_b_) mid_b_ = (t.bid_fp + t.ask_fp) / 2;
        else return;
        if (__builtin_expect(mid_a_ == 0 || mid_b_ == 0, 0)) return;

        // Compute spread (using derived's formula if overridden)
        spread_ = static_cast<Derived*>(this)->compute_spread();
        stats_.push(spread_);
        if (__builtin_expect(!stats_.warm(), 0)) return;

        const double z = stats_.zscore(spread_);
        static_cast<Derived*>(this)->evaluate_signal(z);
    }

    // Default spread = price_a - β × price_b
    FORCE_INLINE double compute_spread() noexcept {
        return from_fp(mid_a_) - hedge_ratio_ * from_fp(mid_b_);
    }

    FORCE_INLINE void evaluate_signal(double z) noexcept {
        switch (pos_) {
        case PairPos::FLAT:
            if (z < -Config::ZSCORE_ENTRY) enter_long_spread();   // spread too low
            else if (z > Config::ZSCORE_ENTRY) enter_short_spread();
            break;
        case PairPos::LONG_SPREAD:
            if (z > -Config::ZSCORE_EXIT)  exit_pair();           // reverted
            else if (z < -Config::ZSCORE_STOP) stop_loss();       // diverged
            break;
        case PairPos::SHORT_SPREAD:
            if (z < Config::ZSCORE_EXIT)   exit_pair();
            else if (z > Config::ZSCORE_STOP) stop_loss();
            break;
        }
    }

    COLD void print_stats(const char* name) const noexcept {
        std::cout << "  [" << name << " id=" << strategy_id_
                  << "] ticks=" << ticks_ << " orders=" << orders_
                  << " spread_mean=" << std::fixed << std::setprecision(4)
                  << stats_.mean() << " spread_std=" << stats_.std_dev() << "\n";
    }

protected:
    // Long spread = buy A + sell β×B
    FORCE_INLINE void enter_long_spread() noexcept {
        submit('B', inst_a_, mid_a_, Config::POSITION_SIZE);
        submit('S', inst_b_, mid_b_, static_cast<uint32_t>(Config::POSITION_SIZE * hedge_ratio_));
        pos_ = PairPos::LONG_SPREAD; ++hedges_;
    }
    FORCE_INLINE void enter_short_spread() noexcept {
        submit('S', inst_a_, mid_a_, Config::POSITION_SIZE);
        submit('B', inst_b_, mid_b_, static_cast<uint32_t>(Config::POSITION_SIZE * hedge_ratio_));
        pos_ = PairPos::SHORT_SPREAD; ++hedges_;
    }
    FORCE_INLINE void exit_pair() noexcept {
        if (pos_ == PairPos::LONG_SPREAD) {
            submit('S', inst_a_, mid_a_, Config::POSITION_SIZE);
            submit('B', inst_b_, mid_b_, static_cast<uint32_t>(Config::POSITION_SIZE * hedge_ratio_));
        } else {
            submit('B', inst_a_, mid_a_, Config::POSITION_SIZE);
            submit('S', inst_b_, mid_b_, static_cast<uint32_t>(Config::POSITION_SIZE * hedge_ratio_));
        }
        pos_ = PairPos::FLAT;
    }
    FORCE_INLINE void stop_loss() noexcept {
        // Force exit — same as exit pair but log the loss
        exit_pair();
    }
    FORCE_INLINE void submit(char side, uint32_t inst, int64_t px_fp, uint32_t qty) noexcept {
        Order o{};
        o.order_id = ++orders_; o.strategy_id = strategy_id_;
        o.instrument_id = inst; o.price_fp = px_fp;
        o.qty = qty; o.side = static_cast<uint8_t>(side);
        o.tif = 'I'; o.order_type = 'L';
        ring_->push(o);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 1: ETF-ETF Pair (SPY vs IVV — both track SP500)
// Purpose: Exploit tracking error between two ETFs on same index.
//          SPY/IVV spread should be ~1 bps (tiny). Any wider → arb.
//          Edge: replication difference, AUM fees (SPY=0.095%, IVV=0.03%)
// ─────────────────────────────────────────────────────────────────────────────
struct EtfEtfConfig {
    static constexpr double   ZSCORE_ENTRY  = 2.5;  // tight: pairs trade narrow
    static constexpr double   ZSCORE_EXIT   = 0.3;
    static constexpr double   ZSCORE_STOP   = 4.0;
    static constexpr size_t   LOOKBACK_WINDOW = 60;   // 60 ticks rolling
    static constexpr uint32_t POSITION_SIZE   = 5000;
};
template<typename Config = EtfEtfConfig>
class EtfEtfPair : public PairBase<EtfEtfPair<Config>, Config> {
public:
    // Ratio spread: spread = log(price_a / price_b) → more stable than price diff
    FORCE_INLINE double compute_spread() noexcept {
        if (this->mid_a_ <= 0 || this->mid_b_ <= 0) return 0.0;
        return std::log(from_fp(this->mid_a_)) - std::log(from_fp(this->mid_b_));
    }
    void evaluate_signal(double z) noexcept {
        PairBase<EtfEtfPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 2: ETF-Futures Pair (SPY vs ES1 futures)
// Purpose: Same as index arb but implemented as a pair strategy framework.
//          Spread = ETF_price × multiplier - Futures_price
//          Should equal fair value basis. Trade deviations.
// ─────────────────────────────────────────────────────────────────────────────
struct EtfFutureConfig {
    static constexpr double   ZSCORE_ENTRY   = 2.0;
    static constexpr double   ZSCORE_EXIT    = 0.2;
    static constexpr double   ZSCORE_STOP    = 4.5;
    static constexpr size_t   LOOKBACK_WINDOW= 30;
    static constexpr uint32_t POSITION_SIZE  = 100;   // futures contracts
    static constexpr double   ETF_MULTIPLIER = 10.0;  // 10 ETF shares per futures
};
template<typename Config = EtfFutureConfig>
class EtfFuturesPair : public PairBase<EtfFuturesPair<Config>, Config> {
public:
    FORCE_INLINE double compute_spread() noexcept {
        return from_fp(this->mid_a_) * Config::ETF_MULTIPLIER - from_fp(this->mid_b_);
    }
    void evaluate_signal(double z) noexcept {
        PairBase<EtfFuturesPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────��───────────────────────────────────────────────
// PAIR VARIANT 3: Sector ETF Pair (XLK vs QQQ — both tech-heavy)
// Purpose: Trade relative performance between two overlapping sector ETFs.
//          XLK = pure GICS Tech sector; QQQ = Nasdaq-100 (70% tech).
//          When tech sells off: QQQ drops more than XLK (broader index noise)
//          Trade the relative outperformance using OLS hedge ratio.
// ─────────────────────────────────────────────────────────────────────────────
struct SectorPairConfig {
    static constexpr double   ZSCORE_ENTRY   = 2.0;
    static constexpr double   ZSCORE_EXIT    = 0.5;
    static constexpr double   ZSCORE_STOP    = 4.0;
    static constexpr size_t   LOOKBACK_WINDOW= 40;
    static constexpr uint32_t POSITION_SIZE  = 2000;
};
template<typename Config = SectorPairConfig>
class SectorEtfPair : public PairBase<SectorEtfPair<Config>, Config> {
public:
    void evaluate_signal(double z) noexcept {
        PairBase<SectorEtfPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 4: Geographic ETF Pair (EWJ vs EWG — Japan vs Germany)
// Purpose: Trade relative country performance. Spread driven by FX, macro,
//          rates differentials. Mean-reverts over weeks-months.
// Extra: FX-adjust both legs to USD before computing spread.
// ─────────────────────────────────────────────────────────────────────────────
struct GeoPairConfig {
    static constexpr double   ZSCORE_ENTRY   = 2.0;
    static constexpr double   ZSCORE_EXIT    = 0.5;
    static constexpr double   ZSCORE_STOP    = 4.0;
    static constexpr size_t   LOOKBACK_WINDOW= 60;
    static constexpr uint32_t POSITION_SIZE  = 10000;
};
template<typename Config = GeoPairConfig>
class GeographicEtfPair : public PairBase<GeographicEtfPair<Config>, Config> {
    double fx_a_{1.0};   // FX rate leg A (e.g. JPY/USD for EWJ)
    double fx_b_{1.0};   // FX rate leg B (e.g. EUR/USD for EWG)
public:
    COLD void set_fx(double fx_a, double fx_b) noexcept { fx_a_=fx_a; fx_b_=fx_b; }
    // FX-adjusted spread: both legs in USD
    FORCE_INLINE double compute_spread() noexcept {
        return from_fp(this->mid_a_) * fx_a_ - this->hedge_ratio_ * from_fp(this->mid_b_) * fx_b_;
    }
    void evaluate_signal(double z) noexcept {
        PairBase<GeographicEtfPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 5: Factor ETF Pair (IVW Growth vs IVE Value — SP500 split)
// Purpose: Trade value vs growth rotation. Spread reflects risk premium shift.
//          In rising rate env: value outperforms → spread mean-reverts upward.
//          Seasonal patterns: value/growth rotate on macro cycle.
// ──────────────────────────────────────────────────────────��──────────────────
struct FactorPairConfig {
    static constexpr double   ZSCORE_ENTRY   = 1.8; // slightly looser — slower reverting
    static constexpr double   ZSCORE_EXIT    = 0.5;
    static constexpr double   ZSCORE_STOP    = 3.5;
    static constexpr size_t   LOOKBACK_WINDOW= 80;
    static constexpr uint32_t POSITION_SIZE  = 5000;
};
template<typename Config = FactorPairConfig>
class FactorEtfPair : public PairBase<FactorEtfPair<Config>, Config> {
public:
    void evaluate_signal(double z) noexcept {
        PairBase<FactorEtfPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 6: Duration ETF Pair (TLT 20yr vs IEF 7-10yr bonds)
// Purpose: Trade the yield curve shape (2s10s spread proxy via ETFs).
//          Spread = TLT_return - β × IEF_return (duration-adjusted)
//          β = TLT_duration / IEF_duration ≈ 18/8 = 2.25 (duration-neutral)
//          Mean-reverts when yield curve returns to normal shape.
// ─────────────────────────────────────────────────────────────────────────────
struct DurationPairConfig {
    static constexpr double   ZSCORE_ENTRY   = 2.0;
    static constexpr double   ZSCORE_EXIT    = 0.5;
    static constexpr double   ZSCORE_STOP    = 4.0;
    static constexpr size_t   LOOKBACK_WINDOW= 30;
    static constexpr uint32_t POSITION_SIZE  = 5000;
    static constexpr double   DURATION_RATIO = 2.25; // TLT_dur/IEF_dur
};
template<typename Config = DurationPairConfig>
class DurationEtfPair : public PairBase<DurationEtfPair<Config>, Config> {
public:
    COLD void init_duration(SpscRing<Order,256>* ring, uint64_t sid,
                             uint32_t a, uint32_t b) noexcept {
        this->init(ring, sid, a, b, Config::DURATION_RATIO);
    }
    void evaluate_signal(double z) noexcept {
        PairBase<DurationEtfPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 7: Commodity vs Equity Pair (GLD vs GDX — gold vs gold miners)
// Purpose: Gold miners should track spot gold × leverage.
//          GDX has 1.5-2.0× leverage to GLD historically.
//          Spread = log(GDX) - leverage × log(GLD)
//          Dislocations: miners underperform when energy costs spike.
// ─────────────────────────────────────────────────────────────────────────────
struct CommodityEquityConfig {
    static constexpr double   ZSCORE_ENTRY   = 2.0;
    static constexpr double   ZSCORE_EXIT    = 0.3;
    static constexpr double   ZSCORE_STOP    = 4.0;
    static constexpr size_t   LOOKBACK_WINDOW= 50;
    static constexpr uint32_t POSITION_SIZE  = 3000;
    static constexpr double   LEVERAGE       = 1.7; // GDX beta to GLD
};
template<typename Config = CommodityEquityConfig>
class CommodityEquityPair : public PairBase<CommodityEquityPair<Config>, Config> {
public:
    FORCE_INLINE double compute_spread() noexcept {
        if (this->mid_a_ <= 0 || this->mid_b_ <= 0) return 0.0;
        return std::log(from_fp(this->mid_a_)) - Config::LEVERAGE * std::log(from_fp(this->mid_b_));
    }
    void evaluate_signal(double z) noexcept {
        PairBase<CommodityEquityPair<Config>,Config>::evaluate_signal(z);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PAIR VARIANT 8: Volatility Pair (VXX short-term vs VXMT mid-term VIX)
// Purpose: Trade the VIX term structure. In normal markets: VXX < VXMT
//          (contango → VXX has negative roll yield → perpetual headwind).
//          During stress: curve inverts (backwardation) → VXX spikes more.
//          Spread reverts after each vol spike.
// ─────────────────────────────────────────────────────────────────────────────
struct VolPairConfig {
    static constexpr double   ZSCORE_ENTRY   = 2.0;
    static constexpr double   ZSCORE_EXIT    = 0.3;
    static constexpr double   ZSCORE_STOP    = 5.0;  // vol can blow through
    static constexpr size_t   LOOKBACK_WINDOW= 20;   // shorter: vol regime changes
    static constexpr uint32_t POSITION_SIZE  = 2000;
};
template<typename Config = VolPairConfig>
class VolatilityPair : public PairBase<VolatilityPair<Config>, Config> {
public:
    void evaluate_signal(double z) noexcept {
        PairBase<VolatilityPair<Config>,Config>::evaluate_signal(z);
    }
};

// ============================================================================
// SECTION D9 — PAIR STRATEGY PARAMETER SUMMARY (printed at runtime)
// ============================================================================
void print_pair_summary() noexcept {
    std::cout << "\n╔════════════════════════════════════════════════���═════════════════════╗\n"
              << "║  ALL PAIR STRATEGIES — SUMMARY                                       ║\n"
              << "╠══════════════════════════════════════════════════════════════════════╣\n"
              << "║  #  │ Name              │ Instruments        │ Spread          │WinHz ║\n"
              << "╠══════════════════════════════════════════════════════════════════════╣\n"
              << "║  1  │ ETF-ETF           │ SPY vs IVV         │ log(A/B)        │ min  ║\n"
              << "║  2  │ ETF-Futures       │ SPY vs ES1         │ A×mult - B      │ secs ║\n"
              << "║  3  �� Sector ETF        │ XLK vs QQQ         │ A - β×B         │ min  ║\n"
              << "║  4  │ Geographic ETF    │ EWJ vs EWG         │ A×fx_a - β×B×fx │ days ║\n"
              << "║  5  │ Factor ETF        │ IVW vs IVE         │ A - β×B         │ days ║\n"
              << "║  6  │ Duration ETF      │ TLT vs IEF         │ A - dur×B       │ days ║\n"
              << "║  7  │ Commodity-Equity  │ GLD vs GDX         │ log(A)-lev×log│ wks  ║\n"
              << "║  8  │ Volatility        │ VXX vs VXMT        │ A - β×B         │ min  ║\n"
              << "╠══════════════════════════════════════════════════════════════════════╣\n"
              << "║  Z-Entry: 2.0-2.5σ │ Z-Exit: 0.3-0.5σ │ Z-Stop: 3.5-5.0σ         ║\n"
              << "║  Hedge ratio (β) from OLS regression. Update weekly.                ║\n"
              << "║  All strategies: CRTP (no virtual), SpscRing orders, fixed-point    ║\n"
              << "╚══════════════════════════════════════════════════════════════════════╝\n\n";
}

// ============================================================================
// MAIN — DEMO ALL NEW STRATEGIES
// ============================================================================

int main() {
    std::cout <<
    "╔══════════════════════════════════════════════════════════════════════╗\n"
    "║  IOPC/iNAV, Dual Counter MM, 8 Pair Strategies �� ULL Demo          ║\n"
    "║  C++20 | CRTP | SeqLock | SPSC | Fixed-Point | RHEL 8/9 compat    ║\n"
    "╚══════════════════════════════════════════════════════════════════════╝\n\n";

    // ── Fair value demo ───────────────────────────────────────────────────
    std::cout << "=== FAIR VALUE CALCULATIONS ===\n";
    const double spot = 5280.0, r = 0.045, d = 0.015, T = 45.0/252.0;
    const double f_fair   = FairValueEngine::index_future_fair(spot, r, d, T);
    const double fvb      = f_fair - spot;
    const double repo_impl= FairValueEngine::implied_repo(f_fair * 1.0005, spot, d, T);
    const double roll      = FairValueEngine::roll_fair(spot, r, d, T, T + 91.0/252.0);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  SP500 spot         : " << spot << "\n";
    std::cout << "  F_fair (45d)       : " << f_fair << "\n";
    std::cout << "  Fair Value Basis   : " << fvb << " (" << fvb/spot*10000 << " bps)\n";
    std::cout << "  Implied repo       : " << repo_impl * 100 << " %\n";
    std::cout << "  Roll cost (to next): " << roll << " (" << roll/spot*10000 << " bps)\n\n";

    // iNAV example
    const std::array<double,5> weights  = {0.30,0.25,0.15,0.20,0.10};
    const std::array<double,5> mids     = {189.51,335.12,3510.25,140.22,175.82};
    const std::array<double,5> fx_rates = {1.0,1.0,1.0,1.0,1.0};
    const double inav_val = FairValueEngine::inav(
        absl::MakeConstSpan(weights), absl::MakeConstSpan(mids),
        absl::MakeConstSpan(fx_rates), 0.0, 1.0);
    std::cout << "  iNAV (5-stock wt) : " << inav_val << "\n";
    const double inav_topdown = FairValueEngine::inav_topdown(5280.0, 1.0/10.0, 1.0, 0.02);
    std::cout << "  iNAV top-down (SPX/10): " << inav_topdown << "\n";
    const double fee = FairValueEngine::accrued_fee(9.5, 15, 528.0); // SPY TER=0.095%
    std::cout << "  Accrued fee (15d) : " << fee << " (" << fee/528.0*10000 << " bps)\n\n";

    // ── Static allocation (BSS, no stack overflow) ────────────────────────
    static SpscRing<Order,256> ring_a, ring_b, ring_pair;

    // ── Dual Counter MM ───────────────────────────────────────────────────
    std::cout << "=== DUAL COUNTER MARKET MAKING ===\n";
    static DualCounterMM<DualCounterConfig> dc_mm;
    dc_mm.init(&ring_a, &ring_b, 10, 5000);

    const size_t DC_TICKS = 500'000;
    const auto dc_t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < DC_TICKS; ++i) {
        Tick t;
        t.symbol_id = 5000;
        t.bid_fp = to_fp(499.95 + (i % 50) * 0.01);
        t.ask_fp = t.bid_fp + to_fp(0.02);
        t.bid_qty = 1000 + (i % 500);
        t.ask_qty = 800  + (i % 600);
        dc_mm.on_tick(t);
    }
    const auto dc_t1 = std::chrono::steady_clock::now();
    const double dc_ms = std::chrono::duration<double,std::milli>(dc_t1-dc_t0).count();
    std::cout << "  " << DC_TICKS << " ticks in " << dc_ms << " ms ("
              << DC_TICKS/dc_ms/1000.0 << " M/s)\n";
    dc_mm.print_stats();
    std::cout << "\n";

    // ── Pair Strategies ───────────────────────────────────────────────────
    std::cout << "=== 8 PAIR STRATEGIES SIMULATION ===\n";
    static EtfEtfPair<>         p1; p1.init(&ring_pair, 1, 5001, 5002, 1.0);
    static EtfFuturesPair<>     p2; p2.init(&ring_pair, 2, 5001, 9999, 1.0);
    static SectorEtfPair<>      p3; p3.init(&ring_pair, 3, 6001, 6002, 0.85);
    static GeographicEtfPair<>  p4; p4.init(&ring_pair, 4, 7001, 7002, 1.1);
    static FactorEtfPair<>      p5; p5.init(&ring_pair, 5, 8001, 8002, 0.92);
    static DurationEtfPair<>    p6; p6.init_duration(&ring_pair, 6, 9001, 9002);
    static CommodityEquityPair<>p7; p7.init(&ring_pair, 7, 3001, 3002, CommodityEquityConfig::LEVERAGE);
    static VolatilityPair<>     p8; p8.init(&ring_pair, 8, 4001, 4002, 0.75);

    // Simulate ticks to warm up and generate signals
    constexpr size_t PAIR_TICKS = 200'000;
    const auto pt0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < PAIR_TICKS; ++i) {
        // Alternate between leg A and leg B for each pair
        const uint32_t syms[] = {5001,5002,9999,6001,6002,7001,7002,
                                  8001,8002,9001,9002,3001,3002,4001,4002};
        Tick t;
        t.symbol_id = syms[i % 15];
        // Simulate a mean-reverting spread: price oscillates around mean
        const double noise  = std::sin(i * 0.01) * 2.0 + (i % 7) * 0.1;
        t.bid_fp = to_fp(100.0 + noise);
        t.ask_fp = t.bid_fp + to_fp(0.02);
        t.recv_tsc = rdtsc();

        p1.on_tick(t); p2.on_tick(t); p3.on_tick(t); p4.on_tick(t);
        p5.on_tick(t); p6.on_tick(t); p7.on_tick(t); p8.on_tick(t);
    }
    const auto pt1 = std::chrono::steady_clock::now();
    const double pt_ms = std::chrono::duration<double,std::milli>(pt1-pt0).count();
    std::cout << "  " << PAIR_TICKS << " ticks × 8 pairs in " << pt_ms << " ms ("
              << PAIR_TICKS*8.0/pt_ms/1000.0 << " M effective ticks/s)\n\n";

    p1.print_stats("ETF-ETF        (SPY/IVV)");
    p2.print_stats("ETF-Futures    (SPY/ES1)");
    p3.print_stats("Sector ETF     (XLK/QQQ)");
    p4.print_stats("Geographic ETF (EWJ/EWG)");
    p5.print_stats("Factor ETF     (IVW/IVE)");
    p6.print_stats("Duration ETF   (TLT/IEF)");
    p7.print_stats("Commodity-Eq   (GLD/GDX)");
    p8.print_stats("Volatility     (VXX/VXMT)");

    print_pair_summary();
    return 0;
}

