/*
 * ============================================================================
 * ULL MARKET FEED HANDLERS - C++17
 * ============================================================================
 *
 * All feed handler implementations + plug-and-play container.
 *
 * Contents:
 *   §A  IFeedHandler — pure virtual interface (plug-and-play contract)
 *   §B  HKEX OMD-C / OMD-D feed handler
 *         Protocol: CME-style binary; big-endian; multicast 224.0.x.x
 *         Messages: AggrOrderBookUpdate(353), Trade(350), SecurityStatus(360)
 *   §C  ASX ITCH 1.1 feed handler
 *         Protocol: NASDAQ ITCH-conformant; big-endian; multicast
 *         Messages: AddOrder(A), DeleteOrder(D), OrderExecuted(E), Trade(P)
 *   §D  SGX ITCH feed handler (reuses ASX parser, different multicast groups)
 *   §E  OSE (Osaka/JPX) ITCH feed handler
 *   §F  FeedHandlerContainer — loads N handlers, manages threads, NUMA
 *
 * Thread model per handler:
 *   recv_thread  (NUMA 0, core N)   ← ef_vi / UDP recv → SpscRing
 *   parse_thread (NUMA 0, core N+1) ← SpscRing → parse → SoABook + SpmcDisruptor
 *
 * Fan-out model:
 *   SpmcDisruptor → strategy threads (NUMA 1, cores M..)
 *     Each strategy registers as a consumer and specifies its instrument set.
 *     Events not matching a strategy's subscription are skipped in O(1).
 * ============================================================================
 */
#pragma once
#include "ull_feed_handler_infrastructure.hpp"

#include <atomic>
#include <thread>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>
#include <optional>
#include <climits>

namespace ull {

// ============================================================================
// §A  IFeedHandler  — plug-and-play interface
// ============================================================================
// All per-handler config lives here (no virtual constructors needed).
struct FeedConfig {
    MarketID    market_id;
    std::string name;                // human label, e.g. "HKEX_OMDCD"
    std::string interface_name;      // NIC interface, e.g. "eth0" / "enp1s0f0"
    std::string mcast_group_a;       // Channel A incremental multicast
    uint16_t    mcast_port_a{0};
    std::string mcast_group_b;       // Channel B (redundant)
    uint16_t    mcast_port_b{0};
    std::string snapshot_mcast;      // Snapshot / recovery multicast
    uint16_t    snapshot_port{0};
    int         recv_cpu{0};         // core for recv thread (NUMA 0)
    int         parse_cpu{1};        // core for parse thread (NUMA 0)
    int         numa_node{0};        // NUMA node for recv/parse threads
    bool        use_channel_b{true}; // dedup from two feeds
};

// Subscription: set of instruments this feed handler exposes
struct InstrumentInfo {
    InstrumentID id;
    Symbol       symbol;
    uint64_t     exchange_security_id;  // exchange-native numeric ID
    int32_t      price_exponent;        // e.g. -4 means price in 0.0001 units
    uint32_t     lot_size;
};

// Abstract base class — one instance per market feed
class IFeedHandler {
public:
    virtual ~IFeedHandler() = default;

    // Called once by container after creation
    virtual void configure(FeedConfig cfg, BookRegistry& books,
                           SpmcDisruptor<MarketEvent, 65536, 32>& ring) = 0;

    // Register all instruments and get their InstrumentIDs
    virtual std::vector<InstrumentInfo> list_instruments() const = 0;

    // Start recv + parse threads
    virtual void start() = 0;
    virtual void stop()  = 0;

    virtual const FeedConfig& config() const = 0;
    virtual bool is_running() const noexcept = 0;
    virtual void print_stats() const = 0;
};

// ============================================================================
// BINARY PARSING HELPERS (big-endian, zero-copy from DMA buffer)
// ============================================================================
namespace parse {

[[nodiscard]] inline uint16_t be16(const uint8_t* p) noexcept {
    return uint16_t(p[0]) << 8 | p[1];
}
[[nodiscard]] inline uint32_t be32(const uint8_t* p) noexcept {
    return uint32_t(p[0])<<24 | uint32_t(p[1])<<16 | uint32_t(p[2])<<8 | p[3];
}
[[nodiscard]] inline uint64_t be64(const uint8_t* p) noexcept {
    return uint64_t(be32(p)) << 32 | be32(p + 4);
}
[[nodiscard]] inline uint16_t le16(const uint8_t* p) noexcept {
    return uint16_t(p[1]) << 8 | p[0];
}
[[nodiscard]] inline uint32_t le32(const uint8_t* p) noexcept {
    return uint32_t(p[3])<<24 | uint32_t(p[2])<<16 | uint32_t(p[1])<<8 | p[0];
}
[[nodiscard]] inline uint64_t le64(const uint8_t* p) noexcept {
    return uint64_t(le32(p + 4)) << 32 | le32(p);
}

// Convert exchange raw price using exponent: raw * 10^exponent → int64_t fp (9dp)
[[nodiscard]] inline int64_t raw_to_fp(int64_t raw, int32_t exponent) noexcept {
    // target: raw * 10^exponent * 10^9 = raw * 10^(9+exponent)
    int32_t shift = 9 + exponent;
    if (shift >= 0) {
        int64_t scale = 1;
        for (int i = 0; i < shift; ++i) scale *= 10;
        return raw * scale;
    } else {
        int64_t scale = 1;
        for (int i = 0; i < -shift; ++i) scale *= 10;
        return raw / scale;
    }
}

}  // namespace parse

// ============================================================================
// COMMON BASE: RawPacket buffer distributed via SPSC to parser
// ============================================================================
struct alignas(CACHE_LINE) RawPacket {
    uint8_t  data[1400];   // max UDP payload we care about
    uint16_t len;
    uint16_t src_port;
    uint64_t timestamp_ns;
    // 1400+2+2+8 = 1412 bytes; pad to next 64-byte boundary → 1472 = 23×64
    char     _pad[60];
};
// RawPacket is 1 cache line of metadata + payload
// (data[] is contiguous, parser reads directly from here = zero-copy within process)

// ============================================================================
// §B  HKEX OMD-C / OMD-D FEED HANDLER
//
// Protocol: HKEX OMD (Options Market Data / Derivatives) binary protocol
//   - Big-endian fields
//   - UDP multicast, 224.0.x.x (production), 239.x.x.x (co-lo)
//   - Packet header + one or more messages per UDP packet
//   - Two redundant feeds: Channel A (primary) + Channel B (backup)
//
// Message types used:
//   350  Trade               (price, qty, aggressor side)
//   353  AggrOrderBookUpdate (5-level bid/ask update)
//   360  SecurityStatus      (trading halt/resume/pre-open/close)
//   369  MarketDefinition    (instrument spec at startup)
//
// Wire format of AggrOrderBookUpdate (353):
//   4   SecurityCode  uint32
//   4   AskPrice[0]   int32  (price * 10^-3)
//   4   AskQty[0]     uint32
//   4   BidPrice[0]   int32
//   4   BidQty[0]     uint32
//   ... (5 levels total = 5 * 4 * 4 = 80 bytes body after security code)
//   2   NumberOfOrders uint16 per level (appended)
//
// OMD Packet Header (16 bytes):
//   2  PktSize
//   1  MsgCount
//   1  SeqNumOffset
//   4  SeqNum
//   8  SendTime (nanoseconds since midnight)
// ============================================================================
class HkexOmdFeedHandler final : public IFeedHandler {
public:
    explicit HkexOmdFeedHandler() = default;

