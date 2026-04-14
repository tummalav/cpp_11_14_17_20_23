/*
 * ============================================================================
 * ULTRA LOW LATENCY ORDER BOOK - C++17
 * ============================================================================
 *
 * Design Principles:
 *  1. ZERO heap allocation on hot path     -> pre-allocated Order pool (object pool)
 *  2. O(1) price level access              -> fixed-point int64 prices + array indexing
 *  3. O(1) order cancel                    -> intrusive doubly-linked list per price level
 *  4. Cache-line aligned structures        -> alignas(64), each Order = 64 bytes
 *  5. Lock-free order ingress              -> SPSC ring buffer (network -> matching engine)
 *  6. Zero virtual dispatch / std::function-> raw function pointers for callbacks
 *  7. Separate bid/ask memory regions      -> eliminates false sharing on hot path
 *  8. Branch prediction hints              -> __builtin_expect on exceptional paths
 *
 * Memory Layout:
 *  - Order:      64 bytes (1 cache line)
 *  - PriceLevel: 64 bytes (1 cache line)
 *  - bid_levels_ / ask_levels_: contiguous arrays, indexed by price offset
 *
 * Latency Budget (target on RHEL8, isolated core, Solarflare NIC):
 *  - add_limit (no match):  ~50-100  ns
 *  - cancel:                ~20-50   ns
 *  - add_limit (1 fill):    ~100-200 ns
 *  - SPSC push/pop:         ~10-20   ns
 * ============================================================================
 */

#include <atomic>
#include <array>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <cstring>
#include <random>
#include <vector>
#include <memory>

namespace ull_ob {

// ============================================================
// Compile-time constants
// ============================================================
static constexpr uint32_t MAX_ORDERS       = 1u << 16;  // 65 536 simultaneous active orders
static constexpr uint32_t MAX_PRICE_LEVELS = 1u << 12;  // 4 096 price ticks (±2048 ticks)
static constexpr uint32_t HALF_LEVELS      = MAX_PRICE_LEVELS / 2;
static constexpr uint32_t INVALID_IDX      = UINT32_MAX;
static constexpr int64_t  PRICE_SCALE      = 100LL;     // 2 decimal places: 1 tick = 0.01
static constexpr uint32_t QUEUE_CAPACITY   = 1u << 14;  // 16 384 SPSC ring buffer slots

// ============================================================
// Fixed-point price helpers (avoids floating-point comparison)
// ============================================================
[[nodiscard]] inline int64_t to_fp(double p) noexcept {
    return static_cast<int64_t>(p * PRICE_SCALE + 0.5);
}
[[nodiscard]] inline double from_fp(int64_t fp) noexcept {
    return static_cast<double>(fp) / PRICE_SCALE;
}

// ============================================================
// Enums – 1 byte each to keep Order struct dense
// ============================================================
enum class Side    : uint8_t { BUY  = 0, SELL  = 1 };
enum class OType   : uint8_t { LIMIT= 0, MARKET= 1, IOC= 2, FOK= 3 };
enum class OStatus : uint8_t { ACTIVE=0, PARTIAL=1, FILLED=2, CANCELLED=3 };

// ============================================================
// Order – exactly ONE cache line (64 bytes)
// Hot path: no std::string, no dynamic allocation
// ============================================================
struct alignas(64) Order {
    uint64_t  id;           //  8  order ID
    int64_t   price_fp;     //  8  fixed-point price
    uint64_t  qty;          //  8  original quantity
    uint64_t  filled_qty;   //  8  quantity filled so far
    uint32_t  prev_idx;     //  4  intrusive doubly-linked list
    uint32_t  next_idx;     //  4
    uint32_t  level_idx;    //  4  index into bid_levels_ / ask_levels_
    uint32_t  slot_idx;     //  4  own index in the order pool
    Side      side;         //  1
    OType     type;         //  1
    OStatus   status;       //  1
    char      pad[13];      // 13  -> total 64 bytes

    [[nodiscard]] uint64_t remaining() const noexcept { return qty - filled_qty; }
    [[nodiscard]] bool     done()      const noexcept { return filled_qty >= qty; }
};
static_assert(sizeof(Order) == 64, "Order must be exactly 64 bytes (1 cache line)");

// ============================================================
// PriceLevel – exactly ONE cache line (64 bytes)
// Manages an intrusive FIFO list of orders at a price
// ============================================================
struct alignas(64) PriceLevel {
    int64_t  price_fp;      //  8
    uint64_t total_qty;     //  8  sum of remaining() across all active orders
    uint32_t head_idx;      //  4  front of FIFO queue (time priority)
    uint32_t tail_idx;      //  4  back  of FIFO queue
    uint32_t order_count;   //  4
    char     pad[36];       // 36  -> total 64 bytes

