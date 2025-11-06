#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <memory>
#include <functional>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace asx::ouch {

// OUCH Message Types for ASX
enum class MessageType : uint8_t {
    // Inbound Messages (from client to exchange)
    ENTER_ORDER = 'O',
    REPLACE_ORDER = 'U',
    CANCEL_ORDER = 'X',

    // Outbound Messages (from exchange to client)
    ORDER_ACCEPTED = 'A',
    ORDER_REPLACED = 'U',
    ORDER_CANCELED = 'C',
    ORDER_EXECUTED = 'E',
    ORDER_REJECTED = 'J',
    BROKEN_TRADE = 'B',
    PRICE_TICK = 'P'
};

// Order Side
enum class Side : uint8_t {
    BUY = 'B',
    SELL = 'S'
};

// Order Type
enum class OrderType : uint8_t {
    LIMIT = 'L',
    MARKET = 'M'
};

// Time in Force
enum class TimeInForce : uint8_t {
    DAY = 'D',
    IOC = 'I',  // Immediate or Cancel
    FOK = 'F',  // Fill or Kill
    GTC = 'G'   // Good Till Cancel
};

// Order State
enum class OrderState : uint8_t {
    PENDING_NEW,
    ACCEPTED,
    PARTIALLY_FILLED,
    FILLED,
    CANCELED,
    REJECTED,
    PENDING_CANCEL,
    PENDING_REPLACE
};

// Base OUCH Message Header
struct __attribute__((packed)) MessageHeader {
    uint16_t length;
    uint8_t message_type;
    uint64_t timestamp;  // Nanoseconds since epoch
};

// Enter Order Message
struct __attribute__((packed)) EnterOrderMessage {
    MessageHeader header;
    std::array<char, 14> order_token;
    Side side;
    uint32_t quantity;
    std::array<char, 8> instrument;
    uint64_t price;  // Price in ticks
    TimeInForce time_in_force;
    std::array<char, 4> firm;
    uint8_t display;
    uint64_t capacity;
    uint64_t minimum_quantity;
    uint8_t cross_trade_prevention;
};

// Replace Order Message
struct __attribute__((packed)) ReplaceOrderMessage {
    MessageHeader header;
    std::array<char, 14> existing_order_token;
    std::array<char, 14> replacement_order_token;
    uint32_t quantity;
    uint64_t price;
    uint8_t display;
    uint64_t minimum_quantity;
};

// Cancel Order Message
struct __attribute__((packed)) CancelOrderMessage {
    MessageHeader header;
    std::array<char, 14> order_token;
    uint32_t quantity;
};

// Order Accepted Message
struct __attribute__((packed)) OrderAcceptedMessage {
    MessageHeader header;
    std::array<char, 14> order_token;
    Side side;
    uint32_t quantity;
    std::array<char, 8> instrument;
    uint64_t price;
    TimeInForce time_in_force;
    std::array<char, 4> firm;
    uint8_t display;
    uint64_t order_reference_number;
    uint64_t capacity;
    uint64_t minimum_quantity;
    uint8_t cross_trade_prevention;
    uint8_t order_state;
};

// Order Executed Message
struct __attribute__((packed)) OrderExecutedMessage {
    MessageHeader header;
    std::array<char, 14> order_token;
    uint32_t executed_quantity;
    uint64_t execution_price;
    uint64_t liquidity_flag;
    uint64_t match_number;
};

// Order Rejected Message
struct __attribute__((packed)) OrderRejectedMessage {
    MessageHeader header;
    std::array<char, 14> order_token;
    uint8_t reject_reason;
};

// Forward declarations
class OUCHSession;
class OrderManager;

// Callback interface for order events
class IOrderEventHandler {
public:
    virtual ~IOrderEventHandler() = default;

    virtual void onOrderAccepted(const OrderAcceptedMessage& msg) = 0;
    virtual void onOrderExecuted(const OrderExecutedMessage& msg) = 0;
    virtual void onOrderRejected(const OrderRejectedMessage& msg) = 0;
    virtual void onOrderCanceled(const std::array<char, 14>& order_token) = 0;
    virtual void onOrderReplaced(const std::array<char, 14>& old_token,
                                const std::array<char, 14>& new_token) = 0;
    virtual void onBrokenTrade(uint64_t match_number) = 0;
};

// Order tracking structure
struct OrderInfo {
    std::array<char, 14> order_token;
    std::array<char, 8> instrument;
    Side side;
    uint32_t original_quantity;
    uint32_t remaining_quantity;
    uint32_t executed_quantity;
    uint64_t price;
    OrderState state;
    uint64_t order_reference_number;
    std::chrono::high_resolution_clock::time_point submit_time;
    std::chrono::high_resolution_clock::time_point last_update_time;
};

// Plugin interface for OUCH handlers
class IOUCHPlugin {
public:
    virtual ~IOUCHPlugin() = default;

    virtual bool initialize(const std::string& config) = 0;
    virtual void shutdown() = 0;
    virtual const char* getPluginName() const = 0;
    virtual const char* getPluginVersion() const = 0;
    virtual bool isReady() const = 0;

    // Order operations
    virtual bool sendEnterOrder(const EnterOrderMessage& order) = 0;
    virtual bool sendReplaceOrder(const ReplaceOrderMessage& replace) = 0;
    virtual bool sendCancelOrder(const CancelOrderMessage& cancel) = 0;

    // Event handler registration
    virtual void registerEventHandler(std::shared_ptr<IOrderEventHandler> handler) = 0;
    virtual void unregisterEventHandler() = 0;

    // Statistics and monitoring
    virtual uint64_t getOrdersSent() const = 0;
    virtual uint64_t getOrdersAccepted() const = 0;
    virtual uint64_t getOrdersRejected() const = 0;
    virtual uint64_t getExecutions() const = 0;
    virtual double getAverageLatency() const = 0;
};

// High-performance memory pool for message allocation
template<typename T, size_t PoolSize = 1024>
class MessagePool {
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

// Network configuration
struct NetworkConfig {
    std::string server_ip;
    uint16_t server_port;
    std::string local_ip;
    uint16_t local_port;
    std::string username;
    std::string password;
    std::string session_id;
    bool enable_heartbeat;
    uint32_t heartbeat_interval_ms;
    uint32_t connect_timeout_ms;
    uint32_t read_timeout_ms;
    bool enable_nagle;
    int tcp_nodelay;
    int so_rcvbuf_size;
    int so_sndbuf_size;
};

// Session configuration
struct SessionConfig {
    NetworkConfig network;
    std::string firm_id;
    bool enable_order_tracking;
    uint32_t max_orders_per_second;
    uint32_t max_pending_orders;
    bool enable_latency_tracking;
    std::string log_level;
    std::string log_file;
};

} // namespace asx::ouch
