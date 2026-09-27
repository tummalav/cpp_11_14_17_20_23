//
// Created by Tummala Venkata Sateesh on 22/4/26.
//
//Telemetry usage and example to use telemetry client C++
// Corvil use cases and how to use
// rdtsc examples for ultra low latency
// wire to wire latency examples

/**
 * =============================================================================
 * telemetry_corvil_rdtsc.cpp
 *
 * Comprehensive Guide:
 *   1. Telemetry Client C++ — application-level latency instrumentation
 *   2. Corvil CNX — passive wire-level hardware timestamping
 *   3. RDTSC — CPU cycle counter for sub-nanosecond precision
 *   4. Wire-to-Wire Latency — end-to-end measurement pipeline
 *
 * Target: RHEL8, x86_64, GCC 11+, isolated cores, Solarflare NIC
 *
 * Compile:
 *   g++ -std=c++17 -O3 -march=native telemetry_corvil_rdtsc.cpp \
 *       -lpthread -o telemetry_demo
 * =============================================================================
 *
 * HIGH LEVEL OVERVIEW:
 * ====================
 *
 *  [EXCHANGE]
 *      │  (UDP multicast / TCP)
 *      ▼
 *  [NIC (Solarflare)]  ← Corvil taps HERE (passive mirror port)
 *      │  hardware timestamp per packet (PTP/GPS-locked)
 *      ▼
 *  [Kernel Bypass (OpenOnload/TCPDirect/ef_vi)]
 *      │
 *      ▼
 *  [Market Data Feed Handler Thread]  ← T1: RDTSC (first byte decoded)
 *      │  SPSC queue
 *      ▼
 *  [Strategy / Pricer Thread]         ← T2: RDTSC (tick received)
 *      │  SPSC queue                  ← T3: RDTSC (quote generated)
 *      ▼
 *  [Order Gateway Thread]             ← T4: RDTSC (order sent to kernel/NIC)
 *      │  TCP/UDP
 *      ▼
 *  [EXCHANGE]  ← Corvil taps again (order wire timestamp)
 *
 *  Wire-to-Wire latency = Corvil timestamp(order out) - Corvil timestamp(market data in)
 *  Tick-to-Order latency = T4 - T1  (internal application latency)
 *  Kernel/NIC overhead  = Wire-to-Wire - Tick-to-Order
 *
 * =============================================================================
 *
 * CORVIL CNX EXPLAINED:
 * =====================
 *
 *  Corvil is a PASSIVE network monitoring appliance (CNX box) that:
 *    - Connects via a TAP or SPAN/mirror port on the switch
 *    - Has its own PTP/GPS-locked hardware clock (nanosecond accuracy)
 *    - Captures EVERY packet at the wire level (before/after the NIC)
 *    - Stamps each captured packet with a precise hardware timestamp
 *    - Reconstructs TCP streams, decodes FIX/ITCH/OUCH/FAST protocols
 *    - Measures latency between correlated messages (e.g. market data → order)
 *
 *  CORVIL DOES NOT MODIFY YOUR APPLICATION:
 *    - No agent inside your process
 *    - No code changes needed for basic wire timestamping
 *    - Optional: embed a Corvil Sequence Tag (CST) inside FIX messages
 *      so Corvil can correlate a specific market data tick to the order it triggered
 *
 *  CORVIL DEPLOYMENT:
 *
 *    [Exchange Feed]
 *         │
 *         ├──── TAP ──→ [Corvil CNX Port A]  ← stamps each market data packet
 *         │
 *    [Your Server NIC]
 *         │
 *         ├──── TAP ──→ [Corvil CNX Port B]  ← stamps each outbound order packet
 *
 *    Corvil CNX correlates Port A timestamp with Port B timestamp
 *    → Wire-to-Wire latency reported in Corvil Analytics dashboard
 *
 *  CORVIL SEQUENCE TAG (CST):
 *    - You embed a 8-byte tag inside your FIX NewOrderSingle (tag 9880 or custom)
 *    - Corvil sees this tag in your order and looks up the matching
 *      market data packet that triggered it (from Port A capture)
 *    - Enables per-message, causality-based latency measurement
 *
 * =============================================================================
 *
 * RDTSC EXPLAINED:
 * ================
 *
 *  RDTSC = Read Time Stamp Counter
 *    - x86 instruction available since Pentium
 *    - Returns 64-bit CPU cycle counter
 *    - Overhead: ~3-7 CPU cycles (~1-3 ns at 3GHz)
 *    - Resolution: 1 CPU cycle (~0.3 ns at 3GHz)
 *    - Does NOT call kernel → zero syscall overhead
 *
 *  THREE VARIANTS:
 *    1. rdtsc  (plain)     — no serialisation, ~7 cycles, use for mid-pipeline marks
 *    2. lfence+rdtsc       — fenced start, prevents prior instructions crossing
 *    3. rdtscp             — serialising end, waits for all prior instructions
 *
 *  WHY NOT clock_gettime()?
 *    - clock_gettime(CLOCK_REALTIME) = ~20-50ns (vDSO, still expensive)
 *    - RDTSC = ~2-3ns (no syscall, direct hardware register read)
 *    - For sub-microsecond trading, use RDTSC for all hot path timestamps
 *
 *  CALIBRATION:
 *    - TSC counts CPU cycles, not nanoseconds
 *    - You need: ns = ticks * (1.0 / GHz)
 *    - Example: 3.6 GHz CPU → 1 tick = 0.277 ns
 *    - Measure once at startup against steady_clock for accurate conversion
 *
 * =============================================================================
 */

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>
#include <chrono>
#include <functional>
#include <unordered_map>

// ============================================================================
// PLATFORM DETECTION
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    #include <immintrin.h>
    #define PLATFORM_X86 1
    #define CPU_PAUSE()  _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
    #define PLATFORM_ARM 1
    #define CPU_PAUSE()  __asm__ volatile("yield" ::: "memory")
#else
    #define CPU_PAUSE()  std::this_thread::yield()
#endif

#define CACHE_LINE   64
#define CACHE_ALIGN  alignas(CACHE_LINE)
#define FORCE_INLINE __attribute__((always_inline)) inline
#define NO_INLINE    __attribute__((noinline))

// ============================================================================
// SECTION 1: RDTSC — Three variants explained with use cases
// ============================================================================

