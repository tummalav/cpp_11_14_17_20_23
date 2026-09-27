/**
 * Static Polymorphism Techniques in C++
 * Alternative approaches to virtual functions for ultra-low latency systems
 * Compile-time resolution eliminates vtable overhead
 */

#include <iostream>
#include <variant>
#include <string>
#include <chrono>
#include <functional>
#include <type_traits>
#include <concepts>
#include <memory>

// Forward declarations for examples
struct MarketTick {
    std::string symbol;
    double price;
    int64_t timestamp;
    int volume;
};

struct Order {
    std::string symbol;
    double price;
    int quantity;
    char side; // 'B' or 'S'
};

//=============================================================================
// 1. TEMPLATE-BASED POLYMORPHISM (Most Common for Trading Systems)
//=============================================================================

template<typename Strategy>
class TradingEngine {
    Strategy strategy_;

public:
    explicit TradingEngine(Strategy s) : strategy_(std::move(s)) {}

    void process_market_data(const MarketTick& tick) {
        // Resolved at compile time - zero overhead
        strategy_.on_market_data(tick);
    }

    void process_order_update(const Order& order) {
        strategy_.on_order_update(order);
    }
};

// Different strategy implementations
class MarketMakingStrategy {
public:
    void on_market_data(const MarketTick& tick) {
        std::cout << "MM Strategy: Processing " << tick.symbol
                  << " @ " << tick.price << std::endl;
        // Market making logic here
    }

    void on_order_update(const Order& order) {
        std::cout << "MM Strategy: Order update for " << order.symbol << std::endl;
    }
};

class ArbitrageStrategy {
public:
    void on_market_data(const MarketTick& tick) {
        std::cout << "Arbitrage Strategy: Analyzing " << tick.symbol
                  << " for opportunities" << std::endl;
        // Arbitrage detection logic
    }

    void on_order_update(const Order& order) {
        std::cout << "Arbitrage Strategy: Order filled " << order.symbol << std::endl;
    }
};

//=============================================================================
// 2. CRTP (Curiously Recurring Template Pattern)
//=============================================================================

template<typename Derived>
class BaseStrategy {
public:
    void execute_trading_logic(const MarketTick& tick) {
        // Common preprocessing
        if (tick.volume > 0) {
            // Compile-time dispatch - no virtual call overhead
            static_cast<Derived*>(this)->handle_tick(tick);
        }
    }

    void risk_check() {
        static_cast<Derived*>(this)->validate_position();
    }

protected:
    ~BaseStrategy() = default; // Prevent polymorphic deletion
};

class HighFrequencyStrategy : public BaseStrategy<HighFrequencyStrategy> {
public:
    void handle_tick(const MarketTick& tick) {
        std::cout << "HFT: Ultra-fast processing " << tick.symbol << std::endl;
        // Microsecond-level optimizations
    }

    void validate_position() {
        std::cout << "HFT: Position validation" << std::endl;
    }
};

class StatisticalArbitrageStrategy : public BaseStrategy<StatisticalArbitrageStrategy> {
public:
    void handle_tick(const MarketTick& tick) {
        std::cout << "StatArb: Statistical analysis for " << tick.symbol << std::endl;
        // Statistical models and correlation analysis
    }

    void validate_position() {
        std::cout << "StatArb: Risk model validation" << std::endl;
    }
};

//=============================================================================
// 3. FUNCTION OBJECTS/FUNCTORS
//=============================================================================

struct MarketMakingFunctor {
    double spread_threshold = 0.01;

    void operator()(const MarketTick& tick) {
        std::cout << "Functor MM: Processing " << tick.symbol
                  << " with spread " << spread_threshold << std::endl;
    }
};

struct ScalpingFunctor {
    int target_profit_ticks = 1;

    void operator()(const MarketTick& tick) {
        std::cout << "Functor Scalping: Quick profit on " << tick.symbol << std::endl;
    }
};

template<typename Handler>
void process_tick_stream(const std::vector<MarketTick>& ticks, Handler handler) {
    for (const auto& tick : ticks) {
        handler(tick); // Inlined at compile time
    }
}

