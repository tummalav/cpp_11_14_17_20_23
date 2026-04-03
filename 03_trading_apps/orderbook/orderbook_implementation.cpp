#include <iostream>
#include <map>
#include <unordered_map>
#include <queue>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <functional>

/*
 * Comprehensive Order Book Implementation
 * Features: Price-time priority, order matching, market data, various order types
 * Use cases: Trading systems, market making, order management
 */

// =============================================================================
// 1. BASIC ORDER STRUCTURES AND TYPES
// =============================================================================

namespace orderbook {

    using OrderId = uint64_t;
    using Price = double;
    using Quantity = uint64_t;
    using Timestamp = std::chrono::high_resolution_clock::time_point;

    enum class Side {
        BUY,
        SELL
    };

    enum class OrderType {
        MARKET,
        LIMIT,
        STOP,
        STOP_LIMIT
    };

    enum class TimeInForce {
        GTC,  // Good Till Cancel
        IOC,  // Immediate or Cancel
        FOK,  // Fill or Kill
        DAY   // Day order
    };

    enum class OrderStatus {
        PENDING,
        PARTIALLY_FILLED,
        FILLED,
        CANCELLED,
        REJECTED
    };

    struct Order {
        OrderId id;
        Side side;
        OrderType type;
        TimeInForce tif;
        Price price;
        Quantity quantity;
        Quantity filled_quantity;
        Timestamp timestamp;
        OrderStatus status;
        std::string client_id;

        Order(OrderId order_id, Side order_side, OrderType order_type,
              Price order_price, Quantity order_qty, const std::string& client)
            : id(order_id), side(order_side), type(order_type), tif(TimeInForce::GTC),
              price(order_price), quantity(order_qty), filled_quantity(0),
              timestamp(std::chrono::high_resolution_clock::now()),
              status(OrderStatus::PENDING), client_id(client) {}

        Quantity remaining_quantity() const {
            return quantity - filled_quantity;
        }

        bool is_fully_filled() const {
            return filled_quantity >= quantity;
        }

        void print() const {
            std::cout << "Order[" << id << "] "
                      << (side == Side::BUY ? "BUY" : "SELL")
                      << " " << quantity << "@" << std::fixed << std::setprecision(2) << price
                      << " (filled: " << filled_quantity << ", remaining: " << remaining_quantity() << ")"
                      << " Status: ";
            switch(status) {
                case OrderStatus::PENDING: std::cout << "PENDING"; break;
                case OrderStatus::PARTIALLY_FILLED: std::cout << "PARTIALLY_FILLED"; break;
                case OrderStatus::FILLED: std::cout << "FILLED"; break;
                case OrderStatus::CANCELLED: std::cout << "CANCELLED"; break;
                case OrderStatus::REJECTED: std::cout << "REJECTED"; break;
            }
            std::cout << "\n";
        }
    };

    struct Trade {
        OrderId buy_order_id;
        OrderId sell_order_id;
        Price price;
        Quantity quantity;
        Timestamp timestamp;
        std::string buy_client;
        std::string sell_client;

        Trade(OrderId buy_id, OrderId sell_id, Price trade_price,
              Quantity trade_qty, const std::string& buyer, const std::string& seller)
            : buy_order_id(buy_id), sell_order_id(sell_id), price(trade_price),
              quantity(trade_qty), timestamp(std::chrono::high_resolution_clock::now()),
              buy_client(buyer), sell_client(seller) {}

        void print() const {
            std::cout << "Trade: " << quantity << "@" << std::fixed << std::setprecision(2) << price
                      << " (Buy Order: " << buy_order_id << ", Sell Order: " << sell_order_id << ")\n";
        }
    };

    // =============================================================================
    // 2. PRICE LEVEL IMPLEMENTATION
    // =============================================================================

    class PriceLevel {
    private:
        Price price_;
        std::queue<std::shared_ptr<Order>> orders_;
        Quantity total_quantity_;

