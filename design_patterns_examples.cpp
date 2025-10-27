#include <map>
    };
#include <thread>
#include <mutex>
#include <queue>
    // -------------------------------------------------------------------------
namespace creational_patterns {
    private:
    // -------------------------------------------------------------------------
    // 1. SINGLETON PATTERN - Market Data Manager
        double riskLimit;
        double positionSize;
    private:
        static std::unique_ptr<MarketDataManager> instance;
        bool enableSwingTrading;
        std::unordered_map<std::string, double> prices;

        double getPrice(const std::string& symbol) const {
        void execute() override {
            double marketPrice = MarketDataManager::getInstance().getPrice(symbol);
            bool triggered = (side == OrderSide::BUY && marketPrice >= stopPrice) ||
                            (side == OrderSide::SELL && marketPrice <= stopPrice);

            if (triggered) {
                std::cout << "STOP ORDER TRIGGERED: " << (side == OrderSide::BUY ? "BUY" : "SELL")
                          << " " << quantity << " shares of " << symbol
                          << " at market price $" << std::fixed << std::setprecision(2) << marketPrice << std::endl;
            } else {
                std::cout << "STOP ORDER PENDING: " << getOrderInfo() << " (Market: $"
                          << std::fixed << std::setprecision(2) << marketPrice << ")" << std::endl;
            }
        }

        std::string getOrderInfo() const override {
            return "Stop Order: " + symbol + " x" + std::to_string(quantity) + " @ $" + std::to_string(stopPrice);
        }

        double calculateCommission() const override {
            return quantity * 0.008; // $0.008 per share for stop orders
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
                case OrderType::STOP:
                    return std::make_unique<StopOrder>(symbol, quantity, side, price);
                default:
                    return nullptr;
            }
                int sliceQuantity = static_cast<int>(quantity * (volumeProfile[i] / 100.0));
                std::cout << "  Hour " << (i + 1) << ": Execute " << sliceQuantity
                          << " shares (" << volumeProfile[i] << "% of volume)" << std::endl;
            }
        }

        std::string getStrategyName() const override {
            return "Volume-Weighted Average Price (VWAP)";

    // -------------------------------------------------------------------------
    // 3. BUILDER PATTERN - Trading Strategy Builder
    class AggressiveStrategy : public ExecutionStrategy {
    public:
        void execute(const std::string& symbol, int quantity, double targetPrice) override {
            std::cout << "Aggressive Execution: Immediate market order for " << quantity
                      << " shares of " << symbol << " at current market price" << std::endl;
            std::cout << "  Priority: Speed over price optimization" << std::endl;
        }

        std::string getStrategyName() const override {
            return "Aggressive/Immediate Execution";
        }
    };

    class OrderExecutionContext {
    class TradingStrategy {
        std::unique_ptr<ExecutionStrategy> strategy;
        std::string strategyName;
        std::vector<std::string> instruments;
        void setStrategy(std::unique_ptr<ExecutionStrategy> newStrategy) {
            strategy = std::move(newStrategy);
        int maxPositions;
        bool enableDayTrading;
        void executeOrder(const std::string& symbol, int quantity, double targetPrice) {
        std::string riskModel;
                std::cout << "Using " << strategy->getStrategyName() << std::endl;
                strategy->execute(symbol, quantity, targetPrice);
            }
        }
    };

    // -------------------------------------------------------------------------
    // 3. COMMAND PATTERN - Trade Commands
    // -------------------------------------------------------------------------
    class TradeCommand {
    public:
        virtual ~TradeCommand() = default;
        virtual void execute() = 0;
        virtual void undo() = 0;
        virtual std::string getDescription() const = 0;
    };

    class TradingAccount {
    private:
        std::unordered_map<std::string, int> positions;
        double cashBalance;

    public:
        TradingAccount(double initialCash) : cashBalance(initialCash) {}

        void buyShares(const std::string& symbol, int quantity, double price) {
            double cost = quantity * price;
            if (cashBalance >= cost) {
                positions[symbol] += quantity;
                cashBalance -= cost;
                std::cout << "BOUGHT: " << quantity << " shares of " << symbol
                          << " at $" << std::fixed << std::setprecision(2) << price
                          << " | Cash: $" << cashBalance << std::endl;
            } else {
                std::cout << "INSUFFICIENT FUNDS for purchase" << std::endl;
            }
        }

        void sellShares(const std::string& symbol, int quantity, double price) {
            if (positions[symbol] >= quantity) {
                positions[symbol] -= quantity;
                cashBalance += quantity * price;
                std::cout << "SOLD: " << quantity << " shares of " << symbol
                          << " at $" << std::fixed << std::setprecision(2) << price
                          << " | Cash: $" << cashBalance << std::endl;
            } else {
                std::cout << "INSUFFICIENT SHARES to sell" << std::endl;
            }
        }

        int getPosition(const std::string& symbol) const {
            auto it = positions.find(symbol);
            return (it != positions.end()) ? it->second : 0;
        }

        double getCashBalance() const { return cashBalance; }
    };

    class BuyCommand : public TradeCommand {
    private:
        TradingAccount& account;
        std::string symbol;
        int quantity;
        double price;

    public:
        BuyCommand(TradingAccount& acc, const std::string& sym, int qty, double prc)
            : account(acc), symbol(sym), quantity(qty), price(prc) {}

        void execute() override {
            account.buyShares(symbol, quantity, price);
        }

        void undo() override {
            account.sellShares(symbol, quantity, price);
            std::cout << "UNDOING BUY: Sold back " << quantity << " shares of " << symbol << std::endl;
        }

        std::string getDescription() const override {
            return "BUY " + std::to_string(quantity) + " shares of " + symbol;
        }
    };

    class SellCommand : public TradeCommand {
    private:
        TradingAccount& account;
        std::string symbol;
        int quantity;
        double price;

    public:
        SellCommand(TradingAccount& acc, const std::string& sym, int qty, double prc)
            : account(acc), symbol(sym), quantity(qty), price(prc) {}

        void execute() override {
            account.sellShares(symbol, quantity, price);
        }

        void undo() override {
            account.buyShares(symbol, quantity, price);
            std::cout << "UNDOING SELL: Bought back " << quantity << " shares of " << symbol << std::endl;
        }

        std::string getDescription() const override {
            return "SELL " + std::to_string(quantity) + " shares of " + symbol;
        }
    };

    class TradingPlatform {
    private:
        std::vector<std::unique_ptr<TradeCommand>> commandHistory;

    public:
        void executeCommand(std::unique_ptr<TradeCommand> command) {
            std::cout << "Executing: " << command->getDescription() << std::endl;
            command->execute();
            commandHistory.push_back(std::move(command));
        }

        void undoLastCommand() {
            if (!commandHistory.empty()) {
                auto& lastCommand = commandHistory.back();
                std::cout << "Undoing: " << lastCommand->getDescription() << std::endl;
                lastCommand->undo();
                commandHistory.pop_back();
            } else {
                std::cout << "No commands to undo" << std::endl;
        void setStrategyName(const std::string& name) { this->strategyName = name; }
        void addInstrument(const std::string& instrument) { instruments.push_back(instrument); }
        void setRiskLimit(double limit) { this->riskLimit = limit; }
        void setPositionSize(double size) { this->positionSize = size; }
        void setMaxPositions(int max) { this->maxPositions = max; }
        void setDayTrading(bool enable) { this->enableDayTrading = enable; }
        void setSwingTrading(bool enable) { this->enableSwingTrading = enable; }
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
            std::cout << "  Swing Trading: " << (enableSwingTrading ? "Yes" : "No") << "\n";
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
            strategy->addInstrument("NVDA");
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
            strategy->setSwingTrading(false);
            return *this;
        }
        TradingStrategyBuilder& buildRiskModel() override {
            strategy->setRiskModel("VaR 95%");
            return *this;
        }
    };

    class MeanReversionStrategyBuilder : public TradingStrategyBuilder {
    public:
        TradingStrategyBuilder& buildName() override {
            strategy->setStrategyName("Mean Reversion Strategy");
            return *this;
        }
        TradingStrategyBuilder& buildInstruments() override {
            strategy->addInstrument("SPY");
            strategy->addInstrument("QQQ");
            strategy->addInstrument("IWM");
            return *this;
        }
        TradingStrategyBuilder& buildRiskParameters() override {
            strategy->setRiskLimit(50000.0);
            return *this;
        }
        TradingStrategyBuilder& buildPositioning() override {
            strategy->setPositionSize(5000.0);
            strategy->setMaxPositions(3);
            return *this;
        }
        TradingStrategyBuilder& buildTradingStyle() override {
            strategy->setDayTrading(false);
            strategy->setSwingTrading(true);
            return *this;
        }
        TradingStrategyBuilder& buildRiskModel() override {
            strategy->setRiskModel("Expected Shortfall");
            return *this;
        }
    };

    class StrategyDirector {
    public:
        std::unique_ptr<TradingStrategy> buildStrategy(TradingStrategyBuilder& builder) {
            return builder.buildName()
                         .buildInstruments()
                         .buildRiskParameters()
                         .buildPositioning()
                         .buildTradingStyle()
                         .buildRiskModel()
                         .getResult();
        }
    };

    // -------------------------------------------------------------------------
    // 4. PROTOTYPE PATTERN - Trade Template Cloning
    // -------------------------------------------------------------------------
    class TradeTemplate {
    public:
        virtual ~TradeTemplate() = default;
        virtual std::unique_ptr<TradeTemplate> clone() const = 0;
        virtual void execute() = 0;
        virtual void setQuantity(int qty) = 0;
        virtual void setSymbol(const std::string& sym) = 0;
    };

    class EquityTrade : public TradeTemplate {
    private:
        std::string symbol;
        int quantity;
        OrderSide side;
        double price;
        std::string exchange;

    public:
        EquityTrade(const std::string& symbol, int quantity, OrderSide side,
                   double price, const std::string& exchange)
            : symbol(symbol), quantity(quantity), side(side), price(price), exchange(exchange) {}

        std::unique_ptr<TradeTemplate> clone() const override {
            return std::make_unique<EquityTrade>(*this);
        }

        void execute() override {
            std::cout << "EQUITY TRADE: " << (side == OrderSide::BUY ? "BUY" : "SELL")
                      << " " << quantity << " shares of " << symbol
                      << " at $" << std::fixed << std::setprecision(2) << price
                      << " on " << exchange << std::endl;
        }

        void setQuantity(int qty) override { quantity = qty; }
        void setSymbol(const std::string& sym) override { symbol = sym; }
    };

    class OptionTrade : public TradeTemplate {
    private:
        std::string underlying;
        std::string expiry;
        double strike;
        char optionType; // 'C' for Call, 'P' for Put
        int contracts;
        OrderSide side;
        double premium;

    public:
        OptionTrade(const std::string& underlying, const std::string& expiry,
                   double strike, char optionType, int contracts, OrderSide side, double premium)
            : underlying(underlying), expiry(expiry), strike(strike), optionType(optionType),
              contracts(contracts), side(side), premium(premium) {}

        std::unique_ptr<TradeTemplate> clone() const override {
            return std::make_unique<OptionTrade>(*this);
        }

        void execute() override {
            std::cout << "OPTION TRADE: " << (side == OrderSide::BUY ? "BUY" : "SELL")
                      << " " << contracts << " contracts of " << underlying
                      << " " << expiry << " " << strike << (optionType == 'C' ? "C" : "P")
                      << " at $" << std::fixed << std::setprecision(2) << premium << " premium" << std::endl;
        }

        void setQuantity(int qty) override { contracts = qty; }
        void setSymbol(const std::string& sym) override { underlying = sym; }
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
        auto stopOrder = OrderFactory::createOrder(OrderType::STOP, "TSLA", 200, OrderSide::SELL, 240.0);

        std::cout << "Created orders using factory:\n";
        marketOrder->execute();
        limitOrder->execute();
        stopOrder->execute();

        std::cout << "\nCommission calculations:\n";
        std::cout << "Market Order Commission: $" << std::fixed << std::setprecision(2)
                  << marketOrder->calculateCommission() << std::endl;
        std::cout << "Limit Order Commission: $" << std::fixed << std::setprecision(2)
                  << limitOrder->calculateCommission() << std::endl;

        // Builder Pattern - Trading Strategy
        std::cout << "\n--- BUILDER PATTERN - Trading Strategy ---\n";
        StrategyDirector director;

        MomentumStrategyBuilder momentumBuilder;
        auto momentumStrategy = director.buildStrategy(momentumBuilder);
        std::cout << "Momentum Strategy:\n";
        momentumStrategy->displayStrategy();

        MeanReversionStrategyBuilder meanRevBuilder;
        auto meanRevStrategy = director.buildStrategy(meanRevBuilder);
        std::cout << "Mean Reversion Strategy:\n";
        meanRevStrategy->displayStrategy();

        // Prototype Pattern - Trade Templates
        std::cout << "\n--- PROTOTYPE PATTERN - Trade Templates ---\n";
        auto equityTemplate = std::make_unique<EquityTrade>("AAPL", 100, OrderSide::BUY, 175.50, "NASDAQ");
        auto optionTemplate = std::make_unique<OptionTrade>("AAPL", "2024-01-19", 180.0, 'C', 10, OrderSide::BUY, 5.50);

        std::cout << "Original trade templates:\n";
        equityTemplate->execute();
        optionTemplate->execute();

        // Clone and modify templates
        auto clonedEquity = equityTemplate->clone();
        auto clonedOption = optionTemplate->clone();

        clonedEquity->setSymbol("GOOGL");
        clonedEquity->setQuantity(50);
        clonedOption->setSymbol("GOOGL");
        clonedOption->setQuantity(5);

        std::cout << "\nCloned and modified trade templates:\n";
        clonedEquity->execute();
        clonedOption->execute();
    }
}

// =============================================================================
// STRUCTURAL DESIGN PATTERNS - CAPITAL MARKETS TRADING EXAMPLES
// =============================================================================

namespace structural_patterns {

    // -------------------------------------------------------------------------
    // 1. ADAPTER PATTERN - Legacy Trading System Integration
    // -------------------------------------------------------------------------
    // Legacy FIX protocol interface
    class LegacyFixProtocol {
    public:
        void sendFixMessage(const std::string& fixMessage) {
            std::cout << "[LEGACY FIX] Sending: " << fixMessage << std::endl;
        }

        std::string receiveFixMessage() {
            return "8=FIX.4.2|35=D|49=SENDER|56=TARGET|52=20241024-10:30:00|";
        }
    };

    // Modern JSON-based trading interface
    class ModernTradingInterface {
    public:
        virtual ~ModernTradingInterface() = default;
        virtual void sendOrder(const std::string& jsonOrder) = 0;
        virtual std::string getOrderStatus(const std::string& orderId) = 0;
    };

    // Adapter to integrate legacy FIX system with modern JSON interface
    class FixToJsonAdapter : public ModernTradingInterface {
    private:
        std::unique_ptr<LegacyFixProtocol> fixProtocol;

        std::string convertJsonToFix(const std::string& jsonOrder) {
            // Simplified conversion - in reality this would parse JSON and create FIX
            return "8=FIX.4.2|35=D|55=AAPL|54=1|38=100|40=2|44=175.50|";
        }

        std::string convertFixToJson(const std::string& fixMessage) {
            // Simplified conversion - in reality this would parse FIX and create JSON
            return R"({"msgType":"ExecutionReport","symbol":"AAPL","side":"BUY","status":"FILLED"})";
        }

    public:
        FixToJsonAdapter() : fixProtocol(std::make_unique<LegacyFixProtocol>()) {}

        void sendOrder(const std::string& jsonOrder) override {
            std::cout << "Adapter converting JSON to FIX format\n";
            std::string fixMessage = convertJsonToFix(jsonOrder);
            fixProtocol->sendFixMessage(fixMessage);
        }

        std::string getOrderStatus(const std::string& orderId) override {
            std::string fixResponse = fixProtocol->receiveFixMessage();
            std::cout << "Adapter converting FIX response to JSON\n";
            return convertFixToJson(fixResponse);
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

    class MarginDecorator : public OrderDecorator {
    private:
        double marginRequirement;

    public:
        MarginDecorator(std::unique_ptr<BaseOrder> order, double margin = 0.5)
            : OrderDecorator(std::move(order)), marginRequirement(margin) {}

        std::string getOrderDetails() const override {
            return order->getOrderDetails() + " + Margin(" + std::to_string(marginRequirement * 100) + "%)";
        }

        double calculateTotalCost() const override {
            return order->calculateTotalCost() * marginRequirement; // Only need margin amount
        }
    };

    class RiskCheckDecorator : public OrderDecorator {
    private:
        double riskLimit;

    public:
        RiskCheckDecorator(std::unique_ptr<BaseOrder> order, double limit = 50000.0)
            : OrderDecorator(std::move(order)), riskLimit(limit) {}

        std::string getOrderDetails() const override {
            double cost = order->calculateTotalCost();

        double calculateTotalCost() const override {
            return order->calculateTotalCost();
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

        void routeOrder(const std::string& order) {
            std::cout << "OMS: Routing order to exchange" << std::endl;
        }
    };

    class RiskManager {
    public:
        bool checkRiskLimits(double orderValue) {
            std::cout << "Risk Manager: Checking position limits for $"
                      << std::fixed << std::setprecision(2) << orderValue << std::endl;
            return orderValue < 100000.0; // Simple risk check
        }

        void updateExposure(const std::string& symbol, double exposure) {
            std::cout << "Risk Manager: Updating " << symbol << " exposure: $"
                      << std::fixed << std::setprecision(2) << exposure << std::endl;
        }
    };

    class PortfolioManager {
    public:
        void updatePosition(const std::string& symbol, int quantity) {
            std::cout << "Portfolio Manager: Updating position - " << symbol
                      << " quantity: " << quantity << std::endl;
        }

        double getCurrentPnL() {
            std::cout << "Portfolio Manager: Calculating current P&L" << std::endl;
            return 15750.50; // Mock P&L
        }
    };

    class ComplianceEngine {
    public:
        bool checkCompliance(const std::string& order) {
            std::cout << "Compliance: Checking regulatory compliance" << std::endl;
            return true; // Mock compliance check
        }
    };

    // Facade that simplifies the trading system complexity
    class TradingSystemFacade {
    private:
        OrderManagementSystem oms;
        RiskManager riskManager;
        PortfolioManager portfolioManager;
        ComplianceEngine compliance;

    public:
        bool executeTradeWorkflow(const std::string& symbol, int quantity, double price) {
            std::cout << "Trading System: Executing complete trade workflow...\n";

            std::string orderDetails = symbol + " " + std::to_string(quantity) + "@" + std::to_string(price);
            double orderValue = quantity * price;

            // Step 1: Compliance check
            if (!compliance.checkCompliance(orderDetails)) {
                std::cout << "Trade rejected: Compliance failure\n";
                return false;
            }

            // Step 2: Risk check
            if (!riskManager.checkRiskLimits(orderValue)) {
                std::cout << "Trade rejected: Risk limit breach\n";
                return false;
            }

            // Step 3: Order validation and routing
            oms.validateOrder(orderDetails);
            oms.routeOrder(orderDetails);

            // Step 4: Update portfolio and risk
            portfolioManager.updatePosition(symbol, quantity);
            riskManager.updateExposure(symbol, orderValue);

            std::cout << "Trade executed successfully! Current P&L: $"
                      << std::fixed << std::setprecision(2) << portfolioManager.getCurrentPnL() << "\n";
            return true;
        }
    };

    // -------------------------------------------------------------------------
    // 4. PROXY PATTERN - Market Data Proxy with Caching
    // -------------------------------------------------------------------------
    class MarketDataFeed {
    public:
        virtual ~MarketDataFeed() = default;
        virtual double getPrice(const std::string& symbol) = 0;
        virtual std::vector<double> getHistoricalPrices(const std::string& symbol, int days) = 0;
    };

    class RealMarketDataFeed : public MarketDataFeed {
    public:
        double getPrice(const std::string& symbol) override {
            std::cout << "Real Feed: Fetching live price for " << symbol << " from exchange" << std::endl;
            // Simulate network delay
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Mock price data
            if (symbol == "AAPL") return 175.50;
            if (symbol == "GOOGL") return 2800.75;
            if (symbol == "TSLA") return 245.30;
            return 100.0;
        }

        std::vector<double> getHistoricalPrices(const std::string& symbol, int days) override {
            std::cout << "Real Feed: Fetching " << days << " days of historical data for "
                      << symbol << std::endl;
            // Simulate expensive database query
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::vector<double> prices;
            double basePrice = getPrice(symbol);
            for (int i = 0; i < days; ++i) {
                prices.push_back(basePrice + (i * 0.5) - (days * 0.25));
            }
            return prices;
        }
    };

    class CachedMarketDataProxy : public MarketDataFeed {
    private:
        mutable std::unique_ptr<RealMarketDataFeed> realFeed;
        mutable std::unordered_map<std::string, double> priceCache;
        mutable std::unordered_map<std::string, std::vector<double>> historicalCache;
        mutable std::chrono::time_point<std::chrono::steady_clock> lastUpdate;

    public:
        CachedMarketDataProxy() : lastUpdate(std::chrono::steady_clock::now()) {}

        double getPrice(const std::string& symbol) override {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate);

            // Cache valid for 5 seconds
            if (elapsed.count() > 5 || priceCache.find(symbol) == priceCache.end()) {
                if (!realFeed) {
                    std::cout << "Proxy: Creating real market data feed connection\n";
                    realFeed = std::make_unique<RealMarketDataFeed>();
                }
                std::cout << "Proxy: Cache miss - fetching fresh data\n";
                priceCache[symbol] = realFeed->getPrice(symbol);
                lastUpdate = now;
            } else {
                std::cout << "Proxy: Cache hit - returning cached price\n";
            }

            return priceCache[symbol];
        }

        std::vector<double> getHistoricalPrices(const std::string& symbol, int days) override {
            std::string cacheKey = symbol + "_" + std::to_string(days);

            if (historicalCache.find(cacheKey) == historicalCache.end()) {
                if (!realFeed) {
                    std::cout << "Proxy: Creating real market data feed connection\n";
                    realFeed = std::make_unique<RealMarketDataFeed>();
                }
                std::cout << "Proxy: Historical cache miss - fetching data\n";
                historicalCache[cacheKey] = realFeed->getHistoricalPrices(symbol, days);
            } else {
                std::cout << "Proxy: Historical cache hit - returning cached data\n";
            }

            return historicalCache[cacheKey];
        }
    };

    // -------------------------------------------------------------------------
    // 5. COMPOSITE PATTERN - Portfolio Hierarchy
    // -------------------------------------------------------------------------
    class PortfolioComponent {
    public:
        virtual ~PortfolioComponent() = default;
        virtual void displayDetails(int indent = 0) const = 0;
        virtual double getTotalValue() const = 0;
        virtual double getTotalPnL() const = 0;
    };

    class Position : public PortfolioComponent {
    private:
        std::string symbol;
        int quantity;
        double avgPrice;
        double currentPrice;

    public:
        File(const std::string& name, size_t size) : name(name), size(size) {}

        void showDetails(int indent = 0) const override {
            std::string indentation(indent, ' ');
            std::cout << indentation << "File: " << name << " (" << size << " bytes)\n";
        }

        size_t getSize() const override {
            return size;
        }
    };

    class Directory : public FileSystemComponent {
    private:
        std::string name;
        std::vector<std::unique_ptr<FileSystemComponent>> children;

    public:
        Directory(const std::string& name) : name(name) {}

        void add(std::unique_ptr<FileSystemComponent> component) {
    // -------------------------------------------------------------------------
    // 4. STATE PATTERN - Order State Management
    // -------------------------------------------------------------------------
    class OrderState {
    public:
        virtual ~OrderState() = default;
        virtual void handle() = 0;
        virtual std::string getStateName() const = 0;
    };

    class TradingOrder {
    private:
        std::unique_ptr<OrderState> currentState;
        std::string symbol;
        int quantity;
        double price;

    public:
        TradingOrder(const std::string& sym, int qty, double prc)
            : symbol(sym), quantity(qty), price(prc) {}

        void setState(std::unique_ptr<OrderState> state) {
            currentState = std::move(state);
        }

        void processOrder() {
            if (currentState) {
                std::cout << "Order " << symbol << " (" << quantity << "@" << price
                          << ") - Current state: " << currentState->getStateName() << std::endl;
                currentState->handle();
            }
        }

        std::string getOrderDetails() const {
            return symbol + " " + std::to_string(quantity) + "@" + std::to_string(price);
        }
    };

    // Forward declarations
    class PendingState;
    class ValidatedState;
    class ExecutedState;

    class PendingState : public OrderState {
    private:
        TradingOrder* order;

    public:
        PendingState(TradingOrder* ord) : order(ord) {}

        void handle() override;
        std::string getStateName() const override {
            return "PENDING";
        }
    };

    class ValidatedState : public OrderState {
    private:
        TradingOrder* order;

    public:
        ValidatedState(TradingOrder* ord) : order(ord) {}

        void handle() override;
        std::string getStateName() const override {
            return "VALIDATED";
        }
    };

    class ExecutedState : public OrderState {
    private:
        TradingOrder* order;

    public:
        ExecutedState(TradingOrder* ord) : order(ord) {}

        void handle() override {
            std::cout << "Order execution complete. Final state reached." << std::endl;
        }

        std::string getStateName() const override {
            return "EXECUTED";
        }
    };

    // Implementation of handle methods
    void PendingState::handle() {
        std::cout << "Processing pending order -> Moving to VALIDATED" << std::endl;
        order->setState(std::make_unique<ValidatedState>(order));
    }

    void ValidatedState::handle() {
        std::cout << "Risk checks passed -> Moving to EXECUTED" << std::endl;
        order->setState(std::make_unique<ExecutedState>(order));
    }

    void demonstrateStructuralPatterns() {
        std::cout << "\n=============== STRUCTURAL PATTERNS - TRADING EXAMPLES ===============\n";

        // Adapter Pattern - Legacy FIX Integration
        std::cout << "\n--- ADAPTER PATTERN - Legacy FIX Integration ---\n";
        auto modernInterface = std::make_unique<FixToJsonAdapter>();
        std::string jsonOrder = R"({"symbol":"AAPL","side":"BUY","quantity":100,"price":175.50})";
        modernInterface->sendOrder(jsonOrder);
        std::string status = modernInterface->getOrderStatus("ORDER123");
        std::cout << "Order Status: " << status << std::endl;

        // Decorator Pattern - Order Enhancement
        std::cout << "\n--- DECORATOR PATTERN - Order Enhancement ---\n";
        auto basicOrder = std::make_unique<SimpleOrder>("AAPL", 1000, 175.50);
        std::cout << basicOrder->getOrderDetails() << " | Cost: $"
                  << std::fixed << std::setprecision(2) << basicOrder->calculateTotalCost() << std::endl;

        auto orderWithCommission = std::make_unique<CommissionDecorator>(std::move(basicOrder));
        std::cout << orderWithCommission->getOrderDetails() << " | Cost: $"
                  << std::fixed << std::setprecision(2) << orderWithCommission->calculateTotalCost() << std::endl;

        auto marginOrder = std::make_unique<MarginDecorator>(std::move(orderWithCommission));
        std::cout << marginOrder->getOrderDetails() << " | Margin Required: $"
                  << std::fixed << std::setprecision(2) << marginOrder->calculateTotalCost() << std::endl;

        auto riskCheckedOrder = std::make_unique<RiskCheckDecorator>(std::move(marginOrder), 100000.0);
        std::cout << riskCheckedOrder->getOrderDetails() << std::endl;

        // Facade Pattern - Trading System
        std::cout << "\n--- FACADE PATTERN - Trading System ---\n";
        TradingSystemFacade tradingSystem;
        tradingSystem.executeTradeWorkflow("AAPL", 500, 175.50);
        std::cout << std::endl;
        tradingSystem.executeTradeWorkflow("TSLA", 2000, 245.30); // This should trigger risk limit

        // Proxy Pattern - Market Data Caching
        std::cout << "\n--- PROXY PATTERN - Market Data Caching ---\n";
        auto marketData = std::make_unique<CachedMarketDataProxy>();

        std::cout << "\nFirst access to market data:\n";
        std::cout << "AAPL Price: $" << std::fixed << std::setprecision(2)
                  << marketData->getPrice("AAPL") << std::endl;

        std::cout << "\nSecond access (should use cache):\n";
        std::cout << "AAPL Price: $" << std::fixed << std::setprecision(2)
                  << marketData->getPrice("AAPL") << std::endl;

        std::cout << "\nHistorical data access:\n";
        auto historical = marketData->getHistoricalPrices("AAPL", 5);
        std::cout << "5-day historical prices: ";
        for (double price : historical) {
            std::cout << "$" << std::fixed << std::setprecision(2) << price << " ";
        }
        std::cout << std::endl;

        // Composite Pattern - Portfolio Structure
        std::cout << "\n--- COMPOSITE PATTERN - Portfolio Structure ---\n";
        auto masterPortfolio = std::make_unique<Portfolio>("Master Portfolio");

        auto equityPortfolio = std::make_unique<Portfolio>("Equity Portfolio");
        equityPortfolio->add(std::make_unique<Position>("AAPL", 100, 170.00, 175.50));
        equityPortfolio->add(std::make_unique<Position>("GOOGL", 25, 2750.00, 2800.75));

        auto techPortfolio = std::make_unique<Portfolio>("Tech Portfolio");
        techPortfolio->add(std::make_unique<Position>("TSLA", 50, 250.00, 245.30));
        techPortfolio->add(std::make_unique<Position>("NVDA", 75, 420.00, 445.20));

        masterPortfolio->add(std::move(equityPortfolio));
        masterPortfolio->add(std::move(techPortfolio));
        masterPortfolio->add(std::make_unique<Position>("SPY", 200, 410.00, 415.50));

        masterPortfolio->displayDetails();
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
                std::cout << " -> SIGNAL: Consider BUYING (price drop > " << std::abs(buyThreshold) << ")" << std::endl;
            } else if (change > sellThreshold) {
                std::cout << " -> SIGNAL: Consider SELLING (price rise > " << sellThreshold << ")" << std::endl;
            } else {
                std::cout << " -> SIGNAL: HOLD (within thresholds)" << std::endl;
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

    class VWAPStrategy : public ExecutionStrategy {
    public:
        void execute(const std::string& symbol, int quantity, double targetPrice) override {
            std::cout << "VWAP Execution: Distributing " << quantity << " shares of " << symbol
                      << " based on historical volume patterns" << std::endl;

            std::vector<double> volumeProfile = {5.0, 8.0, 12.0, 15.0, 18.0, 20.0, 15.0, 7.0};
            for (size_t i = 0; i < volumeProfile.size(); ++i) {
                int sliceQuantity = static_cast<int>(quantity * (volumeProfile[i] / 100.0));
                std::cout << "  Hour " << (i + 1) << ": Execute " << sliceQuantity
                          << " shares (" << volumeProfile[i] << "% of volume)" << std::endl;
            }
        }

        std::string getStrategyName() const override {
            return "Volume-Weighted Average Price (VWAP)";
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

    // -------------------------------------------------------------------------
    // 3. COMMAND PATTERN - Trading Commands
    // -------------------------------------------------------------------------
    class TradingAccount {
    private:
        std::unordered_map<std::string, int> positions;
        double cashBalance;

    public:
        TradingAccount(double initialCash) : cashBalance(initialCash) {}

        void buyShares(const std::string& symbol, int quantity, double price) {
            double cost = quantity * price;
            if (cashBalance >= cost) {
                positions[symbol] += quantity;
                cashBalance -= cost;
                std::cout << "BOUGHT: " << quantity << " shares of " << symbol
                          << " at $" << std::fixed << std::setprecision(2) << price
                          << " | Cash: $" << cashBalance << std::endl;
            } else {
                std::cout << "INSUFFICIENT FUNDS for purchase" << std::endl;
            }
        }

        void sellShares(const std::string& symbol, int quantity, double price) {
            if (positions[symbol] >= quantity) {
                positions[symbol] -= quantity;
                cashBalance += quantity * price;
                std::cout << "SOLD: " << quantity << " shares of " << symbol
                          << " at $" << std::fixed << std::setprecision(2) << price
                          << " | Cash: $" << cashBalance << std::endl;
            } else {
                std::cout << "INSUFFICIENT SHARES to sell" << std::endl;
            }
        }
    };

    void demonstrateBehavioralPatterns() {
        std::cout << "\n=============== BEHAVIORAL PATTERNS - TRADING EXAMPLES ===============\n";

        // Observer Pattern - Price Alert System
        std::cout << "\n--- OBSERVER PATTERN - Price Alert System ---\n";
        MarketDataStream marketStream;
        TradingAlgorithm momentumAlgo("MOMENTUM", -2.0, 3.0);
        TradingAlgorithm meanRevAlgo("MEAN_REV", -1.0, 1.5);

        marketStream.subscribe(&momentumAlgo);
        marketStream.subscribe(&meanRevAlgo);

        // Initialize prices and trigger updates
        marketStream.updatePrice("AAPL", 175.00);
        marketStream.updatePrice("AAPL", 172.50); // -2.50 change
        marketStream.updatePrice("AAPL", 178.00); // +5.50 change

        // Strategy Pattern - Order Execution
        std::cout << "\n--- STRATEGY PATTERN - Order Execution Strategies ---\n";
        OrderExecutionContext executor;

        std::cout << "\nLarge order execution:\n";
        executor.setStrategy(std::make_unique<TWAPStrategy>(8));
        executor.executeOrder("AAPL", 10000, 175.50);

        std::cout << "\nInstitutional order execution:\n";
        executor.setStrategy(std::make_unique<VWAPStrategy>());
        executor.executeOrder("GOOGL", 5000, 2800.00);

        // Command Pattern - Trading Account
        std::cout << "\n--- COMMAND PATTERN - Trading Account ---\n";
        TradingAccount account(100000.0);
        account.buyShares("AAPL", 100, 175.50);
        account.sellShares("AAPL", 50, 178.00);

        // State Pattern - Order State Management
        std::cout << "\n--- STATE PATTERN - Order State Management ---\n";
        structural_patterns::TradingOrder order("AAPL", 500, 175.50);
        order.setState(std::make_unique<structural_patterns::PendingState>(&order));

        order.processOrder(); // Pending -> Validated
        order.processOrder(); // Validated -> Executed
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
    std::cout << "  • Singleton: Market Data Manager - Single instance for global price feeds\n";
    std::cout << "  • Factory Method: Order Factory - Creates different order types (Market, Limit, Stop)\n";
    std::cout << "  • Builder: Trading Strategy Builder - Constructs complex trading strategies\n";
    std::cout << "  • Prototype: Trade Template Cloning - Clones and modifies trade templates\n\n";

    std::cout << "STRUCTURAL PATTERNS:\n";
    std::cout << "  • Adapter: Legacy FIX Protocol Integration - Bridges old and new systems\n";
    std::cout << "  • Decorator: Order Enhancement - Adds commission, margin, risk checks\n";
    std::cout << "  • Facade: Trading System Facade - Simplifies complex trading workflow\n";
    std::cout << "  • Proxy: Market Data Caching - Provides cached access to expensive data\n";
    std::cout << "  • Composite: Portfolio Hierarchy - Manages nested portfolio structures\n\n";

    std::cout << "BEHAVIORAL PATTERNS:\n";
    std::cout << "  • Observer: Price Alert System - Notifies algorithms of price changes\n";
    std::cout << "  • Strategy: Order Execution Strategies - TWAP, VWAP, Aggressive execution\n";
    std::cout << "  • Command: Trading Commands - Encapsulates buy/sell operations\n";
    std::cout << "  • State: Order State Management - Manages order lifecycle states\n";
    std::cout << "\nAll patterns demonstrated with realistic capital markets use cases!\n";
    std::cout << "=============================================================================\n";

    return 0;
}

        auto techPortfolio = std::make_unique<Portfolio>("Tech Portfolio");
        techPortfolio->add(std::make_unique<Position>("TSLA", 50, 250.00, 245.30));
        techPortfolio->add(std::make_unique<Position>("NVDA", 75, 420.00, 445.20));

        masterPortfolio->add(std::move(equityPortfolio));
        masterPortfolio->add(std::move(techPortfolio));
        masterPortfolio->add(std::make_unique<Position>("SPY", 200, 410.00, 415.50));

        masterPortfolio->displayDetails();
    }
}

// =============================================================================
// BEHAVIORAL DESIGN PATTERNS - CAPITAL MARKETS TRADING EXAMPLES
// =============================================================================

namespace behavioral_patterns {

    void demonstrateBehavioralPatterns() {
        std::cout << "\n=============== BEHAVIORAL PATTERNS - TRADING EXAMPLES ===============\n";

        // State Pattern - Order State Management
        std::cout << "\n--- STATE PATTERN - Order State Management ---\n";
        structural_patterns::TradingOrder order("AAPL", 500, 175.50);
        order.setState(std::make_unique<structural_patterns::PendingState>(&order));

        order.processOrder(); // Pending -> Validated
        order.processOrder(); // Validated -> Executed

        std::cout << "\nPattern demonstrations completed successfully!\n";
    }

    // -------------------------------------------------------------------------
    // 2. STRATEGY PATTERN
    // -------------------------------------------------------------------------
    class SortingStrategy {
    public:
        virtual ~SortingStrategy() = default;
        virtual void sort(std::vector<int>& data) = 0;
        virtual std::string getName() const = 0;
    };

    class BubbleSort : public SortingStrategy {
    public:
        void sort(std::vector<int>& data) override {
            std::cout << "Performing Bubble Sort...\n";
            // Simplified bubble sort
            for (size_t i = 0; i < data.size(); ++i) {
                for (size_t j = 0; j < data.size() - 1 - i; ++j) {
                    if (data[j] > data[j + 1]) {
                        std::swap(data[j], data[j + 1]);
                    }
                }
            }
        }

        std::string getName() const override {
            return "Bubble Sort";
        }
    };

    class QuickSort : public SortingStrategy {
    public:
        void sort(std::vector<int>& data) override {
            std::cout << "Performing Quick Sort...\n";
            std::sort(data.begin(), data.end());
        }

        std::string getName() const override {
            return "Quick Sort";
        }
    };

    class SortContext {
    private:
        std::unique_ptr<SortingStrategy> strategy;

    public:
        void setStrategy(std::unique_ptr<SortingStrategy> strategy) {
            this->strategy = std::move(strategy);
        }

        void executeSort(std::vector<int>& data) {
            if (strategy) {
                std::cout << "Using " << strategy->getName() << std::endl;
                strategy->sort(data);
            }
        }
    };

    // -------------------------------------------------------------------------
    // 3. COMMAND PATTERN
    // -------------------------------------------------------------------------
    class Command {
    public:
        virtual ~Command() = default;
        virtual void execute() = 0;
        virtual void undo() = 0;
    };

    class Light {
    private:
        bool isOn = false;

    public:
        void turnOn() {
            isOn = true;
            std::cout << "Light is ON" << std::endl;
        }

        void turnOff() {
            isOn = false;
            std::cout << "Light is OFF" << std::endl;
        }

        bool getState() const {
            return isOn;
        }
    };

    class LightOnCommand : public Command {
    private:
        Light& light;

    public:
        LightOnCommand(Light& light) : light(light) {}

        void execute() override {
            light.turnOn();
        }

        void undo() override {
            light.turnOff();
        }
    };

    class LightOffCommand : public Command {
    private:
        Light& light;

    public:
        LightOffCommand(Light& light) : light(light) {}

        void execute() override {
            light.turnOff();
        }

        void undo() override {
            light.turnOn();
        }
    };

    class RemoteControl {
    private:
        std::unique_ptr<Command> lastCommand;

    public:
        void setCommand(std::unique_ptr<Command> command) {
            command->execute();
            lastCommand = std::move(command);
        }

        void pressUndo() {
            if (lastCommand) {
                std::cout << "Undoing last command...\n";
                lastCommand->undo();
            }
        }
    };

    // -------------------------------------------------------------------------
    // 4. STATE PATTERN
    // -------------------------------------------------------------------------
    class State {
    public:
        virtual ~State() = default;
        virtual void handle() = 0;
        virtual std::string getName() const = 0;
    };

    class TrafficLight {
    private:
        std::unique_ptr<State> currentState;

    public:
        void setState(std::unique_ptr<State> state) {
            currentState = std::move(state);
        }

        void request() {
            if (currentState) {
                std::cout << "Current state: " << currentState->getName() << std::endl;
                currentState->handle();
            }
        }

        State* getState() const {
            return currentState.get();
        }
    };

    // Forward declarations
    class RedState;
    class GreenState;
    class YellowState;

    class RedState : public State {
    private:
        TrafficLight* trafficLight;

    public:
        RedState(TrafficLight* light) : trafficLight(light) {}

        void handle() override;
        std::string getName() const override {
            return "Red";
        }
    };

    class GreenState : public State {
    private:
        TrafficLight* trafficLight;

    public:
        GreenState(TrafficLight* light) : trafficLight(light) {}

        void handle() override;
        std::string getName() const override {
            return "Green";
        }
    };

    class YellowState : public State {
    private:
        TrafficLight* trafficLight;

    public:
        YellowState(TrafficLight* light) : trafficLight(light) {}

        void handle() override;
        std::string getName() const override {
            return "Yellow";
        }
    };

    // Implementation of handle methods after all classes are declared
    void RedState::handle() {
        std::cout << "Red light: STOP! Changing to Green..." << std::endl;
        trafficLight->setState(std::make_unique<GreenState>(trafficLight));
    }

    void GreenState::handle() {
        std::cout << "Green light: GO! Changing to Yellow..." << std::endl;
        trafficLight->setState(std::make_unique<YellowState>(trafficLight));
    }

    void YellowState::handle() {
        std::cout << "Yellow light: CAUTION! Changing to Red..." << std::endl;
        trafficLight->setState(std::make_unique<RedState>(trafficLight));
    }

    // -------------------------------------------------------------------------
    // 5. TEMPLATE METHOD PATTERN
    // -------------------------------------------------------------------------
    class DataProcessor {
    public:
        virtual ~DataProcessor() = default;

        // Template method
        void process() {
            readData();
            processData();
            writeData();
        }

    protected:
        virtual void readData() = 0;
        virtual void processData() = 0;
        virtual void writeData() = 0;
    };

    class CSVProcessor : public DataProcessor {
    protected:
        void readData() override {
            std::cout << "Reading data from CSV file..." << std::endl;
        }

        void processData() override {
            std::cout << "Processing CSV data (parsing columns)..." << std::endl;
        }

        void writeData() override {
            std::cout << "Writing processed data to CSV file..." << std::endl;
        }
    };

    class JSONProcessor : public DataProcessor {
    protected:
        void readData() override {
            std::cout << "Reading data from JSON file..." << std::endl;
        }

        void processData() override {
            std::cout << "Processing JSON data (parsing objects)..." << std::endl;
        }

        void writeData() override {
            std::cout << "Writing processed data to JSON file..." << std::endl;
        }
    };

    // -------------------------------------------------------------------------
    // 6. CHAIN OF RESPONSIBILITY PATTERN
    // -------------------------------------------------------------------------
    class Handler {
    protected:
        std::unique_ptr<Handler> nextHandler;

    public:
        virtual ~Handler() = default;

        void setNext(std::unique_ptr<Handler> handler) {
            nextHandler = std::move(handler);
        }

        virtual void handleRequest(const std::string& request) {
            if (nextHandler) {
                nextHandler->handleRequest(request);
            } else {
                std::cout << "No handler could process: " << request << std::endl;
            }
        }
    };

    class TechnicalSupportHandler : public Handler {
    public:
        void handleRequest(const std::string& request) override {
            if (request.find("technical") != std::string::npos) {
                std::cout << "Technical Support handled: " << request << std::endl;
            } else {
                Handler::handleRequest(request);
            }
        }
    };

    class BillingSupportHandler : public Handler {
    public:
        void handleRequest(const std::string& request) override {
            if (request.find("billing") != std::string::npos) {
                std::cout << "Billing Support handled: " << request << std::endl;
            } else {
                Handler::handleRequest(request);
            }
        }
    };

    class GeneralSupportHandler : public Handler {
    public:
        void handleRequest(const std::string& request) override {
            if (request.find("general") != std::string::npos) {
                std::cout << "General Support handled: " << request << std::endl;
            } else {
                Handler::handleRequest(request);
            }
        }
    };

    void demonstrateBehavioralPatterns() {
        std::cout << "\n=============== BEHAVIORAL PATTERNS ===============\n";

        // Observer Pattern
        std::cout << "\n--- OBSERVER PATTERN ---\n";
        NewsAgency agency;
        NewsChannel cnn("CNN");
        NewsChannel bbc("BBC");
        NewsChannel fox("FOX");

        agency.attach(&cnn);
        agency.attach(&bbc);
        agency.attach(&fox);

        agency.setNews("Breaking: New design pattern discovered!");
        agency.setNews("Technology: C++ gets new features!");

        // Strategy Pattern
        std::cout << "\n--- STRATEGY PATTERN ---\n";
        std::vector<int> data1 = {64, 34, 25, 12, 22, 11, 90};
        std::vector<int> data2 = {64, 34, 25, 12, 22, 11, 90};

        SortContext context;

        std::cout << "Original data: ";
        for (int num : data1) std::cout << num << " ";
        std::cout << std::endl;

        context.setStrategy(std::make_unique<BubbleSort>());
        context.executeSort(data1);
        std::cout << "Sorted data: ";
        for (int num : data1) std::cout << num << " ";
        std::cout << std::endl;

        context.setStrategy(std::make_unique<QuickSort>());
        context.executeSort(data2);
        std::cout << "Sorted data: ";
        for (int num : data2) std::cout << num << " ";
        std::cout << std::endl;

        // Command Pattern
        std::cout << "\n--- COMMAND PATTERN ---\n";
        Light livingRoomLight;
        RemoteControl remote;

        auto lightOn = std::make_unique<LightOnCommand>(livingRoomLight);
        auto lightOff = std::make_unique<LightOffCommand>(livingRoomLight);

        remote.setCommand(std::move(lightOn));
        remote.pressUndo();

        remote.setCommand(std::move(lightOff));
        remote.pressUndo();

        // State Pattern
        std::cout << "\n--- STATE PATTERN ---\n";
        TrafficLight trafficLight;
        trafficLight.setState(std::make_unique<RedState>(&trafficLight));

        trafficLight.request(); // Red -> Green
        trafficLight.request(); // Green -> Yellow
        trafficLight.request(); // Yellow -> Red

        // Template Method Pattern
        std::cout << "\n--- TEMPLATE METHOD PATTERN ---\n";
        std::cout << "Processing CSV data:\n";
        CSVProcessor csvProcessor;
        csvProcessor.process();

        std::cout << "\nProcessing JSON data:\n";
        JSONProcessor jsonProcessor;
        jsonProcessor.process();

        // Chain of Responsibility Pattern
        std::cout << "\n--- CHAIN OF RESPONSIBILITY PATTERN ---\n";
        auto technical = std::make_unique<TechnicalSupportHandler>();
        auto billing = std::make_unique<BillingSupportHandler>();
        auto general = std::make_unique<GeneralSupportHandler>();

        technical->setNext(std::move(billing));
        technical->setNext(std::move(general)); // This will replace billing, showing chain modification

        technical->handleRequest("I have a technical issue with my software");
        technical->handleRequest("I have a billing question about my account");
        technical->handleRequest("I have a general inquiry");
        technical->handleRequest("I have an unknown issue type");
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL PATTERNS
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE DESIGN PATTERNS EXAMPLES IN C++\n";
    std::cout << "=============================================================================\n";

    creational_patterns::demonstrateCreationalPatterns();
    structural_patterns::demonstrateStructuralPatterns();
    behavioral_patterns::demonstrateBehavioralPatterns();

    std::cout << "\n=============================================================================\n";
    std::cout << "DESIGN PATTERNS SUMMARY:\n";
    std::cout << "=============================================================================\n";
    std::cout << "CREATIONAL PATTERNS:\n";
    std::cout << "  • Singleton: Ensures single instance with global access\n";
    std::cout << "  • Factory Method: Creates objects without specifying exact classes\n";
    std::cout << "  • Builder: Constructs complex objects step by step\n";
    std::cout << "  • Prototype: Creates objects by cloning existing instances\n\n";

    std::cout << "STRUCTURAL PATTERNS:\n";
    std::cout << "  • Adapter: Allows incompatible interfaces to work together\n";
    std::cout << "  • Decorator: Adds behavior to objects dynamically\n";
    std::cout << "  • Facade: Provides simplified interface to complex subsystem\n";
    std::cout << "  • Proxy: Provides placeholder/surrogate for expensive objects\n";
    std::cout << "  • Composite: Composes objects into tree structures\n\n";

    std::cout << "BEHAVIORAL PATTERNS:\n";
    std::cout << "  • Observer: Notifies multiple objects about state changes\n";
    std::cout << "  • Strategy: Encapsulates algorithms and makes them interchangeable\n";
    std::cout << "  • Command: Encapsulates requests as objects\n";
    std::cout << "  • State: Changes object behavior based on internal state\n";
    std::cout << "  • Template Method: Defines algorithm skeleton in base class\n";
    std::cout << "  • Chain of Responsibility: Passes requests along handler chain\n";
    std::cout << "=============================================================================\n";

    return 0;
}
