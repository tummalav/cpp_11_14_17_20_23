#include "hkex_omd_feed_handler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <random>
#include <set>

using namespace hkex::omd;

// Forward declaration for the factory function
std::unique_ptr<IOMDPlugin> createHKEXOMDPlugin();

// Example event handler implementation for market data
class MarketDataEventHandler : public IOMDEventHandler {
private:
    std::set<uint32_t> monitored_securities_;
    uint64_t order_count_;
    uint64_t trade_count_;
    uint64_t last_stats_time_;

public:
    MarketDataEventHandler() : order_count_(0), trade_count_(0), last_stats_time_(0) {
        // Monitor some popular HKEX securities
        monitored_securities_.insert(700);   // Tencent
        monitored_securities_.insert(5);     // HSBC
        monitored_securities_.insert(941);   // China Mobile
        monitored_securities_.insert(1299);  // AIA
        monitored_securities_.insert(2318);  // Ping An
        monitored_securities_.insert(3988);  // Bank of China
        monitored_securities_.insert(1398);  // ICBC
        monitored_securities_.insert(2628);  // China Life
    }

    void onAddOrder(const AddOrderMessage& msg) override {
        order_count_++;

        if (monitored_securities_.count(msg.header.security_code)) {
            std::cout << "ADD ORDER - Security: " << msg.header.security_code
                      << ", Price: " << (msg.price / 1000.0)
                      << ", Qty: " << msg.quantity
                      << ", Side: " << (msg.side == Side::BUY ? "BUY" : "SELL")
                      << ", OrderID: " << msg.order_id << std::endl;
        }

        printPeriodicStats();
    }

    void onModifyOrder(const ModifyOrderMessage& msg) override {
        if (monitored_securities_.count(msg.header.security_code)) {
            std::cout << "MODIFY ORDER - Security: " << msg.header.security_code
                      << ", New Price: " << (msg.new_price / 1000.0)
                      << ", New Qty: " << msg.new_quantity
                      << ", OrderID: " << msg.order_id << std::endl;
        }
    }

    void onDeleteOrder(const DeleteOrderMessage& msg) override {
        if (monitored_securities_.count(msg.header.security_code)) {
            std::cout << "DELETE ORDER - Security: " << msg.header.security_code
                      << ", OrderID: " << msg.order_id << std::endl;
        }
    }

    void onTrade(const TradeMessage& msg) override {
        trade_count_++;

        if (monitored_securities_.count(msg.header.security_code)) {
            std::cout << "TRADE - Security: " << msg.header.security_code
                      << ", Price: " << (msg.price / 1000.0)
                      << ", Qty: " << msg.quantity
                      << ", TradeID: " << msg.trade_id
                      << ", Type: " << (msg.trade_type == 1 ? "AUCTION" : "CONTINUOUS") << std::endl;
        }

        printPeriodicStats();
    }

    void onTradeCancel(const TradeCancelMessage& msg) override {
        if (monitored_securities_.count(msg.header.security_code)) {
            std::cout << "TRADE CANCEL - Security: " << msg.header.security_code
                      << ", TradeID: " << msg.trade_id
                      << ", Price: " << (msg.price / 1000.0)
                      << ", Qty: " << msg.quantity << std::endl;
        }
    }

    void onSecurityDefinition(const SecurityDefinitionMessage& msg) override {
        std::cout << "SECURITY DEFINITION - Code: " << msg.header.security_code
                  << ", Symbol: " << std::string(msg.symbol.data(), 12)
                  << ", Name: " << std::string(msg.name_eng.data(), 40)
                  << ", Type: " << static_cast<int>(msg.security_type)
                  << ", LotSize: " << msg.lot_size << std::endl;
    }

    void onSecurityStatus(const SecurityStatusMessage& msg) override {
        std::cout << "SECURITY STATUS - Code: " << msg.header.security_code
                  << ", Phase: " << static_cast<int>(msg.suspend_resume_reason) << std::endl;
    }

    void onStatistics(const StatisticsMessage& msg) override {
        if (monitored_securities_.count(msg.header.security_code)) {
            std::cout << "STATISTICS - Security: " << msg.header.security_code
                      << ", Volume: " << msg.shares_traded
                      << ", Turnover: " << (msg.turnover / 1000.0)
                      << ", High: " << (msg.high_price / 1000.0)
                      << ", Low: " << (msg.low_price / 1000.0)
                      << ", Last: " << (msg.last_price / 1000.0)
                      << ", VWAP: " << (msg.vwap / 1000.0) << std::endl;
        }
    }