    public:
        PriceLevel(Price price) : price_(price), total_quantity_(0) {}

        void add_order(std::shared_ptr<Order> order) {
            orders_.push(order);
            total_quantity_ += order->remaining_quantity();
        }

        std::shared_ptr<Order> get_next_order() {
            if (orders_.empty()) return nullptr;

            auto order = orders_.front();
            if (order->is_fully_filled() || order->status == OrderStatus::CANCELLED) {
                orders_.pop();
                total_quantity_ -= (order->quantity - order->filled_quantity);
                return get_next_order(); // Recursively get next valid order
            }
            return order;
        }

        void remove_filled_order() {
            if (!orders_.empty()) {
                auto order = orders_.front();
                orders_.pop();
                total_quantity_ -= order->remaining_quantity();
            }
        }

        bool remove_order(OrderId order_id) {
            std::queue<std::shared_ptr<Order>> temp_queue;
            bool found = false;

            while (!orders_.empty()) {
                auto order = orders_.front();
                orders_.pop();

                if (order->id == order_id) {
                    total_quantity_ -= order->remaining_quantity();
                    order->status = OrderStatus::CANCELLED;
                    found = true;
                } else {
                    temp_queue.push(order);
                }
            }

            orders_ = std::move(temp_queue);
            return found;
        }

        Price get_price() const { return price_; }
        Quantity get_total_quantity() const { return total_quantity_; }
        bool is_empty() const { return orders_.empty() || total_quantity_ == 0; }
        size_t order_count() const { return orders_.size(); }

        void print() const {
            std::cout << "Price Level " << std::fixed << std::setprecision(2) << price_
                      << ": " << total_quantity_ << " shares (" << orders_.size() << " orders)\n";
        }
    };

    // =============================================================================
    // 3. ORDER BOOK IMPLEMENTATION
    // =============================================================================

    class OrderBook {
    private:
        std::string symbol_;

        // Buy side (bids) - highest price first
        std::map<Price, std::unique_ptr<PriceLevel>, std::greater<Price>> buy_levels_;

        // Sell side (asks) - lowest price first
        std::map<Price, std::unique_ptr<PriceLevel>, std::less<Price>> sell_levels_;

        // Order tracking
        std::unordered_map<OrderId, std::shared_ptr<Order>> orders_;

        // Trade history
        std::vector<Trade> trades_;

        // Statistics
        Price last_trade_price_;
        Quantity total_volume_;
        OrderId next_order_id_;

        // Event callbacks
        std::function<void(const Trade&)> on_trade_callback_;
        std::function<void(const Order&)> on_order_update_callback_;

    public:
        OrderBook(const std::string& symbol)
            : symbol_(symbol), last_trade_price_(0.0), total_volume_(0), next_order_id_(1) {}

        // Order management
        OrderId add_order(Side side, OrderType type, Price price, Quantity quantity,
                         const std::string& client_id = "") {
            auto order = std::make_shared<Order>(next_order_id_++, side, type, price, quantity, client_id);

            if (type == OrderType::MARKET) {
                return process_market_order(order);
            } else {
                return process_limit_order(order);
            }
        }

        bool cancel_order(OrderId order_id) {
            auto it = orders_.find(order_id);
            if (it == orders_.end()) {
                std::cout << "Order " << order_id << " not found\n";
                return false;
            }

            auto order = it->second;
            if (order->status == OrderStatus::FILLED) {
                std::cout << "Cannot cancel filled order " << order_id << "\n";
                return false;
            }

            // Remove from price level
            if (order->side == Side::BUY) {
                auto level_it = buy_levels_.find(order->price);
                if (level_it != buy_levels_.end()) {
                    level_it->second->remove_order(order_id);
                    if (level_it->second->is_empty()) {
                        buy_levels_.erase(level_it);
                    }
                }
            } else {
                auto level_it = sell_levels_.find(order->price);
                if (level_it != sell_levels_.end()) {
                    level_it->second->remove_order(order_id);
                    if (level_it->second->is_empty()) {
                        sell_levels_.erase(level_it);
                    }
                }
            }

            order->status = OrderStatus::CANCELLED;
            orders_.erase(it);

            if (on_order_update_callback_) {
                on_order_update_callback_(*order);
            }

            std::cout << "Order " << order_id << " cancelled\n";
            return true;
        }

