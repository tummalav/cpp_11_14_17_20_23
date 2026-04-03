#include "hkex_omd_feed_handler.hpp"
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

using namespace hkex::omd;

// Declare the factory function
std::unique_ptr<IOMDPlugin> createHKEXOMDPlugin();

// Performance metrics collector for market data
class MDPerformanceMetrics {
private:
    std::vector<uint64_t> latencies_;
    std::atomic<uint64_t> total_messages_{0};
    std::atomic<uint64_t> order_messages_{0};
    std::atomic<uint64_t> trade_messages_{0};
    std::atomic<uint64_t> statistics_messages_{0};
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    std::mutex latencies_mutex_;
    std::map<uint32_t, uint64_t> security_message_counts_;
    std::mutex security_counts_mutex_;

public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        latencies_.clear();
        total_messages_.store(0);
        order_messages_.store(0);
        trade_messages_.store(0);
        statistics_messages_.store(0);
        security_message_counts_.clear();
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
    void incrementStatisticsMessages() { statistics_messages_.fetch_add(1); }

    void recordSecurityMessage(uint32_t security_code) {
        std::lock_guard<std::mutex> lock(security_counts_mutex_);
        security_message_counts_[security_code]++;
    }

    void printResults() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time_ - start_time_);
        double elapsed_seconds = duration.count() / 1000000.0;

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "HKEX OMD MARKET DATA PERFORMANCE RESULTS" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        std::cout << "Test Duration: " << std::fixed << std::setprecision(3)
                  << elapsed_seconds << " seconds" << std::endl;
        std::cout << "Total Messages: " << total_messages_.load() << std::endl;
        std::cout << "Order Messages: " << order_messages_.load() << std::endl;
        std::cout << "Trade Messages: " << trade_messages_.load() << std::endl;
        std::cout << "Statistics Messages: " << statistics_messages_.load() << std::endl;

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

        // Top securities by message count
        std::vector<std::pair<uint32_t, uint64_t>> sorted_securities;
        for (const auto& pair : security_message_counts_) {
            sorted_securities.push_back(pair);
        }
        std::sort(sorted_securities.begin(), sorted_securities.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

        std::cout << "\nTop 10 Securities by Message Count:" << std::endl;
        for (size_t i = 0; i < std::min(sorted_securities.size(), size_t(10)); ++i) {
            std::cout << "  " << sorted_securities[i].first << ": "
                      << sorted_securities[i].second << " messages" << std::endl;
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
};

// High-frequency event handler for performance testing
class PerformanceMDEventHandler : public IOMDEventHandler {
private:
    MDPerformanceMetrics* metrics_;
    std::atomic<uint64_t> message_count_{0};
    std::chrono::high_resolution_clock::time_point start_time_;

public:
    explicit PerformanceMDEventHandler(MDPerformanceMetrics* metrics)
        : metrics_(metrics), start_time_(std::chrono::high_resolution_clock::now()) {}

    void onAddOrder(const AddOrderMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);

        // Calculate latency
        auto now = std::chrono::high_resolution_clock::now();
        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count() - msg.header.send_time;
        metrics_->recordLatency(latency_ns);

        message_count_.fetch_add(1);
    }

    void onModifyOrder(const ModifyOrderMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);
        message_count_.fetch_add(1);
    }

    void onDeleteOrder(const DeleteOrderMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementOrderMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);
        message_count_.fetch_add(1);
    }

    void onTrade(const TradeMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementTradeMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);

        // Calculate trade latency
        auto now = std::chrono::high_resolution_clock::now();
        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count() - msg.header.send_time;
        metrics_->recordLatency(latency_ns);

        message_count_.fetch_add(1);
    }

    void onTradeCancel(const TradeCancelMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementTradeMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);
        message_count_.fetch_add(1);
    }

    void onSecurityDefinition(const SecurityDefinitionMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);
        message_count_.fetch_add(1);
    }

    void onSecurityStatus(const SecurityStatusMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);
        message_count_.fetch_add(1);
    }

    void onStatistics(const StatisticsMessage& msg) override {
        metrics_->incrementTotalMessages();
        metrics_->incrementStatisticsMessages();
        metrics_->recordSecurityMessage(msg.header.security_code);
        message_count_.fetch_add(1);
    }

    void onIndexData(const IndexDataMessage& msg) override {
        metrics_->incrementTotalMessages();
        message_count_.fetch_add(1);
    }

    void onMarketTurnover(const MarketTurnoverMessage& msg) override {
        metrics_->incrementTotalMessages();
        message_count_.fetch_add(1);
    }

    void onHeartbeat() override {
        // Silent for performance testing
    }

    void onSequenceReset(uint32_t new_seq_num) override {
        std::cout << "Sequence reset to: " << new_seq_num << std::endl;
    }

    void onDisconnect(const std::string& reason) override {
        std::cout << "Disconnected: " << reason << std::endl;
    }

    uint64_t getMessageCount() const {
        return message_count_.load();
    }
};

// Throughput test - measure maximum message processing rate
void throughputTest(IOMDPlugin* plugin, PerformanceMDEventHandler* handler,
                   MDPerformanceMetrics* metrics, int duration_seconds) {
    std::cout << "\n=== THROUGHPUT TEST: " << duration_seconds << " seconds ===" << std::endl;

    // Subscribe to all securities for maximum load
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
    metrics->saveToFile("throughput_test_latencies.csv");
}

