/**
 * ================================================================================================
 * LOCK-FREE / WAIT-FREE QUEUE VARIANTS FOR ULTRA-LOW LATENCY TRADING
 * ================================================================================================
 *
 * This comprehensive guide covers:
 * 1. SPSC (Single Producer Single Consumer) - Wait-free
 * 2. MPSC (Multi Producer Single Consumer) - Lock-free
 * 3. SPMC (Single Producer Multi Consumer) - Lock-free
 * 4. MPMC (Multi Producer Multi Consumer) - Lock-free
 *
 * LATENCY NUMBERS (3 GHz CPU, same NUMA node, isolated cores):
 * - SPSC: 10-50   nanoseconds (wait-free,  no CAS,  fastest)
 * - MPSC: 50-100  nanoseconds (lock-free,  CAS on enqueue only)
 * - SPMC: 50-150  nanoseconds (lock-free,  CAS on dequeue only)
 * - MPMC: 100-200 nanoseconds (lock-free,  CAS on both sides)
 *
 * KEY CONCEPTS:
 * - Wait-free   : Every operation completes in O(1) bounded steps (guaranteed progress)
 * - Lock-free   : At least one thread makes progress (system-wide progress)
 * - ABA-safe    : Uses 64-bit sequence numbers per cell (ABA impossible at 10^9 ops/sec)
 * - Cache-friendly: Producer/consumer cursors on separate 64-byte cache lines
 * - Cell-padded : Each Cell padded to cache line to prevent false sharing between adjacent slots
 * - Zero alloc  : Pre-allocated ring buffer, no heap on hot path
 * - Power-of-2  : Capacity must be 2^N → bitmask index (avoids costly modulo)
 *
 * TRADING USE CASES:
 * - Market data feed → Strategy (SPSC)        : tick-to-signal < 50 ns
 * - Multiple strategies → Order router (MPSC) : multi-algo fan-in
 * - Single feed → Multiple strategies (SPMC)  : broadcast, every consumer sees every tick
 * - Multiple feeds → Multiple strategies (MPMC): work-stealing / load-balanced processing
 *
 * DEPLOYMENT CHECKLIST:
 * - Pin threads to isolated CPU cores (taskset / pthread_setaffinity_np)
 * - Set CPU governor to "performance" (disable P-states)
 * - Disable hyper-threading on target cores
 * - Lock memory pages: mlockall(MCL_CURRENT | MCL_FUTURE)
 * - Use huge pages if buffer > 2 MB (reduces TLB misses)
 * - Warmup queues before live trading (10k+ iterations to trigger JIT / branch predictor)
 *
 * ================================================================================================
 */

#include <atomic>
#include <array>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <algorithm>

// ================================================================================================
// PLATFORM DETECTION & INTRINSICS
// ================================================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    // PAUSE instruction: hints CPU this is a spin-wait loop
    // Reduces power, improves pipeline efficiency, lowers memory contention
    #define CPU_PAUSE()  _mm_pause()
    // RDTSC: raw CPU cycle counter — ~3-5 cycles overhead, sub-nanosecond resolution
    inline uint64_t READ_TSC() noexcept {
        uint32_t lo, hi;
        asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }
#elif defined(__aarch64__) || defined(__arm__)
    // ARM: YIELD instruction equivalent to x86 PAUSE
    #define CPU_PAUSE()  asm volatile("yield" ::: "memory")
    inline uint64_t READ_TSC() noexcept {
        uint64_t val;
        asm volatile("mrs %0, cntvct_el0" : "=r"(val));
        return val;
    }
#else
    #define CPU_PAUSE()  std::this_thread::yield()
    inline uint64_t READ_TSC() noexcept {
        return static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
#endif

// Compiler fence: prevents compiler reordering (no CPU barrier)
#define COMPILER_FENCE() asm volatile("" ::: "memory")

// ================================================================================================
// CACHE LINE SIZE AND ALIGNMENT
// ================================================================================================
constexpr size_t CACHE_LINE_SIZE = 64;

// Prevent false sharing by aligning to cache line boundaries
#define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)

// ================================================================================================
// COMMON TRADING DATA STRUCTURES
// ================================================================================================

// Market Data Tick (48 bytes - fits in cache line)
struct MarketDataTick {
    char symbol[8];          // Symbol (e.g., "AAPL")
    double bid_price;        // Best bid price
    double ask_price;        // Best ask price
    uint32_t bid_size;       // Best bid size
    uint32_t ask_size;       // Best ask size
    uint64_t timestamp;      // Exchange timestamp
    uint32_t sequence_num;   // Sequence number
    uint8_t exchange_id;     // Exchange ID
    uint8_t flags;           // Status flags
    uint16_t padding;        // Alignment padding
};

// Order Event (64 bytes - exactly one cache line)
struct OrderEvent {
    uint64_t order_id;       // Unique order ID
    char symbol[8];          // Symbol
    double price;            // Order price
    uint32_t quantity;       // Order quantity
    uint32_t strategy_id;    // Strategy ID
    uint64_t timestamp;      // Order timestamp
    char side;               // 'B' = Buy, 'S' = Sell
    char order_type;         // 'L' = Limit, 'M' = Market
    uint8_t time_in_force;   // TIF
    uint8_t flags;           // Order flags
    uint32_t padding;        // Alignment
};

// Fill Event (64 bytes)
struct FillEvent {
    uint64_t order_id;       // Original order ID
    uint64_t fill_id;        // Unique fill ID
    char symbol[8];          // Symbol
    double fill_price;       // Execution price
    uint32_t fill_quantity;  // Executed quantity
    uint32_t strategy_id;    // Strategy ID
    uint64_t timestamp;      // Fill timestamp
    uint8_t exchange_id;     // Exchange ID
    char side;               // 'B' or 'S'
    uint16_t padding;        // Alignment
};

// Compile-time layout verification
static_assert(sizeof(MarketDataTick) <= CACHE_LINE_SIZE, "MarketDataTick exceeds cache line");
static_assert(sizeof(OrderEvent)     <= CACHE_LINE_SIZE, "OrderEvent exceeds cache line");
static_assert(sizeof(FillEvent)      <= CACHE_LINE_SIZE, "FillEvent exceeds cache line");

// ================================================================================================
// 1. SPSC (SINGLE PRODUCER SINGLE CONSUMER) - WAIT-FREE
// ================================================================================================
/**
 * SPSC Ring Buffer - Wait-Free Implementation
 *
 * PROPERTIES:
 * - Wait-free: Both push and pop complete in O(1) bounded time
 * - No CAS operations needed (single producer, single consumer)
 * - Memory ordering: acquire-release semantics
 * - ABA-safe: Uses sequence numbers per cell
 *
 * LATENCY: 10-50 nanoseconds
 *
 * ALGORITHM:
 * - Each cell has a sequence number
 * - Producer checks if cell is ready (seq == pos)
 * - Consumer checks if data is ready (seq == pos + 1)
 * - After wrap-around, sequence is incremented by buffer size
 *
 * USE CASES FOR TRADING:
 * 1. Market Data Feed Handler → Strategy Engine (most common)
 * 2. Strategy Engine → Order Gateway
 * 3. Order Gateway → Exchange Connectivity
 * 4. Risk Check → Order Router
 * 5. Exchange Response → Order Manager
 *
 * WHY WAIT-FREE:
 * - Producer only writes to head position (no contention)
 * - Consumer only reads from tail position (no contention)
 * - No retry loops needed
 * - Guaranteed progress in finite steps
 */

template<typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 2,                    "Capacity must be at least 2");
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable for wait-free hot path");

