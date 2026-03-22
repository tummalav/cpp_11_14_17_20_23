/**
 * ============================================================================
 * ULTRA LOW LATENCY ORDER BOOK — IMPROVED PRODUCTION QUALITY
 * ============================================================================
 *
 * DATA STRUCTURES:
 *   Buy  Side : flat array[price_index] → PriceLevel   O(1) lookup
 *   Sell Side : flat array[price_index] → PriceLevel   O(1) lookup
 *   Order Map : direct array[order_id]  → Order*       O(1) lookup
 *   Per Level : intrusive doubly-linked list of Orders  O(1) add/remove
 *   Best BBO  : cached integer indices                  O(1) access
 *
 * WHY ARRAY FOR PRICE LEVELS (not btree / hash map)?
 *   index = (price - min_price) / tick_size  — one integer divide
 *   No hash, no tree traversal → direct memory slot
 *   Adjacent price levels are adjacent in memory → CPU prefetch works
 *
 * LATENCY TARGETS (RHEL8, -O3 -march=native):
 *   Add Order       : 20– 50ns
 *   Cancel Order    : 15– 35ns
 *   Modify Order    : 10– 25ns
 *   Best Bid/Ask    :  1–  5ns  (cached integer)
 *   Market Depth    : 10– 30ns
 *   SPSC enqueue    :  5– 15ns
 *   SPSC dequeue    :  5– 15ns
 *
 * BUILD:
 *   g++ -std=c++20 -O3 -march=native -flto -DNDEBUG \
 *       ultra_low_latency_orderbook_improved.cpp -o ull_ob -pthread
 * ============================================================================
 */

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#ifdef __GNUC__
#  define LIKELY(x)     __builtin_expect(!!(x), 1)
#  define UNLIKELY(x)   __builtin_expect(!!(x), 0)
#  define FORCE_INLINE  __attribute__((always_inline)) inline
#  define PREFETCH(p)   __builtin_prefetch((p), 0, 3)
#else
#  define LIKELY(x)     (x)
#  define UNLIKELY(x)   (x)
#  define FORCE_INLINE  inline
#  define PREFETCH(p)
#endif

static constexpr size_t CACHE_LINE = 64;

// ── Primitive types ──────────────────────────────────────────────────────────
using OrderId   = uint64_t;
using Price     = int64_t;    // fixed-point: actual * PRICE_SCALE
using Quantity  = uint64_t;
using Timestamp = uint64_t;

static constexpr Price   PRICE_SCALE   = 10000;  // 4 decimal places
static constexpr Price   INVALID_PRICE = -1;
static constexpr OrderId INVALID_ID    = 0;

enum class Side       : uint8_t { BUY = 0, SELL = 1 };
enum class OrderType  : uint8_t { LIMIT = 0, MARKET = 1, IOC = 2, FOK = 3 };
enum class OrderStatus: uint8_t { NEW, PARTIAL, FILLED, CANCELLED };

FORCE_INLINE uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

// ============================================================================
// ORDER  (exactly 1 cache line = 64 bytes)
// ============================================================================
/**
 *  Layout (64 bytes):
 *  ┌──────────────────────────────────────────────────────────────────┐
 *  │ order_id(8)│price(8)│qty(8)│orig_qty(8)│ts(8)│next*(8)│prev*(8)│
 *  │ side(1)│type(1)│status(1)│pad(5)                                │
 *  └──────────────────────────────────────────────────────────────────┘
 */
struct alignas(CACHE_LINE) Order {
    OrderId     order_id;
    Price       price;
    Quantity    quantity;
    Quantity    orig_quantity;
    Timestamp   timestamp;
    Order*      next;         // next in same price level (FIFO)
    Order*      prev;         // prev in same price level
    Side        side;
    OrderType   type;
    OrderStatus status;
    uint8_t     pad[5];

    void reset() {
        order_id = INVALID_ID; price = INVALID_PRICE;
        quantity = orig_quantity = timestamp = 0;
        next = prev = nullptr;
        side = Side::BUY; type = OrderType::LIMIT;
        status = OrderStatus::NEW;
        std::memset(pad, 0, 5);
    }
};
static_assert(sizeof(Order) == CACHE_LINE, "Order must be 64 bytes");

// ============================================================================
// PRICE LEVEL  (exactly 1 cache line = 64 bytes)
// ============================================================================
/**
 *  Doubly-linked FIFO list of Orders at one price.
 *  O(1) enqueue at tail, O(1) dequeue anywhere.
 */
struct alignas(CACHE_LINE) PriceLevel {
    Price    price;
    Quantity total_quantity;
    uint32_t order_count;
    uint32_t _pad0;
    Order*   head;
    Order*   tail;
    uint8_t  pad[CACHE_LINE - sizeof(Price) - sizeof(Quantity)
                 - 2*sizeof(uint32_t) - 2*sizeof(Order*)];

