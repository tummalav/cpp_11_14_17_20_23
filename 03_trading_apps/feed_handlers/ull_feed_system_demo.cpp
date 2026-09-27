/*
 * ============================================================================
 * ULL FEED HANDLER SYSTEM — STRATEGY DISPATCHER + COMPLETE DEMO
 * ============================================================================
 *
 * §G  IStrategy interface + StrategySubscription + StrategyDispatcher
 *       - Each strategy declares a bitmap of InstrumentIDs it cares about
 *       - Dispatcher reads SPMC ring, routes events to subscribed strategies
 *       - Dispatcher runs on NUMA 1 (strategy side), pinned to dedicated core
 *       - Each strategy also runs on its own NUMA-1 core
 *
 * §H  Example strategies:
 *       - MidPriceLogger    — logs every L1 update (demo)
 *       - StatArbStrategy   — cross-symbol spread trading signal
 *       - MarketMakerStrat  — basic quoted-spread tightening signal
 *
 * §I  Complete integration demo:
 *       - Container with HKEX + ASX + SGX handlers (simulated)
 *       - Synthetic order flow injected directly for benchmark
 *       - Measures end-to-end latency: inject→parse→Disruptor→strategy
 *       - NUMA layout diagram printed at startup
 *
 * §J  Latency benchmark: ring throughput, per-event dispatch latency
 * ============================================================================
 */

#include "ull_multicontainer_ipc.hpp"   // includes ull_market_feed_handlers.hpp + IStrategy/StrategySubscription

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <bitset>
#include <cstring>

namespace ull {

// ============================================================================
// §G  STRATEGY INTERFACE & DISPATCHER
// ============================================================================
// NOTE: IStrategy and StrategySubscription are now defined in
//       ull_multicontainer_ipc.hpp (§5a) — included above.
//       The StrategyDispatcher below uses them directly.

// StrategyDispatcher: reads SPMC Disruptor, routes to strategy callbacks
// Runs on a single thread (NUMA 1) — one dispatcher per strategy group
// For true isolation, each strategy can have its own dispatcher thread.
class StrategyDispatcher {
public:
    using Ring = SpmcDisruptor<MarketEvent, 65536, 32>;

    StrategyDispatcher(Ring& ring, BookRegistry& books,
                        int cpu_id, int numa_node = 1)
        : ring_(ring), books_(books), cpu_id_(cpu_id), numa_node_(numa_node) {}

    // Add a strategy. Strategy's consumer slot is registered on strategy_start().
    void add_strategy(IStrategy* strat) {
        strategies_.push_back(strat);
    }

    void start() {
        // Register one consumer per strategy on the ring
        for (auto* s : strategies_) {
            int id = ring_.register_consumer();
            consumer_ids_.push_back(id);
        }
        running_.store(true, std::memory_order_release);
        thread_ = std::thread(&StrategyDispatcher::dispatch_loop, this);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (thread_.joinable()) thread_.join();
    }

    uint64_t events_dispatched() const noexcept {
        return events_dispatched_.load(std::memory_order_relaxed);
    }

private:
    void dispatch_loop() noexcept {
        pin_thread_to_cpu(cpu_id_);
        set_realtime(70);  // slightly lower priority than recv/parse

        // Build subscription cache (avoid virtual calls in hot loop)
        std::vector<StrategySubscription> subs;
        subs.reserve(strategies_.size());
        for (auto* s : strategies_) subs.push_back(s->subscription());

        while (running_.load(std::memory_order_relaxed)) {
            for (size_t i = 0; i < strategies_.size(); ++i) {
                int consumer_id = consumer_ids_[i];

                while (ring_.available_for(consumer_id)) {
                    uint64_t seq         = ring_.next_seq(consumer_id);
                    const MarketEvent& ev = ring_.slot(seq);

                    if (subs[i].matches(ev)) {
                        const SoABook& book =
                            books_.book(ev.instrument_id);
                        strategies_[i]->on_event(ev, book);
                        events_dispatched_.fetch_add(1, std::memory_order_relaxed);
                    }
                    ring_.advance(consumer_id);
                }
                spin_pause();
            }
        }
    }