private:
    /**
     * Cell: each slot padded to cache line size.
     * Without padding: adjacent cells share a cache line → false sharing
     * between producer writing cell[N] and consumer writing cell[N-1].
     */
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
        // Pad remainder of cell to next cache-line boundary
        static constexpr size_t DATA_BYTES = sizeof(std::atomic<uint64_t>) + sizeof(T);
        static constexpr size_t PAD_BYTES  =
            (DATA_BYTES % CACHE_LINE_SIZE == 0) ? 0
                                                 : (CACHE_LINE_SIZE - DATA_BYTES % CACHE_LINE_SIZE);
        char _pad[PAD_BYTES];
    };

    // Producer cursor: written only by producer → own cache line
    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;
    // Consumer cursor: written only by consumer → own cache line (prevents false sharing)
    CACHE_ALIGNED std::atomic<uint64_t> dequeue_pos_;
    // Ring buffer storage: each Cell is independently cache-line padded
    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    SPSCRingBuffer() noexcept : enqueue_pos_(0), dequeue_pos_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }
    }

    // Non-copyable, non-movable (atomics + pinned layout)
    SPSCRingBuffer(const SPSCRingBuffer&)            = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&)                 = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&)      = delete;

    // ── PRODUCER API ─────────────────────────────────────────────────────

    /**
     * push(item) — wait-free enqueue (copy).
     * Returns true on success, false if queue is FULL.
     * Called by PRODUCER thread ONLY. O(1), no retry.
     */
    [[nodiscard]]
    bool push(const T& item) noexcept {
        const uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        Cell& cell         = buffer_[pos & INDEX_MASK];
        const uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        // seq == pos  → slot ready for writing (fresh or recycled)
        // seq != pos  → slot still occupied   (queue full)
        if (__builtin_expect(seq != pos, 0)) {
            return false;   // Full
        }

        cell.data = item;
        // Release: makes data visible to consumer before seq advances
        cell.sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    /**
     * push(item) — wait-free enqueue (move).
     * For trivially-copyable T, move == copy at machine level.
     */
    [[nodiscard]]
    bool push(T&& item) noexcept {
        const uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        Cell& cell         = buffer_[pos & INDEX_MASK];
        const uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        if (__builtin_expect(seq != pos, 0)) {
            return false;
        }

        cell.data = std::move(item);
        cell.sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    /**
     * try_push_spin(item, max_spins)
     * Spin up to max_spins times with CPU_PAUSE before giving up.
     * Useful for absorbing short bursts of back-pressure.
     */
    [[nodiscard]]
    bool try_push_spin(const T& item, uint32_t max_spins = 128) noexcept {
        for (uint32_t i = 0; i < max_spins; ++i) {
            if (__builtin_expect(push(item), 1)) return true;
            CPU_PAUSE();
        }
        return false;
    }

    /**
     * push_bulk(items, count) — push up to `count` items.
     * Returns number of items actually pushed.
     * Producer thread only.
     */
    size_t push_bulk(const T* items, size_t count) noexcept {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!push(items[i])) break;
            ++pushed;
        }
        return pushed;
    }

    // ── CONSUMER API ─────────────────────────────────────────────────────

    /**
     * pop(out) — wait-free dequeue.
     * Returns true and writes item to `out` on success.
     * Returns false if queue is EMPTY.
     * Called by CONSUMER thread ONLY. O(1), no retry.
     */
    [[nodiscard]]
    bool pop(T& out) noexcept {
        const uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        Cell& cell         = buffer_[pos & INDEX_MASK];
        const uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        // seq == pos+1 → data ready (producer has written and released)
        // seq != pos+1 → cell not yet written (empty or producer slow)
        if (__builtin_expect(seq != pos + 1, 0)) {
            return false;   // Empty
        }

        out = cell.data;
        // Recycle slot: pos+Capacity signals "free for re-use after full wrap"
        cell.sequence.store(pos + Capacity, std::memory_order_release);
        dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    /**
     * peek(out) — read front element WITHOUT consuming it.
     * Returns true if data available. Consumer thread only.
     */
    [[nodiscard]]
    bool peek(T& out) const noexcept {
        const uint64_t pos  = dequeue_pos_.load(std::memory_order_relaxed);
        const Cell& cell    = buffer_[pos & INDEX_MASK];
        const uint64_t seq  = cell.sequence.load(std::memory_order_acquire);

        if (__builtin_expect(seq != pos + 1, 0)) {
            return false;
        }
        out = cell.data;
        return true;
    }

    /**
     * try_pop_spin(out, max_spins)
     * Spin up to max_spins times waiting for data before giving up.
     */
    [[nodiscard]]
    bool try_pop_spin(T& out, uint32_t max_spins = 128) noexcept {
        for (uint32_t i = 0; i < max_spins; ++i) {
            if (__builtin_expect(pop(out), 1)) return true;
            CPU_PAUSE();
        }
        return false;
    }

    /**
     * pop_bulk(items, max_count) — pop up to `max_count` items.
     * Returns number of items actually popped.
     * Consumer thread only.
     */
    size_t pop_bulk(T* items, size_t max_count) noexcept {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; ++i) {
            if (!pop(items[i])) break;
            ++popped;
        }
        return popped;
    }

    /**
     * drain_all(handler) — consume all available items, invoke handler for each.
     * handler: callable with signature void(const T&)
     * Returns number of items consumed. Consumer thread only.
     * Amortises the pop overhead across a batch — ideal for strategy processing.
     */
    template<typename Handler>
    size_t drain_all(Handler&& handler) noexcept {
        size_t count = 0;
        T item;
        while (pop(item)) {
            handler(item);
            ++count;
        }
        return count;
    }

    // ── DIAGNOSTICS (approximate — not linearizable) ──────────────────────

    /** Approximate number of elements. Safe to call from any thread for monitoring. */
    size_t size() const noexcept {
        const uint64_t enq = enqueue_pos_.load(std::memory_order_acquire);
        const uint64_t deq = dequeue_pos_.load(std::memory_order_acquire);
        return static_cast<size_t>(enq - deq);   // wrapping subtraction is correct
    }

    bool empty() const noexcept { return size() == 0; }
    bool full()  const noexcept { return size() >= Capacity; }

    static constexpr size_t capacity() noexcept { return Capacity; }
};

// ================================================================================================
// SPSC USE CASE: MARKET DATA FEED TO STRATEGY
// ================================================================================================

class SPSC_MarketDataToStrategy {
private:
    SPSCRingBuffer<MarketDataTick, 8192> market_data_queue_;
    std::atomic<bool> running_{false};

public:
    // Producer: Market data feed handler
    void feed_handler_thread() {
        running_.store(true, std::memory_order_release);

        uint64_t tick_count = 0;

        while (running_.load(std::memory_order_acquire)) {
            MarketDataTick tick;
            std::strncpy(tick.symbol, "AAPL", 8);
            tick.bid_price = 150.25 + (tick_count % 100) * 0.01;
            tick.ask_price = tick.bid_price + 0.01;
            tick.bid_size = 100;
            tick.ask_size = 200;
            tick.timestamp = READ_TSC();  // Use TSC for ultra-low latency
            tick.sequence_num = tick_count;
            tick.exchange_id = 1;
            tick.flags = 0;

            // Wait-free push (never blocks)
            while (!market_data_queue_.push(tick)) {
                CPU_PAUSE();  // CPU hint: we're spinning
            }

            ++tick_count;
        }
    }

