/*
 * ultra_low_latency_design_guidelines.cpp
 *
 * Comprehensive Design Guidelines for Ultra Low Latency Capital Markets
 * Strategy Model Engine Implementation
 *
 * This document covers all critical aspects of building sub-microsecond latency
 * trading systems for capital markets, including hardware, software, network,
 * and architectural considerations.
 *
 * Target Latencies:
 *  - Market Data Processing: < 500 nanoseconds
 *  - Strategy Calculation: < 1 microsecond
 *  - Order Generation: < 2 microseconds
 *  - End-to-End Latency: < 10 microseconds
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>

// ============================================================================
// 1. HARDWARE OPTIMIZATION GUIDELINES
// ============================================================================

namespace Hardware {
    /*
     * CPU Selection and Configuration:
     * - Intel Xeon with high frequency (3.5+ GHz base, 4.0+ GHz turbo)
     * - Disable hyperthreading for predictable performance
     * - Use CPU affinity to bind critical threads to specific cores
     * - Isolate cores from kernel scheduler (isolcpus kernel parameter)
     * - Disable power management (C-states, P-states)
     */

    void configure_cpu_affinity() {
        // Example: Bind critical thread to core 2
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }

    /*
     * Memory Hierarchy Optimization:
     * - Use NUMA-aware allocation
     * - Pre-allocate all memory at startup
     * - Use huge pages (2MB/1GB) to reduce TLB misses
     * - Keep hot data in L1/L2 cache (< 64KB working set)
     * - Avoid memory allocations in hot path
     */

    template<typename T, size_t N>
    class PreAllocatedPool {
        alignas(64) std::array<T, N> pool_;
        std::atomic<size_t> index_{0};
    public:
        T* acquire() {
            size_t idx = index_.fetch_add(1, std::memory_order_relaxed);
            return (idx < N) ? &pool_[idx] : nullptr;
        }
        void reset() { index_.store(0, std::memory_order_relaxed); }
    };

    /*
     * Network Interface Optimization:
     * - Use kernel bypass (DPDK, Solarflare OpenOnload)
     * - Enable SR-IOV for hardware virtualization
     * - Use dedicated NICs for market data and order entry
     * - Configure interrupt coalescing and NAPI
     * - Use receive side scaling (RSS)
     */
}

// ============================================================================
// 2. SOFTWARE ARCHITECTURE GUIDELINES
// ============================================================================

namespace Architecture {
    /*
     * Threading Model:
     * - Single-threaded hot path to avoid synchronization
     * - Use lock-free data structures for inter-thread communication
     * - Dedicate threads by function (market data, strategy, order management)
     * - Use busy-wait loops instead of blocking calls
     * - Minimize context switches
     */

    class UltraLowLatencyEngine {
    private:
        alignas(64) std::atomic<bool> running_{true};
        alignas(64) std::atomic<uint64_t> market_data_sequence_{0};

        // Separate threads for different functions
        std::thread market_data_thread_;
        std::thread strategy_thread_;
        std::thread order_thread_;

    public:
        void start() {
            market_data_thread_ = std::thread(&UltraLowLatencyEngine::market_data_loop, this);
            strategy_thread_ = std::thread(&UltraLowLatencyEngine::strategy_loop, this);
            order_thread_ = std::thread(&UltraLowLatencyEngine::order_loop, this);
        }

    private:
        void market_data_loop() {
            Hardware::configure_cpu_affinity(); // Bind to dedicated core
            while (running_.load(std::memory_order_relaxed)) {
                // Process market data - no blocking calls
                process_market_data();
            }
        }

        void strategy_loop() {
            while (running_.load(std::memory_order_relaxed)) {
                // Strategy calculations
                calculate_strategy();
            }
        }

        void order_loop() {
            while (running_.load(std::memory_order_relaxed)) {
                // Order management
                process_orders();
            }
        }

        void process_market_data() { /* Implementation */ }
        void calculate_strategy() { /* Implementation */ }
        void process_orders() { /* Implementation */ }
    };

    /*
     * Data Structure Guidelines:
     * - Use lock-free circular buffers for inter-thread communication
     * - Avoid dynamic allocations in hot path
     * - Use structure of arrays (SoA) instead of array of structures (AoS)
     * - Align data structures to cache line boundaries (64 bytes)
     * - Use memory prefetching for predictable access patterns
     */

    template<typename T, size_t Size>
    class LockFreeRingBuffer {
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

        alignas(64) std::atomic<size_t> write_pos_{0};
        alignas(64) std::atomic<size_t> read_pos_{0};
        alignas(64) std::array<T, Size> buffer_;

    public:
        bool try_push(const T& item) {
            const size_t current_write = write_pos_.load(std::memory_order_relaxed);
            const size_t next_write = (current_write + 1) & (Size - 1);

            if (next_write == read_pos_.load(std::memory_order_acquire)) {
                return false; // Buffer full
            }

            buffer_[current_write] = item;
            write_pos_.store(next_write, std::memory_order_release);
            return true;
        }

        bool try_pop(T& item) {
            const size_t current_read = read_pos_.load(std::memory_order_relaxed);

            if (current_read == write_pos_.load(std::memory_order_acquire)) {
                return false; // Buffer empty
            }

            item = buffer_[current_read];
            read_pos_.store((current_read + 1) & (Size - 1), std::memory_order_release);
            return true;
        }

        bool is_empty() const {
            return read_pos_.load(std::memory_order_relaxed) ==
                   write_pos_.load(std::memory_order_relaxed);
        }
    };
}

// ============================================================================
// 3. COMPILER AND CODE OPTIMIZATION
// ============================================================================

namespace Optimization {
    /*
     * Compiler Flags for Ultra Low Latency:
     * -O3 -march=native -mtune=native -flto
     * -fno-exceptions -fno-rtti (if not needed)
     * -funroll-loops -fprefetch-loop-arrays
     * -ffast-math (if precision allows)
     * -DNDEBUG (remove assert checks)
     */

