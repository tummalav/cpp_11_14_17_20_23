# Market Making Backtesting Framework Documentation

## Overview

The Market Making Backtesting Framework is a comprehensive C++20 system designed specifically for testing and optimizing market making strategies. It provides realistic simulation of market microstructure, order book dynamics, and execution costs.

## 🎯 Key Features

### 1. **High-Fidelity Order Book Simulation**
- **Price-time priority matching engine**
- **Realistic slippage and commission modeling**
- **Market impact simulation**
- **Latency simulation for execution delays**

### 2. **Multiple Market Making Strategies**
- **Symmetric Market Maker**: Traditional spread-based approach
- **Adaptive Market Maker**: Volatility-adjusted spreads and sizing
- **Extensible architecture** for custom strategy development

### 3. **Comprehensive Risk Management**
- **Position limits and inventory controls**
- **Dynamic spread adjustment based on inventory**
- **Volatility-based order sizing**
- **Real-time P&L tracking**

### 4. **Advanced Performance Analytics**
- **Realized and unrealized P&L tracking**
- **Sharpe ratio calculation**
- **Maximum drawdown analysis**
- **Trade-by-trade performance metrics**
- **CSV export for external analysis**

### 5. **Realistic Market Data Simulation**
- **Geometric Brownian motion with momentum**
- **Microstructure effects modeling**
- **Configurable volatility and spread parameters**
- **Poisson-distributed tick arrival times**

## 📁 Files Structure

```
market_making_backtesting_framework.cpp  # Main framework implementation
mm_backtest_simple_test.cpp             # Simple test verification
MARKET_MAKING_BACKTEST_README.md        # This documentation
```

## 🚀 Quick Start

### Compilation
```bash
# Standard compilation
g++ -std=c++2a -pthread -Wall -Wextra -O2 market_making_backtesting_framework.cpp -o mm_backtest

# Optimized for production
g++ -std=c++2a -pthread -O3 -march=native -DNDEBUG -ffast-math -funroll-loops \
    market_making_backtesting_framework.cpp -o mm_backtest
```

### Basic Usage
```bash
# Run the framework
./mm_backtest

# Check for generated CSV files
ls -la *.csv
```

## 🏗️ Architecture Components

### 1. Core Data Structures

#### MarketTick
```cpp
struct MarketTick {
    Timestamp timestamp;
    Symbol symbol;
    Price bid_price, ask_price;
    Quantity bid_size, ask_size;
    Price last_price;
    Quantity last_size;
    uint64_t sequence_number;
    
    Price mid_price() const;
    Price spread() const;
    double spread_bps() const;
};
```

#### Order
```cpp
struct Order {
    OrderId id;
    Timestamp timestamp;
    Symbol symbol;
    OrderSide side;      // BUY, SELL
    OrderType type;      // MARKET, LIMIT, IOC, FOK
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    OrderStatus status;  // PENDING, FILLED, CANCELLED, etc.
    std::string strategy_id;
};
```

#### Position
```cpp
struct Position {
    Symbol symbol;
    int64_t quantity;           // Can be negative for short
    double average_price;
    double unrealized_pnl;
    double realized_pnl;
    Timestamp last_update;
    
    bool is_long() const;
    bool is_short() const;
    bool is_flat() const;
    double notional_value(Price current_price) const;
};
```

### 2. Order Book Implementation

The `OrderBook` class provides:
- **Price-level aggregation** with time priority
- **Market order execution** against resting liquidity
- **Limit order placement** and management
- **Best bid/ask calculation** with depth information
- **Spread and mid-price calculations**

```cpp
class OrderBook {
public:
    void update_from_tick(const MarketTick& tick);
    void add_order(const Order& order);
    std::vector<Trade> execute_market_order(const Order& order);
    Price get_best_bid() const;
    Price get_best_ask() const;
    Price get_mid_price() const;
    double get_spread() const;
};
```

### 3. Market Making Strategies

#### Base Strategy Interface
```cpp
class MarketMakingStrategy {
public:
    virtual std::vector<Order> generate_orders(
        const MarketTick& tick, 
        const Position& position,
        const OrderBook& book,
        Timestamp current_time) = 0;
    
    virtual void on_trade(const Trade& trade, Position& position) = 0;
    virtual void on_market_update(const MarketTick& tick) {}
};
```