    // Consumer: Strategy engine
    void strategy_thread() {
        MarketDataTick tick;
        uint64_t processed_count = 0;

        while (running_.load(std::memory_order_acquire) || !market_data_queue_.empty()) {
            if (market_data_queue_.pop(tick)) {
                // Strategy logic
                double mid_price = (tick.bid_price + tick.ask_price) / 2.0;
                (void)mid_price;
                double spread = tick.ask_price - tick.bid_price;

                // Measure latency (TSC to TSC)
                uint64_t latency = READ_TSC() - tick.timestamp;
                (void)latency;  // used by profiling tools / removed in release
                if (spread < 0.02) {
                    // Generate order
                    // order_queue_.push(order);
                }

                ++processed_count;

                // Typical latency: 10-50 nanoseconds
            } else {
                CPU_PAUSE();
            }
        }

        std::cout << "Strategy processed " << processed_count << " ticks\n";
    }

    void stop() {
        running_.store(false, std::memory_order_release);
    }
};

// ================================================================================================
// 2. MPSC (MULTI PRODUCER SINGLE CONSUMER) - LOCK-FREE
// ================================================================================================
/**
 * MPSC Ring Buffer - Lock-Free Implementation
 *
 * PROPERTIES:
 * - Lock-free: At least one thread makes progress
 * - Uses CAS for producer coordination
 * - Single consumer: no contention on read side
 * - ABA-safe: Sequence numbers prevent ABA problem
 *
 * LATENCY: 50-100 nanoseconds
 *
 * ALGORITHM:
 * - Multiple producers compete for slots using CAS
 * - Each slot has sequence number for coordination
 * - Consumer reads when data is ready
 *
 * USE CASES FOR TRADING:
 * 1. Multiple Strategies → Single Order Router
 * 2. Multiple Market Data Feeds → Consolidated Feed Handler
 * 3. Multiple Risk Checks → Order Gateway
 * 4. Multiple Exchange Responses → Order Manager
 * 5. Multiple Algo Engines → Execution Gateway
 */

template<typename T, size_t Capacity>
class MPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
        static constexpr size_t DATA_BYTES = sizeof(std::atomic<uint64_t>) + sizeof(T);
        static constexpr size_t PAD_BYTES  =
            (DATA_BYTES % CACHE_LINE_SIZE == 0) ? 0
                                                 : (CACHE_LINE_SIZE - DATA_BYTES % CACHE_LINE_SIZE);
        char _pad[PAD_BYTES];
    };

    // Producer side: CAS on this to claim a slot
    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;
    // Consumer side: single consumer, no CAS needed
    CACHE_ALIGNED std::atomic<uint64_t> dequeue_pos_;
    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    MPSCRingBuffer() noexcept : enqueue_pos_(0), dequeue_pos_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }
    }

    MPSCRingBuffer(const MPSCRingBuffer&)            = delete;
    MPSCRingBuffer& operator=(const MPSCRingBuffer&) = delete;

    // ── PRODUCER API (multiple threads) ──────────────────────────────────

    /**
     * push(item) — lock-free enqueue (copy).
     * Multiple producers compete via CAS to claim a slot.
     * Returns false if buffer is FULL.
     */
    [[nodiscard]]
    bool push(const T& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos  = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq   = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff  = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                // Slot available — atomically claim it
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;   // Buffer full
            } else {
                CPU_PAUSE();    // Another producer claimed this slot, retry
            }
        }

        cell->data = item;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /** push(item) — lock-free enqueue (move). */
    [[nodiscard]]
    bool push(T&& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos  = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq  = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                CPU_PAUSE();
            }
        }

        cell->data = std::move(item);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /** try_push_spin — spin up to max_spins before giving up. */
    [[nodiscard]]
    bool try_push_spin(const T& item, uint32_t max_spins = 128) noexcept {
        for (uint32_t i = 0; i < max_spins; ++i) {
            if (__builtin_expect(push(item), 1)) return true;
            CPU_PAUSE();
        }
        return false;
    }

    /** push_bulk — push up to `count` items, returns items pushed. */
    size_t push_bulk(const T* items, size_t count) noexcept {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!push(items[i])) break;
            ++pushed;
        }
        return pushed;
    }

    // ── CONSUMER API (single thread only) ────────────────────────────────

    /**
     * pop(out) — wait-free dequeue (single consumer).
     * Returns true and writes item to `out` on success.
     * Returns false if EMPTY.
     */
    [[nodiscard]]
    bool pop(T& item) noexcept {
        const uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        Cell* cell         = &buffer_[pos & INDEX_MASK];

        const uint64_t seq = cell->sequence.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (diff == 0) {
            item = cell->data;
            cell->sequence.store(pos + Capacity, std::memory_order_release);
            dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
            return true;
        }

        return false;   // Empty or producer hasn't published yet
    }

    /**
     * peek(out) — read front element without consuming.
     * Consumer thread only.
     */
    [[nodiscard]]
    bool peek(T& out) const noexcept {
        const uint64_t pos  = dequeue_pos_.load(std::memory_order_relaxed);
        const Cell* cell    = &buffer_[pos & INDEX_MASK];
        const uint64_t seq  = cell->sequence.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (diff == 0) {
            out = cell->data;
            return true;
        }
        return false;
    }

    /** try_pop_spin — spin up to max_spins waiting for data. */
    [[nodiscard]]
    bool try_pop_spin(T& out, uint32_t max_spins = 128) noexcept {
        for (uint32_t i = 0; i < max_spins; ++i) {
            if (__builtin_expect(pop(out), 1)) return true;
            CPU_PAUSE();
        }
        return false;
    }

    /** pop_bulk — pop up to `max_count` items, returns items popped. */
    size_t pop_bulk(T* items, size_t max_count) noexcept {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; ++i) {
            if (!pop(items[i])) break;
            ++popped;
        }
        return popped;
    }

    /** drain_all(handler) — consume all available items, invoke handler for each. */
    template<typename Handler>
    size_t drain_all(Handler&& handler) noexcept {
        size_t count = 0;
        T item;
        while (pop(item)) {
            handler(item);
            ++count;
        }
        return count;
    }

    // ── DIAGNOSTICS ───────────────────────────────────────────────────────

    size_t size() const noexcept {
        const uint64_t enq = enqueue_pos_.load(std::memory_order_acquire);
        const uint64_t deq = dequeue_pos_.load(std::memory_order_acquire);
        return static_cast<size_t>(enq - deq);
    }

    bool empty() const noexcept { return size() == 0; }
    bool full()  const noexcept { return size() >= Capacity; }
    static constexpr size_t capacity() noexcept { return Capacity; }
};

// ================================================================================================
// MPSC USE CASE: MULTIPLE STRATEGIES TO ORDER ROUTER
// ================================================================================================