    void reset(Price p) {
        price = p; total_quantity = 0;
        order_count = 0; _pad0 = 0;
        head = tail = nullptr;
    }

    FORCE_INLINE void enqueue(Order* o) {   // add at tail (FIFO)
        o->next = nullptr; o->prev = tail;
        if (LIKELY(tail)) tail->next = o; else head = o;
        tail = o;
        total_quantity += o->quantity;
        ++order_count;
    }

    FORCE_INLINE void dequeue(Order* o) {   // remove anywhere O(1)
        if (o->prev) o->prev->next = o->next; else head = o->next;
        if (o->next) o->next->prev = o->prev; else tail = o->prev;
        o->next = o->prev = nullptr;
        total_quantity -= o->quantity;
        --order_count;
    }

    FORCE_INLINE bool empty() const { return order_count == 0; }
};
static_assert(sizeof(PriceLevel) == CACHE_LINE, "PriceLevel must be 64 bytes");

// ============================================================================
// OBJECT POOL  (zero malloc on hot path)
// ============================================================================
/**
 *  Pre-allocates N objects in a flat array.
 *  free_list_ = stack of pointers → O(1) alloc/dealloc.
 *
 *  [obj_0][obj_1]...[obj_N-1]   ← contiguous pool
 *  [ptr_0][ptr_1]...[ptr_N-1]   ← free list stack
 */
template<typename T, size_t N>
class alignas(CACHE_LINE) ObjectPool {
    T                 pool_[N];
    std::array<T*,N>  free_list_;
    size_t            free_count_;
public:
    ObjectPool() : free_count_(N) {
        for (size_t i = 0; i < N; ++i) free_list_[i] = &pool_[i];
    }
    FORCE_INLINE T*   allocate()       noexcept { return UNLIKELY(free_count_==0)?nullptr:free_list_[--free_count_]; }
    FORCE_INLINE void deallocate(T* p) noexcept { assert(free_count_<N); free_list_[free_count_++]=p; }
    size_t available() const { return free_count_; }
    size_t capacity()  const { return N; }
};

// ============================================================================
// SPSC LOCK-FREE RING BUFFER QUEUE
// ============================================================================
/**
 *  Single Producer Single Consumer — no CAS, just atomic load/store.
 *  Producer and consumer indices on separate cache lines → no false sharing.
 *
 *  Pipeline: [Gateway Thread] → [SPSCQueue] → [OrderBook Thread]
 *  Latency:  ~5–15ns per operation
 */
template<typename T, size_t N>
class SPSCQueue {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
    static constexpr size_t MASK = N - 1;

    alignas(CACHE_LINE) std::atomic<size_t> write_{0};
    alignas(CACHE_LINE) size_t              wr_cache_rd_{0};  // producer-side cache
    alignas(CACHE_LINE) std::atomic<size_t> read_{0};
    alignas(CACHE_LINE) size_t              rd_cache_wr_{0};  // consumer-side cache
    alignas(CACHE_LINE) T                   buf_[N];

public:
    bool push(const T& item) noexcept {
        const size_t w    = write_.load(std::memory_order_relaxed);
        const size_t next = (w + 1) & MASK;
        if (UNLIKELY(next == wr_cache_rd_)) {
            wr_cache_rd_ = read_.load(std::memory_order_acquire);
            if (UNLIKELY(next == wr_cache_rd_)) return false;
        }
        buf_[w] = item;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) noexcept {
        const size_t r = read_.load(std::memory_order_relaxed);
        if (UNLIKELY(r == rd_cache_wr_)) {
            rd_cache_wr_ = write_.load(std::memory_order_acquire);
            if (UNLIKELY(r == rd_cache_wr_)) return false;
        }
        item = buf_[r & MASK];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }

    bool   empty()    const { return write_.load() == read_.load(); }
    size_t size()     const { return write_.load() - read_.load(); }
    size_t capacity() const { return N; }
};

// ============================================================================
// TRADE / EXECUTION REPORT
// ============================================================================
struct Trade {
    uint64_t  trade_id;
    OrderId   aggressor_id;
    OrderId   passive_id;
    Price     trade_price;
    Quantity  trade_qty;
    Side      aggressor_side;
    Timestamp timestamp;
};

// ============================================================================
// CALLBACKS
// ============================================================================
struct BookCallbacks {
    std::function<void(const Order&)>  on_add;
    std::function<void(const Order&)>  on_cancel;
    std::function<void(const Trade&)>  on_trade;
    std::function<void(Price,Price,Quantity,Quantity)> on_bbo;  // bid,ask,bqty,aqty
};