    void configure(FeedConfig cfg, BookRegistry& books,
                   SpmcDisruptor<MarketEvent, 65536, 32>& ring) override {
        cfg_   = std::move(cfg);
        books_ = &books;
        ring_  = &ring;
    }

    std::vector<InstrumentInfo> list_instruments() const override {
        return instruments_;
    }

    // Pre-register known instruments (in production: learned from MarketDefinition msgs)
    void add_instrument(uint32_t security_code, std::string_view symbol,
                        int32_t price_exp = -3) {
        InstrumentInfo info;
        info.exchange_security_id = security_code;
        info.symbol               = Symbol(symbol);
        info.price_exponent       = price_exp;
        info.lot_size             = 1;
        // Register with book registry
        info.id = books_->register_instrument(symbol);
        instruments_.push_back(info);
        code_to_id_[security_code] = info.id;
    }

    void start() override {
        running_.store(true, std::memory_order_release);
        recv_thread_  = std::thread(&HkexOmdFeedHandler::recv_loop,  this);
        parse_thread_ = std::thread(&HkexOmdFeedHandler::parse_loop, this);
    }

    void stop() override {
        running_.store(false, std::memory_order_release);
        if (recv_thread_.joinable())  recv_thread_.join();
        if (parse_thread_.joinable()) parse_thread_.join();
    }

    const FeedConfig& config() const override { return cfg_; }
    bool is_running() const noexcept override {
        return running_.load(std::memory_order_relaxed);
    }

    void print_stats() const override {
        std::cout << "[" << cfg_.name << "] pkts=" << stats_.packets
                  << " msgs=" << stats_.messages
                  << " book_upd=" << stats_.book_updates
                  << " trades=" << stats_.trades
                  << " gaps=" << stats_.gaps
                  << " ooo=" << stats_.out_of_order << "\n";
    }

private:
    // ---- Recv thread: ef_vi poll → SPSC ring ----------------------------
    void recv_loop() noexcept {
        pin_thread_to_cpu(cfg_.recv_cpu);
        set_realtime(85);
        EfViTransport transport;
        transport.open(cfg_.interface_name.c_str(),
                       cfg_.mcast_group_a.c_str(),
                       cfg_.mcast_port_a, cfg_.numa_node);
        // Also open channel B transport for dedup
        EfViTransport transport_b;
        if (cfg_.use_channel_b && !cfg_.mcast_group_b.empty()) {
            transport_b.open(cfg_.interface_name.c_str(),
                             cfg_.mcast_group_b.c_str(),
                             cfg_.mcast_port_b, cfg_.numa_node);
        }
        while (running_.load(std::memory_order_relaxed)) {
            RecvPacket pkt{};
            if (transport.recv_packet(pkt)) {
                push_raw(pkt.data, pkt.len, pkt.timestamp_ns);
                transport.release_packet(pkt.buf_id);
                ++stats_.packets;
            }
            if (cfg_.use_channel_b) {
                RecvPacket pkt_b{};
                if (transport_b.recv_packet(pkt_b)) {
                    push_raw(pkt_b.data, pkt_b.len, pkt_b.timestamp_ns);
                    transport_b.release_packet(pkt_b.buf_id);
                }
            }
        }
    }

    void push_raw(const uint8_t* data, uint16_t len, uint64_t ts) noexcept {
        RawPacket* slot = spsc_.try_claim();
        if (!slot) { ++stats_.gaps; return; }  // ring full: drop (should not happen on isolated core)
        uint16_t copy_len = std::min<uint16_t>(len, sizeof(slot->data));
        std::memcpy(slot->data, data, copy_len);
        slot->len          = copy_len;
        slot->timestamp_ns = ts;
        spsc_.publish();
    }

    // ---- Parse thread: SPSC ring → decode → update SoABook + Disruptor --
    void parse_loop() noexcept {
        pin_thread_to_cpu(cfg_.parse_cpu);
        set_realtime(84);
        while (running_.load(std::memory_order_relaxed)) {
            const RawPacket* raw = spsc_.try_peek();
            if (!raw) { spin_pause(); continue; }

            parse_packet(raw->data, raw->len, raw->timestamp_ns);
            spsc_.consume();
        }
    }