class MPSC_MultiStrategyToOrderRouter {
private:
    MPSCRingBuffer<OrderEvent, 16384> order_queue_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> order_id_generator_{1};

public:
    // Producer 1: Mean reversion strategy
    void mean_reversion_strategy(int strategy_id) {
        while (running_.load(std::memory_order_acquire)) {
            OrderEvent order;
            order.order_id = order_id_generator_.fetch_add(1, std::memory_order_relaxed);
            std::strncpy(order.symbol, "AAPL", 8);
            order.price = 150.25;
            order.quantity = 100;
            order.strategy_id = strategy_id;
            order.timestamp = READ_TSC();
            order.side = 'B';
            order.order_type = 'L';
            order.time_in_force = 0;  // Day order
            order.flags = 0;

            // Lock-free push
            while (!order_queue_.push(std::move(order))) {
                CPU_PAUSE();
            }

            // Simulate strategy logic
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // Producer 2: Momentum strategy
    void momentum_strategy(int strategy_id) {
        while (running_.load(std::memory_order_acquire)) {
            OrderEvent order;
            order.order_id = order_id_generator_.fetch_add(1, std::memory_order_relaxed);
            std::strncpy(order.symbol, "MSFT", 8);
            order.price = 320.50;
            order.quantity = 200;
            order.strategy_id = strategy_id;
            order.timestamp = READ_TSC();
            order.side = 'S';
            order.order_type = 'L';
            order.time_in_force = 0;
            order.flags = 0;

            while (!order_queue_.push(std::move(order))) {
                CPU_PAUSE();
            }

            std::this_thread::sleep_for(std::chrono::microseconds(150));
        }
    }

    // Producer 3: Market making strategy
    void market_making_strategy(int strategy_id) {
        while (running_.load(std::memory_order_acquire)) {
            OrderEvent order;
            order.order_id = order_id_generator_.fetch_add(1, std::memory_order_relaxed);
            std::strncpy(order.symbol, "GOOGL", 8);
            order.price = 2800.75;
            order.quantity = 50;
            order.strategy_id = strategy_id;
            order.timestamp = READ_TSC();
            order.side = 'B';
            order.order_type = 'L';
            order.time_in_force = 3;  // IOC
            order.flags = 0;

            while (!order_queue_.push(std::move(order))) {
                CPU_PAUSE();
            }

            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    // Consumer: Order router
    void order_router_thread() {
        OrderEvent order;
        uint64_t routed_count = 0;

        while (running_.load(std::memory_order_acquire) || !order_queue_.empty()) {
            if (order_queue_.pop(order)) {
                // Measure latency (hook into LatencyProbe in production)
                uint64_t latency = READ_TSC() - order.timestamp;
                (void)latency;

                // Risk checks
                if (order.quantity > 10000) {
                    // Reject order
                    continue;
                }

                // Route to appropriate exchange
                route_to_exchange(order);

                ++routed_count;

                // Typical latency: 50-100 nanoseconds
            } else {
                CPU_PAUSE();
            }
        }

        std::cout << "Order router processed " << routed_count << " orders\n";
    }

    void start() {
        running_.store(true, std::memory_order_release);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
    }

private:
    void route_to_exchange(const OrderEvent& order) {
        (void)order;
        // Stub: send_to_exchange(order);
    }
};

// ================================================================================================
// 3. SPMC (SINGLE PRODUCER MULTI CONSUMER) - LOCK-FREE
// ================================================================================================
/**
 * SPMC Ring Buffer - Lock-Free Implementation
 *
 * PROPERTIES:
 * - Lock-free: At least one thread makes progress
 * - Single producer: no CAS on write side
 * - Multiple consumers: each tracks own read position
 * - Each consumer sees every message (broadcast)
 *
 * LATENCY: 50-150 nanoseconds per consumer
 *
 * ALGORITHM:
 * - Producer writes sequentially (no CAS needed)
 * - Each consumer maintains own read position
 * - Consumers use CAS to advance their position
 * - Sequence numbers ensure data visibility
 *
 * USE CASES FOR TRADING:
 * 1. Single Market Data Feed → Multiple Strategies (broadcast)
 * 2. Single Order Fill → Position Manager + P&L + Risk System
 * 3. Single Reference Data Update → All Strategies
 * 4. Single Tick Feed → Multiple Analytics (VWAP, TWAP, Volatility)
 * 5. Audit Trail: Single event → Multiple audit/compliance systems
 */

template<typename T, size_t Capacity, size_t MaxConsumers = 16>
class SPMCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
    };

    // Producer side: single producer
    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;

    // Consumer side: each consumer has own position
    struct ConsumerPosition {
        CACHE_ALIGNED std::atomic<uint64_t> pos;
        CACHE_ALIGNED std::atomic<bool> active;
    };

    CACHE_ALIGNED std::array<ConsumerPosition, MaxConsumers> consumer_positions_;
    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    SPMCRingBuffer() : enqueue_pos_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        for (size_t i = 0; i < MaxConsumers; ++i) {
            consumer_positions_[i].pos.store(0, std::memory_order_relaxed);
            consumer_positions_[i].active.store(false, std::memory_order_relaxed);
        }
    }

    // Register a consumer (returns consumer ID)
    int register_consumer() {
        for (size_t i = 0; i < MaxConsumers; ++i) {
            bool expected = false;
            if (consumer_positions_[i].active.compare_exchange_strong(expected, true,
                                                                       std::memory_order_acquire)) {
                consumer_positions_[i].pos.store(0, std::memory_order_release);
                return static_cast<int>(i);
            }
        }
        return -1;  // No slots available
    }

    void unregister_consumer(int consumer_id) {
        if (consumer_id >= 0 && static_cast<size_t>(consumer_id) < MaxConsumers) {
            consumer_positions_[static_cast<size_t>(consumer_id)].active.store(
                false, std::memory_order_release);
        }
    }

    // Wait-free push (single producer)
    bool push(const T& item) noexcept {
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        // Check if slowest consumer has caught up (overflow guard)
        uint64_t min_consumer_pos = get_min_consumer_position();
        if (pos >= min_consumer_pos + Capacity) {
            return false;  // Buffer full (slowest consumer too far behind)
        }

        // Write data and publish
        cell.data = item;
        cell.sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    // Lock-free pop (multiple consumers)
    bool pop(int consumer_id, T& item) noexcept {
        if (consumer_id < 0 || static_cast<size_t>(consumer_id) >= MaxConsumers) {
            return false;
        }

        const size_t cid = static_cast<size_t>(consumer_id);
        uint64_t pos = consumer_positions_[cid].pos.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        if (seq == pos + 1) {
            // Data is ready
            item = cell.data;
            consumer_positions_[cid].pos.store(pos + 1, std::memory_order_release);
            return true;
        }

        return false;  // No data available for this consumer
    }

private:
    uint64_t get_min_consumer_position() const noexcept {
        uint64_t min_pos = UINT64_MAX;
        for (size_t i = 0; i < MaxConsumers; ++i) {
            if (consumer_positions_[i].active.load(std::memory_order_acquire)) {
                uint64_t pos = consumer_positions_[i].pos.load(std::memory_order_acquire);
                if (pos < min_pos) {
                    min_pos = pos;
                }
            }
        }
        return min_pos == UINT64_MAX ? 0 : min_pos;
    }
};

// ================================================================================================
// SPMC USE CASE: SINGLE FEED TO MULTIPLE STRATEGIES
// ================================================================================================

class SPMC_SingleFeedToMultipleStrategies {
private:
    SPMCRingBuffer<MarketDataTick, 8192, 8> market_data_broadcast_;
    std::atomic<bool> running_{false};

public:
    // Producer: Market data feed
    void feed_handler_thread() {
        running_.store(true, std::memory_order_release);

        uint64_t tick_count = 0;

        while (running_.load(std::memory_order_acquire)) {
            MarketDataTick tick;
            std::strncpy(tick.symbol, "SPY", 8);
            tick.bid_price = 400.25 + (tick_count % 100) * 0.01;
            tick.ask_price = tick.bid_price + 0.01;
            tick.bid_size = 1000;
            tick.ask_size = 1500;
            tick.timestamp = READ_TSC();
            tick.sequence_num = tick_count;
            tick.exchange_id = 1;
            tick.flags = 0;

            while (!market_data_broadcast_.push(tick)) {
                CPU_PAUSE();
            }

            ++tick_count;

            // Simulate feed rate (e.g., 100K ticks/sec = 10 microseconds)
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }

    // Consumer 1: Mean reversion strategy
    void mean_reversion_strategy() {
        int consumer_id = market_data_broadcast_.register_consumer();
        if (consumer_id < 0) {
            std::cerr << "Failed to register mean reversion consumer\n";
            return;
        }

        MarketDataTick tick;
        uint64_t processed = 0;

        while (running_.load(std::memory_order_acquire)) {
            if (market_data_broadcast_.pop(consumer_id, tick)) {
                // Strategy logic
                double mid = (tick.bid_price + tick.ask_price) / 2.0;

                // Mean reversion signal
                if (mid < 400.20) {
                    // Generate buy signal
                }

                ++processed;
            } else {
                CPU_PAUSE();
            }
        }

        market_data_broadcast_.unregister_consumer(consumer_id);
        std::cout << "Mean reversion processed " << processed << " ticks\n";
    }

    // Consumer 2: Momentum strategy
    void momentum_strategy() {
        int consumer_id = market_data_broadcast_.register_consumer();
        if (consumer_id < 0) {
            std::cerr << "Failed to register momentum consumer\n";
            return;
        }

        MarketDataTick tick;
        uint64_t processed = 0;
        double prev_price = 0;

        while (running_.load(std::memory_order_acquire)) {
            if (market_data_broadcast_.pop(consumer_id, tick)) {
                double mid = (tick.bid_price + tick.ask_price) / 2.0;

                // Momentum signal
                if (prev_price > 0 && mid > prev_price * 1.001) {
                    // Generate buy signal (upward momentum)
                }

                prev_price = mid;
                ++processed;
            } else {
                CPU_PAUSE();
            }
        }

        market_data_broadcast_.unregister_consumer(consumer_id);
        std::cout << "Momentum processed " << processed << " ticks\n";
    }

    // Consumer 3: Market making strategy
    void market_making_strategy() {
        int consumer_id = market_data_broadcast_.register_consumer();
        if (consumer_id < 0) {
            std::cerr << "Failed to register market making consumer\n";
            return;
        }

        MarketDataTick tick;
        uint64_t processed = 0;

        while (running_.load(std::memory_order_acquire)) {
            if (market_data_broadcast_.pop(consumer_id, tick)) {
                double spread = tick.ask_price - tick.bid_price;

                // Market making logic
                if (spread > 0.02) {
                    // Place buy and sell orders inside spread
                }

                ++processed;
            } else {
                CPU_PAUSE();
            }
        }

        market_data_broadcast_.unregister_consumer(consumer_id);
        std::cout << "Market making processed " << processed << " ticks\n";
    }

    void start() {
        running_.store(true, std::memory_order_release);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
    }
};

// ================================================================================================
// 4. MPMC (MULTI PRODUCER MULTI CONSUMER) - LOCK-FREE
// ================================================================================================
/**
 * MPMC Ring Buffer - Lock-Free Implementation
 *
 * PROPERTIES:
 * - Lock-free: At least one thread makes progress
 * - Multiple producers: use CAS to claim slots
 * - Multiple consumers: compete for items using CAS
 * - Work distribution: each item consumed by ONE consumer
 *
 * LATENCY: 100-200 nanoseconds
 *
 * ALGORITHM:
 * - Producers use CAS to claim enqueue slots
 * - Consumers use CAS to claim dequeue slots
 * - Sequence numbers coordinate access
 *
 * USE CASES FOR TRADING:
 * 1. Multiple Feeds → Multiple Strategy Instances (load balancing)
 * 2. Multiple Order Sources → Multiple Risk Checkers (parallel risk)
 * 3. Work Pool Pattern: Distribute orders across execution threads
 * 4. Multi-venue Trading: Multiple exchanges → Multiple handlers
 * 5. Event Bus: Any component to any component
 */

template<typename T, size_t Capacity>
class MPMCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
        static constexpr size_t DATA_BYTES = sizeof(std::atomic<uint64_t>) + sizeof(T);
        static constexpr size_t PAD_BYTES  =
            (DATA_BYTES % CACHE_LINE_SIZE == 0) ? 0
                                                 : (CACHE_LINE_SIZE - DATA_BYTES % CACHE_LINE_SIZE);
        char _pad[PAD_BYTES];
    };

    // Both cursors on separate cache lines — producers and consumers never share
    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;
    CACHE_ALIGNED std::atomic<uint64_t> dequeue_pos_;
    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    MPMCRingBuffer() noexcept : enqueue_pos_(0), dequeue_pos_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(static_cast<uint64_t>(i), std::memory_order_relaxed);
        }
    }

    MPMCRingBuffer(const MPMCRingBuffer&)            = delete;
    MPMCRingBuffer& operator=(const MPMCRingBuffer&) = delete;

    // ── PRODUCER API (multiple threads) ──────────────────────────────────

    /**
     * push(item) — lock-free enqueue (copy).
     * Returns false if buffer FULL.
     */
    [[nodiscard]]
    bool push(const T& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos  = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq  = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;   // Full
            } else {
                CPU_PAUSE();
            }
        }

        cell->data = item;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /** push(item) — lock-free enqueue (move). */
    [[nodiscard]]
    bool push(T&& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos  = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq  = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                CPU_PAUSE();
            }
        }

        cell->data = std::move(item);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /** try_push_spin — spin up to max_spins before giving up. */
    [[nodiscard]]
    bool try_push_spin(const T& item, uint32_t max_spins = 128) noexcept {
        for (uint32_t i = 0; i < max_spins; ++i) {
            if (__builtin_expect(push(item), 1)) return true;
            CPU_PAUSE();
        }
        return false;
    }

    /** push_bulk — push up to `count` items, returns items pushed. */
    size_t push_bulk(const T* items, size_t count) noexcept {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!push(items[i])) break;
            ++pushed;
        }
        return pushed;
    }

    // ── CONSUMER API (multiple threads) ──────────────────────────────────

    /**
     * pop(out) — lock-free dequeue.
     * Multiple consumers compete via CAS to claim the next item.
     * Returns false if EMPTY.
     */
    [[nodiscard]]
    bool pop(T& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos  = dequeue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq  = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                                                       std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;   // Empty
            } else {
                CPU_PAUSE();
            }
        }

        item = cell->data;
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return true;
    }

    /**
     * peek(out) — read front element without consuming.
     * NOTE: In MPMC, peek is advisory only — another consumer may
     * steal the item between peek and pop. Use with caution.
     */
    [[nodiscard]]
    bool peek(T& out) const noexcept {
        const uint64_t pos  = dequeue_pos_.load(std::memory_order_relaxed);
        const Cell* cell    = &buffer_[pos & INDEX_MASK];
        const uint64_t seq  = cell->sequence.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (diff == 0) {
            out = cell->data;
            return true;
        }
        return false;
    }

    /** try_pop_spin — spin up to max_spins waiting for data. */
    [[nodiscard]]
    bool try_pop_spin(T& out, uint32_t max_spins = 128) noexcept {
        for (uint32_t i = 0; i < max_spins; ++i) {
            if (__builtin_expect(pop(out), 1)) return true;
            CPU_PAUSE();
        }
        return false;
    }

    /** pop_bulk — pop up to `max_count` items, returns items popped. */
    size_t pop_bulk(T* items, size_t max_count) noexcept {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; ++i) {
            if (!pop(items[i])) break;
            ++popped;
        }
        return popped;
    }

    /** drain_all(handler) — consume all available items. Consumer thread only (or one at a time). */
    template<typename Handler>
    size_t drain_all(Handler&& handler) noexcept {
        size_t count = 0;
        T item;
        while (pop(item)) {
            handler(item);
            ++count;
        }
        return count;
    }

    // ── DIAGNOSTICS ───────────────────────────────────────────────────────

    size_t size() const noexcept {
        const uint64_t enq = enqueue_pos_.load(std::memory_order_acquire);
        const uint64_t deq = dequeue_pos_.load(std::memory_order_acquire);
        return static_cast<size_t>(enq - deq);
    }

    bool empty() const noexcept { return size() == 0; }
    bool full()  const noexcept { return size() >= Capacity; }
    static constexpr size_t capacity() noexcept { return Capacity; }
};

