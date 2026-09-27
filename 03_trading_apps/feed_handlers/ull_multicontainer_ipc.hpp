/*
 * ============================================================================
 * ULL MULTI-CONTAINER IPC — SHARED-MEMORY LOCK-FREE INTER-PROCESS FEED HUB
 * ============================================================================
 *
 * Answers the question:
 *   "Can N strategies and N feed handlers run in the SAME container
 *    communicating via lock-free queues, OR in SEPARATE containers
 *    communicating via shared-memory lock-free queues when isolated
 *    containers run on the same server / same NUMA node?"
 *
 * Answer: YES — both topologies are implemented here.
 *
 * ── Mode 1: Monolithic (single process) ─────────────────────────────────────
 *
 *   ┌────────────────────── One Process ────────────────────────────────────┐
 *   │  FeedHandlerContainer                                                 │
 *   │    ├─ HkexOmdFeedHandler ─► SpscRing ─► Parser ──────┐               │
 *   │    ├─ AsxItchFeedHandler  ─► SpscRing ─► Parser ──►  in-proc         │
 *   │    └─ SgxItchFeedHandler  ─► SpscRing ─► Parser ──►  SpmcDisruptor   │
 *   │                                                          │            │
 *   │   StrategyDispatcher  ◄───────────────────────────────────────────────│
 *   │     ├─ Strategy A  (sub: HSI, HSIHN)                   │            │
 *   │     ├─ Strategy B  (sub: BHP, CBA)                     │            │
 *   │     └─ Strategy C  (sub: ALL)                          │            │
 *   └───────────────────────────────────────────────────────────────────────┘
 *
 *   Communication: in-process SpmcDisruptor → ~15 ns tick-to-strategy.
 *   Max consumers: MAX_CONSUMERS template param (default 32).
 *
 * ── Mode 2: Multi-container (separate processes, same server) ───────────────
 *
 *   Same NUMA node favored: SHM on huge pages, bridge pinned to NUMA 0 core.
 *
 *   ┌─── Process A: Feed Handler Container (NUMA 0) ─────────────────────────┐
 *   │  FeedHandlerContainer (N handlers)                                     │
 *   │    └─► in-proc SpmcDisruptor                                           │
 *   │               └─► ShmPublisherBridge (dedicated core)                 │
 *   │                         └─► SHM SpmcRing (mmap MAP_SHARED)            │
 *   └──────────────────────────────────────────────────────────────────────── ┘
 *                                       │
 *                        ┌──────────────┘  (shm_open / mmap)
 *                        ▼
 *   ┌─── Process B: Strategy Container A (NUMA 1) ───────────────────────────┐
 *   │  ShmSubscriberBridge (consumer 0) ─► in-proc SpmcDisruptor            │
 *   │    └─► StrategyDispatcher ─► Strategy 1, Strategy 2                   │
 *   └─────────────────────────────────────────────────────────────────────── ┘
 *   ┌─── Process C: Strategy Container B (NUMA 1) ───────────────────────────┐
 *   │  ShmSubscriberBridge (consumer 1) ─► in-proc SpmcDisruptor            │
 *   │    └─► StrategyDispatcher ─► Strategy 3, Strategy 4                   │
 *   └─────────────────────────────────────────────────────────────────────── ┘
 *
 *   Communication: SHM SpmcRing → ~40-80 ns tick-to-strategy (same socket).
 *   Same NUMA: ring allocated on NUMA 0 L3 (mbind on Linux). Cache-line
 *   transfers across processes via MESI snooping — no kernel involvement.
 *
 * ── SHM Ring Design ─────────────────────────────────────────────────────────
 *
 *   ShmSpmcRing layout in shared memory (contiguous):
 *     [RingMeta      64B — magic, capacity, name             ]  CL 0
 *     [ProducerSeq   64B — atomic claimed + published        ]  CL 1
 *     [ConsumerCount 64B — atomic count (how many subs)      ]  CL 2
 *     [ConsumerCursor 64B × MAX_PROC  — one CL per process   ]  CL 3..N
 *     [T slots × CAPACITY            — event data, CL-sized  ]  rest
 *
 *   Key properties:
 *   • Producer-side back-pressure: spins when ring full (busy-wait only).
 *   • std::atomic<uint64_t> in MAP_SHARED mmap is safe on x86-64 and ARM64
 *     because: (a) hardware coherence (MESI/MOESI), (b) seq-cst fences map
 *     to LOCK-prefixed / DMB-SY instructions that are process-independent.
 *   • Monotonically increasing 64-bit sequences → no ABA, no modular wrap
 *     in practical lifetime (2^64 / 50M events/s = ~11,000 years).
 *   • Huge pages (Linux only): reduces TLB pressure for 4MB+ ring.
 *   • NUMA binding (Linux only): mbind() places ring pages on correct NUMA
 *     node to avoid remote NUMA accesses from feed-side bridge thread.
 *
 * ── Latency Budget (co-located, same NUMA) ──────────────────────────────────
 *
 *   NIC → ef_vi DMA      ~0.5 µs
 *   DMA → SpscRing       ~5 ns  (store + release)
 *   SpscRing → Parser    ~5 ns  (peek + consume)
 *   Parser → in-proc     ~10 ns (parse + claim + publish)
 *   in-proc → SHM bridge ~5 ns  (consumer advance + SHM claim + publish)
 *   SHM ring → subscriber~15 ns (cache line transfer, same socket)
 *   Subscriber dispatch  ~5 ns
 *   ──────────────────
 *   Total (cross-proc)   ~45 ns
 *   Total (same-proc)    ~35 ns
 * ============================================================================
 */
