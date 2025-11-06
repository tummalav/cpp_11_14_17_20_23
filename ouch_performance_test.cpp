#include "ouch_plugin_manager.hpp"
#include "ouch_asx_order_handler.cpp" // Include implementation for direct usage
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <thread>

namespace asx::ouch {

// Performance test event handler
class PerfTestEventHandler : public IOrderEventHandler {
private:
    std::vector<uint64_t> latencies_;
    std::atomic<uint64_t> accepted_count_{0};
    std::atomic<uint64_t> rejected_count_{0};
    std::atomic<uint64_t> executed_count_{0};
    mutable std::mutex latencies_mutex_;

public:
    void onOrderAccepted(const OrderAcceptedMessage& msg) override {
        uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        uint64_t latency = now - msg.header.timestamp;

        {
            std::lock_guard<std::mutex> lock(latencies_mutex_);
            latencies_.push_back(latency);
        }

        accepted_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void onOrderExecuted(const OrderExecutedMessage& msg) override {
        executed_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void onOrderRejected(const OrderRejectedMessage& msg) override {
        rejected_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void onOrderCanceled(const std::array<char, 14>& order_token) override {}
    void onOrderReplaced(const std::array<char, 14>& old_token,
                        const std::array<char, 14>& new_token) override {}
    void onBrokenTrade(uint64_t match_number) override {}

    uint64_t getAcceptedCount() const { return accepted_count_.load(); }
    uint64_t getRejectedCount() const { return rejected_count_.load(); }
    uint64_t getExecutedCount() const { return executed_count_.load(); }

    std::vector<double> getLatencyStatistics() const {
        std::lock_guard<std::mutex> lock(latencies_mutex_);

        if (latencies_.empty()) {
            return {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}; // min, avg, max, p50, p95, p99
        }

        std::vector<uint64_t> sorted_latencies = latencies_;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());

        double min_us = sorted_latencies.front() / 1000.0;
        double max_us = sorted_latencies.back() / 1000.0;

        uint64_t sum = std::accumulate(sorted_latencies.begin(), sorted_latencies.end(), 0ULL);
        double avg_us = (sum / sorted_latencies.size()) / 1000.0;

        size_t p50_idx = sorted_latencies.size() * 50 / 100;
        size_t p95_idx = sorted_latencies.size() * 95 / 100;
        size_t p99_idx = sorted_latencies.size() * 99 / 100;

        double p50_us = sorted_latencies[p50_idx] / 1000.0;
        double p95_us = sorted_latencies[p95_idx] / 1000.0;
        double p99_us = sorted_latencies[p99_idx] / 1000.0;

        return {min_us, avg_us, max_us, p50_us, p95_us, p99_us};
    }

    void reset() {
        std::lock_guard<std::mutex> lock(latencies_mutex_);
        latencies_.clear();
        accepted_count_.store(0);
        rejected_count_.store(0);
        executed_count_.store(0);
    }
};

// Performance test configuration
struct PerfTestConfig {
    uint32_t num_orders = 10000;
    uint32_t orders_per_second = 1000;
    uint32_t num_threads = 1;
    uint32_t warmup_orders = 1000;
    bool measure_latency = true;
    std::string instrument = "BHP.AX";
    uint32_t order_size = 100;
    uint64_t base_price = 4500;
};

// Single-threaded performance test
void runSingleThreadedTest(IOUCHPlugin* plugin, const PerfTestConfig& config,
                          PerfTestEventHandler& handler) {
    std::cout << "Running single-threaded performance test...\n";
    std::cout << "Orders: " << config.num_orders
              << ", Target Rate: " << config.orders_per_second << " orders/sec\n";

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t interval_ns = 1000000000ULL / config.orders_per_second;

    for (uint32_t i = 0; i < config.num_orders; ++i) {
        auto order = OrderBuilder()
            .setOrderToken("PERF" + std::to_string(i))
            .setSide((i % 2 == 0) ? Side::BUY : Side::SELL)
            .setQuantity(config.order_size)
            .setInstrument(config.instrument)
            .setPrice(config.base_price + (i % 10))
            .setTimeInForce(TimeInForce::IOC)
            .setFirm("TEST")
            .setDisplay(1)
            .setMinimumQuantity(1)
            .build();

        if (!plugin->sendEnterOrder(order)) {
            std::cerr << "Failed to send order " << i << std::endl;
            break;
        }

        // Rate limiting
        if (i < config.num_orders - 1) {
            auto next_send_time = start_time + std::chrono::nanoseconds((i + 1) * interval_ns);
            auto now = std::chrono::high_resolution_clock::now();
            if (next_send_time > now) {
                std::this_thread::sleep_for(next_send_time - now);
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Sent " << config.num_orders << " orders in " << duration.count() << " ms\n";
    std::cout << "Actual rate: " << (config.num_orders * 1000.0 / duration.count()) << " orders/sec\n";
}

// Multi-threaded performance test
void runMultiThreadedTest(IOUCHPlugin* plugin, const PerfTestConfig& config,
                         PerfTestEventHandler& handler) {
    std::cout << "Running multi-threaded performance test...\n";
    std::cout << "Orders: " << config.num_orders
              << ", Threads: " << config.num_threads
              << ", Target Rate: " << config.orders_per_second << " orders/sec\n";

    std::atomic<uint32_t> order_counter{0};
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    uint32_t orders_per_thread = config.num_orders / config.num_threads;
    uint32_t rate_per_thread = config.orders_per_second / config.num_threads;
    uint64_t interval_ns = 1000000000ULL / rate_per_thread;

    for (uint32_t t = 0; t < config.num_threads; ++t) {
        threads.emplace_back([&, t]() {
            uint32_t thread_start = t * orders_per_thread;
            uint32_t thread_end = (t == config.num_threads - 1) ?
                                  config.num_orders : (t + 1) * orders_per_thread;

            auto thread_start_time = std::chrono::high_resolution_clock::now();

            for (uint32_t i = thread_start; i < thread_end; ++i) {
                uint32_t order_id = order_counter.fetch_add(1, std::memory_order_relaxed);

                auto order = OrderBuilder()
                    .setOrderToken("MT" + std::to_string(order_id))
                    .setSide((order_id % 2 == 0) ? Side::BUY : Side::SELL)
                    .setQuantity(config.order_size)
                    .setInstrument(config.instrument)
                    .setPrice(config.base_price + (order_id % 10))
                    .setTimeInForce(TimeInForce::IOC)
                    .setFirm("TEST")
                    .setDisplay(1)
                    .setMinimumQuantity(1)
                    .build();

                if (!plugin->sendEnterOrder(order)) {
                    std::cerr << "Thread " << t << " failed to send order " << order_id << std::endl;
                    break;
                }

                // Rate limiting per thread
                if (i < thread_end - 1) {
                    uint32_t local_order = i - thread_start;
                    auto next_send_time = thread_start_time +
                                        std::chrono::nanoseconds((local_order + 1) * interval_ns);
                    auto now = std::chrono::high_resolution_clock::now();
                    if (next_send_time > now) {
                        std::this_thread::sleep_for(next_send_time - now);
                    }
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Sent " << config.num_orders << " orders in " << duration.count() << " ms\n";
    std::cout << "Actual rate: " << (config.num_orders * 1000.0 / duration.count()) << " orders/sec\n";
}

// Latency measurement test
void runLatencyTest(IOUCHPlugin* plugin, const PerfTestConfig& config,
                   PerfTestEventHandler& handler) {
    std::cout << "Running latency measurement test...\n";
    std::cout << "Measuring latency for " << config.num_orders << " orders\n";

    handler.reset();

    // Send orders with timestamps
    for (uint32_t i = 0; i < config.num_orders; ++i) {
        auto order = OrderBuilder()
            .setOrderToken("LAT" + std::to_string(i))
            .setSide(Side::BUY)
            .setQuantity(config.order_size)
            .setInstrument(config.instrument)
            .setPrice(config.base_price - 100) // Ensure no execution
            .setTimeInForce(TimeInForce::IOC)
            .setFirm("TEST")
            .setDisplay(1)
            .setMinimumQuantity(1)
            .build();

        if (!plugin->sendEnterOrder(order)) {
            std::cerr << "Failed to send latency test order " << i << std::endl;
            break;
        }

        // Small delay to avoid overwhelming the system
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Wait for responses
    std::cout << "Waiting for responses...\n";
    auto wait_start = std::chrono::steady_clock::now();

    while (handler.getAcceptedCount() + handler.getRejectedCount() < config.num_orders) {
        auto elapsed = std::chrono::steady_clock::now() - wait_start;
        if (elapsed > std::chrono::seconds(30)) {
            std::cout << "Timeout waiting for responses\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Print latency statistics
    auto stats = handler.getLatencyStatistics();
    std::cout << "\nLatency Statistics (microseconds):\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Min:     " << stats[0] << " μs\n";
    std::cout << "  Average: " << stats[1] << " μs\n";
    std::cout << "  Max:     " << stats[2] << " μs\n";
    std::cout << "  P50:     " << stats[3] << " μs\n";
    std::cout << "  P95:     " << stats[4] << " μs\n";
    std::cout << "  P99:     " << stats[5] << " μs\n";
    std::cout << "  Accepted: " << handler.getAcceptedCount() << "\n";
    std::cout << "  Rejected: " << handler.getRejectedCount() << "\n";
}

} // namespace asx::ouch

int main(int argc, char* argv[]) {
    using namespace asx::ouch;

    std::cout << "ASX OUCH Performance Test\n";
    std::cout << "========================\n\n";

    // Parse command line arguments (simplified)
    PerfTestConfig config;
    if (argc > 1) config.num_orders = std::stoul(argv[1]);
    if (argc > 2) config.orders_per_second = std::stoul(argv[2]);
    if (argc > 3) config.num_threads = std::stoul(argv[3]);

    try {
        // Create and initialize plugin
        auto plugin = std::make_unique<ASXOUCHOrderHandler>();

        std::string plugin_config = R"({
            "server_ip": "203.0.113.10",
            "server_port": 8080,
            "firm_id": "TEST",
            "enable_order_tracking": false,
            "enable_latency_tracking": true
        })";

        if (!plugin->initialize(plugin_config)) {
            std::cerr << "Failed to initialize plugin\n";
            return 1;
        }

        // Wait for connection
        while (!plugin->isReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "Plugin ready. Starting performance tests...\n\n";

        // Create event handler
        PerfTestEventHandler handler;
        plugin->registerEventHandler(std::make_shared<PerfTestEventHandler>(handler));

        // Run warmup
        std::cout << "Warming up with " << config.warmup_orders << " orders...\n";
        PerfTestConfig warmup_config = config;
        warmup_config.num_orders = config.warmup_orders;
        runSingleThreadedTest(plugin.get(), warmup_config, handler);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Test 1: Single-threaded throughput
        std::cout << "\n=== Test 1: Single-threaded Throughput ===\n";
        runSingleThreadedTest(plugin.get(), config, handler);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Test 2: Multi-threaded throughput
        if (config.num_threads > 1) {
            std::cout << "\n=== Test 2: Multi-threaded Throughput ===\n";
            runMultiThreadedTest(plugin.get(), config, handler);

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        // Test 3: Latency measurement
        std::cout << "\n=== Test 3: Latency Measurement ===\n";
        PerfTestConfig latency_config = config;
        latency_config.num_orders = std::min(1000U, config.num_orders);
        runLatencyTest(plugin.get(), latency_config, handler);

        // Final plugin statistics
        std::cout << "\n=== Final Plugin Statistics ===\n";
        std::cout << "Total Orders Sent: " << plugin->getOrdersSent() << "\n";
        std::cout << "Total Orders Accepted: " << plugin->getOrdersAccepted() << "\n";
        std::cout << "Total Orders Rejected: " << plugin->getOrdersRejected() << "\n";
        std::cout << "Total Executions: " << plugin->getExecutions() << "\n";
        std::cout << "Plugin Average Latency: " << plugin->getAverageLatency() << " μs\n";

        // Shutdown
        plugin->shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nPerformance test completed.\n";
    return 0;
}