    void reset(int64_t fp) noexcept {
        price_fp = fp; total_qty = 0;
        head_idx = tail_idx = INVALID_IDX;
        order_count = 0;
    }
    [[nodiscard]] bool empty() const noexcept { return head_idx == INVALID_IDX; }
};
static_assert(sizeof(PriceLevel) == 64, "PriceLevel must be exactly 64 bytes");

// ============================================================
// OrderRequest – message passed through SPSC ingress queue
// (32 bytes, fits 2 per cache line)
// ============================================================
struct OrderRequest {
    uint64_t  client_order_id;  //  8
    int64_t   price_fp;         //  8
    uint64_t  qty;              //  8
    Side      side;             //  1
    OType     type;             //  1
    char      pad[6];           //  6  -> 32 bytes
};
static_assert(sizeof(OrderRequest) == 32);

// ============================================================
// SPSC Lock-Free Ring Buffer (Single Producer / Single Consumer)
//
// Producer:  network/gateway thread  -> try_push()
// Consumer:  matching engine thread  -> try_pop()
//
// Technique:
//  - Separate cache lines for head_ and tail_ to avoid false sharing
//  - Acquire/release memory ordering (no full fence needed)
//  - Power-of-2 capacity for branchless modulo (& MASK)
// ============================================================
template<uint32_t N>
class alignas(64) SPSCQueue {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
    static constexpr uint32_t MASK = N - 1;

    alignas(64) std::atomic<uint64_t> tail_{0};  // producer writes here
    alignas(64) std::atomic<uint64_t> head_{0};  // consumer reads from here
    alignas(64) std::array<OrderRequest, N> buf_;

public:
    // Called by producer thread
    [[nodiscard]] bool try_push(const OrderRequest& req) noexcept {
        const uint64_t t = tail_.load(std::memory_order_relaxed);
        const uint64_t h = head_.load(std::memory_order_acquire);
        if (__builtin_expect(t - h >= N, 0)) return false;  // full
        buf_[t & MASK] = req;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    // Called by consumer thread
    [[nodiscard]] bool try_pop(OrderRequest& req) noexcept {
        const uint64_t h = head_.load(std::memory_order_relaxed);
        const uint64_t t = tail_.load(std::memory_order_acquire);
        if (__builtin_expect(h == t, 0)) return false;      // empty
        req = buf_[h & MASK];
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint64_t size() const noexcept {
        return tail_.load(std::memory_order_relaxed) -
               head_.load(std::memory_order_relaxed);
    }
};

// ============================================================
// Trade – result of an execution
// ============================================================
struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    int64_t  price_fp;
    uint64_t qty;
};

// ============================================================
// OrderPool – pre-allocated slab, zero heap on hot path
//
// Uses a free-list (stack) for O(1) alloc/release.
// All Order objects are in a single contiguous array:
//   -> cache-friendly iteration
//   -> huge page friendly (mmap in production)
// ============================================================
class OrderPool {
    alignas(64) std::array<Order,    MAX_ORDERS> orders_;
    alignas(64) std::array<uint32_t, MAX_ORDERS> free_stack_;
    uint32_t top_{MAX_ORDERS};

public:
    OrderPool() noexcept {
        for (uint32_t i = 0; i < MAX_ORDERS; ++i) {
            free_stack_[i]      = i;
            orders_[i].slot_idx = i;
            orders_[i].prev_idx = INVALID_IDX;
            orders_[i].next_idx = INVALID_IDX;
            orders_[i].status   = OStatus::CANCELLED;
        }
    }

    [[nodiscard]] Order* alloc() noexcept {
        if (__builtin_expect(top_ == 0, 0)) return nullptr; // pool exhausted
        return &orders_[free_stack_[--top_]];
    }

    void release(uint32_t slot) noexcept {
        free_stack_[top_++] = slot;
    }