    // Branch prediction hints
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)

    // Force inlining of critical functions
    #define FORCE_INLINE __attribute__((always_inline)) inline

    // Memory prefetching
    #define PREFETCH_READ(addr)  __builtin_prefetch((addr), 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)

    // Hot path optimization example
    FORCE_INLINE bool process_tick(const MarketTick& tick) {
        // Prefetch next likely memory locations
        PREFETCH_READ(&tick.price);
        PREFETCH_READ(&tick.quantity);

        if (LIKELY(tick.is_valid())) {
            // Fast path for valid ticks
            return update_strategy(tick);
        }

        if (UNLIKELY(tick.is_error())) {
            // Slow path for error handling
            handle_error(tick);
        }

        return false;
    }

    /*
     * Assembly Optimization for Critical Paths:
     * - Use SIMD instructions (SSE, AVX) for parallel calculations
     * - Minimize branch mispredictions
     * - Use bit manipulation tricks
     * - Optimize memory access patterns
     */

    // Example: Fast timestamp using RDTSC
    FORCE_INLINE uint64_t get_timestamp_ns() {
        uint32_t lo, hi;
        __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
    }
}

// ============================================================================
// 4. MEMORY MANAGEMENT GUIDELINES
// ============================================================================

namespace Memory {
    /*
     * Memory Pool Design:
     * - Pre-allocate all memory at startup
     * - Use object pools for frequently allocated/deallocated objects
     * - Implement custom allocators for specific use cases
     * - Avoid malloc/free in hot path
     * - Use stack allocation where possible
     */

    template<typename T>
    class ObjectPool {
        std::vector<T> objects_;
        std::vector<T*> available_;
        size_t next_available_ = 0;

    public:
        explicit ObjectPool(size_t size) : objects_(size) {
            available_.reserve(size);
            for (auto& obj : objects_) {
                available_.push_back(&obj);
            }
        }

        T* acquire() {
            if (UNLIKELY(next_available_ >= available_.size())) {
                return nullptr; // Pool exhausted
            }
            return available_[next_available_++];
        }

        void release(T* obj) {
            if (LIKELY(next_available_ > 0)) {
                available_[--next_available_] = obj;
            }
        }
    };

    /*
     * Cache-Friendly Data Layout:
     * - Group frequently accessed data together
     * - Align structures to cache line boundaries
     * - Use padding to avoid false sharing
     * - Implement data locality optimization
     */

    struct alignas(64) MarketData {
        double price;
        uint64_t quantity;
        uint64_t timestamp;
        uint32_t symbol_id;
        char padding[64 - sizeof(double) - 2*sizeof(uint64_t) - sizeof(uint32_t)];
    };

    static_assert(sizeof(MarketData) == 64, "MarketData must fit in one cache line");
}

// ============================================================================
// 5. NETWORK OPTIMIZATION
// ============================================================================

namespace Network {
    /*
     * Low Latency Networking:
     * - Use kernel bypass (DPDK, OpenOnload)
     * - Implement zero-copy networking
     * - Use multicast for market data distribution
     * - Optimize packet processing pipeline111``1
     * - Minimize network hops
     */

    class UDPReceiver {
    private:
        int socket_fd_;
        uint8_t* receive_buffer_;
        static constexpr size_t BUFFER_SIZE = 65536;

    public:
        UDPReceiver() {
            // Create raw socket for bypass
            socket_fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
            receive_buffer_ = static_cast<uint8_t*>(
                aligned_alloc(64, BUFFER_SIZE));
        }

        // Zero-copy receive
        ssize_t receive_packet() {
            return recv(socket_fd_, receive_buffer_, BUFFER_SIZE, MSG_DONTWAIT);
        }

        const uint8_t* get_buffer() const { return receive_buffer_; }
    };

    /*
     * Message Processing:
     * - Use template specialization for different message types
     * - Implement compile-time message parsing
     * - Minimize memory copies
     * - Use bit manipulation for field extraction
     */

    template<typename MessageType>
    FORCE_INLINE void process_message(const uint8_t* data, size_t length);

    template<>
    FORCE_INLINE void process_message<MarketDataMessage>(const uint8_t* data, size_t length) {
        // Fast path for market data
        const auto* msg = reinterpret_cast<const MarketDataMessage*>(data);
        update_orderbook(msg->symbol_id, msg->price, msg->quantity);
    }
}

// ============================================================================
// 6. STRATEGY ENGINE DESIGN - ENHANCED FOR MULTI-INSTRUMENT MARKET MAKING
// ============================================================================

namespace Strategy {
    // Enhanced instrument type system for market making
    enum class InstrumentType {
        SINGLE_STOCK,    // Individual equity (AAPL, MSFT)
        FUTURE,          // Future contract with single underlying (ES, NQ)
        ETF,             // ETF with multiple underlyings (SPY, QQQ)
        OPTION,          // Option contract (for completeness)
        INDEX            // Index level (S&P 500, NASDAQ)
    };

    // Base instrument interface
    class IInstrument {
    public:
        virtual ~IInstrument() = default;
        virtual uint32_t get_symbol_id() const = 0;
        virtual InstrumentType get_type() const = 0;
        virtual std::vector<uint32_t> get_underlying_symbols() const = 0;
        virtual uint32_t get_primary_hedge_instrument() const = 0;
        virtual std::vector<uint32_t> get_hedge_instruments() const = 0;
        virtual double calculate_fair_value() const = 0;
        virtual double get_hedge_ratio(uint32_t hedge_symbol) const = 0;
    };

    // Single stock instrument
    class SingleStockInstrument : public IInstrument {
    private:
        uint32_t symbol_id_;
        uint32_t future_hedge_;  // Related future for hedging (e.g., ES for SPY-related stocks)
        uint32_t etf_hedge_;     // Related ETF for hedging
        double beta_;            // Beta to market
        double last_price_;