#pragma once
#include "ull_market_feed_handlers.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <cassert>

#ifdef __linux__
#  include <sys/mman.h>
#  include <numaif.h>      // mbind (if NUMA_AVAILABLE)
#  include <sys/syscall.h>
#endif

namespace ull {

// ============================================================================
// §1  ShmRegion — POSIX shared memory region (RAII)
//
//     Creator process:  ShmRegion::create("/ull_feed_ring_ES", 4*1024*1024)
//     Subscriber proc:  ShmRegion::attach("/ull_feed_ring_ES", 4*1024*1024)
// ============================================================================
class ShmRegion {
public:
    // Creator: shm_open(O_CREAT) + ftruncate + mmap + optional NUMA bind
    static ShmRegion create(const std::string& name, size_t size,
                              int numa_node = -1, bool huge_pages = false) {
        ShmRegion r;
        r.name_    = name;
        r.size_    = size;
        r.creator_ = true;

        r.fd_ = shm_open(name.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (r.fd_ < 0)
            throw std::runtime_error("shm_open(create) failed: " + std::string(strerror(errno)));

        if (ftruncate(r.fd_, static_cast<off_t>(size)) < 0) {
            ::close(r.fd_);
            shm_unlink(name.c_str());
            throw std::runtime_error("ftruncate failed: " + std::string(strerror(errno)));
        }

        int mmap_flags = MAP_SHARED;
#ifdef __linux__
        if (huge_pages) mmap_flags |= MAP_HUGETLB;
#endif
        r.addr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, mmap_flags, r.fd_, 0);
        if (r.addr_ == MAP_FAILED) {
            ::close(r.fd_);
            shm_unlink(name.c_str());
            throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
        }

#ifdef __linux__
        // NUMA binding: place pages on the requested NUMA node
        if (numa_node >= 0) {
            unsigned long nodemask = 1UL << unsigned(numa_node);
            mbind(r.addr_, size, MPOL_BIND,
                  &nodemask, sizeof(nodemask) * 8, MPOL_MF_MOVE);
        }
#else
        (void)numa_node;
#endif
        // Pre-fault all pages so first access doesn't stall on page fault
        mlock(r.addr_, size);
        std::memset(r.addr_, 0, size);

        return r;
    }

    // Attacher: shm_open(readonly) + mmap (no create, no truncate)
    static ShmRegion attach(const std::string& name, size_t size) {
        ShmRegion r;
        r.name_    = name;
        r.size_    = size;
        r.creator_ = false;

        r.fd_ = shm_open(name.c_str(), O_RDWR, 0666);
        if (r.fd_ < 0)
            throw std::runtime_error("shm_open(attach) failed for '" + name +
                                     "': " + std::string(strerror(errno)));

        r.addr_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, r.fd_, 0);
        if (r.addr_ == MAP_FAILED) {
            ::close(r.fd_);
            throw std::runtime_error("mmap(attach) failed: " + std::string(strerror(errno)));
        }
        return r;
    }

    ~ShmRegion() { cleanup(); }

    ShmRegion(ShmRegion&& o) noexcept
        : fd_(o.fd_), addr_(o.addr_), size_(o.size_),
          name_(std::move(o.name_)), creator_(o.creator_) {
        o.fd_ = -1; o.addr_ = nullptr; o.size_ = 0;
    }
    ShmRegion& operator=(ShmRegion&& o) noexcept {
        if (this != &o) { cleanup(); fd_ = o.fd_; addr_ = o.addr_;
            size_ = o.size_; name_ = std::move(o.name_); creator_ = o.creator_;
            o.fd_ = -1; o.addr_ = nullptr; o.size_ = 0; }
        return *this;
    }
    ShmRegion(const ShmRegion&) = delete;
    ShmRegion& operator=(const ShmRegion&) = delete;

    [[nodiscard]] void*       addr() const noexcept { return addr_; }
    [[nodiscard]] size_t      size() const noexcept { return size_; }
    [[nodiscard]] bool        valid() const noexcept { return addr_ != nullptr; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    ShmRegion() = default;
    void cleanup() noexcept {
        if (addr_) { munmap(addr_, size_); addr_ = nullptr; }
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
        if (creator_ && !name_.empty()) shm_unlink(name_.c_str());
    }
    int         fd_{-1};
    void*       addr_{nullptr};
    size_t      size_{0};
    std::string name_;
    bool        creator_{false};
};

// ============================================================================
// §2  ShmSpmcRing<T, CAPACITY, MAX_PROC_CONSUMERS>
//
//     Lock-free single-producer, multi-consumer ring placed in shared memory.
//     Each consumer is a SEPARATE PROCESS (or thread); each gets its own
//     cache-line-isolated cursor to prevent false sharing.
//
//     Memory layout (fixed, flat, no pointers — safe across processes):
//
//       Offset 0                : RingMeta      (1 cache line, 64 B)
//       Offset 64               : ProducerState (1 cache line, 64 B)
//       Offset 128              : ConsumerCount (1 cache line, 64 B)
//       Offset 192              : ConsumerCursors[MAX_PROC_CONSUMERS × 64 B]
//       Offset 192 + MAX×64     : T slots[CAPACITY]  (CAPACITY × sizeof(T))
//
//     Usage:
//       // Publisher process
//       auto region = ShmRegion::create("/ull_ring_es", ShmSpmcRing::required_size());
//       auto* ring  = ShmSpmcRing::create_at(region.addr(), "ES-feed");
//
//       // Subscriber process
//       auto region = ShmRegion::attach("/ull_ring_es", ShmSpmcRing::required_size());
//       auto* ring  = ShmSpmcRing::attach_at(region.addr());
//       int   cid   = ring->register_consumer();
// ============================================================================
template<typename T, uint32_t CAPACITY, uint32_t MAX_PROC_CONSUMERS = 8>
class ShmSpmcRing {
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be power-of-2");
    static_assert(MAX_PROC_CONSUMERS <= 64,          "MAX_PROC_CONSUMERS too large");
    static_assert(std::is_trivially_copyable_v<T>,   "T must be trivially copyable");