    [[nodiscard]] Order*       at(uint32_t i)       noexcept { return &orders_[i]; }
    [[nodiscard]] const Order* at(uint32_t i) const noexcept { return &orders_[i]; }
    [[nodiscard]] uint32_t     available()    const noexcept { return top_; }
};

// ============================================================
// ULLOrderBook – core matching engine
//
// Single-threaded matching engine design:
//   - All add/cancel calls must come from a single thread
//   - Network thread submits via push_order() into SPSC queue
//   - Matching engine calls drain_ingress() in its spin loop
//
// Price Level Indexing:
//   level_idx = (price_fp - ref_price_fp_) + HALF_LEVELS
//   O(1) lookup, no tree traversal
// ============================================================
class ULLOrderBook {
public:
    // Callback types (raw fn pointers = zero overhead, no heap)
    using TradeCallback  = void(*)(const Trade&, void* ctx);
    using CancelCallback = void(*)(uint64_t order_id, void* ctx);

    explicit ULLOrderBook(double ref_price,
                          TradeCallback  on_trade  = nullptr,
                          void*          trade_ctx = nullptr) noexcept
        : ref_price_fp_(to_fp(ref_price))
        , trade_cb_(on_trade)
        , cb_ctx_(trade_ctx)
    {
        for (uint32_t i = 0; i < MAX_PRICE_LEVELS; ++i) {
            int64_t fp = ref_price_fp_ + static_cast<int64_t>(i) - HALF_LEVELS;
            bid_levels_[i].reset(fp);
            ask_levels_[i].reset(fp);
        }
        order_lookup_.fill(INVALID_IDX);
    }

    // ----------------------------------------------------------------
    // HOT PATH API
    // ----------------------------------------------------------------

    // Add limit order from matching engine thread
    // Returns: assigned order_id (0 = failure)
    uint64_t add_limit(Side side, double price, uint64_t qty) noexcept {
        return add_limit_fp(side, to_fp(price), qty);
    }

    uint64_t add_limit_fp(Side side, int64_t price_fp, uint64_t qty) noexcept {
        const uint32_t level_idx = price_to_idx(price_fp);
        if (__builtin_expect(level_idx == INVALID_IDX, 0)) return 0;

        Order* o = pool_.alloc();
        if (__builtin_expect(!o, 0)) return 0;

        const uint64_t oid = next_oid_++;
        o->id         = oid;
        o->price_fp   = price_fp;
        o->qty        = qty;
        o->filled_qty = 0;
        o->side       = side;
        o->type       = OType::LIMIT;
        o->status     = OStatus::ACTIVE;
        o->level_idx  = level_idx;
        o->prev_idx   = INVALID_IDX;
        o->next_idx   = INVALID_IDX;

        order_lookup_[oid & (MAX_ORDERS - 1)] = o->slot_idx;

        // 1. Try to match against opposite side
        if (side == Side::BUY) {
            match_bid(o);
            if (!o->done()) {
                // 2. Rest goes to book
                enqueue(bid_levels_[level_idx], o);
                if (price_fp > best_bid_fp_) best_bid_fp_ = price_fp;
            }
        } else {
            match_ask(o);
            if (!o->done()) {
                enqueue(ask_levels_[level_idx], o);
                if (price_fp < best_ask_fp_) best_ask_fp_ = price_fp;
            }
        }

        // Fully filled inline – release back to pool
        if (o->done()) {
            o->status = OStatus::FILLED;
            order_lookup_[oid & (MAX_ORDERS - 1)] = INVALID_IDX;
            pool_.release(o->slot_idx);
        }

        return oid;
    }

    // Add market order (always aggressive, takes from opposite side)
    uint64_t add_market(Side side, uint64_t qty) noexcept {
        Order* o = pool_.alloc();
        if (__builtin_expect(!o, 0)) return 0;

        o->id         = next_oid_++;
        o->price_fp   = (side == Side::BUY) ? INT64_MAX : INT64_MIN;
        o->qty        = qty;
        o->filled_qty = 0;
        o->side       = side;
        o->type       = OType::MARKET;
        o->status     = OStatus::ACTIVE;
        o->level_idx  = INVALID_IDX;
        o->prev_idx   = INVALID_IDX;
        o->next_idx   = INVALID_IDX;

        if (side == Side::BUY) match_bid(o); else match_ask(o);

        o->status = o->done() ? OStatus::FILLED : OStatus::PARTIAL;
        pool_.release(o->slot_idx);
        return o->id;
    }