//=============================================================================
// 4. STD::VARIANT (C++17) - Type-Safe Union
//=============================================================================

struct EquityStrategy {
    void process(const MarketTick& tick) {
        std::cout << "Equity Strategy: " << tick.symbol << std::endl;
    }
    std::string get_type() const { return "Equity"; }
};

struct FixedIncomeStrategy {
    void process(const MarketTick& tick) {
        std::cout << "Fixed Income Strategy: " << tick.symbol << std::endl;
    }
    std::string get_type() const { return "FixedIncome"; }
};

struct CommodityStrategy {
    void process(const MarketTick& tick) {
        std::cout << "Commodity Strategy: " << tick.symbol << std::endl;
    }
    std::string get_type() const { return "Commodity"; }
};

using TradingStrategy = std::variant<EquityStrategy, FixedIncomeStrategy, CommodityStrategy>;

class VariantBasedEngine {
    TradingStrategy strategy_;

public:
    explicit VariantBasedEngine(TradingStrategy s) : strategy_(std::move(s)) {}

    void process_tick(const MarketTick& tick) {
        // Compile-time type dispatch
        std::visit([&tick](auto& s) {
            s.process(tick);
        }, strategy_);
    }

    std::string get_strategy_type() {
        return std::visit([](const auto& s) {
            return s.get_type();
        }, strategy_);
    }
};

//=============================================================================
// 5. TEMPLATE SPECIALIZATION
//=============================================================================

enum class MessageType {
    MARKET_DATA,
    ORDER_UPDATE,
    TRADE_REPORT,
    HEARTBEAT
};

// Primary template
template<MessageType MsgType>
struct MessageProcessor {
    static void process(const uint8_t* data, size_t length) {
        std::cout << "Generic message processing" << std::endl;
    }
};

// Specialized for market data - hot path optimization
template<>
struct MessageProcessor<MessageType::MARKET_DATA> {
    static void process(const uint8_t* data, size_t length) {
        // Ultra-optimized market data parsing
        const auto* tick = reinterpret_cast<const MarketTick*>(data);
        std::cout << "Optimized market data: " << tick->symbol
                  << " @ " << tick->price << std::endl;
    }
};

// Specialized for order updates
template<>
struct MessageProcessor<MessageType::ORDER_UPDATE> {
    static void process(const uint8_t* data, size_t length) {
        const auto* order = reinterpret_cast<const Order*>(data);
        std::cout << "Order processing: " << order->symbol
                  << " qty=" << order->quantity << std::endl;
    }
};

//=============================================================================
// 6. POLICY-BASED DESIGN
//=============================================================================

// Latency policies
struct UltraLowLatencyPolicy {
    static void submit_order(const Order& order) {
        std::cout << "Ultra-fast order submission: " << order.symbol << std::endl;
        // Direct market access, bypass checks
    }

    static constexpr int max_latency_ns = 100;
};

struct StandardLatencyPolicy {
    static void submit_order(const Order& order) {
        std::cout << "Standard order submission: " << order.symbol << std::endl;
        // Normal order processing
    }

    static constexpr int max_latency_ns = 1000;
};

// Risk policies
struct AggressiveRiskPolicy {
    static bool check_order(const Order& order) {
        std::cout << "Aggressive risk check passed" << std::endl;
        return true; // Minimal checks for speed
    }
};

struct ConservativeRiskPolicy {
    static bool check_order(const Order& order) {
        std::cout << "Conservative risk check: validating " << order.symbol << std::endl;
        // Comprehensive risk validation
        return order.quantity < 10000; // Example check
    }
};

template<typename LatencyPolicy, typename RiskPolicy>
class PolicyBasedEngine {
public:
    void submit_order(const Order& order) {
        if (RiskPolicy::check_order(order)) {
            LatencyPolicy::submit_order(order);
        }
    }

    constexpr int get_max_latency() const {
        return LatencyPolicy::max_latency_ns;
    }
};

//=============================================================================
// 7. COMPILE-TIME POLYMORPHISM WITH IF CONSTEXPR (C++17)
//=============================================================================

