/*
 * ============================================================================
 * CME GROUP MDP 3.0 (Market Data Platform) FEED HANDLER - C++17
 * ============================================================================
 *
 * CME MDP 3.0 uses:
 *  - Simple Binary Encoding (SBE) for message serialisation
 *  - UDP multicast for market data distribution
 *  - Two channels per instrument group: Incremental (real-time) + Snapshot
 *  - FIX protocol semantics but binary encoding
 *
 * Key Message Types:
 *  - MDIncrementalRefreshBook (tag 46): Order book incremental updates
 *  - MDIncrementalRefreshTrade (tag 48): Individual trades
 *  - SnapshotFullRefresh (tag 38): Full order book snapshot
 *  - InstrumentDefinitionOption (tag 55): Contract specifications
 *  - SecurityStatus (tag 30): Trading halt/resume
 *  - QuoteRequest (tag 39): RFQ messages
 *
 * MDP 3.0 Feed Architecture:
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  CME Data Center    ┌──────────────┐                    │
 *  │                     │ Incremental  │ 239.x.x.x:port     │
 *  │  Channel A/B  ─────►│ Feed (UDP)   │ (real-time ticks)  │
 *  │                     └──────────────┘                    │
 *  │                     ┌──────────────┐                    │
 *  │                     │ Snapshot     │ 224.x.x.x:port     │
 *  │                     │ Feed (TCP)   │ (recovery)         │
 *  │                     └──────────────┘                    │
 *  └─────────────────────────────────────────────────────────┘
 *
 * Latency: CME co-location (Aurora) achieves ~1-5 µs end-to-end
 * ============================================================================
 */

#include <cstdint>
#include <cstring>
#include <cassert>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <optional>

// Simulated SBE - in production this is auto-generated from FIX repository XML
namespace sbe {

// SBE primitive types
using Int8  = int8_t;
using Int16 = int16_t;
using Int32 = int32_t;
using Int64 = int64_t;
using UInt8  = uint8_t;
using UInt16 = uint16_t;
using UInt32 = uint32_t;
using UInt64 = uint64_t;

// Null values for optional fields (SBE convention)
static constexpr Int32  NULL_INT32  = INT32_MIN;
static constexpr Int64  NULL_INT64  = INT64_MIN;
static constexpr UInt32 NULL_UINT32 = UINT32_MAX;
static constexpr UInt64 NULL_UINT64 = UINT64_MAX;

// Price scale: CME uses mantissa/exponent notation
// Price = mantissa * 10^exponent (exponent is typically -7 for futures)
struct Decimal9 {
    Int64 mantissa;
    Int8  exponent;   // typically -7 for CME futures

    [[nodiscard]] double to_double() const noexcept {
        if (mantissa == NULL_INT64) return 0.0;
        double scale = 1.0;
        int exp = exponent;
        if (exp >= 0) { for (int i = 0; i < exp;  ++i) scale *= 10.0; return mantissa * scale; }
        else          { for (int i = 0; i < -exp; ++i) scale *= 10.0; return mantissa / scale; }
    }

    [[nodiscard]] bool is_null() const noexcept { return mantissa == NULL_INT64; }
};

}  // namespace sbe