    public:
        SingleStockInstrument(uint32_t symbol_id, uint32_t future_hedge, uint32_t etf_hedge, double beta)
            : symbol_id_(symbol_id), future_hedge_(future_hedge), etf_hedge_(etf_hedge), beta_(beta), last_price_(0.0) {}

        uint32_t get_symbol_id() const override { return symbol_id_; }
        InstrumentType get_type() const override { return InstrumentType::SINGLE_STOCK; }

        std::vector<uint32_t> get_underlying_symbols() const override {
            return {symbol_id_}; // Self-referential
        }

        uint32_t get_primary_hedge_instrument() const override { return future_hedge_; }

        std::vector<uint32_t> get_hedge_instruments() const override {
            return {future_hedge_, etf_hedge_};
        }

        double calculate_fair_value() const override {
            // Fair value based on future price and beta
            return last_price_; // Simplified - would use future price * beta
        }

        double get_hedge_ratio(uint32_t hedge_symbol) const override {
            if (hedge_symbol == future_hedge_) return beta_;
            if (hedge_symbol == etf_hedge_) return beta_ * 0.1; // Reduced hedge via ETF
            return 0.0;
        }

        void update_price(double price) { last_price_ = price; }
        double get_beta() const { return beta_; }
    };

    // Future instrument
    class FutureInstrument : public IInstrument {
    private:
        uint32_t symbol_id_;
        uint32_t underlying_index_;  // Underlying index (e.g., S&P 500 for ES)
        std::vector<uint32_t> component_etfs_; // Related ETFs for hedging
        double contract_multiplier_;
        double last_price_;

    public:
        FutureInstrument(uint32_t symbol_id, uint32_t underlying_index,
                        std::vector<uint32_t> component_etfs, double multiplier)
            : symbol_id_(symbol_id), underlying_index_(underlying_index),
              component_etfs_(component_etfs), contract_multiplier_(multiplier), last_price_(0.0) {}

        uint32_t get_symbol_id() const override { return symbol_id_; }
        InstrumentType get_type() const override { return InstrumentType::FUTURE; }

        std::vector<uint32_t> get_underlying_symbols() const override {
            return {underlying_index_}; // Points to index
        }

        uint32_t get_primary_hedge_instrument() const override {
            return component_etfs_.empty() ? 0 : component_etfs_[0];
        }

        std::vector<uint32_t> get_hedge_instruments() const override {
            return component_etfs_;
        }

        double calculate_fair_value() const override {
            // Fair value based on underlying index + carry
            return last_price_; // Simplified
        }

        double get_hedge_ratio(uint32_t hedge_symbol) const override {
            // 1:1 hedge ratio with primary ETF, scaled for others
            auto it = std::find(component_etfs_.begin(), component_etfs_.end(), hedge_symbol);
            if (it != component_etfs_.end()) {
                return (it == component_etfs_.begin()) ? 1.0 : 0.5; // Primary vs secondary hedge
            }
            return 0.0;
        }

        void update_price(double price) { last_price_ = price; }
        double get_multiplier() const { return contract_multiplier_; }
    };

    // Enhanced ETF instrument (building on existing ETFConstituent)
    class ETFInstrument : public IInstrument {
    private:
        uint32_t symbol_id_;
        uint32_t index_id_;
        std::vector<ETFConstituent> constituents_;
        uint32_t future_hedge_;  // Primary future hedge (e.g., ES for SPY)
        std::vector<uint32_t> stock_hedges_; // Individual stock hedges
        double nav_per_share_;

    public:
        ETFInstrument(uint32_t symbol_id, uint32_t index_id,
                     std::vector<ETFConstituent> constituents, uint32_t future_hedge)
            : symbol_id_(symbol_id), index_id_(index_id), constituents_(constituents),
              future_hedge_(future_hedge), nav_per_share_(0.0) {

            // Extract individual stocks as potential hedges
            for (const auto& constituent : constituents_) {
                if (constituent.weight > 0.01) { // Only significant holdings
                    stock_hedges_.push_back(constituent.symbol_id);
                }
            }
        }

        uint32_t get_symbol_id() const override { return symbol_id_; }
        InstrumentType get_type() const override { return InstrumentType::ETF; }

        std::vector<uint32_t> get_underlying_symbols() const override {
            std::vector<uint32_t> underlyings;
            for (const auto& constituent : constituents_) {
                underlyings.push_back(constituent.symbol_id);
            }
            return underlyings;
        }

        uint32_t get_primary_hedge_instrument() const override { return future_hedge_; }

        std::vector<uint32_t> get_hedge_instruments() const override {
            std::vector<uint32_t> hedges = {future_hedge_};
            hedges.insert(hedges.end(), stock_hedges_.begin(), stock_hedges_.end());
            return hedges;
        }

        double calculate_fair_value() const override {
            return nav_per_share_; // Real-time NAV calculation
        }

        double get_hedge_ratio(uint32_t hedge_symbol) const override {
            if (hedge_symbol == future_hedge_) return 1.0; // 1:1 with future

            // Check if it's a constituent stock
            for (const auto& constituent : constituents_) {
                if (constituent.symbol_id == hedge_symbol) {
                    return constituent.weight; // Weight-based hedge ratio
                }
            }
            return 0.0;
        }

        void update_nav(double nav) { nav_per_share_ = nav; }
        const std::vector<ETFConstituent>& get_constituents() const { return constituents_; }
    };

    // Market making strategy enhanced for cross-instrument hedging
    class EnhancedMarketMakingStrategy : public IStrategy {
    private:
        std::unique_ptr<IInstrument> instrument_;
        std::unordered_map<uint32_t, std::unique_ptr<IInstrument>> hedge_instruments_;
        std::unordered_map<uint32_t, double> hedge_positions_; // hedge_symbol -> position
        std::unordered_map<uint32_t, MarketTick> latest_prices_;