#### Symmetric Market Maker
- **Fixed spread targeting** around mid-price
- **Inventory skewing** to manage position risk
- **Position limits** to control maximum exposure

**Key Parameters:**
- `target_spread_bps`: Target spread in basis points
- `min_spread_bps`: Minimum allowable spread
- `order_size`: Fixed order quantity
- `max_position_size`: Maximum allowed position

#### Adaptive Market Maker
- **Volatility-adjusted spreads** based on price history
- **Dynamic order sizing** inversely related to volatility
- **Enhanced inventory management** with volatility penalties

**Key Features:**
- Real-time volatility calculation using rolling window
- Adaptive spread: `adaptive_spread = target_spread × volatility_multiplier`
- Dynamic sizing: `adaptive_size = base_size × (1 / volatility_multiplier)`

### 4. Backtesting Engine

The `BacktestEngine` orchestrates the entire simulation:

```cpp
class BacktestEngine {
public:
    BacktestEngine(const BacktestConfig& config, 
                   std::unique_ptr<MarketMakingStrategy> strategy);
    
    void add_market_data(const MarketTick& tick);
    BacktestResults get_results() const;
    void export_results_to_csv(const std::string& filename) const;
};
```

#### Configuration Options
```cpp
struct BacktestConfig {
    Timestamp start_time, end_time;
    double initial_capital;
    double commission_rate;        // As decimal (e.g., 0.001 = 10 bps)
    double slippage_bps;          // Slippage in basis points
    int64_t latency_microseconds; // Execution latency
    bool enable_market_impact;
    double market_impact_factor;
};
```

### 5. Market Data Simulation

#### Basic Simulation
```cpp
MarketDataSimulator sim(150.0, 0.25, 4.0); // $150 base, 25% vol, 4 bps spread
auto ticks = sim.generate_ticks("AAPL", start_time, end_time, 1000);
```

#### Realistic Simulation with Microstructure
```cpp
auto ticks = sim.generate_realistic_ticks("AAPL", start_time, end_time, 500);
```

**Features:**
- **Geometric Brownian motion** with configurable volatility
- **Momentum effects** with periodic direction changes
- **Dynamic spreads** with market-dependent noise
- **Poisson-distributed** inter-arrival times
- **Realistic order sizes** and market depth

## 📊 Performance Metrics

### Core Metrics
- **Total P&L**: Realized + Unrealized profit/loss
- **Sharpe Ratio**: Risk-adjusted return measure
- **Maximum Drawdown**: Largest peak-to-trough decline
- **Return on Capital**: Total return as percentage of initial capital

### Trading Metrics
- **Total Trades**: Number of executed transactions
- **Average Spread Captured**: Mean spread captured per trade
- **Commission Costs**: Total transaction costs
- **Maximum Position**: Largest position held during backtest

### Risk Metrics
- **Position Tracking**: Real-time inventory monitoring
- **Volatility Analysis**: Rolling volatility calculations
- **Drawdown Duration**: Time spent in drawdown periods

## 🎮 Usage Examples

### Example 1: Basic Symmetric Market Maker
```cpp
// Configuration
BacktestConfig config;
config.initial_capital = 1000000.0;  // $1M
config.commission_rate = 0.0005;     // 5 bps
config.slippage_bps = 0.5;           // 0.5 bps

// Create strategy
auto strategy = std::make_unique<SymmetricMarketMaker>(
    "symmetric_mm", "AAPL", 
    1000000.0,  // inventory limit
    50000.0,    // max position
    5.0,        // target spread bps
    1.0,        // min spread bps
    1000.0      // order size
);

// Run backtest
BacktestEngine engine(config, std::move(strategy));
for (const auto& tick : market_data) {
    engine.add_market_data(tick);
}

auto results = engine.get_results();
results.print_summary();
```

### Example 2: Adaptive Market Maker with Volatility Adjustment
```cpp
auto adaptive_strategy = std::make_unique<AdaptiveMarketMaker>(
    "adaptive_mm", "AAPL",
    1000000.0,  // inventory limit
    50000.0,    // max position
    6.0,        // target spread bps
    1.5,        // min spread bps
    1200.0,     // base order size
    50          // volatility lookback periods
);
```

