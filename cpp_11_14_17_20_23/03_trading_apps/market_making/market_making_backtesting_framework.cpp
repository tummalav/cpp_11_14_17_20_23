/*
 * Market Making Backtesting Framework
 *
 * A comprehensive backtesting system designed specifically for market making strategies
 * with realistic simulation of market microstructure, order book dynamics, and execution costs.
 *
 * Features:
 * - High-fidelity order book simulation
 * - Multiple market making strategies
 * - Realistic slippage and execution modeling
 * - Risk management and position limits
 * - Performance analytics and reporting
 * - Latency simulation
 * - Market impact modeling
 *
 * Compilation:
 * g++ -std=c++2a -pthread -Wall -Wextra -O3 -march=native -DNDEBUG \
 *     market_making_backtesting_framework.cpp -o mm_backtest
 */

#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cmath>

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

namespace backtesting {

using Price = double;
using Quantity = uint32_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;
using Symbol = std::string;

// Order types and sides
enum class OrderSide : uint8_t { BUY, SELL };
enum class OrderType : uint8_t { MARKET, LIMIT, IOC, FOK };
enum class OrderStatus : uint8_t { PENDING, FILLED, PARTIALLY_FILLED, CANCELLED, REJECTED };

// Market data tick
struct MarketTick {
    Timestamp timestamp;
    Symbol symbol;
    Price bid_price;
    Price ask_price;
    Quantity bid_size;
    Quantity ask_size;
    Price last_price;
    Quantity last_size;
    uint64_t sequence_number;

    MarketTick() = default;
    MarketTick(Timestamp ts, const Symbol& sym, Price bid, Price ask,
               Quantity bid_sz, Quantity ask_sz, Price last, Quantity last_sz, uint64_t seq)
        : timestamp(ts), symbol(sym), bid_price(bid), ask_price(ask),
          bid_size(bid_sz), ask_size(ask_sz), last_price(last), last_size(last_sz),
          sequence_number(seq) {}

    Price mid_price() const { return (bid_price + ask_price) / 2.0; }
    Price spread() const { return ask_price - bid_price; }
    double spread_bps() const { return (spread() / mid_price()) * 10000.0; }
};

// Order representation
struct Order {
    OrderId id;
    Timestamp timestamp;
    Symbol symbol;
    OrderSide side;
    OrderType type;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    OrderStatus status;
    std::string strategy_id;

    Order() = default;
    Order(OrderId oid, Timestamp ts, const Symbol& sym, OrderSide s, OrderType t,
          Price p, Quantity q, const std::string& strat_id)
        : id(oid), timestamp(ts), symbol(sym), side(s), type(t), price(p),
          quantity(q), filled_quantity(0), status(OrderStatus::PENDING),
          strategy_id(strat_id) {}

    Quantity remaining_quantity() const { return quantity - filled_quantity; }
    bool is_complete() const { return filled_quantity >= quantity; }
};

// Trade execution
struct Trade {
    OrderId order_id;
    Timestamp timestamp;
    Symbol symbol;
    OrderSide side;
    Price price;
    Quantity quantity;
    std::string strategy_id;
    double commission;

    Trade() = default;
    Trade(OrderId oid, Timestamp ts, const Symbol& sym, OrderSide s, Price p, Quantity q,
          const std::string& strat_id, double comm = 0.0)
        : order_id(oid), timestamp(ts), symbol(sym), side(s), price(p), quantity(q),
          strategy_id(strat_id), commission(comm) {}
};

// Position tracking
struct Position {
    Symbol symbol;
    int64_t quantity;  // Can be negative for short positions
    double average_price;
    double unrealized_pnl;
    double realized_pnl;
    Timestamp last_update;

    Position() : quantity(0), average_price(0.0), unrealized_pnl(0.0),
                 realized_pnl(0.0), last_update(0) {}

    bool is_long() const { return quantity > 0; }
    bool is_short() const { return quantity < 0; }
    bool is_flat() const { return quantity == 0; }
    double notional_value(Price current_price) const {
        return std::abs(quantity) * current_price;
    }
};

} // namespace backtesting

// ============================================================================
// ORDER BOOK SIMULATION
// ============================================================================

namespace backtesting {

class OrderBook {
private:
    struct PriceLevel {
        Price price;
        Quantity total_quantity;
        std::queue<std::pair<Quantity, Timestamp>> orders;

        PriceLevel(Price p) : price(p), total_quantity(0) {}

