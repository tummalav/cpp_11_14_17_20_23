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

namespace nasdaq::itch {

// NASDAQ ITCH 5.0 Message Types (Latest specification)
enum class MessageType : uint8_t {
    // System Event Messages
    SYSTEM_EVENT = 'S',
    STOCK_DIRECTORY = 'R',
    STOCK_TRADING_ACTION = 'H',
    REG_SHO_RESTRICTION = 'Y',
    MARKET_PARTICIPANT_POSITION = 'L',
    MWCB_DECLINE_LEVEL = 'V',
    MWCB_STATUS = 'W',
    IPO_QUOTING_PERIOD_UPDATE = 'K',
    LULD_AUCTION_COLLAR = 'J',

    // Order Messages
    ADD_ORDER = 'A',
    ADD_ORDER_WITH_MPID = 'F',
    ORDER_EXECUTED = 'E',
    ORDER_EXECUTED_WITH_PRICE = 'C',
    ORDER_CANCEL = 'X',
    ORDER_DELETE = 'D',
    ORDER_REPLACE = 'U',

    // Trade Messages
    TRADE_NON_CROSS = 'P',
    TRADE_CROSS = 'Q',
    BROKEN_TRADE = 'B',

    // NOII Messages (Net Order Imbalance Indicator)
    NOII = 'I',

    // RPII Messages (Retail Price Improvement Indicator)
    RPII = 'N'
};

// Side enumeration
enum class Side : uint8_t {
    BUY = 'B',
    SELL = 'S'
};

// Market Category
enum class MarketCategory : uint8_t {
    NASDAQ_GLOBAL_SELECT = 'Q',
    NASDAQ_GLOBAL_MARKET = 'G',
    NASDAQ_CAPITAL_MARKET = 'S',
    NYSE = 'N',
    NYSE_MKT = 'A',
    NYSE_ARCA = 'P',
    BATS_Z = 'Z',
    INVESTORS_EXCHANGE = 'V'
};

// Financial Status Indicator
enum class FinancialStatus : uint8_t {
    NORMAL = ' ',
    DEFICIENT = 'D',
    DELINQUENT = 'E',
    BANKRUPT = 'Q',
    SUSPENDED = 'S',
    DEFICIENT_BANKRUPT = 'G',
    DEFICIENT_DELINQUENT = 'H',
    DELINQUENT_BANKRUPT = 'J',
    DEFICIENT_DELINQUENT_BANKRUPT = 'K'
};

// Trading State
enum class TradingState : uint8_t {
    HALTED = 'H',
    PAUSED = 'P',
    QUOTATION_ONLY = 'Q',
    TRADING = 'T'
};

// ITCH Message Header (common to all messages)
struct alignas(8) ITCHMessageHeader {
    uint16_t length;              // Message length
    MessageType message_type;     // Message type
    uint8_t stock_locate;         // Stock locate code
    uint16_t tracking_number;     // Tracking number
    uint64_t timestamp;           // Nanoseconds since midnight
} __attribute__((packed));

// System Event Message
struct alignas(8) SystemEventMessage {
    ITCHMessageHeader header;
    uint8_t event_code;           // Event code (O=Start of Messages, S=Start of System hours, etc.)
} __attribute__((packed));

// Stock Directory Message
struct alignas(8) StockDirectoryMessage {
    ITCHMessageHeader header;
    std::array<char, 8> stock;               // Stock symbol
    MarketCategory market_category;          // Market category
    FinancialStatus financial_status;        // Financial status indicator
    uint32_t round_lot_size;                // Round lot size
    uint8_t round_lots_only;                // Round lots only flag
    uint8_t issue_classification;           // Issue classification
    std::array<char, 2> issue_subtype;     // Issue sub-type
    uint8_t authenticity;                   // Authenticity flag
    uint8_t short_sale_threshold;           // Short sale threshold indicator
    uint8_t ipo_flag;                       // IPO flag
    uint8_t luld_reference_price_tier;      // LULD reference price tier
    uint8_t etp_flag;                       // ETP flag
    uint32_t etp_leverage_factor;           // ETP leverage factor
    uint8_t inverse_indicator;              // Inverse indicator
} __attribute__((packed));

// Stock Trading Action Message
struct alignas(8) StockTradingActionMessage {
    ITCHMessageHeader header;
    std::array<char, 8> stock;               // Stock symbol
    TradingState trading_state;              // Trading state
    std::array<char, 4> reason;             // Reason
} __attribute__((packed));

// Add Order Message
struct alignas(8) AddOrderMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
    Side buy_sell_indicator;                // Buy/sell indicator
    uint32_t shares;                        // Shares
    std::array<char, 8> stock;              // Stock symbol
    uint32_t price;                         // Price (1/10000 of a dollar)
} __attribute__((packed));

