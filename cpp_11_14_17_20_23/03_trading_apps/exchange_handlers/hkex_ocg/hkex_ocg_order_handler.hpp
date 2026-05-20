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

namespace hkex::ocg {

// HKEX OCG-C Message Types (Latest API v4.9)
enum class MessageType : uint8_t {
    // Inbound Messages (Client to Exchange)
    LOGON_REQUEST = 'A',
    LOGOUT_REQUEST = 'B',
    NEW_ORDER_SINGLE = 'D',
    ORDER_CANCEL_REQUEST = 'F',
    ORDER_REPLACE_REQUEST = 'G',
    ORDER_MASS_CANCEL_REQUEST = 'Q',
    ALLOCATION_INSTRUCTION = 'J',
    BUSINESS_MESSAGE_REJECT = 'j',

    // Outbound Messages (Exchange to Client)
    LOGON_RESPONSE = 'a',
    LOGOUT_RESPONSE = 'b',
    EXECUTION_REPORT = '8',
    ORDER_CANCEL_REJECT = '9',
    BUSINESS_MESSAGE_REJECT_RESPONSE = 'y',
    MASS_CANCEL_REPORT = 'r',
    ALLOCATION_REPORT = 'R'  // Changed from 'AS' which was too large

    // Market Data Messages
    MARKET_DATA_SNAPSHOT = 'W',
    MARKET_DATA_INCREMENTAL = 'X',