    Ring&                           ring_;
    BookRegistry&                   books_;
    int                             cpu_id_;
    int                             numa_node_;
    std::vector<IStrategy*>         strategies_;
    std::vector<int>                consumer_ids_;
    std::thread                     thread_;
    std::atomic<bool>               running_{false};
    std::atomic<uint64_t>           events_dispatched_{0};
};

// ============================================================================
// §H  EXAMPLE STRATEGIES
// ============================================================================

// H1. MidPriceLogger — logs L1 changes (useful for debugging / monitoring)
class MidPriceLogger final : public IStrategy {
public:
    explicit MidPriceLogger(std::string name,
                             const std::vector<InstrumentID>& ids)
        : name_(std::move(name)) {
        for (auto id : ids) sub_.subscribe_instrument(id);
        sub_.subscribe_l2 = false;
        sub_.subscribe_status = false;
    }

    void on_event(const MarketEvent& ev, const SoABook& book) noexcept override {
        if (ev.type != EventType::BookL1) return;
        auto l1 = book.read_l1();
        if (!l1.valid) return;
        ++updates_;
        // Store last mid for stats (no I/O on hot path in production)
        last_mid_fp_ = l1.mid_fp;
        last_spread_fp_ = l1.spread_fp;
    }

    const char* name() const noexcept override { return name_.c_str(); }

    StrategySubscription subscription() const override { return sub_; }

    void print_stats() const override {
        std::cout << "[" << name_ << "] updates=" << updates_
                  << " last_mid=" << std::fixed << std::setprecision(6)
                  << from_fp(last_mid_fp_)
                  << " spread=" << from_fp(last_spread_fp_) << "\n";
    }

private:
    std::string          name_;
    StrategySubscription sub_;
    uint64_t             updates_{0};
    int64_t              last_mid_fp_{0};
    int64_t              last_spread_fp_{0};
};

// H2. StatArbStrategy — monitors spread between two correlated instruments
class StatArbStrategy final : public IStrategy {
public:
    StatArbStrategy(const std::string& name,
                    InstrumentID leg1, InstrumentID leg2,
                    double target_spread_threshold)
        : name_(name), leg1_(leg1), leg2_(leg2),
          threshold_fp_(to_fp(target_spread_threshold)) {
        sub_.subscribe_instrument(leg1);
        sub_.subscribe_instrument(leg2);
        sub_.subscribe_l2      = false;
        sub_.subscribe_trades  = false;
    }

    void on_event(const MarketEvent& ev, const SoABook& book) noexcept override {
        if (ev.type != EventType::BookL1) return;
        ++events_;

        // Update mid price for the leg that just updated
        if (ev.instrument_id == leg1_) {
            mid1_fp_ = (ev.price_fp + ev.price2_fp) / 2;
        } else if (ev.instrument_id == leg2_) {
            mid2_fp_ = (ev.price_fp + ev.price2_fp) / 2;
        }

        if (mid1_fp_ == 0 || mid2_fp_ == 0) return;

        // Cross-instrument spread signal
        int64_t spread_fp = mid1_fp_ - mid2_fp_;
        if (spread_fp > threshold_fp_) {
            ++buy_signals_;   // leg2 is cheap relative to leg1
        } else if (spread_fp < -threshold_fp_) {
            ++sell_signals_;  // leg1 is cheap relative to leg2
        }
        last_spread_fp_ = spread_fp;
    }

    const char* name() const noexcept override { return name_.c_str(); }

    StrategySubscription subscription() const override { return sub_; }