### Example 3: Custom Strategy Implementation
```cpp
class CustomMarketMaker : public MarketMakingStrategy {
public:
    std::vector<Order> generate_orders(const MarketTick& tick, 
                                     const Position& position,
                                     const OrderBook& book,
                                     Timestamp current_time) override {
        // Custom logic here
        std::vector<Order> orders;
        
        // Example: Time-of-day dependent spreads
        auto hour = get_hour_from_timestamp(current_time);
        double time_multiplier = (hour >= 9 && hour <= 16) ? 1.0 : 1.5;
        
        // Your custom strategy logic...
        
        return orders;
    }
    
    void on_trade(const Trade& trade, Position& position) override {
        // Custom position management
    }
};
```

## 📈 Advanced Features

### 1. Multi-Asset Support
The framework can be extended to support multiple instruments:
```cpp
std::unordered_map<Symbol, std::unique_ptr<MarketMakingStrategy>> strategies;
strategies["AAPL"] = std::make_unique<SymmetricMarketMaker>(...);
strategies["GOOGL"] = std::make_unique<AdaptiveMarketMaker>(...);
```

### 2. Real Market Data Integration
```cpp
// Load historical data
std::vector<MarketTick> load_historical_data(const std::string& filename) {
    // Implementation for CSV/binary data loading
}

// Use real data instead of simulation
auto real_ticks = load_historical_data("AAPL_20241031.csv");
```

### 3. Portfolio-Level Risk Management
```cpp
class PortfolioRiskManager {
public:
    bool check_portfolio_limits(const std::unordered_map<Symbol, Position>& positions);
    double calculate_portfolio_var(const std::unordered_map<Symbol, Position>& positions);
    void apply_hedging_orders(/* parameters */);
};
```

### 4. Machine Learning Integration
```cpp
class MLMarketMaker : public MarketMakingStrategy {
private:
    // Feature extraction
    std::vector<double> extract_features(const MarketTick& tick, const Position& position);
    
    // Model prediction
    double predict_optimal_spread(const std::vector<double>& features);
    
public:
    std::vector<Order> generate_orders(...) override {
        auto features = extract_features(tick, position);
        double optimal_spread = predict_optimal_spread(features);
        // Use ML prediction for order generation
    }
};
```

## 🔧 Configuration and Tuning

### Strategy Parameters
| Parameter | Symmetric MM | Adaptive MM | Description |
|-----------|-------------|-------------|-------------|
| `target_spread_bps` | 5.0 | 6.0 | Base spread targeting |
| `min_spread_bps` | 1.0 | 1.5 | Minimum spread floor |
| `order_size` | 1000 | 1200 | Base order quantity |
| `max_position_size` | 50000 | 50000 | Position limit |
| `inventory_limit` | 1M | 1M | Total inventory cap |

### Market Simulation Parameters
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `base_price` | 150.0 | 1-10000 | Starting price |
| `volatility` | 0.25 | 0.05-2.0 | Annual volatility |
| `spread_bps` | 4.0 | 0.5-50 | Market spread |
| `tick_interval` | 1000μs | 100-10000μs | Average tick frequency |

### Execution Parameters
| Parameter | Default | Description |
|-----------|---------|-------------|
| `commission_rate` | 0.0005 | Commission as decimal |
| `slippage_bps` | 0.5 | Execution slippage |
| `latency_microseconds` | 50 | Order execution delay |

## 📊 Results Analysis

### CSV Export Format
The framework exports results in CSV format for external analysis:

```csv
timestamp,equity,position
1000000,1000000.0,0
1001000,1000143.5,1000
1002000,1000287.2,2000
...
```

### Python Analysis Example
```python
import pandas as pd
import matplotlib.pyplot as plt

# Load results
df = pd.read_csv('symmetric_mm_results.csv')

# Plot equity curve
plt.figure(figsize=(12, 6))
plt.plot(df['timestamp'], df['equity'])
plt.title('Equity Curve')
plt.xlabel('Time')
plt.ylabel('Portfolio Value')
plt.show()

# Calculate metrics
returns = df['equity'].pct_change().dropna()
sharpe = returns.mean() / returns.std() * np.sqrt(252)
max_dd = (df['equity'] / df['equity'].cummax() - 1).min()

print(f"Sharpe Ratio: {sharpe:.3f}")
print(f"Max Drawdown: {max_dd:.3%}")
```