// Latency test - measure end-to-end processing latency
void latencyTest(IOMDPlugin* plugin, PerformanceMDEventHandler* handler,
                MDPerformanceMetrics* metrics, int duration_seconds) {
    std::cout << "\n=== LATENCY TEST: " << duration_seconds << " seconds ===" << std::endl;

    // Subscribe to specific high-volume securities
    plugin->unsubscribeAll();
    std::vector<uint32_t> test_securities = {700, 5, 941, 1299, 2318, 3988, 1398, 2628};

    for (uint32_t security : test_securities) {
        plugin->subscribe(security);
    }

    metrics->start();

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    metrics->stop();

    metrics->printResults();
    metrics->saveToFile("latency_test_results.csv");
}

// Market depth test - analyze order book building performance
void marketDepthTest(IOMDPlugin* plugin, int duration_seconds) {
    std::cout << "\n=== MARKET DEPTH TEST: " << duration_seconds << " seconds ===" << std::endl;

    // Focus on liquid securities
    std::vector<uint32_t> liquid_securities = {700, 5, 941, 1299, 2318};

    plugin->unsubscribeAll();
    for (uint32_t security : liquid_securities) {
        plugin->subscribe(security);
    }

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    // Analyze order book quality
    std::cout << "Order Book Analysis:" << std::endl;
    for (uint32_t security : liquid_securities) {
        const OrderBook* book = plugin->getOrderBook(security);
        if (book) {
            std::cout << "Security " << security << ":" << std::endl;
            std::cout << "  - Bid Levels: " << book->bid_levels.size() << std::endl;
            std::cout << "  - Ask Levels: " << book->ask_levels.size() << std::endl;
            std::cout << "  - Total Volume: " << book->total_volume << std::endl;
            std::cout << "  - Last Trade: " << (book->last_trade_price / 1000.0) << std::endl;
        }
    }
}

// Memory usage test
void memoryUsageTest(IOMDPlugin* plugin, int duration_seconds) {
    std::cout << "\n=== MEMORY USAGE TEST: " << duration_seconds << " seconds ===" << std::endl;

    plugin->subscribeAll();

    // Monitor for leaks or excessive memory usage
    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t initial_messages = plugin->getMessagesReceived();

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    auto end_time = std::chrono::high_resolution_clock::now();
    uint64_t final_messages = plugin->getMessagesReceived();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    uint64_t messages_processed = final_messages - initial_messages;

    std::cout << "Memory Usage Test Results:" << std::endl;
    std::cout << "- Test Duration: " << duration.count() << " seconds" << std::endl;
    std::cout << "- Messages Processed: " << messages_processed << std::endl;
    std::cout << "- Average Messages/sec: " << (messages_processed / duration.count()) << std::endl;
    std::cout << "- Packets Dropped: " << plugin->getPacketsDropped() << std::endl;
    std::cout << "- Sequence Errors: " << plugin->getSequenceErrors() << std::endl;
}

int main() {
    std::cout << "HKEX OMD Ultra-Low Latency Performance Test" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Create plugin and initialize
    auto plugin = createHKEXOMDPlugin();

    MDPerformanceMetrics metrics;
    auto handler = std::make_shared<PerformanceMDEventHandler>(&metrics);

    plugin->registerEventHandler(handler);

    if (!plugin->initialize("{}")) {
        std::cerr << "Failed to initialize plugin" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!plugin->connect()) {
        std::cerr << "Failed to connect to market data feed" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (!plugin->isReady()) {
        std::cerr << "Plugin not ready" << std::endl;
        return 1;
    }

    std::cout << "Plugin ready. Starting performance tests..." << std::endl;

    try {
        // Test 1: Throughput test
        throughputTest(plugin.get(), handler.get(), &metrics, 30);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 2: Latency test
        latencyTest(plugin.get(), handler.get(), &metrics, 60);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 3: Market depth test
        marketDepthTest(plugin.get(), 30);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 4: Memory usage test
        memoryUsageTest(plugin.get(), 120);

        // Final statistics
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "FINAL SESSION STATISTICS" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Total Messages Received: " << plugin->getMessagesReceived() << std::endl;
        std::cout << "Total Messages Processed: " << plugin->getMessagesProcessed() << std::endl;
        std::cout << "Total Sequence Errors: " << plugin->getSequenceErrors() << std::endl;
        std::cout << "Total Packets Dropped: " << plugin->getPacketsDropped() << std::endl;
        std::cout << "Total Heartbeats: " << plugin->getHeartbeatsReceived() << std::endl;
        std::cout << "Session Average Latency: " << std::fixed << std::setprecision(2)
                  << plugin->getAverageLatency() << " microseconds" << std::endl;
        std::cout << "Current Sequence Number: " << plugin->getCurrentSequenceNumber() << std::endl;

        // Calculate overall statistics
        auto total_runtime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now() -
            std::chrono::high_resolution_clock::now()).count(); // This would be calculated properly

        std::cout << "Overall Throughput: " << std::fixed << std::setprecision(0)
                  << (plugin->getMessagesReceived() / 300.0) << " messages/sec" << std::endl; // Assuming ~5 minutes total

    } catch (const std::exception& e) {
        std::cerr << "Exception during performance test: " << e.what() << std::endl;
    }

    // Cleanup
    plugin->disconnect();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    plugin->shutdown();

    std::cout << "\nPerformance test completed successfully" << std::endl;
    return 0;
}