    // OMD Packet header: 16 bytes
    // msg header inside packet: 4 bytes (MsgSize=2, MsgType=2)
    void parse_packet(const uint8_t* data, uint16_t len,
                      uint64_t ts_ns) noexcept {
        if (len < 16) return;
        // uint16_t pkt_size   = parse::be16(data + 0);
        uint8_t  msg_count  = data[2];
        uint32_t seq_num    = parse::be32(data + 4);
        uint64_t send_time  = parse::be64(data + 8);  // ns since midnight

        // Gap detection
        if (seq_num != expected_seq_) {
            if (seq_num > expected_seq_) ++stats_.gaps;
            else                        ++stats_.out_of_order;
        }
        expected_seq_ = seq_num + 1;

        uint16_t offset = 16;  // after OMD packet header
        for (uint8_t m = 0; m < msg_count && offset < len; ++m) {
            if (offset + 4 > len) break;
            uint16_t msg_size = parse::be16(data + offset);
            uint16_t msg_type = parse::be16(data + offset + 2);
            if (offset + msg_size > len) break;
            const uint8_t* body = data + offset + 4;

            switch (msg_type) {
                case 353: on_aggr_book_update(body, msg_size - 4, ts_ns, seq_num); break;
                case 350: on_trade           (body, msg_size - 4, ts_ns, seq_num); break;
                case 360: on_security_status (body, msg_size - 4, ts_ns, seq_num); break;
                default:  break;
            }
            offset += msg_size;
            ++stats_.messages;
        }
    }

    // AggrOrderBookUpdate (353): 4 + 5*(4+4+4+4) + 5*2 = 4+80+10 = 94 bytes
    // Format: SecurityCode(4), [AskPx(4)+AskQty(4)+BidPx(4)+BidQty(4)]*5, [NumOrders(2)]*10
    void on_aggr_book_update(const uint8_t* b, uint16_t sz,
                              uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 4) return;
        uint32_t code = parse::be32(b);
        auto it = code_to_id_.find(code);
        if (it == code_to_id_.end()) return;  // not subscribed
        InstrumentID id   = it->second;
        int32_t      exp  = get_exponent(id);
        SoABook&     book = books_->book(id);

        static constexpr int DEPTH = 5;
        int64_t ask_fp [DEPTH], bid_fp [DEPTH];
        int32_t ask_qty[DEPTH], bid_qty[DEPTH];
        int32_t ask_ord[DEPTH], bid_ord[DEPTH];

        // 5 levels: each level = AskPx(4)+AskQty(4)+BidPx(4)+BidQty(4)
        if (sz < 4 + DEPTH * 16) return;
        for (int i = 0; i < DEPTH; ++i) {
            int off = 4 + i * 16;
            ask_fp [i] = parse::raw_to_fp(int32_t(parse::be32(b + off)),     exp);
            ask_qty[i] = int32_t(parse::be32(b + off + 4));
            bid_fp [i] = parse::raw_to_fp(int32_t(parse::be32(b + off + 8)), exp);
            bid_qty[i] = int32_t(parse::be32(b + off + 12));
            ask_ord[i] = bid_ord[i] = 0;  // default
        }
        // Optional: number of orders (10 × uint16 at end)
        if (sz >= uint16_t(4 + DEPTH*16 + DEPTH*2*2)) {
            int off = 4 + DEPTH * 16;
            for (int i = 0; i < DEPTH; ++i) {
                bid_ord[i] = parse::be16(b + off + i * 2);
                ask_ord[i] = parse::be16(b + off + DEPTH*2 + i * 2);
            }
        }

        book.replace_bids(bid_fp, bid_qty, bid_ord, DEPTH);
        book.replace_asks(ask_fp, ask_qty, ask_ord, DEPTH);
        book.set_in_sync(true);
        book.last_update_ns = ts_ns;
        ++stats_.book_updates;

        // Publish L1 event to Disruptor
        emit_event(id, EventType::BookL1, ts_ns, seq,
                   book.best_bid_fp, book.bid_qtys[0],
                   book.best_ask_fp, book.ask_qtys[0],
                   0, book.symbol);
    }

    // Trade (350): SecurityCode(4), Price(4), Qty(4), TradeCode(1), ...
    void on_trade(const uint8_t* b, uint16_t sz,
                  uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 12) return;
        uint32_t code  = parse::be32(b);
        auto it = code_to_id_.find(code);
        if (it == code_to_id_.end()) return;
        InstrumentID id   = it->second;
        int32_t      exp  = get_exponent(id);
        int64_t price_fp  = parse::raw_to_fp(int32_t(parse::be32(b + 4)), exp);
        int32_t qty       = int32_t(parse::be32(b + 8));
        SoABook& book     = books_->book(id);
        book.update_trade(price_fp, qty, ts_ns);
        ++stats_.trades;

