/*
 * C++20 constinit Use Cases and Examples
 *
 * constinit specifies that a variable must be initialized with a constant
 * expression at compile-time. Unlike constexpr, constinit variables can
 * be modified at runtime after initialization.
 *
 * Key Benefits:
 * 1. Guarantees static initialization (no runtime initialization)
 * 2. Eliminates static initialization order fiasco
 * 3. Zero-cost initialization for global/static variables
 * 4. Can be modified at runtime (unlike constexpr variables)
 * 5. Ensures deterministic program startup
 * 6. Perfect for performance-critical global state
 */

#include <iostream>
#include <string>
#include <array>
#include <chrono>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

// ============================================================================
// 1. BASIC CONSTINIT USAGE
// ============================================================================

// constinit ensures compile-time initialization
constinit int global_counter = 42;

// constinit with constexpr - initialized at compile-time, can be modified
constinit int modifiable_value = 100;

// constexpr - cannot be modified after initialization
constexpr int immutable_value = 200;

// Regular global - may have runtime initialization
int regular_global = 300;

// Function that might be called during dynamic initialization
int expensive_initialization() {
    std::cout << "This would be called at runtime without constinit\n";
    return 500;
}

// This would cause compilation error with constinit:
// constinit int bad_init = expensive_initialization();  // ERROR!

void demonstrate_basic_constinit() {
    std::cout << "\n=== Basic constinit Usage ===\n";

    std::cout << "global_counter (constinit): " << global_counter << "\n";
    std::cout << "modifiable_value (constinit): " << modifiable_value << "\n";
    std::cout << "immutable_value (constexpr): " << immutable_value << "\n";
    std::cout << "regular_global: " << regular_global << "\n";

    // constinit variables can be modified at runtime
    global_counter = 1000;
    modifiable_value = 2000;

    std::cout << "\nAfter modification:\n";
    std::cout << "global_counter: " << global_counter << "\n";
    std::cout << "modifiable_value: " << modifiable_value << "\n";

    // This would cause compilation error:
    // immutable_value = 3000;  // ERROR: constexpr variables are immutable

    std::cout << "constinit variables are initialized at compile-time but can be modified at runtime\n";
}

// ============================================================================
// 2. SOLVING STATIC initialization ORDER FIASCO
// ============================================================================

// Without constinit - potential initialization order problems
namespace problematic {
    int global_dependency = 100;
    int dependent_global = global_dependency * 2;  // Order-dependent initialization
}

// With constinit - guaranteed safe initialization
namespace safe {
    constinit int global_dependency = 100;
    constinit int dependent_global = global_dependency * 2;  // Safe: compile-time init
}

// Complex example: Configuration system
struct Config {
    int max_connections;
    double timeout_seconds;
    bool debug_mode;

    constexpr Config(int conn, double timeout, bool debug)
        : max_connections(conn), timeout_seconds(timeout), debug_mode(debug) {}
};

// Safe global configuration - initialized at compile-time
constinit Config system_config{1000, 30.0, false};

// Logger that depends on configuration
class Logger {
private:
    bool debug_enabled;

public:
    constexpr Logger(const Config& config) : debug_enabled(config.debug_mode) {}

    void log(const std::string& message) {
        if (debug_enabled) {
            std::cout << "[DEBUG] " << message << "\n";
        } else {
            std::cout << "[INFO] " << message << "\n";
        }
    }

    void set_debug(bool enable) { debug_enabled = enable; }
};

// Safe logger initialization - no dependency on initialization order
constinit Logger global_logger{system_config};

void demonstrate_initialization_order() {
    std::cout << "\n=== Static Initialization Order Safety ===\n";

    std::cout << "Problematic namespace:\n";
    std::cout << "  global_dependency: " << problematic::global_dependency << "\n";
    std::cout << "  dependent_global: " << problematic::dependent_global << "\n";

    std::cout << "Safe namespace (constinit):\n";
    std::cout << "  global_dependency: " << safe::global_dependency << "\n";
    std::cout << "  dependent_global: " << safe::dependent_global << "\n";

    std::cout << "\nConfiguration and Logger:\n";
    std::cout << "System config - max connections: " << system_config.max_connections << "\n";
    std::cout << "System config - timeout: " << system_config.timeout_seconds << "s\n";
    std::cout << "System config - debug mode: " << std::boolalpha << system_config.debug_mode << "\n";

    global_logger.log("System initialized successfully");

    // Configuration can be modified at runtime
    system_config.debug_mode = true;
    global_logger.set_debug(true);
    global_logger.log("Debug mode enabled");

    std::cout << "constinit ensures safe initialization order\n";
}