    void onIndexData(const IndexDataMessage& msg) override {
        std::cout << "INDEX DATA - Code: " << std::string(msg.index_code.data(), 12)
                  << ", Value: " << (msg.index_value / 1000.0)
                  << ", Change: " << (msg.net_change / 1000.0)
                  << ", %Change: " << (msg.percentage_change / 100.0) << std::endl;
    }

    void onMarketTurnover(const MarketTurnoverMessage& msg) override {
        std::cout << "MARKET TURNOVER - Segment: " << static_cast<int>(msg.market_segment)
                  << ", Currency: " << std::string(msg.currency.data(), 4)
                  << ", Turnover: " << (msg.turnover / 1000000.0) << " M" << std::endl;
    }

    void onHeartbeat() override {
        static uint64_t heartbeat_count = 0;
        heartbeat_count++;

        if (heartbeat_count % 10 == 0) {
            std::cout << "Heartbeat received (" << heartbeat_count << ")" << std::endl;
        }
    }

    void onSequenceReset(uint32_t new_seq_num) override {
        std::cout << "SEQUENCE RESET - New sequence number: " << new_seq_num << std::endl;
    }

    void onDisconnect(const std::string& reason) override {
        std::cout << "DISCONNECTED: " << reason << std::endl;
    }

private:
    void printPeriodicStats() {
        uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        if (current_time - last_stats_time_ >= 30) {  // Every 30 seconds
            std::cout << "\n=== PERIODIC STATS ===" << std::endl;
            std::cout << "Orders processed: " << order_count_ << std::endl;
            std::cout << "Trades processed: " << trade_count_ << std::endl;
            std::cout << "======================\n" << std::endl;
            last_stats_time_ = current_time;
        }
    }
};

// Order book display utility
void displayOrderBook(const OrderBook* book) {
    if (!book) return;

    std::cout << "\n=== ORDER BOOK for Security " << book->security_code << " ===" << std::endl;

    // Display ask levels (highest to lowest)
    std::cout << "ASK LEVELS:" << std::endl;
    for (auto it = book->ask_levels.rbegin(); it != book->ask_levels.rend(); ++it) {
        std::cout << "  " << std::fixed << std::setprecision(3)
                  << (it->price / 1000.0) << " x " << it->quantity
                  << " (" << it->order_count << " orders)" << std::endl;
    }

    std::cout << "SPREAD" << std::endl;

    // Display bid levels (highest to lowest)
    std::cout << "BID LEVELS:" << std::endl;
    for (const auto& level : book->bid_levels) {
        std::cout << "  " << std::fixed << std::setprecision(3)
                  << (level.price / 1000.0) << " x " << level.quantity
                  << " (" << level.order_count << " orders)" << std::endl;
    }

    std::cout << "\nLAST TRADE: " << (book->last_trade_price / 1000.0)
              << " x " << book->last_trade_quantity << std::endl;
    std::cout << "TOTAL VOLUME: " << book->total_volume << std::endl;
    std::cout << "TOTAL TURNOVER: " << (book->total_turnover / 1000000.0) << " M" << std::endl;
    std::cout << "===============================================\n" << std::endl;
}

