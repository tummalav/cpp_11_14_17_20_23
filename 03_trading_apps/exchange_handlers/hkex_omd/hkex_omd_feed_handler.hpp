#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <memory>
#include <functional>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <thread>
#include <vector>
#include <mutex>

namespace hkex::omd {

// HKEX OMD Message Types (Latest Optiq Market Data v3.5)
enum class MessageType : uint16_t {
    // Level 1 Market Data
    SEQUENCE_RESET = 1,
    LOGON = 100,
    LOGOUT = 101,

    // Market Data Messages
    ADD_ORDER = 30,
    MODIFY_ORDER = 31,
    DELETE_ORDER = 32,
    ADD_ODD_LOT_ORDER = 33,
    DELETE_ODD_LOT_ORDER = 34,

    // Trade Messages
    TRADE = 40,
    TRADE_CANCEL = 41,
    TRADE_TICKER = 42,

    // Market Status
    MARKET_DEFINITION = 50,
    SECURITY_DEFINITION = 51,
    SECURITY_STATUS = 52,

    // Statistics
    STATISTICS = 60,
    MARKET_TURNOVER = 61,

    // Index Data
    INDEX_DEFINITION = 70,
    INDEX_DATA = 71,

    // News and Corporate Actions
    NEWS = 80,

    // Closing Price
    CLOSING_PRICE = 90,

    // VCM (Volatility Control Mechanism)
    VCM_TRIGGER = 95,

    // Heartbeat
    HEARTBEAT = 999
};

// Order Side
enum class Side : uint8_t {
    BUY = 1,
    SELL = 2
};

// Order Type
enum class OrderType : uint8_t {
    MARKET = 1,
    LIMIT = 2,
    ENHANCED_LIMIT = 3,
    SPECIAL_LIMIT = 4,
    AT_AUCTION = 5
};

// Market Phase
enum class MarketPhase : uint8_t {
    PRE_OPENING = 1,
    OPENING_AUCTION = 2,
    CONTINUOUS_TRADING = 3,
    CLOSING_AUCTION = 4,
    POST_CLOSING = 5,
    HALT = 6,
    SUSPEND = 7
};

// Security Type
enum class SecurityType : uint8_t {
    EQUITY = 1,
    WARRANT = 2,
    CBBC = 3,
    ETF = 4,
    REIT = 5,
    BOND = 6,
    STRUCTURED_PRODUCT = 7
};

// Market Segment
enum class MarketSegment : uint8_t {
    MAIN_BOARD = 1,
    GEM = 2,
    STRUCTURED_PRODUCTS = 3,
    DEBT_SECURITIES = 4,
    EXCHANGE_TRADED_FUNDS = 5,
    REAL_ESTATE_INVESTMENT_TRUSTS = 6
};

// OMD Packet Header
struct alignas(8) PacketHeader {
    uint16_t packet_size;
    uint8_t msg_count;
    uint8_t filler;
    uint32_t seq_num;
    uint64_t send_time;  // Nanoseconds since epoch
} __attribute__((packed));

// OMD Message Header
struct alignas(8) MessageHeader {
    uint16_t msg_size;
    uint16_t msg_type;
    uint32_t security_code;
    uint64_t msg_seq_num;
    uint64_t send_time;  // Nanoseconds since epoch
} __attribute__((packed));

// Add Order Message
struct alignas(8) AddOrderMessage {
    MessageHeader header;
    uint64_t order_id;
    uint64_t price;           // Price in sub-units
    uint64_t quantity;        // Quantity in shares
    Side side;                // Buy/Sell
    OrderType order_type;     // Order type
    uint8_t order_book_type;  // 1=Order Book, 2=Odd Lot
    uint8_t filler[5];
} __attribute__((packed));

// Modify Order Message
struct alignas(8) ModifyOrderMessage {
    MessageHeader header;
    uint64_t order_id;
    uint64_t new_price;       // New price in sub-units
    uint64_t new_quantity;    // New quantity in shares
    Side side;                // Buy/Sell
    OrderType order_type;     // Order type
    uint8_t filler[6];
} __attribute__((packed));

// Delete Order Message
struct alignas(8) DeleteOrderMessage {
    MessageHeader header;
    uint64_t order_id;
    Side side;                // Buy/Sell
    uint8_t filler[7];
} __attribute__((packed));

// Trade Message
struct alignas(8) TradeMessage {
    MessageHeader header;
    uint64_t trade_id;
    uint64_t price;           // Trade price in sub-units
    uint64_t quantity;        // Trade quantity in shares
    uint64_t buyer_order_id;  // Buyer order ID
    uint64_t seller_order_id; // Seller order ID
    uint8_t trade_type;       // 1=Auction, 2=Continuous
    uint8_t filler[7];
} __attribute__((packed));

// Trade Cancel Message
struct alignas(8) TradeCancelMessage {
    MessageHeader header;
    uint64_t trade_id;
    uint64_t price;           // Original trade price
    uint64_t quantity;        // Original trade quantity
    uint8_t filler[8];
} __attribute__((packed));

// Security Definition Message
struct alignas(8) SecurityDefinitionMessage {
    MessageHeader header;
    std::array<char, 12> symbol;          // Security symbol
    std::array<char, 40> name_eng;        // English name
    std::array<char, 40> name_chi;        // Chinese name
    std::array<char, 4> currency;         // Currency code
    SecurityType security_type;           // Security type
    MarketSegment market_segment;         // Market segment
    uint32_t lot_size;                    // Board lot size
    uint64_t price_sub_units;             // Price decimal places
    uint64_t nominal_value;               // Nominal value
    uint8_t filler[8];
} __attribute__((packed));

// Security Status Message
struct alignas(8) SecurityStatusMessage {
    MessageHeader header;
    MarketPhase suspend_resume_reason;    // Market phase
    uint8_t filler[7];
} __attribute__((packed));

// Statistics Message
struct alignas(8) StatisticsMessage {
    MessageHeader header;
    uint64_t shares_traded;               // Total shares traded
    uint64_t turnover;                    // Total turnover
    uint64_t high_price;                  // Day high price
    uint64_t low_price;                   // Day low price
    uint64_t last_price;                  // Last trade price
    uint64_t vwap;                        // Volume weighted average price
    uint64_t shortable_shares;            // Available for short selling
    uint8_t filler[8];
} __attribute__((packed));

// Index Data Message
struct alignas(8) IndexDataMessage {
    MessageHeader header;
    std::array<char, 12> index_code;      // Index code
    uint64_t index_value;                 // Index value (scaled)
    uint64_t net_change;                  // Net change from previous close
    uint64_t percentage_change;           // Percentage change (scaled)
    uint8_t filler[8];
} __attribute__((packed));

// Market Turnover Message
struct alignas(8) MarketTurnoverMessage {
    MessageHeader header;
    MarketSegment market_segment;         // Market segment
    std::array<char, 4> currency;         // Currency
    uint64_t turnover;                    // Market turnover
    uint8_t filler[7];
} __attribute__((packed));

// Forward declarations
class OMDSession;
class OrderBookManager;
class MarketDataProcessor;

// Market data callback interface
class IOMDEventHandler {
public:
    virtual ~IOMDEventHandler() = default;

