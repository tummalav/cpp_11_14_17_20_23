#include <iostream>
#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <string>
#include <chrono>
#include <algorithm>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <cassert>
#include <cstring>
#include <immintrin.h>

// Platform-specific includes
#ifdef __linux__
    #include <sched.h>
    #include <sys/mman.h>
    #include <sys/shm.h>
#elif defined(__APPLE__)
    #include <pthread.h>
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#endif

/*
 * ULTRA-LOW LATENCY TRADING PIPELINE
 * ==================================
 * Complete end-to-end trading system including:
 * - Execution Engine with Crossing Engine
 * - Order Management System (OMS)
 * - Smart Order Router (SOR)
 * - Real-time Market Data Feed Handler
 * - Risk/Compliance Engine
 * - Multi-protocol Exchange Connectivity (FIX, OUCH, ITCH, OMNet, Binary)
 * - Pluggable Exchange Handlers
 * - Monolithic or Microservices Architecture
 * - Shared Memory or Message Bus Communication
 * - Ultra-low latency optimizations
 */

namespace trading_pipeline {

// =============================================================================
// CORE CONSTANTS AND CONFIGURATION
// =============================================================================

static constexpr size_t CACHE_LINE_SIZE = 64;
static constexpr size_t MAX_ORDERS = 100000;
static constexpr size_t MAX_VENUES = 32;
static constexpr size_t MAX_INSTRUMENTS = 10000;
static constexpr size_t MAX_CLIENTS = 1000;
static constexpr size_t MEMORY_POOL_SIZE = 64 * 1024 * 1024; // 64MB
static constexpr size_t SHARED_MEMORY_SIZE = 128 * 1024 * 1024; // 128MB
static constexpr size_t MAX_MESSAGE_SIZE = 4096;
static constexpr size_t RING_BUFFER_SIZE = 65536; // Must be power of 2

// Performance configuration
struct SystemConfig {
    // CPU Configuration
    int market_data_cpu = 2;
    int execution_cpu = 4;
    int risk_cpu = 6;
    int oms_cpu = 8;
    int sor_cpu = 10;
    int exchange_cpu_base = 12; // Base CPU for exchange handlers

    // Memory Configuration
    bool use_shared_memory = true;
    bool use_huge_pages = true;
    size_t shared_mem_size = SHARED_MEMORY_SIZE;

    // Threading Configuration
    bool pin_threads = true;
    bool isolate_cpus = true;
    int thread_priority = 99; // Real-time priority

    // Communication Configuration
    enum class CommType {
        SHARED_MEMORY,
        MESSAGE_BUS,
        DIRECT_FUNCTION_CALLS
    } communication_type = CommType::SHARED_MEMORY;

    // Architecture Configuration
    enum class ArchType {
        MONOLITHIC,
        MICROSERVICES,
        HYBRID
    } architecture_type = ArchType::MONOLITHIC;
};

// =============================================================================
// HIGH-PERFORMANCE DATA TYPES
// =============================================================================

using OrderId = uint64_t;
using ClientId = uint32_t;
using InstrumentId = uint32_t;
using VenueId = uint16_t;
using Price = int64_t;  // Fixed point (multiply by 10000 for 4 decimal places)
using Quantity = uint64_t;
using Timestamp = uint64_t; // Nanoseconds since epoch

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    MARKET = 0,
    LIMIT = 1,
    STOP = 2,
    STOP_LIMIT = 3,
    IOC = 4,    // Immediate or Cancel
    FOK = 5,    // Fill or Kill
    GTD = 6,    // Good Till Date
    ICEBERG = 7
};

enum class OrderStatus : uint8_t {
    NEW = 0,
    PENDING = 1,
    PARTIALLY_FILLED = 2,
    FILLED = 3,
    CANCELLED = 4,
    REJECTED = 5,
    EXPIRED = 6
};

enum class ExecType : uint8_t {
    NEW = 0,
    FILL = 1,
    PARTIAL_FILL = 2,
    CANCEL = 3,
    REJECT = 4,
    REPLACE = 5
};

enum class VenueType : uint8_t {
    EXCHANGE = 0,
    ECN = 1,
    DARK_POOL = 2,
    CROSSING_NETWORK = 3,
    MARKET_MAKER = 4
};

enum class ProtocolType : uint8_t {
    FIX_42 = 0,
    FIX_44 = 1,
    FIX_50 = 2,
    OUCH = 3,
    ITCH = 4,
    OMNET = 5,
    BINARY_PROPRIETARY = 6,
    REST_JSON = 7,
    WEBSOCKET = 8
};

// =============================================================================
// LOCK-FREE RING BUFFER FOR INTER-COMPONENT COMMUNICATION
// =============================================================================

template<typename T, size_t Size>
class alignas(CACHE_LINE_SIZE) SPSCRingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

private:
    alignas(CACHE_LINE_SIZE) std::array<T, Size> buffer_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};

    static constexpr size_t mask_ = Size - 1;

public:
    bool try_push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & mask_;

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }

        item = buffer_[current_head];
        head_.store((current_head + 1) & mask_, std::memory_order_release);
        return true;
    }

    size_t size() const noexcept {
        return (tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire)) & mask_;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    bool full() const noexcept {
        const size_t current_tail = tail_.load(std::memory_order_acquire);
        const size_t next_tail = (current_tail + 1) & mask_;
        return next_tail == head_.load(std::memory_order_acquire);
    }
};