// Add Order with MPID Message
struct alignas(8) AddOrderWithMPIDMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
    Side buy_sell_indicator;                // Buy/sell indicator
    uint32_t shares;                        // Shares
    std::array<char, 8> stock;              // Stock symbol
    uint32_t price;                         // Price (1/10000 of a dollar)
    std::array<char, 4> attribution;       // Market participant identifier
} __attribute__((packed));

// Order Executed Message
struct alignas(8) OrderExecutedMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
    uint32_t executed_shares;               // Executed shares
    uint64_t match_number;                  // Match number
} __attribute__((packed));

// Order Executed with Price Message
struct alignas(8) OrderExecutedWithPriceMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
    uint32_t executed_shares;               // Executed shares
    uint64_t match_number;                  // Match number
    uint8_t printable;                      // Printable flag
    uint32_t execution_price;               // Execution price
} __attribute__((packed));

// Order Cancel Message
struct alignas(8) OrderCancelMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
    uint32_t cancelled_shares;              // Cancelled shares
} __attribute__((packed));

// Order Delete Message
struct alignas(8) OrderDeleteMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
} __attribute__((packed));

// Order Replace Message
struct alignas(8) OrderReplaceMessage {
    ITCHMessageHeader header;
    uint64_t original_order_reference_number;  // Original order reference number
    uint64_t new_order_reference_number;       // New order reference number
    uint32_t shares;                           // Shares
    uint32_t price;                            // Price
} __attribute__((packed));

// Trade Message (Non-Cross)
struct alignas(8) TradeMessage {
    ITCHMessageHeader header;
    uint64_t order_reference_number;        // Order reference number
    Side buy_sell_indicator;                // Buy/sell indicator
    uint32_t shares;                        // Shares
    std::array<char, 8> stock;              // Stock symbol
    uint32_t price;                         // Price
    uint64_t match_number;                  // Match number
} __attribute__((packed));

// Cross Trade Message
struct alignas(8) CrossTradeMessage {
    ITCHMessageHeader header;
    uint32_t shares;                        // Shares
    std::array<char, 8> stock;              // Stock symbol
    uint32_t cross_price;                   // Cross price
    uint64_t match_number;                  // Match number
    uint8_t cross_type;                     // Cross type
} __attribute__((packed));

// Broken Trade Message
struct alignas(8) BrokenTradeMessage {
    ITCHMessageHeader header;
    uint64_t match_number;                  // Match number
} __attribute__((packed));

// NOII Message
struct alignas(8) NOIIMessage {
    ITCHMessageHeader header;
    uint64_t paired_shares;                 // Paired shares
    uint64_t imbalance_shares;              // Imbalance shares
    uint8_t imbalance_direction;            // Imbalance direction
    std::array<char, 8> stock;              // Stock symbol
    uint32_t far_price;                     // Far price
    uint32_t near_price;                    // Near price
    uint32_t current_reference_price;       // Current reference price
    uint8_t cross_type;                     // Cross type
    uint8_t price_variation_indicator;      // Price variation indicator
} __attribute__((packed));

// Forward declarations
class ITCHSession;
class OrderBookManager;
class ITCHProcessor;

// ITCH event handler interface
class IITCHEventHandler {
public:
    virtual ~IITCHEventHandler() = default;

    // System events
    virtual void onSystemEvent(const SystemEventMessage& msg) = 0;
    virtual void onStockDirectory(const StockDirectoryMessage& msg) = 0;
    virtual void onStockTradingAction(const StockTradingActionMessage& msg) = 0;

    // Order events
    virtual void onAddOrder(const AddOrderMessage& msg) = 0;
    virtual void onAddOrderWithMPID(const AddOrderWithMPIDMessage& msg) = 0;
    virtual void onOrderExecuted(const OrderExecutedMessage& msg) = 0;
    virtual void onOrderExecutedWithPrice(const OrderExecutedWithPriceMessage& msg) = 0;
    virtual void onOrderCancel(const OrderCancelMessage& msg) = 0;
    virtual void onOrderDelete(const OrderDeleteMessage& msg) = 0;
    virtual void onOrderReplace(const OrderReplaceMessage& msg) = 0;

    // Trade events
    virtual void onTrade(const TradeMessage& msg) = 0;
    virtual void onCrossTrade(const CrossTradeMessage& msg) = 0;
    virtual void onBrokenTrade(const BrokenTradeMessage& msg) = 0;