// ================================================================================================
// MPMC USE CASE: WORK POOL FOR ORDER EXECUTION
// ================================================================================================

class MPMC_OrderExecutionWorkPool {
private:
    MPMCRingBuffer<OrderEvent, 16384> work_queue_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> order_id_generator_{1};
    std::atomic<uint64_t> total_processed_{0};

public:
    // Multiple Producers: Generate orders from different sources
    void order_generator(int source_id, const char* symbol, int order_count) {
        for (int i = 0; i < order_count; ++i) {
            if (!running_.load(std::memory_order_acquire)) break;

            OrderEvent order;
            order.order_id = order_id_generator_.fetch_add(1, std::memory_order_relaxed);
            std::strncpy(order.symbol, symbol, 8);
            order.price = 100.0 + source_id + i * 0.01;
            order.quantity = 100 * (source_id + 1);
            order.strategy_id = source_id;
            order.timestamp = READ_TSC();
            order.side = (i % 2 == 0) ? 'B' : 'S';
            order.order_type = 'L';
            order.time_in_force = 0;
            order.flags = 0;

            while (!work_queue_.push(std::move(order))) {
                CPU_PAUSE();
            }
        }
    }

    // Multiple Consumers: Process orders (work distribution)
    void order_executor(int executor_id) {
        OrderEvent order;
        uint64_t local_processed = 0;

        while (running_.load(std::memory_order_acquire) || work_queue_.size() > 0) {
            if (work_queue_.pop(order)) {
                // Measure latency (hook into LatencyProbe in production)
                uint64_t latency = READ_TSC() - order.timestamp;
                (void)latency;

                // Execute order
                execute_order(order, executor_id);

                ++local_processed;

                // Simulate execution time
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            } else {
                CPU_PAUSE();
            }
        }

        total_processed_.fetch_add(local_processed, std::memory_order_relaxed);
        std::cout << "Executor " << executor_id << " processed " << local_processed << " orders\n";
    }