/**
 * rdtsc_now() — PLAIN RDTSC
 * ─────────────────────────
 * Overhead : ~7 CPU cycles (~2 ns @ 3.5 GHz)
 * Ordering : NO fence — CPU can reorder instructions across this read
 * Use for  : Mid-pipeline marks where program flow already guarantees ordering
 *            (e.g., between push() and pop() of a ring buffer)
 */
FORCE_INLINE uint64_t rdtsc_now() noexcept {
#if defined(PLATFORM_X86)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#elif defined(PLATFORM_ARM)
    uint64_t v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#else
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

/**
 * rdtsc_start() — FENCED START (LFENCE + RDTSC + LFENCE)
 * ──────────────────────────────────────────────────────
 * Overhead : ~10-15 CPU cycles (~4 ns @ 3.5 GHz)
 * Ordering : LFENCE before RDTSC ensures all PRIOR instructions complete
 *            LFENCE after RDTSC ensures RDTSC itself completes before next insn
 * Use for  : First measurement point — start of a latency window
 *
 * Example:
 *   uint64_t t_start = rdtsc_start();   ← mark start
 *   ... code under measurement ...
 *   uint64_t t_end   = rdtsc_end();     ← mark end
 */
FORCE_INLINE uint64_t rdtsc_start() noexcept {
#if defined(PLATFORM_X86)
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\trdtsc\n\tlfence"
                     : "=a"(lo), "=d"(hi)
                     :: "memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    return rdtsc_now();
#endif
}

/**
 * rdtsc_end() — SERIALISING END (RDTSCP)
 * ───────────────────────────────────────
 * Overhead : ~20-25 CPU cycles (~7 ns @ 3.5 GHz)
 * Ordering : RDTSCP waits for ALL prior instructions to retire
 *            strongest ordering guarantee — no reordering across this point
 * Use for  : Last measurement point — end of latency window
 *
 * Note: rdtscp also reads processor-id into ecx (ignored here)
 */
FORCE_INLINE uint64_t rdtsc_end() noexcept {
#if defined(PLATFORM_X86)
    uint32_t lo, hi, aux;   // aux = processor ID (ignored)
    __asm__ volatile("rdtscp"
                     : "=a"(lo), "=d"(hi), "=c"(aux)
                     :: "memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    return rdtsc_now();
#endif
}

/**
 * rdtsc_start_end() — Which to use where?
 *
 *  HOT PATH (inside market data / order thread):
 *    ┌──────────────────────────────────────────────────────────────┐
 *    │  uint64_t t0 = rdtsc_start();   // fenced start (safe)      │
 *    │  process_tick(tick);            // code under measurement    │
 *    │  uint64_t t1 = rdtsc_end();     // serialising end (safe)    │
 *    │  spsc_stats.push(t1 - t0);     // raw ticks — convert later  │
 *    └──────────────────────────────────────────────────────────────┘
 *
 *  MULTI-STAGE PIPELINE:
 *    ┌──────────────────────────────────────────────────────────────┐
 *    │  t[0] = rdtsc_start();   // fenced — first mark             │
 *    │  decode_fix(msg);                                            │
 *    │  t[1] = rdtsc_now();    // plain — mid-stage mark           │
 *    │  apply_markup(quote);                                        │
 *    │  t[2] = rdtsc_now();    // plain — mid-stage mark           │
 *    │  send_tcp(msg);                                              │
 *    │  t[3] = rdtsc_end();    // serialising — final mark         │
 *    └──────────────────────────────────────────────────────────────┘
 */

// ============================================================================
// SECTION 2: TSC CALIBRATION
// Convert raw TSC ticks to nanoseconds.
// Done ONCE at startup — not in hot path.
// ============================================================================

/**
 * TscCalibrator
 * ─────────────
 * Measures TSC frequency by comparing TSC ticks to steady_clock over 100ms.
 * Result: ns_per_tick (e.g. 0.2857 ns/tick for 3.5 GHz CPU)
 *
 * USAGE:
 *   TscCalibrator& cal = TscCalibrator::instance();
 *   uint64_t ns = cal.to_ns(tsc_end - tsc_start);
 */
class TscCalibrator {
public:
    double ns_per_tick{0.0};
    double ticks_per_ns{0.0};
    double ghz{0.0};

    TscCalibrator() { calibrate(); }

    static TscCalibrator& instance() {
        static TscCalibrator inst;
        return inst;
    }

    void calibrate() {
        // Warm up TSC
        for (int i = 0; i < 5; ++i) rdtsc_now();

        // Measure over 100ms for accuracy
        auto wall_start = std::chrono::steady_clock::now();
        uint64_t tsc_s  = rdtsc_now();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        uint64_t tsc_e  = rdtsc_now();
        auto wall_end   = std::chrono::steady_clock::now();

        double wall_ns  = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                wall_end - wall_start).count());

        double ticks    = static_cast<double>(tsc_e - tsc_s);

        ns_per_tick     = wall_ns / ticks;
        ticks_per_ns    = ticks   / wall_ns;
        ghz             = 1.0 / ns_per_tick;

        std::cout << "[TscCalibrator] TSC frequency: "
                  << std::fixed << std::setprecision(3) << ghz << " GHz"
                  << "  ns_per_tick=" << ns_per_tick << "\n";
    }

    FORCE_INLINE uint64_t to_ns(uint64_t ticks) const noexcept {
        return static_cast<uint64_t>(static_cast<double>(ticks) * ns_per_tick);
    }

    FORCE_INLINE uint64_t to_us(uint64_t ticks) const noexcept {
        return to_ns(ticks) / 1000ULL;
    }

    FORCE_INLINE uint64_t to_ticks(uint64_t ns) const noexcept {
        return static_cast<uint64_t>(static_cast<double>(ns) * ticks_per_ns);
    }
};

// ============================================================================
// SECTION 3: LATENCY HISTOGRAM
// Lock-free, hot-path safe, buckets from <100ns to >100us
// ============================================================================

/**
 * LatencyHistogram
 * ────────────────
 * Lock-free histogram for measuring latency distributions.
 * Use in hot path: only ~3-5 ns per record() call.
 *
 * Buckets (ns): <100 | 100-200 | 200-500 | 500-1k | 1k-2k
 *               2k-5k | 5k-10k | 10k-50k | 50k-100k | >100k
 *
 * USAGE:
 *   LatencyHistogram h("tick_to_order");
 *   uint64_t t0 = rdtsc_start();
 *   ... code ...
 *   uint64_t ns = TscCalibrator::instance().to_ns(rdtsc_end() - t0);
 *   h.record(ns);
 *   h.print();
 */