// =============================================================================
// CORE TRADING DATA STRUCTURES
// =============================================================================

// Cache-aligned order structure
struct alignas(CACHE_LINE_SIZE) Order {
    OrderId order_id;
    ClientId client_id;
    InstrumentId instrument_id;
    VenueId venue_id;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    Quantity leaves_quantity;
    Side side;
    OrderType type;
    OrderStatus status;
    Timestamp create_time;
    Timestamp update_time;
    char symbol[16];
    char client_order_id[32];

    // Exchange-specific fields
    char exchange_order_id[32];
    uint32_t sequence_number;

    Order() : order_id(0), client_id(0), instrument_id(0), venue_id(0),
              price(0), quantity(0), filled_quantity(0), leaves_quantity(0),
              side(Side::BUY), type(OrderType::LIMIT), status(OrderStatus::NEW),
              create_time(0), update_time(0), sequence_number(0) {
        memset(symbol, 0, sizeof(symbol));
        memset(client_order_id, 0, sizeof(client_order_id));
        memset(exchange_order_id, 0, sizeof(exchange_order_id));
    }

    Quantity remaining_quantity() const noexcept {
        return quantity - filled_quantity;
    }

    bool is_fully_filled() const noexcept {
        return filled_quantity >= quantity;
    }

    void update_fill(Quantity fill_qty, Price fill_price) noexcept {
        filled_quantity += fill_qty;
        leaves_quantity = quantity - filled_quantity;
        update_time = get_timestamp();

        if (is_fully_filled()) {
            status = OrderStatus::FILLED;
        } else {
            status = OrderStatus::PARTIALLY_FILLED;
        }
    }

private:
    static Timestamp get_timestamp() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
};

// Execution report
struct alignas(32) ExecutionReport {
    OrderId order_id;
    ClientId client_id;
    InstrumentId instrument_id;
    VenueId venue_id;
    ExecType exec_type;
    OrderStatus order_status;
    Price price;
    Quantity quantity;
    Quantity cum_quantity;
    Quantity leaves_quantity;
    Side side;
    Timestamp timestamp;
    char exec_id[32];
    char text[64];

    ExecutionReport() : order_id(0), client_id(0), instrument_id(0), venue_id(0),
                       exec_type(ExecType::NEW), order_status(OrderStatus::NEW),
                       price(0), quantity(0), cum_quantity(0), leaves_quantity(0),
                       side(Side::BUY), timestamp(get_timestamp()) {
        memset(exec_id, 0, sizeof(exec_id));
        memset(text, 0, sizeof(text));
    }

private:
    static Timestamp get_timestamp() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
};

// Market data tick
struct alignas(32) MarketDataTick {
    InstrumentId instrument_id;
    VenueId venue_id;
    Price bid_price;
    Price ask_price;
    Quantity bid_size;
    Quantity ask_size;
    Price last_price;
    Quantity last_size;
    Timestamp timestamp;
    uint64_t sequence_number;

    MarketDataTick() : instrument_id(0), venue_id(0), bid_price(0), ask_price(0),
                      bid_size(0), ask_size(0), last_price(0), last_size(0),
                      timestamp(get_timestamp()), sequence_number(0) {}

private:
    static Timestamp get_timestamp() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
};

// Risk check result
struct RiskCheckResult {
    OrderId order_id;
    bool approved;
    std::string reason;
    Timestamp check_time;

    RiskCheckResult() : order_id(0), approved(false), reason(""), check_time(0) {}

    RiskCheckResult(OrderId id, bool result, const std::string& msg = "")
        : order_id(id), approved(result), reason(msg),
          check_time(std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch()).count()) {}
};

// =============================================================================
// VENUE AND EXCHANGE CONFIGURATION
// =============================================================================

struct VenueConfig {
    VenueId venue_id;
    std::string name;
    VenueType type;
    ProtocolType protocol;
    std::string host;
    int port;
    bool enabled;
    int latency_microseconds; // Expected latency
    double fee_rate;
    bool supports_market_data;
    bool supports_trading;

    VenueConfig(VenueId id, const std::string& venue_name, VenueType vtype,
               ProtocolType proto, const std::string& hostname, int port_num)
        : venue_id(id), name(venue_name), type(vtype), protocol(proto),
          host(hostname), port(port_num), enabled(true),
          latency_microseconds(100), fee_rate(0.0001),
          supports_market_data(true), supports_trading(true) {}
};

// =============================================================================
// ABSTRACT EXCHANGE HANDLER INTERFACE (PLUGGABLE)
// =============================================================================

class IExchangeHandler {
public:
    virtual ~IExchangeHandler() = default;

    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;

    // Order management
    virtual bool send_new_order(const Order& order) = 0;
    virtual bool send_cancel_order(OrderId order_id) = 0;
    virtual bool send_replace_order(const Order& order) = 0;

    // Market data
    virtual bool subscribe_market_data(InstrumentId instrument_id) = 0;
    virtual bool unsubscribe_market_data(InstrumentId instrument_id) = 0;

    // Configuration
    virtual VenueId get_venue_id() const = 0;
    virtual ProtocolType get_protocol_type() const = 0;
    virtual const VenueConfig& get_config() const = 0;