// ============================================================================
// ARRAY ORDER BOOK  (O(1) everything)
// ============================================================================
/**
 *  Price → Index mapping:
 *    idx = (price - min_price) / tick_size      ← one integer divide
 *
 *  BUY  side: bid_levels_[idx]  best_bid_idx_ = highest occupied idx
 *  SELL side: ask_levels_[idx]  best_ask_idx_ = lowest  occupied idx
 *
 *  Template params:
 *    MAX_ORDERS = max simultaneous active orders
 *    MAX_LEVELS = total price ticks in range
 */
template<size_t MAX_ORDERS = 500'000, size_t MAX_LEVELS = 20'000>
class alignas(CACHE_LINE) ArrayOrderBook {
public:
    // ── Config ────────────────────────────────────────────────────────────
    Price    tick_size_;
    Price    min_price_;
    uint32_t num_ticks_;

    // ── Price level arrays (BUY/SELL separate → no cache line sharing) ────
    alignas(CACHE_LINE) std::array<PriceLevel, MAX_LEVELS> bid_;
    alignas(CACHE_LINE) std::array<PriceLevel, MAX_LEVELS> ask_;

    // ── Order lookup: O(1) by order_id ────────────────────────────────────
    alignas(CACHE_LINE) std::array<Order*, MAX_ORDERS> order_map_;

    // ── Memory pool ───────────────────────────────────────────────────────
    ObjectPool<Order, MAX_ORDERS> pool_;

    // ── BBO cache (hottest data) ──────────────────────────────────────────
    alignas(CACHE_LINE) int32_t best_bid_idx_;
    alignas(CACHE_LINE) int32_t best_ask_idx_;

    // ── Stats ─────────────────────────────────────────────────────────────
    uint64_t n_add_ = 0, n_cancel_ = 0, n_modify_ = 0, n_trade_ = 0;
    uint64_t next_trade_id_ = 1;

    BookCallbacks cb_;

public:
    ArrayOrderBook(Price min_price, Price max_price, Price tick_size = 1)
        : tick_size_(tick_size), min_price_(min_price)
        , num_ticks_(static_cast<uint32_t>((max_price - min_price) / tick_size + 1))
        , best_bid_idx_(-1)
        , best_ask_idx_(static_cast<int32_t>((max_price - min_price) / tick_size + 1))
    {
        assert(num_ticks_ <= MAX_LEVELS);
        order_map_.fill(nullptr);
        for (uint32_t i = 0; i < num_ticks_; ++i) {
            bid_[i].reset(min_price_ + Price(i) * tick_size_);
            ask_[i].reset(min_price_ + Price(i) * tick_size_);
        }
    }

    void set_callbacks(BookCallbacks cb) { cb_ = std::move(cb); }

    // ── Index helpers ─────────────────────────────────────────────────────
    FORCE_INLINE int32_t to_idx(Price p) const { return (int32_t)((p - min_price_) / tick_size_); }
    FORCE_INLINE bool    valid(int32_t i) const { return i >= 0 && i < (int32_t)num_ticks_; }

    // =========================================================================
    // ADD ORDER
    // =========================================================================
    bool add_order(OrderId id, Side side, Price price, Quantity qty,
                   OrderType type = OrderType::LIMIT) {
        if (UNLIKELY(!id || id >= MAX_ORDERS || !qty)) return false;
        int32_t idx = to_idx(price);
        if (UNLIKELY(!valid(idx))) return false;

        Order* o = pool_.allocate();
        if (UNLIKELY(!o)) return false;

        o->order_id = id; o->price = price; o->quantity = qty;
        o->orig_quantity = qty; o->timestamp = now_ns();
        o->side = side; o->type = type; o->status = OrderStatus::NEW;
        o->next = o->prev = nullptr;

        order_map_[id] = o;

        // Market order → match immediately, no resting
        if (type == OrderType::MARKET) {
            match_market(o);
            if (o->quantity > 0) { order_map_[id] = nullptr; pool_.deallocate(o); }
            return true;
        }

        // Try aggressive match (limit crossing spread)
        Quantity filled = try_match(o);
        if (o->quantity == 0) return true;

        if (type == OrderType::IOC) { order_map_[id] = nullptr; pool_.deallocate(o); return true; }
        if (type == OrderType::FOK && filled < o->orig_quantity) {
            order_map_[id] = nullptr; pool_.deallocate(o); return true;
        }

        // Rest at price level
        auto& levels = (side == Side::BUY) ? bid_ : ask_;
        levels[idx].enqueue(o);

        // Update BBO cache
        if (side == Side::BUY  && idx > best_bid_idx_) { best_bid_idx_ = idx; fire_bbo(); }
        if (side == Side::SELL && idx < best_ask_idx_) { best_ask_idx_ = idx; fire_bbo(); }

        ++n_add_;
        if (cb_.on_add) cb_.on_add(*o);
        return true;
    }