class LatencyHistogram {
    static constexpr int BUCKETS = 10;
    static constexpr uint64_t BOUNDS[BUCKETS-1] =
        {100, 200, 500, 1'000, 2'000, 5'000, 10'000, 50'000, 100'000};
    static constexpr const char* LABELS[BUCKETS] = {
        "<100ns", "100-200ns", "200-500ns", "500ns-1us",
        "1-2us",  "2-5us",    "5-10us",    "10-50us",  "50-100us", ">100us"
    };

    CACHE_ALIGN std::atomic<uint64_t> cnt_[BUCKETS]{};
    CACHE_ALIGN std::atomic<uint64_t> total_{0};
    CACHE_ALIGN std::atomic<uint64_t> sum_{0};
    CACHE_ALIGN std::atomic<uint64_t> max_{0};
    CACHE_ALIGN std::atomic<uint64_t> min_{UINT64_MAX};
    const std::string label_;

public:
    explicit LatencyHistogram(const char* label = "") : label_(label) {}

    void record(uint64_t ns) noexcept {
        int b = BUCKETS - 1;
        for (int i = 0; i < BUCKETS - 1; ++i) {
            if (ns < BOUNDS[i]) { b = i; break; }
        }
        cnt_[b].fetch_add(1, std::memory_order_relaxed);
        total_.fetch_add(1, std::memory_order_relaxed);
        sum_.fetch_add(ns,  std::memory_order_relaxed);

        uint64_t mx = max_.load(std::memory_order_relaxed);
        while (ns > mx && !max_.compare_exchange_weak(mx, ns, std::memory_order_relaxed)) {}

        uint64_t mn = min_.load(std::memory_order_relaxed);
        while (ns < mn && !min_.compare_exchange_weak(mn, ns, std::memory_order_relaxed)) {}
    }

    uint64_t percentile(double p) const noexcept {
        uint64_t n = total_.load(std::memory_order_acquire);
        if (!n) return 0;
        uint64_t target = static_cast<uint64_t>(p * static_cast<double>(n) / 100.0);
        uint64_t cum = 0;
        for (int i = 0; i < BUCKETS; ++i) {
            cum += cnt_[i].load(std::memory_order_relaxed);
            if (cum >= target) return (i < BUCKETS - 1) ? BOUNDS[i] : 2'000'000ULL;
        }
        return 0;
    }

    void print() const {
        uint64_t n = total_.load(std::memory_order_acquire);
        if (!n) { std::cout << "  [" << label_ << "] No samples\n"; return; }
        uint64_t avg = sum_.load() / n;

        std::cout << "\n╔══════════════════════════════════════════════════════\n";
        std::cout << "║  Latency: " << label_ << "\n";
        std::cout << "╠══════════════════════════════════════════════════════\n";
        std::cout << "║  Samples : " << n     << "\n"
                  << "║  Min     : " << min_.load() << " ns\n"
                  << "║  Avg     : " << avg         << " ns\n"
                  << "║  P50     : " << percentile(50.0)  << " ns\n"
                  << "║  P95     : " << percentile(95.0)  << " ns\n"
                  << "║  P99     : " << percentile(99.0)  << " ns\n"
                  << "║  P99.9   : " << percentile(99.9)  << " ns\n"
                  << "║  Max     : " << max_.load()       << " ns\n";
        std::cout << "╠══════════════════════════════════════════════════════\n";
        uint64_t cum = 0;
        for (int i = 0; i < BUCKETS; ++i) {
            uint64_t c = cnt_[i].load();
            cum += c;
            double pct  = 100.0 * static_cast<double>(c)   / static_cast<double>(n);
            double cpct = 100.0 * static_cast<double>(cum) / static_cast<double>(n);
            std::string bar(static_cast<int>(pct / 3.0), '#');
            std::cout << "║  " << std::left  << std::setw(12) << LABELS[i]
                      << " "   << std::right << std::setw(8)  << c
                      << "  "  << std::fixed << std::setprecision(1)
                      << std::setw(5) << pct << "%  cum="
                      << std::setw(5) << cpct << "% |" << bar << "\n";
        }
        std::cout << "╚══════════════════════════════════════════════════════\n";
    }

    void reset() {
        for (auto& c : cnt_) c.store(0);
        total_.store(0); sum_.store(0);
        max_.store(0);   min_.store(UINT64_MAX);
    }
};

// ============================================================================
// SECTION 4: MULTI-STAGE PIPELINE TIMESTAMP MAP
// Stamp each hop in the trading pipeline for fine-grained latency breakdown
// ============================================================================

/**
 * PIPELINE STAGES (Market Data → Order):
 *
 *   NIC_RECV    : first byte received at NIC (hardware/software timestamp)
 *   SOCKET_READ : data read from socket buffer into app buffer
 *   MSG_DECODED : FIX/ITCH/OUCH message fully decoded
 *   BOOK_UPDATED: order book updated with new price
 *   SIGNAL_GEN  : strategy signal generated (buy/sell decision)
 *   ORDER_BUILT : NewOrderSingle message built in memory
 *   RISK_CHECKED: pre-trade risk checks passed
 *   ORDER_SENT  : send() called on socket (message in kernel buffer)
 *   NIC_SENT    : message transmitted at wire level (Corvil measures this)
 */
enum class PipelineStage : uint8_t {
    NIC_RECV     = 0,
    SOCKET_READ  = 1,
    MSG_DECODED  = 2,
    BOOK_UPDATED = 3,
    SIGNAL_GEN   = 4,
    ORDER_BUILT  = 5,
    RISK_CHECKED = 6,
    ORDER_SENT   = 7,
    NIC_SENT     = 8,   // Corvil-measured wire timestamp
    COUNT        = 9
};

static const char* STAGE_NAMES[] = {
    "NIC_RECV    ",
    "SOCKET_READ ",
    "MSG_DECODED ",
    "BOOK_UPDATED",
    "SIGNAL_GEN  ",
    "ORDER_BUILT ",
    "RISK_CHECKED",
    "ORDER_SENT  ",
    "NIC_SENT    "
};