    // Administrative Messages
    HEARTBEAT = '0',
    TEST_REQUEST = '1',
    RESEND_REQUEST = '2',
    REJECT = '3',
    SEQUENCE_RESET = '4'
};

// Order Side
enum class Side : uint8_t {
    BUY = '1',
    SELL = '2'
};

// Order Type (HKEX Specific)
enum class OrderType : uint8_t {
    MARKET = '1',
    LIMIT = '2',
    STOP = '3',
    STOP_LIMIT = '4',
    MARKET_ON_CLOSE = '5',
    LIMIT_ON_CLOSE = '6',
    PEGGED = 'P',
    ENHANCED_LIMIT = 'U',
    SPECIAL_LIMIT = 'S'
};

// Time in Force
enum class TimeInForce : uint8_t {
    DAY = '0',
    GOOD_TILL_CANCEL = '1',
    AT_THE_OPENING = '2',
    IMMEDIATE_OR_CANCEL = '3',
    FILL_OR_KILL = '4',
    GOOD_TILL_CROSSING = '5',
    GOOD_TILL_DATE = '6',
    AT_THE_CLOSE = '7'
};

// Execution Type
enum class ExecType : uint8_t {
    NEW = '0',
    PARTIAL_FILL = '1',
    FILL = '2',
    DONE_FOR_DAY = '3',
    CANCELED = '4',
    REPLACED = '5',
    PENDING_CANCEL = '6',
    STOPPED = '7',
    REJECTED = '8',
    SUSPENDED = '9',
    PENDING_NEW = 'A',
    CALCULATED = 'B',
    EXPIRED = 'C',
    RESTATED = 'D',
    PENDING_REPLACE = 'E',
    TRADE = 'F',
    TRADE_CORRECT = 'G',
    TRADE_CANCEL = 'H',
    ORDER_STATUS = 'I'
};

// Order Status
enum class OrderStatus : uint8_t {
    NEW = '0',
    PARTIALLY_FILLED = '1',
    FILLED = '2',
    DONE_FOR_DAY = '3',
    CANCELED = '4',
    REPLACED = '5',
    PENDING_CANCEL = '6',
    STOPPED = '7',
    REJECTED = '8',
    SUSPENDED = '9',
    PENDING_NEW = 'A',
    CALCULATED = 'B',
    EXPIRED = 'C',
    ACCEPTED_FOR_BIDDING = 'D',
    PENDING_REPLACE = 'E'
};

// Market Segments
enum class MarketSegment : uint8_t {
    MAIN_BOARD = 'M',
    GEM = 'G',
    STRUCTURED_PRODUCTS = 'S',
    DEBT_SECURITIES = 'D',
    EXCHANGE_TRADED_FUNDS = 'E',
    REAL_ESTATE_INVESTMENT_TRUSTS = 'R',
    CHINA_CONNECT = 'C'
};

// OCG-C Message Header (Latest Format)
struct alignas(8) MessageHeader {
    uint32_t msg_length;        // Message length including header
    uint8_t msg_type;           // Message type
    uint8_t msg_cat;            // Message category
    uint16_t session_id;        // Session ID
    uint32_t sequence_number;   // Message sequence number
    uint64_t sending_time;      // Nanoseconds since epoch
} __attribute__((packed));

// Logon Request Message
struct alignas(8) LogonRequest {
    MessageHeader header;
    std::array<char, 16> username;
    std::array<char, 16> password;
    std::array<char, 8> firm_id;
    std::array<char, 4> trading_session_id;
    uint32_t heartbeat_interval;  // Seconds
    uint8_t reset_seq_num_flag;
    std::array<char, 32> client_id;
    uint8_t encryption_method;
    std::array<char, 64> raw_data;
} __attribute__((packed));

// New Order Single Message
struct alignas(8) NewOrderSingle {
    MessageHeader header;
    std::array<char, 20> cl_ord_id;      // Client Order ID
    std::array<char, 12> security_id;     // Security ID (Stock Code)
    std::array<char, 4> security_id_source; // Security ID Source
    std::array<char, 3> symbol;           // Stock Symbol
    Side side;                            // Buy/Sell
    uint64_t order_qty;                   // Order Quantity
    OrderType ord_type;                   // Order Type
    uint64_t price;                       // Price in ticks
    TimeInForce time_in_force;            // Time in Force
    std::array<char, 8> account;          // Account
    std::array<char, 16> investor_id;     // Investor ID
    uint8_t capacity;                     // Order Capacity
    uint64_t min_qty;                     // Minimum Quantity
    uint64_t max_floor;                   // Max Floor
    std::array<char, 32> text;            // Free format text
    uint64_t transact_time;               // Transaction Time
    MarketSegment market_segment;         // Market Segment
    uint8_t price_type;                   // Price Type
    std::array<char, 8> order_restrictions; // Order Restrictions
    uint8_t disclosed_qty;                // Disclosed Quantity
    std::array<char, 16> party_id;        // Party ID
} __attribute__((packed));

// Order Cancel Request
struct alignas(8) OrderCancelRequest {
    MessageHeader header;
    std::array<char, 20> orig_cl_ord_id;  // Original Client Order ID
    std::array<char, 20> cl_ord_id;       // Client Order ID
    std::array<char, 12> security_id;     // Security ID
    Side side;                            // Side
    uint64_t transact_time;               // Transaction Time
    std::array<char, 32> text;            // Text
} __attribute__((packed));

// Order Replace Request
struct alignas(8) OrderReplaceRequest {
    MessageHeader header;
    std::array<char, 20> orig_cl_ord_id;  // Original Client Order ID
    std::array<char, 20> cl_ord_id;       // New Client Order ID
    std::array<char, 12> security_id;     // Security ID
    Side side;                            // Side
    uint64_t order_qty;                   // Order Quantity
    OrderType ord_type;                   // Order Type
    uint64_t price;                       // Price
    TimeInForce time_in_force;            // Time in Force
    std::array<char, 8> account;          // Account
    uint64_t transact_time;               // Transaction Time
    uint64_t min_qty;                     // Minimum Quantity
    uint64_t max_floor;                   // Max Floor
    std::array<char, 32> text;            // Text
} __attribute__((packed));

// Execution Report Message
struct alignas(8) ExecutionReport {
    MessageHeader header;
    std::array<char, 20> order_id;        // Order ID
    std::array<char, 20> cl_ord_id;       // Client Order ID
    std::array<char, 20> orig_cl_ord_id;  // Original Client Order ID
    std::array<char, 20> exec_id;         // Execution ID
    ExecType exec_type;                   // Execution Type
    OrderStatus ord_status;               // Order Status
    std::array<char, 12> security_id;     // Security ID
    std::array<char, 3> symbol;           // Symbol
    Side side;                            // Side
    uint64_t order_qty;                   // Order Quantity
    uint64_t last_qty;                    // Last Quantity
    uint64_t last_px;                     // Last Price
    uint64_t leaves_qty;                  // Leaves Quantity
    uint64_t cum_qty;                     // Cumulative Quantity
    uint64_t avg_px;                      // Average Price
    uint64_t transact_time;               // Transaction Time
    std::array<char, 32> text;            // Text
    std::array<char, 8> last_mkt;         // Last Market
    uint64_t commission;                  // Commission
    uint8_t comm_type;                    // Commission Type
    uint64_t gross_trade_amt;             // Gross Trade Amount
} __attribute__((packed));

// Order Cancel Reject
struct alignas(8) OrderCancelReject {
    MessageHeader header;
    std::array<char, 20> order_id;        // Order ID
    std::array<char, 20> cl_ord_id;       // Client Order ID
    std::array<char, 20> orig_cl_ord_id;  // Original Client Order ID
    OrderStatus ord_status;               // Order Status
    uint8_t cxl_rej_reason;              // Cancel Reject Reason
    std::array<char, 32> text;            // Text
    uint64_t transact_time;               // Transaction Time
} __attribute__((packed));

// Forward declarations
class OCGSession;
class OrderManager;

// Event handler interface
class IOCGEventHandler {
public:
    virtual ~IOCGEventHandler() = default;