    // =========================================================================
    // CANCEL ORDER
    // =========================================================================
    bool cancel_order(OrderId id) {
        if (UNLIKELY(id >= MAX_ORDERS)) return false;
        Order* o = order_map_[id];
        if (UNLIKELY(!o)) return false;

        int32_t idx  = to_idx(o->price);
        Side    side = o->side;
        auto&   levels = (side == Side::BUY) ? bid_ : ask_;

        levels[idx].dequeue(o);
        if (levels[idx].empty()) update_bbo_after_remove(side, idx);

        o->status = OrderStatus::CANCELLED;
        if (cb_.on_cancel) cb_.on_cancel(*o);

        order_map_[id] = nullptr;
        pool_.deallocate(o);
        ++n_cancel_;
        return true;
    }

    // =========================================================================
    // MODIFY ORDER  (quantity only)
    // =========================================================================
    bool modify_order(OrderId id, Quantity new_qty) {
        if (UNLIKELY(id >= MAX_ORDERS)) return false;
        Order* o = order_map_[id];
        if (UNLIKELY(!o)) return false;
        if (new_qty == 0) return cancel_order(id);

        int32_t idx = to_idx(o->price);
        auto& levels = (o->side == Side::BUY) ? bid_ : ask_;
        levels[idx].total_quantity = levels[idx].total_quantity - o->quantity + new_qty;
        o->quantity = new_qty;
        ++n_modify_;
        return true;
    }

    // =========================================================================
    // BBO ACCESS  (< 5ns — just read cached indices)
    // =========================================================================
    FORCE_INLINE Price    best_bid()     const { return best_bid_idx_ >= 0 ? bid_[best_bid_idx_].price : INVALID_PRICE; }
    FORCE_INLINE Price    best_ask()     const { return best_ask_idx_ < (int32_t)num_ticks_ ? ask_[best_ask_idx_].price : INVALID_PRICE; }
    FORCE_INLINE Quantity best_bid_qty() const { return best_bid_idx_ >= 0 ? bid_[best_bid_idx_].total_quantity : 0; }
    FORCE_INLINE Quantity best_ask_qty() const { return best_ask_idx_ < (int32_t)num_ticks_ ? ask_[best_ask_idx_].total_quantity : 0; }

    FORCE_INLINE Price spread()    const {
        Price b=best_bid(), a=best_ask();
        return (b!=INVALID_PRICE && a!=INVALID_PRICE) ? a-b : INVALID_PRICE;
    }
    FORCE_INLINE Price mid_price() const {
        Price b=best_bid(), a=best_ask();
        return (b!=INVALID_PRICE && a!=INVALID_PRICE) ? (b+a)/2 : INVALID_PRICE;
    }

    // =========================================================================
    // MARKET DEPTH
    // =========================================================================
    struct DepthLevel { Price price; Quantity qty; uint32_t count; };

    void get_bid_depth(std::vector<DepthLevel>& out, size_t n=10) const {
        out.clear();
        for (int32_t i=best_bid_idx_; i>=0 && out.size()<n; --i)
            if (!bid_[i].empty()) out.push_back({bid_[i].price, bid_[i].total_quantity, bid_[i].order_count});
    }
    void get_ask_depth(std::vector<DepthLevel>& out, size_t n=10) const {
        out.clear();
        int32_t max=(int32_t)num_ticks_;
        for (int32_t i=best_ask_idx_; i<max && (int32_t)out.size()<(int32_t)n; ++i)
            if (!ask_[i].empty()) out.push_back({ask_[i].price, ask_[i].total_quantity, ask_[i].order_count});
    }

    // =========================================================================
    // VWAP from current book depth
    // =========================================================================
    double vwap(Side side, size_t depth=5) const {
        double tv=0.0; uint64_t tq=0; size_t cnt=0;
        if (side == Side::BUY) {
            for (int32_t i=best_bid_idx_; i>=0 && cnt<depth; --i)
                if (!bid_[i].empty()) { tv+=double(bid_[i].price)*bid_[i].total_quantity; tq+=bid_[i].total_quantity; ++cnt; }
        } else {
            int32_t max=(int32_t)num_ticks_;
            for (int32_t i=best_ask_idx_; i<max && cnt<depth; ++i)
                if (!ask_[i].empty()) { tv+=double(ask_[i].price)*ask_[i].total_quantity; tq+=ask_[i].total_quantity; ++cnt; }
        }
        return tq>0 ? tv/tq/PRICE_SCALE : 0.0;
    }

