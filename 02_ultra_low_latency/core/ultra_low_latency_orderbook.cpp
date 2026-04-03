/*
 * ultra_low_latency_orderbook.cpp
 *
 * Ultra-low latency order book implementation optimized for high-frequency trading
 *
 * Key Performance Features:
 *  - Lock-free design for single-threaded hot path
 *  - Pre-allocated memory pools (no allocations in hot path)
 *  - Cache-line aligned data structures
 *  - SIMD-friendly data layout
 *  - Branch prediction hints
 *  - Price level aggregation with O(1) top-of-book access
 *  - Intrusive linked lists for zero-allocation order management
 *  - Direct memory indexing for price levels
 *
 * Build:
 *   g++ -std=c++20 -O3 -march=native -mtune=native -flto \
 *       -Wall -Wextra -DNDEBUG \
 *       ultra_low_latency_orderbook.cpp -o ultra_low_latency_orderbook -pthread
 *
 * Run:
 *   ./ultra_low_latency_orderbook
 *
 * Performance Targets:
 *  - Add order: < 50 ns
 *  - Cancel order: < 30 ns
 *  - Modify order: < 40 ns
 *  - Top-of-book access: < 5 ns
 */

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>

// Platform-specific includes for SIMD
#ifdef __x86_64__
#include <immintrin.h>  // For x86 SIMD intrinsics
#elif defined(__aarch64__)
#include <arm_neon.h>   // For ARM NEON intrinsics
#endif

// Branch prediction hints
#ifdef __GNUC__
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

// Cache line size for alignment
static constexpr size_t CACHE_LINE_SIZE = 64;

// Force inline for hot path functions
#define FORCE_INLINE __attribute__((always_inline)) inline

using namespace std::chrono;

// ============================================================================
// Order Types
// ============================================================================

enum class Side : uint8_t { BUY = 0, SELL = 1 };

using OrderId = uint64_t;
using Price = int64_t;      // Fixed point: price * 10000 (4 decimal places)
using Quantity = uint64_t;
using Timestamp = uint64_t;

// ============================================================================
// Order Structure (cache-line aligned, intrusive linked list)
// ============================================================================

struct alignas(CACHE_LINE_SIZE) Order {
    OrderId order_id;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    Side side;

    // Intrusive list pointers (within same price level)
    Order* next;
    Order* prev;

    // Pointer to parent price level
    struct PriceLevel* price_level;

    // Padding to fill cache line
    char padding[CACHE_LINE_SIZE - sizeof(OrderId) - sizeof(Price) -
                 sizeof(Quantity) - sizeof(Timestamp) - sizeof(Side) -
                 sizeof(Order*) * 2 - sizeof(void*)];

    Order() : order_id(0), price(0), quantity(0), timestamp(0),
              side(Side::BUY), next(nullptr), prev(nullptr),
              price_level(nullptr) {}
};

static_assert(sizeof(Order) == CACHE_LINE_SIZE, "Order must be cache-line sized");

// ============================================================================
// Price Level (aggregated orders at same price)
// ============================================================================

struct alignas(CACHE_LINE_SIZE) PriceLevel {
    Price price;
    Quantity total_quantity;
    uint32_t order_count;
    uint32_t _padding1;  // Explicit padding for alignment

    // Intrusive doubly-linked list of orders (FIFO within level)
    Order* head;
    Order* tail;

    // Intrusive linked list for price levels
    PriceLevel* next;
    PriceLevel* prev;

    char padding[CACHE_LINE_SIZE - sizeof(Price) - sizeof(Quantity) -
                 sizeof(uint32_t) * 2 - sizeof(Order*) * 2 - sizeof(PriceLevel*) * 2];

    PriceLevel() : price(0), total_quantity(0), order_count(0), _padding1(0),
                   head(nullptr), tail(nullptr), next(nullptr), prev(nullptr) {}

    FORCE_INLINE void add_order(Order* order) {
        if (UNLIKELY(!tail)) {
            head = tail = order;
            order->next = order->prev = nullptr;
        } else {
            tail->next = order;
            order->prev = tail;
            order->next = nullptr;
            tail = order;
        }
        total_quantity += order->quantity;
        ++order_count;
        order->price_level = this;
    }

    FORCE_INLINE void remove_order(Order* order) {
        if (order->prev) order->prev->next = order->next;
        else head = order->next;

        if (order->next) order->next->prev = order->prev;
        else tail = order->prev;

        total_quantity -= order->quantity;
        --order_count;
        order->price_level = nullptr;
    }