    // Order book events
    virtual void onAddOrder(const AddOrderMessage& msg) = 0;
    virtual void onModifyOrder(const ModifyOrderMessage& msg) = 0;
    virtual void onDeleteOrder(const DeleteOrderMessage& msg) = 0;

    // Trade events
    virtual void onTrade(const TradeMessage& msg) = 0;
    virtual void onTradeCancel(const TradeCancelMessage& msg) = 0;

    // Reference data events
    virtual void onSecurityDefinition(const SecurityDefinitionMessage& msg) = 0;
    virtual void onSecurityStatus(const SecurityStatusMessage& msg) = 0;

    // Statistics events
    virtual void onStatistics(const StatisticsMessage& msg) = 0;
    virtual void onIndexData(const IndexDataMessage& msg) = 0;
    virtual void onMarketTurnover(const MarketTurnoverMessage& msg) = 0;

    // Session events
    virtual void onHeartbeat() = 0;
    virtual void onSequenceReset(uint32_t new_seq_num) = 0;
    virtual void onDisconnect(const std::string& reason) = 0;
};

// Price level structure for order book
struct PriceLevel {
    uint64_t price;
    uint64_t quantity;
    uint32_t order_count;

    PriceLevel() : price(0), quantity(0), order_count(0) {}
    PriceLevel(uint64_t p, uint64_t q, uint32_t c) : price(p), quantity(q), order_count(c) {}
};

// Order book structure
struct OrderBook {
    uint32_t security_code;
    std::vector<PriceLevel> bid_levels;   // Best bid at index 0
    std::vector<PriceLevel> ask_levels;   // Best ask at index 0
    uint64_t last_trade_price;
    uint64_t last_trade_quantity;
    uint64_t total_volume;
    uint64_t total_turnover;
    std::chrono::high_resolution_clock::time_point last_update_time;

