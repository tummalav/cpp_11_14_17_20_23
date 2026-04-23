#pragma once
/**
 * nasdaq_itch_feed_handler.hpp  —  NASDAQ ITCH 5.0 Ultra-Low Latency Feed Handler
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  DESIGN: Why CRTP instead of virtual interfaces?
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  Virtual dispatch cost (per call):
 *    - vtable pointer load  : 1 cache miss  (~4-65 ns)
 *    - indirect call        : branch miss   (~5-15 ns)
 *    - icache pollution     : unpredictable
 *    Total                  : 10-80 ns per event callback
 *
 *  NASDAQ ITCH at peak: 1-5 million messages/second
 *    → 1M msgs/sec × 70ns virtual overhead = 70 ms wasted per second (7% CPU!)
 *
 *  CRTP cost:
 *    - Direct inlined call  : 0 ns (compiler inlines the handler body)
 *    - Zero vtable          : no pointer indirection
 *    - Devirtualized by compiler at compile time
 *
 *  Other ULL fixes vs original:
 *    ✗ BEFORE: std::vector<uint8_t> in SPSC queue → heap alloc per message
 *    ✓ AFTER : RawPacket (fixed 2KB stack-allocated array) in queue
 *
 *    ✗ BEFORE: std::string stock in OrderInfo    → heap alloc per order
 *    ✓ AFTER : SymbolKey (packed 8-byte integer) — zero allocation
 *
 *    ✗ BEFORE: std::unordered_map<uint64_t, OrderInfo> with std::mutex
 *    ✓ AFTER : OpenAddressingOrderMap — lock-free open addressing hash map
 *
 *    ✗ BEFORE: std::vector<PriceLevel> order book with std::sort
 *    ✓ AFTER : std::array<PriceLevel, MAX_LEVELS> with insertion sort (O(N), N≤20)
 *
 *    ✗ BEFORE: std::mutex in every hot path function
 *    ✓ AFTER : Single-writer single-reader per data structure, no mutex on hot path
 *
 *    ✗ BEFORE: high_resolution_clock::now() — 20-50 ns syscall
 *    ✓ AFTER : rdtsc() — 3-7 ns, zero syscall
 *
 *    ✗ BEFORE: std::shared_ptr<IITCHEventHandler> — atomic refcount + vtable
 *    ✓ AFTER : CRTP direct dispatch — zero overhead
 *
 *    ✗ BEFORE: std::this_thread::sleep_for(50ns) in spin loops — OS jitter
 *    ✓ AFTER : CPU_PAUSE() busy-wait — deterministic sub-100ns
 *
 *    ✗ BEFORE: extractStock() builds std::string in hot path
 *    ✓ AFTER : pack_symbol() packs 8 chars into uint64_t — zero allocation
 *
 * Compile:
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG nasdaq_itch_feed_handler.cpp \
 *       -lpthread -o itch_handler
 */

#include <cstdint>
#include <cstring>
#include <cassert>
#include <array>
#include <atomic>
#include <thread>
#include <functional>
#include <sys/socket.h>

// ============================================================================
// PLATFORM UTILITIES
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #define CPU_PAUSE()  _mm_pause()
    inline uint64_t rdtsc_now() noexcept {
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }
    inline uint64_t rdtsc_start() noexcept {
        uint32_t lo, hi;
        __asm__ volatile("lfence\n\trdtsc\n\tlfence" : "=a"(lo), "=d"(hi) :: "memory");
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }
#else
    #include <chrono>
    #define CPU_PAUSE()  __asm__ volatile("yield" ::: "memory")
    inline uint64_t rdtsc_now() noexcept {
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }
    inline uint64_t rdtsc_start() noexcept { return rdtsc_now(); }
#endif

#define CACHE_LINE   64
#define CACHE_ALIGN  alignas(CACHE_LINE)
#define FORCE_INLINE __attribute__((always_inline)) inline
#define HOT_PATH     __attribute__((hot))
#define COLD_PATH    __attribute__((cold))