// ============================================================================
// 3. PERFORMANCE-CRITICAL GLOBAL STATE
// ============================================================================

// High-frequency trading system globals
constinit std::atomic<uint64_t> trade_sequence_number{1};
constinit std::atomic<double> last_market_price{100.0};
constinit std::atomic<bool> market_open{false};

// Cache for frequently accessed data
struct MarketDataCache {
    std::array<double, 1000> prices;
    std::array<uint64_t, 1000> volumes;
    std::atomic<size_t> count{0};

    constexpr MarketDataCache() : prices{}, volumes{} {}
};

constinit MarketDataCache market_cache{};

// Trading statistics
struct TradingStats {
    std::atomic<uint64_t> total_trades{0};
    std::atomic<double> total_volume{0.0};
    std::atomic<double> total_pnl{0.0};

    constexpr TradingStats() = default;
};

constinit TradingStats trading_stats{};

// Thread-safe counter with zero initialization cost
class FastCounter {
private:
    std::atomic<uint64_t> value;

public:
    constexpr FastCounter(uint64_t initial = 0) : value(initial) {}

    uint64_t increment() {
        return value.fetch_add(1) + 1;
    }

    uint64_t get() const {
        return value.load();
    }

    void reset() {
        value.store(0);
    }
};

constinit FastCounter message_counter{0};
constinit FastCounter error_counter{0};
constinit FastCounter order_counter{1000};  // Start from 1000

void demonstrate_performance_critical_globals() {
    std::cout << "\n=== Performance-Critical Global State ===\n";

    // Trading system operations
    std::cout << "Initial trading state:\n";
    std::cout << "  Trade sequence: " << trade_sequence_number.load() << "\n";
    std::cout << "  Last market price: $" << last_market_price.load() << "\n";
    std::cout << "  Market open: " << std::boolalpha << market_open.load() << "\n";

    // Simulate market opening
    market_open.store(true);
    last_market_price.store(150.75);

    // Simulate trading activity
    for (int i = 0; i < 5; ++i) {
        uint64_t trade_id = trade_sequence_number.fetch_add(1);
        double volume = 100.0 * (i + 1);

        trading_stats.total_trades.fetch_add(1);
        trading_stats.total_volume.fetch_add(volume);

        std::cout << "Trade " << trade_id << ": volume " << volume << "\n";
    }

    std::cout << "\nTrading statistics:\n";
    std::cout << "  Total trades: " << trading_stats.total_trades.load() << "\n";
    std::cout << "  Total volume: " << trading_stats.total_volume.load() << "\n";

    // Fast counters
    std::cout << "\nFast counters:\n";
    for (int i = 0; i < 3; ++i) {
        std::cout << "Message " << message_counter.increment() << " processed\n";
    }

    std::cout << "Order ID: " << order_counter.increment() << "\n";
    std::cout << "Order ID: " << order_counter.increment() << "\n";

    std::cout << "All globals initialized at compile-time with zero runtime cost\n";
}

// ============================================================================
// 4. COMPILE-TIME CONSTANTS WITH RUNTIME MUTABILITY
// ============================================================================

// Mathematical constants that can be overridden for testing
constinit double PI = 3.14159265358979323846;
constinit double E = 2.71828182845904523536;
constinit double GOLDEN_RATIO = 1.61803398874989484820;

// Financial constants
constinit double DEFAULT_RISK_FREE_RATE = 0.02;  // 2%
constinit double DEFAULT_VOLATILITY = 0.20;      // 20%
constinit double DEFAULT_COMMISSION_RATE = 0.001; // 0.1%

// System limits that can be adjusted
constinit size_t MAX_CONNECTIONS = 1000;
constinit size_t MAX_BUFFER_SIZE = 65536;
constinit size_t MAX_RETRY_COUNT = 3;

