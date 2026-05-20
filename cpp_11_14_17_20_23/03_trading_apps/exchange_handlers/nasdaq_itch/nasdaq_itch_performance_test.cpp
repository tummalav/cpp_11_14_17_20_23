#include "nasdaq_itch_feed_handler.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <map>
#include <unordered_set>

using namespace nasdaq::itch;

// Declare the factory function
std::unique_ptr<IITCHPlugin> createNASDAQITCHPlugin();

// Performance metrics collector for ITCH market data
class ITCHPerformanceMetrics {
private:
    std::vector<uint64_t> latencies_;
    std::atomic<uint64_t> total_messages_{0};
    std::atomic<uint64_t> order_messages_{0};
    std::atomic<uint64_t> trade_messages_{0};
    std::atomic<uint64_t> system_messages_{0};
    std::atomic<uint64_t> directory_messages_{0};
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    std::mutex latencies_mutex_;
    std::map<std::string, uint64_t> symbol_message_counts_;
    std::mutex symbol_counts_mutex_;

public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        latencies_.clear();
        total_messages_.store(0);
        order_messages_.store(0);
        trade_messages_.store(0);
        system_messages_.store(0);
        directory_messages_.store(0);
        symbol_message_counts_.clear();
    }

    void stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
    }

    void recordLatency(uint64_t latency_ns) {
        std::lock_guard<std::mutex> lock(latencies_mutex_);
        latencies_.push_back(latency_ns);
    }

    void incrementTotalMessages() { total_messages_.fetch_add(1); }
    void incrementOrderMessages() { order_messages_.fetch_add(1); }
    void incrementTradeMessages() { trade_messages_.fetch_add(1); }
    void incrementSystemMessages() { system_messages_.fetch_add(1); }
    void incrementDirectoryMessages() { directory_messages_.fetch_add(1); }

    void recordSymbolMessage(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(symbol_counts_mutex_);
        symbol_message_counts_[symbol]++;
    }

    void printResults() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time_ - start_time_);
        double elapsed_seconds = duration.count() / 1000000.0;

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "NASDAQ ITCH PERFORMANCE RESULTS" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        std::cout << "Test Duration: " << std::fixed << std::setprecision(3)
                  << elapsed_seconds << " seconds" << std::endl;
        std::cout << "Total Messages: " << total_messages_.load() << std::endl;
        std::cout << "Order Messages: " << order_messages_.load() << std::endl;
        std::cout << "Trade Messages: " << trade_messages_.load() << std::endl;
        std::cout << "System Messages: " << system_messages_.load() << std::endl;
        std::cout << "Directory Messages: " << directory_messages_.load() << std::endl;

        if (total_messages_.load() > 0) {
            double msg_per_sec = total_messages_.load() / elapsed_seconds;
            std::cout << "Throughput: " << std::fixed << std::setprecision(0)
                      << msg_per_sec << " messages/sec" << std::endl;
        }

        if (!latencies_.empty()) {
            std::vector<uint64_t> sorted_latencies = latencies_;
            std::sort(sorted_latencies.begin(), sorted_latencies.end());

            uint64_t min_lat = sorted_latencies.front();
            uint64_t max_lat = sorted_latencies.back();
            uint64_t p50 = sorted_latencies[sorted_latencies.size() * 50 / 100];
            uint64_t p95 = sorted_latencies[sorted_latencies.size() * 95 / 100];
            uint64_t p99 = sorted_latencies[sorted_latencies.size() * 99 / 100];

            uint64_t sum = 0;
            for (auto lat : sorted_latencies) sum += lat;
            double avg = static_cast<double>(sum) / sorted_latencies.size();

            std::cout << "\nLatency Statistics (microseconds):" << std::endl;
            std::cout << "  Min:     " << std::fixed << std::setprecision(2) << min_lat / 1000.0 << std::endl;
            std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg / 1000.0 << std::endl;
            std::cout << "  P50:     " << std::fixed << std::setprecision(2) << p50 / 1000.0 << std::endl;
            std::cout << "  P95:     " << std::fixed << std::setprecision(2) << p95 / 1000.0 << std::endl;
            std::cout << "  P99:     " << std::fixed << std::setprecision(2) << p99 / 1000.0 << std::endl;
            std::cout << "  Max:     " << std::fixed << std::setprecision(2) << max_lat / 1000.0 << std::endl;
        }

        // Top symbols by message count
        std::vector<std::pair<std::string, uint64_t>> sorted_symbols;
        for (const auto& pair : symbol_message_counts_) {
            sorted_symbols.push_back(pair);
        }
        std::sort(sorted_symbols.begin(), sorted_symbols.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        std::cout << "\nTop 15 Symbols by Message Count:" << std::endl;
        for (size_t i = 0; i < std::min(sorted_symbols.size(), size_t(15)); ++i) {
            std::cout << "  " << sorted_symbols[i].first << ": "
                      << sorted_symbols[i].second << " messages" << std::endl;
        }

        std::cout << std::string(70, '=') << std::endl;
    }

    void saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        file << "latency_ns,message_index\n";
        for (size_t i = 0; i < latencies_.size(); ++i) {
            file << latencies_[i] << "," << i << "\n";
        }
        file.close();

        std::cout << "Latency data saved to: " << filename << std::endl;
    }

    uint64_t getTotalMessages() const { return total_messages_.load(); }
};