    virtual void onLogonResponse(bool success, const std::string& reason) = 0;
    virtual void onExecutionReport(const ExecutionReport& exec_report) = 0;
    virtual void onOrderCancelReject(const OrderCancelReject& cancel_reject) = 0;
    virtual void onBusinessReject(const std::string& reason) = 0;
    virtual void onDisconnect(const std::string& reason) = 0;
    virtual void onHeartbeat() = 0;
};

// Order tracking structure
struct OrderInfo {
    std::array<char, 20> cl_ord_id;
    std::array<char, 20> order_id;
    std::array<char, 12> security_id;
    Side side;
    uint64_t original_qty;
    uint64_t remaining_qty;
    uint64_t executed_qty;
    uint64_t price;
    OrderStatus status;
    std::chrono::high_resolution_clock::time_point submit_time;
    std::chrono::high_resolution_clock::time_point last_update_time;
    uint64_t avg_px;
    uint64_t cum_qty;
};

// Ultra-low latency SPSC Ring Buffer
template<typename T, size_t Size>
class alignas(64) SPSCRingBuffer {
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
template<typename T, size_t PoolSize = 1024>
class alignas(64) MessagePool {
private:
    alignas(64) std::array<T, PoolSize> pool_;
    alignas(64) std::array<std::atomic<bool>, PoolSize> used_;
    std::atomic<size_t> next_index_{0};

public:
    MessagePool() {
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

// High-precision timestamp utilities
class TimestampUtils {
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

    static inline void convertTSCToNanos([[maybe_unused]] uint64_t tsc, [[maybe_unused]] double tsc_frequency) noexcept {
        // Convert TSC cycles to nanoseconds
        // tsc_frequency should be calibrated at startup
    }
};

// Network configuration
struct NetworkConfig {
    std::string primary_ip;
    uint16_t primary_port;
    std::string backup_ip;
    uint16_t backup_port;
    std::string local_ip;
    uint16_t local_port;
    std::string username;
    std::string password;
    std::string firm_id;
    std::string client_id;
    bool enable_heartbeat;
    uint32_t heartbeat_interval_ms;
    uint32_t connect_timeout_ms;
    uint32_t read_timeout_ms;
    bool enable_nagle;
    int tcp_nodelay;
    int so_rcvbuf_size;
    int so_sndbuf_size;
    bool enable_quick_ack;
    bool enable_tcp_user_timeout;
    uint32_t tcp_user_timeout_ms;
};

// Session configuration
struct SessionConfig {
    NetworkConfig network;
    std::string trading_session_id;
    bool enable_order_tracking;
    uint32_t max_orders_per_second;
    uint32_t max_pending_orders;
    bool enable_latency_tracking;
    std::string log_level;
    std::string log_file;
    bool enable_failover;
    uint32_t failover_timeout_ms;
    bool enable_compression;
    uint8_t compression_level;
};

// Plugin interface
class IOCGPlugin {
public:
    virtual ~IOCGPlugin() = default;

    virtual bool initialize(const std::string& config) = 0;
    virtual void shutdown() = 0;
    virtual const char* getPluginName() const = 0;
    virtual const char* getPluginVersion() const = 0;
    virtual bool isReady() const = 0;

    // Session operations
    virtual bool login() = 0;
    virtual bool logout() = 0;
    virtual bool isLoggedIn() const = 0;

    // Order operations
    virtual bool sendNewOrder(const NewOrderSingle& order) = 0;
    virtual bool sendCancelOrder(const OrderCancelRequest& cancel) = 0;
    virtual bool sendReplaceOrder(const OrderReplaceRequest& replace) = 0;

    // Event handler registration
    virtual void registerEventHandler(std::shared_ptr<IOCGEventHandler> handler) = 0;
    virtual void unregisterEventHandler() = 0;

    // Statistics and monitoring
    virtual uint64_t getOrdersSent() const = 0;
    virtual uint64_t getOrdersAccepted() const = 0;
    virtual uint64_t getOrdersRejected() const = 0;
    virtual uint64_t getExecutions() const = 0;
    virtual double getAverageLatency() const = 0;
    virtual uint64_t getHeartbeatsSent() const = 0;
    virtual uint64_t getHeartbeatsReceived() const = 0;
};

} // namespace hkex::ocg