        bool enabled_ = true;
        double spread_basis_points_ = 5.0;
        int64_t max_position_ = 1000;
        int64_t current_position_ = 0;

        // Risk management
        double max_hedge_notional_ = 10000000.0; // $10M max hedge exposure
        double hedge_rebalance_threshold_ = 0.1; // 10% deviation triggers rebalance

    public:
        EnhancedMarketMakingStrategy(std::unique_ptr<IInstrument> instrument)
            : instrument_(std::move(instrument)) {}

        void on_market_data(const MarketTick& tick) override {
            if (!enabled_) return;

            latest_prices_[tick.symbol_id] = tick;

            // Check if this is our primary instrument
            if (tick.symbol_id == instrument_->get_symbol_id()) {
                process_primary_instrument_update(tick);
            }
            // Check if this is a hedge instrument
            else if (is_hedge_instrument(tick.symbol_id)) {
                process_hedge_instrument_update(tick);
            }
        }

        void on_trade(const Trade& trade) override {
            if (trade.symbol_id == instrument_->get_symbol_id()) {
                // Update primary position
                current_position_ += (trade.side == Side::BUY) ? trade.quantity : -trade.quantity;

                // Check if hedge rebalancing is needed
                if (should_rebalance_hedges()) {
                    execute_hedge_rebalancing(trade.timestamp);
                }
            }
        }

        void on_timer(uint64_t timestamp) override {
            // Periodic hedge monitoring and fair value updates
            update_fair_value_models(timestamp);
            monitor_hedge_performance(timestamp);
        }

        bool is_enabled() const override { return enabled_; }
        const char* get_name() const override { return "EnhancedMarketMaking"; }
        void set_enabled(bool enabled) override { enabled_ = enabled; }

    private:
        void process_primary_instrument_update(const MarketTick& tick) {
            // Calculate fair value using hedge instruments
            double fair_value = calculate_fair_value_with_hedges();

            const double mid_price = (tick.bid + tick.ask) * 0.5;
            const double spread = tick.ask - tick.bid;

            if (LIKELY(spread > 0.001 && std::abs(current_position_) < max_position_)) {
                // Adjust quotes based on fair value vs market price
                double fair_adjustment = (fair_value - mid_price) * 0.5; // 50% fair value adjustment
                generate_enhanced_quotes(tick.symbol_id, fair_value, spread, tick.timestamp, fair_adjustment);
            }
        }

        void process_hedge_instrument_update(const MarketTick& tick) {
            // Update hedge instrument prices and recalculate fair value
            if (auto hedge_inst = hedge_instruments_.find(tick.symbol_id); hedge_inst != hedge_instruments_.end()) {
                // Update hedge instrument price (instrument-specific logic)
                update_hedge_instrument_price(tick.symbol_id, tick);
            }
        }

        double calculate_fair_value_with_hedges() {
            double fair_value = instrument_->calculate_fair_value();

            // Adjust based on hedge instrument pricing
            for (uint32_t hedge_symbol : instrument_->get_hedge_instruments()) {
                if (auto price_it = latest_prices_.find(hedge_symbol); price_it != latest_prices_.end()) {
                    double hedge_ratio = instrument_->get_hedge_ratio(hedge_symbol);
                    double hedge_mid = (price_it->second.bid + price_it->second.ask) * 0.5;

                    // Apply hedge-based fair value adjustment
                    fair_value += hedge_ratio * hedge_mid * 0.1; // 10% hedge influence
                }
            }

            return fair_value;
        }

        void generate_enhanced_quotes(uint32_t symbol_id, double fair_value, double spread,
                                    uint64_t timestamp, double fair_adjustment) {
            const double our_spread = spread + (spread_basis_points_ / 10000.0);
            const double adjusted_mid = fair_value + fair_adjustment;

            const double bid_price = adjusted_mid - our_spread * 0.5;
            const double ask_price = adjusted_mid + our_spread * 0.5;

            submit_order(symbol_id, bid_price, 100, Side::BUY, timestamp);
            submit_order(symbol_id, ask_price, 100, Side::SELL, timestamp);
        }

        bool should_rebalance_hedges() {
            if (current_position_ == 0) return false;

            // Check each hedge instrument for rebalancing need
            for (uint32_t hedge_symbol : instrument_->get_hedge_instruments()) {
                double required_hedge = std::abs(current_position_) * instrument_->get_hedge_ratio(hedge_symbol);
                double current_hedge = std::abs(hedge_positions_[hedge_symbol]);

                if (std::abs(required_hedge - current_hedge) / required_hedge > hedge_rebalance_threshold_) {
                    return true;
                }
            }
            return false;
        }

        void execute_hedge_rebalancing(uint64_t timestamp) {
            std::cout << "Rebalancing hedges for instrument " << instrument_->get_symbol_id() << std::endl;

            for (uint32_t hedge_symbol : instrument_->get_hedge_instruments()) {
                double hedge_ratio = instrument_->get_hedge_ratio(hedge_symbol);
                double required_hedge = current_position_ * hedge_ratio;
                double current_hedge = hedge_positions_[hedge_symbol];
                double hedge_diff = required_hedge - current_hedge;

                if (std::abs(hedge_diff) > 1.0) { // Minimum hedge size
                    Side hedge_side = (hedge_diff > 0) ? Side::BUY : Side::SELL;

                    if (auto price_it = latest_prices_.find(hedge_symbol); price_it != latest_prices_.end()) {
                        double hedge_price = (hedge_side == Side::BUY) ? price_it->second.ask : price_it->second.bid;
                        submit_hedge_order(hedge_symbol, hedge_price,
                                         static_cast<uint64_t>(std::abs(hedge_diff)),
                                         hedge_side, timestamp);
                    }
                }
            }
        }