        // Market data queries
        Price get_best_bid() const {
            return buy_levels_.empty() ? 0.0 : buy_levels_.begin()->first;
        }

        Price get_best_ask() const {
            return sell_levels_.empty() ? 0.0 : sell_levels_.begin()->first;
        }

        Price get_mid_price() const {
            Price bid = get_best_bid();
            Price ask = get_best_ask();
            return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
        }

        Price get_spread() const {
            Price bid = get_best_bid();
            Price ask = get_best_ask();
            return (bid > 0 && ask > 0) ? (ask - bid) : 0.0;
        }

        Quantity get_bid_quantity(int levels = 1) const {
            Quantity total = 0;
            int count = 0;
            for (const auto& [price, level] : buy_levels_) {
                if (count >= levels) break;
                total += level->get_total_quantity();
                count++;
            }
            return total;
        }

        Quantity get_ask_quantity(int levels = 1) const {
            Quantity total = 0;
            int count = 0;
            for (const auto& [price, level] : sell_levels_) {
                if (count >= levels) break;
                total += level->get_total_quantity();
                count++;
            }
            return total;
        }

        // Statistics
        Price get_last_trade_price() const { return last_trade_price_; }
        Quantity get_total_volume() const { return total_volume_; }
        size_t get_trade_count() const { return trades_.size(); }

        const std::vector<Trade>& get_trades() const { return trades_; }

        std::shared_ptr<Order> get_order(OrderId order_id) const {
            auto it = orders_.find(order_id);
            return (it != orders_.end()) ? it->second : nullptr;
        }

        // Event callbacks
        void set_trade_callback(std::function<void(const Trade&)> callback) {
            on_trade_callback_ = callback;
        }

        void set_order_update_callback(std::function<void(const Order&)> callback) {
            on_order_update_callback_ = callback;
        }

        // Display functions
        void print_book(int levels = 5) const {
            std::cout << "\n=== Order Book for " << symbol_ << " ===\n";
            std::cout << "Best Bid: " << std::fixed << std::setprecision(2) << get_best_bid()
                      << ", Best Ask: " << get_best_ask()
                      << ", Spread: " << get_spread() << "\n";
            std::cout << "Last Trade: " << last_trade_price_
                      << ", Volume: " << total_volume_ << "\n\n";

            // Print asks (highest to lowest)
            std::cout << "ASKS (Sell Orders):\n";
            std::cout << "Price    | Quantity | Orders\n";
            std::cout << "---------|----------|-------\n";

            auto ask_it = sell_levels_.rbegin();
            for (int i = 0; i < levels && ask_it != sell_levels_.rend(); ++ask_it, ++i) {
                std::cout << std::setw(8) << std::fixed << std::setprecision(2) << ask_it->first
                          << " | " << std::setw(8) << ask_it->second->get_total_quantity()
                          << " | " << std::setw(6) << ask_it->second->order_count() << "\n";
            }

            std::cout << "---------|----------|-------\n";
            std::cout << "BIDS (Buy Orders):\n";

            auto bid_it = buy_levels_.begin();
            for (int i = 0; i < levels && bid_it != buy_levels_.end(); ++bid_it, ++i) {
                std::cout << std::setw(8) << std::fixed << std::setprecision(2) << bid_it->first
                          << " | " << std::setw(8) << bid_it->second->get_total_quantity()
                          << " | " << std::setw(6) << bid_it->second->order_count() << "\n";
            }
            std::cout << "\n";
        }