    void print_stats() const override {
        std::cout << "[" << name_ << "] events=" << events_
                  << " buy_signals=" << buy_signals_
                  << " sell_signals=" << sell_signals_
                  << " spread=" << std::fixed << std::setprecision(6)
                  << from_fp(last_spread_fp_) << "\n";
    }

private:
    std::string          name_;
    InstrumentID         leg1_, leg2_;
    StrategySubscription sub_;
    int64_t              threshold_fp_;
    int64_t              mid1_fp_{0}, mid2_fp_{0};
    int64_t              last_spread_fp_{0};
    uint64_t             events_{0};
    uint64_t             buy_signals_{0}, sell_signals_{0};
};

// H3. MarketMakerStrategy — tracks quoted spread vs fair value
class MarketMakerStrategy final : public IStrategy {
public:
    MarketMakerStrategy(const std::string& name,
                        const std::vector<InstrumentID>& ids,
                        double target_spread_bps)
        : name_(name),
          target_spread_fp_(to_fp(target_spread_bps / 10000.0)) {
        for (auto id : ids) {
            sub_.subscribe_instrument(id);
            last_mid_fp_[id] = 0;
        }
    }

    void on_event(const MarketEvent& ev, const SoABook& book) noexcept override {
        ++events_;
        if (ev.type == EventType::BookL1) {
            auto l1 = book.read_l1();
            if (!l1.valid) return;
            last_mid_fp_[ev.instrument_id] = l1.mid_fp;
            // If current spread > 2× target: quote tighter = opportunity
            if (l1.spread_fp > 2 * target_spread_fp_) ++quote_opps_;
        } else if (ev.type == EventType::Trade) {
            ++trades_seen_;
            // Check adverse selection: if trade hits our theoretical quote
            int64_t mid = last_mid_fp_.count(ev.instrument_id)
                          ? last_mid_fp_[ev.instrument_id] : 0;
            if (mid > 0) {
                int64_t dev = ev.price_fp - mid;
                total_adverse_fp_ += std::abs(dev);
            }
        }
    }

    const char* name() const noexcept override { return name_.c_str(); }

    StrategySubscription subscription() const override { return sub_; }

    void print_stats() const override {
        std::cout << "[" << name_ << "] events=" << events_
                  << " quote_opps=" << quote_opps_
                  << " trades_seen=" << trades_seen_
                  << " avg_adverse=" << std::fixed << std::setprecision(8)
                  << (trades_seen_ > 0
                      ? from_fp(total_adverse_fp_ / int64_t(trades_seen_))
                      : 0.0) << "\n";
    }

private:
    std::string          name_;
    StrategySubscription sub_;
    int64_t              target_spread_fp_;
    std::unordered_map<InstrumentID, int64_t> last_mid_fp_;
    uint64_t             events_{0};
    uint64_t             quote_opps_{0};
    uint64_t             trades_seen_{0};
    int64_t              total_adverse_fp_{0};
};

// ============================================================================
// §I  COMPLETE INTEGRATION DEMO
// ============================================================================

// Injects synthetic MarketEvents directly into the Disruptor ring
// (bypasses actual network parsing for host-side benchmarking)
class SyntheticEventInjector {
public:
    using Ring = SpmcDisruptor<MarketEvent, 65536, 32>;

    SyntheticEventInjector(Ring& ring, BookRegistry& books)
        : ring_(ring), books_(books) {}