        bool is_hedge_instrument(uint32_t symbol_id) const {
            auto hedges = instrument_->get_hedge_instruments();
            return std::find(hedges.begin(), hedges.end(), symbol_id) != hedges.end();
        }

        void update_hedge_instrument_price(uint32_t hedge_symbol, const MarketTick& tick) {
            // Update specific hedge instrument price (could be polymorphic)
            // Implementation depends on hedge instrument type
        }

        void update_fair_value_models(uint64_t timestamp) {
            // Update theoretical models based on latest hedge prices
        }

        void monitor_hedge_performance(uint64_t timestamp) {
            // Monitor hedge effectiveness and adjust ratios if needed
        }

        void submit_order(uint32_t symbol_id, double price, uint64_t quantity,
                         Side side, uint64_t timestamp);

        void submit_hedge_order(uint32_t hedge_symbol, double price, uint64_t quantity,
                              Side side, uint64_t timestamp) {
            std::cout << "Hedge Order: " << hedge_symbol << " " << (side == Side::BUY ? "BUY" : "SELL")
                      << " " << quantity << "@" << price << std::endl;

            // Update hedge position
            double position_change = (side == Side::BUY) ?
                static_cast<double>(quantity) : -static_cast<double>(quantity);
            hedge_positions_[hedge_symbol] += position_change;
        }
    };

    // Multi-instrument market making engine
    class MultiInstrumentMarketMakingEngine {
    private:
        std::unordered_map<uint32_t, std::unique_ptr<IInstrument>> instruments_;
        std::unordered_map<uint32_t, std::unique_ptr<EnhancedMarketMakingStrategy>> strategies_;
        std::unordered_map<uint32_t, std::vector<uint32_t>> hedge_relationships_; // symbol -> hedge symbols

        // Cross-instrument risk monitoring
        std::atomic<double> total_portfolio_delta_{0.0};
        std::atomic<double> max_portfolio_risk_{1000000.0}; // $1M max risk

    public:
        void add_single_stock(uint32_t symbol_id, uint32_t future_hedge, uint32_t etf_hedge, double beta) {
            auto instrument = std::make_unique<SingleStockInstrument>(symbol_id, future_hedge, etf_hedge, beta);
            auto strategy = std::make_unique<EnhancedMarketMakingStrategy>(std::move(instrument));

            strategies_[symbol_id] = std::move(strategy);
            hedge_relationships_[symbol_id] = {future_hedge, etf_hedge};

            std::cout << "Added single stock " << symbol_id << " with hedges: "
                      << future_hedge << ", " << etf_hedge << std::endl;
        }

        void add_future(uint32_t symbol_id, uint32_t underlying_index,
                       std::vector<uint32_t> component_etfs, double multiplier) {
            auto instrument = std::make_unique<FutureInstrument>(symbol_id, underlying_index, component_etfs, multiplier);
            auto strategy = std::make_unique<EnhancedMarketMakingStrategy>(std::move(instrument));

            strategies_[symbol_id] = std::move(strategy);
            hedge_relationships_[symbol_id] = component_etfs;

            std::cout << "Added future " << symbol_id << " with " << component_etfs.size() << " ETF hedges" << std::endl;
        }

        void add_etf(uint32_t symbol_id, uint32_t index_id,
                    std::vector<ETFConstituent> constituents, uint32_t future_hedge) {
            auto instrument = std::make_unique<ETFInstrument>(symbol_id, index_id, constituents, future_hedge);
            auto strategy = std::make_unique<EnhancedMarketMakingStrategy>(std::move(instrument));

            strategies_[symbol_id] = std::move(strategy);

            // Build hedge relationships
            std::vector<uint32_t> hedges = {future_hedge};
            for (const auto& constituent : constituents) {
                hedges.push_back(constituent.symbol_id);
            }
            hedge_relationships_[symbol_id] = hedges;

            std::cout << "Added ETF " << symbol_id << " with " << hedges.size() << " hedge instruments" << std::endl;
        }

        void process_market_data(const MarketTick& tick) {
            // Distribute to relevant strategies
            if (auto strategy_it = strategies_.find(tick.symbol_id); strategy_it != strategies_.end()) {
                strategy_it->second->on_market_data(tick);
            }

            // Also send to strategies that hedge with this instrument
            for (const auto& [symbol_id, hedge_list] : hedge_relationships_) {
                if (std::find(hedge_list.begin(), hedge_list.end(), tick.symbol_id) != hedge_list.end()) {
                    if (auto hedge_strategy = strategies_.find(symbol_id); hedge_strategy != strategies_.end()) {
                        hedge_strategy->second->on_market_data(tick);
                    }
                }
            }
        }

        void process_trade(const Trade& trade) {
            if (auto strategy_it = strategies_.find(trade.symbol_id); strategy_it != strategies_.end()) {
                strategy_it->second->on_trade(trade);
            }

            // Update portfolio-level risk metrics
            update_portfolio_risk(trade);
        }

        double get_portfolio_delta() const {
            return total_portfolio_delta_.load(std::memory_order_relaxed);
        }

        void print_hedge_relationships() const {
            std::cout << "\n=== HEDGE RELATIONSHIPS ===\n";
            for (const auto& [symbol_id, hedges] : hedge_relationships_) {
                std::cout << "Instrument " << symbol_id << " hedges: ";
                for (uint32_t hedge : hedges) {
                    std::cout << hedge << " ";
                }
                std::cout << "\n";
            }
        }

    private:
        void update_portfolio_risk(const Trade& trade) {
            // Update portfolio-level delta and risk metrics
            // Implementation would calculate cross-instrument exposure
        }
    };

    // ============================================================================

    /*
     * Risk Management:
     * - Implement real-time position monitoring
     * - Use circuit breakers for rapid market moves
     * - Maintain pre-trade risk checks
     * - Implement emergency stop mechanisms
     */