    static constexpr uint64_t MAGIC = 0xFEEDCAFEDEAD1234ULL;
    static constexpr uint64_t MASK  = CAPACITY - 1;
    // INITIAL_SEQ: starting value so first published event has seq=0
    static constexpr uint64_t INITIAL_SEQ = uint64_t(-1);

    // ── Cache-line-padded structs (no false sharing between producer/consumers) ──
    struct alignas(CACHE_LINE) RingMeta {
        uint64_t magic;               //  8
        uint32_t capacity;            //  4
        uint32_t max_proc_consumers;  //  4
        char     name[48];            // 48  human-readable label
        // total: 64 bytes
    };

    struct alignas(CACHE_LINE) ProducerState {
        std::atomic<uint64_t> claimed{INITIAL_SEQ};   // last claimed seq
        std::atomic<uint64_t> published{INITIAL_SEQ}; // last published seq
        char _pad[CACHE_LINE - 2 * sizeof(std::atomic<uint64_t>)];
    };

    struct alignas(CACHE_LINE) ConsumerCountSlot {
        std::atomic<uint32_t> count{0};
        char _pad[CACHE_LINE - sizeof(std::atomic<uint32_t>)];
    };

    struct alignas(CACHE_LINE) ConsumerCursorSlot {
        std::atomic<uint64_t> seq{INITIAL_SEQ}; // last consumed seq
        char _pad[CACHE_LINE - sizeof(std::atomic<uint64_t>)];
    };

    // ── Layout (no T[] member — slots live just after this struct in SHM) ──
    RingMeta            meta_;
    ProducerState       producer_;
    ConsumerCountSlot   consumer_count_;
    ConsumerCursorSlot  consumer_cursors_[MAX_PROC_CONSUMERS];

public:
    // Byte size needed in the SHM region for this ring + its event slots
    static constexpr size_t required_size() noexcept {
        return sizeof(ShmSpmcRing) + CAPACITY * sizeof(T);
    }

    // ── Factory: called by publisher to initialise the ring in SHM ──────────
    static ShmSpmcRing* create_at(void* mem, std::string_view ring_name) {
        // Placement-new initialises all atomics to their default (INITIAL_SEQ/0)
        auto* r = new(mem) ShmSpmcRing();
        r->meta_.magic              = MAGIC;
        r->meta_.capacity           = CAPACITY;
        r->meta_.max_proc_consumers = MAX_PROC_CONSUMERS;
        std::strncpy(r->meta_.name, ring_name.data(), sizeof(r->meta_.name) - 1);
        return r;
    }

    // ── Factory: called by subscriber — attaches without re-initialising ────
    static ShmSpmcRing* attach_at(void* mem) noexcept {
        auto* r = static_cast<ShmSpmcRing*>(mem);
        assert(r->meta_.magic == MAGIC && "SHM ring magic check failed");
        return r;
    }

    // ── Consumer registration (each subscribing process calls once) ─────────
    // Returns consumer_id in [0, MAX_PROC_CONSUMERS).
    // Thread-safe across processes (atomic fetch_add on seq in SHM).
    [[nodiscard]] int register_consumer() noexcept {
        uint32_t cid = consumer_count_.count.fetch_add(1, std::memory_order_acq_rel);
        if (cid >= MAX_PROC_CONSUMERS) return -1;  // ring full
        // New consumer starts at current published position (no replay of old events)
        consumer_cursors_[cid].seq.store(
            producer_.published.load(std::memory_order_acquire),
            std::memory_order_release);
        return static_cast<int>(cid);
    }

    void deregister_consumer(int cid) noexcept {
        // Mark cursor at max so it doesn't block the producer
        consumer_cursors_[cid].seq.store(~uint64_t(0), std::memory_order_release);
    }

    // ── Producer API (single-publisher bridge thread) ────────────────────────
    // Claim the next slot; busy-waits if the ring would overwrite the slowest
    // active consumer (back-pressure without kernel involvement).
    [[nodiscard]] __attribute__((hot))
    uint64_t claim() noexcept {
        uint64_t seq = producer_.claimed.load(std::memory_order_relaxed) + 1;
        // Spin until there is space (slowest consumer is >= CAPACITY behind us)
        while (seq - slowest_consumer() >= uint64_t(CAPACITY)) {
            spin_pause();
        }
        producer_.claimed.store(seq, std::memory_order_relaxed);
        return seq;
    }

    // Reference to the slot for this sequence number (write before publish)
    [[nodiscard]] __attribute__((hot))
    T& slot(uint64_t seq) noexcept { return slots()[seq & MASK]; }

    // Publish: mark slot as readable.  In-order: waits if a prior claim
    // hasn't been published yet (important for single-producer correctness).
    __attribute__((hot))
    void publish(uint64_t seq) noexcept {
        // Ensure in-order visibility: prior slot must be published first
        uint64_t expected = seq - 1;
        while (producer_.published.load(std::memory_order_relaxed) != expected) {
            spin_pause();
        }
        producer_.published.store(seq, std::memory_order_release);
    }

    // ── Consumer API (each subscribing process uses its own cid) ────────────
    [[nodiscard]] __attribute__((hot))
    bool available_for(int cid) const noexcept {
        uint64_t my_seq = consumer_cursors_[cid].seq.load(std::memory_order_relaxed);
        uint64_t pub    = producer_.published.load(std::memory_order_acquire);
        return my_seq < pub;
    }