        emit_event(id, EventType::Trade, ts_ns, seq,
                   price_fp, qty, price_fp, qty, 0, book.symbol);
    }

    // SecurityStatus (360): SecurityCode(4), Status(1), ...
    void on_security_status(const uint8_t* b, uint16_t sz,
                             uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 5) return;
        uint32_t code   = parse::be32(b);
        uint8_t  status = b[4];   // 1=PreOpen,2=Open,3=PreClose,4=Close,5=Halt
        auto it = code_to_id_.find(code);
        if (it == code_to_id_.end()) return;
        InstrumentID id = it->second;
        SoABook& book   = books_->book(id);
        bool halted = (status == 5);
        book.set_halted(halted);
        if (status == 2) book.set_in_sync(true);

        emit_event(id, EventType::Status, ts_ns, seq,
                   int64_t(status), 0, 0, 0, 0, book.symbol);
    }

    // Publish an event to the SPMC Disruptor (zero-copy from book state)
    __attribute__((hot))
    void emit_event(InstrumentID id, EventType type, uint64_t ts_ns,
                    uint32_t seq, int64_t px1, int32_t qty1,
                    int64_t px2, int32_t qty2, int level,
                    const char* sym) noexcept {
        uint64_t slot_seq = ring_->claim();
        MarketEvent& ev   = ring_->slot(slot_seq);
        ev.timestamp_ns   = ts_ns;
        ev.instrument_id  = id;
        ev.type           = type;
        ev.market_id      = uint8_t(cfg_.market_id);
        ev.price_fp       = px1;
        ev.qty            = qty1;
        ev.price2_fp      = px2;
        ev.qty2           = qty2;
        ev.level          = level;
        ev.seq_num        = seq;
        std::strncpy(ev.symbol, sym, 7); ev.symbol[7] = '\0';
        ring_->publish(slot_seq);
    }

    [[nodiscard]] int32_t get_exponent(InstrumentID id) const noexcept {
        for (auto& info : instruments_)
            if (info.id == id) return info.price_exponent;
        return -3;
    }

    // State
    FeedConfig                                  cfg_;
    BookRegistry*                               books_{nullptr};
    SpmcDisruptor<MarketEvent, 65536, 32>*      ring_{nullptr};
    std::vector<InstrumentInfo>                 instruments_;
    std::unordered_map<uint64_t, InstrumentID>  code_to_id_;

    SpscRing<RawPacket, 4096>                   spsc_;

    std::thread                                 recv_thread_;
    std::thread                                 parse_thread_;
    std::atomic<bool>                           running_{false};
    uint32_t                                    expected_seq_{1};

    struct Stats {
        uint64_t packets{0}, messages{0}, book_updates{0};
        uint64_t trades{0}, gaps{0}, out_of_order{0};
    } stats_;
};

// ============================================================================
// §C  ASX ITCH 1.1 FEED HANDLER
//
// Protocol: NASDAQ ITCH 5.0-conformant (ASX adaptation)
//   - Big-endian
//   - Message types defined in ASX ITCH 1.1 specification
//   - MoldUDP64 framing (sequence number + message block)
//
// MoldUDP64 header (20 bytes):
//   10  Session (ASCII)
//    8  SeqNum (uint64, big-endian)
//    2  MessageCount (uint16)
//   Then MessageCount blocks of: Length(2) + Body(Length)
//
// Key ITCH messages:
//   'A'  AddOrder         – order_ref(8), side(1), qty(4), symbol(8), price(4)
//   'D'  DeleteOrder      – order_ref(8)
//   'U'  ReplaceOrder     – old_ref(8), new_ref(8), qty(4), price(4)
//   'E'  OrderExecuted    – order_ref(8), qty(4), match_id(8)
//   'C'  OrderExecutedWithPrice – order_ref(8), qty(4), match_id(8), print(1), price(4)
//   'P'  Trade            – order_ref(8), side(1), qty(4), symbol(8), price(4), match_id(8)
//   'H'  TradingAction    – symbol(8), state(1), reason(4)
//   'S'  SystemEvent      – code(1): O=open, C=close, ...
//   'R'  StockDirectory   – symbol(8), mktcategory(1), ... (instrument definition)
//
// Order book building strategy:
//   - Maintain per-instrument order map: order_ref → {side, price_fp, qty}
//   - On AddOrder: add to order map + insert into SoA price level
//   - On DeleteOrder: remove from order map + decrement price level qty
//   - On ReplaceOrder: delete old + add new
//   - On Execute/Trade: reduce qty (may trigger Delete if fully executed)
// ============================================================================

// Per-order state for order tracking (one cache line)
struct alignas(CACHE_LINE) OrderEntry {
    uint64_t order_ref;      //  8
    int64_t  price_fp;       //  8
    uint32_t qty;            //  4
    int32_t  level;          //  4  index in SoA book
    Side     side;           //  1
    uint8_t  flags;          //  1
    char     _pad[64 - 8 - 8 - 4 - 4 - 1 - 1];
};

// Lightweight price-level aggregator (per instrument, per side)
// Used by ITCH handler to maintain SoA book from individual orders
struct PriceLevelMap {
    // price_fp → {total_qty, order_count}
    struct LevelData { int64_t price_fp; int32_t qty; int32_t orders; };
    std::vector<LevelData> levels;  // sorted: bids desc, asks asc

    void add_order(int64_t price_fp, int32_t qty, bool is_bid) {
        for (auto& l : levels) {
            if (l.price_fp == price_fp) { l.qty += qty; ++l.orders; return; }
        }
        levels.push_back({price_fp, qty, 1});
        sort(is_bid);
    }

    void remove_order(int64_t price_fp, int32_t qty) {
        for (auto it = levels.begin(); it != levels.end(); ++it) {
            if (it->price_fp == price_fp) {
                it->qty -= qty;
                --it->orders;
                if (it->qty <= 0) levels.erase(it);
                return;
            }
        }
    }

    void sort(bool descending) {
        std::sort(levels.begin(), levels.end(), [descending](const LevelData& a, const LevelData& b) {
            return descending ? a.price_fp > b.price_fp : a.price_fp < b.price_fp;
        });
    }

    void flush_to_book_bids(SoABook& book) {
        int n = std::min(int(levels.size()), MAX_BOOK_DEPTH);
        int64_t prices[MAX_BOOK_DEPTH]; int32_t qtys[MAX_BOOK_DEPTH], ords[MAX_BOOK_DEPTH];
        for (int i = 0; i < n; ++i) {
            prices[i] = levels[i].price_fp;
            qtys[i]   = levels[i].qty;
            ords[i]   = levels[i].orders;
        }
        book.replace_bids(prices, qtys, ords, n);
    }

