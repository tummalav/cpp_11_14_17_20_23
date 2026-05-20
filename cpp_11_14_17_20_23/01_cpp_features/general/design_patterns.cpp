#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <atomic>
#include <iomanip>

/*
 * Capital Markets Trading Applications Design Patterns Examples in C++
 * Creational, Structural, and Behavioral Patterns
 * Modern C++ implementation with practical trading use cases
 */

// =============================================================================
// CREATIONAL DESIGN PATTERNS - CAPITAL MARKETS TRADING EXAMPLES
// =============================================================================

namespace creational_patterns {

    // -------------------------------------------------------------------------
    // 1. SINGLETON PATTERN - Market Data Manager
    // -------------------------------------------------------------------------
    class MarketDataManager {
    private:
        static std::unique_ptr<MarketDataManager> instance;
        static std::once_flag initialized;
        std::unordered_map<std::string, double> prices;
        mutable std::mutex priceMutex;

        MarketDataManager() = default;

    public:
        MarketDataManager(const MarketDataManager&) = delete;
        MarketDataManager& operator=(const MarketDataManager&) = delete;

        static MarketDataManager& getInstance() {
            std::call_once(initialized, []() {
                instance = std::unique_ptr<MarketDataManager>(new MarketDataManager());
            });
            return *instance;
        }

        void updatePrice(const std::string& symbol, double price) {
            std::lock_guard<std::mutex> lock(priceMutex);
            prices[symbol] = price;
            std::cout << "[MARKET DATA] " << symbol << " updated to $" << std::fixed
                      << std::setprecision(2) << price << std::endl;
        }

        double getPrice(const std::string& symbol) const {
            std::lock_guard<std::mutex> lock(priceMutex);
            auto it = prices.find(symbol);
            return (it != prices.end()) ? it->second : 0.0;
        }

        void displayPrices() const {
            std::lock_guard<std::mutex> lock(priceMutex);
            std::cout << "Current Market Prices:\n";
            for (const auto& [symbol, price] : prices) {
                std::cout << "  " << symbol << ": $" << std::fixed
                          << std::setprecision(2) << price << std::endl;
            }
        }
    };

    std::unique_ptr<MarketDataManager> MarketDataManager::instance = nullptr;
    std::once_flag MarketDataManager::initialized;

    // -------------------------------------------------------------------------
    // 2. FACTORY METHOD PATTERN - Order Factory
    // -------------------------------------------------------------------------
    enum class OrderSide { BUY, SELL };
    enum class OrderType { MARKET, LIMIT, STOP };

    class Order {
    public:
        virtual ~Order() = default;
        virtual void execute() = 0;
        virtual std::string getOrderInfo() const = 0;
        virtual double calculateCommission() const = 0;
    };

    class MarketOrder : public Order {
    private:
        std::string symbol;
        int quantity;
        OrderSide side;

    public:
        MarketOrder(const std::string& symbol, int quantity, OrderSide side)
            : symbol(symbol), quantity(quantity), side(side) {}

        void execute() override {
            double price = MarketDataManager::getInstance().getPrice(symbol);
            std::cout << "MARKET ORDER EXECUTED: " << (side == OrderSide::BUY ? "BUY" : "SELL")
                      << " " << quantity << " shares of " << symbol
                      << " at market price $" << std::fixed << std::setprecision(2) << price << std::endl;
        }

        std::string getOrderInfo() const override {
            return "Market Order: " + symbol + " x" + std::to_string(quantity);
        }

        double calculateCommission() const override {
            return quantity * 0.005; // $0.005 per share
        }
    };

    class LimitOrder : public Order {
    private:
        std::string symbol;
        int quantity;
        OrderSide side;
        double limitPrice;

    public:
        LimitOrder(const std::string& symbol, int quantity, OrderSide side, double limitPrice)
            : symbol(symbol), quantity(quantity), side(side), limitPrice(limitPrice) {}

        void execute() override {
            double marketPrice = MarketDataManager::getInstance().getPrice(symbol);
            bool canExecute = (side == OrderSide::BUY && marketPrice <= limitPrice) ||
                             (side == OrderSide::SELL && marketPrice >= limitPrice);

            if (canExecute) {
                std::cout << "LIMIT ORDER EXECUTED: " << (side == OrderSide::BUY ? "BUY" : "SELL")
                          << " " << quantity << " shares of " << symbol
                          << " at limit price $" << std::fixed << std::setprecision(2) << limitPrice << std::endl;
            } else {
                std::cout << "LIMIT ORDER PENDING: " << getOrderInfo() << " (Market: $"
                          << std::fixed << std::setprecision(2) << marketPrice << ")" << std::endl;
            }
        }