namespace nasdaq::itch {

// ============================================================================
// CONSTANTS
// ============================================================================

static constexpr size_t MAX_BOOK_LEVELS   = 20;     // Top-of-book levels stored
static constexpr size_t MAX_ORDERS        = 1 << 20; // 1M orders, power-of-2 for mask
static constexpr size_t MAX_SYMBOLS       = 8192;    // Max unique symbols
static constexpr size_t RAW_PKT_CAPACITY  = 2048;   // Max ITCH UDP packet size
static constexpr size_t SPSC_QUEUE_SIZE   = 1 << 15; // 32768 slots
static constexpr size_t PROC_QUEUE_SIZE   = 1 << 14; // 16384 slots

// ============================================================================
// SECTION 1: NASDAQ ITCH 5.0 MESSAGE TYPES
// ============================================================================

enum class MessageType : uint8_t {
    SYSTEM_EVENT              = 'S',
    STOCK_DIRECTORY           = 'R',
    STOCK_TRADING_ACTION      = 'H',
    REG_SHO_RESTRICTION       = 'Y',
    MARKET_PARTICIPANT_POSITION = 'L',
    MWCB_DECLINE_LEVEL        = 'V',
    MWCB_STATUS               = 'W',
    IPO_QUOTING_PERIOD_UPDATE = 'K',
    LULD_AUCTION_COLLAR       = 'J',
    ADD_ORDER                 = 'A',
    ADD_ORDER_WITH_MPID       = 'F',
    ORDER_EXECUTED            = 'E',
    ORDER_EXECUTED_WITH_PRICE = 'C',
    ORDER_CANCEL              = 'X',
    ORDER_DELETE              = 'D',
    ORDER_REPLACE             = 'U',
    TRADE_NON_CROSS           = 'P',
    TRADE_CROSS               = 'Q',
    BROKEN_TRADE              = 'B',
    NOII                      = 'I',
    RPII                      = 'N'
};

enum class Side            : uint8_t { BUY = 'B', SELL = 'S' };
enum class MarketCategory  : uint8_t {
    NASDAQ_GLOBAL_SELECT = 'Q', NASDAQ_GLOBAL_MARKET = 'G',
    NASDAQ_CAPITAL_MARKET = 'S', NYSE = 'N', NYSE_MKT = 'A',
    NYSE_ARCA = 'P', BATS_Z = 'Z', INVESTORS_EXCHANGE = 'V'
};
enum class FinancialStatus : uint8_t {
    NORMAL = ' ', DEFICIENT = 'D', DELINQUENT = 'E', BANKRUPT = 'Q',
    SUSPENDED = 'S', DEFICIENT_BANKRUPT = 'G', DEFICIENT_DELINQUENT = 'H',
    DELINQUENT_BANKRUPT = 'J', DEFICIENT_DELINQUENT_BANKRUPT = 'K'
};
enum class TradingState    : uint8_t {
    HALTED = 'H', PAUSED = 'P', QUOTATION_ONLY = 'Q', TRADING = 'T'
};

// ============================================================================
// SECTION 2: ITCH WIRE MESSAGE STRUCTURES (packed, zero-copy from wire)
// ============================================================================

struct alignas(2) ITCHMessageHeader {
    uint16_t    length;
    MessageType message_type;
    uint8_t     stock_locate;
    uint16_t    tracking_number;
    uint64_t    timestamp;   // nanoseconds since midnight (ITCH time)
} __attribute__((packed));

struct alignas(2) SystemEventMessage {
    ITCHMessageHeader header;
    uint8_t           event_code;
} __attribute__((packed));

struct alignas(2) StockDirectoryMessage {
    ITCHMessageHeader header;
    char              stock[8];
    MarketCategory    market_category;
    FinancialStatus   financial_status;
    uint32_t          round_lot_size;
    uint8_t           round_lots_only;
    uint8_t           issue_classification;
    char              issue_subtype[2];
    uint8_t           authenticity;
    uint8_t           short_sale_threshold;
    uint8_t           ipo_flag;
    uint8_t           luld_reference_price_tier;
    uint8_t           etp_flag;
    uint32_t          etp_leverage_factor;
    uint8_t           inverse_indicator;
} __attribute__((packed));

struct alignas(2) StockTradingActionMessage {
    ITCHMessageHeader header;
    char              stock[8];
    TradingState      trading_state;
    char              reason[4];
} __attribute__((packed));

struct alignas(2) AddOrderMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
    Side              buy_sell_indicator;
    uint32_t          shares;
    char              stock[8];
    uint32_t          price;   // in 1/10000 dollars
} __attribute__((packed));

struct alignas(2) AddOrderWithMPIDMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
    Side              buy_sell_indicator;
    uint32_t          shares;
    char              stock[8];
    uint32_t          price;
    char              attribution[4];
} __attribute__((packed));

struct alignas(2) OrderExecutedMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
    uint32_t          executed_shares;
    uint64_t          match_number;
} __attribute__((packed));

struct alignas(2) OrderExecutedWithPriceMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
    uint32_t          executed_shares;
    uint64_t          match_number;
    uint8_t           printable;
    uint32_t          execution_price;
} __attribute__((packed));

struct alignas(2) OrderCancelMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
    uint32_t          cancelled_shares;
} __attribute__((packed));

struct alignas(2) OrderDeleteMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
} __attribute__((packed));