// High-frequency event handler for performance testing
class PerformanceITCHEventHandler : public IITCHEventHandler {
private:
    ITCHPerformanceMetrics* metrics_;
    std::atomic<uint64_t> message_count_{0};
    std::unordered_set<std::string> tracked_symbols_;
    std::chrono::high_resolution_clock::time_point start_time_;

public:
    explicit PerformanceITCHEventHandler(ITCHPerformanceMetrics* metrics)
        : metrics_(metrics), start_time_(std::chrono::high_resolution_clock::now()) {

        // Track popular symbols for latency measurement
        tracked_symbols_.insert("AAPL    ");
        tracked_symbols_.insert("MSFT    ");
        tracked_symbols_.insert("GOOGL   ");
        tracked_symbols_.insert("AMZN    ");
        tracked_symbols_.insert("TSLA    ");
        tracked_symbols_.insert("META    ");
        tracked_symbols_.insert("NFLX    ");
        tracked_symbols_.insert("NVDA    ");
    }

    void onSystemEvent(const SystemEventMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementSystemMessages();
        message_count_.fetch_add(1);
    }

    void onStockDirectory(const StockDirectoryMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementDirectoryMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        message_count_.fetch_add(1);
    }

    void onStockTradingAction(const StockTradingActionMessage& msg) override {
        metrics_->incrementTotalMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        message_count_.fetch_add(1);
    }

    void onAddOrder(const AddOrderMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        // Calculate latency for tracked symbols
        if (tracked_symbols_.count(stock)) {
            auto now = std::chrono::high_resolution_clock::now();
            auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count() - msg.header.timestamp;
            metrics_->recordLatency(latency_ns);
        }

        message_count_.fetch_add(1);
    }

    void onAddOrderWithMPID(const AddOrderWithMPIDMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        message_count_.fetch_add(1);
    }

    void onOrderExecuted(const OrderExecutedMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementTradeMessages();
        message_count_.fetch_add(1);
    }

    void onOrderExecutedWithPrice(const OrderExecutedWithPriceMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementTradeMessages();

        // Calculate execution latency
        auto now = std::chrono::high_resolution_clock::now();
        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count() - msg.header.timestamp;
        metrics_->recordLatency(latency_ns);

        message_count_.fetch_add(1);
    }

    void onOrderCancel(const OrderCancelMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();
        message_count_.fetch_add(1);
    }

    void onOrderDelete(const OrderDeleteMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();
        message_count_.fetch_add(1);
    }

    void onOrderReplace(const OrderReplaceMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();
        message_count_.fetch_add(1);
    }

    void onTrade(const TradeMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementTradeMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        // Calculate trade latency
        if (tracked_symbols_.count(stock)) {
            auto now = std::chrono::high_resolution_clock::now();
            auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count() - msg.header.timestamp;
            metrics_->recordLatency(latency_ns);
        }

        message_count_.fetch_add(1);
    }

    void onCrossTrade(const CrossTradeMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementTradeMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        message_count_.fetch_add(1);
    }

    void onBrokenTrade(const BrokenTradeMessage& msg) override {
        metrics_->incrementTotalMessages();
        message_count_.fetch_add(1);
    }

    void onNOII(const NOIIMessage& msg) override {
        metrics_->incrementTotalMessages();

        std::string stock(msg.stock.data(), 8);
        metrics_->recordSymbolMessage(stock);

        message_count_.fetch_add(1);
    }