    // Next sequence number to read for this consumer
    [[nodiscard]] __attribute__((hot))
    uint64_t next_seq(int cid) const noexcept {
        return consumer_cursors_[cid].seq.load(std::memory_order_relaxed) + 1;
    }

    // Advance consumer cursor (call after processing the event at next_seq-1)
    __attribute__((hot))
    void advance(int cid) noexcept {
        consumer_cursors_[cid].seq.fetch_add(1, std::memory_order_release);
    }

    // Diagnostic accessors
    [[nodiscard]] const char* ring_name() const noexcept { return meta_.name; }
    [[nodiscard]] uint32_t    num_consumers() const noexcept {
        return consumer_count_.count.load(std::memory_order_relaxed);
    }

private:
    // Compute the minimum consumer cursor (= slowest active consumer)
    // This is the back-pressure check used in claim().
    [[nodiscard]] uint64_t slowest_consumer() const noexcept {
        uint32_t n = consumer_count_.count.load(std::memory_order_relaxed);
        if (n == 0) return producer_.published.load(std::memory_order_relaxed);
        uint64_t min_seq = ~uint64_t(0);
        for (uint32_t i = 0; i < n; ++i) {
            uint64_t s = consumer_cursors_[i].seq.load(std::memory_order_relaxed);
            if (s < min_seq) min_seq = s;
        }
        return min_seq;
    }

    // Slots live immediately after this struct in the SHM region
    [[nodiscard]] T* slots() noexcept {
        return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(this) + sizeof(ShmSpmcRing));
    }
    [[nodiscard]] const T* slots() const noexcept {
        return reinterpret_cast<const T*>(
            reinterpret_cast<const uint8_t*>(this) + sizeof(ShmSpmcRing));
    }
};

// Convenience alias: MarketEvent SHM ring with up to 8 subscriber processes
using MarketEventShmRing = ShmSpmcRing<MarketEvent, 65536, 8>;

// ============================================================================
// §3  ShmPublisherBridge
//
//     Runs inside the PUBLISHER PROCESS (Feed Handler Container).
//     Reads from the in-process SpmcDisruptor (as one of its consumers)
//     and re-publishes every event to the SHM ring so that separate
//     subscriber processes can receive it.
//
//     ┌ in-proc SpmcDisruptor ─► [ShmPublisherBridge thread] ─► SHM ring ─►
//
//     Thread is pinned to `bridge_cpu` (NUMA 0 side, same as parsers).
//     Priority: just below the parse threads (e.g. rt_prio = 80).
// ============================================================================
class ShmPublisherBridge {
public:
    using InProcRing = SpmcDisruptor<MarketEvent, 65536, 32>;

    ShmPublisherBridge(InProcRing& src_ring,
                        MarketEventShmRing& shm_ring,
                        int bridge_cpu = 7)
        : src_ring_(src_ring), shm_ring_(shm_ring), bridge_cpu_(bridge_cpu) {}

    void start() {
        // Register as a consumer on the in-process ring
        src_consumer_id_ = src_ring_.register_consumer();
        running_.store(true, std::memory_order_release);
        thread_ = std::thread(&ShmPublisherBridge::bridge_loop, this);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (thread_.joinable()) thread_.join();
        src_ring_.unregister_consumer(src_consumer_id_);
    }

    // Statistics
    struct Stats {
        uint64_t events_forwarded{0};
        uint64_t stalls{0};   // times the SHM ring stalled (backpressure)
    };
    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }

private:
    __attribute__((hot)) void bridge_loop() noexcept {
        pin_thread_to_cpu(bridge_cpu_);
        set_realtime(80);

        while (running_.load(std::memory_order_relaxed)) {
            // Drain as many events as are available from the in-process ring
            while (src_ring_.available_for(src_consumer_id_)) {
                uint64_t src_seq       = src_ring_.next_seq(src_consumer_id_);
                const MarketEvent& ev  = src_ring_.slot(src_seq);

                // Claim a slot in the SHM ring (busy-waits if SHM ring full)
                uint64_t shm_seq = shm_ring_.claim();
                shm_ring_.slot(shm_seq) = ev;  // zero-copy within process
                shm_ring_.publish(shm_seq);

                src_ring_.advance(src_consumer_id_);
                ++stats_.events_forwarded;
            }
            spin_pause();
        }
    }

    InProcRing&          src_ring_;
    MarketEventShmRing&  shm_ring_;
    int                  bridge_cpu_;
    int                  src_consumer_id_{-1};
    std::thread          thread_;
    std::atomic<bool>    running_{false};
    Stats                stats_;
};

// ============================================================================
// §4  ShmSubscriberBridge
//
//     Runs inside a SUBSCRIBER PROCESS (Strategy Container).
//     Reads from the SHM ring and either:
//       (a) forwards into a local in-process SpmcDisruptor for multi-strategy
//           dispatch (use when this container hosts several strategies), OR
//       (b) calls a direct callback on the subscriber thread (use for a
//           single-strategy container where extra hop latency matters).
//
//     ┌ SHM ring ─► [ShmSubscriberBridge thread] ─► local SpmcDisruptor ─► N strategies
//
//     Thread pinned to `bridge_cpu` (NUMA 1 side recommended).
//     Priority: same as strategy dispatcher threads (rt_prio = 70).
// ============================================================================
class ShmSubscriberBridge {
public:
    using LocalRing  = SpmcDisruptor<MarketEvent, 65536, 32>;
    using DirectCb   = std::function<void(const MarketEvent&)>;