        std::string getOrderInfo() const override {
            return "Limit Order: " + symbol + " x" + std::to_string(quantity) + " @ $" + std::to_string(limitPrice);
        }

        double calculateCommission() const override {
            return quantity * 0.007; // $0.007 per share for limit orders
        }
    };

    class OrderFactory {
    public:
        static std::unique_ptr<Order> createOrder(OrderType type, const std::string& symbol,
                                                int quantity, OrderSide side, double price = 0.0) {
            switch (type) {
                case OrderType::MARKET:
                    return std::make_unique<MarketOrder>(symbol, quantity, side);
                case OrderType::LIMIT:
                    return std::make_unique<LimitOrder>(symbol, quantity, side, price);
                default:
                    return nullptr;
            }
        }
    };

    // -------------------------------------------------------------------------
    // 3. BUILDER PATTERN - Trading Strategy Builder
    // -------------------------------------------------------------------------
    class TradingStrategy {
    private:
        std::string strategyName;
        std::vector<std::string> instruments;
        double riskLimit;
        double positionSize;
        int maxPositions;
        bool enableDayTrading;
        std::string riskModel;

    public:
        void setStrategyName(const std::string& name) { this->strategyName = name; }
        void addInstrument(const std::string& instrument) { instruments.push_back(instrument); }
        void setRiskLimit(double limit) { this->riskLimit = limit; }
        void setPositionSize(double size) { this->positionSize = size; }
        void setMaxPositions(int max) { this->maxPositions = max; }
        void setDayTrading(bool enable) { this->enableDayTrading = enable; }
        void setRiskModel(const std::string& model) { this->riskModel = model; }

        void displayStrategy() const {
            std::cout << "Trading Strategy Configuration:\n";
            std::cout << "  Strategy: " << strategyName << "\n";
            std::cout << "  Instruments: ";
            for (const auto& inst : instruments) std::cout << inst << " ";
            std::cout << "\n  Risk Limit: $" << std::fixed << std::setprecision(2) << riskLimit << "\n";
            std::cout << "  Position Size: $" << std::fixed << std::setprecision(2) << positionSize << "\n";
            std::cout << "  Max Positions: " << maxPositions << "\n";
            std::cout << "  Day Trading: " << (enableDayTrading ? "Yes" : "No") << "\n";
            std::cout << "  Risk Model: " << riskModel << "\n\n";
        }
    };

    class TradingStrategyBuilder {
    protected:
        std::unique_ptr<TradingStrategy> strategy;

    public:
        TradingStrategyBuilder() : strategy(std::make_unique<TradingStrategy>()) {}
        virtual ~TradingStrategyBuilder() = default;

        virtual TradingStrategyBuilder& buildName() = 0;
        virtual TradingStrategyBuilder& buildInstruments() = 0;
        virtual TradingStrategyBuilder& buildRiskParameters() = 0;
        virtual TradingStrategyBuilder& buildPositioning() = 0;
        virtual TradingStrategyBuilder& buildTradingStyle() = 0;
        virtual TradingStrategyBuilder& buildRiskModel() = 0;

        std::unique_ptr<TradingStrategy> getResult() {
            return std::move(strategy);
        }
    };

    class MomentumStrategyBuilder : public TradingStrategyBuilder {
    public:
        TradingStrategyBuilder& buildName() override {
            strategy->setStrategyName("Momentum Trading Strategy");
            return *this;
        }
        TradingStrategyBuilder& buildInstruments() override {
            strategy->addInstrument("AAPL");
            strategy->addInstrument("GOOGL");
            strategy->addInstrument("TSLA");
            return *this;
        }
        TradingStrategyBuilder& buildRiskParameters() override {
            strategy->setRiskLimit(100000.0);
            return *this;
        }
        TradingStrategyBuilder& buildPositioning() override {
            strategy->setPositionSize(10000.0);
            strategy->setMaxPositions(5);
            return *this;
        }
        TradingStrategyBuilder& buildTradingStyle() override {
            strategy->setDayTrading(true);
            return *this;
        }
        TradingStrategyBuilder& buildRiskModel() override {
            strategy->setRiskModel("VaR 95%");
            return *this;
        }
    };