    // Callbacks (set by the system)
    virtual void set_execution_callback(std::function<void(const ExecutionReport&)> callback) = 0;
    virtual void set_market_data_callback(std::function<void(const MarketDataTick&)> callback) = 0;
    virtual void set_connection_callback(std::function<void(bool)> callback) = 0;
};

// =============================================================================
// FIX PROTOCOL HANDLER
// =============================================================================

class FIXExchangeHandler : public IExchangeHandler {
private:
    VenueConfig config_;
    std::atomic<bool> connected_{false};
    std::thread network_thread_;
    std::atomic<bool> running_{false};

    std::function<void(const ExecutionReport&)> exec_callback_;
    std::function<void(const MarketDataTick&)> md_callback_;
    std::function<void(bool)> conn_callback_;

    // FIX session state
    uint32_t next_seq_num_{1};
    std::string session_id_;

public:
    explicit FIXExchangeHandler(const VenueConfig& config)
        : config_(config), session_id_(config.name + "_SESSION") {}

    ~FIXExchangeHandler() {
        disconnect();
    }

    bool connect() override {
        running_ = true;
        network_thread_ = std::thread(&FIXExchangeHandler::network_loop, this);

        // Simulate connection establishment
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        connected_ = true;

        if (conn_callback_) {
            conn_callback_(true);
        }

        std::cout << "FIX Handler connected to " << config_.name << std::endl;
        return true;
    }

    void disconnect() override {
        running_ = false;
        connected_ = false;

        if (network_thread_.joinable()) {
            network_thread_.join();
        }

        if (conn_callback_) {
            conn_callback_(false);
        }

        std::cout << "FIX Handler disconnected from " << config_.name << std::endl;
    }

    bool is_connected() const override {
        return connected_.load();
    }

    bool send_new_order(const Order& order) override {
        if (!connected_) return false;

        // Simulate FIX message creation and sending
        std::cout << "FIX: Sending New Order " << order.order_id
                  << " to " << config_.name << std::endl;

        // Simulate network latency and response
        std::thread([this, order]() {
            std::this_thread::sleep_for(std::chrono::microseconds(config_.latency_microseconds));

            // Simulate execution report
            ExecutionReport exec_report;
            exec_report.order_id = order.order_id;
            exec_report.client_id = order.client_id;
            exec_report.instrument_id = order.instrument_id;
            exec_report.venue_id = config_.venue_id;
            exec_report.exec_type = ExecType::NEW;
            exec_report.order_status = OrderStatus::NEW;
            exec_report.price = order.price;
            exec_report.quantity = order.quantity;
            exec_report.leaves_quantity = order.quantity;
            strcpy(exec_report.exec_id, "FIX_EXEC_001");

            if (exec_callback_) {
                exec_callback_(exec_report);
            }
        }).detach();

        return true;
    }

    bool send_cancel_order(OrderId order_id) override {
        if (!connected_) return false;

        std::cout << "FIX: Sending Cancel Order " << order_id
                  << " to " << config_.name << std::endl;
        return true;
    }

    bool send_replace_order(const Order& order) override {
        if (!connected_) return false;

        std::cout << "FIX: Sending Replace Order " << order.order_id
                  << " to " << config_.name << std::endl;
        return true;
    }

    bool subscribe_market_data(InstrumentId instrument_id) override {
        if (!connected_) return false;

        std::cout << "FIX: Subscribing to market data for instrument "
                  << instrument_id << " on " << config_.name << std::endl;
        return true;
    }

    bool unsubscribe_market_data(InstrumentId instrument_id) override {
        if (!connected_) return false;

        std::cout << "FIX: Unsubscribing from market data for instrument "
                  << instrument_id << " on " << config_.name << std::endl;
        return true;
    }

    VenueId get_venue_id() const override { return config_.venue_id; }
    ProtocolType get_protocol_type() const override { return config_.protocol; }
    const VenueConfig& get_config() const override { return config_; }

    void set_execution_callback(std::function<void(const ExecutionReport&)> callback) override {
        exec_callback_ = callback;
    }

    void set_market_data_callback(std::function<void(const MarketDataTick&)> callback) override {
        md_callback_ = callback;
    }