/**
 * PipelineTsMap — per-message timestamp collection for all pipeline stages.
 *
 * USAGE (in hot path):
 *   PipelineTsMap ts;
 *   ts.mark(PipelineStage::NIC_RECV);     // fenced start
 *   decode_message(buf);
 *   ts.mark(PipelineStage::MSG_DECODED);  // plain rdtsc
 *   update_book(msg);
 *   ts.mark(PipelineStage::BOOK_UPDATED); // plain rdtsc
 *   generate_signal();
 *   ts.mark(PipelineStage::SIGNAL_GEN);   // plain rdtsc
 *   send_order(order);
 *   ts.mark(PipelineStage::ORDER_SENT);   // serialising end
 *
 *   // Off hot path: send to stats thread via SPSC queue
 *   stats_queue.push(ts);
 */
struct PipelineTsMap {
    uint64_t t[static_cast<int>(PipelineStage::COUNT)]{};
    uint32_t seq_num{0};        // market data sequence number
    uint64_t corvil_wire_ns{0}; // wall-clock ns from Corvil CNX (post-trade)

    FORCE_INLINE void mark(PipelineStage s) noexcept {
        int idx = static_cast<int>(s);
        if      (s == PipelineStage::NIC_RECV)   t[idx] = rdtsc_start();  // fenced start
        else if (s == PipelineStage::ORDER_SENT) t[idx] = rdtsc_end();    // serialising end
        else                                      t[idx] = rdtsc_now();   // plain mid-stage
    }

    FORCE_INLINE uint64_t delta_ns(PipelineStage from, PipelineStage to) const noexcept {
        int f = static_cast<int>(from);
        int e = static_cast<int>(to);
        if (!t[f] || !t[e]) return 0;
        return TscCalibrator::instance().to_ns(t[e] - t[f]);
    }

    FORCE_INLINE uint64_t tick_to_order_ns() const noexcept {
        return delta_ns(PipelineStage::NIC_RECV, PipelineStage::ORDER_SENT);
    }

    void print_breakdown() const {
        const TscCalibrator& cal = TscCalibrator::instance();
        std::cout << "\n  Pipeline Latency Breakdown (seq=" << seq_num << "):\n";
        for (int i = 0; i < static_cast<int>(PipelineStage::COUNT) - 1; ++i) {
            if (!t[i] || !t[i+1]) continue;
            uint64_t ns = cal.to_ns(t[i+1] - t[i]);
            std::cout << "    " << STAGE_NAMES[i]
                      << " → " << STAGE_NAMES[i+1]
                      << " : " << ns << " ns\n";
        }
        std::cout << "    ─────────────────────────────────────\n";
        std::cout << "    TOTAL tick-to-order: "
                  << tick_to_order_ns() << " ns\n";
        if (corvil_wire_ns > 0) {
            std::cout << "    Corvil wire-to-wire: "
                      << corvil_wire_ns << " ns\n";
            std::cout << "    NIC/kernel overhead: "
                      << (corvil_wire_ns - tick_to_order_ns()) << " ns\n";
        }
    }
};

// ============================================================================
// SECTION 5: TELEMETRY CLIENT
// ============================================================================

/**
 * TelemetryClient
 * ───────────────
 * Application-level telemetry: collect latency samples from multiple
 * components and report statistics.
 *
 * DESIGN PRINCIPLES:
 *   - Zero dynamic allocation on hot path
 *   - Lock-free SPSC queue from hot thread to stats thread
 *   - Stats thread converts ticks → ns, updates histograms, reports
 *   - Histograms are lock-free (atomic fetch_add only)
 *   - Named metrics: one histogram per measurement point
 *
 * ARCHITECTURE:
 *
 *   Hot Thread                     Stats Thread
 *   ──────────                     ────────────
 *   t0 = rdtsc_start()
 *   process(msg)
 *   t1 = rdtsc_end()
 *   spsc.push({t1-t0, "tick_to_order"})  →  spsc.pop()
 *                                           hist["tick_to_order"].record(ns)
 *                                           every 5s: print reports
 */

// Telemetry sample: raw TSC ticks + metric ID (avoid string in hot path)
struct TelemetrySample {
    uint64_t ticks;     // raw TSC tick diff — convert to ns in stats thread
    uint32_t metric_id; // index into metric name table
    uint32_t seq;       // message sequence number for correlation
};

// Lock-free SPSC queue for telemetry samples (hot path → stats thread)
template<size_t Cap>
class TelemetryQueue {
    static_assert((Cap & (Cap-1)) == 0, "Cap must be power of 2");
    CACHE_ALIGN std::atomic<uint64_t> write_{0};
    CACHE_ALIGN std::atomic<uint64_t> read_{0};
    CACHE_ALIGN std::array<TelemetrySample, Cap> buf_{};
    static constexpr uint64_t MASK = Cap - 1;

public:
    // Hot path — producer: called from market data / order thread
    FORCE_INLINE bool push(TelemetrySample s) noexcept {
        uint64_t w = write_.load(std::memory_order_relaxed);
        uint64_t r = read_.load(std::memory_order_acquire);
        if (w - r >= Cap) return false;  // full — drop (never block!)
        buf_[w & MASK] = s;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }

    // Stats thread — consumer: drains the queue
    bool pop(TelemetrySample& out) noexcept {
        uint64_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) return false;  // empty
        out = buf_[r & MASK];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }

    size_t size() const noexcept {
        return static_cast<size_t>(
            write_.load(std::memory_order_acquire) -
            read_.load(std::memory_order_acquire));
    }
};

/**
 * TelemetryClient — Central telemetry registry
 * Usage:
 *   auto& tel = TelemetryClient::instance();
 *   uint32_t mid = tel.register_metric("tick_to_order_ns");
 *
 *   // Hot path:
 *   uint64_t t0 = rdtsc_start();
 *   process(tick);
 *   tel.record(mid, rdtsc_end() - t0, seq);   // ~5 ns total overhead
 *
 *   // Stats thread:
 *   tel.drain_and_report();
 */
class TelemetryClient {
    static constexpr size_t MAX_METRICS  = 32;
    static constexpr size_t QUEUE_SIZE   = 1 << 16;  // 65536 samples

    TelemetryQueue<QUEUE_SIZE>         queue_;
    std::array<LatencyHistogram, MAX_METRICS> histograms_;
    std::array<std::string, MAX_METRICS>      names_;
    std::atomic<uint32_t>              metric_count_{0};