    void flush_to_book_asks(SoABook& book) {
        int n = std::min(int(levels.size()), MAX_BOOK_DEPTH);
        int64_t prices[MAX_BOOK_DEPTH]; int32_t qtys[MAX_BOOK_DEPTH], ords[MAX_BOOK_DEPTH];
        for (int i = 0; i < n; ++i) {
            prices[i] = levels[i].price_fp;
            qtys[i]   = levels[i].qty;
            ords[i]   = levels[i].orders;
        }
        book.replace_asks(prices, qtys, ords, n);
    }
};

class ItchFeedHandler : public IFeedHandler {
public:
    // market_id distinguishes ASX vs SGX vs OSE at the container level
    explicit ItchFeedHandler(MarketID market_id) : market_id_(market_id) {}

    void configure(FeedConfig cfg, BookRegistry& books,
                   SpmcDisruptor<MarketEvent, 65536, 32>& ring) override {
        cfg_   = std::move(cfg);
        books_ = &books;
        ring_  = &ring;
    }

    std::vector<InstrumentInfo> list_instruments() const override {
        return instruments_;
    }

    void add_instrument(const std::string& symbol,
                        int32_t price_exp = -4 /*ASX: 4 dp*/) {
        InstrumentInfo info;
        info.symbol               = Symbol(symbol);
        info.exchange_security_id = symbol_hash(symbol);
        info.price_exponent       = price_exp;
        info.lot_size             = 1;
        info.id                   = books_->register_instrument(symbol);
        instruments_.push_back(info);
        sym_to_id_[info.exchange_security_id] = info.id;

        // Initialise per-instrument level maps
        bid_maps_[info.id] = PriceLevelMap{};
        ask_maps_[info.id] = PriceLevelMap{};
    }

    void start() override {
        running_.store(true, std::memory_order_release);
        recv_thread_  = std::thread(&ItchFeedHandler::recv_loop,  this);
        parse_thread_ = std::thread(&ItchFeedHandler::parse_loop, this);
    }

    void stop() override {
        running_.store(false, std::memory_order_release);
        if (recv_thread_.joinable())  recv_thread_.join();
        if (parse_thread_.joinable()) parse_thread_.join();
    }

    const FeedConfig& config() const override { return cfg_; }
    bool is_running() const noexcept override {
        return running_.load(std::memory_order_relaxed);
    }

    void print_stats() const override {
        std::cout << "[" << cfg_.name << "] pkts=" << stats_.packets
                  << " msgs=" << stats_.messages
                  << " orders=" << stats_.orders
                  << " cancels=" << stats_.cancels
                  << " trades=" << stats_.trades
                  << " gaps=" << stats_.gaps << "\n";
    }

private:
    void recv_loop() noexcept {
        pin_thread_to_cpu(cfg_.recv_cpu);
        set_realtime(85);
        EfViTransport transport;
        transport.open(cfg_.interface_name.c_str(),
                       cfg_.mcast_group_a.c_str(),
                       cfg_.mcast_port_a, cfg_.numa_node);
        while (running_.load(std::memory_order_relaxed)) {
            RecvPacket pkt{};
            if (transport.recv_packet(pkt)) {
                push_raw(pkt.data, pkt.len, pkt.timestamp_ns);
                transport.release_packet(pkt.buf_id);
                ++stats_.packets;
            }
        }
    }

    void push_raw(const uint8_t* data, uint16_t len, uint64_t ts) noexcept {
        RawPacket* slot = spsc_.try_claim();
        if (!slot) { ++stats_.gaps; return; }
        uint16_t n = std::min<uint16_t>(len, sizeof(slot->data));
        std::memcpy(slot->data, data, n);
        slot->len          = n;
        slot->timestamp_ns = ts;
        spsc_.publish();
    }

    void parse_loop() noexcept {
        pin_thread_to_cpu(cfg_.parse_cpu);
        set_realtime(84);
        while (running_.load(std::memory_order_relaxed)) {
            const RawPacket* raw = spsc_.try_peek();
            if (!raw) { spin_pause(); continue; }
            parse_moldudp(raw->data, raw->len, raw->timestamp_ns);
            spsc_.consume();
        }
    }

    // MoldUDP64 framing: Session(10) + SeqNum(8) + MsgCount(2) + [Len(2)+Body]*
    void parse_moldudp(const uint8_t* data, uint16_t len,
                        uint64_t ts_ns) noexcept {
        if (len < 20) return;
        uint64_t seq       = parse::be64(data + 10);
        uint16_t msg_count = parse::be16(data + 18);

        // Gap detection
        if (seq != expected_seq_ && expected_seq_ != 0) ++stats_.gaps;
        expected_seq_ = seq + msg_count;

        uint16_t offset = 20;
        for (uint16_t m = 0; m < msg_count && offset < len; ++m) {
            if (offset + 2 > len) break;
            uint16_t msg_len = parse::be16(data + offset);
            if (offset + 2 + msg_len > len) break;
            const uint8_t* body = data + offset + 2;
            parse_itch_msg(body, msg_len, ts_ns, uint32_t(seq + m));
            offset += 2 + msg_len;
            ++stats_.messages;
        }
    }