    // =========================================================================
    // DISPLAY
    // =========================================================================
    void print(size_t depth=5) const {
        std::vector<DepthLevel> bids, asks;
        get_bid_depth(bids, depth); get_ask_depth(asks, depth);

        std::cout << "\n╔═══════════════════════════════════════════╗\n";
        std::cout <<   "║           ORDER BOOK SNAPSHOT             ║\n";
        std::cout <<   "╠═══════════════════════════════════════════╣\n";
        for (auto it=asks.rbegin(); it!=asks.rend(); ++it)
            std::cout << "║  ASK  " << std::setw(10) << std::fixed << std::setprecision(4)
                      << double(it->price)/PRICE_SCALE << "  x " << std::setw(8) << it->qty
                      << "  (" << it->count << " orders)  ║\n";
        Price sp=spread();
        std::cout << "║─── spread=" << std::setw(8) << (sp!=INVALID_PRICE?double(sp)/PRICE_SCALE:0.0)
                  << "  mid=" << std::setw(10) << (mid_price()!=INVALID_PRICE?double(mid_price())/PRICE_SCALE:0.0) << " ───║\n";
        for (auto& b: bids)
            std::cout << "║  BID  " << std::setw(10) << std::fixed << std::setprecision(4)
                      << double(b.price)/PRICE_SCALE << "  x " << std::setw(8) << b.qty
                      << "  (" << b.count << " orders)  ║\n";
        std::cout << "╚═══════════════════════════════════════════╝\n";
    }

    void print_stats() const {
        std::cout << "\n=== Stats: add=" << n_add_ << " cancel=" << n_cancel_
                  << " modify=" << n_modify_ << " trades=" << n_trade_
                  << " pool=" << pool_.available() << "/" << MAX_ORDERS << " ===\n";
    }

private:
    // ── Matching engine ───────────────────────────────────────────────────

    Quantity try_match(Order* inc) {
        Quantity filled = 0;
        if (inc->side == Side::BUY) {
            while (inc->quantity > 0 && best_ask_idx_ < (int32_t)num_ticks_) {
                PriceLevel& lvl = ask_[best_ask_idx_];
                if (lvl.empty())               { ++best_ask_idx_; continue; }
                if (inc->price < lvl.price)    break;
                filled += match_level(inc, lvl);
                if (lvl.empty()) ++best_ask_idx_;
            }
        } else {
            while (inc->quantity > 0 && best_bid_idx_ >= 0) {
                PriceLevel& lvl = bid_[best_bid_idx_];
                if (lvl.empty())               { --best_bid_idx_; continue; }
                if (inc->price > lvl.price)    break;
                filled += match_level(inc, lvl);
                if (lvl.empty()) --best_bid_idx_;
            }
        }
        if (filled) fire_bbo();
        return filled;
    }

    void match_market(Order* inc) {
        if (inc->side == Side::BUY) {
            while (inc->quantity>0 && best_ask_idx_<(int32_t)num_ticks_) {
                PriceLevel& lvl=ask_[best_ask_idx_];
                if (lvl.empty()){++best_ask_idx_;continue;}
                match_level(inc,lvl); if(lvl.empty())++best_ask_idx_;
            }
        } else {
            while (inc->quantity>0 && best_bid_idx_>=0) {
                PriceLevel& lvl=bid_[best_bid_idx_];
                if (lvl.empty()){--best_bid_idx_;continue;}
                match_level(inc,lvl); if(lvl.empty())--best_bid_idx_;
            }
        }
    }

    Quantity match_level(Order* inc, PriceLevel& lvl) {
        Quantity filled = 0;
        for (Order* pass = lvl.head; pass && inc->quantity > 0; ) {
            Quantity qty = std::min(inc->quantity, pass->quantity);
            Trade t{ next_trade_id_++, inc->order_id, pass->order_id,
                     pass->price, qty, inc->side, now_ns() };
            inc->quantity -= qty; pass->quantity -= qty;
            lvl.total_quantity -= qty; filled += qty; ++n_trade_;
            if (cb_.on_trade) cb_.on_trade(t);
            Order* nxt = pass->next;
            if (pass->quantity == 0) {
                pass->status = OrderStatus::FILLED;
                lvl.dequeue(pass);
                order_map_[pass->order_id] = nullptr;
                pool_.deallocate(pass);
            } else { pass->status = OrderStatus::PARTIAL; }
            if (inc->quantity==0) inc->status=OrderStatus::FILLED;
            else inc->status=OrderStatus::PARTIAL;
            pass = nxt;
        }
        return filled;
    }

    FORCE_INLINE void update_bbo_after_remove(Side side, int32_t removed) {
        if (side == Side::BUY && removed == best_bid_idx_) {
            int32_t i = best_bid_idx_-1;
            while (i>=0 && bid_[i].empty()) --i;
            best_bid_idx_ = i; fire_bbo();
        } else if (side == Side::SELL && removed == best_ask_idx_) {
            int32_t i = best_ask_idx_+1, max=(int32_t)num_ticks_;
            while (i<max && ask_[i].empty()) ++i;
            best_ask_idx_ = i; fire_bbo();
        }
    }