    TelemetryClient() = default;

public:
    static TelemetryClient& instance() {
        static TelemetryClient inst;
        return inst;
    }

    // Register a named metric — returns metric_id for use in hot path
    // Call at startup, NOT in hot path
    uint32_t register_metric(const char* name) {
        uint32_t id = metric_count_.fetch_add(1, std::memory_order_relaxed);
        assert(id < MAX_METRICS && "Too many metrics registered");
        names_[id] = name;
        new (&histograms_[id]) LatencyHistogram(name);
        return id;
    }

    // Hot path: record a raw TSC tick measurement
    // ~5 ns total overhead (SPSC push only)
    FORCE_INLINE bool record(uint32_t metric_id, uint64_t ticks, uint32_t seq = 0) noexcept {
        return queue_.push({ticks, metric_id, seq});
    }

    // Stats thread: drain queue, convert ticks → ns, update histograms
    // Call from dedicated stats/monitoring thread
    void drain() {
        const TscCalibrator& cal = TscCalibrator::instance();
        TelemetrySample s;
        while (queue_.pop(s)) {
            if (s.metric_id < metric_count_.load(std::memory_order_relaxed)) {
                uint64_t ns = cal.to_ns(s.ticks);
                histograms_[s.metric_id].record(ns);
            }
        }
    }

    // Print all registered metric histograms
    void report() {
        drain();
        uint32_t n = metric_count_.load(std::memory_order_relaxed);
        std::cout << "\n╔══════════════════════════════════════════════════\n";
        std::cout << "║  TELEMETRY REPORT\n";
        std::cout << "╚══════════════════════════════════════════════════\n";
        for (uint32_t i = 0; i < n; ++i) {
            histograms_[i].print();
        }
    }

    void reset_all() {
        uint32_t n = metric_count_.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < n; ++i) histograms_[i].reset();
    }
};

// ============================================================================
// SECTION 6: CORVIL SEQUENCE TAG (CST) — FIX MESSAGE EMBEDDING
// ============================================================================

/**
 * CorvilSequenceTag (CST)
 * ───────────────────────
 * Corvil CNX supports optional "Corvil Sequence Tags" (CST) embedded inside
 * application messages. This allows Corvil to correlate:
 *   - The exact market data packet that TRIGGERED an order
 *   - The resulting NewOrderSingle packet sent to the exchange
 *
 * By correlating these, Corvil measures TRUE causal latency:
 *   Causal latency = wire_timestamp(order out) − wire_timestamp(trigger market data in)
 *
 * HOW IT WORKS:
 *   1. Your market data handler reads a packet
 *   2. Corvil has already hardware-timestamped that packet on Port A
 *   3. You extract the Corvil Sequence Number from the Corvil header
 *      (or you generate your own correlation ID)
 *   4. You embed this ID inside your outbound FIX message (custom tag, e.g. 9880)
 *   5. Corvil sees your order on Port B, reads tag 9880, looks up the
 *      corresponding Port A timestamp, computes end-to-end causal latency
 *
 * CORVIL FIX TAG CONFIGURATION:
 *   In Corvil CNX Management Interface:
 *   - Configure FIX decoder for your session
 *   - Set "Correlation Tag" = 9880 (or your custom tag number)
 *   - Enable "Causal Latency" measurement
 *
 * BELOW: Simulated CST embedding (no Corvil SDK dependency):
 */

struct CorvilSequenceTag {
    uint64_t timestamp_ns;   // wall-clock ns from Corvil hardware clock
    uint32_t sequence_id;    // correlation ID embedded in FIX message
    uint32_t port_id;        // Corvil capture port (A=market data, B=order)
};

/**
 * FIX Message with embedded Corvil Sequence Tag
 *
 * Standard FIX NewOrderSingle with custom tag 9880 for Corvil correlation:
 *
 *   8=FIX.4.4|9=...|35=D|49=TRADER|56=EXCHANGE|
 *   11=ORD123|55=AAPL|54=1|38=100|40=2|44=150.25|
 *   9880=4729384756|               ← Corvil Sequence Tag (CST)
 *   10=xxx|
 *
 * The CST (9880) value matches the sequence number Corvil assigned to the
 * market data packet that triggered this order.
 */
class FIXMessageBuilder {
    static constexpr size_t BUF_SIZE = 4096;
    char buf_[BUF_SIZE]{};
    int  len_{0};

public:
    void reset() { len_ = 0; }

    void append_field(int tag, const char* value) {
        len_ += std::snprintf(buf_ + len_, BUF_SIZE - len_, "%d=%s\001", tag, value);
    }

    void append_field(int tag, uint64_t value) {
        len_ += std::snprintf(buf_ + len_, BUF_SIZE - len_, "%d=%lu\001", tag, value);
    }

    void append_field(int tag, double value, int decimals = 2) {
        len_ += std::snprintf(buf_ + len_, BUF_SIZE - len_,
                              "%d=%.*f\001", tag, decimals, value);
    }

    // Build NewOrderSingle with embedded Corvil Sequence Tag
    const char* build_new_order_single(
        const char* clordid,
        const char* symbol,
        char  side,            // '1'=Buy, '2'=Sell
        int   quantity,
        double price,
        uint64_t corvil_seq_tag)  // embedded for Corvil correlation
    {
        reset();
        append_field(35, "D");              // MsgType = NewOrderSingle
        append_field(49, "MYTRADER");       // SenderCompID
        append_field(56, "EXCHANGE");       // TargetCompID
        append_field(11, clordid);          // ClOrdID
        append_field(55, symbol);           // Symbol
        char side_str[2] = {side, 0};
        append_field(54, side_str);         // Side
        append_field(38, static_cast<uint64_t>(quantity)); // OrderQty
        append_field(40, "2");              // OrdType = Limit
        append_field(44, price, 4);         // Price
        append_field(59, "0");              // TimeInForce = Day
        append_field(9880, corvil_seq_tag); // *** CORVIL SEQUENCE TAG ***
        return buf_;
    }

    const char* data() const { return buf_; }
    int length() const { return len_; }
};

// ============================================================================
// SECTION 7: WIRE-TO-WIRE LATENCY MEASUREMENT
// Full example: market data in → order out
// ============================================================================