    void parse_itch_msg(const uint8_t* b, uint16_t sz,
                         uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz == 0) return;
        char msg_type = char(b[0]);
        switch (msg_type) {
            case 'A': on_add_order   (b + 1, sz - 1, ts_ns, seq); break;
            case 'F': on_add_order_mpid(b + 1, sz - 1, ts_ns, seq); break;
            case 'D': on_delete_order(b + 1, sz - 1, ts_ns, seq); break;
            case 'U': on_replace_order(b+1, sz-1, ts_ns, seq);    break;
            case 'E': on_execute_order(b+1, sz-1, ts_ns, seq, false, 0); break;
            case 'C': on_execute_order_price(b+1, sz-1, ts_ns, seq); break;
            case 'P': on_trade       (b + 1, sz - 1, ts_ns, seq); break;
            case 'H': on_trading_action(b+1, sz-1, ts_ns);        break;
            case 'R': on_stock_directory(b+1, sz-1);              break;
            default:  break;
        }
    }

    // AddOrder: StockLocate(2) Tracking(2) Timestamp(6) OrderRef(8)
    //           BuySell(1) Qty(4) Symbol(8) Price(4)
    void on_add_order(const uint8_t* b, uint16_t sz,
                      uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 33) return;
        uint64_t order_ref   = parse::be64(b + 9);
        char     side_ch     = char(b[17]);
        uint32_t qty_raw     = parse::be32(b + 18);
        char     sym_buf[9]  = {};
        std::memcpy(sym_buf, b + 22, 8);
        uint32_t price_raw   = parse::be32(b + 30);

        InstrumentID id = lookup_symbol(sym_buf);
        if (id == INVALID_INSTRUMENT) return;

        int32_t  exp     = get_exponent(id);
        int64_t  price_fp= parse::raw_to_fp(int32_t(price_raw), exp);
        Side     side    = (side_ch == 'B') ? Side::BUY : Side::SELL;

        // Store order
        OrderEntry oe{};
        oe.order_ref = order_ref;
        oe.price_fp  = price_fp;
        oe.qty       = qty_raw;
        oe.side      = side;
        order_map_[order_ref] = oe;

        // Update price level map
        auto& bmap = bid_maps_[id];
        auto& amap = ask_maps_[id];
        SoABook& book = books_->book(id);

        if (side == Side::BUY) {
            bmap.add_order(price_fp, int32_t(qty_raw), true);
            bmap.flush_to_book_bids(book);
        } else {
            amap.add_order(price_fp, int32_t(qty_raw), false);
            amap.flush_to_book_asks(book);
        }
        book.set_in_sync(true);
        ++stats_.orders;

        emit_event(id, EventType::BookL2, ts_ns, seq,
                   price_fp, int32_t(qty_raw),
                   book.best_ask_fp, book.ask_qtys[0], 0, book.symbol);
    }

    void on_add_order_mpid(const uint8_t* b, uint16_t sz,
                            uint64_t ts_ns, uint32_t seq) noexcept {
        // MPID AddOrder has 4 extra bytes for MPID at the end; delegate to on_add_order
        on_add_order(b, sz, ts_ns, seq);
    }

    // DeleteOrder: StockLocate(2) Tracking(2) Timestamp(6) OrderRef(8) = 18 bytes
    void on_delete_order(const uint8_t* b, uint16_t sz,
                          uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 17) return;
        uint64_t order_ref = parse::be64(b + 9);
        auto it = order_map_.find(order_ref);
        if (it == order_map_.end()) return;
        OrderEntry& oe = it->second;

        InstrumentID id = find_instrument_by_order(oe);
        if (id == INVALID_INSTRUMENT) { order_map_.erase(it); return; }
        SoABook& book = books_->book(id);

        if (oe.side == Side::BUY) {
            bid_maps_[id].remove_order(oe.price_fp, int32_t(oe.qty));
            bid_maps_[id].flush_to_book_bids(book);
        } else {
            ask_maps_[id].remove_order(oe.price_fp, int32_t(oe.qty));
            ask_maps_[id].flush_to_book_asks(book);
        }
        order_map_.erase(it);
        ++stats_.cancels;

        emit_event(id, EventType::BookL2, ts_ns, seq,
                   book.best_bid_fp, book.bid_qtys[0],
                   book.best_ask_fp, book.ask_qtys[0], 0, book.symbol);
    }

    // ReplaceOrder: old_ref(8) + new_ref(8) + qty(4) + price(4)
    void on_replace_order(const uint8_t* b, uint16_t sz,
                           uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 33) return;
        uint64_t old_ref  = parse::be64(b + 9);
        uint64_t new_ref  = parse::be64(b + 17);
        uint32_t new_qty  = parse::be32(b + 25);
        uint32_t new_px   = parse::be32(b + 29);

        auto it = order_map_.find(old_ref);
        if (it == order_map_.end()) return;
        OrderEntry old_oe = it->second;

        // Delete old
        on_delete_order_direct(old_ref, old_oe, ts_ns, seq);

        // Add new (synthesize AddOrder-like data)
        InstrumentID id = find_instrument_for_price(old_oe.price_fp);
        if (id == INVALID_INSTRUMENT) return;
        int32_t  exp     = get_exponent(id);
        int64_t  new_fp  = parse::raw_to_fp(int32_t(new_px), exp);
        SoABook& book    = books_->book(id);

        OrderEntry ne{};
        ne.order_ref = new_ref;
        ne.price_fp  = new_fp;
        ne.qty       = new_qty;
        ne.side      = old_oe.side;
        order_map_[new_ref] = ne;

        if (ne.side == Side::BUY) {
            bid_maps_[id].add_order(new_fp, int32_t(new_qty), true);
            bid_maps_[id].flush_to_book_bids(book);
        } else {
            ask_maps_[id].add_order(new_fp, int32_t(new_qty), false);
            ask_maps_[id].flush_to_book_asks(book);
        }

        emit_event(id, EventType::BookL2, ts_ns, seq,
                   book.best_bid_fp, book.bid_qtys[0],
                   book.best_ask_fp, book.ask_qtys[0], 0, book.symbol);
    }

    void on_delete_order_direct(uint64_t order_ref, const OrderEntry& oe,
                                 uint64_t ts_ns, uint32_t seq) noexcept {
        InstrumentID id = find_instrument_by_order(oe);
        if (id == INVALID_INSTRUMENT) { order_map_.erase(order_ref); return; }
        SoABook& book = books_->book(id);
        if (oe.side == Side::BUY) {
            bid_maps_[id].remove_order(oe.price_fp, int32_t(oe.qty));
            bid_maps_[id].flush_to_book_bids(book);
        } else {
            ask_maps_[id].remove_order(oe.price_fp, int32_t(oe.qty));
            ask_maps_[id].flush_to_book_asks(book);
        }
        order_map_.erase(order_ref);
    }

    // OrderExecuted: order_ref(8) qty(4) match_id(8)
    void on_execute_order(const uint8_t* b, uint16_t sz,
                           uint64_t ts_ns, uint32_t seq,
                           bool with_price, int64_t exec_price_fp) noexcept {
        if (sz < 29) return;
        uint64_t order_ref = parse::be64(b + 9);
        uint32_t exec_qty  = parse::be32(b + 17);

        auto it = order_map_.find(order_ref);
        if (it == order_map_.end()) return;
        OrderEntry& oe = it->second;
        InstrumentID id = find_instrument_by_order(oe);
        if (id == INVALID_INSTRUMENT) return;
        SoABook& book = books_->book(id);

        int64_t trade_px = with_price ? exec_price_fp : oe.price_fp;
        book.update_trade(trade_px, exec_qty, ts_ns);
        ++stats_.trades;

        // Reduce order qty; delete if fully filled
        if (exec_qty >= oe.qty) {
            on_delete_order_direct(order_ref, oe, ts_ns, seq);
        } else {
            if (oe.side == Side::BUY) {
                bid_maps_[id].remove_order(oe.price_fp, int32_t(exec_qty));
                bid_maps_[id].add_order(oe.price_fp, int32_t(oe.qty - exec_qty), true);
                bid_maps_[id].flush_to_book_bids(book);
            } else {
                ask_maps_[id].remove_order(oe.price_fp, int32_t(exec_qty));
                ask_maps_[id].add_order(oe.price_fp, int32_t(oe.qty - exec_qty), false);
                ask_maps_[id].flush_to_book_asks(book);
            }
            oe.qty -= exec_qty;
        }

        emit_event(id, EventType::Trade, ts_ns, seq,
                   trade_px, int32_t(exec_qty),
                   book.best_ask_fp, book.ask_qtys[0], 0, book.symbol);
    }

    void on_execute_order_price(const uint8_t* b, uint16_t sz,
                                 uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 35) return;
        uint64_t order_ref = parse::be64(b + 9);
        auto it = order_map_.find(order_ref);
        if (it == order_map_.end()) return;
        int32_t exp  = get_exponent(find_instrument_by_order(it->second));
        int64_t px   = parse::raw_to_fp(int32_t(parse::be32(b + 31)), exp);
        on_execute_order(b, sz, ts_ns, seq, true, px);
    }

    // Trade (non-displayed cross): similar to AddOrder but just emits trade event
    void on_trade(const uint8_t* b, uint16_t sz,
                  uint64_t ts_ns, uint32_t seq) noexcept {
        if (sz < 33) return;
        uint32_t qty_raw = parse::be32(b + 18);
        char sym_buf[9]  = {};
        std::memcpy(sym_buf, b + 22, 8);
        uint32_t price_raw = parse::be32(b + 30);

        InstrumentID id = lookup_symbol(sym_buf);
        if (id == INVALID_INSTRUMENT) return;
        int32_t exp      = get_exponent(id);
        int64_t price_fp = parse::raw_to_fp(int32_t(price_raw), exp);
        SoABook& book    = books_->book(id);
        book.update_trade(price_fp, qty_raw, ts_ns);
        ++stats_.trades;

        emit_event(id, EventType::Trade, ts_ns, seq,
                   price_fp, int32_t(qty_raw),
                   0, 0, 0, book.symbol);
    }

    // TradingAction: symbol(8) state(1) reason(4)
    void on_trading_action(const uint8_t* b, uint16_t sz,
                            uint64_t ts_ns) noexcept {
        if (sz < 19) return;
        char sym_buf[9] = {};
        std::memcpy(sym_buf, b + 9, 8);
        char state = char(b[17]);  // H=Halt, T=Trading, P=Pause
        InstrumentID id = lookup_symbol(sym_buf);
        if (id == INVALID_INSTRUMENT) return;
        books_->book(id).set_halted(state == 'H' || state == 'P');
        books_->book(id).set_in_sync(state == 'T');
    }

    // StockDirectory: learn about an instrument dynamically
    void on_stock_directory(const uint8_t* b, uint16_t sz) noexcept {
        (void)b; (void)sz;
        // In production: parse instrument spec and register in books_
        // Simplified: instruments are pre-registered via add_instrument()
    }

    __attribute__((hot))
    void emit_event(InstrumentID id, EventType type, uint64_t ts_ns,
                    uint32_t seq, int64_t px1, int32_t qty1,
                    int64_t px2, int32_t qty2, int level,
                    const char* sym) noexcept {
        uint64_t slot_seq = ring_->claim();
        MarketEvent& ev   = ring_->slot(slot_seq);
        ev.timestamp_ns   = ts_ns;
        ev.instrument_id  = id;
        ev.type           = type;
        ev.market_id      = uint8_t(market_id_);
        ev.price_fp       = px1;
        ev.qty            = qty1;
        ev.price2_fp      = px2;
        ev.qty2           = qty2;
        ev.level          = level;
        ev.seq_num        = seq;
        std::strncpy(ev.symbol, sym, 7); ev.symbol[7] = '\0';
        ring_->publish(slot_seq);
    }

    [[nodiscard]] InstrumentID lookup_symbol(const char* sym) const noexcept {
        // Trim trailing spaces (ITCH symbols are left-justified, space-padded)
        char clean[9] = {};
        int n = 8;
        while (n > 0 && (sym[n-1] == ' ' || sym[n-1] == '\0')) --n;
        std::memcpy(clean, sym, n);
        uint64_t h = symbol_hash(clean);
        auto it = sym_to_id_.find(h);
        return (it != sym_to_id_.end()) ? it->second : INVALID_INSTRUMENT;
    }

    [[nodiscard]] InstrumentID find_instrument_by_order(const OrderEntry& oe) const noexcept {
        // Brute-force: iterate instruments to find which one owns this price
        // In production: maintain order_ref→InstrumentID map
        for (auto& info : instruments_) {
            auto bit = bid_maps_.find(info.id);
            if (bit != bid_maps_.end()) {
                for (auto& l : bit->second.levels)
                    if (l.price_fp == oe.price_fp) return info.id;
            }
            auto ait = ask_maps_.find(info.id);
            if (ait != ask_maps_.end()) {
                for (auto& l : ait->second.levels)
                    if (l.price_fp == oe.price_fp) return info.id;
            }
        }
        // Fallback: first registered instrument (simplified)
        return instruments_.empty() ? INVALID_INSTRUMENT : instruments_[0].id;
    }

    [[nodiscard]] InstrumentID find_instrument_for_price(int64_t /*price_fp*/) const noexcept {
        return instruments_.empty() ? INVALID_INSTRUMENT : instruments_[0].id;
    }

    [[nodiscard]] int32_t get_exponent(InstrumentID id) const noexcept {
        for (auto& info : instruments_) if (info.id == id) return info.price_exponent;
        return -4;
    }

    [[nodiscard]] static uint64_t symbol_hash(std::string_view s) noexcept {
        uint64_t h = 14695981039346656037ULL;
        for (char c : s) { h ^= uint8_t(c); h *= 1099511628211ULL; }
        return h;
    }
    [[nodiscard]] static uint64_t symbol_hash(const char* s) noexcept {
        return symbol_hash(std::string_view(s, std::strlen(s)));
    }

    // State
    MarketID                                        market_id_;
    FeedConfig                                      cfg_;
    BookRegistry*                                   books_{nullptr};
    SpmcDisruptor<MarketEvent, 65536, 32>*          ring_{nullptr};
    std::vector<InstrumentInfo>                     instruments_;
    std::unordered_map<uint64_t, InstrumentID>      sym_to_id_;
    std::unordered_map<uint64_t, OrderEntry>        order_map_;
    std::unordered_map<InstrumentID, PriceLevelMap> bid_maps_, ask_maps_;

    SpscRing<RawPacket, 4096>                       spsc_;
    std::thread                                     recv_thread_;
    std::thread                                     parse_thread_;
    std::atomic<bool>                               running_{false};
    uint64_t                                        expected_seq_{0};

    struct Stats {
        uint64_t packets{0}, messages{0}, orders{0}, cancels{0};
        uint64_t trades{0}, gaps{0};
    } stats_;
};

