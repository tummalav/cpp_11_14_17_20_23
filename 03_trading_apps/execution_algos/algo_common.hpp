/**
 * algo_common.hpp — Shared ULL types & utilities for execution algos
 *
 * ULL rules applied throughout:
 *   ✓ Zero hot-path heap allocation — fixed-size arrays everywhere
 *   ✓ alignas(64) on every hot structure — no false sharing
 *   ✓ FORCE_INLINE / HOT_PATH / COLD_PATH attributes
 *   ✓ RDTSC-based timing — no clock_gettime syscall in hot path
 *   ✓ Integer prices (× 10000) — no float in hot path
 *   ✓ Lock-free SPSC log ring — feed-handler thread never blocks on I/O
 *   ✓ Explicit memory_order — never seq_cst in hot path
 *   ✓ CPU_PAUSE() in spin loops
 *   ✓ __builtin_expect on unlikely branches
 */
#pragma once

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <chrono>

// ════════════════════════════════════════════════════════════════════════════
// PLATFORM MACROS
// ════════════════════════════════════════════════════════════════════════════

#define CACHE_LINE   64
#define CACHE_ALIGN  alignas(CACHE_LINE)
#define FORCE_INLINE __attribute__((always_inline)) inline
#define HOT_PATH     __attribute__((hot))
#define COLD_PATH    __attribute__((cold))

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#  define CPU_PAUSE() _mm_pause()
   FORCE_INLINE uint64_t rdtsc_now() noexcept {
       uint32_t lo, hi;
       __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
       return (static_cast<uint64_t>(hi) << 32) | lo;
   }
#elif defined(__aarch64__)
#  define CPU_PAUSE() __asm__ volatile("yield" ::: "memory")
   FORCE_INLINE uint64_t rdtsc_now() noexcept {
       uint64_t t;
       __asm__ volatile("mrs %0, cntvct_el0" : "=r"(t));
       return t;
   }
#else
#  define CPU_PAUSE() do {} while(0)
   FORCE_INLINE uint64_t rdtsc_now() noexcept {
       return static_cast<uint64_t>(
           std::chrono::steady_clock::now().time_since_epoch().count());
   }
#endif

// ════════════════════════════════════════════════════════════════════════════
// PRIMITIVE TYPES
// ════════════════════════════════════════════════════════════════════════════

using Price  = uint32_t;   // price × 10000  (4 decimal places, no float)
using Qty    = uint64_t;   // shares / contracts
using NsTime = uint64_t;   // nanoseconds since midnight

static constexpr Price PRICE_SCALE = 10000;

FORCE_INLINE double   px_to_dbl(Price p) noexcept { return p / double(PRICE_SCALE); }
FORCE_INLINE Price    dbl_to_px(double p) noexcept {
    return static_cast<Price>(p * PRICE_SCALE + 0.5);
}
// Integer midpoint — no float division
FORCE_INLINE Price    midpoint(Price bid, Price ask) noexcept { return (bid + ask) >> 1; }

FORCE_INLINE NsTime hhmm_ns(int hh, int mm, int ss = 0) noexcept {
    return static_cast<NsTime>(hh) * 3'600'000'000'000ULL
         + static_cast<NsTime>(mm) *    60'000'000'000ULL
         + static_cast<NsTime>(ss) *     1'000'000'000ULL;
}
FORCE_INLINE NsTime now_midnight_ns() noexcept {
    using namespace std::chrono;
    const auto now      = system_clock::now();
    const auto today_s  = duration_cast<seconds>(now.time_since_epoch());
    const auto midnight = seconds(today_s.count() - today_s.count() % 86400);
    return static_cast<NsTime>(
        duration_cast<nanoseconds>(now.time_since_epoch() -
            duration_cast<nanoseconds>(midnight)).count());
}

// ════════════════════════════════════════════════════════════════════════════
// ENUMERATIONS
// ════════════════════════════════════════════════════════════════════════════

enum class Side        : uint8_t { Buy=0, Sell=1 };
enum class OrderType   : uint8_t { Limit=0, Market=1, MOO=2, MOC=3, LOC=4 };
enum class TimeInForce : uint8_t { Day=0, IOC=1, GTC=2, AUC=3 };  // AUC=auction
enum class AlgoState   : uint8_t { Idle=0, Active=1, Paused=2, Complete=3, Cancelled=4 };

static inline const char* side_str(Side s)      { return s==Side::Buy?"BUY":"SELL"; }
static inline const char* state_str(AlgoState s) {
    switch(s){
        case AlgoState::Idle:      return "IDLE";
        case AlgoState::Active:    return "ACTIVE";
        case AlgoState::Paused:    return "PAUSED";
        case AlgoState::Complete:  return "COMPLETE";
        case AlgoState::Cancelled: return "CANCELLED";
    }
    return "?";
}