    FORCE_INLINE bool is_empty() const { return order_count == 0; }
};

static_assert(sizeof(PriceLevel) == CACHE_LINE_SIZE, "PriceLevel must be cache-line sized");

// ============================================================================
// Memory Pool (pre-allocated, lock-free for single thread)
// ============================================================================

template<typename T, size_t N>
class alignas(CACHE_LINE_SIZE) MemoryPool {
private:
    std::array<T, N> pool_;
    std::array<T*, N> free_list_;
    size_t free_count_;

public:
    MemoryPool() : free_count_(N) {
        for (size_t i = 0; i < N; ++i) {
            free_list_[i] = &pool_[i];
        }
    }

    FORCE_INLINE T* allocate() {
        if (UNLIKELY(free_count_ == 0)) return nullptr;
        return free_list_[--free_count_];
    }

    FORCE_INLINE void deallocate(T* ptr) {
        if (LIKELY(free_count_ < N)) {
            free_list_[free_count_++] = ptr;
        }
    }

    size_t available() const { return free_count_; }
    size_t capacity() const { return N; }
};

// ============================================================================
// Ultra Low Latency Order Book
// ============================================================================

template<size_t MAX_ORDERS = 1000000, size_t MAX_PRICE_LEVELS = 10000>
class alignas(CACHE_LINE_SIZE) UltraLowLatencyOrderBook {
private:
    // Memory pools
    MemoryPool<Order, MAX_ORDERS> order_pool_;
    MemoryPool<PriceLevel, MAX_PRICE_LEVELS> level_pool_;

    // Fast order lookup (direct indexing, assuming order IDs are sequential)
    std::array<Order*, MAX_ORDERS> order_map_;

    // Price level maps (separate for buy/sell for cache locality)
    // Using array for direct indexing when price range is known
    // In production, use hash map or tree for unlimited price range
    static constexpr size_t PRICE_BUCKETS = 100000;
    std::array<PriceLevel*, PRICE_BUCKETS> buy_levels_;
    std::array<PriceLevel*, PRICE_BUCKETS> sell_levels_;

    // Top of book cache (most frequently accessed)
    PriceLevel* best_bid_;
    PriceLevel* best_ask_;

    // Statistics
    uint64_t total_orders_;
    uint64_t total_trades_;

    // Base price for indexing (e.g., 10000.0000 -> 100000000)
    Price base_price_;

public:
    UltraLowLatencyOrderBook(Price base_price = 100000000)
        : best_bid_(nullptr), best_ask_(nullptr),
          total_orders_(0), total_trades_(0),
          base_price_(base_price) {

        order_map_.fill(nullptr);
        buy_levels_.fill(nullptr);
        sell_levels_.fill(nullptr);
    }

    // ========================================================================
    // Core Order Book Operations (HOT PATH)
    // ========================================================================

    FORCE_INLINE bool add_order(OrderId order_id, Side side, Price price, Quantity quantity) {
        // Allocate order from pool
        Order* order = order_pool_.allocate();
        if (UNLIKELY(!order)) return false;

        // Initialize order
        order->order_id = order_id;
        order->price = price;
        order->quantity = quantity;
        order->side = side;

        // Platform-specific timestamp counter
        #ifdef __x86_64__
        order->timestamp = __rdtsc();  // CPU timestamp counter (x86)
        #elif defined(__aarch64__)
        uint64_t val;
        asm volatile("mrs %0, cntvct_el0" : "=r"(val));  // ARM cycle counter
        order->timestamp = val;
        #else
        order->timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        #endif

        // Store in order map
        if (LIKELY(order_id < MAX_ORDERS)) {
            order_map_[order_id] = order;
        }

        // Get or create price level
        PriceLevel* level = get_or_create_level(side, price);
        if (UNLIKELY(!level)) {
            order_pool_.deallocate(order);
            return false;
        }

        // Add order to price level
        level->add_order(order);

        // Update top of book
        update_top_of_book(side, level);

        ++total_orders_;
        return true;
    }

    FORCE_INLINE bool cancel_order(OrderId order_id) {
        if (UNLIKELY(order_id >= MAX_ORDERS)) return false;

        Order* order = order_map_[order_id];
        if (UNLIKELY(!order)) return false;

        PriceLevel* level = order->price_level;
        Side side = order->side;

        // Remove order from level
        level->remove_order(order);

        // Remove level if empty
        if (UNLIKELY(level->is_empty())) {
            remove_level(side, level);
        } else {
            // Update top of book if this was best level
            if ((side == Side::BUY && level == best_bid_) ||
                (side == Side::SELL && level == best_ask_)) {
                update_top_of_book_after_cancel(side);
            }
        }

        // Clean up
        order_map_[order_id] = nullptr;
        order_pool_.deallocate(order);

        return true;
    }