enum class InstrumentType {
    EQUITY,
    ETF,
    FUTURE,
    OPTION
};

template<InstrumentType Type>
class InstrumentProcessor {
public:
    void process_market_data(const MarketTick& tick) {
        if constexpr (Type == InstrumentType::EQUITY) {
            process_equity(tick);
        } else if constexpr (Type == InstrumentType::ETF) {
            process_etf(tick);
        } else if constexpr (Type == InstrumentType::FUTURE) {
            process_future(tick);
        } else if constexpr (Type == InstrumentType::OPTION) {
            process_option(tick);
        }
    }

private:
    void process_equity(const MarketTick& tick) {
        std::cout << "Equity processing: " << tick.symbol << std::endl;
        // Equity-specific logic compiled in
    }

    void process_etf(const MarketTick& tick) {
        std::cout << "ETF processing: " << tick.symbol << " (basket analysis)" << std::endl;
        // ETF basket calculation logic
    }

    void process_future(const MarketTick& tick) {
        std::cout << "Future processing: " << tick.symbol << " (expiry tracking)" << std::endl;
        // Future-specific calculations
    }

    void process_option(const MarketTick& tick) {
        std::cout << "Option processing: " << tick.symbol << " (Greeks calculation)" << std::endl;
        // Option Greeks and volatility calculations
    }
};

//=============================================================================
// 8. FUNCTION POINTERS (C-style but fast)
//=============================================================================

using TickProcessor = void(*)(const MarketTick&);

void equity_tick_processor(const MarketTick& tick) {
    std::cout << "Function pointer: Equity tick " << tick.symbol << std::endl;
}

void forex_tick_processor(const MarketTick& tick) {
    std::cout << "Function pointer: Forex tick " << tick.symbol << std::endl;
}

void commodity_tick_processor(const MarketTick& tick) {
    std::cout << "Function pointer: Commodity tick " << tick.symbol << std::endl;
}

class FunctionPointerEngine {
    TickProcessor processor_;

public:
    explicit FunctionPointerEngine(TickProcessor p) : processor_(p) {}

    void process_tick(const MarketTick& tick) {
        processor_(tick); // Direct function call - very fast
    }

    void change_processor(TickProcessor new_processor) {
        processor_ = new_processor;
    }
};

//=============================================================================
// 9. CONCEPTS (C++20) - Constrained Templates
//=============================================================================

#if __cplusplus >= 202002L // C++20

template<typename T>
concept TradingStrategy_v2 = requires(T t, const MarketTick& tick, const Order& order) {
    t.on_market_data(tick);
    t.on_order_update(order);
    { t.get_strategy_name() } -> std::convertible_to<std::string>;
};

template<TradingStrategy_v2 S>
class ConceptBasedEngine {
    S strategy_;

public:
    explicit ConceptBasedEngine(S s) : strategy_(std::move(s)) {}

    void process_tick(const MarketTick& tick) {
        strategy_.on_market_data(tick); // Compile-time verified interface
    }

    std::string get_name() {
        return strategy_.get_strategy_name();
    }
};

class MomentumStrategy {
public:
    void on_market_data(const MarketTick& tick) {
        std::cout << "Momentum: Trend analysis for " << tick.symbol << std::endl;
    }

    void on_order_update(const Order& order) {
        std::cout << "Momentum: Order update processed" << std::endl;
    }

    std::string get_strategy_name() const {
        return "Momentum";
    }
};

#endif

//=============================================================================
// 10. TYPE ERASURE PATTERN (Advanced)
//=============================================================================

class StrategyInterface {
public:
    virtual ~StrategyInterface() = default;
    virtual void process_tick(const MarketTick& tick) = 0;
    virtual std::string name() const = 0;
};

template<typename Strategy>
class StrategyWrapper : public StrategyInterface {
    Strategy strategy_;

public:
    explicit StrategyWrapper(Strategy s) : strategy_(std::move(s)) {}

    void process_tick(const MarketTick& tick) override {
        strategy_.on_market_data(tick);
    }