struct alignas(2) OrderReplaceMessage {
    ITCHMessageHeader header;
    uint64_t          original_order_reference_number;
    uint64_t          new_order_reference_number;
    uint32_t          shares;
    uint32_t          price;
} __attribute__((packed));

struct alignas(2) TradeMessage {
    ITCHMessageHeader header;
    uint64_t          order_reference_number;
    Side              buy_sell_indicator;
    uint32_t          shares;
    char              stock[8];
    uint32_t          price;
    uint64_t          match_number;
} __attribute__((packed));

struct alignas(2) CrossTradeMessage {
    ITCHMessageHeader header;
    uint32_t          shares;
    char              stock[8];
    uint32_t          cross_price;
    uint64_t          match_number;
    uint8_t           cross_type;
} __attribute__((packed));

struct alignas(2) BrokenTradeMessage {
    ITCHMessageHeader header;
    uint64_t          match_number;
} __attribute__((packed));

struct alignas(2) NOIIMessage {
    ITCHMessageHeader header;
    uint64_t          paired_shares;
    uint64_t          imbalance_shares;
    uint8_t           imbalance_direction;
    char              stock[8];
    uint32_t          far_price;
    uint32_t          near_price;
    uint32_t          current_reference_price;
    uint8_t           cross_type;
    uint8_t           price_variation_indicator;
} __attribute__((packed));

// ============================================================================
// SECTION 3: SYMBOL KEY — pack 8 chars into uint64_t (zero heap allocation)
//
//  Instead of std::string("AAPL    ") → heap allocation + SSO threshold
//  Use:  pack_symbol("AAPL    ") → 0x4141504C20202020  (8-byte integer)
//
//  Hash map lookup: uint64_t key, not std::string
//  → O(1) with zero heap, fits in register
// ============================================================================

using SymbolKey = uint64_t;

FORCE_INLINE SymbolKey pack_symbol(const char* sym8) noexcept {
    // Pack 8-byte fixed-width ITCH symbol into a single uint64_t for zero-copy lookup
    SymbolKey k = 0;
    std::memcpy(&k, sym8, 8);
    return k;
}

// ============================================================================
// SECTION 4: RAW PACKET BUFFER — fixed-size, zero heap allocation in hot path
//
//  Old:  std::vector<uint8_t> packet → operator new() per received packet
//  New:  RawPacket — fixed 2KB array, stored directly in SPSC slot
//        No heap allocation. Copy cost = memcpy(2KB) = ~100ns (cache warm)
// ============================================================================

struct alignas(CACHE_LINE) RawPacket {
    uint64_t recv_tsc;              // rdtsc at receive (not clock_gettime!)
    uint16_t len;                   // bytes valid in data[]
    uint8_t  data[RAW_PKT_CAPACITY];

    RawPacket() noexcept : recv_tsc(0), len(0) {}
};

// ============================================================================
// SECTION 5: SPSC RING BUFFER — wait-free, no heap, cache-line separated
// ============================================================================

template<typename T, size_t Cap>
class alignas(CACHE_LINE) SPSCQueue {
    static_assert((Cap & (Cap-1)) == 0, "Cap must be power of 2");

    struct Cell {
        std::atomic<uint64_t> seq;
        T                     data;
        // Pad cell to cache line boundary to prevent false sharing between adjacent slots
        static constexpr size_t DATA_BYTES = sizeof(std::atomic<uint64_t>) + sizeof(T);
        static constexpr size_t PAD =
            (DATA_BYTES % CACHE_LINE == 0) ? 0 : (CACHE_LINE - DATA_BYTES % CACHE_LINE);
        char _pad[PAD];
    };

    CACHE_ALIGN std::atomic<uint64_t> enq_{0};
    CACHE_ALIGN std::atomic<uint64_t> deq_{0};
    CACHE_ALIGN std::array<Cell, Cap> buf_;

    static constexpr uint64_t MASK = Cap - 1;

public:
    SPSCQueue() noexcept {
        for (size_t i = 0; i < Cap; ++i)
            buf_[i].seq.store(i, std::memory_order_relaxed);
    }

    // Producer thread only
    FORCE_INLINE bool push(const T& item) noexcept {
        const uint64_t pos  = enq_.load(std::memory_order_relaxed);
        Cell&          cell = buf_[pos & MASK];
        const uint64_t seq  = cell.seq.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

        if (__builtin_expect(diff == 0, 1)) {
            cell.data = item;
            cell.seq.store(pos + 1, std::memory_order_release);
            enq_.store(pos + 1, std::memory_order_relaxed);
            return true;
        }
        return false;  // full
    }