    // Market data events
    virtual void onNOII(const NOIIMessage& msg) = 0;

    // Connection events
    virtual void onDisconnect(const std::string& reason) = 0;
};

// Price level for order book
struct PriceLevel {
    uint32_t price;      // Price in 1/10000 dollars
    uint64_t shares;     // Total shares at this level
    uint32_t order_count; // Number of orders

    PriceLevel() : price(0), shares(0), order_count(0) {}
    PriceLevel(uint32_t p, uint64_t s, uint32_t c) : price(p), shares(s), order_count(c) {}
};

// Order information
struct OrderInfo {
    uint64_t order_reference_number;
    std::string stock;
    Side side;
    uint32_t original_shares;
    uint32_t remaining_shares;
    uint32_t price;
    std::chrono::high_resolution_clock::time_point add_time;
};

// Order book structure
struct OrderBook {
    std::string stock;
    std::vector<PriceLevel> bid_levels;     // Best bid at index 0
    std::vector<PriceLevel> ask_levels;     // Best ask at index 0
    uint32_t last_trade_price;
    uint32_t last_trade_shares;
    uint64_t total_volume;
    std::chrono::high_resolution_clock::time_point last_update_time;

    OrderBook() : last_trade_price(0), last_trade_shares(0), total_volume(0) {
        bid_levels.reserve(20);  // Top 20 levels
        ask_levels.reserve(20);  // Top 20 levels
    }
};

// Ultra-low latency SPSC Ring Buffer for ITCH messages
template<typename T, size_t Size>
class alignas(64) ITCHSPSCRingBuffer {
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
template<typename T, size_t PoolSize = 4096>
class alignas(64) ITCHMessagePool {
private:
    alignas(64) std::array<T, PoolSize> pool_;
    alignas(64) std::array<std::atomic<bool>, PoolSize> used_;
    std::atomic<size_t> next_index_{0};

public:
    ITCHMessagePool() {
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

// High-precision timestamp utilities for ITCH
class ITCHTimestampUtils {
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

    static inline uint64_t convertITCHTimestamp(uint64_t itch_timestamp) noexcept {
        // ITCH timestamps are nanoseconds since midnight
        // Convert to epoch time if needed
        return itch_timestamp;
    }
};

// Network configuration for ITCH feed
struct ITCHNetworkConfig {
    std::string multicast_ip;
    uint16_t multicast_port;
    std::string interface_ip;
    std::string username;
    std::string password;
    uint32_t receive_buffer_size;
    uint32_t socket_timeout_ms;
    bool enable_timestamping;
    bool enable_packet_filtering;
    bool enable_mold_udp;
};

// Session configuration for ITCH feed
struct ITCHSessionConfig {
    ITCHNetworkConfig network;
    std::string session_id;
    bool enable_order_book_building;
    bool enable_statistics_calculation;
    std::string log_level;
    std::string log_file;
    uint32_t max_order_book_levels;
    bool enable_latency_measurement;
    bool enable_message_recovery;
    uint32_t recovery_timeout_ms;
};

// Plugin interface for ITCH feed handler
class IITCHPlugin {
public:
    virtual ~IITCHPlugin() = default;

    virtual bool initialize(const std::string& config) = 0;
    virtual void shutdown() = 0;
    virtual const char* getPluginName() const = 0;
    virtual const char* getPluginVersion() const = 0;
    virtual bool isReady() const = 0;

    // Session operations
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Subscription operations
    virtual bool subscribe(const std::string& symbol) = 0;
    virtual bool unsubscribe(const std::string& symbol) = 0;
    virtual bool subscribeAll() = 0;
    virtual bool unsubscribeAll() = 0;

    // Order book access
    virtual const OrderBook* getOrderBook(const std::string& symbol) const = 0;
    virtual std::vector<std::string> getSubscribedSymbols() const = 0;

    // Event handler registration
    virtual void registerEventHandler(std::shared_ptr<IITCHEventHandler> handler) = 0;
    virtual void unregisterEventHandler() = 0;

    // Statistics and monitoring
    virtual uint64_t getMessagesReceived() const = 0;
    virtual uint64_t getMessagesProcessed() const = 0;
    virtual uint64_t getOrdersTracked() const = 0;
    virtual uint64_t getTradesProcessed() const = 0;
    virtual double getAverageLatency() const = 0;
    virtual uint64_t getPacketsDropped() const = 0;
};

} // namespace nasdaq::itch