// §D  SGX ITCH — same parser, different market_id and multicast groups
class SgxItchFeedHandler : public ItchFeedHandler {
public:
    SgxItchFeedHandler() : ItchFeedHandler(MarketID::SGX_ITCH) {}
};

// §E  OSE ITCH — same parser, market_id = OSAKA
class OseItchFeedHandler : public ItchFeedHandler {
public:
    OseItchFeedHandler() : ItchFeedHandler(MarketID::OSAKA_ITCH) {}
};

// §F ASX ITCH
class AsxItchFeedHandler : public ItchFeedHandler {
public:
    AsxItchFeedHandler() : ItchFeedHandler(MarketID::ASX_ITCH) {}
};

// ============================================================================
// §F  FEED HANDLER CONTAINER
//
//   - Owns the BookRegistry (NUMA 1 allocation)
//   - Owns the shared SpmcDisruptor ring (NUMA 0/1 interleaved)
//   - Hosts N IFeedHandler instances
//   - Assigns CPU cores from a given base (NUMA 0 side)
//   - Strategy threads register as consumers on the Disruptor
// ============================================================================
class FeedHandlerContainer {
public:
    static constexpr uint32_t RING_CAPACITY = 65536;  // events
    static constexpr uint32_t MAX_HANDLERS  = 16;