## 🚀 Performance Optimization

### Compilation Flags
```bash
# Maximum optimization for production backtesting
g++ -std=c++2a -pthread -O3 -march=native -DNDEBUG \
    -ffast-math -funroll-loops -flto \
    -fprofile-generate  # First pass for PGO
    
# After training run
g++ -std=c++2a -pthread -O3 -march=native -DNDEBUG \
    -ffast-math -funroll-loops -flto \
    -fprofile-use      # Second pass with profile data
```

### Memory Optimization
- **Object pooling** for frequent allocations
- **Custom allocators** for order and trade objects
- **Memory mapping** for large datasets
- **NUMA-aware** memory allocation

### Algorithmic Optimizations
- **Hash maps** for O(1) order lookups
- **Skip lists** for efficient order book operations
- **SIMD** for bulk calculations
- **Branch prediction** optimization

## 🔄 Extension Points

### Custom Strategy Development
1. **Inherit** from `MarketMakingStrategy`
2. **Implement** `generate_orders()` method
3. **Add** custom risk management in `on_trade()`
4. **Override** `on_market_update()` for market analysis

### Custom Order Types
```cpp
enum class CustomOrderType : uint8_t {
    ICEBERG,        // Hidden quantity orders
    STOP_LOSS,      // Stop-loss orders
    BRACKET,        // Bracket orders
    ALGO_TWAP       // Time-weighted average price
};
```

### Advanced Execution Models
- **Child order management** for large parent orders
- **Smart order routing** across multiple venues
- **Latency arbitrage** modeling
- **Dark pool** interaction simulation

## 📋 Testing and Validation

### Unit Testing Framework
```cpp
class BacktestFrameworkTests {
public:
    void test_order_book_matching();
    void test_position_management();
    void test_pnl_calculations();
    void test_strategy_logic();
    void test_risk_limits();
};
```

### Backtesting Best Practices
1. **Out-of-sample testing** with reserved data
2. **Walk-forward analysis** for parameter stability
3. **Monte Carlo simulation** for robustness testing
4. **Sensitivity analysis** for parameter ranges
5. **Transaction cost analysis** with realistic assumptions

### Validation Metrics
- **Backtest vs. live performance** correlation
- **Strategy capacity** analysis
- **Market regime** robustness
- **Parameter sensitivity** testing

## 🎯 Future Enhancements

### 1. Advanced Order Types
- **Iceberg orders** with hidden quantity
- **Stop-loss** and **take-profit** orders
- **Time-in-force** variations (GTD, GTC, etc.)

### 2. Multi-Venue Simulation
- **Cross-venue arbitrage** opportunities
- **Smart order routing** algorithms
- **Venue-specific** latency and fees

### 3. Options Market Making
- **Greeks calculations** and hedging
- **Volatility surface** modeling
- **Risk-neutral** pricing models

### 4. Machine Learning Integration
- **Reinforcement learning** for adaptive strategies
- **Feature engineering** from market microstructure
- **Online learning** for real-time adaptation

### 5. Real-Time Integration
- **Live market data** feeds (FIX, binary protocols)
- **Real-time risk monitoring**
- **Alert systems** for limit breaches

## 🏁 Conclusion

The Market Making Backtesting Framework provides a comprehensive platform for:

✅ **Strategy Development**: Rapid prototyping and testing of market making algorithms  
✅ **Risk Management**: Comprehensive position and portfolio risk controls  
✅ **Performance Analysis**: Detailed analytics and reporting capabilities  
✅ **Realistic Simulation**: High-fidelity market microstructure modeling  
✅ **Production Ready**: Optimized for speed and scalability  

The framework is designed for:
- **Quantitative Researchers** developing market making strategies
- **Risk Managers** validating strategy risk profiles
- **Portfolio Managers** optimizing capital allocation
- **Traders** understanding market microstructure effects

With its modular architecture and extensive customization options, the framework serves as both an educational tool and a production-grade backtesting system for sophisticated market making operations.

---
*For questions, issues, or contributions, please refer to the source code documentation and inline comments.*