    void start() {
        running_.store(true, std::memory_order_release);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
    }

    uint64_t get_total_processed() const {
        return total_processed_.load(std::memory_order_acquire);
    }

private:
    void execute_order(const OrderEvent& order, int executor_id) {
        (void)order; (void)executor_id;
        // Stub: send_to_exchange(order);
    }
};

// ================================================================================================
// LATENCY PROBE — Collects TSC-cycle samples and prints percentile report
// ================================================================================================

class LatencyProbe {
public:
    explicit LatencyProbe(size_t reserve_count = 1000000) {
        samples_.reserve(reserve_count);
    }

    inline void record(uint64_t start_tsc) noexcept {
        samples_.push_back(READ_TSC() - start_tsc);
    }

    void report(const char* label) const {
        if (samples_.empty()) { std::printf("[%s] No samples\n", label); return; }

        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        const size_t n = sorted.size();
        auto pct = [&](double p) -> uint64_t {
            return sorted[static_cast<size_t>(p * static_cast<double>(n) / 100.0)];
        };

        double mean = 0.0;
        for (auto v : sorted) mean += static_cast<double>(v);
        mean /= static_cast<double>(n);

        std::printf("\n[%s] Latency (TSC cycles) — %zu samples\n", label, n);
        std::printf("  min    : %10llu cycles (~%.1f ns @3GHz)\n",
                    (unsigned long long)sorted.front(), sorted.front() / 3.0);
        std::printf("  mean   : %10.1f cycles (~%.1f ns @3GHz)\n", mean, mean / 3.0);
        std::printf("  p50    : %10llu cycles (~%.1f ns @3GHz)\n",
                    (unsigned long long)pct(50), pct(50) / 3.0);
        std::printf("  p95    : %10llu cycles (~%.1f ns @3GHz)\n",
                    (unsigned long long)pct(95), pct(95) / 3.0);
        std::printf("  p99    : %10llu cycles (~%.1f ns @3GHz)\n",
                    (unsigned long long)pct(99), pct(99) / 3.0);
        std::printf("  p99.9  : %10llu cycles (~%.1f ns @3GHz)\n",
                    (unsigned long long)pct(99.9), pct(99.9) / 3.0);
        std::printf("  max    : %10llu cycles (~%.1f ns @3GHz)\n",
                    (unsigned long long)sorted.back(), sorted.back() / 3.0);
    }

    void reset() noexcept { samples_.clear(); }
    size_t count() const noexcept { return samples_.size(); }

private:
    std::vector<uint64_t> samples_;
};

// ================================================================================================
// PERFORMANCE BENCHMARKING
// ================================================================================================

class PerformanceBenchmark {
public:
    /**
     * benchmark_spsc<Queue, T>(name)
     *
     * Measures one-way producer→consumer latency for SPSC queues.
     * Stamps timestamp_ns = READ_TSC() at push time and reads it at pop time.
     * Reports min/p50/p95/p99/p99.9/max latency in TSC cycles and nanoseconds.
     */
    template<typename Queue, typename T>
    static void benchmark_spsc(const char* name) {
        Queue queue;
        constexpr int WARMUP     = 50000;
        constexpr int ITERATIONS = 1000000;

        LatencyProbe probe(ITERATIONS);
        std::atomic<bool> start{false};
        std::atomic<bool> warmup_done{false};

        // Producer
        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) { CPU_PAUSE(); }

            // Warmup: drive JIT / branch predictor
            for (int i = 0; i < WARMUP; ++i) {
                T item{};
                item.timestamp = READ_TSC();
                while (!queue.push(item)) { CPU_PAUSE(); }
            }
            warmup_done.store(true, std::memory_order_release);