    void demonstrateCreationalPatterns() {
        std::cout << "\n=============== CREATIONAL PATTERNS - TRADING EXAMPLES ===============\n";

        // Singleton Pattern - Market Data Manager
        std::cout << "\n--- SINGLETON PATTERN - Market Data Manager ---\n";
        auto& marketData = MarketDataManager::getInstance();
        marketData.updatePrice("AAPL", 175.50);
        marketData.updatePrice("GOOGL", 2800.75);
        marketData.updatePrice("TSLA", 245.30);
        marketData.displayPrices();

        // Factory Method Pattern - Order Factory
        std::cout << "\n--- FACTORY METHOD PATTERN - Order Factory ---\n";
        auto marketOrder = OrderFactory::createOrder(OrderType::MARKET, "AAPL", 100, OrderSide::BUY);
        auto limitOrder = OrderFactory::createOrder(OrderType::LIMIT, "GOOGL", 50, OrderSide::SELL, 2850.0);

        std::cout << "Created orders using factory:\n";
        marketOrder->execute();
        limitOrder->execute();

        // Builder Pattern - Trading Strategy
        std::cout << "\n--- BUILDER PATTERN - Trading Strategy ---\n";
        MomentumStrategyBuilder momentumBuilder;
        auto momentumStrategy = momentumBuilder.buildName()
                                             .buildInstruments()
                                             .buildRiskParameters()
                                             .buildPositioning()
                                             .buildTradingStyle()
                                             .buildRiskModel()
                                             .getResult();
        std::cout << "Momentum Strategy:\n";
        momentumStrategy->displayStrategy();
    }
}

// =============================================================================
// STRUCTURAL DESIGN PATTERNS - CAPITAL MARKETS TRADING EXAMPLES
// =============================================================================

namespace structural_patterns {

    // -------------------------------------------------------------------------
    // 1. ADAPTER PATTERN - Legacy Trading System Integration
    // -------------------------------------------------------------------------
    class LegacyFixProtocol {
    public:
        void sendFixMessage(const std::string& fixMessage) {
            std::cout << "[LEGACY FIX] Sending: " << fixMessage << std::endl;
        }
    };

    class ModernTradingInterface {
    public:
        virtual ~ModernTradingInterface() = default;
        virtual void sendOrder(const std::string& jsonOrder) = 0;
    };

    class FixToJsonAdapter : public ModernTradingInterface {
    private:
        std::unique_ptr<LegacyFixProtocol> fixProtocol;

    public:
        FixToJsonAdapter() : fixProtocol(std::make_unique<LegacyFixProtocol>()) {}

        void sendOrder(const std::string& jsonOrder) override {
            std::cout << "Adapter converting JSON to FIX format\n";
            std::string fixMessage = "8=FIX.4.2|35=D|55=AAPL|54=1|38=100|40=2|44=175.50|";
            fixProtocol->sendFixMessage(fixMessage);
        }
    };

    // -------------------------------------------------------------------------
    // 2. DECORATOR PATTERN - Order Enhancement
    // -------------------------------------------------------------------------
    class BaseOrder {
    public:
        virtual ~BaseOrder() = default;
        virtual std::string getOrderDetails() const = 0;
        virtual double calculateTotalCost() const = 0;
    };

    class SimpleOrder : public BaseOrder {
    private:
        std::string symbol;
        int quantity;
        double price;

    public:
        SimpleOrder(const std::string& symbol, int quantity, double price)
            : symbol(symbol), quantity(quantity), price(price) {}

        std::string getOrderDetails() const override {
            return "Order: " + std::to_string(quantity) + " shares of " + symbol +
                   " at $" + std::to_string(price);
        }

        double calculateTotalCost() const override {
            return quantity * price;
        }
    };

    class OrderDecorator : public BaseOrder {
    protected:
        std::unique_ptr<BaseOrder> order;

    public:
        OrderDecorator(std::unique_ptr<BaseOrder> order) : order(std::move(order)) {}
    };

    class CommissionDecorator : public OrderDecorator {
    private:
        double commissionRate;

    public:
        CommissionDecorator(std::unique_ptr<BaseOrder> order, double rate = 0.005)
            : OrderDecorator(std::move(order)), commissionRate(rate) {}

        std::string getOrderDetails() const override {
            return order->getOrderDetails() + " + Commission(" + std::to_string(commissionRate * 100) + "%)";
        }

        double calculateTotalCost() const override {
            double baseCost = order->calculateTotalCost();
            return baseCost + (baseCost * commissionRate);
        }
    };

    // -------------------------------------------------------------------------
    // 3. FACADE PATTERN - Trading System Facade
    // -------------------------------------------------------------------------
    class OrderManagementSystem {
    public:
        void validateOrder(const std::string& order) {
            std::cout << "OMS: Validating order - " << order << std::endl;
        }
    };