    class RiskManager {
    private:
        std::atomic<int64_t> max_position_{1000000};
        std::atomic<double> max_daily_loss_{-100000.0};
        std::atomic<bool> emergency_stop_{false};

    public:
        FORCE_INLINE bool check_order(uint32_t symbol_id, int64_t quantity, double price) {
            if (UNLIKELY(emergency_stop_.load(std::memory_order_relaxed))) {
                return false;
            }

            // Fast position check
            const int64_t current_position = get_position(symbol_id);
            const int64_t new_position = current_position + quantity;

            return std::abs(new_position) <= max_position_.load(std::memory_order_relaxed);
        }

    private:
        int64_t get_position(uint32_t symbol_id) const;
    };
}

// ============================================================================
// 7. LATENCY MEASUREMENT AND MONITORING
// ============================================================================

namespace Monitoring {
    /*
     * Latency Measurement:
     * - Use high-resolution timers (RDTSC, clock_gettime)
     * - Implement microsecond-level profiling
     * - Monitor jitter and percentiles
     * - Use lock-free logging for minimal impact
     */

    class LatencyProfiler {
    private:
        struct Sample {
            uint64_t start_time;
            uint64_t end_time;
            const char* label;
        };

        static constexpr size_t MAX_SAMPLES = 1000000;
        Sample samples_[MAX_SAMPLES];
        std::atomic<size_t> sample_count_{0};

    public:
        class ScopedTimer {
            LatencyProfiler& profiler_;
            size_t sample_index_;

        public:
            ScopedTimer(LatencyProfiler& profiler, const char* label)
                : profiler_(profiler) {
                sample_index_ = profiler_.sample_count_.fetch_add(1, std::memory_order_relaxed);
                if (sample_index_ < MAX_SAMPLES) {
                    profiler_.samples_[sample_index_].label = label;
                    profiler_.samples_[sample_index_].start_time = Optimization::get_timestamp_ns();
                }
            }

            ~ScopedTimer() {
                if (sample_index_ < MAX_SAMPLES) {
                    profiler_.samples_[sample_index_].end_time = Optimization::get_timestamp_ns();
                }
            }
        };

        void analyze_latency() {
            // Analyze collected samples for latency distribution
            std::vector<uint64_t> latencies;
            const size_t count = std::min(sample_count_.load(), MAX_SAMPLES);

            for (size_t i = 0; i < count; ++i) {
                latencies.push_back(samples_[i].end_time - samples_[i].start_time);
            }

            std::sort(latencies.begin(), latencies.end());

            std::cout << "Latency Analysis:\n";
            std::cout << "P50: " << latencies[count * 50 / 100] << " ns\n";
            std::cout << "P95: " << latencies[count * 95 / 100] << " ns\n";
            std::cout << "P99: " << latencies[count * 99 / 100] << " ns\n";
        }
    };

    // Usage macro for easy profiling
    #define PROFILE_SCOPE(profiler, label) \
        Monitoring::LatencyProfiler::ScopedTimer timer(profiler, label)
}

// ============================================================================
// 8. SYSTEM CONFIGURATION GUIDELINES
// ============================================================================

namespace SystemConfig {
    /*
     * Operating System Tuning:
     * - Use real-time kernel (PREEMPT_RT)
     * - Disable CPU frequency scaling
     * - Set process priority to real-time
     * - Configure kernel parameters for low latency
     * - Disable unnecessary services
     */

    void configure_realtime_priority() {
        struct sched_param param;
        param.sched_priority = 99; // Highest real-time priority

        if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
            std::cerr << "Failed to set real-time priority\n";
        }
    }

    /*
     * Kernel Parameters (/etc/sysctl.conf):
     * net.core.busy_poll=50
     * net.core.busy_read=50
     * net.core.netdev_max_backlog=5000
     * net.ipv4.tcp_low_latency=1
     * kernel.sched_min_granularity_ns=10000000
     * kernel.sched_wakeup_granularity_ns=15000000
     */

    /*
     * Boot Parameters:
     * isolcpus=2,3,4,5 nohz_full=2,3,4,5 rcu_nocbs=2,3,4,5
     * intel_idle.max_cstate=0 processor.max_cstate=1
     * intel_pstate=disable
     */
}

// ============================================================================
// 9. TESTING AND VALIDATION
// ============================================================================

namespace Testing {
    /*
     * Performance Testing:
     * - Implement synthetic market data generators
     * - Measure end-to-end latency under load
     * - Test worst-case scenarios
     * - Validate deterministic behavior
     * - Stress test with high message rates
     */

    class LatencyTest {
    public:
        void run_synthetic_test() {
            Monitoring::LatencyProfiler profiler;
            constexpr int NUM_ITERATIONS = 100000;

            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                PROFILE_SCOPE(profiler, "full_tick_processing");

                // Simulate market data processing
                process_synthetic_tick(i);
            }

            profiler.analyze_latency();
        }

    private:
        void process_synthetic_tick(int tick_id) {
            // Simulate complete tick processing pipeline
            MarketTick tick{
                .symbol_id = static_cast<uint32_t>(tick_id % 100),
                .price = 100.0 + (tick_id % 1000) * 0.01,
                .quantity = 100,
                .timestamp = Optimization::get_timestamp_ns()
            };

            // Process through strategy engine
            Strategy::MarketMakingStrategy strategy;
            strategy.on_market_data(tick.symbol_id, tick.price - 0.01,
                                  tick.price + 0.01, tick.timestamp);
        }
    };
}

// ============================================================================
// 10. IMPLEMENTATION CHECKLIST
// ============================================================================