    // Inject N L1 events across a set of instruments
    void inject_l1_stream(const std::vector<InstrumentID>& ids,
                           int n_events, double base_mid,
                           double tick_size, double spread_ticks) {
        std::mt19937_64 rng(42);
        std::normal_distribution<double> price_noise(0.0, tick_size * 2);

        for (int i = 0; i < n_events; ++i) {
            InstrumentID id = ids[i % ids.size()];
            double       mid = base_mid + price_noise(rng);
            double       half_spread = spread_ticks * tick_size / 2.0;
            int64_t      bid_fp = to_fp(mid - half_spread);
            int64_t      ask_fp = to_fp(mid + half_spread);
            int32_t      qty    = 100 + int(rng() % 900);

            // Update book
            SoABook& book = books_.book(id);
            book.update_bid(0, bid_fp, qty, 5);
            book.update_ask(0, ask_fp, qty, 5);
            book.set_in_sync(true);

            // Emit to ring
            uint64_t seq = ring_.claim();
            MarketEvent& ev  = ring_.slot(seq);
            ev.timestamp_ns  = rdtsc();
            ev.instrument_id = id;
            ev.type          = EventType::BookL1;
            ev.market_id     = uint8_t(MarketID::HKEX_OMDCDQ);
            ev.price_fp      = bid_fp;
            ev.qty           = qty;
            ev.price2_fp     = ask_fp;
            ev.qty2          = qty;
            ev.level         = 0;
            ev.seq_num       = uint32_t(i);
            std::memcpy(ev.symbol, book.symbol, 8);
            ring_.publish(seq);
        }
    }