    // Mode A: forward to a local ring (multi-strategy dispatch)
    ShmSubscriberBridge(MarketEventShmRing& shm_ring,
                         LocalRing& local_ring,
                         int bridge_cpu = 16)
        : shm_ring_(shm_ring), local_ring_(&local_ring),
          bridge_cpu_(bridge_cpu), mode_(Mode::LocalRing) {}

    // Mode B: direct callback (single-strategy, lowest latency)
    ShmSubscriberBridge(MarketEventShmRing& shm_ring,
                         DirectCb cb,
                         int bridge_cpu = 16)
        : shm_ring_(shm_ring), local_ring_(nullptr),
          direct_cb_(std::move(cb)), bridge_cpu_(bridge_cpu),
          mode_(Mode::DirectCallback) {}

    void start() {
        shm_consumer_id_ = shm_ring_.register_consumer();
        if (shm_consumer_id_ < 0)
            throw std::runtime_error("ShmSubscriberBridge: SHM ring has no free consumer slots");
        running_.store(true, std::memory_order_release);
        thread_ = std::thread(&ShmSubscriberBridge::recv_loop, this);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (thread_.joinable()) thread_.join();
        shm_ring_.deregister_consumer(shm_consumer_id_);
    }

    struct Stats {
        uint64_t events_received{0};
        uint64_t events_forwarded{0};
    };
    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }

private:
    enum class Mode { LocalRing, DirectCallback };

    __attribute__((hot)) void recv_loop() noexcept {
        pin_thread_to_cpu(bridge_cpu_);
        set_realtime(70);

        while (running_.load(std::memory_order_relaxed)) {
            while (shm_ring_.available_for(shm_consumer_id_)) {
                uint64_t seq           = shm_ring_.next_seq(shm_consumer_id_);
                const MarketEvent& ev  = shm_ring_.slot(seq);
                ++stats_.events_received;

                if (mode_ == Mode::LocalRing) {
                    // Forward into the local in-process ring for fan-out
                    uint64_t lseq = local_ring_->claim();
                    local_ring_->slot(lseq) = ev;
                    local_ring_->publish(lseq);
                    ++stats_.events_forwarded;
                } else {
                    // Direct callback — minimal latency
                    if (direct_cb_) direct_cb_(ev);
                }

                shm_ring_.advance(shm_consumer_id_);
            }
            spin_pause();
        }
    }

    MarketEventShmRing&  shm_ring_;
    LocalRing*           local_ring_;
    DirectCb             direct_cb_;
    int                  bridge_cpu_;
    int                  shm_consumer_id_{-1};
    std::thread          thread_;
    std::atomic<bool>    running_{false};
    Stats                stats_;
    Mode                 mode_;
};

// ============================================================================
// §5a  StrategySubscription & IStrategy
//
//      Defined here (in the IPC header) so both the monolithic demo and the
//      multi-container system share the same canonical definitions.
//      ull_feed_system_demo.cpp must include this header (not the lower-level
//      ull_market_feed_handlers.hpp) to avoid duplicate definitions.
// ============================================================================
struct StrategySubscription {
    static constexpr size_t MAX_INSTR = 512;
    std::bitset<MAX_INSTR> instruments;
    bool subscribe_l1{true};
    bool subscribe_l2{true};
    bool subscribe_trades{true};
    bool subscribe_status{false};

    void subscribe_instrument(InstrumentID id) {
        if (id < MAX_INSTR) instruments.set(id);
    }
    void subscribe_all_instruments(const BookRegistry& books) {
        for (uint32_t i = 0; i < books.count(); ++i) instruments.set(i);
    }
    [[nodiscard]] bool matches(const MarketEvent& ev) const noexcept {
        if (ev.instrument_id >= MAX_INSTR) return false;
        if (!instruments.test(ev.instrument_id)) return false;
        switch (ev.type) {
            case EventType::BookL1:  return subscribe_l1;
            case EventType::BookL2:  return subscribe_l2;
            case EventType::Trade:   return subscribe_trades;
            case EventType::Status:  return subscribe_status;
            default:                 return true;
        }
    }
};

class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual void on_event(const MarketEvent& ev, const SoABook& book) noexcept = 0;
    virtual const char* name() const noexcept = 0;
    virtual StrategySubscription subscription() const = 0;
    virtual void print_stats() const {}
};

// ============================================================================
// §5  MultiContainerSystem
//
//     High-level builder that configures the system in either mode:
//
//     Mode 1 – Monolithic (single process):
//       system.add_feed_handler(...)
//       system.add_strategy(...)
//       system.start_monolithic()
//       // all communication via in-process SpmcDisruptor
//
//     Mode 2 – Publisher side:
//       system.add_feed_handler(...)
//       auto shm = system.start_publisher("/ull_ring_ES", numa_node=0)
//       // bridge thread forwards in-proc events to SHM ring
//
//     Mode 2 – Subscriber side:
//       system.attach_shm("/ull_ring_ES")
//       system.add_strategy(...)
//       system.start_subscriber(bridge_cpu=16)
//       // subscriber thread reads SHM, fans out to local strategies
// ============================================================================
struct MultiContainerConfig {
    std::string shm_name;       // e.g. "/ull_feed_ring_ES"
    int         numa_node{0};   // NUMA node for SHM allocation (publisher only)
    bool        huge_pages{false};
    int         publisher_bridge_cpu{7};   // NUMA 0 core for publisher bridge
    int         subscriber_bridge_cpu{16}; // NUMA 1 core for subscriber bridge
};

class MultiContainerSystem {
public:
    using InProcRing = SpmcDisruptor<MarketEvent, 65536, 32>;