// Market data monitoring simulation
void marketDataMonitoring(IOMDPlugin* plugin) {
    std::cout << "\n=== Starting Market Data Monitoring ===" << std::endl;

    // Subscribe to popular securities
    std::vector<uint32_t> securities = {700, 5, 941, 1299, 2318, 3988, 1398, 2628, 1810, 883};

    for (uint32_t security : securities) {
        plugin->subscribe(security);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Monitor for a period
    std::this_thread::sleep_for(std::chrono::seconds(60));

    // Display order books
    std::cout << "\n=== ORDER BOOK SNAPSHOTS ===" << std::endl;
    for (uint32_t security : securities) {
        const OrderBook* book = plugin->getOrderBook(security);
        if (book) {
            displayOrderBook(book);
        }
    }
}

// Performance measurement
void performanceTest(IOMDPlugin* plugin) {
    std::cout << "\n=== Performance Test ===" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t initial_messages = plugin->getMessagesReceived();

    // Subscribe to all securities for maximum throughput
    plugin->subscribeAll();

    // Run for 30 seconds
    std::this_thread::sleep_for(std::chrono::seconds(30));

    auto end_time = std::chrono::high_resolution_clock::now();
    uint64_t final_messages = plugin->getMessagesReceived();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double elapsed_seconds = duration.count() / 1000000.0;
    uint64_t messages_processed = final_messages - initial_messages;

    std::cout << "Performance Results:" << std::endl;
    std::cout << "- Test Duration: " << std::fixed << std::setprecision(3) << elapsed_seconds << " seconds" << std::endl;
    std::cout << "- Messages Received: " << messages_processed << std::endl;
    std::cout << "- Messages/Second: " << std::fixed << std::setprecision(0)
              << (messages_processed / elapsed_seconds) << std::endl;
    std::cout << "- Average Latency: " << std::fixed << std::setprecision(2)
              << plugin->getAverageLatency() << " microseconds" << std::endl;
}

// Print real-time statistics
void printStatistics(IOMDPlugin* plugin) {
    std::cout << "\n=== Session Statistics ===" << std::endl;
    std::cout << "Messages Received: " << plugin->getMessagesReceived() << std::endl;
    std::cout << "Messages Processed: " << plugin->getMessagesProcessed() << std::endl;
    std::cout << "Sequence Errors: " << plugin->getSequenceErrors() << std::endl;
    std::cout << "Packets Dropped: " << plugin->getPacketsDropped() << std::endl;
    std::cout << "Heartbeats Received: " << plugin->getHeartbeatsReceived() << std::endl;
    std::cout << "Current Sequence Number: " << plugin->getCurrentSequenceNumber() << std::endl;
    std::cout << "Average Latency: " << std::fixed << std::setprecision(2)
              << plugin->getAverageLatency() << " microseconds" << std::endl;

    auto subscribed = plugin->getSubscribedSecurities();
    std::cout << "Subscribed Securities (" << subscribed.size() << "): ";
    for (size_t i = 0; i < std::min(subscribed.size(), size_t(10)); ++i) {
        std::cout << subscribed[i];
        if (i < subscribed.size() - 1) std::cout << ", ";
    }
    if (subscribed.size() > 10) std::cout << " ...";
    std::cout << std::endl;
}

int main() {
    std::cout << "HKEX OMD Market Data Feed Handler Example Application" << std::endl;
    std::cout << "====================================================" << std::endl;

    // Create plugin instance
    auto plugin = createHKEXOMDPlugin();

    // Create and register event handler
    auto event_handler = std::make_shared<MarketDataEventHandler>();
    plugin->registerEventHandler(event_handler);

    // Initialize plugin
    std::cout << "Initializing market data feed handler..." << std::endl;
    if (!plugin->initialize("{}")) {  // Empty config for demo
        std::cerr << "Failed to initialize plugin" << std::endl;
        return 1;
    }

    std::cout << "Plugin initialized successfully" << std::endl;
    std::cout << "Plugin Name: " << plugin->getPluginName() << std::endl;
    std::cout << "Plugin Version: " << plugin->getPluginVersion() << std::endl;

    // Connect to market data feed
    std::cout << "\nConnecting to HKEX OMD feed..." << std::endl;
    if (!plugin->connect()) {
        std::cerr << "Failed to connect to market data feed" << std::endl;
        return 1;
    }

    std::cout << "Connected to HKEX OMD feed successfully" << std::endl;

    // Wait for initial connection setup
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!plugin->isReady()) {
        std::cerr << "Plugin not ready for market data" << std::endl;
        return 1;
    }

    std::cout << "Plugin ready for market data processing" << std::endl;

    try {
        // 1. Market data monitoring
        marketDataMonitoring(plugin.get());

        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 2. Performance test
        performanceTest(plugin.get());

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 3. Real-time monitoring demonstration
        std::cout << "\n=== Real-time Market Data Monitoring ===" << std::endl;
        std::cout << "Monitoring for 60 seconds..." << std::endl;

        // Subscribe to specific securities
        plugin->unsubscribeAll();
        plugin->subscribe(700);  // Tencent
        plugin->subscribe(5);    // HSBC
        plugin->subscribe(941);  // China Mobile

        auto monitoring_start = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::high_resolution_clock::now() - monitoring_start).count() < 60) {

            std::this_thread::sleep_for(std::chrono::seconds(10));

            // Display order books every 10 seconds
            const OrderBook* tencent_book = plugin->getOrderBook(700);
            if (tencent_book) {
                displayOrderBook(tencent_book);
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception during market data processing: " << e.what() << std::endl;
    }

    // Print final statistics
    printStatistics(plugin.get());

    // Disconnect and cleanup
    std::cout << "\nDisconnecting from market data feed..." << std::endl;
    plugin->disconnect();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Shutting down..." << std::endl;
    plugin->shutdown();

    std::cout << "Market data feed handler application completed successfully" << std::endl;
    return 0;
}