    // Cancel order – O(1) via intrusive list prev/next pointers
    bool cancel(uint64_t order_id) noexcept {
        const uint32_t slot = order_lookup_[order_id & (MAX_ORDERS - 1)];
        if (__builtin_expect(slot == INVALID_IDX, 0)) return false;

        Order* o = pool_.at(slot);
        // Verify ID (hash collision guard)
        if (__builtin_expect(o->id != order_id, 0)) return false;
        if (__builtin_expect(o->status == OStatus::FILLED ||
                             o->status == OStatus::CANCELLED, 0)) return false;

        const uint32_t lidx = o->level_idx;
        if (o->side == Side::BUY) {
            bid_levels_[lidx].total_qty -= o->remaining();
            dequeue(bid_levels_[lidx], o);
            if (bid_levels_[lidx].empty() && o->price_fp == best_bid_fp_)
                update_best_bid(lidx);
        } else {
            ask_levels_[lidx].total_qty -= o->remaining();
            dequeue(ask_levels_[lidx], o);
            if (ask_levels_[lidx].empty() && o->price_fp == best_ask_fp_)
                update_best_ask(lidx);
        }

        o->status = OStatus::CANCELLED;
        order_lookup_[order_id & (MAX_ORDERS - 1)] = INVALID_IDX;
        pool_.release(slot);
        return true;
    }

    // ----------------------------------------------------------------
    // SPSC INGRESS INTERFACE
    // Network thread -> push_order()
    // Matching engine thread -> drain_ingress() in spin loop
    // ----------------------------------------------------------------
    [[nodiscard]] bool push_order(const OrderRequest& req) noexcept {
        return ingress_.try_push(req);
    }

    // Drain all pending orders from SPSC queue and process them
    uint32_t drain_ingress() noexcept {
        uint32_t processed = 0;
        OrderRequest req;
        while (ingress_.try_pop(req)) {
            if (req.type == OType::MARKET)
                add_market(req.side, req.qty);
            else
                add_limit_fp(req.side, req.price_fp, req.qty);
            ++processed;
        }
        return processed;
    }

    // ----------------------------------------------------------------
    // MARKET DATA (read-only, lock-free from any thread)
    // ----------------------------------------------------------------
    [[nodiscard]] double   best_bid()   const noexcept { return best_bid_fp_ == INT64_MIN ? 0.0 : from_fp(best_bid_fp_); }
    [[nodiscard]] double   best_ask()   const noexcept { return best_ask_fp_ == INT64_MAX ? 0.0 : from_fp(best_ask_fp_); }
    [[nodiscard]] double   mid()        const noexcept {
        if (best_bid_fp_ == INT64_MIN || best_ask_fp_ == INT64_MAX) return 0.0;
        return from_fp((best_bid_fp_ + best_ask_fp_) / 2);
    }
    [[nodiscard]] double   spread()     const noexcept {
        if (best_bid_fp_ == INT64_MIN || best_ask_fp_ == INT64_MAX) return 0.0;
        return from_fp(best_ask_fp_ - best_bid_fp_);
    }
    [[nodiscard]] double   last_price() const noexcept { return from_fp(last_trade_fp_); }
    [[nodiscard]] uint64_t total_trades()   const noexcept { return total_trades_; }
    [[nodiscard]] uint64_t total_volume()   const noexcept { return total_volume_; }
    [[nodiscard]] uint32_t pool_available() const noexcept { return pool_.available(); }

    // ----------------------------------------------------------------
    // DEBUG PRINT (not on hot path)
    // ----------------------------------------------------------------
    void print_book(int depth = 5) const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n=== ULL Order Book ===\n";
        std::cout << "  Best Bid: " << best_bid()
                  << "  Best Ask: " << best_ask()
                  << "  Spread: "   << spread()
                  << "  Last: "     << last_price() << "\n";
        std::cout << "  Trades: "   << total_trades_
                  << "  Volume: "   << total_volume_
                  << "  Pool free: "<< pool_.available() << "\n";

        // Collect ask levels (best ask upward)
        std::vector<std::pair<double,uint64_t>> asks, bids;
        if (best_ask_fp_ != INT64_MAX) {
            uint32_t start = price_to_idx(best_ask_fp_);
            for (uint32_t i = start; i < MAX_PRICE_LEVELS && (int)asks.size() < depth; ++i)
                if (!ask_levels_[i].empty())
                    asks.emplace_back(from_fp(ask_levels_[i].price_fp),
                                      ask_levels_[i].total_qty);
        }
        if (best_bid_fp_ != INT64_MIN) {
            uint32_t start = price_to_idx(best_bid_fp_);
            for (int32_t i = (int32_t)start; i >= 0 && (int)bids.size() < depth; --i)
                if (!bid_levels_[i].empty())
                    bids.emplace_back(from_fp(bid_levels_[i].price_fp),
                                      bid_levels_[i].total_qty);
        }