namespace cme_mdp3 {

// ============================================================================
// MessageHeader (4 bytes, always present)
// ============================================================================
struct MessageHeader {
    uint16_t block_length;   // length of root portion
    uint16_t template_id;    // message type (e.g., 46 = MDIncrementalRefreshBook)
    uint16_t schema_id;      // always 1
    uint16_t version;        // schema version
};

// ============================================================================
// Packet Header (prepended by UDP transport layer)
// ============================================================================
struct PacketHeader {
    uint32_t seq_num;       // sequence number (1-based, monotonically increasing)
    uint64_t sending_time;  // nanoseconds since epoch
};

// ============================================================================
// Action types for book updates
// ============================================================================
enum class MDUpdateAction : uint8_t {
    New    = 0,
    Change = 1,
    Delete = 2,
    DeleteThru = 3,
    DeleteFrom = 4,
    Overlay    = 5,
};

enum class MDEntryType : char {
    Bid           = '0',
    Offer         = '1',
    Trade         = '2',
    OpeningPrice  = '4',
    SettlementPrice = '6',
    HighPrice     = '7',
    LowPrice      = '8',
    TradeVolume   = 'B',
    ImpliedBid    = 'E',
    ImpliedOffer  = 'F',
    EmptyBook     = 'J',
};

// ============================================================================
// Book entry from MDIncrementalRefreshBook46
// ============================================================================
struct BookEntry {
    sbe::Decimal9   price;
    int32_t         size;
    MDUpdateAction  action;
    MDEntryType     entry_type;
    uint32_t        security_id;
    int32_t         price_level;
    uint32_t        rpt_seq;        // sequence within this packet
    uint32_t        number_of_orders;
};

// ============================================================================
// Trade entry from MDIncrementalRefreshTrade48
// ============================================================================
struct TradeEntry {
    sbe::Decimal9  last_px;
    int32_t        last_qty;
    int32_t        trade_id;
    char           aggressor_side;  // '1'=buy, '2'=sell
    uint32_t       security_id;
    uint32_t       rpt_seq;
};

// ============================================================================
// Snapshot entry from SnapshotFullRefresh38
// ============================================================================
struct SnapshotLevel {
    sbe::Decimal9  price;
    int32_t        size;
    int32_t        number_of_orders;
    int32_t        price_level;
    MDEntryType    entry_type;
};

struct SnapshotMessage {
    uint32_t               security_id;
    uint32_t               last_msg_seq_num_processed;
    uint32_t               total_num_reports;
    uint32_t               current_chunk;
    std::vector<SnapshotLevel> levels;
};

// ============================================================================
// Order Book (price-level book for one instrument)
// ============================================================================
struct PriceLevel {
    double   price;
    int32_t  qty;
    int32_t  num_orders;
    int32_t  level;
};

class OrderBook {
public:
    explicit OrderBook(uint32_t security_id) : security_id_(security_id) {}

    void apply_book_entry(const BookEntry& e) {
        auto& side = (e.entry_type == MDEntryType::Bid) ? bids_ : asks_;
        double price = e.price.to_double();

        switch (e.action) {
            case MDUpdateAction::New:
                side.push_back({price, e.size, (int)e.number_of_orders, e.price_level});
                sort_levels(side, e.entry_type == MDEntryType::Bid);
                break;
            case MDUpdateAction::Change:
                for (auto& l : side) {
                    if (l.level == e.price_level) {
                        l.price = price;
                        l.qty   = e.size;
                        l.num_orders = e.number_of_orders;
                        break;
                    }
                }
                break;
            case MDUpdateAction::Delete:
                side.erase(
                    std::remove_if(side.begin(), side.end(),
                        [&](const PriceLevel& l){ return l.level == e.price_level; }),
                    side.end());
                break;
            case MDUpdateAction::DeleteThru:
                // Delete all levels from 1 through price_level
                side.erase(
                    std::remove_if(side.begin(), side.end(),
                        [&](const PriceLevel& l){ return l.level <= e.price_level; }),
                    side.end());
                break;
            case MDUpdateAction::DeleteFrom:
                // Delete all levels from price_level to end
                side.erase(
                    std::remove_if(side.begin(), side.end(),
                        [&](const PriceLevel& l){ return l.level >= e.price_level; }),
                    side.end());
                break;
            case MDUpdateAction::Overlay:
                for (auto& l : side) {
                    if (l.level == e.price_level) {
                        l.price = price;
                        l.qty   = e.size;
                        break;
                    }
                }
                break;
        }
        ++rpt_seq_;
    }

    void apply_snapshot(const SnapshotMessage& snap) {
        bids_.clear();
        asks_.clear();
        for (const auto& lvl : snap.levels) {
            PriceLevel pl{lvl.price.to_double(), lvl.size, lvl.number_of_orders, lvl.price_level};
            if (lvl.entry_type == MDEntryType::Bid)        bids_.push_back(pl);
            else if (lvl.entry_type == MDEntryType::Offer) asks_.push_back(pl);
        }
        sort_levels(bids_, true);
        sort_levels(asks_, false);
        rpt_seq_ = snap.last_msg_seq_num_processed;
        in_sync_ = true;
    }