/**
 * WireToWireLatency
 * ─────────────────
 * Combines:
 *   - RDTSC measurements (internal application latency)
 *   - Corvil wire timestamps (true end-to-end wire latency)
 *
 * HOW LATENCIES COMPARE:
 *
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │  [Exchange] ──wire──→ [NIC] ──kernel bypass──→ [App] ──wire──→ [Exchange] │
 *   │                                                                  │
 *   │  Corvil measures:  ←─────────── Wire-to-Wire ───────────────→   │
 *   │  RDTSC measures:          ←──── App internal ────────────→      │
 *   │                                                                  │
 *   │  Difference = NIC + kernel bypass + TCP stack overhead          │
 *   │  With Solarflare OpenOnload: NIC overhead ≈ 500ns-2us           │
 *   │  With standard kernel TCP:   NIC overhead ≈ 5-50us              │
 *   └─────────────────────────────────────────────────────────────────┘
 *
 * LATENCY BUDGET BREAKDOWN (target: sub-10us wire-to-wire):
 *
 *   NIC receive (Solarflare OpenOnload):         0.5 - 1.0  us
 *   Kernel bypass / socket read:                 0.1 - 0.3  us
 *   Market data decode (ITCH/FIX/OUCH):          0.1 - 0.5  us
 *   Order book update:                           0.05 - 0.2 us
 *   Strategy signal generation:                  0.1 - 0.5  us
 *   Order message build:                         0.05 - 0.1 us
 *   Pre-trade risk check:                        0.1 - 0.5  us
 *   Socket send (kernel bypass):                 0.2 - 0.5  us
 *   NIC transmit (Solarflare OpenOnload):        0.5 - 1.0  us
 *   ──────────────────────────────────────────────────────────
 *   Total wire-to-wire:                          1.7 - 4.6  us  (best case)
 *   Typical production with tuning:              3  - 10    us
 *   Standard kernel TCP (no tuning):             50 - 200   us
 */
class WireToWireLatencyMonitor {
    // Per-stage histograms
    LatencyHistogram h_nic_to_decode_{"NIC→Decode"};
    LatencyHistogram h_decode_to_signal_{"Decode→Signal"};
    LatencyHistogram h_signal_to_order_{"Signal→Order"};
    LatencyHistogram h_order_to_risk_{"Order→Risk"};
    LatencyHistogram h_risk_to_sent_{"Risk→Sent"};
    LatencyHistogram h_total_app_{"TotalApp(RDTSC)"};
    LatencyHistogram h_wire_to_wire_{"WireToWire(Corvil)"};
    LatencyHistogram h_nic_overhead_{"NICOverhead"};

    std::atomic<uint64_t> sample_count_{0};

    const TscCalibrator& cal_;

public:
    explicit WireToWireLatencyMonitor()
        : cal_(TscCalibrator::instance()) {}