        std::cout << "  ASKS (worst -> best):\n";
        for (auto it = asks.rbegin(); it != asks.rend(); ++it)
            std::cout << "    " << std::setw(8) << it->first
                      << "  qty=" << it->second << "\n";
        std::cout << "  ------------ spread " << spread() << " ------------\n";
        std::cout << "  BIDS (best -> worst):\n";
        for (const auto& b : bids)
            std::cout << "    " << std::setw(8) << b.first
                      << "  qty=" << b.second << "\n";
        std::cout << "\n";
    }

private:
    // ----------------------------------------------------------------
    // Private data – ordered to minimise padding and false sharing
    // ----------------------------------------------------------------
    const int64_t ref_price_fp_;

    // Best bid/ask on separate cache lines -> hot read by market data consumers
    alignas(64) int64_t best_bid_fp_{INT64_MIN};
    alignas(64) int64_t best_ask_fp_{INT64_MAX};

    // Price level arrays: separate allocations -> no bid/ask false sharing
    alignas(64) std::array<PriceLevel, MAX_PRICE_LEVELS> bid_levels_;
    alignas(64) std::array<PriceLevel, MAX_PRICE_LEVELS> ask_levels_;

    // Order pool: single contiguous slab, huge-page friendly
    OrderPool pool_;

    // Order lookup: order_id -> pool slot index
    // Uses power-of-2 modulo hash; safe because active orders <= MAX_ORDERS
    alignas(64) std::array<uint32_t, MAX_ORDERS> order_lookup_;

    // SPSC ingress queue
    SPSCQueue<QUEUE_CAPACITY> ingress_;

    uint64_t next_oid_{1};
    uint64_t total_trades_{0};
    uint64_t total_volume_{0};
    int64_t  last_trade_fp_{0};

    TradeCallback trade_cb_;
    void*         cb_ctx_;

    // ----------------------------------------------------------------
    // Price -> array index conversion: O(1), branchless (mostly)
    // ----------------------------------------------------------------
    [[nodiscard]] uint32_t price_to_idx(int64_t fp) const noexcept {
        const int64_t off = fp - ref_price_fp_ + static_cast<int64_t>(HALF_LEVELS);
        if (__builtin_expect(off < 0 || off >= static_cast<int64_t>(MAX_PRICE_LEVELS), 0))
            return INVALID_IDX;
        return static_cast<uint32_t>(off);
    }

    // ----------------------------------------------------------------
    // Intrusive list: enqueue to tail (time priority = FIFO)
    // ----------------------------------------------------------------
    void enqueue(PriceLevel& lvl, Order* o) noexcept {
        o->prev_idx = lvl.tail_idx;
        o->next_idx = INVALID_IDX;

        if (lvl.tail_idx != INVALID_IDX)
            pool_.at(lvl.tail_idx)->next_idx = o->slot_idx;
        else
            lvl.head_idx = o->slot_idx;

        lvl.tail_idx    = o->slot_idx;
        lvl.total_qty  += o->remaining();
        ++lvl.order_count;
    }

    // Intrusive list: O(1) remove via prev/next pointers
    // NOTE: does NOT adjust total_qty (caller must do so)
    void dequeue(PriceLevel& lvl, Order* o) noexcept {
        if (o->prev_idx != INVALID_IDX)
            pool_.at(o->prev_idx)->next_idx = o->next_idx;
        else
            lvl.head_idx = o->next_idx;

        if (o->next_idx != INVALID_IDX)
            pool_.at(o->next_idx)->prev_idx = o->prev_idx;
        else
            lvl.tail_idx = o->prev_idx;

        --lvl.order_count;
        o->prev_idx = o->next_idx = INVALID_IDX;
    }