        void print_trades(int count = 10) const {
            std::cout << "Recent Trades (last " << count << "):\n";
            std::cout << "Price    | Quantity | Buy Order | Sell Order\n";
            std::cout << "---------|----------|-----------|----------\n";

            int start = std::max(0, static_cast<int>(trades_.size()) - count);
            for (int i = start; i < static_cast<int>(trades_.size()); ++i) {
                const auto& trade = trades_[i];
                std::cout << std::setw(8) << std::fixed << std::setprecision(2) << trade.price
                          << " | " << std::setw(8) << trade.quantity
                          << " | " << std::setw(9) << trade.buy_order_id
                          << " | " << std::setw(9) << trade.sell_order_id << "\n";
            }
            std::cout << "\n";
        }

    private:
        OrderId process_market_order(std::shared_ptr<Order> order) {
            orders_[order->id] = order;

            if (order->side == Side::BUY) {
                // Market buy: match against asks (lowest price first)
                match_market_order_against_asks(order);
            } else {
                // Market sell: match against bids (highest price first)
                match_market_order_against_bids(order);
            }

            return order->id;
        }

        OrderId process_limit_order(std::shared_ptr<Order> order) {
            orders_[order->id] = order;

            if (order->side == Side::BUY) {
                // Try to match against existing asks first
                match_buy_order_against_asks(order);

                // Add remaining quantity to buy levels if not fully filled
                if (!order->is_fully_filled() && order->status != OrderStatus::CANCELLED) {
                    add_to_buy_levels(order);
                }
            } else {
                // Try to match against existing bids first
                match_sell_order_against_bids(order);

                // Add remaining quantity to sell levels if not fully filled
                if (!order->is_fully_filled() && order->status != OrderStatus::CANCELLED) {
                    add_to_sell_levels(order);
                }
            }

            return order->id;
        }