// Feature flags
constinit bool ENABLE_LOGGING = true;
constinit bool ENABLE_PROFILING = false;
constinit bool ENABLE_DEBUG_MODE = false;

class MathUtils {
public:
    static double circle_area(double radius) {
        return PI * radius * radius;
    }

    static double compound_growth(double principal, double rate, int periods) {
        double base = 1.0 + rate;
        double result = principal;
        for (int i = 0; i < periods; ++i) {
            result *= base;
        }
        return result;
    }

    static double option_time_value(double volatility, double time) {
        return volatility * time * 0.4;  // Simplified
    }
};

void demonstrate_mutable_constants() {
    std::cout << "\n=== Compile-Time Constants with Runtime Mutability ===\n";

    std::cout << "Mathematical constants:\n";
    std::cout << "  PI = " << PI << "\n";
    std::cout << "  E = " << E << "\n";
    std::cout << "  Golden Ratio = " << GOLDEN_RATIO << "\n";

    std::cout << "\nUsing constants in calculations:\n";
    double radius = 5.0;
    std::cout << "Circle area (r=" << radius << "): " << MathUtils::circle_area(radius) << "\n";

    // Financial calculations
    std::cout << "\nFinancial calculations:\n";
    double investment = 10000.0;
    double growth = MathUtils::compound_growth(investment, DEFAULT_RISK_FREE_RATE, 5);
    std::cout << "$" << investment << " at " << (DEFAULT_RISK_FREE_RATE * 100)
              << "% for 5 years: $" << growth << "\n";

    double time_value = MathUtils::option_time_value(DEFAULT_VOLATILITY, 0.25);
    std::cout << "Option time value (vol=" << (DEFAULT_VOLATILITY * 100)
              << "%, t=0.25): " << time_value << "\n";

    // Runtime modification for testing/configuration
    std::cout << "\nModifying constants for testing:\n";
    PI = 3.14;  // Simplified PI for testing
    DEFAULT_VOLATILITY = 0.30;  // Higher volatility

    std::cout << "Modified PI = " << PI << "\n";
    std::cout << "Circle area with simplified PI: " << MathUtils::circle_area(radius) << "\n";
    std::cout << "Option time value with higher volatility: "
              << MathUtils::option_time_value(DEFAULT_VOLATILITY, 0.25) << "\n";

    // System configuration
    std::cout << "\nSystem limits:\n";
    std::cout << "  Max connections: " << MAX_CONNECTIONS << "\n";
    std::cout << "  Max buffer size: " << MAX_BUFFER_SIZE << " bytes\n";
    std::cout << "  Max retry count: " << MAX_RETRY_COUNT << "\n";

    // Feature flags
    std::cout << "\nFeature flags:\n";
    std::cout << "  Logging: " << std::boolalpha << ENABLE_LOGGING << "\n";
    std::cout << "  Profiling: " << ENABLE_PROFILING << "\n";
    std::cout << "  Debug mode: " << ENABLE_DEBUG_MODE << "\n";

    std::cout << "All values initialized at compile-time but adjustable at runtime\n";
}

// ============================================================================
// 5. THREAD-SAFE GLOBAL INITIALIZATION
// ============================================================================

// Thread-safe singleton pattern with constinit
class ThreadSafeCounter {
private:
    mutable std::mutex mutex_;
    uint64_t value_;

public:
    constexpr ThreadSafeCounter(uint64_t initial = 0) : value_(initial) {}

    uint64_t increment() {
        std::lock_guard<std::mutex> lock(mutex_);
        return ++value_;
    }

    uint64_t get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = 0;
    }
};

constinit ThreadSafeCounter global_request_counter{0};
constinit ThreadSafeCounter global_error_counter{0};

// Connection pool with thread-safe initialization
struct ConnectionPool {
    std::atomic<size_t> active_connections{0};
    std::atomic<size_t> total_connections{0};
    std::atomic<bool> initialized{false};

    constexpr ConnectionPool() = default;

    void add_connection() {
        active_connections.fetch_add(1);
        total_connections.fetch_add(1);
    }

    void remove_connection() {
        if (active_connections.load() > 0) {
            active_connections.fetch_sub(1);
        }
    }