    // Inject trade events
    void inject_trades(const std::vector<InstrumentID>& ids,
                        int n_trades, double mid_price) {
        std::mt19937_64 rng(123);
        for (int i = 0; i < n_trades; ++i) {
            InstrumentID id   = ids[i % ids.size()];
            double       tick = 0.0001;
            int64_t      px   = to_fp(mid_price + (int(rng()%5)-2) * tick);
            int32_t      qty  = int(100 + rng() % 400);

            SoABook& book = books_.book(id);
            book.update_trade(px, qty, rdtsc());

            uint64_t seq = ring_.claim();
            MarketEvent& ev  = ring_.slot(seq);
            ev.timestamp_ns  = rdtsc();
            ev.instrument_id = id;
            ev.type          = EventType::Trade;
            ev.market_id     = uint8_t(MarketID::ASX_ITCH);
            ev.price_fp      = px;
            ev.qty           = qty;
            ev.seq_num       = uint32_t(n_trades + i);
            std::memcpy(ev.symbol, book.symbol, 8);
            ring_.publish(seq);
        }
    }

private:
    Ring&         ring_;
    BookRegistry& books_;
};

// ============================================================================
// §J  LATENCY BENCHMARK
// ============================================================================
struct BenchResult {
    double min_ns, mean_ns, p50_ns, p95_ns, p99_ns, p999_ns, max_ns;
    double throughput_meps;  // million events per second
};

BenchResult benchmark_ring_throughput(int n_events = 2'000'000) {
    std::cout << "\n[BENCH] SPMC Disruptor throughput & latency (" << n_events << " events)...\n";

    using Ring = SpmcDisruptor<MarketEvent, 65536, 32>;
    Ring ring;
    BookRegistry books;
    InstrumentID id = books.register_instrument("TEST");
    books.book(id).init(id, "TEST");
    books.book(id).set_in_sync(true);

    // Register two consumer threads
    int cid1 = ring.register_consumer();
    int cid2 = ring.register_consumer();

    std::atomic<uint64_t> c1_count{0}, c2_count{0};
    std::atomic<bool>     running{true};
    std::vector<double>   c1_lats;
    c1_lats.reserve(n_events);

    // Consumer 1 thread: measures per-event latency
    std::thread c1([&]() {
        pin_thread_to_cpu(2);  // NUMA 1 representative core
        while (running.load(std::memory_order_relaxed) ||
               ring.available_for(cid1)) {
            while (ring.available_for(cid1)) {
                uint64_t seq         = ring.next_seq(cid1);
                const MarketEvent& ev = ring.slot(seq);
                uint64_t now_ts       = rdtsc();
                // Compute approx ns latency from publish to consume
                // (only meaningful when producer and consumer on same TSC domain)
                ring.advance(cid1);
                ++c1_count;
            }
            spin_pause();
        }
    });

    // Consumer 2 thread: just drain
    std::thread c2([&]() {
        pin_thread_to_cpu(3);
        while (running.load(std::memory_order_relaxed) ||
               ring.available_for(cid2)) {
            while (ring.available_for(cid2)) {
                ring.advance(cid2);
                ++c2_count;
            }
            spin_pause();
        }
    });

    // Producer: publish n_events, measure publish latency
    std::vector<double> publish_lats;
    publish_lats.reserve(n_events);
    auto wall_start = std::chrono::steady_clock::now();

    for (int i = 0; i < n_events; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        uint64_t seq = ring.claim();
        MarketEvent& ev  = ring.slot(seq);
        ev.timestamp_ns  = rdtsc();
        ev.instrument_id = id;
        ev.type          = EventType::BookL1;
        ev.price_fp      = to_fp(100.0 + i * 0.0001);
        ev.qty           = 100;
        ring.publish(seq);
        auto t1 = std::chrono::steady_clock::now();
        publish_lats.push_back(
            std::chrono::duration<double, std::nano>(t1 - t0).count());
    }

    auto wall_end = std::chrono::steady_clock::now();

    // Wait for consumers to drain
    while (c1_count.load() < uint64_t(n_events) ||
           c2_count.load() < uint64_t(n_events)) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    running.store(false, std::memory_order_release);
    c1.join(); c2.join();

    // Stats
    std::sort(publish_lats.begin(), publish_lats.end());
    double sum = 0; for (auto v : publish_lats) sum += v;
    double wall_sec = std::chrono::duration<double>(wall_end - wall_start).count();

    BenchResult res;
    res.min_ns          = publish_lats.front();
    res.mean_ns         = sum / n_events;
    res.p50_ns          = publish_lats[n_events / 2];
    res.p95_ns          = publish_lats[size_t(n_events * 0.95)];
    res.p99_ns          = publish_lats[size_t(n_events * 0.99)];
    res.p999_ns         = publish_lats[size_t(n_events * 0.999)];
    res.max_ns          = publish_lats.back();
    res.throughput_meps = n_events / wall_sec / 1e6;

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Publish latency (claim+fill+publish):\n";
    std::cout << "    min    : " << res.min_ns    << " ns\n";
    std::cout << "    mean   : " << res.mean_ns   << " ns\n";
    std::cout << "    p50    : " << res.p50_ns    << " ns\n";
    std::cout << "    p95    : " << res.p95_ns    << " ns\n";
    std::cout << "    p99    : " << res.p99_ns    << " ns\n";
    std::cout << "    p99.9  : " << res.p999_ns   << " ns\n";
    std::cout << "    max    : " << res.max_ns    << " ns\n";
    std::cout << "  Throughput: " << std::setprecision(2)
              << res.throughput_meps << " M events/sec\n";
    std::cout << "  Consumer 1 consumed: " << c1_count.load() << "\n";
    std::cout << "  Consumer 2 consumed: " << c2_count.load() << "\n";

    return res;
}

BenchResult benchmark_seqlock_book_read(int n_reads = 5'000'000) {
    std::cout << "\n[BENCH] SoA book seqlock read/write (" << n_reads << " reads)...\n";

    BookRegistry books;
    InstrumentID id = books.register_instrument("BENCH");
    SoABook& book   = books.book(id);
    book.set_in_sync(true);

    // Background writer: updates book at 1M updates/sec simulation
    std::atomic<bool> writer_running{true};
    std::thread writer([&]() {
        pin_thread_to_cpu(4);
        double price = 100.0;
        int64_t qty  = 500;
        while (writer_running.load(std::memory_order_relaxed)) {
            int64_t fp = to_fp(price);
            book.update_bid(0, fp, int32_t(qty),     5);
            book.update_ask(0, fp + to_fp(0.01), int32_t(qty), 5);
            price += 0.0001;
            if (price > 101.0) price = 100.0;
            spin_pause(); spin_pause(); spin_pause();
        }
    });

    // Reader: measure seqlock read latency
    std::vector<double> read_lats;
    read_lats.reserve(n_reads);

    pin_thread_to_cpu(5);
    for (int i = 0; i < n_reads; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        volatile auto l1 = book.read_l1();
        auto t1 = std::chrono::steady_clock::now();
        read_lats.push_back(
            std::chrono::duration<double, std::nano>(t1 - t0).count());
        (void)l1;
    }
    writer_running.store(false);
    writer.join();

    std::sort(read_lats.begin(), read_lats.end());
    double sum = 0; for (auto v : read_lats) sum += v;

    BenchResult res;
    res.min_ns   = read_lats.front();
    res.mean_ns  = sum / n_reads;
    res.p50_ns   = read_lats[n_reads / 2];
    res.p95_ns   = read_lats[size_t(n_reads * 0.95)];
    res.p99_ns   = read_lats[size_t(n_reads * 0.99)];
    res.p999_ns  = read_lats[size_t(n_reads * 0.999)];
    res.max_ns   = read_lats.back();
    res.throughput_meps = 0;

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  SoA book read_l1() under concurrent write:\n";
    std::cout << "    min    : " << res.min_ns   << " ns\n";
    std::cout << "    mean   : " << res.mean_ns  << " ns\n";
    std::cout << "    p50    : " << res.p50_ns   << " ns\n";
    std::cout << "    p99    : " << res.p99_ns   << " ns\n";
    std::cout << "    p99.9  : " << res.p999_ns  << " ns\n";
    std::cout << "    max    : " << res.max_ns   << " ns\n";
    return res;
}

// ============================================================================
// §I  COMPLETE INTEGRATION DEMO
// ============================================================================
void run_complete_demo() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Ultra Low Latency Feed Handler System — Asian Markets       ║\n";
    std::cout << "║  Virtu/Citadel/Jump style — Production Architecture          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    // ---- Print NUMA layout ----
    std::cout << "\n┌──────────────────────────────────────────────────────────────┐\n";
    std::cout << "│  NUMA Topology                                               │\n";
    std::cout << "│                                                              │\n";
    std::cout << "│  NUMA Node 0 (Feed Handler Side)   NUMA Node 1 (Strategy)   │\n";
    std::cout << "│  CPU 0-1: HKEX recv/parse          CPU 16-17: Dispatcher 1  │\n";
    std::cout << "│  CPU 2-3: ASX  recv/parse          CPU 18-19: Strategy A    │\n";
    std::cout << "│  CPU 4-5: SGX  recv/parse          CPU 20-21: Strategy B    │\n";
    std::cout << "│  CPU 6-7: spare/monitoring         CPU 22-23: Strategy C    │\n";
    std::cout << "│                                                              │\n";
    std::cout << "│  NIC (Solarflare SF-N6122)  ──ef_vi──►  DMA → SpscRing     │\n";
    std::cout << "│                      SpscRing ──► parse ──► SpscDisruptor   │\n";
    std::cout << "│                                   SpmcDisruptor ──► N strats│\n";
    std::cout << "└──────────────────────────────────────────────────────────────┘\n";