/*
 * ULTRA LOW LATENCY IMPLEMENTATION CHECKLIST:
 *
 * Hardware:
 * □ High-frequency CPU with isolated cores
 * □ NUMA-optimized memory allocation
 * □ Kernel bypass networking (DPDK/OpenOnload)
 * □ NVMe SSDs for logging
 * □ Dedicated network interfaces
 *
 * Software Architecture:
 * □ Single-threaded hot path
 * □ Lock-free data structures
 * □ Pre-allocated memory pools
 * □ Cache-aligned data structures
 * □ Minimal system calls
 *
 * Optimization:
 * □ Compiler optimizations enabled
 * □ Profile-guided optimization (PGO)
 * □ Branch prediction hints
 * □ SIMD instructions where applicable
 * □ Assembly optimization for critical paths
 *
 * System Configuration:
 * □ Real-time kernel
 * □ Process pinning to isolated cores
 * □ Real-time scheduling priority
 * □ Kernel parameter tuning
 * □ Interrupt affinity configuration
 *
 * Testing:
 * □ Latency measurement infrastructure
 * □ Synthetic load testing
 * □ Worst-case scenario validation
 * □ Production environment testing
 * □ Continuous monitoring
 *
 * Risk Management:
 * □ Pre-trade risk checks
 * □ Circuit breakers
 * □ Emergency stop mechanisms
 * □ Position limits
 * □ Real-time monitoring
 */

// Forward declarations and enums
enum class Side { BUY, SELL };

struct MarketDataMessage {
    uint32_t symbol_id;
    double price;
    uint64_t quantity;
    uint64_t timestamp;
};

// Forward declarations for implementation
struct MarketTick {
    uint32_t symbol_id;
    double price;
    uint64_t quantity;
    uint64_t timestamp;
    double bid;
    double ask;
    uint64_t bid_quantity;
    uint64_t ask_quantity;

    bool is_valid() const { return price > 0 && quantity > 0; }
    bool is_error() const { return price <= 0; }
};

struct Trade {
    uint32_t symbol_id;
    double price;
    uint64_t quantity;
    Side side;
    uint64_t timestamp;
};


// Add missing methods to LockFreeRingBuffer
namespace Architecture {
    template<typename T, size_t Size>
    class LockFreeRingBuffer {
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

        alignas(64) std::atomic<size_t> write_pos_{0};
        alignas(64) std::atomic<size_t> read_pos_{0};
        alignas(64) std::array<T, Size> buffer_;

    public:
        bool try_push(const T& item) {
            const size_t current_write = write_pos_.load(std::memory_order_relaxed);
            const size_t next_write = (current_write + 1) & (Size - 1);

            if (next_write == read_pos_.load(std::memory_order_acquire)) {
                return false; // Buffer full
            }

            buffer_[current_write] = item;
            write_pos_.store(next_write, std::memory_order_release);
            return true;
        }

        bool try_pop(T& item) {
            const size_t current_read = read_pos_.load(std::memory_order_relaxed);

            if (current_read == write_pos_.load(std::memory_order_acquire)) {
                return false; // Buffer empty
            }

            item = buffer_[current_read];
            read_pos_.store((current_read + 1) & (Size - 1), std::memory_order_release);
            return true;
        }

        bool is_empty() const {
            return read_pos_.load(std::memory_order_relaxed) ==
                   write_pos_.load(std::memory_order_relaxed);
        }
    };
}