// ════════════════════════════════════════════════════════════════════════════
// MARKET DATA STRUCTURES  — each fits in exactly 1 cache line (64 bytes)
// ════════════════════════════════════════════════════════════════════════════

struct alignas(CACHE_LINE) MarketTrade {
    NsTime   timestamp_ns;
    Price    price;
    Qty      qty;
    uint64_t exchange_seq;
    Side     aggressor;
    char     symbol[8];
    uint8_t  _pad[2];

    void clear() noexcept { std::memset(this, 0, sizeof(*this)); }
};
static_assert(sizeof(MarketTrade) <= CACHE_LINE, "MarketTrade > cache line");

struct alignas(CACHE_LINE) Quote {
    NsTime   timestamp_ns;
    Price    bid_px;
    Price    ask_px;
    Qty      bid_qty;
    Qty      ask_qty;
    uint64_t exchange_seq;
    char     symbol[8];
    uint8_t  _pad[4];
};
static_assert(sizeof(Quote) <= CACHE_LINE, "Quote > cache line");

// Opening/Closing auction imbalance message (NYSE NOII / NASDAQ ITCH)
struct alignas(CACHE_LINE) AuctionImbalance {
    NsTime   timestamp_ns;
    Price    ref_price;           // indicative auction price
    Price    far_price;           // far-touch clearing price
    Price    near_price;          // near-touch clearing price
    Qty      paired_qty;          // matched (will fill) at ref_price
    Qty      imbalance_qty;       // excess on one side
    Side     imbalance_side;      // which side has excess
    char     symbol[8];
    uint8_t  _pad[5];
};
static_assert(sizeof(AuctionImbalance) <= CACHE_LINE, "AuctionImbalance > cache line");

// ════════════════════════════════════════════════════════════════════════════
// CHILD ORDER  — all algo-submitted child orders use this struct
// ════════════════════════════════════════════════════════════════════════════

struct alignas(CACHE_LINE) ChildOrder {
    uint64_t    order_id;
    Price       limit_price;      // 0 = market
    Qty         qty;
    Qty         filled_qty;
    uint64_t    submit_tsc;       // rdtsc at submit — for latency measurement
    Side        side;
    OrderType   type;
    TimeInForce tif;
    char        symbol[8];
    bool        live;
    uint8_t     _pad[5];
};
static_assert(sizeof(ChildOrder) == CACHE_LINE, "ChildOrder must be exactly 1 cache line");

struct alignas(CACHE_LINE) FillReport {
    uint64_t order_id;
    Price    fill_price;
    Qty      fill_qty;
    uint64_t fill_tsc;
    NsTime   fill_time_ns;
    uint8_t  _pad[20];
};
static_assert(sizeof(FillReport) == CACHE_LINE, "FillReport must be exactly 1 cache line");

// ════════════════════════════════════════════════════════════════════════════
// ORDER ROUTER INTERFACE  (inject production FIX/OUCH router)
// ════════════════════════════════════════════════════════════════════════════

class IOrderRouter {
public:
    virtual ~IOrderRouter() = default;
    // Returns assigned order_id (>0 on success, 0 on failure)
    virtual uint64_t submit (const ChildOrder& o)                        = 0;
    virtual bool     cancel (uint64_t order_id)                          = 0;
    virtual bool     replace(uint64_t order_id, Qty new_qty, Price new_limit) = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// ROLLING VOLUME BUFFER
//
// Fixed-size circular buffer tracking (timestamp, qty) events.
// Evicts entries older than window_ns on every query — O(k) where k = evicted.
// Zero heap allocation. Replaces std::deque<> used in naive implementations.
//
// CAP: power-of-2 capacity.  1 minute of volume at 10K trades/s → 600K events.
// For typical 60s windows at 1K trades/s → 64K is sufficient.
// ════════════════════════════════════════════════════════════════════════════

template<size_t CAP>
class alignas(CACHE_LINE) RollingVolBuf {
    static_assert((CAP & (CAP-1)) == 0, "CAP must be power-of-2");
    static constexpr uint32_t MASK = static_cast<uint32_t>(CAP - 1);

    struct Entry { NsTime ts; Qty qty; };

    CACHE_ALIGN Entry    buf_[CAP]{};
    uint32_t             head_   = 0;   // index of oldest entry
    uint32_t             tail_   = 0;   // index of next-write slot
    Qty                  sum_    = 0;
    NsTime               window_ = 0;

public:
    explicit RollingVolBuf(NsTime window_ns) noexcept : window_ (window_ns) {}