    size_t get_active() const { return active_connections.load(); }
    size_t get_total() const { return total_connections.load(); }
};

constinit ConnectionPool db_pool{};
constinit ConnectionPool cache_pool{};

// Worker function for threading demonstration
void worker_function(int worker_id) {
    for (int i = 0; i < 3; ++i) {
        uint64_t request_id = global_request_counter.increment();
        std::cout << "Worker " << worker_id << " processing request " << request_id << "\n";

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Simulate connection usage
        db_pool.add_connection();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        db_pool.remove_connection();
    }
}

void demonstrate_thread_safe_globals() {
    std::cout << "\n=== Thread-Safe Global Initialization ===\n";

    std::cout << "Initial state:\n";
    std::cout << "  Request counter: " << global_request_counter.get() << "\n";
    std::cout << "  Error counter: " << global_error_counter.get() << "\n";
    std::cout << "  DB pool active: " << db_pool.get_active() << "\n";
    std::cout << "  DB pool total: " << db_pool.get_total() << "\n";

    // Create multiple threads
    std::vector<std::thread> workers;

    std::cout << "\nStarting worker threads:\n";
    for (int i = 0; i < 3; ++i) {
        workers.emplace_back(worker_function, i + 1);
    }

    // Wait for all threads to complete
    for (auto& worker : workers) {
        worker.join();
    }

    std::cout << "\nFinal state:\n";
    std::cout << "  Request counter: " << global_request_counter.get() << "\n";
    std::cout << "  Error counter: " << global_error_counter.get() << "\n";
    std::cout << "  DB pool active: " << db_pool.get_active() << "\n";
    std::cout << "  DB pool total: " << db_pool.get_total() << "\n";

    std::cout << "All global state safely initialized before any thread access\n";
}

// ============================================================================
// 6. ARRAYS AND COMPLEX DATA STRUCTURES
// ============================================================================

// Large lookup table initialized at compile-time
constexpr std::array<double, 256> generate_sin_table() {
    std::array<double, 256> table{};
    constexpr double PI_VAL = 3.14159265358979323846;

    for (size_t i = 0; i < 256; ++i) {
        double angle = (2.0 * PI_VAL * i) / 256.0;

        // Taylor series approximation for sin
        double x = angle;
        double sin_x = x;
        double term = x;

        for (int n = 1; n <= 10; ++n) {
            term *= -x * x / ((2 * n) * (2 * n + 1));
            sin_x += term;
        }

        table[i] = sin_x;
    }

    return table;
}

constinit std::array<double, 256> sin_lookup_table = generate_sin_table();

// Trading symbol hash table
struct SymbolData {
    uint32_t symbol_id;
    double last_price;
    uint64_t volume;
    bool is_active;

    constexpr SymbolData(uint32_t id = 0, double price = 0.0,
                        uint64_t vol = 0, bool active = false)
        : symbol_id(id), last_price(price), volume(vol), is_active(active) {}
};

constexpr std::array<SymbolData, 100> initialize_symbol_table() {
    std::array<SymbolData, 100> table{};

    // Initialize some common symbols
    table[0] = SymbolData{1001, 150.25, 1000000, true};  // AAPL
    table[1] = SymbolData{1002, 2800.50, 500000, true};  // GOOGL
    table[2] = SymbolData{1003, 300.75, 800000, true};   // MSFT
    table[3] = SymbolData{1004, 800.00, 600000, true};   // TSLA

    return table;
}

constinit std::array<SymbolData, 100> symbol_table = initialize_symbol_table();

// Price history buffer
struct PriceHistory {
    std::array<double, 1000> prices;
    std::atomic<size_t> head{0};
    std::atomic<size_t> count{0};

    constexpr PriceHistory() : prices{} {}

    void add_price(double price) {
        size_t index = head.fetch_add(1) % 1000;
        prices[index] = price;

        size_t current_count = count.load();
        if (current_count < 1000) {
            count.store(current_count + 1);
        }
    }

    double get_average() const {
        size_t cnt = count.load();
        if (cnt == 0) return 0.0;

        double sum = 0.0;
        for (size_t i = 0; i < cnt; ++i) {
            sum += prices[i];
        }

        return sum / cnt;
    }
};

constinit PriceHistory aapl_history{};
constinit PriceHistory googl_history{};