    FORCE_INLINE void fire_bbo() {
        if (cb_.on_bbo) cb_.on_bbo(best_bid(), best_ask(), best_bid_qty(), best_ask_qty());
    }
};

// ============================================================================
// ORDER GATEWAY PIPELINE  (SPSC-based)
// ============================================================================
struct OrderReq {
    enum class T : uint8_t { ADD, CANCEL, MODIFY } type;
    OrderId   id;
    Side      side;
    OrderType otype;
    Price     price;
    Quantity  qty;
};

template<typename Book>
class GatewayPipeline {
    SPSCQueue<OrderReq, 65536> q_;
    Book&                      book_;
    std::atomic<bool>          running_{false};
    std::thread                worker_;
    uint64_t                   processed_{0};
public:
    explicit GatewayPipeline(Book& b) : book_(b) {}
    ~GatewayPipeline() { stop(); }

    bool submit_add   (OrderId id, Side s, Price p, Quantity q, OrderType t=OrderType::LIMIT)
                       { return q_.push({OrderReq::T::ADD, id, s, t, p, q}); }
    bool submit_cancel(OrderId id) { return q_.push({OrderReq::T::CANCEL,id,Side::BUY,OrderType::LIMIT,0,0}); }
    bool submit_modify(OrderId id, Quantity nq) { return q_.push({OrderReq::T::MODIFY,id,Side::BUY,OrderType::LIMIT,0,nq}); }

    void start() {
        running_ = true;
        worker_ = std::thread([this]{ drain_loop(); });
    }
    void stop() { running_=false; if(worker_.joinable()) worker_.join(); }
    uint64_t processed() const { return processed_; }

private:
    void drain_loop() {
        OrderReq r;
        while (running_.load(std::memory_order_relaxed)) {
            if (q_.pop(r)) { dispatch(r); ++processed_; }
        }
        while (q_.pop(r)) { dispatch(r); ++processed_; }
    }
    void dispatch(const OrderReq& r) {
        switch (r.type) {
        case OrderReq::T::ADD:    book_.add_order(r.id, r.side, r.price, r.qty, r.otype); break;
        case OrderReq::T::CANCEL: book_.cancel_order(r.id); break;
        case OrderReq::T::MODIFY: book_.modify_order(r.id, r.qty); break;
        }
    }
};

// ============================================================================
// LATENCY STATS
// ============================================================================
struct LatencyStats {
    std::vector<uint64_t> s;
    void record(uint64_t ns) { s.push_back(ns); }
    void print(const char* name) const {
        if (s.empty()) return;
        auto v = s; std::sort(v.begin(), v.end());
        auto pct=[&](double p){ return v[size_t(v.size()*p)]; };
        uint64_t sum=0; for(auto x:v) sum+=x;
        std::cout << std::left << std::setw(32) << name
                  << " avg=" << std::setw(5) << sum/v.size() << "ns"
                  << " p50=" << std::setw(5) << pct(.50) << "ns"
                  << " p95=" << std::setw(5) << pct(.95) << "ns"
                  << " p99=" << std::setw(5) << pct(.99) << "ns"
                  << " min=" << v.front() << " max=" << v.back() << "ns\n";
    }
};