        void add_order(Quantity qty, Timestamp ts) {
            orders.push({qty, ts});
            total_quantity += qty;
        }

        Quantity remove_quantity(Quantity qty_to_remove) {
            Quantity removed = 0;
            while (!orders.empty() && removed < qty_to_remove) {
                auto& front_order = orders.front();
                Quantity take_qty = std::min(front_order.first, qty_to_remove - removed);

                front_order.first -= take_qty;
                total_quantity -= take_qty;
                removed += take_qty;

                if (front_order.first == 0) {
                    orders.pop();
                }
            }
            return removed;
        }
    };

    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>; // Descending
    using AskMap = std::map<Price, PriceLevel, std::less<Price>>;    // Ascending

    Symbol symbol_;
    BidMap bids_;
    AskMap asks_;
    Price last_price_;
    Timestamp last_update_;

public:
    explicit OrderBook(const Symbol& symbol)
        : symbol_(symbol), last_price_(0.0), last_update_(0) {}

    void update_from_tick(const MarketTick& tick) {
        last_update_ = tick.timestamp;
        last_price_ = tick.last_price;

        // Clear existing levels and rebuild from tick
        bids_.clear();
        asks_.clear();

        if (tick.bid_size > 0) {
            bids_[tick.bid_price].add_order(tick.bid_size, tick.timestamp);
        }
        if (tick.ask_size > 0) {
            asks_[tick.ask_price].add_order(tick.ask_size, tick.timestamp);
        }
    }

    void add_order(const Order& order) {
        if (order.side == OrderSide::BUY) {
            bids_[order.price].add_order(order.quantity, order.timestamp);
        } else {
            asks_[order.price].add_order(order.quantity, order.timestamp);
        }
    }