        void match_market_order_against_asks(std::shared_ptr<Order> buy_order) {
            while (!buy_order->is_fully_filled() && !sell_levels_.empty()) {
                auto& [price, level] = *sell_levels_.begin();
                auto sell_order = level->get_next_order();

                if (!sell_order) {
                    sell_levels_.erase(sell_levels_.begin());
                    continue;
                }

                execute_trade(buy_order, sell_order, sell_order->price);

                if (sell_order->is_fully_filled()) {
                    level->remove_filled_order();
                    if (level->is_empty()) {
                        sell_levels_.erase(sell_levels_.begin());
                    }
                }
            }

            // Update order status
            if (buy_order->is_fully_filled()) {
                buy_order->status = OrderStatus::FILLED;
            } else if (buy_order->filled_quantity > 0) {
                buy_order->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        void match_market_order_against_bids(std::shared_ptr<Order> sell_order) {
            while (!sell_order->is_fully_filled() && !buy_levels_.empty()) {
                auto& [price, level] = *buy_levels_.begin();
                auto buy_order = level->get_next_order();

                if (!buy_order) {
                    buy_levels_.erase(buy_levels_.begin());
                    continue;
                }

                execute_trade(buy_order, sell_order, buy_order->price);

                if (buy_order->is_fully_filled()) {
                    level->remove_filled_order();
                    if (level->is_empty()) {
                        buy_levels_.erase(buy_levels_.begin());
                    }
                }
            }

            // Update order status
            if (sell_order->is_fully_filled()) {
                sell_order->status = OrderStatus::FILLED;
            } else if (sell_order->filled_quantity > 0) {
                sell_order->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        void match_buy_order_against_asks(std::shared_ptr<Order> buy_order) {
            while (!buy_order->is_fully_filled() && !sell_levels_.empty()) {
                auto& [ask_price, level] = *sell_levels_.begin();

                // Price improvement check
                if (buy_order->price < ask_price) {
                    break; // No match possible at this price or better
                }

                auto sell_order = level->get_next_order();
                if (!sell_order) {
                    sell_levels_.erase(sell_levels_.begin());
                    continue;
                }

                execute_trade(buy_order, sell_order, ask_price);

                if (sell_order->is_fully_filled()) {
                    level->remove_filled_order();
                    if (level->is_empty()) {
                        sell_levels_.erase(sell_levels_.begin());
                    }
                }
            }
        }

        void match_sell_order_against_bids(std::shared_ptr<Order> sell_order) {
            while (!sell_order->is_fully_filled() && !buy_levels_.empty()) {
                auto& [bid_price, level] = *buy_levels_.begin();

                // Price improvement check
                if (sell_order->price > bid_price) {
                    break; // No match possible at this price or better
                }

                auto buy_order = level->get_next_order();
                if (!buy_order) {
                    buy_levels_.erase(buy_levels_.begin());
                    continue;
                }

                execute_trade(buy_order, sell_order, bid_price);

                if (buy_order->is_fully_filled()) {
                    level->remove_filled_order();
                    if (level->is_empty()) {
                        buy_levels_.erase(buy_levels_.begin());
                    }
                }
            }
        }

        void execute_trade(std::shared_ptr<Order> buy_order, std::shared_ptr<Order> sell_order,
                          Price trade_price) {
            Quantity trade_quantity = std::min(buy_order->remaining_quantity(),
                                              sell_order->remaining_quantity());

            // Update orders
            buy_order->filled_quantity += trade_quantity;
            sell_order->filled_quantity += trade_quantity;

            // Update order statuses
            if (buy_order->is_fully_filled()) {
                buy_order->status = OrderStatus::FILLED;
            } else if (buy_order->filled_quantity > 0) {
                buy_order->status = OrderStatus::PARTIALLY_FILLED;
            }

            if (sell_order->is_fully_filled()) {
                sell_order->status = OrderStatus::FILLED;
            } else if (sell_order->filled_quantity > 0) {
                sell_order->status = OrderStatus::PARTIALLY_FILLED;
            }

            // Create trade record
            Trade trade(buy_order->id, sell_order->id, trade_price, trade_quantity,
                       buy_order->client_id, sell_order->client_id);
            trades_.push_back(trade);

            // Update statistics
            last_trade_price_ = trade_price;
            total_volume_ += trade_quantity;

            // Notify callbacks
            if (on_trade_callback_) {
                on_trade_callback_(trade);
            }

            if (on_order_update_callback_) {
                on_order_update_callback_(*buy_order);
                on_order_update_callback_(*sell_order);
            }

            std::cout << "TRADE EXECUTED: " << trade_quantity << " @ "
                      << std::fixed << std::setprecision(2) << trade_price << "\n";
        }

        void add_to_buy_levels(std::shared_ptr<Order> order) {
            auto it = buy_levels_.find(order->price);
            if (it == buy_levels_.end()) {
                auto level = std::make_unique<PriceLevel>(order->price);
                level->add_order(order);
                buy_levels_[order->price] = std::move(level);
            } else {
                it->second->add_order(order);
            }
        }

        void add_to_sell_levels(std::shared_ptr<Order> order) {
            auto it = sell_levels_.find(order->price);
            if (it == sell_levels_.end()) {
                auto level = std::make_unique<PriceLevel>(order->price);
                level->add_order(order);
                sell_levels_[order->price] = std::move(level);
            } else {
                it->second->add_order(order);
            }
        }
    };

    // =============================================================================
    // 4. ORDER BOOK MANAGER AND UTILITIES
    // =============================================================================

    class OrderBookManager {
    private:
        std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;

    public:
        OrderBook* get_or_create_book(const std::string& symbol) {
            auto it = books_.find(symbol);
            if (it == books_.end()) {
                auto book = std::make_unique<OrderBook>(symbol);
                OrderBook* book_ptr = book.get();
                books_[symbol] = std::move(book);
                return book_ptr;
            }
            return it->second.get();
        }

        OrderBook* get_book(const std::string& symbol) {
            auto it = books_.find(symbol);
            return (it != books_.end()) ? it->second.get() : nullptr;
        }

        void print_all_books() const {
            for (const auto& [symbol, book] : books_) {
                book->print_book();
            }
        }

        size_t book_count() const { return books_.size(); }
    };

    // =============================================================================
    // 5. TRADING SIMULATION AND EXAMPLES
    // =============================================================================

    class TradingSimulator {
    private:
        OrderBook& book_;
        std::mt19937 rng_;

    public:
        TradingSimulator(OrderBook& book) : book_(book), rng_(std::random_device{}()) {}

        void simulate_trading_session(int num_orders = 50) {
            std::cout << "\n=== SIMULATING TRADING SESSION ===\n";

            std::uniform_real_distribution<double> price_dist(99.0, 101.0);
            std::uniform_int_distribution<int> quantity_dist(100, 1000);
            std::uniform_int_distribution<int> side_dist(0, 1);
            std::uniform_int_distribution<int> client_dist(1, 10);

            // Add some initial orders to build the book
            for (int i = 0; i < num_orders; ++i) {
                Side side = (side_dist(rng_) == 0) ? Side::BUY : Side::SELL;
                Price price = price_dist(rng_);
                Quantity quantity = quantity_dist(rng_);
                std::string client = "Client" + std::to_string(client_dist(rng_));

                // Adjust price slightly based on side to create spread
                if (side == Side::BUY) {
                    price -= 0.05; // Bids slightly lower
                } else {
                    price += 0.05; // Asks slightly higher
                }

                OrderId order_id = book_.add_order(side, OrderType::LIMIT, price, quantity, client);

                if (i % 10 == 0) {
                    std::cout << "Added order " << order_id << "\n";
                    book_.print_book(3);
                }
            }

            std::cout << "Final order book state:\n";
            book_.print_book();
            book_.print_trades();
        }

        void simulate_market_orders(int count = 5) {
            std::cout << "\n=== SIMULATING MARKET ORDERS ===\n";

            std::uniform_int_distribution<int> quantity_dist(50, 200);
            std::uniform_int_distribution<int> side_dist(0, 1);

            for (int i = 0; i < count; ++i) {
                Side side = (side_dist(rng_) == 0) ? Side::BUY : Side::SELL;
                Quantity quantity = quantity_dist(rng_);
                std::string client = "MarketClient" + std::to_string(i + 1);

                std::cout << "\nSubmitting market " << (side == Side::BUY ? "buy" : "sell")
                          << " order for " << quantity << " shares\n";

                OrderId order_id = book_.add_order(side, OrderType::MARKET, 0.0, quantity, client);
                book_.print_book(3);
            }
        }
    };

} // namespace orderbook

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ORDER BOOK FUNCTIONALITY
// =============================================================================

int main() {
    using namespace orderbook;

    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE ORDER BOOK IMPLEMENTATION\n";
    std::cout << "=============================================================================\n";

    // Create order book manager
    OrderBookManager manager;

    // Get or create order book for a symbol
    auto* book = manager.get_or_create_book("AAPL");

    // Set up event callbacks
    book->set_trade_callback([](const Trade& trade) {
        std::cout << "TRADE NOTIFICATION: ";
        trade.print();
    });

    book->set_order_update_callback([](const Order& order) {
        if (order.status == OrderStatus::FILLED) {
            std::cout << "ORDER FILLED: Order " << order.id << " fully executed\n";
        }
    });

    std::cout << "\n1. BASIC ORDER BOOK OPERATIONS\n";
    std::cout << "================================\n";

    // Add some initial orders
    auto order1 = book->add_order(Side::BUY, OrderType::LIMIT, 100.00, 500, "Client1");
    auto order2 = book->add_order(Side::BUY, OrderType::LIMIT, 99.95, 300, "Client2");
    auto order3 = book->add_order(Side::BUY, OrderType::LIMIT, 99.90, 200, "Client3");

    auto order4 = book->add_order(Side::SELL, OrderType::LIMIT, 100.05, 400, "Client4");
    auto order5 = book->add_order(Side::SELL, OrderType::LIMIT, 100.10, 600, "Client5");
    auto order6 = book->add_order(Side::SELL, OrderType::LIMIT, 100.15, 300, "Client6");

    book->print_book();

    std::cout << "\n2. MARKET DATA QUERIES\n";
    std::cout << "======================\n";
    std::cout << "Best Bid: " << book->get_best_bid() << "\n";
    std::cout << "Best Ask: " << book->get_best_ask() << "\n";
    std::cout << "Mid Price: " << book->get_mid_price() << "\n";
    std::cout << "Spread: " << book->get_spread() << "\n";
    std::cout << "Bid Quantity (top 3 levels): " << book->get_bid_quantity(3) << "\n";
    std::cout << "Ask Quantity (top 3 levels): " << book->get_ask_quantity(3) << "\n";

    std::cout << "\n3. EXECUTING TRADES\n";
    std::cout << "===================\n";

    // Add a buy order that crosses the spread
    std::cout << "Adding aggressive buy order at 100.08...\n";
    auto aggressive_buy = book->add_order(Side::BUY, OrderType::LIMIT, 100.08, 250, "AggressiveBuyer");
    book->print_book();

    // Add a sell order that crosses the spread
    std::cout << "Adding aggressive sell order at 99.97...\n";
    auto aggressive_sell = book->add_order(Side::SELL, OrderType::LIMIT, 99.97, 150, "AggressiveSeller");
    book->print_book();

    std::cout << "\n4. MARKET ORDERS\n";
    std::cout << "================\n";

    // Market buy order
    std::cout << "Submitting market buy order for 100 shares...\n";
    auto market_buy = book->add_order(Side::BUY, OrderType::MARKET, 0.0, 100, "MarketBuyer");
    book->print_book();

    // Market sell order
    std::cout << "Submitting market sell order for 200 shares...\n";
    auto market_sell = book->add_order(Side::SELL, OrderType::MARKET, 0.0, 200, "MarketSeller");
    book->print_book();

    std::cout << "\n5. ORDER CANCELLATION\n";
    std::cout << "=====================\n";

    std::cout << "Cancelling order " << order2 << "...\n";
    book->cancel_order(order2);
    book->print_book();

    std::cout << "\n6. TRADE HISTORY\n";
    std::cout << "================\n";
    book->print_trades();

    std::cout << "\n7. TRADING SIMULATION\n";
    std::cout << "=====================\n";

    // Create a new book for simulation
    auto* sim_book = manager.get_or_create_book("GOOGL");
    TradingSimulator simulator(*sim_book);

    // Run simulation
    simulator.simulate_trading_session(30);
    simulator.simulate_market_orders(3);

    std::cout << "\n8. FINAL STATISTICS\n";
    std::cout << "===================\n";

    std::cout << "AAPL Statistics:\n";
    std::cout << "Last Trade Price: " << book->get_last_trade_price() << "\n";
    std::cout << "Total Volume: " << book->get_total_volume() << "\n";
    std::cout << "Trade Count: " << book->get_trade_count() << "\n";

    std::cout << "\nGOOGL Statistics:\n";
    std::cout << "Last Trade Price: " << sim_book->get_last_trade_price() << "\n";
    std::cout << "Total Volume: " << sim_book->get_total_volume() << "\n";
    std::cout << "Trade Count: " << sim_book->get_trade_count() << "\n";

    std::cout << "\nTotal Order Books: " << manager.book_count() << "\n";

    std::cout << "\n=============================================================================\n";
    std::cout << "KEY FEATURES DEMONSTRATED:\n";
    std::cout << "1. Price-time priority matching\n";
    std::cout << "2. Market and limit orders\n";
    std::cout << "3. Order cancellation\n";
    std::cout << "4. Real-time market data\n";
    std::cout << "5. Trade execution and reporting\n";
    std::cout << "6. Event-driven callbacks\n";
    std::cout << "7. Multi-symbol support\n";
    std::cout << "8. Trading simulation\n";
    std::cout << "=============================================================================\n";

    return 0;
}