// ============================================================================
// FUNCTIONAL TEST
// ============================================================================
void functional_test() {
    std::cout << "\n╔══════════════════════════════════╗\n"
              << "║       FUNCTIONAL TEST             ║\n"
              << "╚══════════════════════════════════╝\n";

    // Price range 99.9000–100.1000, tick=0.0001 → int range 999000–1001000
    ArrayOrderBook<10'000, 3000> book(999000, 1001000, 1);

    book.set_callbacks({
        .on_add    = [](const Order& o){
            std::cout << "[ADD]    id=" << o.order_id
                      << " " << (o.side==Side::BUY?"BUY":"SELL")
                      << " px=" << std::fixed << std::setprecision(4)
                      << double(o.price)/PRICE_SCALE
                      << " qty=" << o.quantity << "\n"; },
        .on_cancel = [](const Order& o){ std::cout << "[CANCEL] id=" << o.order_id << "\n"; },
        .on_trade  = [](const Trade& t){
            std::cout << "[TRADE]  px=" << std::fixed << std::setprecision(4)
                      << double(t.trade_price)/PRICE_SCALE
                      << " qty=" << t.trade_qty
                      << " agg=" << (t.aggressor_side==Side::BUY?"BUY":"SELL") << "\n"; },
        .on_bbo    = [](Price b, Price a, Quantity bq, Quantity aq){
            std::cout << "[BBO]    bid=" << std::fixed << std::setprecision(4)
                      << (b!=INVALID_PRICE?double(b)/PRICE_SCALE:0.0) << "x" << bq
                      << "  ask=" << (a!=INVALID_PRICE?double(a)/PRICE_SCALE:0.0) << "x" << aq << "\n"; }
    });

    std::cout << "\n--- Bids ---\n";
    book.add_order(1, Side::BUY, 999900, 100);    // 99.9900
    book.add_order(2, Side::BUY, 999950, 200);    // 99.9950
    book.add_order(3, Side::BUY, 999950, 150);    // 99.9950 same level
    book.add_order(4, Side::BUY, 1000000, 50);   // 100.0000 best bid

    std::cout << "\n--- Asks ---\n";
    book.add_order(5, Side::SELL, 1000100, 100);  // 100.0100 best ask
    book.add_order(6, Side::SELL, 1000200, 200);  // 100.0200
    book.add_order(7, Side::SELL, 1000300, 300);  // 100.0300

    book.print(4);
    std::cout << "  Bid VWAP(3): " << std::fixed << std::setprecision(4) << book.vwap(Side::BUY,3) << "\n";
    std::cout << "  Ask VWAP(3): " << std::fixed << std::setprecision(4) << book.vwap(Side::SELL,3) << "\n";

    std::cout << "\n--- Modify order 2: qty 200→500 ---\n";
    book.modify_order(2, 500);
    book.print(3);

    std::cout << "\n--- Cancel order 4 (best bid) ---\n";
    book.cancel_order(4);
    book.print(3);

    std::cout << "\n--- Aggressive BUY crosses spread ---\n";
    book.add_order(8, Side::BUY, 1000200, 150);   // matches 100@100.01, 50@100.02
    book.print(3);

    std::cout << "\n--- Market SELL ---\n";
    book.add_order(9, Side::SELL, 0, 100, OrderType::MARKET);
    book.print(3);
}