    std::string name() const override {
        return "Wrapped Strategy";
    }
};

class TypeErasedEngine {
    std::unique_ptr<StrategyInterface> strategy_;

public:
    template<typename Strategy>
    explicit TypeErasedEngine(Strategy s)
        : strategy_(std::make_unique<StrategyWrapper<Strategy>>(std::move(s))) {}

    void process_tick(const MarketTick& tick) {
        strategy_->process_tick(tick);
    }
};

//=============================================================================
// PERFORMANCE COMPARISON AND BENCHMARKING
//=============================================================================

class PerformanceTester {
public:
    template<typename Engine, typename... Args>
    static auto benchmark_engine(Engine& engine, const std::vector<MarketTick>& ticks,
                                Args&&... args) {
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& tick : ticks) {
            engine.process_tick(tick);
        }

        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }

    static void run_performance_tests() {
        std::vector<MarketTick> test_ticks = {
            {"AAPL", 150.25, 1634567890123, 1000},
            {"GOOGL", 2800.50, 1634567890124, 500},
            {"MSFT", 330.75, 1634567890125, 750}
        };

        std::cout << "\n=== Performance Comparison ===\n";

        // Template-based (fastest)
        TradingEngine<MarketMakingStrategy> template_engine(MarketMakingStrategy{});
        auto template_time = benchmark_engine(template_engine, test_ticks);
        std::cout << "Template-based: " << template_time.count() << " ns\n";

        // Function pointer (fast)
        FunctionPointerEngine fp_engine(equity_tick_processor);
        auto fp_time = benchmark_engine(fp_engine, test_ticks);
        std::cout << "Function pointer: " << fp_time.count() << " ns\n";

        // Variant (good balance)
        VariantBasedEngine variant_engine(EquityStrategy{});
        auto variant_time = benchmark_engine(variant_engine, test_ticks);
        std::cout << "Variant-based: " << variant_time.count() << " ns\n";
    }
};

//=============================================================================
// EXAMPLE USAGE AND DEMONSTRATIONS
//=============================================================================