    [[nodiscard]] std::optional<PriceLevel> best_bid() const noexcept {
        return bids_.empty() ? std::nullopt : std::optional<PriceLevel>(bids_.front());
    }
    [[nodiscard]] std::optional<PriceLevel> best_ask() const noexcept {
        return asks_.empty() ? std::nullopt : std::optional<PriceLevel>(asks_.front());
    }

    [[nodiscard]] double mid_price() const noexcept {
        auto b = best_bid(), a = best_ask();
        if (b && a) return (b->price + a->price) / 2.0;
        return 0.0;
    }

    [[nodiscard]] double spread() const noexcept {
        auto b = best_bid(), a = best_ask();
        if (b && a) return a->price - b->price;
        return 0.0;
    }

    void print(int depth = 5) const {
        std::cout << "  SecurityID=" << security_id_
                  << " mid=" << std::fixed << std::setprecision(4) << mid_price()
                  << " spread=" << spread()
                  << " in_sync=" << in_sync_ << "\n";

        int n = std::min(depth, (int)asks_.size());
        std::cout << "  ASKS:\n";
        for (int i = n - 1; i >= 0; --i) {
            const auto& l = asks_[i];
            std::cout << "    L" << l.level << "  "
                      << std::setw(12) << std::fixed << std::setprecision(4) << l.price
                      << "  qty=" << std::setw(8) << l.qty
                      << "  orders=" << l.num_orders << "\n";
        }
        std::cout << "  BIDS:\n";
        n = std::min(depth, (int)bids_.size());
        for (int i = 0; i < n; ++i) {
            const auto& l = bids_[i];
            std::cout << "    L" << l.level << "  "
                      << std::setw(12) << std::fixed << std::setprecision(4) << l.price
                      << "  qty=" << std::setw(8) << l.qty
                      << "  orders=" << l.num_orders << "\n";
        }
    }

    [[nodiscard]] uint32_t security_id() const noexcept { return security_id_; }
    [[nodiscard]] bool     is_in_sync()  const noexcept { return in_sync_; }

private:
    static void sort_levels(std::vector<PriceLevel>& side, bool descending) {
        std::sort(side.begin(), side.end(), [descending](const PriceLevel& a, const PriceLevel& b) {
            return descending ? a.price > b.price : a.price < b.price;
        });
    }

    uint32_t               security_id_;
    std::vector<PriceLevel> bids_, asks_;
    uint32_t               rpt_seq_{0};
    bool                   in_sync_{false};
};

// ============================================================================
// CME MDP3 Feed Handler
// ============================================================================
struct FeedStats {
    uint64_t packets_received{0};
    uint64_t messages_processed{0};
    uint64_t book_updates{0};
    uint64_t trades{0};
    uint64_t snapshots{0};
    uint64_t gaps_detected{0};
    uint64_t recoveries{0};
};

class MDP3FeedHandler {
public:
    using BookUpdateCallback = std::function<void(OrderBook&, const BookEntry&)>;
    using TradeCallback      = std::function<void(const TradeEntry&)>;
    using SnapshotCallback   = std::function<void(OrderBook&, const SnapshotMessage&)>;

    void set_book_update_cb(BookUpdateCallback cb) { book_update_cb_ = std::move(cb); }
    void set_trade_cb(TradeCallback cb)             { trade_cb_       = std::move(cb); }
    void set_snapshot_cb(SnapshotCallback cb)       { snapshot_cb_    = std::move(cb); }

    // Register an instrument we care about
    void subscribe(uint32_t security_id) {
        books_.emplace(security_id, OrderBook(security_id));
    }

    // Process a simulated MDIncrementalRefreshBook message
    void on_book_update(const std::vector<BookEntry>& entries) {
        ++stats_.packets_received;
        for (const auto& e : entries) {
            ++stats_.messages_processed;
            ++stats_.book_updates;
            auto it = books_.find(e.security_id);
            if (it == books_.end()) continue;  // not subscribed
            it->second.apply_book_entry(e);
            if (book_update_cb_) book_update_cb_(it->second, e);
        }
    }