    explicit MultiContainerSystem(int base_cpu = 0)
        : container_(base_cpu) {}

    // ── Feed handler registration (same API as FeedHandlerContainer) ─────────
    template<typename Handler>
    Handler* add_feed_handler(std::unique_ptr<Handler> h, FeedConfig cfg) {
        return container_.add_handler(std::move(h), std::move(cfg));
    }

    // ── MODE 1: Monolithic — add strategies for in-process dispatch ──────────
    void add_strategy(IStrategy* strat) { strategies_.push_back(strat); }

    void start_monolithic(int dispatcher_cpu = 16) {
        ensure_dispatcher(dispatcher_cpu);
        dispatcher_->start();
        container_.start_all();
        mode_ = Mode::Monolithic;
    }

    // ── MODE 2a: Publisher — create SHM ring and start bridge ───────────────
    ShmRegion start_publisher(const MultiContainerConfig& cfg) {
        auto region = ShmRegion::create(cfg.shm_name,
                                         MarketEventShmRing::required_size(),
                                         cfg.numa_node, cfg.huge_pages);
        shm_ring_ = MarketEventShmRing::create_at(region.addr(), cfg.shm_name);

        pub_bridge_ = std::make_unique<ShmPublisherBridge>(
            container_.ring(), *shm_ring_, cfg.publisher_bridge_cpu);

        pub_bridge_->start();
        container_.start_all();
        mode_ = Mode::Publisher;

        std::cout << "[MultiContainerSystem] Publisher started.\n"
                  << "  SHM ring: " << cfg.shm_name
                  << " (" << MarketEventShmRing::required_size() / (1024*1024) << " MB"
                  << ", NUMA " << cfg.numa_node << ")\n"
                  << "  Bridge CPU: " << cfg.publisher_bridge_cpu << "\n";

        return region;  // caller keeps region alive to maintain SHM lifetime
    }

    // ── MODE 2b: Subscriber — attach to SHM ring and start bridge ───────────
    ShmRegion start_subscriber(const MultiContainerConfig& cfg,
                                 int dispatcher_cpu = 17) {
        auto region = ShmRegion::attach(cfg.shm_name,
                                         MarketEventShmRing::required_size());
        shm_ring_ = MarketEventShmRing::attach_at(region.addr());

        // Create local in-process ring for fan-out to multiple strategies
        local_ring_ = std::make_unique<InProcRing>();
        ensure_dispatcher(dispatcher_cpu, local_ring_.get());

        sub_bridge_ = std::make_unique<ShmSubscriberBridge>(
            *shm_ring_, *local_ring_, cfg.subscriber_bridge_cpu);

        sub_bridge_->start();
        dispatcher_->start();
        mode_ = Mode::Subscriber;

        std::cout << "[MultiContainerSystem] Subscriber started.\n"
                  << "  Attached to: " << cfg.shm_name << "\n"
                  << "  SHM consumer ID: " << shm_ring_->num_consumers() - 1 << "\n"
                  << "  Strategies: " << strategies_.size() << "\n";

        return region;
    }

    void stop() {
        if (pub_bridge_) pub_bridge_->stop();
        if (sub_bridge_) sub_bridge_->stop();
        if (dispatcher_)  dispatcher_->stop();
        container_.stop_all();
    }

    void print_stats() const {
        container_.print_all_stats();
        if (pub_bridge_) {
            auto& s = pub_bridge_->stats();
            std::cout << "[ShmPublisherBridge] forwarded=" << s.events_forwarded
                      << " stalls=" << s.stalls << "\n";
        }
        if (sub_bridge_) {
            auto& s = sub_bridge_->stats();
            std::cout << "[ShmSubscriberBridge] received=" << s.events_received
                      << " forwarded=" << s.events_forwarded << "\n";
        }
    }

    [[nodiscard]] FeedHandlerContainer& container() noexcept { return container_; }

private:
    // Internal dispatcher wrapping the appropriate ring
    class Dispatcher {
    public:
        using Ring = InProcRing;

        explicit Dispatcher(Ring& ring, BookRegistry& books, int cpu)
            : ring_(ring), books_(books), cpu_(cpu) {}

        void add_strategy(IStrategy* strat) {
            strategies_.push_back(strat);
        }

        void start() {
            for (auto* s : strategies_) {
                int cid = ring_.register_consumer();
                consumer_ids_.push_back(cid);
                subs_.push_back(s->subscription());
            }
            running_.store(true, std::memory_order_release);
            thread_ = std::thread(&Dispatcher::loop, this);
        }

        void stop() {
            running_.store(false, std::memory_order_release);
            if (thread_.joinable()) thread_.join();
        }

    private:
        void loop() noexcept {
            pin_thread_to_cpu(cpu_);
            set_realtime(70);
            while (running_.load(std::memory_order_relaxed)) {
                for (size_t i = 0; i < strategies_.size(); ++i) {
                    int cid = consumer_ids_[i];
                    while (ring_.available_for(cid)) {
                        uint64_t seq          = ring_.next_seq(cid);
                        const MarketEvent& ev = ring_.slot(seq);
                        if (subs_[i].matches(ev))
                            strategies_[i]->on_event(ev, books_.book(ev.instrument_id));
                        ring_.advance(cid);
                    }
                    spin_pause();
                }
            }
        }

        Ring&                           ring_;
        BookRegistry&                   books_;
        int                             cpu_;
        std::vector<IStrategy*>         strategies_;
        std::vector<int>                consumer_ids_;
        std::vector<StrategySubscription> subs_;
        std::thread                     thread_;
        std::atomic<bool>               running_{false};
    };

