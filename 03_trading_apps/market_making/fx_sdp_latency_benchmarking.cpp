/**
 * FX SDP (Single Dealer Platform) — End-to-End Latency Benchmarking
 *
 * Measurement Points:
 *   T2  NIC recv       T3  Socket read    T4  FIX parsed
 *   T5  Aggregated     T6  Markup done    T7  TCP sent
 *   T10 Order recv     T11 Risk checked   T13 LastLook sent
 *   T14 LastLook ans   T15 Exec report
 *
 * Key Segments:
 *   LP Feed Latency  = T5-T2    Distribution Lag = T7-T5
 *   Quote E2E        = T7-T2    Order-to-Fill    = T15-T10
 *
 * Compile:
 *   g++ -std=c++17 -O3 -march=native fx_sdp_latency_benchmarking.cpp -lpthread -o fx_latency_bench
 */

#include <atomic>
#include <array>
#include <cstdint>
#include <thread>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>

constexpr size_t CACHE_LINE = 64;

// ---------------------------------------------------------------------------
// TSC — ~2-7ns overhead, monotonic, no syscall
//
// x86_64 strategy:
//   tsc_start() = LFENCE + RDTSC   — prevents earlier insns from crossing
//   tsc_end()   = RDTSCP           — serialising (waits for all prior insns)
//
//   Measure like:
//     uint64_t t1 = tsc_start();
//     ... code ...
//     uint64_t t2 = tsc_end();
//     uint64_t ns = TscCal::get().to_ns(t2 - t1);
//
//   tsc_now() = RDTSC only (no fence) — cheapest, use for pipeline stage marks
//   where ordering is guaranteed by program flow
// ---------------------------------------------------------------------------
#if defined(__x86_64__) || defined(_M_X64)
  #define CPU_PAUSE() __asm__ volatile("pause" ::: "memory")

  // Cheapest: no serialisation — use for mid-pipeline stage marks (~7 cycles)
  inline uint64_t tsc_now() {
      uint64_t lo, hi;
      __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
      return (hi << 32) | lo;
  }

  // Fenced start: LFENCE prevents earlier instructions crossing the timestamp
  inline uint64_t tsc_start() {
      uint64_t lo, hi;
      __asm__ volatile("lfence\n\trdtsc\n\tlfence"
                       : "=a"(lo), "=d"(hi)
                       :: "memory");
      return (hi << 32) | lo;
  }

  // Serialising end: RDTSCP waits for all prior instructions to complete
  inline uint64_t tsc_end() {
      uint64_t lo, hi;
      uint32_t aux;                                    // processor ID (ignored)
      __asm__ volatile("rdtscp"
                       : "=a"(lo), "=d"(hi), "=c"(aux)
                       :: "memory");
      return (hi << 32) | lo;
  }

#elif defined(__aarch64__)
  #define CPU_PAUSE() __asm__ volatile("yield" ::: "memory")

  inline uint64_t tsc_now() {
      uint64_t v;
      __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
      return v;
  }
  inline uint64_t tsc_start() { return tsc_now(); }
  inline uint64_t tsc_end()   { return tsc_now(); }