    // Consumer thread only
    FORCE_INLINE bool pop(T& out) noexcept {
        const uint64_t pos  = deq_.load(std::memory_order_relaxed);
        Cell&          cell = buf_[pos & MASK];
        const uint64_t seq  = cell.seq.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

        if (__builtin_expect(diff == 0, 1)) {
            out = cell.data;
            cell.seq.store(pos + Cap, std::memory_order_release);
            deq_.store(pos + 1, std::memory_order_relaxed);
            return true;
        }
        return false;  // empty
    }

    bool empty() const noexcept {
        return deq_.load(std::memory_order_acquire) ==
               enq_.load(std::memory_order_acquire);
    }

    size_t size() const noexcept {
        return static_cast<size_t>(
            enq_.load(std::memory_order_acquire) -
            deq_.load(std::memory_order_acquire));
    }

    static constexpr size_t capacity() noexcept { return Cap; }
};

// ============================================================================
// SECTION 6: OPEN-ADDRESSING ORDER MAP
//
//  Old:  std::unordered_map<uint64_t, OrderInfo> + std::mutex
//        → dynamic rehashing, separate chaining, lock contention
//
//  New:  OpenAddressingOrderMap
//        → fixed 1M-slot flat array, linear probing, SINGLE WRITER (recv thread)
//          → zero locks needed on hot path
//        → All memory pre-allocated at startup
//        → O(1) average, no heap allocations per operation
// ============================================================================

struct OrderInfo {
    uint64_t  order_ref;          // order reference number (key)
    SymbolKey symbol_key;         // packed 8-char symbol
    uint32_t  price;              // in 1/10000 dollars
    uint32_t  remaining_shares;   // current open shares
    Side      side;               // BUY or SELL
    uint8_t   _pad[3];
};

static_assert(sizeof(OrderInfo) == 32, "OrderInfo must be 32 bytes (half cache line)");

class OpenAddressingOrderMap {
    static constexpr uint64_t EMPTY = 0;
    static constexpr uint64_t MASK  = MAX_ORDERS - 1;

    // Pre-allocated flat array — all in contiguous memory, cache-friendly
    std::array<OrderInfo, MAX_ORDERS> slots_;

public:
    OpenAddressingOrderMap() noexcept {
        for (auto& s : slots_) s.order_ref = EMPTY;
    }

    // Single writer — no synchronization needed
    FORCE_INLINE void insert(const OrderInfo& o) noexcept {
        uint64_t idx = o.order_ref & MASK;
        while (slots_[idx].order_ref != EMPTY && slots_[idx].order_ref != o.order_ref) {
            idx = (idx + 1) & MASK;
        }
        slots_[idx] = o;
    }

    FORCE_INLINE OrderInfo* find(uint64_t order_ref) noexcept {
        uint64_t idx = order_ref & MASK;
        while (slots_[idx].order_ref != EMPTY) {
            if (slots_[idx].order_ref == order_ref) return &slots_[idx];
            idx = (idx + 1) & MASK;
        }
        return nullptr;
    }

    FORCE_INLINE void remove(uint64_t order_ref) noexcept {
        OrderInfo* p = find(order_ref);
        if (p) p->order_ref = EMPTY;
    }
};

// ============================================================================
// SECTION 7: ARRAY-BASED ORDER BOOK — no heap, cache-friendly
//
//  Old:  std::vector<PriceLevel> + std::sort per update = O(N log N) + heap
//  New:  std::array<PriceLevel, MAX_BOOK_LEVELS> + insertion sort = O(N), N≤20
//        → 20 elements × insertion sort ≈ ~10-50 ns, fully in L1 cache
// ============================================================================

struct PriceLevel {
    uint32_t price;       // in 1/10000 dollars
    uint64_t shares;      // total shares at this level
    uint32_t order_count;
};

struct OrderBook {
    SymbolKey symbol_key{0};
    uint8_t   bid_count{0};
    uint8_t   ask_count{0};
    uint64_t  total_volume{0};
    uint32_t  last_trade_price{0};
    uint32_t  last_trade_shares{0};
    uint64_t  last_update_tsc{0};
    std::array<PriceLevel, MAX_BOOK_LEVELS> bids{};  // sorted descending (best bid first)
    std::array<PriceLevel, MAX_BOOK_LEVELS> asks{};  // sorted ascending  (best ask first)

    // ── BID SIDE ──────────────────────────────────────────────
    FORCE_INLINE void add_bid(uint32_t price, uint32_t qty) noexcept {
        // Check if price level exists
        for (uint8_t i = 0; i < bid_count; ++i) {
            if (bids[i].price == price) {
                bids[i].shares += qty;
                bids[i].order_count++;
                return;
            }
        }
        if (bid_count >= MAX_BOOK_LEVELS) return;  // book full, ignore outside range
        // Insert and keep sorted descending — insertion sort (N≤20, fits in L1)
        uint8_t pos = bid_count++;
        bids[pos] = {price, qty, 1};
        while (pos > 0 && bids[pos].price > bids[pos-1].price) {
            std::swap(bids[pos], bids[pos-1]);
            --pos;
        }
        last_update_tsc = rdtsc_now();
    }