    OrderBook() : security_code(0), last_trade_price(0), last_trade_quantity(0),
                  total_volume(0), total_turnover(0) {
        bid_levels.reserve(10);  // Top 10 levels
        ask_levels.reserve(10);  // Top 10 levels
    }
};

// Ultra-low latency SPSC Ring Buffer (specialized for market data)
template<typename T, size_t Size>
class alignas(64) MDSPSCRingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

private:
    alignas(64) std::array<T, Size> buffer_;
    alignas(64) std::atomic<uint64_t> write_index_{0};
    alignas(64) std::atomic<uint64_t> read_index_{0};

public:
    bool try_push(const T& item) noexcept {
        const uint64_t current_write = write_index_.load(std::memory_order_relaxed);
        const uint64_t next_write = current_write + 1;

        if ((next_write & (Size - 1)) == (read_index_.load(std::memory_order_acquire) & (Size - 1))) {
            return false; // Buffer full
        }

        buffer_[current_write & (Size - 1)] = item;
        write_index_.store(next_write, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) noexcept {
        const uint64_t current_read = read_index_.load(std::memory_order_relaxed);

        if (current_read == write_index_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }

        item = buffer_[current_read & (Size - 1)];
        read_index_.store(current_read + 1, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return read_index_.load(std::memory_order_acquire) ==
               write_index_.load(std::memory_order_acquire);
    }

    size_t size() const noexcept {
        return write_index_.load(std::memory_order_acquire) -
               read_index_.load(std::memory_order_acquire);
    }
};

// Memory pool for zero-allocation message handling
template<typename T, size_t PoolSize = 8192>
class alignas(64) MDMessagePool {
private:
    alignas(64) std::array<T, PoolSize> pool_;
    alignas(64) std::array<std::atomic<bool>, PoolSize> used_;
    std::atomic<size_t> next_index_{0};

public:
    MDMessagePool() {
        for (auto& flag : used_) {
            flag.store(false, std::memory_order_relaxed);
        }
    }

    T* acquire() noexcept {
        size_t start_index = next_index_.load(std::memory_order_relaxed);

        for (size_t i = 0; i < PoolSize; ++i) {
            size_t index = (start_index + i) % PoolSize;
            bool expected = false;

            if (used_[index].compare_exchange_weak(expected, true,
                                                  std::memory_order_acquire)) {
                next_index_.store((index + 1) % PoolSize, std::memory_order_relaxed);
                return &pool_[index];
            }
        }

        return nullptr; // Pool exhausted
    }

    void release(T* ptr) noexcept {
        if (!ptr) return;

        size_t index = ptr - pool_.data();
        if (index < PoolSize) {
            used_[index].store(false, std::memory_order_release);
        }
    }
};

// High-precision timestamp utilities for market data
class MDTimestampUtils {
public:
    static inline uint64_t getFastTimestamp() noexcept {
#ifdef __x86_64__
        uint32_t hi, lo;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
#else
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
    }

    static inline uint64_t getNanosecondTimestamp() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    static inline double convertTSCToNanos([[maybe_unused]] uint64_t tsc, [[maybe_unused]] double tsc_frequency) noexcept {
        // Convert TSC cycles to nanoseconds
        // tsc_frequency should be calibrated at startup
        return static_cast<double>(tsc) / tsc_frequency * 1000000000.0;
    }
};

// Network configuration for market data
struct MDNetworkConfig {
    std::string multicast_ip;
    uint16_t multicast_port;
    std::string interface_ip;
    std::string retransmission_ip;
    uint16_t retransmission_port;
    bool enable_retransmission;
    uint32_t receive_buffer_size;
    uint32_t socket_timeout_ms;
    bool enable_timestamping;
    bool enable_packet_filtering;
};

// Session configuration for market data
struct MDSessionConfig {
    MDNetworkConfig network;
    std::string session_id;
    uint32_t max_gap_fill_requests;
    uint32_t heartbeat_interval_ms;
    bool enable_sequence_checking;
    bool enable_market_data_replay;
    bool enable_order_book_building;
    bool enable_statistics_calculation;
    std::string log_level;
    std::string log_file;
    uint32_t max_order_book_levels;
    bool enable_latency_measurement;
};

// Plugin interface for OMD market data handler
class IOMDPlugin {
public:
    virtual ~IOMDPlugin() = default;

    virtual bool initialize(const std::string& config) = 0;
    virtual void shutdown() = 0;
    virtual const char* getPluginName() const = 0;
    virtual const char* getPluginVersion() const = 0;
    virtual bool isReady() const = 0;

    // Session operations
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Market data operations
    virtual bool subscribe(uint32_t security_code) = 0;
    virtual bool unsubscribe(uint32_t security_code) = 0;
    virtual bool subscribeAll() = 0;
    virtual bool unsubscribeAll() = 0;

    // Order book access
    virtual const OrderBook* getOrderBook(uint32_t security_code) const = 0;
    virtual std::vector<uint32_t> getSubscribedSecurities() const = 0;

    // Event handler registration
    virtual void registerEventHandler(std::shared_ptr<IOMDEventHandler> handler) = 0;
    virtual void unregisterEventHandler() = 0;

    // Statistics and monitoring
    virtual uint64_t getMessagesReceived() const = 0;
    virtual uint64_t getMessagesProcessed() const = 0;
    virtual uint64_t getSequenceErrors() const = 0;
    virtual uint64_t getPacketsDropped() const = 0;
    virtual double getAverageLatency() const = 0;
    virtual uint64_t getHeartbeatsReceived() const = 0;
    virtual uint32_t getCurrentSequenceNumber() const = 0;
};

} // namespace hkex::omd