    void ensure_dispatcher(int cpu, InProcRing* ring_override = nullptr) {
        auto& ring = ring_override ? *ring_override : container_.ring();
        dispatcher_ = std::make_unique<Dispatcher>(ring, container_.books(), cpu);
        for (auto* s : strategies_) dispatcher_->add_strategy(s);
    }

    enum class Mode { Unset, Monolithic, Publisher, Subscriber };

    FeedHandlerContainer               container_;
    std::unique_ptr<InProcRing>        local_ring_;   // subscriber mode only
    MarketEventShmRing*                shm_ring_{nullptr};
    std::unique_ptr<ShmPublisherBridge>   pub_bridge_;
    std::unique_ptr<ShmSubscriberBridge>  sub_bridge_;
    std::unique_ptr<Dispatcher>        dispatcher_;
    std::vector<IStrategy*>            strategies_;
    Mode                               mode_{Mode::Unset};
};

// ============================================================================
// §6  Architecture Reference Functions
//     Print clear topology diagrams for both deployment modes.
// ============================================================================
inline void print_monolithic_topology(int n_handlers, int n_strategies) {
    std::cout << "\n";
    std::cout << "┌─────────── Mode 1: Monolithic (single process) ────────────────────┐\n";
    std::cout << "│                                                                     │\n";
    std::cout << "│  NUMA 0 (Feed Handlers)          NUMA 1 (Strategies)               │\n";
    std::cout << "│  CPU 0-" << (n_handlers*2-1) << ": " << n_handlers
              << " × FeedHandlers         CPU 16+: StrategyDispatcher         │\n";
    std::cout << "│  [recv][parse] per handler                                          │\n";
    std::cout << "│                                                                     │\n";
    std::cout << "│  HKEX Handler ──────────────────────────────────────────────────►  │\n";
    std::cout << "│  ASX  Handler ──► SpscRing ──► Parser ──► in-proc   ──► Strategy 1 │\n";
    std::cout << "│  SGX  Handler ──────────────────────────   SpmcDisruptor ──► Strat2 │\n";
    std::cout << "│                                             (lock-free)  ──► Strat3 │\n";
    std::cout << "│                                                                     │\n";
    std::cout << "│  Latency: ~15-35 ns tick-to-strategy (cache-hot, no IPC)           │\n";
    std::cout << "│  Max strategies: 32 (SpmcDisruptor MAX_CONSUMERS)                  │\n";
    std::cout << "│  Max handlers:   unlimited (each →SpscRing→Parser→ same ring)      │\n";
    std::cout << "└─────────────────────────────────────────────────────────────────────┘\n";
}

inline void print_multicontainer_topology() {
    std::cout << "\n";
    std::cout << "┌─────── Mode 2: Multi-Container (separate processes, same server) ───┐\n";
    std::cout << "│                                                                      │\n";
    std::cout << "│  ┌── Process A: Feed Handler Container ──────────────────────────┐  │\n";
    std::cout << "│  │  HKEX/ASX/SGX Handlers ──► in-proc SpmcDisruptor             │  │\n";
    std::cout << "│  │                                    └─► ShmPublisherBridge     │  │\n";
    std::cout << "│  │                                              │                │  │\n";
    std::cout << "│  └──────────────────────────────────────────────│────────────────┘  │\n";
    std::cout << "│                                                  │                  │\n";
    std::cout << "│                          ┌─── SHM SPMC Ring ────┘                  │\n";
    std::cout << "│                          │   (mmap MAP_SHARED)                     │\n";
    std::cout << "│                          │   65536 slots × 64 B = 4 MB             │\n";
    std::cout << "│                          │   Up to 8 consumer processes            │\n";
    std::cout << "│                          │   Huge pages (optional, Linux)          │\n";
    std::cout << "│                          │   NUMA-bound (optional, Linux)          │\n";
    std::cout << "│                          │                                         │\n";
    std::cout << "│           ┌──────────────┤──────────────────────────┐             │\n";
    std::cout << "│           ▼ consumer 0   ▼ consumer 1               │             │\n";
    std::cout << "│  ┌─ Process B ──────┐  ┌─ Process C ──────┐        │             │\n";
    std::cout << "│  │ShmSubscriberBrid │  │ShmSubscriberBrid │        │             │\n";
    std::cout << "│  │  ──► local ring  │  │  ──► local ring  │        ...           │\n";
    std::cout << "│  │  ──► Strategy 1  │  │  ──► Strategy 3  │                     │\n";
    std::cout << "│  │  ──► Strategy 2  │  │  ──► Strategy 4  │                     │\n";
    std::cout << "│  └──────────────────┘  └──────────────────┘                     │\n";
    std::cout << "│                                                                      │\n";
    std::cout << "│  Latency: ~40-80 ns tick-to-strategy (same NUMA, MESI snooping)    │\n";
    std::cout << "│  Isolation: each container can be independently killed/upgraded     │\n";
    std::cout << "│  Sharing:   N feed handlers feed M strategy containers (1:M fan-out)│\n";
    std::cout << "└──────────────────────────────────────────────────────────────────────┘\n";
}

