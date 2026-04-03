#include "hkex_ocg_order_handler.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iomanip>
#include <algorithm>
#include <fstream>

using namespace hkex::ocg;

// Declare the factory function
std::unique_ptr<IOCGPlugin> createHKEXOCGPlugin();

// Performance metrics collector
class PerformanceMetrics {
private:
    std::vector<uint64_t> latencies_;
    std::atomic<uint64_t> total_orders_{0};
    std::atomic<uint64_t> successful_orders_{0};
    std::atomic<uint64_t> failed_orders_{0};
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    std::mutex latencies_mutex_;

public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        latencies_.clear();
        total_orders_.store(0);
        successful_orders_.store(0);
        failed_orders_.store(0);
    }

    void stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
    }

    void recordOrderLatency(uint64_t latency_ns) {
        std::lock_guard<std::mutex> lock(latencies_mutex_);
        latencies_.push_back(latency_ns);
    }

    void incrementTotalOrders() { total_orders_.fetch_add(1); }
    void incrementSuccessfulOrders() { successful_orders_.fetch_add(1); }
    void incrementFailedOrders() { failed_orders_.fetch_add(1); }

    void printResults() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time_ - start_time_);
        double elapsed_seconds = duration.count() / 1000000.0;

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "HKEX OCG-C PERFORMANCE RESULTS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        std::cout << "Test Duration: " << std::fixed << std::setprecision(3)
                  << elapsed_seconds << " seconds" << std::endl;
        std::cout << "Total Orders: " << total_orders_.load() << std::endl;
        std::cout << "Successful Orders: " << successful_orders_.load() << std::endl;
        std::cout << "Failed Orders: " << failed_orders_.load() << std::endl;

        if (total_orders_.load() > 0) {
            double success_rate = (static_cast<double>(successful_orders_.load()) / total_orders_.load()) * 100.0;
            std::cout << "Success Rate: " << std::fixed << std::setprecision(2) << success_rate << "%" << std::endl;
        }

        double throughput = total_orders_.load() / elapsed_seconds;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) << throughput << " orders/sec" << std::endl;

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

        std::cout << std::string(60, '=') << std::endl;
    }

    void saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        file << "latency_ns,order_index\n";
        for (size_t i = 0; i < latencies_.size(); ++i) {
            file << latencies_[i] << "," << i << "\n";
        }
        file.close();

        std::cout << "Latency data saved to: " << filename << std::endl;
    }
};

// High-frequency event handler for performance testing
class PerformanceEventHandler : public IOCGEventHandler {
private:
    PerformanceMetrics* metrics_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> order_times_;
    std::mutex times_mutex_;

public:
    explicit PerformanceEventHandler(PerformanceMetrics* metrics) : metrics_(metrics) {}

    void recordOrderSendTime(const std::string& cl_ord_id) {
        std::lock_guard<std::mutex> lock(times_mutex_);
        order_times_[cl_ord_id] = std::chrono::high_resolution_clock::now();
    }

    void onLogonResponse(bool success, const std::string& reason) override {
        std::cout << "Login " << (success ? "successful" : "failed") << ": " << reason << std::endl;
    }

    void onExecutionReport(const ExecutionReport& exec_report) override {
        metrics_->incrementSuccessfulOrders();

        std::string cl_ord_id(exec_report.cl_ord_id.data(), 20);
        cl_ord_id.erase(cl_ord_id.find('\0'));  // Remove null terminator

        std::lock_guard<std::mutex> lock(times_mutex_);
        auto it = order_times_.find(cl_ord_id);
        if (it != order_times_.end()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(now - it->second).count();
            metrics_->recordOrderLatency(latency);
            order_times_.erase(it);
        }
    }

    void onOrderCancelReject([[maybe_unused]] const OrderCancelReject& cancel_reject) override {
        metrics_->incrementFailedOrders();
    }

    void onBusinessReject(const std::string& reason) override {
        metrics_->incrementFailedOrders();
        std::cout << "Business Reject: " << reason << std::endl;
    }

    void onDisconnect(const std::string& reason) override {
        std::cout << "Disconnected: " << reason << std::endl;
    }