    FORCE_INLINE bool modify_order(OrderId order_id, Quantity new_quantity) {
        if (UNLIKELY(order_id >= MAX_ORDERS)) return false;

        Order* order = order_map_[order_id];
        if (UNLIKELY(!order)) return false;

        PriceLevel* level = order->price_level;

        // Update quantities
        level->total_quantity = level->total_quantity - order->quantity + new_quantity;
        order->quantity = new_quantity;

        return true;
    }

    // ========================================================================
    // Top of Book Access (Ultra Fast - cached values)
    // ========================================================================

    FORCE_INLINE bool get_best_bid(Price& price, Quantity& quantity) const {
        if (UNLIKELY(!best_bid_)) return false;
        price = best_bid_->price;
        quantity = best_bid_->total_quantity;
        return true;
    }

    FORCE_INLINE bool get_best_ask(Price& price, Quantity& quantity) const {
        if (UNLIKELY(!best_ask_)) return false;
        price = best_ask_->price;
        quantity = best_ask_->total_quantity;
        return true;
    }

    FORCE_INLINE bool get_spread(Price& spread) const {
        if (UNLIKELY(!best_bid_ || !best_ask_)) return false;
        spread = best_ask_->price - best_bid_->price;
        return true;
    }

    FORCE_INLINE bool get_mid_price(Price& mid) const {
        if (UNLIKELY(!best_bid_ || !best_ask_)) return false;
        mid = (best_bid_->price + best_ask_->price) / 2;
        return true;
    }

    // ========================================================================
    // Market Depth (multiple levels)
    // ========================================================================

    struct DepthLevel {
        Price price;
        Quantity quantity;
        uint32_t order_count;
    };