    FORCE_INLINE void remove_bid_qty(uint32_t price, uint32_t qty) noexcept {
        for (uint8_t i = 0; i < bid_count; ++i) {
            if (bids[i].price == price) {
                if (bids[i].shares <= qty || bids[i].order_count <= 1) {
                    // Remove level: shift left
                    for (uint8_t j = i; j < bid_count - 1; ++j) bids[j] = bids[j+1];
                    --bid_count;
                } else {
                    bids[i].shares -= qty;
                    bids[i].order_count--;
                }
                return;
            }
        }
    }

    // ── ASK SIDE ──────────────────────────────────────────────
    FORCE_INLINE void add_ask(uint32_t price, uint32_t qty) noexcept {
        for (uint8_t i = 0; i < ask_count; ++i) {
            if (asks[i].price == price) {
                asks[i].shares += qty;
                asks[i].order_count++;
                return;
            }
        }
        if (ask_count >= MAX_BOOK_LEVELS) return;
        uint8_t pos = ask_count++;
        asks[pos] = {price, qty, 1};
        // Sort ascending (best ask = lowest price first)
        while (pos > 0 && asks[pos].price < asks[pos-1].price) {
            std::swap(asks[pos], asks[pos-1]);
            --pos;
        }
        last_update_tsc = rdtsc_now();
    }

    FORCE_INLINE void remove_ask_qty(uint32_t price, uint32_t qty) noexcept {
        for (uint8_t i = 0; i < ask_count; ++i) {
            if (asks[i].price == price) {
                if (asks[i].shares <= qty || asks[i].order_count <= 1) {
                    for (uint8_t j = i; j < ask_count - 1; ++j) asks[j] = asks[j+1];
                    --ask_count;
                } else {
                    asks[i].shares -= qty;
                    asks[i].order_count--;
                }
                return;
            }
        }
    }

    FORCE_INLINE uint32_t best_bid() const noexcept {
        return bid_count > 0 ? bids[0].price : 0;
    }
    FORCE_INLINE uint32_t best_ask() const noexcept {
        return ask_count > 0 ? asks[0].price : 0;
    }
    FORCE_INLINE uint32_t spread() const noexcept {
        return (bid_count > 0 && ask_count > 0) ? asks[0].price - bids[0].price : 0;
    }
};

// ============================================================================
// SECTION 8: SYMBOL TABLE — flat array, O(1) by symbol_key hash
// ============================================================================

class SymbolTable {
    static constexpr size_t SZ = MAX_SYMBOLS;
    static constexpr size_t MASK = SZ - 1;
    static_assert((SZ & (SZ-1)) == 0, "SZ must be power of 2");

    struct Entry {
        SymbolKey key{0};
        uint16_t  book_idx{0};
        bool      subscribed{false};
    };

    std::array<Entry, SZ>       slots_{};
    std::array<OrderBook, SZ>   books_{};
    uint16_t                    book_count_{0};

public:
    // Lookup or insert symbol, returns book index. Call path: hot
    FORCE_INLINE uint16_t get_or_create(SymbolKey key) noexcept {
        uint64_t h = key & MASK;
        while (slots_[h].key != 0) {
            if (slots_[h].key == key) return slots_[h].book_idx;
            h = (h + 1) & MASK;
        }
        uint16_t idx = book_count_++;
        slots_[h] = {key, idx, false};
        books_[idx].symbol_key = key;
        return idx;
    }

    FORCE_INLINE OrderBook* get_book(SymbolKey key) noexcept {
        uint64_t h = key & MASK;
        while (slots_[h].key != 0) {
            if (slots_[h].key == key) return &books_[slots_[h].book_idx];
            h = (h + 1) & MASK;
        }
        return nullptr;
    }

    FORCE_INLINE bool is_subscribed(SymbolKey key) const noexcept {
        if (subscribe_all_) return true;
        uint64_t h = key & MASK;
        while (slots_[h].key != 0) {
            if (slots_[h].key == key) return slots_[h].subscribed;
            h = (h + 1) & MASK;
        }
        return false;
    }

    // Called from management thread (cold path)
    void subscribe(SymbolKey key) noexcept {
        uint64_t h = key & MASK;
        while (slots_[h].key != 0 && slots_[h].key != key) h = (h + 1) & MASK;
        if (slots_[h].key == 0) { slots_[h].key = key; slots_[h].book_idx = book_count_++; }
        slots_[h].subscribed = true;
    }