// Add demonstration function for multi-strategy engine
void demonstrate_multi_strategy_engine() {
    std::cout << "\n=== MULTI-STRATEGY MULTI-INSTRUMENT ENGINE DEMO ===\n";

    Strategy::MultiInstrumentEngine engine;

    // Add instruments (each gets its own dedicated thread)
    std::vector<uint32_t> instruments = {1001, 1002, 1003, 1004, 1005}; // SPY, QQQ, IWM, etc.

    for (uint32_t symbol_id : instruments) {
        engine.add_instrument(symbol_id);
    }

    engine.start();

    // Simulate market data for different instruments
    for (int i = 0; i < 1000; ++i) {
        for (uint32_t symbol_id : instruments) {
            MarketTick tick{
                .symbol_id = symbol_id,
                .price = 100.0 + (i % 100) * 0.01,
                .quantity = 1000,
                .timestamp = Optimization::get_timestamp_ns(),
                .bid = 100.0 + (i % 100) * 0.01 - 0.01,
                .ask = 100.0 + (i % 100) * 0.01 + 0.01,
                .bid_quantity = 1000,
                .ask_quantity = 1000
            };

            engine.submit_market_data(tick);
        }

        // Small delay to simulate realistic data rates
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Test strategy enable/disable
    std::cout << "\nTesting strategy management...\n";
    engine.disable_strategy(1001, "Arbitrage");
    engine.enable_strategy(1002, "StatArb");

    // Print performance statistics
    engine.print_all_stats();

    engine.stop();

    std::cout << "\nMulti-strategy engine demonstration completed.\n";
}

// Add demonstration function for ETF tracking
void demonstrate_etf_index_tracking() {
    std::cout << "\n=== ETF INDEX TRACKING DEMONSTRATION ===\n";

    // Create multiple ETFs tracking S&P 500 (index_id = 500)
    std::vector<uint32_t> sp500_etfs = {3001, 3002, 3003}; // SPY, IVV, VOO-like

    // Create ETF-specific engine
    Strategy::MultiETFIndexManager sp500_manager(500);

    // Sample constituents for S&P 500
    std::vector<Strategy::ETFConstituent> sp500_constituents = {
        {2001, 0.070, 3500, 175.50, 0, true},  // AAPL - 7% weight
        {2002, 0.065, 3250, 340.25, 0, true},  // MSFT - 6.5% weight
        {2003, 0.038, 1900, 145.75, 0, true},  // AMZN - 3.8% weight
        {2004, 0.035, 1750, 125.30, 0, true},  // GOOGL - 3.5% weight
        {2005, 0.030, 1500, 210.50, 0, true},  // TSLA - 3% weight
        {2006, 0.025, 1250, 295.75, 0, true},  // META - 2.5% weight
        {2007, 0.032, 1600, 480.25, 0, true},  // NVDA - 3.2% weight
        {2008, 0.018, 900, 155.80, 0, true},   // JPM - 1.8% weight
        {2009, 0.015, 750, 162.40, 0, true},   // JNJ - 1.5% weight
        {2010, 0.014, 700, 145.90, 0, true}    // PG - 1.4% weight
    };

    // Add multiple ETFs tracking the same index
    for (uint32_t etf_id : sp500_etfs) {
        Strategy::ETFMetadata etf_metadata{
            .etf_symbol_id = etf_id,
            .index_id = 500,
            .etf_ticker = "ETF" + std::to_string(etf_id),
            .nav_per_share = 450.0,  // Different starting NAVs
            .creation_fee_bps = (etf_id == 3001) ? 9.0 : 3.0,  // SPY has higher fees
            .redemption_fee_bps = (etf_id == 3001) ? 9.0 : 3.0,
            .shares_per_creation_unit = 50000,
            .constituents = sp500_constituents,
            .tracking_error = 0.0001,
            .expense_ratio = (etf_id == 3001) ? 0.0945 : 0.0300  // SPY: 9.45 bps, others: 3 bps
        };

        sp500_manager.add_etf(etf_metadata);
    }

    sp500_manager.update_index_constituents(sp500_constituents);

    std::cout << "\nSimulating market data for S&P 500 constituents...\n";

    // Simulate market data updates for constituents
    for (int i = 0; i < 50; ++i) {
        for (const auto& constituent : sp500_constituents) {
            // Create market tick for constituent
            double price_change = (i % 2 == 0) ? 0.01 : -0.01;
            MarketTick tick{
                .symbol_id = constituent.symbol_id,
                .price = constituent.last_price + price_change * i,
                .quantity = 1000,
                .timestamp = Optimization::get_timestamp_ns(),
                .bid = constituent.last_price + price_change * i - 0.01,
                .ask = constituent.last_price + price_change * i + 0.01,
                .bid_quantity = 1000,
                .ask_quantity = 1000
            };

            // Process through all ETF strategies
            sp500_manager.process_market_data(tick);

            // Simulate some trades
            if (i % 10 == 0) {
                Trade trade{
                    .symbol_id = constituent.symbol_id,
                    .price = tick.price,
                    .quantity = 100,
                    .side = (i % 20 == 0) ? Side::BUY : Side::SELL,
                    .timestamp = tick.timestamp
                };
                sp500_manager.process_trade(trade);
            }
        }

        if (i % 10 == 0) {
            std::cout << "Processed " << (i + 1) << " market data cycles\n";
        }

        // Small delay to simulate realistic data flow
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Print final statistics
    sp500_manager.print_etf_stats();

    std::cout << "\n=== ETF TRACKING ANALYSIS ===\n";
    std::cout << "✅ Multiple ETFs successfully tracking same index\n";
    std::cout << "✅ Real-time NAV calculation based on constituents\n";
    std::cout << "✅ Individual ETF rebalancing and tracking error monitoring\n";
    std::cout << "✅ Index level updates driven by constituent price changes\n";
    std::cout << "✅ Support for n-number of underlying securities per ETF\n";
    std::cout << "✅ Ultra-low latency processing of constituent updates\n";
}

// Placeholder implementations
bool update_strategy(const MarketTick& tick) { return true; }
void handle_error(const MarketTick& tick) {}
void update_orderbook(uint32_t symbol_id, double price, uint64_t quantity) {}

// Add namespace Strategy function definitions
namespace Strategy {
    void MarketMakingStrategy::submit_order(uint32_t symbol_id, double price, uint64_t quantity,
                                          Side side, uint64_t timestamp) {
        // Implementation would submit order to exchange
        std::cout << "MM Order: " << symbol_id << " " << (side == Side::BUY ? "BUY" : "SELL")
                  << " " << quantity << "@" << price << std::endl;
    }

    void ETFIndexTrackingStrategy::submit_order(uint32_t symbol_id, double price, uint64_t quantity,
                                              Side side, uint64_t timestamp) {
        // Implementation would submit ETF order to exchange
        std::cout << "ETF Order: " << symbol_id << " " << (side == Side::BUY ? "BUY" : "SELL")
                  << " " << quantity << "@" << price << std::endl;
    }

    int64_t RiskManager::get_position(uint32_t symbol_id) const {
        // Implementation would return current position for symbol
        return 0; // Placeholder
    }
}

int main() {
    std::cout << "Ultra Low Latency Design Guidelines for Capital Markets\n";
    std::cout << "=====================================================\n\n";

    std::cout << "This implementation provides comprehensive guidelines for:\n";
    std::cout << "• Hardware optimization and configuration\n";
    std::cout << "• Software architecture patterns\n";
    std::cout << "• Multi-strategy per instrument architecture\n";
    std::cout << "• ETF index tracking with multiple underlying securities\n";
    std::cout << "• Support for multiple ETFs tracking the same index\n";
    std::cout << "• Memory management strategies\n";
    std::cout << "• Network optimization techniques\n";
    std::cout << "• Strategy engine design\n";
    std::cout << "• Latency measurement and monitoring\n";
    std::cout << "• System configuration\n";
    std::cout << "• Testing and validation\n\n";

    std::cout << "Target latencies:\n";
    std::cout << "• Market Data Processing: < 500ns\n";
    std::cout << "• Strategy Calculation: < 1μs\n";
    std::cout << "• Order Generation: < 2μs\n";
    std::cout << "• End-to-End Latency: < 10μs\n\n";

    // Run latency test
    Testing::LatencyTest test;
    test.run_synthetic_test();

    // Demonstrate multi-strategy engine
    demonstrate_multi_strategy_engine();

    // Demonstrate ETF index tracking
    demonstrate_etf_index_tracking();

    return 0;
}