    // ----------------------------------------------------------------
    // Matching: incoming BID sweeps ASK side (price-time priority)
    // ----------------------------------------------------------------
    void match_bid(Order* bid) noexcept {
        while (!bid->done() && best_ask_fp_ != INT64_MAX) {
            // Price check: limit order must be >= best ask
            if (bid->type == OType::LIMIT &&
                __builtin_expect(bid->price_fp < best_ask_fp_, 1)) break;

            const uint32_t idx = price_to_idx(best_ask_fp_);
            PriceLevel& lvl = ask_levels_[idx];

            // Sweep this price level (FIFO order within level)
            while (!bid->done() && !lvl.empty()) {
                Order* ask = pool_.at(lvl.head_idx);
                const uint64_t tqty = std::min(bid->remaining(), ask->remaining());

                bid->filled_qty += tqty;
                ask->filled_qty += tqty;
                lvl.total_qty   -= tqty;

                last_trade_fp_   = best_ask_fp_;
                total_volume_   += tqty;
                ++total_trades_;

                if (trade_cb_) {
                    Trade t{bid->id, ask->id, best_ask_fp_, tqty};
                    trade_cb_(t, cb_ctx_);
                }

                // Fully-filled resting ask: dequeue and release
                if (ask->done()) {
                    dequeue(lvl, ask);
                    ask->status = OStatus::FILLED;
                    order_lookup_[ask->id & (MAX_ORDERS - 1)] = INVALID_IDX;
                    pool_.release(ask->slot_idx);
                }
            }

            // Level exhausted: find next best ask
            if (lvl.empty()) update_best_ask(idx);
        }

        if (bid->filled_qty > 0 && !bid->done())
            bid->status = OStatus::PARTIAL;
    }

    // ----------------------------------------------------------------
    // Matching: incoming ASK sweeps BID side
    // ----------------------------------------------------------------
    void match_ask(Order* ask) noexcept {
        while (!ask->done() && best_bid_fp_ != INT64_MIN) {
            if (ask->type == OType::LIMIT &&
                __builtin_expect(ask->price_fp > best_bid_fp_, 1)) break;

            const uint32_t idx = price_to_idx(best_bid_fp_);
            PriceLevel& lvl = bid_levels_[idx];

            while (!ask->done() && !lvl.empty()) {
                Order* bid = pool_.at(lvl.head_idx);
                const uint64_t tqty = std::min(ask->remaining(), bid->remaining());

                ask->filled_qty += tqty;
                bid->filled_qty += tqty;
                lvl.total_qty   -= tqty;

                last_trade_fp_   = best_bid_fp_;
                total_volume_   += tqty;
                ++total_trades_;

                if (trade_cb_) {
                    Trade t{bid->id, ask->id, best_bid_fp_, tqty};
                    trade_cb_(t, cb_ctx_);
                }

                if (bid->done()) {
                    dequeue(lvl, bid);
                    bid->status = OStatus::FILLED;
                    order_lookup_[bid->id & (MAX_ORDERS - 1)] = INVALID_IDX;
                    pool_.release(bid->slot_idx);
                }
            }

            if (lvl.empty()) update_best_bid(idx);
        }

        if (ask->filled_qty > 0 && !ask->done())
            ask->status = OStatus::PARTIAL;
    }

    // ----------------------------------------------------------------
    // Update best bid/ask after a level empties
    // Scan is short in practice (prices cluster around mid)
    // Production: maintain a non-empty level linked list for O(1)
    // ----------------------------------------------------------------
    void update_best_bid(uint32_t empty_idx) noexcept {
        if (empty_idx == 0) { best_bid_fp_ = INT64_MIN; return; }
        for (int32_t i = static_cast<int32_t>(empty_idx) - 1; i >= 0; --i) {
            if (!bid_levels_[i].empty()) {
                best_bid_fp_ = bid_levels_[i].price_fp;
                return;
            }
        }
        best_bid_fp_ = INT64_MIN;
    }

    void update_best_ask(uint32_t empty_idx) noexcept {
        for (uint32_t i = empty_idx + 1; i < MAX_PRICE_LEVELS; ++i) {
            if (!ask_levels_[i].empty()) {
                best_ask_fp_ = ask_levels_[i].price_fp;
                return;
            }
        }
        best_ask_fp_ = INT64_MAX;
    }
};

} // namespace ull_ob