    // Record a complete pipeline measurement
    void record(const PipelineTsMap& ts) {
        auto ns = [&](PipelineStage f, PipelineStage t) {
            return ts.delta_ns(f, t);
        };

        uint64_t nic_decode    = ns(PipelineStage::NIC_RECV,     PipelineStage::MSG_DECODED);
        uint64_t decode_signal = ns(PipelineStage::MSG_DECODED,  PipelineStage::SIGNAL_GEN);
        uint64_t signal_order  = ns(PipelineStage::SIGNAL_GEN,   PipelineStage::ORDER_BUILT);
        uint64_t order_risk    = ns(PipelineStage::ORDER_BUILT,  PipelineStage::RISK_CHECKED);
        uint64_t risk_sent     = ns(PipelineStage::RISK_CHECKED, PipelineStage::ORDER_SENT);
        uint64_t total_app     = ts.tick_to_order_ns();

        if (nic_decode)    h_nic_to_decode_.record(nic_decode);
        if (decode_signal) h_decode_to_signal_.record(decode_signal);
        if (signal_order)  h_signal_to_order_.record(signal_order);
        if (order_risk)    h_order_to_risk_.record(order_risk);
        if (risk_sent)     h_risk_to_sent_.record(risk_sent);
        if (total_app)     h_total_app_.record(total_app);

        // Corvil-provided wire timestamp (fed back via separate channel)
        if (ts.corvil_wire_ns > 0) {
            h_wire_to_wire_.record(ts.corvil_wire_ns);
            if (ts.corvil_wire_ns > total_app) {
                h_nic_overhead_.record(ts.corvil_wire_ns - total_app);
            }
        }

        sample_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void print_report() const {
        std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
        std::cout << "║  WIRE-TO-WIRE LATENCY REPORT                        ║\n";
        std::cout << "║  Samples: " << sample_count_.load() << "\n";
        std::cout << "╚══════════════════════════════════════════════════════╝\n";

        h_nic_to_decode_.print();
        h_decode_to_signal_.print();
        h_signal_to_order_.print();
        h_order_to_risk_.print();
        h_risk_to_sent_.print();
        h_total_app_.print();
        h_wire_to_wire_.print();
        h_nic_overhead_.print();
    }
};

// ============================================================================
// SECTION 8: COMPLETE TRADING PIPELINE SIMULATION
// Demonstrates all measurement points end-to-end
// ============================================================================

/**
 * CORVIL INTEGRATION FLOW:
 *
 *   1. Corvil CNX appliance taps the switch mirror port
 *   2. As each market data packet arrives at your NIC:
 *       - Corvil hardware-timestamps it (Port A)
 *       - Assigns a sequence number (e.g., 4729384756)
 *   3. Your app receives the packet, stamps with rdtsc_start()
 *   4. App processes: decode → book update → signal → order
 *   5. App builds FIX NewOrderSingle, embeds CST tag 9880=4729384756
 *   6. App sends order via socket
 *   7. Corvil hardware-timestamps the order packet (Port B)
 *   8. Corvil matches tag 9880 value to Port A sequence 4729384756
 *   9. Corvil reports: Port_B_timestamp - Port_A_timestamp = 8.3us
 *   10. Your RDTSC says: 6.1us (internal app time)
 *   11. NIC/stack overhead = 8.3us - 6.1us = 2.2us
 */

// Simulate a market data tick
struct MarketDataPacket {
    uint64_t wire_timestamp_ns;  // Corvil hardware timestamp
    uint32_t sequence_num;
    char     symbol[8];
    double   bid_price;
    double   ask_price;
    uint32_t bid_size;
    uint32_t ask_size;
};

// Simulate processing a single tick through the trading pipeline
NO_INLINE void simulate_pipeline_hop(
    WireToWireLatencyMonitor& monitor,
    const MarketDataPacket& pkt,
    uint32_t corvil_seq_for_order)
{
    PipelineTsMap ts;
    ts.seq_num = pkt.sequence_num;

    // ──────────────────────────────────────────────────────────
    // HOP 1: NIC receive → mark entry point (FENCED rdtsc_start)
    // ──────────────────────────────────────────────────────────
    ts.mark(PipelineStage::NIC_RECV);
    // [In real code: this is called immediately after ef_vi_receive_get_timestamp
    //  or after OpenOnload recv() returns the first byte]

    // ──────────────────────────────────────────────────────────
    // HOP 2: Socket read → decode message (FIX/ITCH/OUCH parser)
    // ──────────────────────────────────────────────────────────
    ts.mark(PipelineStage::SOCKET_READ);

    // Simulate message decode (parse header, fields, etc.)
    volatile double bid = pkt.bid_price;
    volatile double ask = pkt.ask_price;
    ts.mark(PipelineStage::MSG_DECODED);

    // ──────────────────────────────────────────────────────────
    // HOP 3: Update order book
    // ──────────────────────────────────────────────────────────
    // [In real code: update sorted price level array / intrusive map]
    volatile double mid = (bid + ask) / 2.0;
    ts.mark(PipelineStage::BOOK_UPDATED);

    // ──────────────────────────────────────────────────────────
    // HOP 4: Strategy signal generation
    // ──────────────────────────────────────────────────────────
    // [In real code: evaluate alpha signal, iNAV, basis, spread model]
    volatile bool should_buy  = (bid < 100.10);
    volatile bool should_sell = (ask > 100.20);
    (void)should_buy; (void)should_sell; (void)mid;
    ts.mark(PipelineStage::SIGNAL_GEN);

    // ──────────────────────────────────────────────────────────
    // HOP 5: Build order message
    // ──────────────────────────────────────────────────────────
    FIXMessageBuilder fix;
    // Embed Corvil Sequence Tag so Corvil can correlate this order
    // to the specific market data packet that triggered it
    const char* order_msg = fix.build_new_order_single(
        "ORD123456",       // ClOrdID
        pkt.symbol,        // Symbol
        '1',               // Side = Buy
        100,               // Quantity
        bid + 0.01,        // Price
        corvil_seq_for_order  // Corvil Sequence Tag 9880 = market data seq
    );
    (void)order_msg;
    ts.mark(PipelineStage::ORDER_BUILT);

    // ──────────────────────────────────────────────────────────
    // HOP 6: Pre-trade risk check
    // ──────────────────────────────────────────────────────────
    // [In real code: check notional limit, position limit, fat finger check]
    volatile bool risk_ok = (fix.length() > 0 && bid > 0);
    (void)risk_ok;
    ts.mark(PipelineStage::RISK_CHECKED);

    // ──────────────────────────────────────────────────────────
    // HOP 7: Send order to exchange (SERIALISING rdtsc_end)
    // ──────────────────────────────────────────────────────────
    // [In real code: tcpdirect_send() or ef_vi_transmit() or send() via OpenOnload]
    ts.mark(PipelineStage::ORDER_SENT);

    // ──────────────────────────────────────────────────────────
    // Corvil wire timestamp (available post-trade from Corvil analytics API)
    // In production: fetched from Corvil REST/CSV export and matched by seq_num
    // Here: simulated as slightly larger than app internal latency
    // ──────────────────────────────────────────────────────────
    uint64_t app_ns = ts.tick_to_order_ns();
    ts.corvil_wire_ns = app_ns + 1500 + (pkt.sequence_num % 500);
    // ↑ 1500ns simulated NIC+stack overhead (Solarflare OpenOnload typical)

    // ──────────────────────────────────────────────────────────
    // Record in monitor (off hot path ideally — push to SPSC and process
    // in stats thread, but here done inline for demo)
    // ──────────────────────────────────────────────────────────
    monitor.record(ts);
}

// ============================================================================
// SECTION 9: PRACTICAL TELEMETRY USAGE EXAMPLES
// ============================================================================

/**
 * Example 1: Simple RDTSC point-to-point latency measurement
 *
 * Typical use: measure a single function or code block latency
 */
NO_INLINE void example_simple_rdtsc_measurement() {
    std::cout << "\n══════════════════════════════════════════════════════\n";
    std::cout << "  EXAMPLE 1: Simple RDTSC Measurement\n";
    std::cout << "══════════════════════════════════════════════════════\n";

    const TscCalibrator& cal = TscCalibrator::instance();
    LatencyHistogram hist("example_simple_block");

    // Measure 100k iterations of a simulated code block
    for (int i = 0; i < 100'000; ++i) {
        uint64_t t0 = rdtsc_start();         // fenced start

        // Code block under measurement
        volatile double x = 1.0;
        for (int j = 0; j < 10; ++j) x *= 1.0001;

        uint64_t t1 = rdtsc_end();           // serialising end
        hist.record(cal.to_ns(t1 - t0));     // convert ticks → ns
    }

    hist.print();
}

/**
 * Example 2: Multi-stage pipeline measurement
 *
 * Typical use: measure each hop in the trading pipeline separately
 * to identify the bottleneck
 */
NO_INLINE void example_multistage_measurement() {
    std::cout << "\n══════════════════════════════════════════════════════\n";
    std::cout << "  EXAMPLE 2: Multi-Stage Pipeline Measurement\n";
    std::cout << "══════════════════════════════════════════════════════\n";

    LatencyHistogram h_decode("decode");
    LatencyHistogram h_book("book_update");
    LatencyHistogram h_signal("signal_gen");
    LatencyHistogram h_total("total");
    const TscCalibrator& cal = TscCalibrator::instance();

    for (int i = 0; i < 10'000; ++i) {
        uint64_t t0 = rdtsc_start();           // stage 0: entry

        // Stage 1: Decode
        volatile uint64_t seq = i;
        volatile double   bid = 100.0 + (i % 100) * 0.01;
        uint64_t t1 = rdtsc_now();             // stage 1: after decode

        // Stage 2: Book update
        volatile double mid = bid + 0.01;
        uint64_t t2 = rdtsc_now();             // stage 2: after book

        // Stage 3: Signal generation
        volatile bool buy = mid < 100.50;
        (void)seq; (void)buy;
        uint64_t t3 = rdtsc_end();             // stage 3: final (serialising)

        h_decode.record(cal.to_ns(t1 - t0));
        h_book.record(cal.to_ns(t2 - t1));
        h_signal.record(cal.to_ns(t3 - t2));
        h_total.record(cal.to_ns(t3 - t0));
    }

    h_decode.print();
    h_book.print();
    h_signal.print();
    h_total.print();
}

/**
 * Example 3: TelemetryClient — async stats reporting
 *
 * Typical use: hot path records raw ticks into SPSC queue,
 * separate stats thread drains and reports periodically.
 * Zero dynamic allocation in hot path.
 */
NO_INLINE void example_telemetry_client() {
    std::cout << "\n══════════════════════════════════════════════════════\n";
    std::cout << "  EXAMPLE 3: TelemetryClient (async stats thread)\n";
    std::cout << "══════════════════════════════════════════════════════\n";

    TelemetryClient& tel = TelemetryClient::instance();
    uint32_t m_tick2order = tel.register_metric("tick_to_order_ns");
    uint32_t m_risk_check = tel.register_metric("risk_check_ns");
    uint32_t m_book_update= tel.register_metric("book_update_ns");

    std::atomic<bool> running{true};

    // Stats thread: drain queue and report every 5 seconds
    std::thread stats_thread([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            tel.drain();
            // In production: report to Prometheus/Graphite/Grafana
        }
        tel.drain();
    });