    FORCE_INLINE HOT_PATH void push(NsTime ts, Qty qty) noexcept {
        // If full, evict oldest to make room (overwrite oldest entry)
        if (__builtin_expect((tail_ - head_) >= static_cast<uint32_t>(CAP), 0)) {
            sum_ -= buf_[head_ & MASK].qty;
            ++head_;
        }
        buf_[tail_ & MASK] = {ts, qty};
        ++tail_;
        sum_ += qty;
    }

    // Returns total volume within the rolling window; evicts stale entries.
    FORCE_INLINE HOT_PATH Qty query(NsTime now_ns) noexcept {
        const NsTime cutoff = (now_ns > window_) ? now_ns - window_ : 0;
        while (head_ != tail_ && buf_[head_ & MASK].ts < cutoff) {
            sum_ -= buf_[head_ & MASK].qty;
            ++head_;
        }
        return sum_;
    }

    // Subtract algo's own fills from window (self-trade dedup for POV)
    FORCE_INLINE void subtract(Qty qty) noexcept {
        sum_ = (sum_ > qty) ? sum_ - qty : 0;
    }

    void reset() noexcept { head_ = tail_ = 0; sum_ = 0; }

    uint32_t count() const noexcept { return tail_ - head_; }
};

// ════════════════════════════════════════════════════════════════════════════
// FIXED-SIZE ORDER POOL
//
// Pre-allocated array of ChildOrder.  alloc() returns a pointer to the next
// free slot; never calls new/malloc.  Reclaim by setting order.live = false.
// ════════════════════════════════════════════════════════════════════════════

template<size_t N = 256>
class alignas(CACHE_LINE) OrderPool {
public:
    // Returns next free slot, nullptr if pool full (algo bug — N too small)
    FORCE_INLINE ChildOrder* alloc() noexcept {
        for (uint32_t i = 0; i < N; ++i) {
            uint32_t idx = (next_ + i) % N;
            if (!pool_[idx].live) {
                std::memset(&pool_[idx], 0, sizeof(ChildOrder));
                next_ = (idx + 1) % N;
                return &pool_[idx];
            }
        }
        return nullptr;  // pool exhausted — should not happen in practice
    }

    FORCE_INLINE ChildOrder* find(uint64_t order_id) noexcept {
        for (uint32_t i = 0; i < N; ++i)
            if (pool_[i].live && pool_[i].order_id == order_id)
                return &pool_[i];
        return nullptr;
    }

    FORCE_INLINE uint32_t live_count() const noexcept {
        uint32_t c = 0;
        for (uint32_t i = 0; i < N; ++i) c += pool_[i].live ? 1 : 0;
        return c;
    }

    template<typename Fn>
    FORCE_INLINE void for_each_live(Fn&& fn) noexcept {
        for (uint32_t i = 0; i < N; ++i)
            if (pool_[i].live) fn(pool_[i]);
    }

    void cancel_all(IOrderRouter& router) noexcept {
        for (uint32_t i = 0; i < N; ++i) {
            if (pool_[i].live) {
                router.cancel(pool_[i].order_id);
                pool_[i].live = false;
            }
        }
    }

private:
    CACHE_ALIGN ChildOrder pool_[N]{};
    uint32_t next_ = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// LOCK-FREE SPSC LOG RING
//
// Hot path: producer formats into a 128-byte stack buffer, enqueues.
// Cold path: background thread (or end-of-run flush) prints.
// Zero heap alloc; zero mutex; zero syscall on producer side.
// ════════════════════════════════════════════════════════════════════════════

static constexpr size_t LOG_MSG_LEN  = 128;
static constexpr size_t LOG_RING_CAP = 1024;   // power-of-2

struct LogMsg {
    char     buf[LOG_MSG_LEN];
    uint32_t len;
};

class alignas(CACHE_LINE) LogRing {
    static constexpr uint32_t MASK = LOG_RING_CAP - 1;
    CACHE_ALIGN LogMsg                       ring_[LOG_RING_CAP]{};
    CACHE_ALIGN std::atomic<uint64_t>        enq_{0};
    CACHE_ALIGN std::atomic<uint64_t>        deq_{0};

public:
    // Producer (hot path) — non-blocking, drops if full
    FORCE_INLINE HOT_PATH void push(const char* msg, uint32_t len) noexcept {
        const uint64_t pos  = enq_.load(std::memory_order_relaxed);
        const uint64_t next = pos + 1;
        if (__builtin_expect(next - deq_.load(std::memory_order_acquire) > LOG_RING_CAP, 0))
            return;  // ring full — drop log message (never block hot path)
        uint32_t copy_len = std::min(len, static_cast<uint32_t>(LOG_MSG_LEN - 1));
        std::memcpy(ring_[pos & MASK].buf, msg, copy_len);
        ring_[pos & MASK].buf[copy_len] = '\0';
        ring_[pos & MASK].len = copy_len;
        enq_.store(next, std::memory_order_release);
    }