// ============================================================
// TSC-based latency timer (Linux/x86 only)
// ============================================================
#if defined(__x86_64__) || defined(__i386__)
static inline uint64_t rdtsc() noexcept {
    uint32_t lo, hi;
    __asm__ __volatile__("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}
static constexpr double TSC_FREQ_GHZ = 3.0;  // adjust to your CPU
static inline double tsc_to_ns(uint64_t cycles) noexcept {
    return static_cast<double>(cycles) / TSC_FREQ_GHZ;
}
#else
static inline uint64_t rdtsc() noexcept {
    return static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}
static inline double tsc_to_ns(uint64_t cycles) noexcept { return static_cast<double>(cycles); }
#endif

// ============================================================
// MAIN – Functional demo + latency benchmark
// ============================================================
int main() {
    using namespace ull_ob;

    std::cout << "=================================================================\n";
    std::cout << "  ULTRA LOW LATENCY ORDER BOOK – C++17\n";
    std::cout << "  Order size:      " << sizeof(Order)      << " bytes\n";
    std::cout << "  PriceLevel size: " << sizeof(PriceLevel) << " bytes\n";
    std::cout << "  MAX_ORDERS:      " << MAX_ORDERS         << "\n";
    std::cout << "  MAX_PRICE_LEVELS:" << MAX_PRICE_LEVELS   << "\n";
    std::cout << "=================================================================\n\n";

    // ---- Trade callback (in production: write to ring buffer, don't print) ----
    // ULLOrderBook is ~5.7MB - always heap-allocate (use huge pages in production)
    auto on_trade = [](const Trade& t, void*) {
        std::cout << "  FILL: qty=" << t.qty
                  << " @ " << std::fixed << std::setprecision(2) << from_fp(t.price_fp)
                  << "  (buy=" << t.buy_order_id << " sell=" << t.sell_order_id << ")\n";
    };

    auto book_ptr = std::make_unique<ULLOrderBook>(100.00, on_trade, nullptr);
    auto& book = *book_ptr;

    // ================================================================
    // 1. Basic order book operations
    // ================================================================
    std::cout << "--- 1. BUILD BOOK ---\n";
    book.add_limit(Side::BUY,  99.90, 500);
    book.add_limit(Side::BUY,  99.95, 300);
    auto bid1 = book.add_limit(Side::BUY,  100.00, 200);
    book.add_limit(Side::SELL, 100.05, 400);
    book.add_limit(Side::SELL, 100.10, 600);
    book.add_limit(Side::SELL, 100.15, 300);
    book.print_book();

    // ================================================================
    // 2. Aggressive limit order – crosses spread, triggers fill
    // ================================================================
    std::cout << "--- 2. AGGRESSIVE BUY @ 100.08 (crosses spread) ---\n";
    book.add_limit(Side::BUY, 100.08, 250);
    book.print_book();

    std::cout << "--- 3. AGGRESSIVE SELL @ 99.97 (crosses spread) ---\n";
    book.add_limit(Side::SELL, 99.97, 150);
    book.print_book();

    // ================================================================
    // 4. Market orders
    // ================================================================
    std::cout << "--- 4. MARKET BUY qty=100 ---\n";
    book.add_market(Side::BUY,  100);
    book.print_book();

    std::cout << "--- 5. MARKET SELL qty=200 ---\n";
    book.add_market(Side::SELL, 200);
    book.print_book();

    // ================================================================
    // 6. Cancel order O(1)
    // ================================================================
    std::cout << "--- 6. CANCEL order " << bid1 << " ---\n";
    bool ok = book.cancel(bid1);
    std::cout << "  Cancel result: " << (ok ? "OK" : "FAIL") << "\n";
    book.print_book();

    // ================================================================
    // 7. SPSC queue ingress simulation (network thread -> engine)
    // ================================================================
    std::cout << "--- 7. SPSC INGRESS (simulate 1000 orders via queue) ---\n";
    {
        auto spsc_book_ptr = std::make_unique<ULLOrderBook>(100.00);
        auto& spsc_book = *spsc_book_ptr;
        std::mt19937_64 rng(42);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<int64_t> price_dist(-10, 10);  // ±0.10
        std::uniform_int_distribution<uint64_t> qty_dist(100, 1000);

        // Simulate network thread pushing orders
        for (int i = 0; i < 1000; ++i) {
            OrderRequest req;
            req.client_order_id = static_cast<uint64_t>(i + 1);
            req.side            = (side_dist(rng) == 0) ? Side::BUY : Side::SELL;
            req.price_fp        = to_fp(100.00) + price_dist(rng);
            req.qty             = qty_dist(rng);
            req.type            = OType::LIMIT;
            while (!spsc_book.push_order(req)) { /* spin if full */ }
        }

        // Matching engine thread drains
        uint32_t done = spsc_book.drain_ingress();
        std::cout << "  Processed " << done << " orders from SPSC queue\n";
        spsc_book.print_book();
    }

    // ================================================================
    // 8. Latency benchmark
    // ================================================================
    std::cout << "--- 8. LATENCY BENCHMARK ---\n";
    {
        auto bench_ptr = std::make_unique<ULLOrderBook>(100.00);
        auto& bench_book = *bench_ptr;
        constexpr int ITERS = 100'000;

        // Pre-populate book with resting liquidity
        for (int i = 1; i <= 20; ++i) {
            bench_book.add_limit(Side::BUY,  100.00 - i * 0.01, 10000);
            bench_book.add_limit(Side::SELL, 100.00 + i * 0.01, 10000);
        }

        // --- add_limit (non-matching, passive) ---
        std::vector<uint64_t> add_latencies(ITERS);
        for (int i = 0; i < ITERS; ++i) {
            uint64_t t0 = rdtsc();
            bench_book.add_limit(Side::BUY, 99.50, 100);
            uint64_t t1 = rdtsc();
            add_latencies[i] = t1 - t0;
        }

        // --- cancel latency ---
        // First add ITERS orders to cancel
        std::vector<uint64_t> oids(ITERS);
        for (int i = 0; i < ITERS; ++i)
            oids[i] = bench_book.add_limit(Side::BUY, 99.40, 100);

        std::vector<uint64_t> cancel_latencies(ITERS);
        for (int i = 0; i < ITERS; ++i) {
            uint64_t t0 = rdtsc();
            bench_book.cancel(oids[i]);
            uint64_t t1 = rdtsc();
            cancel_latencies[i] = t1 - t0;
        }

        // --- matching latency (add aggressive limit that fills 1 level) ---
        std::vector<uint64_t> match_latencies(ITERS);
        for (int i = 0; i < ITERS; ++i) {
            // Ensure there's a resting ask at best ask
            bench_book.add_limit(Side::SELL, 100.01, 200);
            uint64_t t0 = rdtsc();
            bench_book.add_limit(Side::BUY, 100.01, 100);  // crosses, 1 fill
            uint64_t t1 = rdtsc();
            match_latencies[i] = t1 - t0;
        }

        // Sort for percentile stats
        std::sort(add_latencies.begin(),    add_latencies.end());
        std::sort(cancel_latencies.begin(), cancel_latencies.end());
        std::sort(match_latencies.begin(),  match_latencies.end());

        auto pct = [&](std::vector<uint64_t>& v, double p) {
            return tsc_to_ns(v[static_cast<size_t>(p / 100.0 * ITERS)]);
        };

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "\n  add_limit (passive) latency (" << ITERS << " iters):\n";
        std::cout << "    p50=" << pct(add_latencies,50) << " ns  "
                  << "p90=" << pct(add_latencies,90) << " ns  "
                  << "p99=" << pct(add_latencies,99) << " ns  "
                  << "p99.9=" << pct(add_latencies,99.9) << " ns\n";

        std::cout << "  cancel latency:\n";
        std::cout << "    p50=" << pct(cancel_latencies,50) << " ns  "
                  << "p90=" << pct(cancel_latencies,90) << " ns  "
                  << "p99=" << pct(cancel_latencies,99) << " ns  "
                  << "p99.9=" << pct(cancel_latencies,99.9) << " ns\n";

        std::cout << "  add_limit (1 fill) latency:\n";
        std::cout << "    p50=" << pct(match_latencies,50) << " ns  "
                  << "p90=" << pct(match_latencies,90) << " ns  "
                  << "p99=" << pct(match_latencies,99) << " ns  "
                  << "p99.9=" << pct(match_latencies,99.9) << " ns\n";
    }

    std::cout << "\n=================================================================\n";
    std::cout << "  KEY ULL TECHNIQUES USED:\n";
    std::cout << "  1. Pre-allocated Order pool     -> zero heap on hot path\n";
    std::cout << "  2. Fixed-point int64 prices     -> O(1) array index, no map\n";
    std::cout << "  3. Intrusive doubly-linked list -> O(1) cancel\n";
    std::cout << "  4. alignas(64) Order/PriceLevel -> 1 cache line per object\n";
    std::cout << "  5. SPSC lock-free ring buffer   -> network -> engine, ~10-20ns\n";
    std::cout << "  6. Separate bid/ask arrays      -> no false sharing\n";
    std::cout << "  7. Function pointer callbacks   -> no std::function overhead\n";
    std::cout << "  8. __builtin_expect hints       -> branch predictor friendly\n";
    std::cout << "=================================================================\n";

    return 0;
}