    class RiskManager {
    public:
        bool checkRiskLimits(double orderValue) {
            std::cout << "Risk Manager: Checking position limits for $"
                      << std::fixed << std::setprecision(2) << orderValue << std::endl;
            return orderValue < 100000.0;
        }
    };

    class TradingSystemFacade {
    private:
        OrderManagementSystem oms;
        RiskManager riskManager;

    public:
        bool executeTradeWorkflow(const std::string& symbol, int quantity, double price) {
            std::cout << "Trading System: Executing complete trade workflow...\n";

            std::string orderDetails = symbol + " " + std::to_string(quantity) + "@" + std::to_string(price);
            double orderValue = quantity * price;

            // Risk check
            if (!riskManager.checkRiskLimits(orderValue)) {
                std::cout << "Trade rejected: Risk limit breach\n";
                return false;
            }

            // Order validation
            oms.validateOrder(orderDetails);

            std::cout << "Trade executed successfully!\n";
            return true;
        }
    };

    void demonstrateStructuralPatterns() {
        std::cout << "\n=============== STRUCTURAL PATTERNS - TRADING EXAMPLES ===============\n";

        // Adapter Pattern
        std::cout << "\n--- ADAPTER PATTERN - Legacy FIX Integration ---\n";
        auto modernInterface = std::make_unique<FixToJsonAdapter>();
        std::string jsonOrder = R"({"symbol":"AAPL","side":"BUY","quantity":100,"price":175.50})";
        modernInterface->sendOrder(jsonOrder);

        // Decorator Pattern
        std::cout << "\n--- DECORATOR PATTERN - Order Enhancement ---\n";
        auto basicOrder = std::make_unique<SimpleOrder>("AAPL", 1000, 175.50);
        std::cout << basicOrder->getOrderDetails() << " | Cost: $"
                  << std::fixed << std::setprecision(2) << basicOrder->calculateTotalCost() << std::endl;

        auto orderWithCommission = std::make_unique<CommissionDecorator>(std::move(basicOrder));
        std::cout << orderWithCommission->getOrderDetails() << " | Cost: $"
                  << std::fixed << std::setprecision(2) << orderWithCommission->calculateTotalCost() << std::endl;

        // Facade Pattern
        std::cout << "\n--- FACADE PATTERN - Trading System ---\n";
        TradingSystemFacade tradingSystem;
        tradingSystem.executeTradeWorkflow("AAPL", 500, 175.50);
        tradingSystem.executeTradeWorkflow("TSLA", 2000, 245.30); // Should trigger risk limit
    }
}

// =============================================================================
// BEHAVIORAL DESIGN PATTERNS - CAPITAL MARKETS TRADING EXAMPLES
// =============================================================================

namespace behavioral_patterns {

    // -------------------------------------------------------------------------
    // 1. OBSERVER PATTERN - Price Alert System
    // -------------------------------------------------------------------------
    class PriceObserver {
    public:
        virtual ~PriceObserver() = default;
        virtual void onPriceUpdate(const std::string& symbol, double price, double change) = 0;
    };

    class MarketDataStream {
    private:
        std::vector<PriceObserver*> observers;
        std::unordered_map<std::string, double> previousPrices;

    public:
        void subscribe(PriceObserver* observer) {
            observers.push_back(observer);
        }

        void updatePrice(const std::string& symbol, double newPrice) {
            double previousPrice = previousPrices[symbol];
            double change = newPrice - previousPrice;
            previousPrices[symbol] = newPrice;

            std::cout << "[MARKET DATA] " << symbol << " price updated: $"
                      << std::fixed << std::setprecision(2) << newPrice
                      << " (Change: " << (change >= 0 ? "+" : "") << change << ")" << std::endl;

            for (auto* observer : observers) {
                observer->onPriceUpdate(symbol, newPrice, change);
            }
        }
    };

    class TradingAlgorithm : public PriceObserver {
    private:
        std::string name;
        double buyThreshold;
        double sellThreshold;

    public:
        TradingAlgorithm(const std::string& name, double buyThreshold, double sellThreshold)
            : name(name), buyThreshold(buyThreshold), sellThreshold(sellThreshold) {}

        void onPriceUpdate(const std::string& symbol, double price, double change) override {
            std::cout << "[ALGO " << name << "] Analyzing " << symbol << " price: $"
                      << std::fixed << std::setprecision(2) << price;

            if (change < buyThreshold) {
                std::cout << " -> SIGNAL: Consider BUYING" << std::endl;
            } else if (change > sellThreshold) {
                std::cout << " -> SIGNAL: Consider SELLING" << std::endl;
            } else {
                std::cout << " -> SIGNAL: HOLD" << std::endl;
            }
        }
    };