    void onHeartbeat() override {
        // Silent for performance testing
    }
};

// Burst test - send orders as fast as possible
void burstTest(IOCGPlugin* plugin, PerformanceEventHandler* handler, PerformanceMetrics* metrics, int num_orders) {
    std::cout << "\n=== BURST TEST: " << num_orders << " orders ===" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> price_dis(10000, 50000);
    std::uniform_int_distribution<> qty_dis(100, 1000);

    std::vector<std::string> symbols = {"700", "005", "941", "1299", "2318", "3988", "1398", "2628", "1810", "0883"};

    metrics->start();

    for (int i = 0; i < num_orders; ++i) {
        NewOrderSingle order{};

        std::string cl_ord_id = "BURST" + std::to_string(i);
        std::strncpy(order.cl_ord_id.data(), cl_ord_id.c_str(), 20);

        std::string symbol = symbols[i % symbols.size()];
        std::strncpy(order.security_id.data(), symbol.c_str(), 12);
        std::strncpy(order.symbol.data(), symbol.c_str(), 3);

        order.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        order.order_qty = qty_dis(gen);
        order.ord_type = OrderType::LIMIT;
        order.price = price_dis(gen);
        order.time_in_force = TimeInForce::DAY;
        order.capacity = 1;
        order.market_segment = MarketSegment::MAIN_BOARD;

        metrics->incrementTotalOrders();
        handler->recordOrderSendTime(cl_ord_id);

        if (!plugin->sendNewOrder(order)) {
            metrics->incrementFailedOrders();
        }
    }

    // Wait for responses
    std::this_thread::sleep_for(std::chrono::seconds(5));
    metrics->stop();

    metrics->printResults();
    metrics->saveToFile("burst_test_latencies.csv");
}

// Sustained throughput test
void sustainedThroughputTest(IOCGPlugin* plugin, PerformanceEventHandler* handler,
                           PerformanceMetrics* metrics, int orders_per_second, int duration_seconds) {
    std::cout << "\n=== SUSTAINED THROUGHPUT TEST: " << orders_per_second
              << " orders/sec for " << duration_seconds << " seconds ===" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> price_dis(10000, 50000);
    std::uniform_int_distribution<> qty_dis(100, 1000);

    std::vector<std::string> symbols = {"700", "005", "941", "1299", "2318"};

    uint64_t interval_ns = 1000000000ULL / orders_per_second;  // Nanoseconds between orders
    int total_orders = orders_per_second * duration_seconds;

    metrics->start();

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_orders; ++i) {
        NewOrderSingle order{};

        std::string cl_ord_id = "SUST" + std::to_string(i);
        std::strncpy(order.cl_ord_id.data(), cl_ord_id.c_str(), 20);

        std::string symbol = symbols[i % symbols.size()];
        std::strncpy(order.security_id.data(), symbol.c_str(), 12);
        std::strncpy(order.symbol.data(), symbol.c_str(), 3);

        order.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        order.order_qty = qty_dis(gen);
        order.ord_type = OrderType::LIMIT;
        order.price = price_dis(gen);
        order.time_in_force = TimeInForce::DAY;
        order.capacity = 1;
        order.market_segment = MarketSegment::MAIN_BOARD;

        metrics->incrementTotalOrders();
        handler->recordOrderSendTime(cl_ord_id);

        if (!plugin->sendNewOrder(order)) {
            metrics->incrementFailedOrders();
        }

        // Sleep to maintain target rate
        auto target_time = start_time + std::chrono::nanoseconds((i + 1) * interval_ns);
        std::this_thread::sleep_until(target_time);
    }

    // Wait for responses
    std::this_thread::sleep_for(std::chrono::seconds(3));
    metrics->stop();

    metrics->printResults();
    metrics->saveToFile("sustained_test_latencies.csv");
}