    void subscribe_all(bool v) noexcept { subscribe_all_ = v; }

    uint16_t book_count() const noexcept { return book_count_; }

private:
    bool subscribe_all_{false};
};

// ============================================================================
// SECTION 9: CRTP EVENT HANDLER BASE
//
//  HOW TO USE:
//    class MyHandler : public ITCHEventHandler<MyHandler> {
//    public:
//        // Override only the messages you care about:
//        void on_add_order(const AddOrderMessage& m, uint64_t recv_tsc) noexcept {
//            // called with zero virtual overhead (inlined by compiler)
//        }
//    };
//
//  All unimplemented callbacks default to no-op (zero cost for unused messages).
//  Compiler eliminates the no-op calls entirely at -O2.
//
//  COST COMPARISON:
//    Virtual:  CALL [vtable + 8]  → icache miss + branch miss  ≈ 10-80ns
//    CRTP:     Inlined handler body, or NOP if not overridden   ≈ 0-3ns
// ============================================================================

template<typename Derived>
class ITCHEventHandler {
public:
    // Default no-op implementations — override only what you need.
    // Zero overhead for unused handlers (compiler eliminates entirely).
    void on_system_event           (const SystemEventMessage&,           uint64_t) noexcept {}
    void on_stock_directory        (const StockDirectoryMessage&,        uint64_t) noexcept {}
    void on_stock_trading_action   (const StockTradingActionMessage&,    uint64_t) noexcept {}
    void on_add_order              (const AddOrderMessage&,              uint64_t) noexcept {}
    void on_add_order_mpid         (const AddOrderWithMPIDMessage&,      uint64_t) noexcept {}
    void on_order_executed         (const OrderExecutedMessage&,         uint64_t) noexcept {}
    void on_order_executed_price   (const OrderExecutedWithPriceMessage&,uint64_t) noexcept {}
    void on_order_cancel           (const OrderCancelMessage&,           uint64_t) noexcept {}
    void on_order_delete           (const OrderDeleteMessage&,           uint64_t) noexcept {}
    void on_order_replace          (const OrderReplaceMessage&,          uint64_t) noexcept {}
    void on_trade                  (const TradeMessage&,                 uint64_t) noexcept {}
    void on_cross_trade            (const CrossTradeMessage&,            uint64_t) noexcept {}
    void on_broken_trade           (const BrokenTradeMessage&,           uint64_t) noexcept {}
    void on_noii                   (const NOIIMessage&,                  uint64_t) noexcept {}
    void on_disconnect             (const char* reason)                  noexcept {}

protected:
    // Downcast helper — safe because CRTP guarantees Derived IS this
    FORCE_INLINE Derived& self() noexcept {
        return *static_cast<Derived*>(this);
    }
};

// ============================================================================
// SECTION 10: DISPATCH TABLE — avoids switch/case overhead in hot path
//
//  Old:  switch(msg_type) { case 'A': ... case 'E': ... }
//        → jump table, but with virtual call inside each case
//
//  New:  Compile-time function pointer dispatch table indexed by msg_type byte
//        → single load + indirect call, handler body inlined by template
//        → ~1-3 ns total dispatch cost
// ============================================================================

template<typename Handler>
using DispatchFn = void(*)(Handler&, const uint8_t*, uint64_t) noexcept;

template<typename Handler>
struct DispatchTable {
    std::array<DispatchFn<Handler>, 256> table;