#else
  #define CPU_PAUSE() do{}while(0)
  inline uint64_t tsc_now() {
      return (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
  }
  inline uint64_t tsc_start() { return tsc_now(); }
  inline uint64_t tsc_end()   { return tsc_now(); }
#endif

// ===========================================================================
// 1. TSC Calibration — convert ticks to ns, measured once at startup
// ===========================================================================
struct TscCal {
    double ns_per_tick;
    TscCal() {
        auto w1 = std::chrono::steady_clock::now();
        uint64_t c1 = tsc_now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint64_t c2 = tsc_now();
        auto w2 = std::chrono::steady_clock::now();
        double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(w2-w1).count();
        ns_per_tick = ns / (double)(c2 - c1);
    }
    uint64_t to_ns(uint64_t ticks) const { return (uint64_t)(ticks * ns_per_tick); }
    static TscCal& get() { static TscCal inst; return inst; }
};

// ===========================================================================
// 2. Lock-free Latency Histogram — 10 buckets, safe for hot path (~3ns/call)
//    Buckets (ns): <100 | 100-200 | 200-500 | 500-1k | 1k-2k
//                  2k-5k | 5k-10k | 10k-50k | 50k-100k | >100k
// ===========================================================================
class LatHist {
    static constexpr int N = 10;
    static constexpr uint64_t B[N-1] = {100,200,500,1000,2000,5000,10000,50000,100000};
    static constexpr const char* NAMES[N] = {
        "<100ns","100-200ns","200-500ns","500ns-1us",
        "1-2us","2-5us","5-10us","10-50us","50-100us",">100us"
    };
    alignas(CACHE_LINE) std::atomic<uint64_t> cnt[N]{};
    alignas(CACHE_LINE) std::atomic<uint64_t> total{0}, sum{0}, mx{0};
    std::string lbl;
public:
    explicit LatHist(const char* label="") : lbl(label) {}

    void record(uint64_t ns) noexcept {
        int b = N-1;
        for (int i = 0; i < N-1; ++i) { if (ns < B[i]) { b = i; break; } }
        cnt[b].fetch_add(1, std::memory_order_relaxed);
        total.fetch_add(1, std::memory_order_relaxed);
        sum.fetch_add(ns,  std::memory_order_relaxed);
        uint64_t cur = mx.load(std::memory_order_relaxed);
        while (ns > cur && !mx.compare_exchange_weak(cur, ns, std::memory_order_relaxed)) {}
    }

    uint64_t pct(double p) const noexcept {
        uint64_t n = total.load(std::memory_order_relaxed);
        if (!n) return 0;
        uint64_t tgt = (uint64_t)(p * n / 100.0), cum = 0;
        for (int i = 0; i < N; ++i) {
            cum += cnt[i];
            if (cum >= tgt) return (i < N-1) ? B[i] : 2000000ULL;
        }
        return 0;
    }

    void print() const {
        uint64_t n = total.load();
        if (!n) { std::cout << "  [" << lbl << "] No data\n"; return; }
        uint64_t avg = sum.load() / n;
        std::cout << "\n┌─── " << lbl << "\n";
        std::cout << "│ n=" << n
                  << "  avg=" << avg       << "ns"
                  << "  P50=" << pct(50)   << "ns"
                  << "  P95=" << pct(95)   << "ns"
                  << "  P99=" << pct(99)   << "ns"
                  << "  P99.9=" << pct(99.9) << "ns"
                  << "  max=" << mx.load() << "ns\n";
        uint64_t cum = 0;
        for (int i = 0; i < N; ++i) {
            uint64_t c = cnt[i]; cum += c;
            double p  = 100.0 * c   / n;
            double cp = 100.0 * cum / n;
            std::string bar((int)(p/4), '#');
            std::cout << "│  " << std::left  << std::setw(11) << NAMES[i]
                      << " "   << std::right << std::setw(8)  << c
                      << "  "  << std::fixed << std::setprecision(1)
                      << std::setw(5) << p  << "%"
                      << " cum=" << std::setw(5) << cp << "% |" << bar << "\n";
        }
        std::cout << "└──────────────────────────────────────\n";
    }
};

// ===========================================================================
// 3. Per-quote pipeline timestamp map — TSC mark at each pipeline stage
// ===========================================================================
enum Stage : uint8_t {
    NIC_RECV=0, SOCKET_READ, FIX_PARSED, AGGREGATED, MARKUP, TCP_SENT, N_STAGES
};

struct QuoteTsMap {
    uint64_t t[N_STAGES]{};
    uint32_t pair_id{}, lp_id{};

    // First stage: fenced RDTSC (prevent reorder with prior code)
    // Mid stages:  plain RDTSC  (cheapest, ~7 cycles)
    // Last stage:  RDTSCP       (serialising, waits for all prior insns)
    void mark(Stage s) noexcept {
        if      (s == NIC_RECV) t[s] = tsc_start();   // fenced start
        else if (s == TCP_SENT) t[s] = tsc_end();      // serialising end
        else                    t[s] = tsc_now();       // cheap mid-stage
    }

    uint64_t ns(Stage from, Stage to) const noexcept {
        if (!t[from] || !t[to]) return 0;
        return TscCal::get().to_ns(t[to] - t[from]);
    }
};

struct TradeTsMap {
    uint64_t order_recv{}, risk_ok{}, ll_sent{}, ll_ans{}, exec_rpt{};
    void mark_order()   noexcept { order_recv = tsc_start(); } // fenced: measurement start
    void mark_risk()    noexcept { risk_ok    = tsc_now();   }
    void mark_ll_sent() noexcept { ll_sent    = tsc_now();   }
    void mark_ll_ans()  noexcept { ll_ans     = tsc_now();   }
    void mark_exec()    noexcept { exec_rpt   = tsc_end();   } // serialising: measurement end

    uint64_t order_to_ack()  const noexcept { return TscCal::get().to_ns(risk_ok  - order_recv); }
    uint64_t ll_rtt()        const noexcept { return TscCal::get().to_ns(ll_ans   - ll_sent);    }
    uint64_t order_to_fill() const noexcept { return TscCal::get().to_ns(exec_rpt - order_recv); }
};

// ===========================================================================
// 4. SPSC stats queue — hot path pushes, stats thread drains (never blocks)
// ===========================================================================
struct Sample { uint64_t ns; uint8_t from, to; };

template<size_t Sz>
class SPSCStats {
    static_assert((Sz & (Sz-1)) == 0, "must be power of 2");
    alignas(CACHE_LINE) std::atomic<uint64_t> w{0};
    alignas(CACHE_LINE) std::atomic<uint64_t> r{0};
    alignas(CACHE_LINE) std::array<Sample, Sz> buf{};
public:
    // Hot path: drop if full — never block!
    bool push(Sample s) noexcept {
        uint64_t wv = w.load(std::memory_order_relaxed);
        if (wv - r.load(std::memory_order_acquire) >= Sz) return false;
        buf[wv & (Sz-1)] = s;
        w.store(wv + 1, std::memory_order_release);
        return true;
    }
    bool pop(Sample& s) noexcept {
        uint64_t rv = r.load(std::memory_order_relaxed);
        if (rv == w.load(std::memory_order_acquire)) return false;
        s = buf[rv & (Sz-1)];
        r.store(rv + 1, std::memory_order_release);
        return true;
    }
};

// ===========================================================================
// 5. Pipeline Latency Collector — all histograms + SPSC queue
// ===========================================================================
struct Collector {
    // Quote pipeline histograms
    LatHist lp_feed  {"LP Feed       T2→T5  NIC recv → Aggregated  "};
    LatHist fix_parse{"FIX Parse     T3→T4  Socket read → Parsed    "};
    LatHist agg_upd  {"Agg Update    T4→T5  Parsed → Aggregated     "};
    LatHist markup_h {"Markup        T5→T6  Aggregated → Markup done"};
    LatHist dist_lag {"Dist Lag      T5→T7  Aggregated → TCP sent   "};
    LatHist quote_e2e{"Quote E2E     T2→T7  NIC recv → TCP sent     "};
    // Trade flow histograms
    LatHist ord_ack  {"Order→Ack     T10→T11 order recv → risk ok   "};
    LatHist ll_rtt   {"LastLook RTT  T13→T14 LP confirm round trip  "};
    LatHist ord_fill {"Order→Fill    T10→T15 full trade round trip  "};

    SPSCStats<65536> q;

    // Called from hot path — push to SPSC, never blocks
    void push(const QuoteTsMap& ts) noexcept {
        auto s = [&](Stage f, Stage t) {
            uint64_t v = ts.ns(f,t);
            if (v) q.push({v,(uint8_t)f,(uint8_t)t});
        };
        s(NIC_RECV,    AGGREGATED);
        s(SOCKET_READ, FIX_PARSED);
        s(FIX_PARSED,  AGGREGATED);
        s(AGGREGATED,  MARKUP);
        s(AGGREGATED,  TCP_SENT);
        s(NIC_RECV,    TCP_SENT);
    }

    void push(const TradeTsMap& tt) noexcept {
        if (uint64_t v = tt.order_to_ack())  ord_ack.record(v);
        if (uint64_t v = tt.ll_rtt())        ll_rtt.record(v);
        if (uint64_t v = tt.order_to_fill()) ord_fill.record(v);
    }

    // Called from stats thread — drain SPSC and update histograms
    void drain() noexcept {
        Sample s;
        while (q.pop(s)) {
            if      (s.from==NIC_RECV    && s.to==AGGREGATED) lp_feed.record(s.ns);
            else if (s.from==SOCKET_READ && s.to==FIX_PARSED) fix_parse.record(s.ns);
            else if (s.from==FIX_PARSED  && s.to==AGGREGATED) agg_upd.record(s.ns);
            else if (s.from==AGGREGATED  && s.to==MARKUP)     markup_h.record(s.ns);
            else if (s.from==AGGREGATED  && s.to==TCP_SENT)   dist_lag.record(s.ns);
            else if (s.from==NIC_RECV    && s.to==TCP_SENT)   quote_e2e.record(s.ns);
        }
    }

    void print_all() const {
        std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║      FX SDP — END-TO-END LATENCY BASELINE REPORT      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        lp_feed.print();  fix_parse.print(); agg_upd.print();
        markup_h.print(); dist_lag.print();  quote_e2e.print();
        ord_ack.print();  ll_rtt.print();    ord_fill.print();
        std::cout << "\n┌── TARGET BASELINE (co-located, kernel bypass) ─────────────┐\n";
        std::cout << "│ LP Feed    P50:1-3µs   P99:5-10µs   Alert:>50µs            │\n";
        std::cout << "│ Quote E2E  P50:2-5µs   P99:10-20µs  Alert:>100µs           │\n";
        std::cout << "│ Ord→Fill   P50:200-500µs P99:<2ms   Alert:>5ms             │\n";
        std::cout << "│ LastLook rejection normal:<5%  Alert:>20%                  │\n";
        std::cout << "└─────────────────────────────────────────────────────────────┘\n";
    }
};

// ===========================================================================
// 6. Simulated pipeline — replace with real thread stage calls in production
// ===========================================================================
static QuoteTsMap simulate_quote(uint32_t pair, uint32_t lp) {
    QuoteTsMap ts; ts.pair_id = pair; ts.lp_id = lp;
    ts.mark(NIC_RECV);
    for (volatile int i = 0; i < 80;   ++i) {}   // socket overhead
    ts.mark(SOCKET_READ);
    for (volatile int i = 0; i < 200;  ++i) {}   // FIX parse
    ts.mark(FIX_PARSED);
    for (volatile int i = 0; i < 150;  ++i) {}   // aggregation
    ts.mark(AGGREGATED);
    for (volatile int i = 0; i < 60;   ++i) {}   // markup
    ts.mark(MARKUP);
    for (volatile int i = 0; i < 350;  ++i) {}   // TCP send
    ts.mark(TCP_SENT);
    return ts;
}

static TradeTsMap simulate_trade() {
    TradeTsMap tt;
    tt.mark_order();
    for (volatile int i = 0; i < 300;  ++i) {}   // risk check
    tt.mark_risk();
    for (volatile int i = 0; i < 500;  ++i) {}   // last look send
    tt.mark_ll_sent();
    for (volatile int i = 0; i < 2000; ++i) {}   // LP response
    tt.mark_ll_ans();
    for (volatile int i = 0; i < 400;  ++i) {}   // exec report build
    tt.mark_exec();
    return tt;
}

// ===========================================================================
// 7. Main
// ===========================================================================
int main() {
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║   FX SDP Latency Benchmarking Framework               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    std::cout << "Calibrating TSC...\n";
    auto& cal = TscCal::get();
    std::cout << "TSC: " << std::fixed << std::setprecision(3)
              << (1.0 / cal.ns_per_tick) << " GHz  ("
              << cal.ns_per_tick << " ns/tick)\n";

    Collector col;

    std::cout << "\n[1] Simulating 200,000 quotes through pipeline...\n";
    for (int i = 0; i < 200'000; ++i)
        col.push(simulate_quote(i % 20, i % 8));

    std::cout << "[2] Simulating 5,000 trades (order->fill)...\n";
    for (int i = 0; i < 5'000; ++i)
        col.push(simulate_trade());

    col.drain();
    col.print_all();

    std::cout << "\n  Production deployment:\n";
    std::cout << "  - Add ts.mark(Stage::*) calls in each pipeline thread\n";
    std::cout << "  - Push samples via col.push() from hot-path (non-blocking)\n";
    std::cout << "  - Run col.drain() from dedicated stats thread every 100us\n";
    std::cout << "  - Export histograms to InfluxDB/Grafana every 30 seconds\n\n";
    return 0;
}

/*
 * ENVIRONMENTS
 * ════════════
 * UAT     : Replay PROD pcap at 1x → 30-min histogram = baseline
 *             tcpreplay --intf1=eth0 --multiplier=1 lp_feed.pcap
 *
 * Pre-Prod: 10x stress → find P99.9 breaking point
 *             tcpreplay --intf1=eth0 --multiplier=10 lp_feed.pcap
 *
 * Prod    : In-process marks (~2ns/call), async SPSC drain
 *           Grafana alerts:
 *             LP Feed P99   > 50us  -> LP connectivity issue
 *             Quote E2E P99 > 100us -> aggregation bottleneck
 *             Ord->Fill P99 > 5ms   -> last look timeout / LP slow
 *             LL reject     > 20%   -> LP pricing issue
 */