    void get_depth(Side side, std::vector<DepthLevel>& levels, size_t max_levels = 10) const {
        levels.clear();
        levels.reserve(max_levels);

        PriceLevel* current = (side == Side::BUY) ? best_bid_ : best_ask_;

        while (current && levels.size() < max_levels) {
            levels.push_back({current->price, current->total_quantity, current->order_count});
            current = (side == Side::BUY) ? current->prev : current->next;
        }
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    void print_stats() const {
        std::cout << "\n=== Order Book Statistics ===\n";
        std::cout << "Total orders: " << total_orders_ << "\n";
        std::cout << "Order pool available: " << order_pool_.available()
                  << "/" << order_pool_.capacity() << "\n";
        std::cout << "Level pool available: " << level_pool_.available()
                  << "/" << level_pool_.capacity() << "\n";

        Price bid_price = 0, ask_price = 0;
        Quantity bid_qty = 0, ask_qty = 0;

        if (get_best_bid(bid_price, bid_qty)) {
            std::cout << "Best Bid: " << (bid_price / 10000.0)
                      << " @ " << bid_qty << "\n";
        }
        if (get_best_ask(ask_price, ask_qty)) {
            std::cout << "Best Ask: " << (ask_price / 10000.0)
                      << " @ " << ask_qty << "\n";
        }

        Price spread;
        if (get_spread(spread)) {
            std::cout << "Spread: " << (spread / 10000.0) << "\n";
        }
    }

private:
    // ========================================================================
    // Internal Helper Functions
    // ========================================================================

    FORCE_INLINE size_t price_to_index(Price price) const {
        // Convert price to bucket index
        // Assuming price range: base_price ± PRICE_BUCKETS/2
        int64_t offset = price - base_price_;
        return (offset + PRICE_BUCKETS / 2) % PRICE_BUCKETS;
    }

    FORCE_INLINE PriceLevel* get_or_create_level(Side side, Price price) {
        size_t idx = price_to_index(price);
        auto& levels = (side == Side::BUY) ? buy_levels_ : sell_levels_;

        PriceLevel* level = levels[idx];

        if (LIKELY(level && level->price == price)) {
            return level;
        }

        // Create new level
        level = level_pool_.allocate();
        if (UNLIKELY(!level)) return nullptr;

        level->price = price;
        level->total_quantity = 0;
        level->order_count = 0;
        level->head = level->tail = nullptr;
        level->next = level->prev = nullptr;

        levels[idx] = level;
        insert_level_sorted(side, level);

        return level;
    }

    FORCE_INLINE void insert_level_sorted(Side side, PriceLevel* level) {
        // Insert into sorted linked list
        if (side == Side::BUY) {
            // Buy side: descending order (highest first)
            if (!best_bid_ || level->price > best_bid_->price) {
                level->next = best_bid_;
                level->prev = nullptr;
                if (best_bid_) best_bid_->prev = level;
                best_bid_ = level;
            } else {
                PriceLevel* current = best_bid_;
                while (current->next && current->next->price > level->price) {
                    current = current->next;
                }
                level->next = current->next;
                level->prev = current;
                if (current->next) current->next->prev = level;
                current->next = level;
            }
        } else {
            // Sell side: ascending order (lowest first)
            if (!best_ask_ || level->price < best_ask_->price) {
                level->next = best_ask_;
                level->prev = nullptr;
                if (best_ask_) best_ask_->prev = level;
                best_ask_ = level;
            } else {
                PriceLevel* current = best_ask_;
                while (current->next && current->next->price < level->price) {
                    current = current->next;
                }
                level->next = current->next;
                level->prev = current;
                if (current->next) current->next->prev = level;
                current->next = level;
            }
        }
    }

    FORCE_INLINE void remove_level(Side side, PriceLevel* level) {
        size_t idx = price_to_index(level->price);
        auto& levels = (side == Side::BUY) ? buy_levels_ : sell_levels_;

        // Remove from linked list
        if (level->prev) level->prev->next = level->next;
        else {
            if (side == Side::BUY) best_bid_ = level->next;
            else best_ask_ = level->next;
        }

        if (level->next) level->next->prev = level->prev;

        // Clear bucket
        levels[idx] = nullptr;

        // Return to pool
        level_pool_.deallocate(level);
    }

    FORCE_INLINE void update_top_of_book(Side side, PriceLevel* level) {
        if (side == Side::BUY) {
            if (!best_bid_ || level->price > best_bid_->price) {
                // New level is better than current best
                // Already handled in insert_level_sorted
            }
        } else {
            if (!best_ask_ || level->price < best_ask_->price) {
                // New level is better than current best
                // Already handled in insert_level_sorted
            }
        }
    }

    FORCE_INLINE void update_top_of_book_after_cancel(Side /* side */) {
        // Top of book is already maintained by the linked list
        // No action needed as best_bid_/best_ask_ point to the head
    }
};

// ============================================================================
// Benchmarking
// ============================================================================

class LatencyBenchmark {
private:
    std::vector<uint64_t> latencies_;

public:
    void record(uint64_t latency_ns) {
        latencies_.push_back(latency_ns);
    }

    void print_statistics(const std::string& operation) const {
        if (latencies_.empty()) return;

        std::vector<uint64_t> sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());

        double avg = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
        uint64_t min = sorted.front();
        uint64_t max = sorted.back();
        uint64_t p50 = sorted[sorted.size() * 50 / 100];
        uint64_t p95 = sorted[sorted.size() * 95 / 100];
        uint64_t p99 = sorted[sorted.size() * 99 / 100];
        uint64_t p999 = sorted[sorted.size() * 999 / 1000];

        std::cout << "\n=== " << operation << " Latency (nanoseconds) ===\n";
        std::cout << "Samples: " << sorted.size() << "\n";
        std::cout << "Min:     " << min << " ns\n";
        std::cout << "Avg:     " << std::fixed << std::setprecision(2) << avg << " ns\n";
        std::cout << "P50:     " << p50 << " ns\n";
        std::cout << "P95:     " << p95 << " ns\n";
        std::cout << "P99:     " << p99 << " ns\n";
        std::cout << "P99.9:   " << p999 << " ns\n";
        std::cout << "Max:     " << max << " ns\n";
    }

    void clear() { latencies_.clear(); }
};

// ============================================================================
// Test & Benchmark Suite
// ============================================================================