    void onDisconnect(const std::string& reason) override {
        std::cout << "Disconnected: " << reason << std::endl;
    }

    uint64_t getMessageCount() const {
        return message_count_.load();
    }
};

// Throughput test - measure maximum ITCH message processing rate
void throughputTest(IITCHPlugin* plugin, PerformanceITCHEventHandler* handler,
                   ITCHPerformanceMetrics* metrics, int duration_seconds) {
    std::cout << "\n=== ITCH THROUGHPUT TEST: " << duration_seconds << " seconds ===" << std::endl;

    // Subscribe to all symbols for maximum load
    plugin->subscribeAll();

    metrics->start();
    uint64_t initial_count = handler->getMessageCount();

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    metrics->stop();
    uint64_t final_count = handler->getMessageCount();
    uint64_t messages_processed = final_count - initial_count;

    double throughput = static_cast<double>(messages_processed) / duration_seconds;

    std::cout << "Throughput Test Results:" << std::endl;
    std::cout << "- Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "- Messages Processed: " << messages_processed << std::endl;
    std::cout << "- Throughput: " << std::fixed << std::setprecision(0)
              << throughput << " messages/sec" << std::endl;

    metrics->printResults();
    metrics->saveToFile("itch_throughput_test_latencies.csv");
}

// Latency test - measure end-to-end ITCH processing latency
void latencyTest(IITCHPlugin* plugin, PerformanceITCHEventHandler* handler,
                ITCHPerformanceMetrics* metrics, int duration_seconds) {
    std::cout << "\n=== ITCH LATENCY TEST: " << duration_seconds << " seconds ===" << std::endl;

    // Subscribe to specific high-activity symbols
    plugin->unsubscribeAll();
    std::vector<std::string> test_symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "META", "NFLX", "NVDA"};

    for (const std::string& symbol : test_symbols) {
        plugin->subscribe(symbol);
    }

    metrics->start();

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    metrics->stop();

    metrics->printResults();
    metrics->saveToFile("itch_latency_test_results.csv");
}

// Order book reconstruction test
void orderBookTest(IITCHPlugin* plugin, int duration_seconds) {
    std::cout << "\n=== ITCH ORDER BOOK TEST: " << duration_seconds << " seconds ===" << std::endl;

    // Focus on liquid symbols for order book testing
    std::vector<std::string> liquid_symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};

    plugin->unsubscribeAll();
    for (const std::string& symbol : liquid_symbols) {
        plugin->subscribe(symbol);
    }

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    // Analyze order book quality
    std::cout << "Order Book Analysis:" << std::endl;
    for (const std::string& symbol : liquid_symbols) {
        const OrderBook* book = plugin->getOrderBook(symbol);
        if (book) {
            std::cout << "Symbol " << symbol << ":" << std::endl;
            std::cout << "  - Bid Levels: " << book->bid_levels.size() << std::endl;
            std::cout << "  - Ask Levels: " << book->ask_levels.size() << std::endl;
            std::cout << "  - Total Volume: " << book->total_volume << std::endl;
            if (book->last_trade_price > 0) {
                std::cout << "  - Last Trade: $" << std::fixed << std::setprecision(4)
                          << (book->last_trade_price / 10000.0) << std::endl;
            }
        }
    }
}

// Message distribution analysis
void messageDistributionTest(IITCHPlugin* plugin, ITCHPerformanceMetrics* metrics, int duration_seconds) {
    std::cout << "\n=== ITCH MESSAGE DISTRIBUTION TEST: " << duration_seconds << " seconds ===" << std::endl;

    plugin->subscribeAll();

    metrics->start();

    // Monitor different types of trading sessions
    for (int i = 0; i < duration_seconds; i += 30) {
        std::this_thread::sleep_for(std::chrono::seconds(30));

        std::cout << "Checkpoint " << (i/30 + 1) << ":" << std::endl;
        std::cout << "  Total Messages: " << metrics->getTotalMessages() << std::endl;
        std::cout << "  Messages/sec: " << std::fixed << std::setprecision(0)
                  << (metrics->getTotalMessages() / (i + 30)) << std::endl;
    }

    metrics->stop();

    std::cout << "Message Distribution Test Results:" << std::endl;
    metrics->printResults();
}