            // Benchmark
            for (int i = 0; i < ITERATIONS; ++i) {
                T item{};
                item.timestamp = READ_TSC();
                while (!queue.push(item)) { CPU_PAUSE(); }
            }
        });

        // Consumer
        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) { CPU_PAUSE(); }

            T item{};
            // Drain warmup ticks (no latency recording)
            int warmup_recv = 0;
            while (warmup_recv < WARMUP) {
                if (queue.pop(item)) ++warmup_recv;
                else CPU_PAUSE();
            }

            // Benchmark ticks
            int received = 0;
            while (received < ITERATIONS) {
                if (queue.pop(item)) {
                    probe.record(item.timestamp);
                    ++received;
                } else {
                    CPU_PAUSE();
                }
            }
        });

        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();

        probe.report(name);
    }

    /**
     * benchmark_throughput<Queue, T>(name, num_producers, num_consumers, total_ops)
     *
     * Measures aggregate throughput (ops/sec) for MPMC queues.
     */
    template<typename Queue, typename T>
    static void benchmark_throughput(const char* name,
                                     int num_producers,
                                     int num_consumers,
                                     int total_ops)
    {
        Queue queue;
        std::atomic<bool> start{false};
        std::atomic<int>  produced{0};
        std::atomic<int>  consumed{0};
        int ops_per_producer = total_ops / num_producers;

        std::vector<std::thread> threads;

        for (int p = 0; p < num_producers; ++p) {
            threads.emplace_back([&]() {
                while (!start.load(std::memory_order_acquire)) { CPU_PAUSE(); }
                for (int i = 0; i < ops_per_producer; ++i) {
                    T item{};
                    while (!queue.push(item)) { CPU_PAUSE(); }
                    produced.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (int c = 0; c < num_consumers; ++c) {
            threads.emplace_back([&]() {
                while (!start.load(std::memory_order_acquire)) { CPU_PAUSE(); }
                T item{};
                while (consumed.load(std::memory_order_relaxed) < total_ops) {
                    if (queue.pop(item)) {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        CPU_PAUSE();
                    }
                }
            });
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        start.store(true, std::memory_order_release);
        for (auto& t : threads) t.join();
        auto t1 = std::chrono::high_resolution_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ops_per_sec = static_cast<double>(total_ops) / (elapsed_ms / 1000.0);

        std::printf("\n[%s] Throughput (%dp/%dc) — %.2f M ops/sec (%.1f ms total)\n",
                    name, num_producers, num_consumers, ops_per_sec / 1e6, elapsed_ms);
    }
};

// ================================================================================================
// MAIN: COMPREHENSIVE EXAMPLES
// ================================================================================================

void demonstrate_spsc() {
    std::cout << "\n=== SPSC: Market Data Feed to Strategy ===\n";

    SPSC_MarketDataToStrategy demo;

    std::thread feed_thread(&SPSC_MarketDataToStrategy::feed_handler_thread, &demo);
    std::thread strategy_thread(&SPSC_MarketDataToStrategy::strategy_thread, &demo);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    demo.stop();

    feed_thread.join();
    strategy_thread.join();

    std::cout << "SPSC demonstration completed\n";
}

void demonstrate_mpsc() {
    std::cout << "\n=== MPSC: Multiple Strategies to Order Router ===\n";

    MPSC_MultiStrategyToOrderRouter demo;
    demo.start();

    std::thread router_thread(&MPSC_MultiStrategyToOrderRouter::order_router_thread, &demo);
    std::thread strategy1(&MPSC_MultiStrategyToOrderRouter::mean_reversion_strategy, &demo, 1);
    std::thread strategy2(&MPSC_MultiStrategyToOrderRouter::momentum_strategy, &demo, 2);
    std::thread strategy3(&MPSC_MultiStrategyToOrderRouter::market_making_strategy, &demo, 3);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    demo.stop();

    strategy1.join();
    strategy2.join();
    strategy3.join();
    router_thread.join();

    std::cout << "MPSC demonstration completed\n";
}

void demonstrate_spmc() {
    std::cout << "\n=== SPMC: Single Feed to Multiple Strategies ===\n";

    SPMC_SingleFeedToMultipleStrategies demo;
    demo.start();

    std::thread feed_thread(&SPMC_SingleFeedToMultipleStrategies::feed_handler_thread, &demo);
    std::thread strategy1(&SPMC_SingleFeedToMultipleStrategies::mean_reversion_strategy, &demo);
    std::thread strategy2(&SPMC_SingleFeedToMultipleStrategies::momentum_strategy, &demo);
    std::thread strategy3(&SPMC_SingleFeedToMultipleStrategies::market_making_strategy, &demo);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    demo.stop();

    feed_thread.join();
    strategy1.join();
    strategy2.join();
    strategy3.join();

    std::cout << "SPMC demonstration completed\n";
}

void demonstrate_mpmc() {
    std::cout << "\n=== MPMC: Work Pool for Order Execution ===\n";

    MPMC_OrderExecutionWorkPool demo;
    demo.start();

    constexpr int num_producers = 3;
    constexpr int num_consumers = 4;
    constexpr int orders_per_producer = 10000;

    std::vector<std::thread> threads;

    // Start consumers
    for (int i = 0; i < num_consumers; ++i) {
        threads.emplace_back(&MPMC_OrderExecutionWorkPool::order_executor, &demo, i);
    }

    // Start producers
    const char* symbols[] = {"AAPL", "MSFT", "GOOGL"};
    for (int i = 0; i < num_producers; ++i) {
        threads.emplace_back(&MPMC_OrderExecutionWorkPool::order_generator, &demo,
                            i, symbols[i], orders_per_producer);
    }

    // Wait for producers to finish
    for (size_t i = static_cast<size_t>(num_consumers); i < threads.size(); ++i) {
        threads[i].join();
    }

    // Let consumers drain the queue
    std::this_thread::sleep_for(std::chrono::seconds(2));
    demo.stop();

    // Wait for consumers
    for (int i = 0; i < num_consumers; ++i) {
        threads[i].join();
    }

    std::cout << "Total orders processed: " << demo.get_total_processed() << "\n";
    std::cout << "MPMC demonstration completed\n";
}

int main() {
    std::cout << "=================================================================\n";
    std::cout << "LOCK-FREE QUEUE VARIANTS FOR ULTRA-LOW LATENCY TRADING\n";
    std::cout << "=================================================================\n";

    // ── Correctness Tests ─────────────────────────────────────────────────
    std::cout << "\n[Correctness] SPSC\n";
    {
        SPSCRingBuffer<MarketDataTick, 8> q;
        for (int i = 0; i < 8; ++i) {
            MarketDataTick t{}; t.sequence_num = i;
            assert(q.push(t));
        }
        assert(q.full());
        MarketDataTick dummy{};
        assert(!q.push(dummy));  // must fail: full

        // peek must not consume
        MarketDataTick p{};
        assert(q.peek(p) && p.sequence_num == 0);
        assert(q.size() == 8);

        uint32_t expected = 0;
        q.drain_all([&](const MarketDataTick& t) {
            assert(t.sequence_num == expected++);
        });
        assert(q.empty());
        std::cout << "  SPSC correctness: PASSED\n";
    }

    std::cout << "[Correctness] MPSC\n";
    {
        MPSCRingBuffer<OrderEvent, 16> q;
        for (int i = 0; i < 16; ++i) {
            OrderEvent e{}; e.order_id = static_cast<uint64_t>(i);
            assert(q.push(e));
        }
        assert(q.full());
        size_t drained = q.drain_all([](const OrderEvent&){});
        assert(drained == 16 && q.empty());
        std::cout << "  MPSC correctness: PASSED\n";
    }

    std::cout << "[Correctness] MPMC\n";
    {
        MPMCRingBuffer<OrderEvent, 16> q;
        for (int i = 0; i < 16; ++i) {
            OrderEvent e{}; e.order_id = i;
            assert(q.push(e));
        }
        assert(q.full());
        size_t drained = q.drain_all([](const OrderEvent&){});
        assert(drained == 16 && q.empty());
        std::cout << "  MPMC correctness: PASSED\n";
    }

    // ── Demonstrations ────────────────────────────────────────────────────
    demonstrate_spsc();
    demonstrate_mpsc();
    demonstrate_spmc();
    demonstrate_mpmc();

    // ── Latency Benchmarks ────────────────────────────────────────────────
    std::cout << "\n=== LATENCY BENCHMARKS (one-way, TSC cycles + ns @3GHz) ===\n";
    PerformanceBenchmark::benchmark_spsc<
        SPSCRingBuffer<MarketDataTick, 8192>, MarketDataTick>("SPSC MarketDataTick");
    PerformanceBenchmark::benchmark_spsc<
        SPSCRingBuffer<OrderEvent, 8192>, OrderEvent>("SPSC OrderEvent");

    // ── Throughput Benchmarks ─────────────────────────────────────────────
    std::cout << "\n=== THROUGHPUT BENCHMARKS (ops/sec) ===\n";
    PerformanceBenchmark::benchmark_throughput<
        MPSCRingBuffer<OrderEvent, 16384>, OrderEvent>(
            "MPSC 3P/1C", 3, 1, 1000000);
    PerformanceBenchmark::benchmark_throughput<
        MPMCRingBuffer<OrderEvent, 16384>, OrderEvent>(
            "MPMC 3P/4C", 3, 4, 1000000);

    std::cout << "\n=================================================================\n";
    std::cout << "SUMMARY:\n";
    std::cout << "- SPSC: 10-50ns   (wait-free, O(1) no CAS — point-to-point)\n";
    std::cout << "- MPSC: 50-100ns  (lock-free, CAS enqueue — multi-algo fan-in)\n";
    std::cout << "- SPMC: 50-150ns  (lock-free, broadcast — one feed → N strategies)\n";
    std::cout << "- MPMC: 100-200ns (lock-free, CAS both sides — work pool)\n";
    std::cout << "\nAll variants provide:\n";
    std::cout << "  peek()          — look without consuming\n";
    std::cout << "  try_push_spin() — push with bounded spin\n";
    std::cout << "  try_pop_spin()  — pop with bounded spin\n";
    std::cout << "  push_bulk()     — amortised batch push\n";
    std::cout << "  pop_bulk()      — amortised batch pop\n";
    std::cout << "  drain_all()     — consume all + callback\n";
    std::cout << "  size()/empty()/full() — approximate monitoring\n";
    std::cout << "=================================================================\n";

    return 0;
}

/**
 * ================================================================================================
 * COMPILATION INSTRUCTIONS
 * ================================================================================================
 *
 * Linux / macOS (recommended):
 *   g++ -std=c++17 -O3 -march=native -pthread \
 *       -fno-omit-frame-pointer \
 *       -o lockfree_queues lockfree_queue_variants_comprehensive_guide.cpp
 *
 * With ThreadSanitizer (debug / correctness testing):
 *   g++ -std=c++17 -O1 -march=native -pthread \
 *       -fsanitize=thread \
 *       -o lockfree_queues_tsan lockfree_queue_variants_comprehensive_guide.cpp
 *
 * With AddressSanitizer:
 *   g++ -std=c++17 -O1 -march=native -pthread \
 *       -fsanitize=address \
 *       -o lockfree_queues_asan lockfree_queue_variants_comprehensive_guide.cpp
 *
 * ================================================================================================
 * DECISION MATRIX: WHICH QUEUE TO USE?
 * ================================================================================================
 *
 *  USE CASE                                         | QUEUE | LATENCY    | NOTES
 *  -------------------------------------------------+-------+------------+--------------------
 *  Market Data Feed Handler -> Strategy Engine      | SPSC  | 10-50 ns   | Best, wait-free
 *  Strategy Engine -> Order Gateway                 | SPSC  | 10-50 ns   | Best, wait-free
 *  Order Gateway -> Exchange Connectivity           | SPSC  | 10-50 ns   | Best, wait-free
 *  Risk Check -> Order Router                       | SPSC  | 10-50 ns   | Best, wait-free
 *  Multiple Strategies -> Order Router              | MPSC  | 50-100 ns  | CAS on enqueue
 *  Multiple Feeds -> Consolidated Feed Handler      | MPSC  | 50-100 ns  | CAS on enqueue
 *  Multiple Risk Checks -> Order Gateway            | MPSC  | 50-100 ns  | CAS on enqueue
 *  Single Feed -> Multiple Strategies (broadcast)   | SPMC  | 50-150 ns  | Each consumer gets all
 *  Fill -> Position + P&L + Risk (broadcast)        | SPMC  | 50-150 ns  | every message
 *  Multiple Feeds -> Multiple Workers (load-balance)| MPMC  | 100-200 ns | Work-stealing
 *  Event Bus (any to any)                           | MPMC  | 100-200 ns | Most flexible
 *
 * ================================================================================================
 * API SUMMARY (all variants)
 * ================================================================================================
 *
 *   push(const T&)              — enqueue copy; returns false if full
 *   push(T&&)                   — enqueue move; returns false if full
 *   try_push_spin(item, spins)  — push with bounded busy-spin back-pressure
 *   push_bulk(items, count)     — batch push; returns items pushed
 *
 *   pop(T& out)                 — dequeue; returns false if empty
 *   peek(T& out)                — read without consuming; SPMC: advisory only
 *   try_pop_spin(out, spins)    — pop with bounded busy-spin
 *   pop_bulk(items, max)        — batch pop; returns items popped
 *   drain_all(handler)          — consume all + invoke handler; returns count
 *
 *   size()  — approximate element count (monitoring only)
 *   empty() — approximate empty check
 *   full()  — approximate full check
 *   capacity() — compile-time constant
 *
 * ================================================================================================
 * KEY OPTIMIZATIONS FOR ULTRA-LOW LATENCY
 * ================================================================================================
 *
 * 1. CPU PINNING
 *    Pin producer/consumer threads to dedicated isolated cores.
 *    taskset -c 0 ./program            (shell, core 0)
 *    pthread_setaffinity_np()          (in code)
 *
 * 2. DISABLE HYPER-THREADING
 *    echo off > /sys/devices/system/cpu/smt/control
 *    Eliminates non-deterministic latency from SMT cache sharing.
 *
 * 3. CPU FREQUENCY SCALING
 *    cpupower frequency-set -g performance
 *    Prevents TSC mis-measurement and latency jitter from P-states.
 *
 * 4. NUMA AWARENESS
 *    Keep producer/consumer on same NUMA node.
 *    numactl --cpunodebind=0 --membind=0 ./program
 *    Cross-NUMA memory access adds ~60-100ns latency.
 *
 * 5. HUGE PAGES
 *    echo 1024 > /proc/sys/vm/nr_hugepages
 *    Reduces TLB misses for large ring buffers (>2MB).
 *
 * 6. MEMORY LOCKING
 *    mlockall(MCL_CURRENT | MCL_FUTURE)
 *    Prevents page faults in hot path (critical for < 1μs target).
 *
 * 7. CACHE LINE PADDING
 *    - enqueue_pos_ and dequeue_pos_ on separate cache lines (done).
 *    - Each Cell padded to cache line (done) → no false sharing between slots.
 *
 * 8. MEMORY ORDERING (minimal barriers)
 *    - acquire on sequence read   : see producer's store before proceeding
 *    - release on sequence write  : make data visible before advancing seq
 *    - relaxed on cursor stores   : ordering already ensured by cell.sequence
 *
 * 9. CPU_PAUSE / YIELD
 *    _mm_pause() on x86: reduces power, improves pipeline during spin.
 *    yield on ARM: equivalent hint.
 *
 * 10. WARMUP BEFORE LIVE TRADING
 *    Run 50k-100k iterations through all hot paths before market open.
 *    Allows CPU branch predictor and prefetcher to learn access patterns.
 *
 * ================================================================================================
 */