    constexpr DispatchTable() noexcept : table{} {
        // Zero-initialize: unknown types are no-ops (nullptr checked at dispatch)
        for (auto& fn : table) fn = nullptr;

        table[static_cast<uint8_t>('S')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_system_event(*reinterpret_cast<const SystemEventMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('R')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_stock_directory(*reinterpret_cast<const StockDirectoryMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('H')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_stock_trading_action(*reinterpret_cast<const StockTradingActionMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('A')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_add_order(*reinterpret_cast<const AddOrderMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('F')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_add_order_mpid(*reinterpret_cast<const AddOrderWithMPIDMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('E')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_order_executed(*reinterpret_cast<const OrderExecutedMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('C')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_order_executed_price(*reinterpret_cast<const OrderExecutedWithPriceMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('X')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_order_cancel(*reinterpret_cast<const OrderCancelMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('D')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_order_delete(*reinterpret_cast<const OrderDeleteMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('U')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_order_replace(*reinterpret_cast<const OrderReplaceMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('P')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_trade(*reinterpret_cast<const TradeMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('Q')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_cross_trade(*reinterpret_cast<const CrossTradeMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('B')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_broken_trade(*reinterpret_cast<const BrokenTradeMessage*>(d), ts);
        };
        table[static_cast<uint8_t>('I')] = [](Handler& h, const uint8_t* d, uint64_t ts) noexcept {
            h.on_noii(*reinterpret_cast<const NOIIMessage*>(d), ts);
        };
    }

    FORCE_INLINE void dispatch(Handler& h,
                                uint8_t msg_type,
                                const uint8_t* data,
                                uint64_t recv_tsc) const noexcept {
        auto fn = table[msg_type];
        if (__builtin_expect(fn != nullptr, 1)) {
            fn(h, data, recv_tsc);
        }
    }
};

// ============================================================================
// SECTION 11: CRTP FEED HANDLER (Plugin)
//
//  HOW TO USE:
//    class MyITCHPlugin : public ITCHFeedHandler<MyITCHPlugin, MyEventHandler> {
//    public:
//        void on_add_order(const AddOrderMessage& m, uint64_t tsc) noexcept {
//            // strategy logic here — zero overhead, compiler may inline
//        }
//    };
//    MyITCHPlugin handler;
//    handler.connect("233.54.12.0", 26400, "10.0.0.1");
//
//  No virtual dispatch. No shared_ptr. No mutex in hot path.
// ============================================================================

struct ITCHConfig {
    const char* multicast_ip   = "233.54.12.0";
    uint16_t    multicast_port = 26400;
    const char* interface_ip   = "0.0.0.0";
    uint32_t    rcvbuf_bytes   = 1 << 21;  // 2MB
    bool        enable_mold_udp = true;
    bool        enable_book_building = true;
    bool        enable_hw_timestamp  = true;
    uint8_t     recv_cpu_core   = 0;       // pin recv thread
    uint8_t     proc_cpu_core   = 1;       // pin processing thread
};

template<typename Derived>
class ITCHFeedHandler : public ITCHEventHandler<Derived> {
public:
    // ── LIFECYCLE ────────────────────────────────────────────────────────
    bool connect(const ITCHConfig& cfg) noexcept;
    void disconnect() noexcept;
    bool is_connected() const noexcept { return connected_.load(std::memory_order_relaxed); }

    // ── SUBSCRIPTION (cold path, management thread) ───────────────────────
    void subscribe(const char* symbol8) noexcept {
        symbols_.subscribe(pack_symbol(symbol8));
    }
    void subscribe_all() noexcept { symbols_.subscribe_all(true); }
    void unsubscribe_all() noexcept { symbols_.subscribe_all(false); }

    // ── ORDER BOOK ACCESS (read from any thread, single writer = recv thread) ─
    const OrderBook* get_book(const char* symbol8) const noexcept {
        return const_cast<SymbolTable&>(symbols_).get_book(pack_symbol(symbol8));
    }

    // ── STATISTICS ───────────────────────────────────────────────────────
    uint64_t msgs_received()  const noexcept { return msgs_recv_.load(std::memory_order_relaxed); }
    uint64_t msgs_processed() const noexcept { return msgs_proc_.load(std::memory_order_relaxed); }
    uint64_t pkts_dropped()   const noexcept { return pkts_drop_.load(std::memory_order_relaxed); }
    uint64_t orders_tracked() const noexcept { return orders_cnt_.load(std::memory_order_relaxed); }
    uint64_t trades_count()   const noexcept { return trades_cnt_.load(std::memory_order_relaxed); }

    // ── LATENCY (from rdtsc_start at recv to end of dispatch) ────────────
    // Returns latest tick-to-dispatch latency in raw TSC ticks
    uint64_t last_dispatch_ticks() const noexcept {
        return last_dispatch_ticks_.load(std::memory_order_relaxed);
    }

    ~ITCHFeedHandler() { disconnect(); }

protected:
    // ── DATA STRUCTURES (hot path — all pre-allocated, no heap in hot path) ──

    // SPSC queue: receive thread → processing thread
    // Element: RawPacket (2KB, fixed size, NO std::vector)
    SPSCQueue<RawPacket, SPSC_QUEUE_SIZE> raw_queue_;

    // Order tracking (single writer: processing thread — no lock needed)
    OpenAddressingOrderMap orders_;

    // Symbol table + order books (single writer: processing thread)
    SymbolTable symbols_;

    // Compile-time dispatch table (no switch, no virtual)
    static inline DispatchTable<Derived> dispatch_table_{};

private:
    // ── THREADING ────────────────────────────────────────────────────────
    std::thread recv_thread_;
    std::thread proc_thread_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};
    ITCHConfig cfg_{};
    int sock_fd_{-1};

    // ── STATISTICS (relaxed atomics — stats thread reads, hot thread writes) ─
    CACHE_ALIGN std::atomic<uint64_t> msgs_recv_{0};
    CACHE_ALIGN std::atomic<uint64_t> msgs_proc_{0};
    CACHE_ALIGN std::atomic<uint64_t> pkts_drop_{0};
    CACHE_ALIGN std::atomic<uint64_t> orders_cnt_{0};
    CACHE_ALIGN std::atomic<uint64_t> trades_cnt_{0};
    CACHE_ALIGN std::atomic<uint64_t> last_dispatch_ticks_{0};

    // ── RECEIVE THREAD: socket → SPSC queue ──────────────────────────────
    void recv_loop() noexcept;

    // ── PROCESSING THREAD: SPSC queue → decode → dispatch → book ─────────
    void proc_loop() noexcept;

    HOT_PATH void process_raw_packet(const RawPacket& pkt) noexcept;
    HOT_PATH void process_itch_msg(const uint8_t* data, uint16_t len,
                                    uint64_t recv_tsc) noexcept;

    // ── ORDER BOOK MAINTENANCE (inlined into processing thread) ──────────
    HOT_PATH void book_add_order(const AddOrderMessage& m) noexcept;
    HOT_PATH void book_add_order(const AddOrderWithMPIDMessage& m) noexcept;
    HOT_PATH void book_executed (const OrderExecutedMessage& m) noexcept;
    HOT_PATH void book_executed (const OrderExecutedWithPriceMessage& m) noexcept;
    HOT_PATH void book_cancel   (const OrderCancelMessage& m) noexcept;
    HOT_PATH void book_delete   (const OrderDeleteMessage& m) noexcept;
    HOT_PATH void book_replace  (const OrderReplaceMessage& m) noexcept;

    // ── HELPERS ──────────────────────────────────────────────────────────
    bool setup_socket() noexcept;
    void pin_thread(int core_id) noexcept;
};

// ============================================================================
// SECTION 12: NETWORK CONFIGURATION
// ============================================================================

struct ITCHNetworkConfig {
    const char* multicast_ip;
    uint16_t    multicast_port;
    const char* interface_ip;
    uint32_t    receive_buffer_size;
    uint32_t    socket_timeout_ms;
    bool        enable_timestamping;
    bool        enable_packet_filtering;
    bool        enable_mold_udp;
};

// ============================================================================
// SECTION 13: USAGE EXAMPLE — CRTP HANDLER
// ============================================================================
/**
 * USAGE:
 *
 *  // Step 1: Define your handler — inherit from ITCHFeedHandler<YourHandler>
 *  class MarketMakingHandler : public ITCHFeedHandler<MarketMakingHandler> {
 *  public:
 *      // Override ONLY the messages you care about.
 *      // NOT virtual — compiler inlines directly. Zero overhead.
 *
 *      void on_add_order(const AddOrderMessage& m, uint64_t recv_tsc) noexcept {
 *          SymbolKey key = pack_symbol(m.stock);
 *          if (!symbols_.is_subscribed(key)) return;    // filter by subscription
 *
 *          // Update internal order book (already done by base class if enabled)
 *          OrderBook* book = symbols_.get_book(key);
 *          if (!book) return;
 *
 *          // React to book update — e.g. recalculate quotes
 *          uint32_t bid = book->best_bid();
 *          uint32_t ask = book->best_ask();
 *          recalculate_quotes(key, bid, ask, recv_tsc);
 *      }
 *
 *      void on_trade(const TradeMessage& m, uint64_t recv_tsc) noexcept {
 *          // Update VWAP, alpha signals, etc.
 *      }
 *
 *  private:
 *      void recalculate_quotes(SymbolKey, uint32_t bid, uint32_t ask, uint64_t tsc) noexcept {
 *          // ...
 *      }
 *  };
 *
 *  // Step 2: Instantiate and connect
 *  MarketMakingHandler handler;
 *
 *  ITCHConfig cfg;
 *  cfg.multicast_ip   = "233.54.12.0";
 *  cfg.multicast_port = 26400;
 *  cfg.recv_cpu_core  = 2;   // pin recv thread
 *  cfg.proc_cpu_core  = 3;   // pin processing thread
 *
 *  handler.subscribe("AAPL    ");   // 8-char padded symbol
 *  handler.subscribe("MSFT    ");
 *  handler.connect(cfg);
 *
 *  // Step 3: Monitor
 *  while (handler.is_connected()) {
 *      std::this_thread::sleep_for(std::chrono::seconds(1));
 *      std::cout << "recv=" << handler.msgs_received()
 *                << " proc=" << handler.msgs_processed()
 *                << " drop=" << handler.pkts_dropped() << "\n";
 *  }
 */

} // namespace nasdaq::itch