// Stress test with maximum message processing
void stressTest(IITCHPlugin* plugin, PerformanceITCHEventHandler* handler,
               ITCHPerformanceMetrics* metrics, int duration_seconds) {
    std::cout << "\n=== ITCH STRESS TEST: " << duration_seconds << " seconds ===" << std::endl;

    plugin->subscribeAll();

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t initial_messages = plugin->getMessagesReceived();
    uint64_t initial_dropped = plugin->getPacketsDropped();

    metrics->start();

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    metrics->stop();

    auto end_time = std::chrono::high_resolution_clock::now();
    uint64_t final_messages = plugin->getMessagesReceived();
    uint64_t final_dropped = plugin->getPacketsDropped();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    uint64_t messages_processed = final_messages - initial_messages;
    uint64_t packets_dropped = final_dropped - initial_dropped;

    std::cout << "Stress Test Results:" << std::endl;
    std::cout << "- Test Duration: " << duration.count() << " seconds" << std::endl;
    std::cout << "- Messages Processed: " << messages_processed << std::endl;
    std::cout << "- Average Messages/sec: " << (messages_processed / duration.count()) << std::endl;
    std::cout << "- Packets Dropped: " << packets_dropped << std::endl;
    std::cout << "- Drop Rate: " << std::fixed << std::setprecision(4)
              << (static_cast<double>(packets_dropped) / messages_processed * 100.0) << "%" << std::endl;
    std::cout << "- Orders Tracked: " << plugin->getOrdersTracked() << std::endl;
    std::cout << "- Trades Processed: " << plugin->getTradesProcessed() << std::endl;

    metrics->printResults();
}

int main() {
    std::cout << "NASDAQ ITCH Ultra-Low Latency Performance Test" << std::endl;
    std::cout << "===============================================" << std::endl;

    // Create plugin and initialize
    auto plugin = createNASDAQITCHPlugin();

    ITCHPerformanceMetrics metrics;
    auto handler = std::make_shared<PerformanceITCHEventHandler>(&metrics);

    plugin->registerEventHandler(handler);

    if (!plugin->initialize("{}")) {
        std::cerr << "Failed to initialize plugin" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!plugin->connect()) {
        std::cerr << "Failed to connect to ITCH feed" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (!plugin->isReady()) {
        std::cerr << "Plugin not ready" << std::endl;
        return 1;
    }

    std::cout << "Plugin ready. Starting ITCH performance tests..." << std::endl;

    try {
        // Test 1: Throughput test
        throughputTest(plugin.get(), handler.get(), &metrics, 30);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 2: Latency test
        latencyTest(plugin.get(), handler.get(), &metrics, 60);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 3: Order book test
        orderBookTest(plugin.get(), 45);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 4: Message distribution test
        messageDistributionTest(plugin.get(), &metrics, 90);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 5: Stress test
        stressTest(plugin.get(), handler.get(), &metrics, 120);

        // Final statistics
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "FINAL ITCH SESSION STATISTICS" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Total Messages Received: " << plugin->getMessagesReceived() << std::endl;
        std::cout << "Total Messages Processed: " << plugin->getMessagesProcessed() << std::endl;
        std::cout << "Total Orders Tracked: " << plugin->getOrdersTracked() << std::endl;
        std::cout << "Total Trades Processed: " << plugin->getTradesProcessed() << std::endl;
        std::cout << "Total Packets Dropped: " << plugin->getPacketsDropped() << std::endl;
        std::cout << "Session Average Latency: " << std::fixed << std::setprecision(2)
                  << plugin->getAverageLatency() << " microseconds" << std::endl;

        // Calculate overall performance metrics
        std::cout << "Overall Throughput: " << std::fixed << std::setprecision(0)
                  << (plugin->getMessagesReceived() / 450.0) << " messages/sec" << std::endl; // Assuming ~7.5 minutes total

        double drop_rate = static_cast<double>(plugin->getPacketsDropped()) / plugin->getMessagesReceived() * 100.0;
        std::cout << "Overall Drop Rate: " << std::fixed << std::setprecision(4) << drop_rate << "%" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception during ITCH performance test: " << e.what() << std::endl;
    }

    // Cleanup
    plugin->disconnect();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    plugin->shutdown();

    std::cout << "\nITCH performance test completed successfully" << std::endl;
    return 0;
}