void demonstrate_complex_data_structures() {
    std::cout << "\n=== Arrays and Complex Data Structures ===\n";

    // Sin lookup table
    std::cout << "Sin lookup table (first 8 values):\n";
    for (size_t i = 0; i < 8; ++i) {
        double angle_deg = (360.0 * i) / 256.0;
        std::cout << "  sin(" << std::fixed << std::setprecision(1) << angle_deg
                  << "°) ≈ " << std::setprecision(4) << sin_lookup_table[i] << "\n";
    }

    // Symbol table
    std::cout << "\nSymbol table:\n";
    for (size_t i = 0; i < 4; ++i) {
        const auto& symbol = symbol_table[i];
        if (symbol.is_active) {
            std::cout << "  Symbol ID " << symbol.symbol_id
                      << ": $" << symbol.last_price
                      << " (vol: " << symbol.volume << ")\n";
        }
    }

    // Modify symbol data at runtime
    symbol_table[0].last_price = 155.50;
    symbol_table[0].volume += 50000;

    std::cout << "\nAfter runtime update:\n";
    std::cout << "  Symbol ID " << symbol_table[0].symbol_id
              << ": $" << symbol_table[0].last_price
              << " (vol: " << symbol_table[0].volume << ")\n";

    // Price history
    std::cout << "\nPrice history simulation:\n";
    double base_price = 150.0;
    for (int i = 0; i < 10; ++i) {
        double price = base_price + (i * 0.25) + ((i % 3) * 0.10);
        aapl_history.add_price(price);
        std::cout << "Added price: $" << std::fixed << std::setprecision(2) << price << "\n";
    }

    std::cout << "Average price: $" << aapl_history.get_average() << "\n";

    std::cout << "All data structures initialized at compile-time\n";
}

// ============================================================================
// 7. CONSTINIT VS CONSTEXPR VS STATIC
// ============================================================================

// Different initialization approaches
constexpr int constexpr_value = 100;           // Compile-time constant, immutable
constinit int constinit_value = 200;           // Compile-time init, runtime mutable
static int static_value = 300;                 // May have runtime initialization
int global_value = 400;                        // May have runtime initialization

// Function to demonstrate the differences
constexpr int get_compile_time_value() { return 42; }
int get_runtime_value() { return 84; }

// Valid constinit initializations
constinit int valid_constinit_1 = 500;
constinit int valid_constinit_2 = get_compile_time_value();
constinit int valid_constinit_3 = valid_constinit_1 * 2;

// These would cause compilation errors with constinit:
// constinit int invalid_constinit_1 = get_runtime_value();  // ERROR!
// constinit int invalid_constinit_2 = rand();               // ERROR!

class ComparisonDemo {
private:
    static constexpr int class_constexpr = 1000;    // Immutable class constant
    static constinit int class_constinit;           // Mutable, compile-time init
    static int class_static;                        // Regular static member

public:
    static void demonstrate_differences() {
        std::cout << "Class members:\n";
        std::cout << "  constexpr: " << class_constexpr << " (immutable)\n";
        std::cout << "  constinit: " << class_constinit << " (mutable)\n";
        std::cout << "  static: " << class_static << " (regular static)\n";

        // Only constinit can be modified
        class_constinit = 1500;
        class_static = 2500;
        // class_constexpr = 1200;  // ERROR: cannot modify constexpr

        std::cout << "After modification:\n";
        std::cout << "  constinit: " << class_constinit << "\n";
        std::cout << "  static: " << class_static << "\n";
    }
};

// Define static members
constinit int ComparisonDemo::class_constinit = 1200;
int ComparisonDemo::class_static = 2000;