    // Hot path simulation: record latency for 100k ticks
    for (int i = 0; i < 100'000; ++i) {
        uint64_t t0 = rdtsc_start();

        // Simulate book update
        uint64_t t1 = rdtsc_now();
        volatile double x = 1.0 + i * 0.0001;

        // Simulate risk check
        uint64_t t2 = rdtsc_now();
        volatile bool ok = x > 0;
        (void)ok;

        uint64_t t3 = rdtsc_end();

        // Record raw ticks into SPSC queue — ~5ns each call
        tel.record(m_book_update,  t1 - t0, i);
        tel.record(m_risk_check,   t2 - t1, i);
        tel.record(m_tick2order,   t3 - t0, i);
    }

    running.store(false);
    stats_thread.join();
    tel.report();
}

/**
 * Example 4: Wire-to-wire latency with Corvil simulation
 */
NO_INLINE void example_wire_to_wire() {
    std::cout << "\n══════════════════════════════════════════════════════\n";
    std::cout << "  EXAMPLE 4: Wire-to-Wire Latency (Corvil simulation)\n";
    std::cout << "══════════════════════════════════════════════════════\n";

    WireToWireLatencyMonitor monitor;

    // Simulate 10k market data ticks flowing through the pipeline
    for (uint32_t i = 0; i < 10'000; ++i) {
        MarketDataPacket pkt;
        pkt.wire_timestamp_ns = rdtsc_now(); // simulated Corvil timestamp
        pkt.sequence_num      = i;
        std::strncpy(pkt.symbol, "AAPL", 8);
        pkt.bid_price = 150.0 + (i % 200) * 0.01;
        pkt.ask_price = pkt.bid_price + 0.02;
        pkt.bid_size  = 500;
        pkt.ask_size  = 800;

        simulate_pipeline_hop(monitor, pkt, i);
    }

    monitor.print_report();
}

// ============================================================================
// SECTION 10: CORVIL vs RDTSC — KEY DIFFERENCES SUMMARY
// ============================================================================

/**
 * ┌────────────────────────────────────────────────────────────────────┐
 * │                 CORVIL vs RDTSC COMPARISON                        │
 * ├──────────────────────┬─────────────────────┬──────────────────────┤
 * │ Aspect               │ RDTSC               │ Corvil CNX           │
 * ├──────────────────────┼─────────────────────┼──────────────────────┤
 * │ What it measures     │ CPU cycles (app)    │ Packet wire time     │
 * │ Location             │ Inside your process │ Passive network tap  │
 * │ Overhead on app      │ ~3-7ns per stamp    │ ZERO                 │
 * │ Code changes needed  │ YES (add marks)     │ NO (optional CST)    │
 * │ Clock source         │ CPU TSC             │ PTP/GPS locked       │
 * │ Accuracy             │ ~1ns (after calib)  │ ~10ns (GPS-locked)   │
 * │ Includes NIC time?   │ NO                  │ YES                  │
 * │ Includes kernel time │ YES (overhead only) │ YES (full wire time) │
 * │ Cross-server timing  │ NO (per process)    │ YES (network wide)   │
 * │ What it misses       │ NIC+wire overhead   │ Internal app stages  │
 * │ Best used for        │ Hot path profiling  │ SLA / compliance     │
 * ├──────────────────────┴─────────────────────┴──────────────────────┤
 * │ COMBINED USE (best practice):                                     │
 * │  RDTSC → find which code stage is slow                            │
 * │  Corvil → measure true end-to-end wire latency for compliance     │
 * │  Gap (Corvil - RDTSC) = NIC + kernel bypass + PCIe overhead       │
 * └────────────────────────────────────────────────────────────────────┘
 *
 * TYPICAL PRODUCTION LATENCY BUDGET:
 *
 *   Component                            Target       Actual (typical)
 *   ─────────────────────────────────    ──────────   ────────────────
 *   NIC receive (Solarflare + Onload)    < 1 us       0.5 - 1.5 us
 *   Market data decode (ITCH/FIX)        < 500 ns     100 - 400 ns
 *   Order book update                    < 200 ns     50  - 200 ns
 *   Strategy signal (simple)             < 500 ns     100 - 500 ns
 *   Strategy signal (complex model)      < 2 us       1   - 5 us
 *   Risk check (pre-trade)               < 500 ns     100 - 500 ns
 *   Order message build                  < 100 ns     50  - 100 ns
 *   NIC transmit (Solarflare + Onload)   < 1 us       0.5 - 1.5 us
 *   ─────────────────────────────────    ──────────   ────────────────
 *   Wire-to-Wire total                   < 5 us       2   - 10 us
 */

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Telemetry + Corvil + RDTSC Ultra Low Latency Demo      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    // Step 1: Calibrate TSC (done once at startup)
    TscCalibrator::instance();

    // Run examples
    example_simple_rdtsc_measurement();
    example_multistage_measurement();
    example_telemetry_client();
    example_wire_to_wire();

    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  All telemetry examples completed                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    return 0;
}