    // Consumer (cold path — background or end-of-run)
    COLD_PATH void flush() noexcept {
        uint64_t pos = deq_.load(std::memory_order_relaxed);
        while (pos < enq_.load(std::memory_order_acquire)) {
            const auto& m = ring_[pos & MASK];
            std::fwrite(m.buf, 1, m.len, stdout);
            std::fputc('\n', stdout);
            deq_.store(++pos, std::memory_order_release);
        }
        std::fflush(stdout);
    }

    bool empty() const noexcept {
        return deq_.load(std::memory_order_acquire) ==
               enq_.load(std::memory_order_acquire);
    }
};

// ════════════════════════════════════════════════════════════════════════════
// ALGO METRICS  — lock-free counters updated in hot path
// ════════════════════════════════════════════════════════════════════════════

struct alignas(CACHE_LINE) AlgoMetrics {
    std::atomic<uint64_t> fills_count      {0};
    std::atomic<uint64_t> filled_qty       {0};
    std::atomic<uint64_t> filled_value     {0};  // sum(fill_px × fill_qty) raw
    std::atomic<uint64_t> orders_sent      {0};
    std::atomic<uint64_t> orders_cancelled {0};
    std::atomic<uint64_t> ticks_processed  {0};
    std::atomic<uint64_t> sum_latency_tsc  {0};  // sum of (fill_tsc - submit_tsc)

    void reset() noexcept {
        fills_count.store(0, std::memory_order_relaxed);
        filled_qty .store(0, std::memory_order_relaxed);
        filled_value.store(0, std::memory_order_relaxed);
        orders_sent.store(0, std::memory_order_relaxed);
        orders_cancelled.store(0, std::memory_order_relaxed);
        ticks_processed.store(0, std::memory_order_relaxed);
        sum_latency_tsc.store(0, std::memory_order_relaxed);
    }

    COLD_PATH void print(const char* algo_name, double ns_per_tick) const noexcept {
        const uint64_t fq  = filled_qty .load(std::memory_order_relaxed);
        const uint64_t fv  = filled_value.load(std::memory_order_relaxed);
        const uint64_t fc  = fills_count .load(std::memory_order_relaxed);
        const uint64_t os  = orders_sent .load(std::memory_order_relaxed);
        const uint64_t lat = sum_latency_tsc.load(std::memory_order_relaxed);

        std::printf("  %-20s  fills=%-6llu  qty=%-10llu  orders=%-5llu",
            algo_name,
            (unsigned long long)fc,
            (unsigned long long)fq,
            (unsigned long long)os);

        if (fq > 0)
            std::printf("  avg_px=$%.4f", (double)fv / (fq * PRICE_SCALE));
        if (fc > 0 && ns_per_tick > 0)
            std::printf("  avg_fill_lat=%.0fns",
                        (double)lat / fc * ns_per_tick);
        std::puts("");
    }
};

// ════════════════════════════════════════════════════════════════════════════
// TSC CALIBRATION  — call once at startup, store result in g_ns_per_tick
// ════════════════════════════════════════════════════════════════════════════

static double g_ns_per_tick = 0.4;  // ~2.5 GHz default; replaced by calibrate_tsc()

COLD_PATH inline void calibrate_tsc() noexcept {
    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t r0 = rdtsc_now();
    // Keep loop from being optimised away
    uint64_t acc = 0;
    for (int i = 0; i < 10'000'000; ++i) {
        acc += static_cast<uint64_t>(i);
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }
    (void)acc;
    const uint64_t r1 = rdtsc_now();
    const auto t1 = std::chrono::steady_clock::now();
    const uint64_t wall_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    g_ns_per_tick = static_cast<double>(wall_ns) / static_cast<double>(r1 - r0);
    std::printf("  TSC calibration: %.3f GHz (%.3f ns/tick)\n",
                1.0 / g_ns_per_tick, g_ns_per_tick);
}

// ════════════════════════════════════════════════════════════════════════════
// LOG HELPER MACRO — formats to stack buffer, pushes to ring, zero heap alloc
// Usage: ALGO_LOG(log_ring, "[VWAP] FILL qty=%llu px=%.4f", qty, px);
// ════════════════════════════════════════════════════════════════════════════

#define ALGO_LOG(ring_, ...)                            \
    do {                                                \
        char _lb[LOG_MSG_LEN];                          \
        int  _ln = std::snprintf(_lb, sizeof(_lb), __VA_ARGS__); \
        if (_ln > 0) (ring_).push(_lb, static_cast<uint32_t>(_ln)); \
    } while(0)