void demonstrate_constinit_vs_others() {
    std::cout << "\n=== constinit vs constexpr vs static ===\n";

    std::cout << "Global variables:\n";
    std::cout << "  constexpr_value: " << constexpr_value << " (immutable)\n";
    std::cout << "  constinit_value: " << constinit_value << " (mutable)\n";
    std::cout << "  static_value: " << static_value << " (regular static)\n";
    std::cout << "  global_value: " << global_value << " (regular global)\n";

    // Modify mutable values
    constinit_value = 250;
    static_value = 350;
    global_value = 450;
    // constexpr_value = 150;  // ERROR: cannot modify constexpr

    std::cout << "\nAfter modification:\n";
    std::cout << "  constinit_value: " << constinit_value << "\n";
    std::cout << "  static_value: " << static_value << "\n";
    std::cout << "  global_value: " << global_value << "\n";

    std::cout << "\nValid constinit initializations:\n";
    std::cout << "  valid_constinit_1: " << valid_constinit_1 << "\n";
    std::cout << "  valid_constinit_2: " << valid_constinit_2 << "\n";
    std::cout << "  valid_constinit_3: " << valid_constinit_3 << "\n";

    ComparisonDemo::demonstrate_differences();

    std::cout << "\nKey differences:\n";
    std::cout << "- constexpr: Compile-time constant, immutable\n";
    std::cout << "- constinit: Compile-time initialization, runtime mutable\n";
    std::cout << "- static: May have runtime initialization, mutable\n";
    std::cout << "- constinit prevents dynamic initialization bugs\n";
}

// ============================================================================
// 8. REAL-WORLD APPLICATION: TRADING SYSTEM GLOBALS
// ============================================================================

// Trading system configuration
struct TradingConfig {
    double max_position_size;
    double risk_limit;
    uint32_t max_orders_per_second;
    bool enable_risk_checks;
    double commission_rate;

    constexpr TradingConfig(double pos_size, double risk, uint32_t orders,
                           bool risk_checks, double commission)
        : max_position_size(pos_size), risk_limit(risk),
          max_orders_per_second(orders), enable_risk_checks(risk_checks),
          commission_rate(commission) {}
};

constinit TradingConfig trading_config{1000000.0, 0.02, 1000, true, 0.001};

// Order book data
struct OrderBookLevel {
    double price;
    uint64_t quantity;
    uint32_t order_count;

    constexpr OrderBookLevel(double p = 0.0, uint64_t q = 0, uint32_t c = 0)
        : price(p), quantity(q), order_count(c) {}
};

constexpr std::array<OrderBookLevel, 10> init_order_book() {
    std::array<OrderBookLevel, 10> book{};

    // Initialize with some sample data
    book[0] = OrderBookLevel{150.25, 1000, 5};
    book[1] = OrderBookLevel{150.24, 800, 3};
    book[2] = OrderBookLevel{150.23, 1200, 7};
    book[3] = OrderBookLevel{150.22, 500, 2};
    book[4] = OrderBookLevel{150.21, 900, 4};

    return book;
}

constinit std::array<OrderBookLevel, 10> bid_levels = init_order_book();
constinit std::array<OrderBookLevel, 10> ask_levels = init_order_book();

// Market statistics
struct MarketStats {
    std::atomic<uint64_t> total_trades{0};
    std::atomic<double> total_volume{0.0};
    std::atomic<double> vwap{0.0};
    std::atomic<double> high_price{0.0};
    std::atomic<double> low_price{999999.0};

    constexpr MarketStats() = default;

    void update_trade(double price, uint64_t volume) {
        total_trades.fetch_add(1);

        double old_total_volume = total_volume.load();
        double old_vwap = vwap.load();
        double new_total_volume = old_total_volume + volume;
        double new_vwap = (old_vwap * old_total_volume + price * volume) / new_total_volume;

        total_volume.store(new_total_volume);
        vwap.store(new_vwap);

        // Update high/low
        double current_high = high_price.load();
        while (price > current_high &&
               !high_price.compare_exchange_weak(current_high, price)) {}

        double current_low = low_price.load();
        while (price < current_low &&
               !low_price.compare_exchange_weak(current_low, price)) {}
    }
};

constinit MarketStats aapl_stats{};
constinit MarketStats googl_stats{};