// ============================================================================
// BENCHMARKS
// ============================================================================
void benchmarks() {
    std::cout << "\n╔══════════════════════════════════╗\n"
              << "║         BENCHMARKS                ║\n"
              << "╚══════════════════════════════════╝\n\n";

    constexpr Price MID = 1000000;  // 100.0000
    ArrayOrderBook<500'000, 1'100'000> book(500000, 1500000, 1);

    const size_t WARM  = 5'000;
    const size_t N     = 200'000;

    // warmup
    for (size_t i=1; i<=WARM; ++i) book.add_order(i, (i%2?Side::BUY:Side::SELL), MID+(i%2?-(Price)(i%200):(Price)(i%200)), 100);
    for (size_t i=1; i<=WARM; ++i) book.cancel_order(i);

    // ADD
    { LatencyStats st;
      for (size_t i=1; i<=N; ++i) {
          Side s=(i%2?Side::BUY:Side::SELL);
          Price p=s==Side::BUY?MID-(Price)(i%500):MID+(Price)(i%500);
          auto t1=now_ns(); book.add_order(i+WARM,s,p,100); st.record(now_ns()-t1);
      }
      st.print("ADD LIMIT ORDER"); }

    // BBO
    { LatencyStats st;
      for (size_t i=0; i<1'000'000; ++i) {
          auto t1=now_ns();
          volatile Price b=book.best_bid(); volatile Price a=book.best_ask();
          volatile Quantity bq=book.best_bid_qty(); volatile Quantity aq=book.best_ask_qty();
          st.record(now_ns()-t1); (void)b;(void)a;(void)bq;(void)aq;
      }
      st.print("BEST BID/ASK"); }

    // SPREAD + MID
    { LatencyStats st;
      for (size_t i=0; i<1'000'000; ++i) {
          auto t1=now_ns();
          volatile Price sp=book.spread(); volatile Price mid=book.mid_price();
          st.record(now_ns()-t1); (void)sp;(void)mid;
      }
      st.print("SPREAD+MID"); }

    // MODIFY
    { LatencyStats st;
      for (size_t i=1; i<=N/2; ++i) { auto t1=now_ns(); book.modify_order(i+WARM,200+i%800); st.record(now_ns()-t1); }
      st.print("MODIFY ORDER"); }

    // CANCEL
    { LatencyStats st;
      for (size_t i=1; i<=N; ++i) { auto t1=now_ns(); book.cancel_order(i+WARM); st.record(now_ns()-t1); }
      st.print("CANCEL ORDER"); }

    // DEPTH
    { // refill
      for (size_t i=1; i<=2000; ++i) {
          book.add_order(i, Side::BUY,  MID-(Price)(i%100), 100);
          book.add_order(i+2000, Side::SELL, MID+(Price)(i%100), 100);
      }
      LatencyStats st;
      std::vector<ArrayOrderBook<500'000,1'100'000>::DepthLevel> b,a;
      for (size_t i=0;i<100'000;++i){ auto t1=now_ns(); book.get_bid_depth(b,10); book.get_ask_depth(a,10); st.record(now_ns()-t1); }
      st.print("MARKET DEPTH L2 (10x2)"); }

    // SPSC
    { SPSCQueue<OrderReq,65536> q;
      LatencyStats ps, cs;
      for (size_t i=0;i<500'000;++i){
          OrderReq r{OrderReq::T::ADD,i+1,Side::BUY,OrderType::LIMIT,MID,100};
          auto t1=now_ns(); q.push(r); ps.record(now_ns()-t1);
          OrderReq out; auto t2=now_ns(); q.pop(out); cs.record(now_ns()-t2);
      }
      ps.print("SPSC push"); cs.print("SPSC pop"); }

    std::cout << "\n┌──────────────────────────────────────────────────────────┐\n";
    std::cout <<   "│  EXPECTED TARGETS (RHEL8, -O3 -march=native)             │\n";
    std::cout <<   "├─────────────────────────┬──────────┬────────────────────┤\n";
    std::cout <<   "│ Operation               │ Target   │ Why                │\n";
    std::cout <<   "├─────────────────────────┼──────────┼────────────────────┤\n";
    std::cout <<   "│ Add Limit Order         │ 20-50ns  │ pool+array index   │\n";
    std::cout <<   "│ Cancel Order            │ 15-35ns  │ O(1) doubly-linked │\n";
    std::cout <<   "│ Modify Order            │ 10-25ns  │ in-place update    │\n";
    std::cout <<   "│ Best Bid/Ask            │ 1-5ns    │ cached int index   │\n";
    std::cout <<   "│ Spread / Mid            │ 1-5ns    │ 2 integer reads    │\n";
    std::cout <<   "│ Market Depth (10 lvls)  │ 10-30ns  │ array scan         │\n";
    std::cout <<   "│ SPSC push               │ 5-15ns   │ no CAS, release    │\n";
    std::cout <<   "│ SPSC pop                │ 5-15ns   │ no CAS, acquire    │\n";
    std::cout <<   "└─────────────────────────┴──────────┴────────────────────┘\n";
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║   ULTRA LOW LATENCY ORDER BOOK — IMPROVED            ║\n";
    std::cout << "║   C++20  |  Array-based O(1)  |  Object Pool         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";
    std::cout << "\n  sizeof(Order)      = " << sizeof(Order)      << " bytes (target 64)\n";
    std::cout <<   "  sizeof(PriceLevel) = " << sizeof(PriceLevel) << " bytes (target 64)\n";
    std::cout <<   "  CPU cores: " << std::thread::hardware_concurrency() << "\n";

    functional_test();
    benchmarks();
    return 0;
}

/*
 * ═══════════════════════════════════════════════════════════════════
 *  WHY THESE DESIGN CHOICES?
 * ═══════════════════════════════════════════════════════════════════
 *
 *  1. ARRAY FOR PRICE LEVELS (not btree/hash map)
 *     idx = (price - min_price) / tick_size → O(1), 1 integer divide
 *     Adjacent prices are adjacent in memory → CPU prefetch works
 *     vs btree: 30-120ns → array: 1-5ns
 *
 *  2. INTEGER FIXED-POINT PRICES
 *     Float == is unreliable, comparison = 1 cycle integer
 *     Eliminates FP rounding in matching engine
 *
 *  3. DOUBLY-LINKED INTRUSIVE LIST PER PRICE LEVEL
 *     O(1) cancel anywhere (pointer inside Order itself)
 *     FIFO time priority preserved
 *     No extra heap allocation (pointers live inside Order)
 *
 *  4. OBJECT POOL
 *     malloc/free = 50-500ns (kernel path)
 *     Pool alloc  = 1-5ns   (array pop)
 *     All objects contiguous → CPU cache reuse
 *
 *  5. CACHE-LINE ALIGNED structs
 *     Order = 64 bytes = 1 cache line
 *     PriceLevel = 64 bytes = 1 cache line
 *     No false sharing between adjacent objects
 *
 *  6. SPSC QUEUE (no CAS, no mutex)
 *     Pure atomic load/store on producer and consumer side
 *     Separate cache lines for write/read indices
 *     5-15ns vs 200-500ns mutex-based queue
 *
 *  7. CACHED BBO INDICES
 *     Most accessed datum in any trading system
 *     1-5ns: just read one integer, no memory traversal
 * ═══════════════════════════════════════════════════════════════════
 */