// Cancel/replace test
void cancelReplaceTest(IOCGPlugin* plugin, PerformanceEventHandler* handler,
                      PerformanceMetrics* metrics, int num_operations) {
    std::cout << "\n=== CANCEL/REPLACE TEST: " << num_operations << " operations ===" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> price_dis(10000, 50000);

    metrics->start();

    std::vector<std::string> order_ids;

    // First, send initial orders
    for (int i = 0; i < num_operations; ++i) {
        NewOrderSingle order{};

        std::string cl_ord_id = "CANREP" + std::to_string(i);
        order_ids.push_back(cl_ord_id);
        std::strncpy(order.cl_ord_id.data(), cl_ord_id.c_str(), 20);
        std::strncpy(order.security_id.data(), "700", 12);
        std::strncpy(order.symbol.data(), "700", 3);

        order.side = Side::BUY;
        order.order_qty = 100;
        order.ord_type = OrderType::LIMIT;
        order.price = price_dis(gen);
        order.time_in_force = TimeInForce::DAY;
        order.capacity = 1;
        order.market_segment = MarketSegment::MAIN_BOARD;

        metrics->incrementTotalOrders();
        handler->recordOrderSendTime(cl_ord_id);
        plugin->sendNewOrder(order);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Then cancel/replace them
    for (int i = 0; i < num_operations; ++i) {
        if (i % 2 == 0) {
            // Cancel order
            OrderCancelRequest cancel{};
            std::strncpy(cancel.orig_cl_ord_id.data(), order_ids[i].c_str(), 20);
            std::strncpy(cancel.cl_ord_id.data(), ("CANCEL" + std::to_string(i)).c_str(), 20);
            std::strncpy(cancel.security_id.data(), "700", 12);
            cancel.side = Side::BUY;

            metrics->incrementTotalOrders();
            plugin->sendCancelOrder(cancel);
        } else {
            // Replace order
            OrderReplaceRequest replace{};
            std::strncpy(replace.orig_cl_ord_id.data(), order_ids[i].c_str(), 20);
            std::strncpy(replace.cl_ord_id.data(), ("REPLACE" + std::to_string(i)).c_str(), 20);
            std::strncpy(replace.security_id.data(), "700", 12);
            replace.side = Side::BUY;
            replace.order_qty = 200;  // Double the quantity
            replace.price = price_dis(gen);
            replace.ord_type = OrderType::LIMIT;
            replace.time_in_force = TimeInForce::DAY;

            metrics->incrementTotalOrders();
            plugin->sendReplaceOrder(replace);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    metrics->stop();

    metrics->printResults();
}

int main() {
    std::cout << "HKEX OCG-C Ultra-Low Latency Performance Test" << std::endl;
    std::cout << "=============================================" << std::endl;

    // Create plugin and initialize
    auto plugin = createHKEXOCGPlugin();

    PerformanceMetrics metrics;
    auto handler = std::make_shared<PerformanceEventHandler>(&metrics);

    plugin->registerEventHandler(handler);

    if (!plugin->initialize("{}")) {
        std::cerr << "Failed to initialize plugin" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!plugin->login()) {
        std::cerr << "Failed to login" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (!plugin->isReady()) {
        std::cerr << "Plugin not ready" << std::endl;
        return 1;
    }

    std::cout << "Plugin ready. Starting performance tests..." << std::endl;

    try {
        // Test 1: Burst test
        burstTest(plugin.get(), handler.get(), &metrics, 1000);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 2: Sustained throughput
        sustainedThroughputTest(plugin.get(), handler.get(), &metrics, 500, 10);
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Test 3: Cancel/Replace test
        cancelReplaceTest(plugin.get(), handler.get(), &metrics, 100);

        // Final statistics
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "FINAL SESSION STATISTICS" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Total Orders Sent: " << plugin->getOrdersSent() << std::endl;
        std::cout << "Total Orders Accepted: " << plugin->getOrdersAccepted() << std::endl;
        std::cout << "Total Orders Rejected: " << plugin->getOrdersRejected() << std::endl;
        std::cout << "Total Executions: " << plugin->getExecutions() << std::endl;
        std::cout << "Session Average Latency: " << std::fixed << std::setprecision(2)
                  << plugin->getAverageLatency() << " microseconds" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception during performance test: " << e.what() << std::endl;
    }

    // Cleanup
    plugin->logout();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    plugin->shutdown();

    std::cout << "\nPerformance test completed successfully" << std::endl;
    return 0;
}