    // Process a trade
    void on_trade(const TradeEntry& trade) {
        ++stats_.trades;
        if (trade_cb_) trade_cb_(trade);
    }

    // Apply snapshot during recovery
    void on_snapshot(const SnapshotMessage& snap) {
        ++stats_.snapshots;
        auto it = books_.find(snap.security_id);
        if (it == books_.end()) return;
        it->second.apply_snapshot(snap);
        if (snapshot_cb_) snapshot_cb_(it->second, snap);
    }

    OrderBook* get_book(uint32_t security_id) {
        auto it = books_.find(security_id);
        return (it != books_.end()) ? &it->second : nullptr;
    }

    const FeedStats& stats() const noexcept { return stats_; }

    void print_stats() const {
        std::cout << "\n=== MDP3 Feed Stats ===\n";
        std::cout << "  Packets received:   " << stats_.packets_received   << "\n";
        std::cout << "  Messages processed: " << stats_.messages_processed << "\n";
        std::cout << "  Book updates:       " << stats_.book_updates       << "\n";
        std::cout << "  Trades:             " << stats_.trades             << "\n";
        std::cout << "  Snapshots applied:  " << stats_.snapshots          << "\n";
        std::cout << "  Gaps detected:      " << stats_.gaps_detected      << "\n";
    }

private:
    std::unordered_map<uint32_t, OrderBook> books_;
    BookUpdateCallback book_update_cb_;
    TradeCallback      trade_cb_;
    SnapshotCallback   snapshot_cb_;
    FeedStats          stats_;
    uint32_t           expected_seq_{1};
};

// ============================================================================
// Simulation helpers
// ============================================================================
std::vector<BookEntry> simulate_book_update(uint32_t security_id,
                                              double   base_bid,
                                              double   tick_size,
                                              int      depth) {
    std::vector<BookEntry> entries;
    // Generate bid side
    for (int i = 1; i <= depth; ++i) {
        BookEntry e{};
        e.price      = {static_cast<int64_t>((base_bid - (i-1)*tick_size) * 1e7), -7};
        e.size       = 100 * (depth - i + 1);
        e.action     = MDUpdateAction::New;
        e.entry_type = MDEntryType::Bid;
        e.security_id= security_id;
        e.price_level= i;
        e.number_of_orders = i * 2;
        entries.push_back(e);
    }
    // Generate ask side
    double base_ask = base_bid + tick_size;
    for (int i = 1; i <= depth; ++i) {
        BookEntry e{};
        e.price      = {static_cast<int64_t>((base_ask + (i-1)*tick_size) * 1e7), -7};
        e.size       = 100 * (depth - i + 1);
        e.action     = MDUpdateAction::New;
        e.entry_type = MDEntryType::Offer;
        e.security_id= security_id;
        e.price_level= i;
        e.number_of_orders = i * 2;
        entries.push_back(e);
    }
    return entries;
}

SnapshotMessage simulate_snapshot(uint32_t security_id, double base_price, int depth) {
    SnapshotMessage snap;
    snap.security_id = security_id;
    snap.last_msg_seq_num_processed = 100;
    snap.total_num_reports = 1;
    snap.current_chunk = 1;

    double tick = 0.0025;  // ES futures tick
    for (int i = 1; i <= depth; ++i) {
        snap.levels.push_back({
            {static_cast<int64_t>((base_price - (i-1)*tick) * 1e7), -7},
            200 * (depth - i + 1), i * 3, i, MDEntryType::Bid
        });
        snap.levels.push_back({
            {static_cast<int64_t>((base_price + i*tick) * 1e7), -7},
            150 * (depth - i + 1), i * 2, i, MDEntryType::Offer
        });
    }
    return snap;
}

// ============================================================================
// Benchmark
// ============================================================================
void benchmark_feed_handler(MDP3FeedHandler& handler, uint32_t security_id) {
    const int N = 500'000;
    std::vector<double> lats;
    lats.reserve(N);

    double price = 4500.00;
    double tick  = 0.0025;

    for (int i = 0; i < N; ++i) {
        // Simulate L1 update (single bid change)
        BookEntry e{};
        e.price       = {static_cast<int64_t>((price + (i % 10) * tick) * 1e7), -7};
        e.size        = 100 + i % 50;
        e.action      = MDUpdateAction::Change;
        e.entry_type  = MDEntryType::Bid;
        e.security_id = security_id;
        e.price_level = 1;
        e.number_of_orders = 5;

        auto t0 = std::chrono::steady_clock::now();
        handler.on_book_update({e});
        auto t1 = std::chrono::steady_clock::now();

        lats.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
    }

    std::sort(lats.begin(), lats.end());
    double sum = 0; for (auto v : lats) sum += v;

    std::cout << "\n--- CME MDP3 Book Update Latency (" << N << " msgs) ---\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  min   : " << lats.front()                         << " ns\n";
    std::cout << "  mean  : " << sum / N                              << " ns\n";
    std::cout << "  p50   : " << lats[N/2]                            << " ns\n";
    std::cout << "  p99   : " << lats[static_cast<size_t>(N*0.99)]    << " ns\n";
    std::cout << "  p99.9 : " << lats[static_cast<size_t>(N*0.999)]   << " ns\n";
    std::cout << "  max   : " << lats.back()                          << " ns\n";
}

// ============================================================================
// Demo
// ============================================================================
void run_demo() {
    std::cout << "===========================================\n";
    std::cout << "  CME MDP 3.0 Feed Handler Demo\n";
    std::cout << "===========================================\n";

    MDP3FeedHandler handler;

    // Subscribe to ES (E-mini S&P 500) and NQ (E-mini NASDAQ)
    const uint32_t ES_ID = 11461;  // CME security ID for ESZ24
    const uint32_t NQ_ID = 11462;
    handler.subscribe(ES_ID);
    handler.subscribe(NQ_ID);

    // Callbacks
    int trade_count = 0;
    handler.set_book_update_cb([](OrderBook& book, const BookEntry& e) {
        // In production: trigger signal generation, risk checks, etc.
        // Deliberately minimal to measure pure feed handler latency
    });
    handler.set_trade_cb([&trade_count](const TradeEntry& t) {
        ++trade_count;
    });

    // ---- Snapshot (initial book state) ----
    std::cout << "\n--- Applying Initial Snapshot (ES @ 4500.00) ---\n";
    auto snap_es = simulate_snapshot(ES_ID, 4500.00, 10);
    handler.on_snapshot(snap_es);
    auto* es_book = handler.get_book(ES_ID);
    if (es_book) es_book->print(5);

    // ---- Incremental updates ----
    std::cout << "\n--- Processing Incremental Book Updates ---\n";
    double bid = 4499.9975, tick = 0.0025;
    for (int i = 0; i < 20; ++i) {
        auto entries = simulate_book_update(ES_ID, bid + i * 0.25, tick, 3);
        handler.on_book_update(entries);
    }
    if (es_book) es_book->print(3);

    // ---- Simulate a trade ----
    std::cout << "\n--- Processing Trades ---\n";
    for (int i = 0; i < 5; ++i) {
        TradeEntry t{};
        t.last_px   = {static_cast<int64_t>(4500.50 * 1e7), -7};
        t.last_qty  = 10 * (i + 1);
        t.trade_id  = i + 1000;
        t.aggressor_side = '1';  // buy-side aggressor
        t.security_id    = ES_ID;
        handler.on_trade(t);
        std::cout << "  Trade " << i+1 << ": qty=" << t.last_qty
                  << " @ " << t.last_px.to_double() << "\n";
    }
    std::cout << "Total trades received: " << trade_count << "\n";

    // ---- NQ snapshot ----
    auto snap_nq = simulate_snapshot(NQ_ID, 15800.00, 5);
    handler.on_snapshot(snap_nq);
    std::cout << "\n--- NQ Book ---\n";
    auto* nq_book = handler.get_book(NQ_ID);
    if (nq_book) nq_book->print(3);

    // ---- Stats ----
    handler.print_stats();

    // ---- Benchmark ----
    benchmark_feed_handler(handler, ES_ID);
}

}  // namespace cme_mdp3

int main() {
    cme_mdp3::run_demo();
    return 0;
}