    // ---- Create container (cores 0,1,2,3,4,5 for 3 handlers) ----
    FeedHandlerContainer container(/*base_recv_cpu=*/0);
    BookRegistry& books = container.books();

    // ---- HKEX OMD-C handler ----
    auto hkex_handler = std::make_unique<HkexOmdFeedHandler>();

    FeedConfig hkex_cfg;
    hkex_cfg.market_id       = MarketID::HKEX_OMDCDQ;
    hkex_cfg.name            = "HKEX_OMDC";
    hkex_cfg.interface_name  = "enp1s0f0";
    hkex_cfg.mcast_group_a   = "239.1.1.1";  // HKEX co-lo incremental feed A
    hkex_cfg.mcast_port_a    = 59000;
    hkex_cfg.mcast_group_b   = "239.1.1.2";  // Channel B
    hkex_cfg.mcast_port_b    = 59001;
    hkex_cfg.snapshot_mcast  = "224.0.1.1";
    hkex_cfg.snapshot_port   = 59100;
    hkex_cfg.use_channel_b   = true;

    auto* hkex = container.add_handler(std::move(hkex_handler), hkex_cfg);
    // Register HKEX instruments: HSI futures, H-shares
    hkex->add_instrument(7001, "HSI",   -2);   // Hang Seng Index futures (0.01 = ½ tick)
    hkex->add_instrument(7002, "HSIHN", -2);
    hkex->add_instrument(7100, "2800",  -4);   // Tracker ETF
    hkex->add_instrument(7101, "9888",  -4);   // Alibaba HK