    using Ring = SpmcDisruptor<MarketEvent, RING_CAPACITY, 32>;

    explicit FeedHandlerContainer(int base_recv_cpu = 0)
        : next_cpu_(base_recv_cpu) {
        ring_ = std::make_unique<Ring>();
    }

    ~FeedHandlerContainer() { stop_all(); }

    // Add a feed handler. Container takes ownership.
    // Returns the handler ptr for further configuration.
    template<typename Handler>
    Handler* add_handler(std::unique_ptr<Handler> handler,
                          FeedConfig cfg) {
        // Assign recv + parse CPUs (NUMA 0 side)
        cfg.recv_cpu  = next_cpu_++;
        cfg.parse_cpu = next_cpu_++;
        cfg.numa_node = 0;

        // Configure via the derived pointer (virtual dispatch, non-final now).
        Handler* raw = handler.get();
        raw->configure(std::move(cfg), registry_, *ring_);
        // Transfer ownership: Handler* → IFeedHandler* via public inheritance.
        // emplace_back uses direct-init, so the explicit unique_ptr ctor is usable.
        handlers_.emplace_back(handler.release());
        return raw;
    }

    // Register a consumer (strategy thread) on the ring.
    // Returns consumer id (pass to ring consumer API).
    [[nodiscard]] int register_consumer() {
        return ring_->register_consumer();
    }

    void start_all() {
        for (auto& h : handlers_) h->start();
    }

    void stop_all() {
        for (auto& h : handlers_) h->stop();
    }

    [[nodiscard]] Ring& ring() noexcept { return *ring_; }
    [[nodiscard]] BookRegistry& books() noexcept { return registry_; }

    void print_all_stats() const {
        std::cout << "\n===== Feed Handler Container Stats =====\n";
        std::cout << "Handlers: " << handlers_.size() << "\n";
        std::cout << "Instruments: " << registry_.count() << "\n";
        for (auto& h : handlers_) h->print_stats();
        std::cout << "=========================================\n";
    }

private:
    BookRegistry                              registry_;
    std::unique_ptr<Ring>                     ring_;
    std::vector<std::unique_ptr<IFeedHandler>> handlers_;
    int                                       next_cpu_{0};
};

}  // namespace ull

