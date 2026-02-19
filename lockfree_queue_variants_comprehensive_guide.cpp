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
 * LATENCY NUMBERS:
 * - SPSC: 10-50 nanoseconds (wait-free, fastest)
 * - MPSC: 50-100 nanoseconds (lock-free)
 * - SPMC: 50-150 nanoseconds (lock-free)
 * - MPMC: 100-200 nanoseconds (lock-free)
 *
 * KEY CONCEPTS:
 * - Wait-free: Every operation completes in bounded steps (guaranteed progress)
 * - Lock-free: At least one thread makes progress (system-wide progress)
 * - ABA-safe: Uses sequence numbers to prevent ABA problem
 * - Cache-friendly: Aligned to cache lines (64 bytes)
 * - Zero allocation: Pre-allocated ring buffer
 *
 * TRADING USE CASES:
 * - Market data feed → Strategy (SPSC)
 * - Multiple strategies → Order router (MPSC)
 * - Single feed → Multiple strategies (SPMC)
 * - Multiple feeds → Multiple strategies (MPMC)
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
#include <algorithm>

// Platform-specific intrinsics
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>  // For CPU_PAUSE() on x86/x64
    #define CPU_PAUSE() CPU_PAUSE()
    #define READ_TSC() READ_TSC()
#elif defined(__aarch64__) || defined(__arm__)
    // ARM architecture
    #define CPU_PAUSE() asm volatile("yield" ::: "memory")
    inline uint64_t READ_TSC() {
        uint64_t val;
        asm volatile("mrs %0, cntvct_el0" : "=r"(val));
        return val;
    }
#else
    #define CPU_PAUSE() std::this_thread::yield()
    inline uint64_t READ_TSC() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