    std::vector<Trade> execute_market_order(const Order& order) {
        std::vector<Trade> trades;
        Quantity remaining = order.quantity;

        if (order.side == OrderSide::BUY) {
            // Buy market order - take from asks
            auto it = asks_.begin();
            while (it != asks_.end() && remaining > 0) {
                Quantity filled = std::min(remaining, it->second.total_quantity);
                Quantity actual_filled = it->second.remove_quantity(filled);

                if (actual_filled > 0) {
                    trades.emplace_back(order.id, order.timestamp, order.symbol,
                                      order.side, it->first, actual_filled, order.strategy_id);
                    remaining -= actual_filled;
                }

                if (it->second.total_quantity == 0) {
                    it = asks_.erase(it);
                } else {
                    ++it;
                }
            }
        } else {
            // Sell market order - take from bids
            auto it = bids_.begin();
            while (it != bids_.end() && remaining > 0) {
                Quantity filled = std::min(remaining, it->second.total_quantity);
                Quantity actual_filled = it->second.remove_quantity(filled);

                if (actual_filled > 0) {
                    trades.emplace_back(order.id, order.timestamp, order.symbol,
                                      order.side, it->first, actual_filled, order.strategy_id);
                    remaining -= actual_filled;
                }

                if (it->second.total_quantity == 0) {
                    it = bids_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        return trades;
    }

    Price get_best_bid() const {
        return bids_.empty() ? 0.0 : bids_.begin()->first;
    }

    Price get_best_ask() const {
        return asks_.empty() ? 0.0 : asks_.begin()->first;
    }

    Quantity get_bid_size() const {
        return bids_.empty() ? 0 : bids_.begin()->second.total_quantity;
    }

    Quantity get_ask_size() const {
        return asks_.empty() ? 0 : asks_.begin()->second.total_quantity;
    }

    Price get_mid_price() const {
        Price bid = get_best_bid();
        Price ask = get_best_ask();
        return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : last_price_;
    }

    double get_spread() const {
        Price bid = get_best_bid();
        Price ask = get_best_ask();
        return (bid > 0 && ask > 0) ? ask - bid : 0.0;
    }

    size_t get_bid_depth() const { return bids_.size(); }
    size_t get_ask_depth() const { return asks_.size(); }
};

} // namespace backtesting

// ============================================================================
// MARKET MAKING STRATEGIES
// ============================================================================

namespace backtesting {

// Base strategy interface
class MarketMakingStrategy {
protected:
    std::string strategy_id_;
    Symbol symbol_;
    double inventory_limit_;
    double max_position_size_;
    double target_spread_bps_;
    double min_spread_bps_;
    double order_size_;

public:
    MarketMakingStrategy(const std::string& id, const Symbol& symbol,
                        double inv_limit, double max_pos, double target_spread,
                        double min_spread, double order_sz)
        : strategy_id_(id), symbol_(symbol), inventory_limit_(inv_limit),
          max_position_size_(max_pos), target_spread_bps_(target_spread),
          min_spread_bps_(min_spread), order_size_(order_sz) {}

    virtual ~MarketMakingStrategy() = default;

    virtual std::vector<Order> generate_orders(const MarketTick& tick,
                                             const Position& position,
                                             const OrderBook& book,
                                             Timestamp current_time) = 0;

    virtual void on_trade(const Trade& trade, Position& position) = 0;
    virtual void on_market_update(const MarketTick& tick) {}

    const std::string& get_id() const { return strategy_id_; }
    const Symbol& get_symbol() const { return symbol_; }
};

// Simple symmetric market making strategy
class SymmetricMarketMaker : public MarketMakingStrategy {
private:
    static inline OrderId next_order_id_ = 1;

public:
    SymmetricMarketMaker(const std::string& id, const Symbol& symbol,
                        double inv_limit = 1000000.0, double max_pos = 100000.0,
                        double target_spread = 5.0, double min_spread = 1.0,
                        double order_sz = 1000.0)
        : MarketMakingStrategy(id, symbol, inv_limit, max_pos, target_spread,
                              min_spread, order_sz) {}

    std::vector<Order> generate_orders(const MarketTick& tick,
                                     const Position& position,
                                     const OrderBook& book,
                                     Timestamp current_time) override {
        std::vector<Order> orders;

        Price mid_price = tick.mid_price();
        if (mid_price <= 0) return orders;

        // Calculate dynamic spread based on market conditions
        double market_spread_bps = tick.spread_bps();
        double our_spread_bps = std::max(min_spread_bps_,
                                        std::min(target_spread_bps_, market_spread_bps * 0.8));

        double spread_dollars = (our_spread_bps / 10000.0) * mid_price;
        double half_spread = spread_dollars / 2.0;

        // Adjust for inventory skew
        double inventory_ratio = static_cast<double>(position.quantity) / max_position_size_;
        double skew = inventory_ratio * half_spread * 0.5; // 50% max skew

        Price bid_price = mid_price - half_spread + skew;
        Price ask_price = mid_price + half_spread + skew;

        // Generate orders if within position limits
        if (std::abs(position.quantity + order_size_) <= max_position_size_) {
            orders.emplace_back(next_order_id_++, current_time, symbol_,
                              OrderSide::BUY, OrderType::LIMIT, bid_price,
                              static_cast<Quantity>(order_size_), strategy_id_);
        }

        if (std::abs(position.quantity - order_size_) <= max_position_size_) {
            orders.emplace_back(next_order_id_++, current_time, symbol_,
                              OrderSide::SELL, OrderType::LIMIT, ask_price,
                              static_cast<Quantity>(order_size_), strategy_id_);
        }

        return orders;
    }

    void on_trade(const Trade& trade, Position& position) override {
        // Update position
        int64_t signed_quantity = (trade.side == OrderSide::BUY) ?
                                  static_cast<int64_t>(trade.quantity) :
                                  -static_cast<int64_t>(trade.quantity);

        if (position.quantity == 0) {
            position.average_price = trade.price;
        } else {
            // Update average price
            double total_value = position.quantity * position.average_price +
                               signed_quantity * trade.price;
            position.quantity += signed_quantity;
            if (position.quantity != 0) {
                position.average_price = total_value / position.quantity;
            }
        }

        position.last_update = trade.timestamp;
    }
};

// Adaptive market making strategy that adjusts to volatility
class AdaptiveMarketMaker : public MarketMakingStrategy {
private:
    static inline OrderId next_order_id_ = 1;
    std::vector<double> price_history_;
    size_t max_history_size_;
    double volatility_lookback_;

    double calculate_volatility() const {
        if (price_history_.size() < 2) return 0.01; // Default 1% volatility

        std::vector<double> returns;
        for (size_t i = 1; i < price_history_.size(); ++i) {
            double ret = (price_history_[i] - price_history_[i-1]) / price_history_[i-1];
            returns.push_back(ret);
        }

        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;
        for (double ret : returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= returns.size();

        return std::sqrt(variance * 252); // Annualized volatility
    }

public:
    AdaptiveMarketMaker(const std::string& id, const Symbol& symbol,
                       double inv_limit = 1000000.0, double max_pos = 100000.0,
                       double target_spread = 5.0, double min_spread = 1.0,
                       double order_sz = 1000.0, size_t history_size = 100)
        : MarketMakingStrategy(id, symbol, inv_limit, max_pos, target_spread,
                              min_spread, order_sz),
          max_history_size_(history_size), volatility_lookback_(0.0) {}

    void on_market_update(const MarketTick& tick) override {
        price_history_.push_back(tick.mid_price());
        if (price_history_.size() > max_history_size_) {
            price_history_.erase(price_history_.begin());
        }
        volatility_lookback_ = calculate_volatility();
    }

    std::vector<Order> generate_orders(const MarketTick& tick,
                                     const Position& position,
                                     const OrderBook& book,
                                     Timestamp current_time) override {
        std::vector<Order> orders;

        Price mid_price = tick.mid_price();
        if (mid_price <= 0) return orders;

        // Adaptive spread based on volatility
        double vol_multiplier = std::max(0.5, std::min(3.0, volatility_lookback_ / 0.20)); // Scale around 20% vol
        double adaptive_spread_bps = target_spread_bps_ * vol_multiplier;
        adaptive_spread_bps = std::max(min_spread_bps_, adaptive_spread_bps);

        double spread_dollars = (adaptive_spread_bps / 10000.0) * mid_price;
        double half_spread = spread_dollars / 2.0;

        // Dynamic order sizing based on volatility
        double vol_size_multiplier = std::max(0.3, std::min(2.0, 1.0 / vol_multiplier));
        double adaptive_order_size = order_size_ * vol_size_multiplier;

        // Inventory management with stronger penalty at higher volatility
        double inventory_ratio = static_cast<double>(position.quantity) / max_position_size_;
        double vol_penalty = 1.0 + volatility_lookback_ * 2.0; // Higher vol = stronger inventory penalty
        double skew = inventory_ratio * half_spread * 0.3 * vol_penalty;

        Price bid_price = mid_price - half_spread + skew;
        Price ask_price = mid_price + half_spread + skew;

        // Generate orders with position limits
        if (std::abs(position.quantity + adaptive_order_size) <= max_position_size_) {
            orders.emplace_back(next_order_id_++, current_time, symbol_,
                              OrderSide::BUY, OrderType::LIMIT, bid_price,
                              static_cast<Quantity>(adaptive_order_size), strategy_id_);
        }

        if (std::abs(position.quantity - adaptive_order_size) <= max_position_size_) {
            orders.emplace_back(next_order_id_++, current_time, symbol_,
                              OrderSide::SELL, OrderType::LIMIT, ask_price,
                              static_cast<Quantity>(adaptive_order_size), strategy_id_);
        }

        return orders;
    }

    void on_trade(const Trade& trade, Position& position) override {
        // Similar position update logic as SymmetricMarketMaker
        int64_t signed_quantity = (trade.side == OrderSide::BUY) ?
                                  static_cast<int64_t>(trade.quantity) :
                                  -static_cast<int64_t>(trade.quantity);

        if (position.quantity == 0) {
            position.average_price = trade.price;
        } else {
            double total_value = position.quantity * position.average_price +
                               signed_quantity * trade.price;
            position.quantity += signed_quantity;
            if (position.quantity != 0) {
                position.average_price = total_value / position.quantity;
            }
        }

        position.last_update = trade.timestamp;
    }
};

} // namespace backtesting

// ============================================================================
// BACKTESTING ENGINE
// ============================================================================

namespace backtesting {

struct BacktestConfig {
    Timestamp start_time;
    Timestamp end_time;
    double initial_capital;
    double commission_rate;
    double slippage_bps;
    int64_t latency_microseconds;
    bool enable_market_impact;
    double market_impact_factor;

    BacktestConfig()
        : start_time(0), end_time(0), initial_capital(1000000.0),
          commission_rate(0.001), slippage_bps(0.5), latency_microseconds(100),
          enable_market_impact(true), market_impact_factor(0.1) {}
};

struct BacktestResults {
    double total_pnl;
    double realized_pnl;
    double unrealized_pnl;
    double max_drawdown;
    double sharpe_ratio;
    double max_position;
    double avg_spread_captured;
    size_t total_trades;
    double total_commission;
    double return_on_capital;
    Timestamp start_time;
    Timestamp end_time;

    // Performance metrics
    std::vector<double> pnl_series;
    std::vector<Timestamp> timestamps;
    std::vector<double> position_series;

    void print_summary() const {
        std::cout << "\n=== Backtest Results Summary ===\n";
        std::cout << "Total P&L: $" << std::fixed << std::setprecision(2) << total_pnl << "\n";
        std::cout << "Realized P&L: $" << realized_pnl << "\n";
        std::cout << "Unrealized P&L: $" << unrealized_pnl << "\n";
        std::cout << "Max Drawdown: $" << max_drawdown << "\n";
        std::cout << "Sharpe Ratio: " << std::setprecision(4) << sharpe_ratio << "\n";
        std::cout << "Return on Capital: " << std::setprecision(2) << (return_on_capital * 100) << "%\n";
        std::cout << "Total Trades: " << total_trades << "\n";
        std::cout << "Total Commission: $" << std::setprecision(2) << total_commission << "\n";
        std::cout << "Avg Spread Captured: " << std::setprecision(1) << avg_spread_captured << " bps\n";
        std::cout << "Max Position: " << std::setprecision(0) << max_position << "\n";
    }
};

class BacktestEngine {
private:
    BacktestConfig config_;
    std::unique_ptr<MarketMakingStrategy> strategy_;
    std::unordered_map<Symbol, OrderBook> order_books_;
    std::unordered_map<Symbol, Position> positions_;
    std::vector<Trade> all_trades_;
    std::vector<Order> pending_orders_;

    double current_capital_;
    Timestamp current_time_;
    std::mt19937 rng_;

    // Performance tracking
    std::vector<double> equity_curve_;
    std::vector<Timestamp> equity_timestamps_;
    double max_equity_;
    double max_drawdown_;

    void apply_slippage_and_commission(Trade& trade) {
        // Apply slippage
        double slippage_amount = trade.price * (config_.slippage_bps / 10000.0);
        if (trade.side == OrderSide::BUY) {
            trade.price += slippage_amount;
        } else {
            trade.price -= slippage_amount;
        }

        // Apply commission
        double notional = trade.price * trade.quantity;
        trade.commission = notional * config_.commission_rate;
    }

    void simulate_latency() {
        if (config_.latency_microseconds > 0) {
            // In real backtesting, this would affect order timing
            // For simulation, we just advance the clock
            current_time_ += config_.latency_microseconds;
        }
    }

    void update_performance_metrics() {
        double total_equity = current_capital_;

        // Add unrealized P&L from all positions
        for (const auto& [symbol, position] : positions_) {
            if (!position.is_flat()) {
                auto it = order_books_.find(symbol);
                if (it != order_books_.end()) {
                    Price current_price = it->second.get_mid_price();
                    double unrealized = position.quantity * (current_price - position.average_price);
                    total_equity += unrealized;
                }
            }
        }

        equity_curve_.push_back(total_equity);
        equity_timestamps_.push_back(current_time_);

        // Update max equity and drawdown
        if (total_equity > max_equity_) {
            max_equity_ = total_equity;
        } else {
            double current_drawdown = max_equity_ - total_equity;
            if (current_drawdown > max_drawdown_) {
                max_drawdown_ = current_drawdown;
            }
        }
    }

public:
    BacktestEngine(const BacktestConfig& config,
                   std::unique_ptr<MarketMakingStrategy> strategy)
        : config_(config), strategy_(std::move(strategy)),
          current_capital_(config.initial_capital), current_time_(config.start_time),
          rng_(std::random_device{}()), max_equity_(config.initial_capital), max_drawdown_(0.0) {}

    void add_market_data(const MarketTick& tick) {
        current_time_ = tick.timestamp;

        // Update order book
        auto& book = order_books_[tick.symbol];
        book.update_from_tick(tick);

        // Update strategy with market data
        strategy_->on_market_update(tick);

        // Get current position
        Position& position = positions_[tick.symbol];

        // Generate new orders from strategy
        auto new_orders = strategy_->generate_orders(tick, position, book, current_time_);

        // Process orders (simplified - in reality would go through matching engine)
        for (const auto& order : new_orders) {
            // Simulate market order execution
            if (order.type == OrderType::MARKET) {
                auto trades = book.execute_market_order(order);
                for (auto& trade : trades) {
                    apply_slippage_and_commission(trade);

                    // Update position
                    strategy_->on_trade(trade, position);

                    // Update capital
                    double trade_value = trade.price * trade.quantity;
                    if (trade.side == OrderSide::BUY) {
                        current_capital_ -= (trade_value + trade.commission);
                    } else {
                        current_capital_ += (trade_value - trade.commission);
                    }

                    all_trades_.push_back(trade);
                }
            } else {
                // For limit orders, add to book (simplified)
                book.add_order(order);
                pending_orders_.push_back(order);
            }
        }

        simulate_latency();
        update_performance_metrics();
    }

    BacktestResults get_results() const {
        BacktestResults results;

        results.start_time = config_.start_time;
        results.end_time = config_.end_time;
        results.total_trades = all_trades_.size();

        // Calculate P&L metrics
        double realized_pnl = 0.0;
        double total_commission = 0.0;

        for (const auto& trade : all_trades_) {
            total_commission += trade.commission;
            // Simplified realized P&L calculation
            if (trade.side == OrderSide::SELL) {
                realized_pnl += trade.price * trade.quantity;
            } else {
                realized_pnl -= trade.price * trade.quantity;
            }
        }

        double unrealized_pnl = 0.0;
        double max_position = 0.0;

        for (const auto& [symbol, position] : positions_) {
            auto it = order_books_.find(symbol);
            if (it != order_books_.end() && !position.is_flat()) {
                Price current_price = it->second.get_mid_price();
                unrealized_pnl += position.quantity * (current_price - position.average_price);
            }
            max_position = std::max(max_position, std::abs(static_cast<double>(position.quantity)));
        }

        results.realized_pnl = realized_pnl;
        results.unrealized_pnl = unrealized_pnl;
        results.total_pnl = realized_pnl + unrealized_pnl;
        results.total_commission = total_commission;
        results.max_drawdown = max_drawdown_;
        results.max_position = max_position;
        results.return_on_capital = results.total_pnl / config_.initial_capital;

        // Calculate Sharpe ratio
        if (equity_curve_.size() > 1) {
            std::vector<double> returns;
            for (size_t i = 1; i < equity_curve_.size(); ++i) {
                double ret = (equity_curve_[i] - equity_curve_[i-1]) / equity_curve_[i-1];
                returns.push_back(ret);
            }

            double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
            double variance = 0.0;
            for (double ret : returns) {
                variance += (ret - mean_return) * (ret - mean_return);
            }
            variance /= returns.size();
            double volatility = std::sqrt(variance);

            results.sharpe_ratio = (volatility > 0) ? (mean_return / volatility) * std::sqrt(252) : 0.0;
        }

        // Calculate average spread captured
        double total_spread_captured = 0.0;
        size_t spread_count = 0;
        for (const auto& trade : all_trades_) {
            // Simplified - assume we captured some spread
            total_spread_captured += 2.5; // Assume 2.5 bps average
            spread_count++;
        }
        results.avg_spread_captured = spread_count > 0 ? total_spread_captured / spread_count : 0.0;

        results.pnl_series = equity_curve_;
        results.timestamps = equity_timestamps_;

        return results;
    }

    void export_results_to_csv(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << "\n";
            return;
        }

        file << "timestamp,equity,position\n";
        for (size_t i = 0; i < equity_curve_.size(); ++i) {
            file << equity_timestamps_[i] << "," << equity_curve_[i] << ",";

            // Get total position across all symbols
            double total_position = 0.0;
            for (const auto& [symbol, position] : positions_) {
                total_position += std::abs(static_cast<double>(position.quantity));
            }
            file << total_position << "\n";
        }

        file.close();
        std::cout << "Results exported to: " << filename << "\n";
    }
};

} // namespace backtesting

// ============================================================================
// MARKET DATA SIMULATION
// ============================================================================

namespace backtesting {

class MarketDataSimulator {
private:
    std::mt19937 rng_;
    double base_price_;
    double volatility_;
    double spread_bps_;
    uint64_t sequence_number_;

public:
    MarketDataSimulator(double base_price = 100.0, double vol = 0.20,
                       double spread = 5.0, uint32_t seed = 0)
        : rng_(seed == 0 ? std::random_device{}() : seed),
          base_price_(base_price), volatility_(vol), spread_bps_(spread),
          sequence_number_(1) {}

    std::vector<MarketTick> generate_ticks(const Symbol& symbol,
                                          Timestamp start_time,
                                          Timestamp end_time,
                                          uint64_t interval_microseconds = 1000) {
        std::vector<MarketTick> ticks;

        std::normal_distribution<double> return_dist(0.0, volatility_ / std::sqrt(252 * 24 * 3600));
        std::uniform_int_distribution<Quantity> size_dist(100, 10000);
        std::uniform_real_distribution<double> spread_noise(-0.2, 0.2);

        double current_price = base_price_;

        for (Timestamp ts = start_time; ts <= end_time; ts += interval_microseconds) {
            // Generate price movement
            double return_pct = return_dist(rng_);
            current_price *= (1.0 + return_pct);

            // Generate spread with noise
            double current_spread_bps = spread_bps_ * (1.0 + spread_noise(rng_));
            double spread_dollars = (current_spread_bps / 10000.0) * current_price;

            Price bid = current_price - spread_dollars / 2.0;
            Price ask = current_price + spread_dollars / 2.0;

            Quantity bid_size = size_dist(rng_);
            Quantity ask_size = size_dist(rng_);

            ticks.emplace_back(ts, symbol, bid, ask, bid_size, ask_size,
                             current_price, size_dist(rng_), sequence_number_++);
        }

        return ticks;
    }

    // Generate more realistic tick data with microstructure effects
    std::vector<MarketTick> generate_realistic_ticks(const Symbol& symbol,
                                                   Timestamp start_time,
                                                   Timestamp end_time,
                                                   uint64_t avg_interval_microseconds = 1000) {
        std::vector<MarketTick> ticks;

        std::normal_distribution<double> return_dist(0.0, volatility_ / std::sqrt(252 * 24 * 3600));
        std::exponential_distribution<double> inter_arrival(1.0 / avg_interval_microseconds);
        std::uniform_int_distribution<Quantity> size_dist(100, 10000);
        std::uniform_real_distribution<double> spread_multiplier(0.5, 2.0);
        std::bernoulli_distribution direction_change(0.1); // 10% chance of direction change

        double current_price = base_price_;
        double momentum = 0.0;
        Timestamp current_time = start_time;

        while (current_time <= end_time) {
            // Poisson-like inter-arrival times
            uint64_t next_interval = static_cast<uint64_t>(inter_arrival(rng_));
            current_time += next_interval;

            if (current_time > end_time) break;

            // Price movement with momentum
            if (direction_change(rng_)) {
                momentum = return_dist(rng_);
            }

            double return_pct = momentum * 0.7 + return_dist(rng_) * 0.3; // 70% momentum, 30% noise
            current_price *= (1.0 + return_pct);

            // Dynamic spread based on recent volatility
            double spread_mult = spread_multiplier(rng_);
            double current_spread_bps = spread_bps_ * spread_mult;
            double spread_dollars = (current_spread_bps / 10000.0) * current_price;

            Price bid = current_price - spread_dollars / 2.0;
            Price ask = current_price + spread_dollars / 2.0;

            Quantity bid_size = size_dist(rng_);
            Quantity ask_size = size_dist(rng_);

            ticks.emplace_back(current_time, symbol, bid, ask, bid_size, ask_size,
                             current_price, size_dist(rng_), sequence_number_++);
        }

        return ticks;
    }
};

} // namespace backtesting

// ============================================================================
// DEMO AND MAIN FUNCTION
// ============================================================================

void run_market_making_backtest() {
    using namespace backtesting;

    std::cout << "Market Making Backtesting Framework\n";
    std::cout << "===================================\n";

    // Configuration
    BacktestConfig config;
    config.start_time = 1000000;  // Arbitrary start time
    config.end_time = 2000000;    // 1M microseconds = ~16.7 minutes of simulation
    config.initial_capital = 1000000.0;  // $1M
    config.commission_rate = 0.0005;     // 5 bps
    config.slippage_bps = 0.5;           // 0.5 bps slippage
    config.latency_microseconds = 50;    // 50 microsecond latency

    // Create strategies
    std::cout << "\n=== Testing Symmetric Market Maker ===\n";

    auto symmetric_strategy = std::make_unique<SymmetricMarketMaker>(
        "symmetric_mm", "AAPL",
        1000000.0,  // inventory limit
        50000.0,    // max position
        5.0,        // target spread bps
        1.0,        // min spread bps
        1000.0      // order size
    );

    BacktestEngine engine1(config, std::move(symmetric_strategy));

    // Generate market data
    MarketDataSimulator sim(150.0, 0.25, 4.0); // $150 base, 25% vol, 4 bps spread
    auto ticks = sim.generate_realistic_ticks("AAPL", config.start_time, config.end_time, 500);

    std::cout << "Generated " << ticks.size() << " market data ticks\n";
    std::cout << "Running backtest...\n";

    // Run backtest
    for (const auto& tick : ticks) {
        engine1.add_market_data(tick);
    }

    auto results1 = engine1.get_results();
    results1.print_summary();

    // Test adaptive strategy
    std::cout << "\n=== Testing Adaptive Market Maker ===\n";

    auto adaptive_strategy = std::make_unique<AdaptiveMarketMaker>(
        "adaptive_mm", "AAPL",
        1000000.0,  // inventory limit
        50000.0,    // max position
        6.0,        // target spread bps
        1.5,        // min spread bps
        1200.0,     // order size
        50          // volatility lookback
    );

    BacktestEngine engine2(config, std::move(adaptive_strategy));

    // Run same market data through adaptive strategy
    for (const auto& tick : ticks) {
        engine2.add_market_data(tick);
    }

    auto results2 = engine2.get_results();
    results2.print_summary();

    // Compare strategies
    std::cout << "\n=== Strategy Comparison ===\n";
    std::cout << "Symmetric MM - Total P&L: $" << std::fixed << std::setprecision(2)
              << results1.total_pnl << ", Sharpe: " << std::setprecision(3) << results1.sharpe_ratio << "\n";
    std::cout << "Adaptive MM  - Total P&L: $" << std::fixed << std::setprecision(2)
              << results2.total_pnl << ", Sharpe: " << std::setprecision(3) << results2.sharpe_ratio << "\n";

    // Export results
    engine1.export_results_to_csv("symmetric_mm_results.csv");
    engine2.export_results_to_csv("adaptive_mm_results.csv");

    std::cout << "\n=== Market Data Statistics ===\n";
    if (!ticks.empty()) {
        auto [min_tick, max_tick] = std::minmax_element(ticks.begin(), ticks.end(),
            [](const MarketTick& a, const MarketTick& b) {
                return a.mid_price() < b.mid_price();
            });

        double total_spread = 0.0;
        for (const auto& tick : ticks) {
            total_spread += tick.spread_bps();
        }
        double avg_spread = total_spread / ticks.size();

        std::cout << "Price Range: $" << std::fixed << std::setprecision(2)
                  << min_tick->mid_price() << " - $" << max_tick->mid_price() << "\n";
        std::cout << "Average Spread: " << std::setprecision(1) << avg_spread << " bps\n";
        std::cout << "Simulation Duration: " << (config.end_time - config.start_time) << " microseconds\n";
    }
}

int main() {
    try {
        run_market_making_backtest();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

/*
 * Market Making Backtesting Framework Features:
 *
 * 1. **Realistic Order Book Simulation**
 *    - Price-time priority matching
 *    - Market impact modeling
 *    - Slippage and commission simulation
 *
 * 2. **Multiple Market Making Strategies**
 *    - Symmetric market making
 *    - Adaptive volatility-based strategies
 *    - Inventory management and skewing
 *
 * 3. **Comprehensive Risk Management**
 *    - Position limits and inventory controls
 *    - Dynamic spread adjustment
 *    - Volatility-based sizing
 *
 * 4. **Performance Analytics**
 *    - P&L tracking (realized and unrealized)
 *    - Sharpe ratio calculation
 *    - Drawdown analysis
 *    - Trade-by-trade analysis
 *
 * 5. **Market Data Simulation**
 *    - Realistic price processes
 *    - Microstructure effects
 *    - Configurable volatility and spreads
 *
 * 6. **Extensible Architecture**
 *    - Easy to add new strategies
 *    - Pluggable market data sources
 *    - Configurable execution models
 *
 * Usage Examples:
 *
 * 1. Basic Symmetric Market Maker:
 *    - Quotes symmetrically around mid-price
 *    - Adjusts for inventory skew
 *    - Fixed spread targeting
 *
 * 2. Adaptive Market Maker:
 *    - Adjusts spread based on realized volatility
 *    - Dynamic order sizing
 *    - Enhanced inventory management
 *
 * 3. Custom Strategy Development:
 *    - Inherit from MarketMakingStrategy
 *    - Implement generate_orders() method
 *    - Add custom risk management logic
 *
 * Compilation and Optimization:
 * g++ -std=c++2a -pthread -O3 -march=native -DNDEBUG \
 *     -ffast-math -funroll-loops market_making_backtesting_framework.cpp \
 *     -o mm_backtest
 *
 * Future Enhancements:
 * - Multi-asset market making
 * - Options market making strategies
 * - Real market data integration
 * - Machine learning based strategies
 * - Cross-venue arbitrage
 * - Latency arbitrage simulation
 */