// ============================================================================
// §7  Demo: same-process simulation of multi-container using threads
//     (Validates SHM ring logic without requiring two separate processes.
//      Real multi-container: compile two separate binaries, share ring name.)
// ============================================================================
inline void demo_multicontainer_simulation() {
    std::cout << "\n=== Multi-Container IPC Simulation (in-process, validating SHM ring) ===\n\n";

    // 1. Allocate the SHM ring in regular heap memory (same layout as SHM)
    //    In production: use ShmRegion::create() + ShmSpmcRing::create_at()
    constexpr size_t ring_bytes = MarketEventShmRing::required_size();
    auto raw_mem = std::make_unique<uint8_t[]>(ring_bytes + CACHE_LINE);
    // Align to cache line
    uint8_t* aligned = reinterpret_cast<uint8_t*>(
        (reinterpret_cast<uintptr_t>(raw_mem.get()) + CACHE_LINE - 1) & ~(CACHE_LINE - 1));
    auto* shm_ring = MarketEventShmRing::create_at(aligned, "sim-feed-ring");

    // 2. Register two "subscriber process" consumers
    int cid0 = shm_ring->register_consumer();  // simulates Process B
    int cid1 = shm_ring->register_consumer();  // simulates Process C

    std::atomic<uint64_t> proc_b_count{0}, proc_c_count{0};
    std::atomic<bool> subs_running{true};

    // 3. Subscriber threads (simulate separate processes reading SHM ring)
    std::thread sub_b([&]() {
        pin_thread_to_cpu(2);
        while (subs_running.load(std::memory_order_relaxed) ||
               shm_ring->available_for(cid0)) {
            while (shm_ring->available_for(cid0)) {
                uint64_t seq         = shm_ring->next_seq(cid0);
                const MarketEvent& ev = shm_ring->slot(seq);
                (void)ev;
                shm_ring->advance(cid0);
                ++proc_b_count;
            }
            spin_pause();
        }
    });

    std::thread sub_c([&]() {
        pin_thread_to_cpu(3);
        while (subs_running.load(std::memory_order_relaxed) ||
               shm_ring->available_for(cid1)) {
            while (shm_ring->available_for(cid1)) {
                uint64_t seq          = shm_ring->next_seq(cid1);
                const MarketEvent& ev = shm_ring->slot(seq);
                (void)ev;
                shm_ring->advance(cid1);
                ++proc_c_count;
            }
            spin_pause();
        }
    });

    // 4. Publisher: inject N events through the SHM ring (simulates bridge thread)
    const int N = 500'000;
    std::vector<double> lats;
    lats.reserve(N);

    auto t_wall0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        auto t0 = std::chrono::steady_clock::now();

        uint64_t seq = shm_ring->claim();
        MarketEvent& ev = shm_ring->slot(seq);
        ev.timestamp_ns = rdtsc();
        ev.instrument_id = i % 11;
        ev.type          = EventType::BookL1;
        ev.price_fp      = to_fp(100.0 + i * 0.0001);
        ev.qty           = 100;
        shm_ring->publish(seq);

        auto t1 = std::chrono::steady_clock::now();
        lats.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    auto t_wall1 = std::chrono::steady_clock::now();

    // Wait for both consumers to drain
    while (proc_b_count.load() < uint64_t(N) || proc_c_count.load() < uint64_t(N)) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    subs_running.store(false);
    sub_b.join();
    sub_c.join();

    // 5. Print results
    std::sort(lats.begin(), lats.end());
    double sum = 0; for (auto v : lats) sum += v;
    double wall_sec = std::chrono::duration<double>(t_wall1 - t_wall0).count();

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "SHM SPMC Ring — " << N << " events → 2 consumers\n";
    std::cout << "  Publish latency (claim+fill+publish):\n";
    std::cout << "    min   : " << lats.front()                       << " ns\n";
    std::cout << "    mean  : " << sum / N                            << " ns\n";
    std::cout << "    p50   : " << lats[N/2]                          << " ns\n";
    std::cout << "    p99   : " << lats[size_t(N*0.99)]               << " ns\n";
    std::cout << "    p99.9 : " << lats[size_t(N*0.999)]              << " ns\n";
    std::cout << "    max   : " << lats.back()                        << " ns\n";
    std::cout << "  Throughput: "
              << std::setprecision(2) << N / wall_sec / 1e6 << " M events/sec\n";
    std::cout << "  Process B consumed: " << proc_b_count.load() << " / " << N << "\n";
    std::cout << "  Process C consumed: " << proc_c_count.load() << " / " << N << "\n";
    std::cout << "  SHM ring consumers registered: "
              << shm_ring->num_consumers() << "\n";

    // 6. Print how to deploy for real two-process scenario
    std::cout << "\n── Real Two-Process Deployment ─────────────────────────────────────\n";
    std::cout << "  // === PROCESS A: Feed Handler Container ===\n";
    std::cout << "  MultiContainerSystem system_a;\n";
    std::cout << "  system_a.add_feed_handler(make_unique<HkexOmdFeedHandler>(), hkex_cfg);\n";
    std::cout << "  auto shm = system_a.start_publisher({.shm_name=\"/ull_ring_ES\",\n";
    std::cout << "                                        .numa_node=0, .huge_pages=true});\n";
    std::cout << "  // shm region kept alive in this process\n\n";
    std::cout << "  // === PROCESS B: Strategy Container A ===\n";
    std::cout << "  MultiContainerSystem system_b;\n";
    std::cout << "  system_b.add_strategy(make_unique<StatArbStrategy>(...).get());\n";
    std::cout << "  system_b.add_strategy(make_unique<MidPriceLogger>(...).get());\n";
    std::cout << "  auto shm_b = system_b.start_subscriber({.shm_name=\"/ull_ring_ES\"});\n\n";
    std::cout << "  // === PROCESS C: Strategy Container B (independent, same ring) ===\n";
    std::cout << "  MultiContainerSystem system_c;\n";
    std::cout << "  system_c.add_strategy(make_unique<MarketMakerStrategy>(...).get());\n";
    std::cout << "  auto shm_c = system_c.start_subscriber({.shm_name=\"/ull_ring_ES\"});\n";
    std::cout << "────────────────────────────────────────────────────────────────────\n";
}

}  // namespace ull