void demonstrate_static_polymorphism() {
    std::cout << "=== Static Polymorphism Techniques Demo ===\n\n";

    MarketTick sample_tick{"AAPL", 150.25, 1634567890123, 1000};
    Order sample_order{"AAPL", 150.00, 100, 'B'};

    // 1. Template-based polymorphism
    std::cout << "1. Template-Based Polymorphism:\n";
    TradingEngine<MarketMakingStrategy> mm_engine(MarketMakingStrategy{});
    mm_engine.process_market_data(sample_tick);

    TradingEngine<ArbitrageStrategy> arb_engine(ArbitrageStrategy{});
    arb_engine.process_market_data(sample_tick);
    std::cout << "\n";

    // 2. CRTP
    std::cout << "2. CRTP (Curiously Recurring Template Pattern):\n";
    HighFrequencyStrategy hft_strategy;
    hft_strategy.execute_trading_logic(sample_tick);

    StatisticalArbitrageStrategy stat_strategy;
    stat_strategy.execute_trading_logic(sample_tick);
    std::cout << "\n";

    // 3. Function objects
    std::cout << "3. Function Objects/Functors:\n";
    std::vector<MarketTick> ticks = {sample_tick};
    process_tick_stream(ticks, MarketMakingFunctor{0.02});
    process_tick_stream(ticks, ScalpingFunctor{2});
    std::cout << "\n";

    // 4. std::variant
    std::cout << "4. std::variant (Type-Safe Union):\n";
    VariantBasedEngine equity_engine(EquityStrategy{});
    equity_engine.process_tick(sample_tick);
    std::cout << "Strategy type: " << equity_engine.get_strategy_type() << "\n";

    VariantBasedEngine commodity_engine(CommodityStrategy{});
    commodity_engine.process_tick(sample_tick);
    std::cout << "\n";

    // 5. Template specialization
    std::cout << "5. Template Specialization:\n";
    uint8_t market_data[sizeof(MarketTick)];
    std::memcpy(market_data, &sample_tick, sizeof(MarketTick));

    MessageProcessor<MessageType::MARKET_DATA>::process(market_data, sizeof(MarketTick));

    uint8_t order_data[sizeof(Order)];
    std::memcpy(order_data, &sample_order, sizeof(Order));
    MessageProcessor<MessageType::ORDER_UPDATE>::process(order_data, sizeof(Order));
    std::cout << "\n";

    // 6. Policy-based design
    std::cout << "6. Policy-Based Design:\n";
    PolicyBasedEngine<UltraLowLatencyPolicy, AggressiveRiskPolicy> ultra_engine;
    ultra_engine.submit_order(sample_order);
    std::cout << "Max latency: " << ultra_engine.get_max_latency() << " ns\n";

    PolicyBasedEngine<StandardLatencyPolicy, ConservativeRiskPolicy> standard_engine;
    standard_engine.submit_order(sample_order);
    std::cout << "\n";

    // 7. if constexpr
    std::cout << "7. Compile-time if constexpr:\n";
    InstrumentProcessor<InstrumentType::EQUITY> equity_processor;
    equity_processor.process_market_data(sample_tick);

    InstrumentProcessor<InstrumentType::ETF> etf_processor;
    etf_processor.process_market_data(sample_tick);
    std::cout << "\n";

    // 8. Function pointers
    std::cout << "8. Function Pointers:\n";
    FunctionPointerEngine fp_engine(equity_tick_processor);
    fp_engine.process_tick(sample_tick);

    fp_engine.change_processor(forex_tick_processor);
    fp_engine.process_tick(sample_tick);
    std::cout << "\n";

#if __cplusplus >= 202002L
    // 9. Concepts (C++20)
    std::cout << "9. Concepts (C++20):\n";
    ConceptBasedEngine<MomentumStrategy> concept_engine(MomentumStrategy{});
    concept_engine.process_tick(sample_tick);
    std::cout << "Strategy: " << concept_engine.get_name() << "\n\n";
#endif

    // Performance comparison
    PerformanceTester::run_performance_tests();
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main() {
    try {
        demonstrate_static_polymorphism();

        std::cout << "\n=== Key Benefits for Ultra-Low Latency Systems ===\n";
        std::cout << "✓ Zero runtime overhead - all resolved at compile time\n";
        std::cout << "✓ Better inlining opportunities for compiler optimization\n";
        std::cout << "✓ No vtable lookups - critical for sub-microsecond latency\n";
        std::cout << "✓ Cache-friendly - no indirect memory access\n";
        std::cout << "✓ Type safety with compile-time error checking\n";
        std::cout << "✓ Optimal for hot path processing in trading systems\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

/*
=== PERFORMANCE RANKING FOR TRADING SYSTEMS ===

1. Templates + CRTP           - Fastest (0 overhead)
2. Function Objects           - Fastest (inlined)
3. Template Specialization    - Fastest (compile-time)
4. if constexpr              - Fastest (branches eliminated)
5. Function Pointers         - Fast (direct call)
6. std::variant             - Good (type-safe dispatch)
7. Concepts                 - Fast (compile-time checking)
8. Type Erasure            - Slower (virtual calls)
9. Virtual Functions       - Slowest (vtable lookup)

=== BEST PRACTICES FOR ULTRA-LOW LATENCY ===

• Use templates for hot path processing
• Combine CRTP with policy-based design
• Prefer compile-time dispatch over runtime
• Use concepts for interface validation
• Avoid virtual functions in critical paths
• Template specialization for message parsing
• Function objects for callbacks and handlers

=== WHEN TO USE EACH TECHNIQUE ===

Templates:           General polymorphism, strategy patterns
CRTP:               Base functionality with customization
Function Objects:    Callbacks, event handlers, algorithms
std::variant:       Type switching with safety
Specialization:     Message parsing, protocol handling
Policy Design:      Configurable behavior at compile time
if constexpr:       Conditional compilation logic
Function Pointers:  C-style callbacks, plugin systems
Concepts:           Interface constraints and validation
*/