    void set_connection_callback(std::function<void(bool)> callback) override {
        conn_callback_ = callback;
    }

private:
    void network_loop() {
        while (running_) {
            // Simulate market data generation
            if (connected_ && md_callback_) {
                for (InstrumentId inst_id = 1; inst_id <= 5; ++inst_id) {
                    MarketDataTick tick;
                    tick.instrument_id = inst_id;
                    tick.venue_id = config_.venue_id;
                    tick.bid_price = 100000 + (rand() % 1000);
                    tick.ask_price = tick.bid_price + 10;
                    tick.bid_size = 1000 + (rand() % 9000);
                    tick.ask_size = 1000 + (rand() % 9000);
                    tick.last_price = tick.bid_price + 5;
                    tick.last_size = 100 + (rand() % 900);

                    md_callback_(tick);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

// =============================================================================
// OUCH PROTOCOL HANDLER
// =============================================================================

class OUCHExchangeHandler : public IExchangeHandler {
private:
    VenueConfig config_;
    std::atomic<bool> connected_{false};
    std::thread network_thread_;
    std::atomic<bool> running_{false};

    std::function<void(const ExecutionReport&)> exec_callback_;
    std::function<void(const MarketDataTick&)> md_callback_;
    std::function<void(bool)> conn_callback_;

public:
    explicit OUCHExchangeHandler(const VenueConfig& config) : config_(config) {}

    ~OUCHExchangeHandler() {
        disconnect();
    }

    bool connect() override {
        running_ = true;
        network_thread_ = std::thread(&OUCHExchangeHandler::network_loop, this);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        connected_ = true;

        if (conn_callback_) {
            conn_callback_(true);
        }

        std::cout << "OUCH Handler connected to " << config_.name << std::endl;
        return true;
    }

    void disconnect() override {
        running_ = false;
        connected_ = false;

        if (network_thread_.joinable()) {
            network_thread_.join();
        }

        if (conn_callback_) {
            conn_callback_(false);
        }

        std::cout << "OUCH Handler disconnected from " << config_.name << std::endl;
    }

    bool is_connected() const override {
        return connected_.load();
    }

    bool send_new_order(const Order& order) override {
        if (!connected_) return false;

        std::cout << "OUCH: Sending New Order " << order.order_id
                  << " to " << config_.name << std::endl;

        // Simulate OUCH binary message and response
        std::thread([this, order]() {
            std::this_thread::sleep_for(std::chrono::microseconds(config_.latency_microseconds));

            ExecutionReport exec_report;
            exec_report.order_id = order.order_id;
            exec_report.client_id = order.client_id;
            exec_report.instrument_id = order.instrument_id;
            exec_report.venue_id = config_.venue_id;
            exec_report.exec_type = ExecType::NEW;
            exec_report.order_status = OrderStatus::NEW;
            exec_report.price = order.price;
            exec_report.quantity = order.quantity;
            exec_report.leaves_quantity = order.quantity;
            strcpy(exec_report.exec_id, "OUCH_EXEC_001");

            if (exec_callback_) {
                exec_callback_(exec_report);
            }
        }).detach();

        return true;
    }

    bool send_cancel_order(OrderId order_id) override {
        if (!connected_) return false;

        std::cout << "OUCH: Sending Cancel Order " << order_id
                  << " to " << config_.name << std::endl;
        return true;
    }

    bool send_replace_order(const Order& order) override {
        if (!connected_) return false;

        std::cout << "OUCH: Sending Replace Order " << order.order_id
                  << " to " << config_.name << std::endl;
        return true;
    }

    bool subscribe_market_data(InstrumentId instrument_id) override {
        if (!connected_) return false;

        std::cout << "OUCH: Subscribing to market data for instrument "
                  << instrument_id << " on " << config_.name << std::endl;
        return true;
    }

    bool unsubscribe_market_data(InstrumentId instrument_id) override {
        if (!connected_) return false;

        std::cout << "OUCH: Unsubscribing from market data for instrument "
                  << instrument_id << " on " << config_.name << std::endl;
        return true;
    }

    VenueId get_venue_id() const override { return config_.venue_id; }
    ProtocolType get_protocol_type() const override { return config_.protocol; }
    const VenueConfig& get_config() const override { return config_; }

    void set_execution_callback(std::function<void(const ExecutionReport&)> callback) override {
        exec_callback_ = callback;
    }

    void set_market_data_callback(std::function<void(const MarketDataTick&)> callback) override {
        md_callback_ = callback;
    }

    void set_connection_callback(std::function<void(bool)> callback) override {
        conn_callback_ = callback;
    }

private:
    void network_loop() {
        while (running_) {
            // Simulate market data generation
            if (connected_ && md_callback_) {
                for (InstrumentId inst_id = 1; inst_id <= 5; ++inst_id) {
                    MarketDataTick tick;
                    tick.instrument_id = inst_id;
                    tick.venue_id = config_.venue_id;
                    tick.bid_price = 100000 + (rand() % 1000);
                    tick.ask_price = tick.bid_price + 10;
                    tick.bid_size = 1000 + (rand() % 9000);
                    tick.ask_size = 1000 + (rand() % 9000);
                    tick.last_price = tick.bid_price + 5;
                    tick.last_size = 100 + (rand() % 900);

                    md_callback_(tick);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
};

// =============================================================================
// ITCH PROTOCOL HANDLER (MARKET DATA ONLY)
// =============================================================================

class ITCHExchangeHandler : public IExchangeHandler {
private:
    VenueConfig config_;
    std::atomic<bool> connected_{false};
    std::thread network_thread_;
    std::atomic<bool> running_{false};

    std::function<void(const ExecutionReport&)> exec_callback_;
    std::function<void(const MarketDataTick&)> md_callback_;
    std::function<void(bool)> conn_callback_;

public:
    explicit ITCHExchangeHandler(const VenueConfig& config) : config_(config) {}

    ~ITCHExchangeHandler() {
        disconnect();
    }

    bool connect() override {
        running_ = true;
        network_thread_ = std::thread(&ITCHExchangeHandler::network_loop, this);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        connected_ = true;

        if (conn_callback_) {
            conn_callback_(true);
        }

        std::cout << "ITCH Handler connected to " << config_.name << std::endl;
        return true;
    }

    void disconnect() override {
        running_ = false;
        connected_ = false;

        if (network_thread_.joinable()) {
            network_thread_.join();
        }

        if (conn_callback_) {
            conn_callback_(false);
        }

        std::cout << "ITCH Handler disconnected from " << config_.name << std::endl;
    }

    bool is_connected() const override {
        return connected_.load();
    }

    // ITCH is market data only - no trading support
    bool send_new_order(const Order& order) override {
        std::cout << "ITCH: Trading not supported on market data feed" << std::endl;
        return false;
    }

    bool send_cancel_order(OrderId order_id) override {
        return false;
    }

    bool send_replace_order(const Order& order) override {
        return false;
    }

    bool subscribe_market_data(InstrumentId instrument_id) override {
        if (!connected_) return false;

        std::cout << "ITCH: Subscribing to market data for instrument "
                  << instrument_id << " on " << config_.name << std::endl;
        return true;
    }

    bool unsubscribe_market_data(InstrumentId instrument_id) override {
        if (!connected_) return false;

        std::cout << "ITCH: Unsubscribing from market data for instrument "
                  << instrument_id << " on " << config_.name << std::endl;
        return true;
    }

    VenueId get_venue_id() const override { return config_.venue_id; }
    ProtocolType get_protocol_type() const override { return config_.protocol; }
    const VenueConfig& get_config() const override { return config_; }

    void set_execution_callback(std::function<void(const ExecutionReport&)> callback) override {
        exec_callback_ = callback;
    }

    void set_market_data_callback(std::function<void(const MarketDataTick&)> callback) override {
        md_callback_ = callback;
    }

    void set_connection_callback(std::function<void(bool)> callback) override {
        conn_callback_ = callback;
    }

private:
    void network_loop() {
        while (running_) {
            // ITCH provides very high frequency market data
            if (connected_ && md_callback_) {
                for (InstrumentId inst_id = 1; inst_id <= 10; ++inst_id) {
                    MarketDataTick tick;
                    tick.instrument_id = inst_id;
                    tick.venue_id = config_.venue_id;
                    tick.bid_price = 100000 + (rand() % 1000);
                    tick.ask_price = tick.bid_price + 10;
                    tick.bid_size = 1000 + (rand() % 9000);
                    tick.ask_size = 1000 + (rand() % 9000);
                    tick.last_price = tick.bid_price + 5;
                    tick.last_size = 100 + (rand() % 900);
                    tick.sequence_number = inst_id * 1000 + (rand() % 1000);

                    md_callback_(tick);
                }
            }

            // ITCH has very high update frequency
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
};

// =============================================================================
// EXCHANGE HANDLER FACTORY (PLUGGABLE ARCHITECTURE)
// =============================================================================

class ExchangeHandlerFactory {
public:
    static std::unique_ptr<IExchangeHandler> create_handler(const VenueConfig& config) {
        switch (config.protocol) {
            case ProtocolType::FIX_42:
            case ProtocolType::FIX_44:
            case ProtocolType::FIX_50:
                return std::make_unique<FIXExchangeHandler>(config);

            case ProtocolType::OUCH:
                return std::make_unique<OUCHExchangeHandler>(config);

            case ProtocolType::ITCH:
                return std::make_unique<ITCHExchangeHandler>(config);

            case ProtocolType::OMNET:
                // TODO: Implement OMNetExchangeHandler
                std::cout << "OMNet handler not implemented yet" << std::endl;
                return nullptr;

            case ProtocolType::BINARY_PROPRIETARY:
                // TODO: Implement BinaryProprietaryExchangeHandler
                std::cout << "Binary proprietary handler not implemented yet" << std::endl;
                return nullptr;

            default:
                std::cout << "Unsupported protocol type" << std::endl;
                return nullptr;
        }
    }
};

// =============================================================================
// RISK/COMPLIANCE ENGINE
// =============================================================================

class RiskEngine {
private:
    struct RiskLimits {
        Quantity max_order_size = 10000;
        Price max_order_value = 1000000000; // $100,000
        Quantity max_daily_volume = 1000000;
        Quantity max_position_size = 100000;
        double max_daily_loss = 50000.0;
        bool allow_short_selling = true;
    };

    struct ClientRisk {
        ClientId client_id;
        RiskLimits limits;
        Quantity current_position;
        Quantity daily_volume;
        double daily_pnl;
        std::unordered_map<InstrumentId, Quantity> positions;

        ClientRisk() : client_id(0), current_position(0), daily_volume(0), daily_pnl(0.0) {}
    };

    std::unordered_map<ClientId, ClientRisk> client_risks_;
    std::mutex risk_mutex_;
    std::atomic<uint64_t> checks_performed_{0};
    std::atomic<uint64_t> checks_rejected_{0};

    // Pre-trade risk checks
    SPSCRingBuffer<Order, RING_BUFFER_SIZE> pending_orders_;
    SPSCRingBuffer<RiskCheckResult, RING_BUFFER_SIZE> risk_results_;

    std::thread risk_thread_;
    std::atomic<bool> running_{false};

public:
    RiskEngine() = default;

    ~RiskEngine() {
        stop();
    }

    void start() {
        running_ = true;
        risk_thread_ = std::thread(&RiskEngine::risk_processing_loop, this);
        std::cout << "Risk Engine started" << std::endl;
    }

    void stop() {
        running_ = false;
        if (risk_thread_.joinable()) {
            risk_thread_.join();
        }
        std::cout << "Risk Engine stopped" << std::endl;
    }

    // Submit order for risk check
    bool submit_for_risk_check(const Order& order) {
        return pending_orders_.try_push(order);
    }

    // Get risk check result
    bool get_risk_result(RiskCheckResult& result) {
        return risk_results_.try_pop(result);
    }

    // Configure client risk limits
    void set_client_limits(ClientId client_id, const RiskLimits& limits) {
        std::lock_guard<std::mutex> lock(risk_mutex_);
        client_risks_[client_id].client_id = client_id;
        client_risks_[client_id].limits = limits;
    }

    // Update position on fill
    void update_position(ClientId client_id, InstrumentId instrument_id,
                        Quantity fill_qty, Side side, Price fill_price) {
        std::lock_guard<std::mutex> lock(risk_mutex_);
        auto& client_risk = client_risks_[client_id];

        if (side == Side::BUY) {
            client_risk.positions[instrument_id] += fill_qty;
        } else {
            client_risk.positions[instrument_id] -= fill_qty;
        }

        client_risk.daily_volume += fill_qty;

        // Update P&L (simplified)
        double trade_value = (fill_price / 10000.0) * fill_qty;
        if (side == Side::SELL) {
            client_risk.daily_pnl += trade_value;
        } else {
            client_risk.daily_pnl -= trade_value;
        }
    }

    void print_statistics() const {
        std::cout << "\n=== Risk Engine Statistics ===\n";
        std::cout << "Checks Performed: " << checks_performed_.load() << "\n";
        std::cout << "Checks Rejected: " << checks_rejected_.load() << "\n";
        if (checks_performed_.load() > 0) {
            double rejection_rate = (double)checks_rejected_.load() / checks_performed_.load() * 100.0;
            std::cout << "Rejection Rate: " << rejection_rate << "%\n";
        }
    }

private:
    void risk_processing_loop() {
        set_thread_affinity(6); // Risk CPU

        while (running_) {
            Order order;
            if (pending_orders_.try_pop(order)) {
                RiskCheckResult result = perform_risk_check(order);
                risk_results_.try_push(result);
            } else {
                // Small delay when no orders to process
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
    }

    RiskCheckResult perform_risk_check(const Order& order) {
        checks_performed_.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(risk_mutex_);

        auto it = client_risks_.find(order.client_id);
        if (it == client_risks_.end()) {
            checks_rejected_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult(order.order_id, false, "Client not found");
        }

        const auto& client_risk = it->second;
        const auto& limits = client_risk.limits;

        // Check order size
        if (order.quantity > limits.max_order_size) {
            checks_rejected_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult(order.order_id, false, "Order size exceeds limit");
        }

        // Check order value
        double order_value = (order.price / 10000.0) * order.quantity;
        if (order_value > limits.max_order_value) {
            checks_rejected_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult(order.order_id, false, "Order value exceeds limit");
        }

        // Check daily volume
        if (client_risk.daily_volume + order.quantity > limits.max_daily_volume) {
            checks_rejected_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult(order.order_id, false, "Daily volume limit exceeded");
        }

        // Check position limits
        auto pos_it = client_risk.positions.find(order.instrument_id);
        Quantity current_pos = (pos_it != client_risk.positions.end()) ? pos_it->second : 0;
        Quantity new_pos = current_pos;

        if (order.side == Side::BUY) {
            new_pos += order.quantity;
        } else {
            new_pos -= order.quantity;

            // Check short selling
            if (new_pos < 0 && !limits.allow_short_selling) {
                checks_rejected_.fetch_add(1, std::memory_order_relaxed);
                return RiskCheckResult(order.order_id, false, "Short selling not allowed");
            }
        }

        if (static_cast<Quantity>(std::abs(static_cast<int64_t>(new_pos))) > limits.max_position_size) {
            checks_rejected_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult(order.order_id, false, "Position limit exceeded");
        }

        // Check daily loss limit
        if (client_risk.daily_pnl < -limits.max_daily_loss) {
            checks_rejected_.fetch_add(1, std::memory_order_relaxed);
            return RiskCheckResult(order.order_id, false, "Daily loss limit exceeded");
        }

        return RiskCheckResult(order.order_id, true, "Approved");
    }

    void set_thread_affinity(int cpu_core) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_core, &cpuset);
        sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
    }
};

// =============================================================================
// MARKET DATA FEED HANDLER
// =============================================================================

class MarketDataFeedHandler {
private:
    std::vector<std::unique_ptr<IExchangeHandler>> md_handlers_;

    // Market data distribution
    SPSCRingBuffer<MarketDataTick, RING_BUFFER_SIZE> md_queue_;

    std::thread processing_thread_;
    std::atomic<bool> running_{false};

    // Market data callbacks
    std::vector<std::function<void(const MarketDataTick&)>> subscribers_;

    // Statistics
    std::atomic<uint64_t> ticks_received_{0};
    std::atomic<uint64_t> ticks_processed_{0};

    // Best bid/offer tracking
    struct BBO {
        Price bid_price = 0;
        Price ask_price = 0;
        Quantity bid_size = 0;
        Quantity ask_size = 0;
        VenueId best_bid_venue = 0;
        VenueId best_ask_venue = 0;
        Timestamp update_time = 0;
    };

    std::unordered_map<InstrumentId, BBO> consolidated_bbo_;
    std::mutex bbo_mutex_;

public:
    MarketDataFeedHandler() = default;

    ~MarketDataFeedHandler() {
        stop();
    }

    void start() {
        running_ = true;
        processing_thread_ = std::thread(&MarketDataFeedHandler::processing_loop, this);

        // Start all market data handlers
        for (auto& handler : md_handlers_) {
            if (handler->get_config().supports_market_data) {
                handler->connect();
            }
        }

        std::cout << "Market Data Feed Handler started with "
                  << md_handlers_.size() << " venues" << std::endl;
    }

    void stop() {
        running_ = false;

        // Stop all handlers
        for (auto& handler : md_handlers_) {
            handler->disconnect();
        }

        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }

        std::cout << "Market Data Feed Handler stopped" << std::endl;
    }

    void add_venue(std::unique_ptr<IExchangeHandler> handler) {
        handler->set_market_data_callback([this](const MarketDataTick& tick) {
            handle_market_data(tick);
        });

        md_handlers_.push_back(std::move(handler));
    }

    void subscribe_instrument(InstrumentId instrument_id) {
        for (auto& handler : md_handlers_) {
            if (handler->get_config().supports_market_data) {
                handler->subscribe_market_data(instrument_id);
            }
        }
    }

    void add_subscriber(std::function<void(const MarketDataTick&)> callback) {
        subscribers_.push_back(callback);
    }

    // Get consolidated best bid/offer
    BBO get_consolidated_bbo(InstrumentId instrument_id) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(bbo_mutex_));
        auto it = consolidated_bbo_.find(instrument_id);
        return (it != consolidated_bbo_.end()) ? it->second : BBO{};
    }

    void print_statistics() const {
        std::cout << "\n=== Market Data Statistics ===\n";
        std::cout << "Ticks Received: " << ticks_received_.load() << "\n";
        std::cout << "Ticks Processed: " << ticks_processed_.load() << "\n";
        std::cout << "Connected Venues: " << md_handlers_.size() << "\n";
    }

private:
    void handle_market_data(const MarketDataTick& tick) {
        ticks_received_.fetch_add(1, std::memory_order_relaxed);
        md_queue_.try_push(tick);
    }

    void processing_loop() {
        set_thread_affinity(2); // Market data CPU

        while (running_) {
            MarketDataTick tick;
            if (md_queue_.try_pop(tick)) {
                process_market_data_tick(tick);
                ticks_processed_.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
    }

    void process_market_data_tick(const MarketDataTick& tick) {
        // Update consolidated BBO
        update_consolidated_bbo(tick);

        // Distribute to subscribers
        for (const auto& subscriber : subscribers_) {
            subscriber(tick);
        }
    }

    void update_consolidated_bbo(const MarketDataTick& tick) {
        std::lock_guard<std::mutex> lock(bbo_mutex_);

        auto& bbo = consolidated_bbo_[tick.instrument_id];

        // Update best bid
        if (tick.bid_price > bbo.bid_price ||
            (tick.bid_price == bbo.bid_price && tick.bid_size > bbo.bid_size)) {
            bbo.bid_price = tick.bid_price;
            bbo.bid_size = tick.bid_size;
            bbo.best_bid_venue = tick.venue_id;
            bbo.update_time = tick.timestamp;
        }

        // Update best ask
        if (tick.ask_price < bbo.ask_price || bbo.ask_price == 0 ||
            (tick.ask_price == bbo.ask_price && tick.ask_size > bbo.ask_size)) {
            bbo.ask_price = tick.ask_price;
            bbo.ask_size = tick.ask_size;
            bbo.best_ask_venue = tick.venue_id;
            bbo.update_time = tick.timestamp;
        }
    }

    void set_thread_affinity(int cpu_core) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_core, &cpuset);
        sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
    }
};

// =============================================================================
// SMART ORDER ROUTER (SOR)
// =============================================================================

class SmartOrderRouter {
private:
    struct VenueScore {
        VenueId venue_id;
        double score;
        Price effective_price;
        Quantity available_quantity;
        int latency_microseconds;

        VenueScore(VenueId id, double s, Price price, Quantity qty, int latency)
            : venue_id(id), score(s), effective_price(price),
              available_quantity(qty), latency_microseconds(latency) {}
    };

    MarketDataFeedHandler* md_handler_;
    std::vector<VenueConfig> venues_;

    // SOR algorithms
    enum class RoutingAlgorithm {
        BEST_PRICE,
        LOWEST_LATENCY,
        LIQUIDITY_SEEKING,
        VWAP,
        TWAP,
        IMPLEMENTATION_SHORTFALL
    };

    RoutingAlgorithm default_algorithm_ = RoutingAlgorithm::BEST_PRICE;

public:
    explicit SmartOrderRouter(MarketDataFeedHandler* md_handler)
        : md_handler_(md_handler) {}

    void add_venue(const VenueConfig& venue) {
        venues_.push_back(venue);
    }

    // Route order and return list of child orders
    std::vector<Order> route_order(const Order& parent_order,
                                  RoutingAlgorithm algorithm = RoutingAlgorithm::BEST_PRICE) {
        std::vector<Order> child_orders;

        switch (algorithm) {
            case RoutingAlgorithm::BEST_PRICE:
                child_orders = route_best_price(parent_order);
                break;

            case RoutingAlgorithm::LOWEST_LATENCY:
                child_orders = route_lowest_latency(parent_order);
                break;

            case RoutingAlgorithm::LIQUIDITY_SEEKING:
                child_orders = route_liquidity_seeking(parent_order);
                break;

            default:
                child_orders = route_best_price(parent_order);
                break;
        }

        std::cout << "SOR: Routed parent order " << parent_order.order_id
                  << " into " << child_orders.size() << " child orders" << std::endl;

        return child_orders;
    }

private:
    std::vector<Order> route_best_price(const Order& parent_order) {
        std::vector<Order> child_orders;
        std::vector<VenueScore> venue_scores;

        // Score venues based on price
        for (const auto& venue : venues_) {
            if (!venue.enabled || !venue.supports_trading) continue;

            auto bbo = md_handler_->get_consolidated_bbo(parent_order.instrument_id);
            Price target_price = (parent_order.side == Side::BUY) ? bbo.ask_price : bbo.bid_price;
            Quantity available_qty = (parent_order.side == Side::BUY) ? bbo.ask_size : bbo.bid_size;

            if (target_price > 0 && available_qty > 0) {
                double score = calculate_price_score(target_price, venue.fee_rate, parent_order.side);
                venue_scores.emplace_back(venue.venue_id, score, target_price,
                                        available_qty, venue.latency_microseconds);
            }
        }

        // Sort by score (best first)
        std::sort(venue_scores.begin(), venue_scores.end(),
                 [](const VenueScore& a, const VenueScore& b) {
                     return a.score > b.score;
                 });

        // Allocate quantity to venues
        Quantity remaining_qty = parent_order.quantity;
        OrderId child_order_id = parent_order.order_id * 1000; // Simple child ID generation

        for (const auto& venue_score : venue_scores) {
            if (remaining_qty == 0) break;

            Quantity child_qty = std::min(remaining_qty, venue_score.available_quantity);

            Order child_order = parent_order;
            child_order.order_id = child_order_id++;
            child_order.venue_id = venue_score.venue_id;
            child_order.quantity = child_qty;
            child_order.leaves_quantity = child_qty;
            child_order.price = venue_score.effective_price;

            child_orders.push_back(child_order);
            remaining_qty -= child_qty;
        }

        return child_orders;
    }

    std::vector<Order> route_lowest_latency(const Order& parent_order) {
        std::vector<Order> child_orders;

        // Find venue with lowest latency
        VenueId best_venue = 0;
        int lowest_latency = INT_MAX;

        for (const auto& venue : venues_) {
            if (venue.enabled && venue.supports_trading &&
                venue.latency_microseconds < lowest_latency) {
                lowest_latency = venue.latency_microseconds;
                best_venue = venue.venue_id;
            }
        }

        if (best_venue > 0) {
            Order child_order = parent_order;
            child_order.venue_id = best_venue;
            child_orders.push_back(child_order);
        }

        return child_orders;
    }

    std::vector<Order> route_liquidity_seeking(const Order& parent_order) {
        std::vector<Order> child_orders;

        // Find venues with most liquidity
        std::vector<VenueScore> venue_scores;

        for (const auto& venue : venues_) {
            if (!venue.enabled || !venue.supports_trading) continue;

            auto bbo = md_handler_->get_consolidated_bbo(parent_order.instrument_id);
            Quantity available_qty = (parent_order.side == Side::BUY) ? bbo.ask_size : bbo.bid_size;
            Price target_price = (parent_order.side == Side::BUY) ? bbo.ask_price : bbo.bid_price;

            if (available_qty > 0) {
                venue_scores.emplace_back(venue.venue_id, available_qty, target_price,
                                        available_qty, venue.latency_microseconds);
            }
        }

        // Sort by available quantity (most liquid first)
        std::sort(venue_scores.begin(), venue_scores.end(),
                 [](const VenueScore& a, const VenueScore& b) {
                     return a.available_quantity > b.available_quantity;
                 });

        // Distribute order to most liquid venues
        Quantity remaining_qty = parent_order.quantity;
        OrderId child_order_id = parent_order.order_id * 1000;

        for (const auto& venue_score : venue_scores) {
            if (remaining_qty == 0) break;

            Quantity child_qty = std::min(remaining_qty, venue_score.available_quantity);

            Order child_order = parent_order;
            child_order.order_id = child_order_id++;
            child_order.venue_id = venue_score.venue_id;
            child_order.quantity = child_qty;
            child_order.leaves_quantity = child_qty;
            child_order.price = venue_score.effective_price;

            child_orders.push_back(child_order);
            remaining_qty -= child_qty;
        }

        return child_orders;
    }

    double calculate_price_score(Price price, double fee_rate, Side side) {
        // Higher score is better
        // For buy orders: lower price is better
        // For sell orders: higher price is better
        double effective_price = price / 10000.0;
        double fee_cost = effective_price * fee_rate;

        if (side == Side::BUY) {
            return 1000000.0 / (effective_price + fee_cost); // Lower is better
        } else {
            return effective_price - fee_cost; // Higher is better
        }
    }
};