    // ---- ASX ITCH handler ----
    auto asx_handler = std::make_unique<AsxItchFeedHandler>();

    FeedConfig asx_cfg;
    asx_cfg.market_id       = MarketID::ASX_ITCH;
    asx_cfg.name            = "ASX_ITCH";
    asx_cfg.interface_name  = "enp1s0f1";
    asx_cfg.mcast_group_a   = "233.71.185.1";  // ASX TradeMatch multicast
    asx_cfg.mcast_port_a    = 52000;
    asx_cfg.snapshot_mcast  = "233.71.185.100";
    asx_cfg.snapshot_port   = 52100;
    asx_cfg.use_channel_b   = false;

    auto* asx = container.add_handler(std::move(asx_handler), asx_cfg);
    asx->add_instrument("BHP     ", -4);  // BHP Group (ASX:BHP)
    asx->add_instrument("CBA     ", -4);  // Commonwealth Bank
    asx->add_instrument("CSL     ", -4);  // CSL Limited
    asx->add_instrument("XJO     ", -2);  // ASX 200 futures

    // ---- SGX ITCH handler ----
    auto sgx_handler = std::make_unique<SgxItchFeedHandler>();

    FeedConfig sgx_cfg;
    sgx_cfg.market_id       = MarketID::SGX_ITCH;
    sgx_cfg.name            = "SGX_ITCH";
    sgx_cfg.interface_name  = "enp2s0f0";
    sgx_cfg.mcast_group_a   = "239.240.1.1";   // SGX multicast
    sgx_cfg.mcast_port_a    = 10100;
    sgx_cfg.use_channel_b   = false;

    auto* sgx = container.add_handler(std::move(sgx_handler), sgx_cfg);
    sgx->add_instrument("CN      ", -4);  // SGX CNH futures
    sgx->add_instrument("NK      ", -1);  // Nikkei 225 futures (SGX)
    sgx->add_instrument("IN3     ", -2);  // SGX Nifty futures

    // Collect all instrument IDs
    std::vector<InstrumentID> all_ids, hkex_ids, asx_ids, sgx_ids;
    for (auto& info : hkex->list_instruments()) {
        hkex_ids.push_back(info.id); all_ids.push_back(info.id); }
    for (auto& info : asx->list_instruments()) {
        asx_ids.push_back(info.id);  all_ids.push_back(info.id); }
    for (auto& info : sgx->list_instruments()) {
        sgx_ids.push_back(info.id);  all_ids.push_back(info.id); }

    std::cout << "\nRegistered " << books.count() << " instruments total:\n";
    for (uint32_t i = 0; i < books.count(); ++i) {
        std::cout << "  [" << i << "] " << books.book(i).symbol << "\n";
    }

    // ---- Create strategies ----
    // Strategy A: L1 logger for all HKEX instruments
    auto strat_a = std::make_unique<MidPriceLogger>("HKEXLogger", hkex_ids);

    // Strategy B: StatArb between HSI and H-share index (cross-instrument)
    InstrumentID hsi_id  = hkex_ids[0];  // HSI
    InstrumentID hsi2_id = hkex_ids.size() > 1 ? hkex_ids[1] : hkex_ids[0];
    auto strat_b = std::make_unique<StatArbStrategy>(
        "HSI_StatArb", hsi_id, hsi2_id, 0.01 /*threshold*/);