    // -------------------------------------------------------------------------
    // 2. STRATEGY PATTERN - Order Execution Strategies
    // -------------------------------------------------------------------------
    class ExecutionStrategy {
    public:
        virtual ~ExecutionStrategy() = default;
        virtual void execute(const std::string& symbol, int quantity, double targetPrice) = 0;
        virtual std::string getStrategyName() const = 0;
    };

    class TWAPStrategy : public ExecutionStrategy {
    private:
        int timeSlices;

    public:
        TWAPStrategy(int slices = 10) : timeSlices(slices) {}

        void execute(const std::string& symbol, int quantity, double targetPrice) override {
            std::cout << "TWAP Execution: Splitting " << quantity << " shares of " << symbol
                      << " into " << timeSlices << " time slices" << std::endl;

            int sliceSize = quantity / timeSlices;
            for (int i = 0; i < timeSlices; ++i) {
                std::cout << "  Slice " << (i + 1) << ": Execute " << sliceSize
                          << " shares at market price" << std::endl;
            }
        }

        std::string getStrategyName() const override {
            return "Time-Weighted Average Price (TWAP)";
        }
    };

    class OrderExecutionContext {
    private:
        std::unique_ptr<ExecutionStrategy> strategy;

    public:
        void setStrategy(std::unique_ptr<ExecutionStrategy> newStrategy) {
            strategy = std::move(newStrategy);
        }

        void executeOrder(const std::string& symbol, int quantity, double targetPrice) {
            if (strategy) {
                std::cout << "Using " << strategy->getStrategyName() << std::endl;
                strategy->execute(symbol, quantity, targetPrice);
            }
        }
    };

    void demonstrateBehavioralPatterns() {
        std::cout << "\n=============== BEHAVIORAL PATTERNS - TRADING EXAMPLES ===============\n";

        // Observer Pattern
        std::cout << "\n--- OBSERVER PATTERN - Price Alert System ---\n";
        MarketDataStream marketStream;
        TradingAlgorithm momentumAlgo("MOMENTUM", -2.0, 3.0);
        TradingAlgorithm meanRevAlgo("MEAN_REV", -1.0, 1.5);

        marketStream.subscribe(&momentumAlgo);
        marketStream.subscribe(&meanRevAlgo);

        marketStream.updatePrice("AAPL", 175.00);
        marketStream.updatePrice("AAPL", 172.50); // -2.50 change
        marketStream.updatePrice("AAPL", 178.00); // +5.50 change

        // Strategy Pattern
        std::cout << "\n--- STRATEGY PATTERN - Order Execution Strategies ---\n";
        OrderExecutionContext executor;

        std::cout << "\nLarge order execution:\n";
        executor.setStrategy(std::make_unique<TWAPStrategy>(8));
        executor.executeOrder("AAPL", 10000, 175.50);
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL TRADING PATTERNS
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "CAPITAL MARKETS TRADING DESIGN PATTERNS EXAMPLES IN C++\n";
    std::cout << "=============================================================================\n";

    creational_patterns::demonstrateCreationalPatterns();
    structural_patterns::demonstrateStructuralPatterns();
    behavioral_patterns::demonstrateBehavioralPatterns();

    std::cout << "\n=============================================================================\n";
    std::cout << "CAPITAL MARKETS DESIGN PATTERNS SUMMARY:\n";
    std::cout << "=============================================================================\n";
    std::cout << "CREATIONAL PATTERNS:\n";
    std::cout << "  • Singleton: Market Data Manager - Global price feeds\n";
    std::cout << "  • Factory Method: Order Factory - Creates different order types\n";
    std::cout << "  • Builder: Trading Strategy Builder - Complex trading strategies\n\n";

    std::cout << "STRUCTURAL PATTERNS:\n";
    std::cout << "  • Adapter: Legacy FIX Protocol Integration\n";
    std::cout << "  • Decorator: Order Enhancement - Commission, risk checks\n";
    std::cout << "  • Facade: Trading System Facade - Simplified workflow\n\n";

    std::cout << "BEHAVIORAL PATTERNS:\n";
    std::cout << "  • Observer: Price Alert System - Algorithm notifications\n";
    std::cout << "  • Strategy: Order Execution Strategies - TWAP, VWAP\n";
    std::cout << "\nAll patterns demonstrated with capital markets use cases!\n";
    std::cout << "=============================================================================\n";

    return 0;
}