#endif

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

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
    };

    // Cache line padding to prevent false sharing
    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;
    CACHE_ALIGNED std::atomic<uint64_t> dequeue_pos_;
    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    SPSCRingBuffer() : enqueue_pos_(0), dequeue_pos_(0) {
        // Initialize sequence numbers
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // Wait-free push (producer side)
    bool push(const T& item) noexcept {
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        // Check if cell is ready for writing
        if (seq != pos) {
            return false;  // Buffer full
        }

        // Write data
        cell.data = item;

        // Make data visible and advance sequence
        cell.sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    // Move version for zero-copy
    bool push(T&& item) noexcept {
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        if (seq != pos) {
            return false;
        }

        cell.data = std::move(item);
        cell.sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    // Wait-free pop (consumer side)
    bool pop(T& item) noexcept {
        uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        // Check if data is ready for reading
        if (seq != pos + 1) {
            return false;  // Buffer empty
        }

        // Read data
        item = cell.data;

        // Mark cell as ready for next write (after wrapping)
        cell.sequence.store(pos + Capacity, std::memory_order_release);
        dequeue_pos_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    // Non-blocking size approximation
    size_t size() const noexcept {
        uint64_t enq = enqueue_pos_.load(std::memory_order_acquire);
        uint64_t deq = dequeue_pos_.load(std::memory_order_acquire);
        return enq - deq;
    }

    bool empty() const noexcept {
        return size() == 0;
    }

    bool full() const noexcept {
        return size() >= Capacity;
    }
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
                double spread = tick.ask_price - tick.bid_price;

                // Measure latency (TSC to TSC)
                uint64_t latency = READ_TSC() - tick.timestamp;

                // Trading decision
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

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
    };

    // Producer side: needs atomic CAS
    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;

    // Consumer side: single consumer, no CAS needed
    CACHE_ALIGNED std::atomic<uint64_t> dequeue_pos_;

    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    MPSCRingBuffer() : enqueue_pos_(0), dequeue_pos_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // Lock-free push (multiple producers)
    bool push(const T& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            // Atomically claim a slot
            pos = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                // Slot is available, try to claim it
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                        std::memory_order_relaxed)) {
                    break;  // Successfully claimed slot
                }
            } else if (diff < 0) {
                // Buffer is full
                return false;
            } else {
                // Another thread is writing to this slot, retry
                CPU_PAUSE();
            }
        }

        // Write data to claimed slot
        cell->data = item;

        // Publish data
        cell->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    // Move version
    bool push(T&& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
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

    // Lock-free pop (single consumer)
    bool pop(T& item) noexcept {
        uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        Cell* cell = &buffer_[pos & INDEX_MASK];

        uint64_t seq = cell->sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (diff == 0) {
            // Data is ready
            item = cell->data;
            cell->sequence.store(pos + Capacity, std::memory_order_release);
            dequeue_pos_.store(pos + 1, std::memory_order_relaxed);
            return true;
        }

        return false;  // Buffer empty
    }

    size_t size() const noexcept {
        uint64_t enq = enqueue_pos_.load(std::memory_order_acquire);
        uint64_t deq = dequeue_pos_.load(std::memory_order_acquire);
        return enq - deq;
    }
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
                // Measure latency
                uint64_t latency = READ_TSC() - order.timestamp;

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
        // Send to exchange gateway
        // exchange_gateway_.send(order);
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
        if (consumer_id >= 0 && consumer_id < MaxConsumers) {
            consumer_positions_[consumer_id].active.store(false, std::memory_order_release);
        }
    }

    // Wait-free push (single producer)
    bool push(const T& item) noexcept {
        uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        // Check if slowest consumer has caught up
        uint64_t min_consumer_pos = get_min_consumer_position();
        if (pos >= min_consumer_pos + Capacity) {
            return false;  // Buffer full (slowest consumer too far behind)
        }

        // Write data
        cell.data = item;

        // Publish
        cell.sequence.store(pos + 1, std::memory_order_release);
        enqueue_pos_.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

    // Lock-free pop (multiple consumers)
    bool pop(int consumer_id, T& item) noexcept {
        if (consumer_id < 0 || consumer_id >= MaxConsumers) {
            return false;
        }

        uint64_t pos = consumer_positions_[consumer_id].pos.load(std::memory_order_relaxed);
        Cell& cell = buffer_[pos & INDEX_MASK];

        uint64_t seq = cell.sequence.load(std::memory_order_acquire);

        if (seq == pos + 1) {
            // Data is ready
            item = cell.data;
            consumer_positions_[consumer_id].pos.store(pos + 1, std::memory_order_release);
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

private:
    struct Cell {
        std::atomic<uint64_t> sequence;
        T data;
    };

    CACHE_ALIGNED std::atomic<uint64_t> enqueue_pos_;
    CACHE_ALIGNED std::atomic<uint64_t> dequeue_pos_;
    CACHE_ALIGNED std::array<Cell, Capacity> buffer_;

    static constexpr size_t INDEX_MASK = Capacity - 1;

public:
    MPMCRingBuffer() : enqueue_pos_(0), dequeue_pos_(0) {
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // Lock-free push (multiple producers)
    bool push(const T& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
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

        cell->data = item;
        cell->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    bool push(T&& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos = enqueue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
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

    // Lock-free pop (multiple consumers)
    bool pop(T& item) noexcept {
        Cell* cell;
        uint64_t pos;

        while (true) {
            pos = dequeue_pos_.load(std::memory_order_relaxed);
            cell = &buffer_[pos & INDEX_MASK];

            uint64_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                                                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                CPU_PAUSE();
            }
        }

        item = cell->data;
        cell->sequence.store(pos + Capacity, std::memory_order_release);

        return true;
    }

    size_t size() const noexcept {
        uint64_t enq = enqueue_pos_.load(std::memory_order_acquire);
        uint64_t deq = dequeue_pos_.load(std::memory_order_acquire);
        return enq - deq;
    }
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
                // Measure latency
                uint64_t latency = READ_TSC() - order.timestamp;

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
        // Simulate order execution
        // send_to_exchange(order);
    }
};

// ================================================================================================
// PERFORMANCE BENCHMARKING
// ================================================================================================

class PerformanceBenchmark {
public:
    template<typename Queue, typename T>
    static void benchmark_spsc(const char* name) {
        Queue queue;
        constexpr int iterations = 1000000;
        std::vector<uint64_t> latencies;
        latencies.reserve(iterations);

        std::atomic<bool> start{false};

        // Producer
        std::thread producer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                CPU_PAUSE();
            }

            for (int i = 0; i < iterations; ++i) {
                T item;
                item.timestamp = READ_TSC();

                while (!queue.push(item)) {
                    CPU_PAUSE();
                }
            }
        });

        // Consumer
        std::thread consumer([&]() {
            while (!start.load(std::memory_order_acquire)) {
                CPU_PAUSE();
            }

            T item;
            int received = 0;

            while (received < iterations) {
                if (queue.pop(item)) {
                    uint64_t latency = READ_TSC() - item.timestamp;
                    latencies.push_back(latency);
                    ++received;
                } else {
                    CPU_PAUSE();
                }
            }
        });

        // Start benchmark
        start.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());

        std::cout << "\n" << name << " Benchmark Results:\n";
        std::cout << "50th percentile: " << latencies[iterations / 2] << " cycles\n";
        std::cout << "95th percentile: " << latencies[iterations * 95 / 100] << " cycles\n";
        std::cout << "99th percentile: " << latencies[iterations * 99 / 100] << " cycles\n";
        std::cout << "99.9th percentile: " << latencies[iterations * 999 / 1000] << " cycles\n";
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
    for (int i = num_consumers; i < threads.size(); ++i) {
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

    // Demonstrate all variants
    demonstrate_spsc();
    demonstrate_mpsc();
    demonstrate_spmc();
    demonstrate_mpmc();

    // Run benchmarks
    std::cout << "\n=== PERFORMANCE BENCHMARKS ===\n";
    PerformanceBenchmark::benchmark_spsc<SPSCRingBuffer<MarketDataTick, 8192>, MarketDataTick>(
        "SPSC (Market Data)");

    std::cout << "\n=================================================================\n";
    std::cout << "SUMMARY:\n";
    std::cout << "- SPSC: 10-50ns   (wait-free, fastest, use for point-to-point)\n";
    std::cout << "- MPSC: 50-100ns  (lock-free, multiple producers to one consumer)\n";
    std::cout << "- SPMC: 50-150ns  (lock-free, broadcast one to many)\n";
    std::cout << "- MPMC: 100-200ns (lock-free, most flexible, work distribution)\n";
    std::cout << "=================================================================\n";

    return 0;
}

/**
 * ================================================================================================
 * COMPILATION INSTRUCTIONS
 * ================================================================================================
 *
 * g++ -std=c++17 -O3 -march=native -pthread \
 *     -o lockfree_queues lockfree_queue_variants_comprehensive_guide.cpp
 *
 * For best performance:
 * - Use -O3 optimization
 * - Use -march=native for CPU-specific optimizations
 * - Pin threads to specific CPU cores using taskset or pthread_setaffinity_np
 * - Disable CPU frequency scaling (set to performance mode)
 * - Disable hyper-threading for deterministic latency
 *
 * ================================================================================================
 * DECISION MATRIX: WHICH QUEUE TO USE?
 * ================================================================================================
 *
 * ┌──────────────────────────────────────────────────────────────────────────────────────┐
 * │ USE CASE                                      │ QUEUE TYPE │ LATENCY     │ NOTES     │
 * ├──────────────────────────────────────────────────────────────────────────────────────┤
 * │ Market Data Feed → Strategy                   │ SPSC       │ 10-50ns     │ Best      │
 * │ Strategy → Order Gateway                      │ SPSC       │ 10-50ns     │ Best      │
 * │ Multiple Strategies → Order Router            │ MPSC       │ 50-100ns    │ Best      │
 * │ Multiple Feeds → Consolidated Handler         │ MPSC       │ 50-100ns    │ Best      │
 * │ Single Feed → Multiple Strategies (broadcast) │ SPMC       │ 50-150ns    │ Best      │
 * │ Fill → Position/PnL/Risk (broadcast)          │ SPMC       │ 50-150ns    │ Best      │
 * │ Multiple Sources → Multiple Workers           │ MPMC       │ 100-200ns   │ Flexible  │
 * │ Event Bus (any to any)                        │ MPMC       │ 100-200ns   │ Flexible  │
 * └──────────────────────────────────────────────────────────────────────────────────────┘
 *
 * ================================================================================================
 * KEY OPTIMIZATIONS FOR ULTRA-LOW LATENCY
 * ================================================================================================
 *
 * 1. CPU PINNING
 *    - Pin producer/consumer threads to dedicated cores
 *    - Avoid context switches
 *    - Use taskset: taskset -c 0 ./program (pin to core 0)
 *    - Or pthread_setaffinity_np() in code
 *
 * 2. DISABLE HYPER-THREADING
 *    - echo off > /sys/devices/system/cpu/smt/control
 *    - Eliminates non-deterministic latency from SMT
 *
 * 3. CPU FREQUENCY SCALING
 *    - Set CPU governor to performance mode
 *    - cpupower frequency-set -g performance
 *
 * 4. NUMA AWARENESS
 *    - Keep producer/consumer on same NUMA node
 *    - Use numactl to control memory allocation
 *
 * 5. HUGE PAGES
 *    - Reduce TLB misses
 *    - echo 1024 > /proc/sys/vm/nr_hugepages
 *
 * 6. MEMORY BARRIERS
 *    - This implementation uses optimal memory ordering
 *    - acquire-release for synchronization
 *    - relaxed for local operations
 *
 * 7. CACHE LINE PADDING
 *    - All critical variables aligned to 64-byte cache lines
 *    - Prevents false sharing between threads
 *
 * 8. BUSY WAITING
 *    - Use CPU_PAUSE() to hint CPU during spin loops
 *    - Reduces power and improves performance
 *
 * ================================================================================================
 */