void demonstrate_trading_system_globals() {
    std::cout << "\n=== Real-World: Trading System Globals ===\n";

    std::cout << "Trading configuration:\n";
    std::cout << "  Max position size: $" << trading_config.max_position_size << "\n";
    std::cout << "  Risk limit: " << (trading_config.risk_limit * 100) << "%\n";
    std::cout << "  Max orders/sec: " << trading_config.max_orders_per_second << "\n";
    std::cout << "  Risk checks: " << std::boolalpha << trading_config.enable_risk_checks << "\n";
    std::cout << "  Commission rate: " << (trading_config.commission_rate * 100) << "%\n";

    std::cout << "\nOrder book (bid levels):\n";
    for (size_t i = 0; i < 5; ++i) {
        const auto& level = bid_levels[i];
        if (level.quantity > 0) {
            std::cout << "  $" << std::fixed << std::setprecision(2) << level.price
                      << " - " << level.quantity << " shares (" << level.order_count << " orders)\n";
        }
    }

    // Simulate trading activity
    std::cout << "\nSimulating trades:\n";
    aapl_stats.update_trade(150.25, 1000);
    aapl_stats.update_trade(150.30, 500);
    aapl_stats.update_trade(150.20, 800);
    aapl_stats.update_trade(150.35, 300);

    std::cout << "AAPL trading statistics:\n";
    std::cout << "  Total trades: " << aapl_stats.total_trades.load() << "\n";
    std::cout << "  Total volume: " << aapl_stats.total_volume.load() << "\n";
    std::cout << "  VWAP: $" << std::fixed << std::setprecision(2) << aapl_stats.vwap.load() << "\n";
    std::cout << "  High: $" << aapl_stats.high_price.load() << "\n";
    std::cout << "  Low: $" << aapl_stats.low_price.load() << "\n";

    // Runtime configuration changes
    trading_config.risk_limit = 0.015;  // Tighten risk limit
    trading_config.enable_risk_checks = false;  // Disable for testing

    std::cout << "\nAfter configuration update:\n";
    std::cout << "  Risk limit: " << (trading_config.risk_limit * 100) << "%\n";
    std::cout << "  Risk checks: " << std::boolalpha << trading_config.enable_risk_checks << "\n";

    std::cout << "All critical globals safely initialized at compile-time\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 constinit Use Cases and Examples\n";
    std::cout << "======================================\n";

    demonstrate_basic_constinit();
    demonstrate_initialization_order();
    demonstrate_performance_critical_globals();
    demonstrate_mutable_constants();
    demonstrate_thread_safe_globals();
    demonstrate_complex_data_structures();
    demonstrate_constinit_vs_others();
    demonstrate_trading_system_globals();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. constinit guarantees compile-time initialization\n";
    std::cout << "2. Eliminates static initialization order fiasco\n";
    std::cout << "3. Variables can be modified at runtime (unlike constexpr)\n";
    std::cout << "4. Perfect for performance-critical global state\n";
    std::cout << "5. Ensures deterministic program startup\n";
    std::cout << "6. Thread-safe initialization before main()\n";
    std::cout << "7. Zero runtime initialization cost\n";
    std::cout << "8. Excellent for trading systems and real-time applications\n";
    std::cout << "9. Can be used with complex data structures and arrays\n";
    std::cout << "10. Provides compile-time safety with runtime flexibility\n";
    std::cout << "11. Prevents dynamic initialization bugs\n";
    std::cout << "12. Essential for high-performance global configuration\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler
 * - GCC 10+, Clang 10+, or MSVC 2019+
 * - Use -std=c++20 flag
 *
 * Example compilation:
 * g++ -std=c++20 -Wall -Wextra -O2 -pthread cpp20_constinit_use_cases_examples.cpp -o constinit_demo
 *
 * Key Features of constinit:
 * 1. Guarantees static initialization (compile-time)
 * 2. Can be modified at runtime (unlike constexpr)
 * 3. Prevents dynamic initialization
 * 4. Eliminates initialization order dependencies
 * 5. Thread-safe initialization
 * 6. Zero runtime initialization overhead
 *
 * Common Use Cases:
 * - Global configuration that changes at runtime
 * - Performance-critical global state
 * - Thread-safe global counters and statistics
 * - Trading system globals and market data
 * - Lookup tables and caches
 * - Feature flags and system limits
 * - Mathematical constants that can be overridden
 * - Prevention of static initialization order fiasco
 *
 * Benefits over alternatives:
 * - constexpr: Allows runtime modification
 * - static: Guarantees compile-time initialization
 * - regular globals: Eliminates initialization order issues
 * - Better performance than dynamic initialization
 * - Safer than uninitialized globals
 */