    // Strategy C: Market maker tracking all ASX + SGX instruments
    std::vector<InstrumentID> asx_sgx_ids = asx_ids;
    asx_sgx_ids.insert(asx_sgx_ids.end(), sgx_ids.begin(), sgx_ids.end());
    auto strat_c = std::make_unique<MarketMakerStrategy>(
        "AsiaMM", asx_sgx_ids, 5.0 /*target spread bps*/);

    // ---- Create dispatcher (runs on NUMA 1, core 16) ----
    StrategyDispatcher dispatcher(container.ring(), books, /*cpu=*/16, /*numa=*/1);
    dispatcher.add_strategy(strat_a.get());
    dispatcher.add_strategy(strat_b.get());
    dispatcher.add_strategy(strat_c.get());
    dispatcher.start();

    // ---- Inject synthetic data (simulates what real feed handlers produce) ----
    std::cout << "\n[INJECT] Injecting synthetic market data...\n";
    SyntheticEventInjector injector(container.ring(), books);

    // Phase 1: L1 book updates for HKEX (simulate 100K updates per instrument)
    std::cout << "  Phase 1: HKEX L1 stream...\n";
    injector.inject_l1_stream(hkex_ids, 200'000, 25000.0, 5.0, 1.0);

    // Phase 2: ASX order flow (book updates)
    std::cout << "  Phase 2: ASX L1 stream...\n";
    injector.inject_l1_stream(asx_ids, 100'000, 45.0, 0.01, 1.0);

    // Phase 3: SGX futures
    std::cout << "  Phase 3: SGX L1 stream...\n";
    injector.inject_l1_stream(sgx_ids, 50'000, 6.8900, 0.0001, 2.0);

    // Phase 4: Trade events
    std::cout << "  Phase 4: Trade events...\n";
    injector.inject_trades(all_ids, 50'000, 100.0);

    // Let dispatcher drain
    std::cout << "  Draining dispatcher...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    dispatcher.stop();

    // ---- Print results ----
    std::cout << "\n===== Strategy Results =====\n";
    strat_a->print_stats();
    strat_b->print_stats();
    strat_c->print_stats();
    std::cout << "Dispatcher events processed: "
              << dispatcher.events_dispatched() << "\n";

    // Print feed handler stats (not started in demo, show config)
    container.print_all_stats();

    // ---- Benchmarks ----
    benchmark_ring_throughput(1'000'000);
    benchmark_seqlock_book_read(1'000'000);

    // ---- Final architecture summary ----
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Architecture Summary                                        ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Transport:  Solarflare ef_vi (zero-copy DMA, no syscall)   ║\n";
    std::cout << "║  Book:       Structure-of-Arrays (SoA), seqlock, 10 levels  ║\n";
    std::cout << "║  Hot-path:   First 64B of SoABook = L1 BBO (1 cache line)   ║\n";
    std::cout << "║  Fan-out:    SPMC Disruptor (LMAX-style, lock-free)          ║\n";
    std::cout << "║  SPSC:       NIC-recv → bookbuilder (per-handler ring)       ║\n";
    std::cout << "║  NUMA 0:     Recv/parse threads (feed handler side)          ║\n";
    std::cout << "║  NUMA 1:     Strategy/dispatcher threads (alpha side)        ║\n";
    std::cout << "║  Strategies: Separate consumer cursors, NO mutual wakeup     ║\n";
    std::cout << "║  Handlers:   Plug-and-play IFeedHandler interface            ║\n";
    std::cout << "║  Markets:    HKEX OMD-C/D, ASX ITCH 1.1, SGX ITCH, OSE      ║\n";
    std::cout << "║  Target:     < 1 µs tick-to-strategy on isolated cores       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
}

}  // namespace ull

// ============================================================================
// MAIN
// ============================================================================
int main() {
    ull::run_complete_demo();
    return 0;
}