void run_benchmarks() {
    std::cout << "=== Ultra Low Latency Order Book Benchmark ===\n";

    UltraLowLatencyOrderBook<> book(1000000000);  // Base price: 100000.0000

    LatencyBenchmark add_bench, cancel_bench, modify_bench, query_bench;

    const size_t NUM_ORDERS = 100000;
    const Price BASE_PRICE = 1000000000;  // 100000.0000

    // Warmup
    std::cout << "\nWarming up...\n";
    for (size_t i = 0; i < 1000; ++i) {
        book.add_order(i, Side::BUY, BASE_PRICE - (i % 100) * 10000, 100);
    }
    for (size_t i = 0; i < 1000; ++i) {
        book.cancel_order(i);
    }

    // Benchmark: Add Orders
    std::cout << "\nBenchmarking Add Order...\n";
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        Price price = BASE_PRICE + ((side == Side::BUY) ? -(int64_t)(i % 100) : (i % 100)) * 10000;
        Quantity qty = 100 + (i % 900);

        auto t1 = high_resolution_clock::now();
        book.add_order(i, side, price, qty);
        auto t2 = high_resolution_clock::now();

        add_bench.record(duration_cast<nanoseconds>(t2 - t1).count());
    }

    add_bench.print_statistics("Add Order");

    // Benchmark: Query Top of Book
    std::cout << "\nBenchmarking Top-of-Book Query...\n";
    for (size_t i = 0; i < 1000000; ++i) {
        Price bid, ask;
        Quantity bid_qty, ask_qty;

        auto t1 = high_resolution_clock::now();
        book.get_best_bid(bid, bid_qty);
        book.get_best_ask(ask, ask_qty);
        auto t2 = high_resolution_clock::now();

        query_bench.record(duration_cast<nanoseconds>(t2 - t1).count());
    }

    query_bench.print_statistics("Top-of-Book Query");

    // Benchmark: Modify Order
    std::cout << "\nBenchmarking Modify Order...\n";
    for (size_t i = 0; i < NUM_ORDERS / 2; ++i) {
        auto t1 = high_resolution_clock::now();
        book.modify_order(i, 200 + (i % 800));
        auto t2 = high_resolution_clock::now();

        modify_bench.record(duration_cast<nanoseconds>(t2 - t1).count());
    }

    modify_bench.print_statistics("Modify Order");

    // Benchmark: Cancel Order
    std::cout << "\nBenchmarking Cancel Order...\n";
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        auto t1 = high_resolution_clock::now();
        book.cancel_order(i);
        auto t2 = high_resolution_clock::now();

        cancel_bench.record(duration_cast<nanoseconds>(t2 - t1).count());
    }

    cancel_bench.print_statistics("Cancel Order");

    book.print_stats();
}

void run_functional_test() {
    std::cout << "\n=== Functional Test ===\n";

    UltraLowLatencyOrderBook<> book(1000000000);

    // Add buy orders
    book.add_order(1, Side::BUY, 999900000, 100);   // 99990.0000 @ 100
    book.add_order(2, Side::BUY, 999950000, 200);   // 99995.0000 @ 200
    book.add_order(3, Side::BUY, 999950000, 150);   // 99995.0000 @ 150

    // Add sell orders
    book.add_order(4, Side::SELL, 1000050000, 100); // 100005.0000 @ 100
    book.add_order(5, Side::SELL, 1000100000, 200); // 100010.0000 @ 200

    book.print_stats();

    // Test depth
    std::cout << "\n--- Buy Side Depth ---\n";
    std::vector<UltraLowLatencyOrderBook<>::DepthLevel> buy_depth;
    book.get_depth(Side::BUY, buy_depth, 5);
    for (const auto& level : buy_depth) {
        std::cout << "Price: " << (level.price / 10000.0)
                  << ", Qty: " << level.quantity
                  << ", Orders: " << level.order_count << "\n";
    }

    std::cout << "\n--- Sell Side Depth ---\n";
    std::vector<UltraLowLatencyOrderBook<>::DepthLevel> sell_depth;
    book.get_depth(Side::SELL, sell_depth, 5);
    for (const auto& level : sell_depth) {
        std::cout << "Price: " << (level.price / 10000.0)
                  << ", Qty: " << level.quantity
                  << ", Orders: " << level.order_count << "\n";
    }

    // Test modify
    std::cout << "\nModifying order 2 to quantity 500...\n";
    book.modify_order(2, 500);
    book.print_stats();

    // Test cancel
    std::cout << "\nCancelling order 1...\n";
    book.cancel_order(1);
    book.print_stats();
}

int main() {
    std::cout << "Ultra Low Latency Order Book Implementation\n";
    std::cout << "Cache Line Size: " << CACHE_LINE_SIZE << " bytes\n";
    std::cout << "Order Size: " << sizeof(Order) << " bytes\n";
    std::cout << "PriceLevel Size: " << sizeof(PriceLevel) << " bytes\n";

    run_functional_test();
    run_benchmarks();

    return 0;
}

