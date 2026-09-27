#include "ouch_plugin_manager.hpp"
#include "ouch_asx_order_handler.cpp" // Include implementation for direct usage
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>

namespace asx::ouch {

// Example event handler implementation
class ExampleEventHandler : public IOrderEventHandler {
private:
    PerformanceMonitor& monitor_;
    std::atomic<uint64_t> order_count_{0};
    std::atomic<uint64_t> execution_count_{0};

public:
    explicit ExampleEventHandler(PerformanceMonitor& monitor) : monitor_(monitor) {}

    void onOrderAccepted(const OrderAcceptedMessage& msg) override {
        order_count_.fetch_add(1, std::memory_order_relaxed);
        monitor_.incrementOrdersPerSecond();

        std::string token(msg.order_token.begin(), msg.order_token.end());
        std::string instrument(msg.instrument.begin(), msg.instrument.end());

        std::cout << "Order Accepted: Token=" << token
                  << ", Instrument=" << instrument
                  << ", Quantity=" << msg.quantity
                  << ", Price=" << msg.price
                  << ", OrderRef=" << msg.order_reference_number << std::endl;
    }

    void onOrderExecuted(const OrderExecutedMessage& msg) override {
        execution_count_.fetch_add(1, std::memory_order_relaxed);
        monitor_.incrementExecutionsPerSecond();

        std::string token(msg.order_token.begin(), msg.order_token.end());

        std::cout << "Order Executed: Token=" << token
                  << ", ExecutedQty=" << msg.executed_quantity
                  << ", Price=" << msg.execution_price
                  << ", MatchNumber=" << msg.match_number << std::endl;
    }

    void onOrderRejected(const OrderRejectedMessage& msg) override {
        std::string token(msg.order_token.begin(), msg.order_token.end());

        std::cout << "Order Rejected: Token=" << token
                  << ", Reason=" << static_cast<int>(msg.reject_reason) << std::endl;
    }

    void onOrderCanceled(const std::array<char, 14>& order_token) override {
        std::string token(order_token.begin(), order_token.end());
        std::cout << "Order Canceled: Token=" << token << std::endl;
    }

    void onOrderReplaced(const std::array<char, 14>& old_token,
                        const std::array<char, 14>& new_token) override {
        std::string old_str(old_token.begin(), old_token.end());
        std::string new_str(new_token.begin(), new_token.end());
        std::cout << "Order Replaced: Old=" << old_str << ", New=" << new_str << std::endl;
    }

    void onBrokenTrade(uint64_t match_number) override {
        std::cout << "Broken Trade: MatchNumber=" << match_number << std::endl;
    }

    uint64_t getOrderCount() const { return order_count_.load(); }
    uint64_t getExecutionCount() const { return execution_count_.load(); }
};

// Market making strategy example
class SimpleMarketMaker {
private:
    IOUCHPlugin* plugin_;
    std::string instrument_;
    uint32_t spread_ticks_;
    uint32_t order_size_;
    uint64_t reference_price_;
    std::atomic<bool> running_{false};
    std::thread strategy_thread_;

public:
    SimpleMarketMaker(IOUCHPlugin* plugin, const std::string& instrument,
                     uint32_t spread_ticks, uint32_t order_size, uint64_t ref_price)
        : plugin_(plugin), instrument_(instrument), spread_ticks_(spread_ticks),
          order_size_(order_size), reference_price_(ref_price) {}

    void start() {
        running_.store(true, std::memory_order_release);
        strategy_thread_ = std::thread(&SimpleMarketMaker::run, this);
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (strategy_thread_.joinable()) {
            strategy_thread_.join();
        }
    }

private:
    void run() {
        uint32_t order_counter = 0;

        while (running_.load(std::memory_order_acquire)) {
            // Calculate bid/ask prices
            uint64_t bid_price = reference_price_ - (spread_ticks_ / 2);
            uint64_t ask_price = reference_price_ + (spread_ticks_ / 2);

            // Create bid order
            auto bid_order = OrderBuilder()
                .setOrderToken("BID" + std::to_string(order_counter++))
                .setSide(Side::BUY)
                .setQuantity(order_size_)
                .setInstrument(instrument_)
                .setPrice(bid_price)
                .setTimeInForce(TimeInForce::DAY)
                .setFirm("ASX1")
                .setDisplay(1)
                .setMinimumQuantity(1)
                .build();

            // Create ask order
            auto ask_order = OrderBuilder()
                .setOrderToken("ASK" + std::to_string(order_counter++))
                .setSide(Side::SELL)
                .setQuantity(order_size_)
                .setInstrument(instrument_)
                .setPrice(ask_price)
                .setTimeInForce(TimeInForce::DAY)
                .setFirm("ASX1")
                .setDisplay(1)
                .setMinimumQuantity(1)
                .build();

            // Send orders
            if (plugin_->sendEnterOrder(bid_order)) {
                std::cout << "Sent bid order: " << bid_price << " x " << order_size_ << std::endl;
            }

            if (plugin_->sendEnterOrder(ask_order)) {
                std::cout << "Sent ask order: " << ask_price << " x " << order_size_ << std::endl;
            }

            // Wait before next quotes
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

} // namespace asx::ouch

// Global variables for signal handling
std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_shutdown.store(true, std::memory_order_release);
}

int main(int argc, char* argv[]) {
    using namespace asx::ouch;

    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "ASX OUCH Order Handler Example\n";
    std::cout << "================================\n\n";

    try {
        // Create plugin manager
        OUCHPluginManager plugin_manager;

        // For this example, we'll create the plugin directly instead of loading from .so
        // In production, you would compile the plugin as a shared library
        auto plugin = std::make_unique<ASXOUCHOrderHandler>();

        // Initialize plugin with configuration
        std::string config = R"({
            "server_ip": "203.0.113.10",
            "server_port": 8080,
            "firm_id": "ASX1",
            "enable_order_tracking": true,
            "enable_latency_tracking": true
        })";

        if (!plugin->initialize(config)) {
            std::cerr << "Failed to initialize OUCH plugin" << std::endl;
            return 1;
        }

        std::cout << "Plugin initialized: " << plugin->getPluginName()
                  << " v" << plugin->getPluginVersion() << std::endl;

        // Create performance monitor
        PerformanceMonitor monitor;

        // Create and register event handler
        auto event_handler = std::make_shared<ExampleEventHandler>(monitor);
        plugin->registerEventHandler(event_handler);

        // Create market maker strategy
        SimpleMarketMaker market_maker(plugin.get(), "BHP.AX", 2, 100, 4500);

        // Wait for plugin to be ready
        while (!plugin->isReady() && !g_shutdown.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (g_shutdown.load()) {
            std::cout << "Shutdown requested before plugin ready" << std::endl;
            return 0;
        }

        std::cout << "Plugin is ready, starting market maker..." << std::endl;

        // Start market making strategy
        market_maker.start();

        // Statistics reporting thread
        std::thread stats_thread([&]() {
            while (!g_shutdown.load()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));

                if (g_shutdown.load()) break;

                std::cout << "\n--- Performance Statistics ---" << std::endl;
                std::cout << "Orders Sent: " << plugin->getOrdersSent() << std::endl;
                std::cout << "Orders Accepted: " << plugin->getOrdersAccepted() << std::endl;
                std::cout << "Orders Rejected: " << plugin->getOrdersRejected() << std::endl;
                std::cout << "Executions: " << plugin->getExecutions() << std::endl;
                std::cout << "Average Latency: " << plugin->getAverageLatency() << " μs" << std::endl;

                monitor.printStats();
                monitor.resetStats();
                std::cout << "------------------------------\n" << std::endl;
            }
        });

        // Example of sending individual orders
        std::cout << "\nSending test orders..." << std::endl;

        // Create and send a buy order
        auto buy_order = OrderBuilder()
            .setOrderToken("TEST001")
            .setSide(Side::BUY)
            .setQuantity(500)
            .setInstrument("BHP.AX")
            .setPrice(4490)  // 44.90 AUD in ticks
            .setTimeInForce(TimeInForce::DAY)
            .setFirm("ASX1")
            .setDisplay(1)
            .setMinimumQuantity(100)
            .build();

        if (plugin->sendEnterOrder(buy_order)) {
            std::cout << "Test buy order sent successfully" << std::endl;
        } else {
            std::cout << "Failed to send test buy order" << std::endl;
        }

        // Create and send a sell order
        auto sell_order = OrderBuilder()
            .setOrderToken("TEST002")
            .setSide(Side::SELL)
            .setQuantity(300)
            .setInstrument("BHP.AX")
            .setPrice(4510)  // 45.10 AUD in ticks
            .setTimeInForce(TimeInForce::IOC)
            .setFirm("ASX1")
            .setDisplay(1)
            .setMinimumQuantity(50)
            .build();

        if (plugin->sendEnterOrder(sell_order)) {
            std::cout << "Test sell order sent successfully" << std::endl;
        } else {
            std::cout << "Failed to send test sell order" << std::endl;
        }

        std::cout << "\nRunning... Press Ctrl+C to stop\n" << std::endl;

        // Main loop
        while (!g_shutdown.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "Shutting down..." << std::endl;

        // Stop market maker
        market_maker.stop();

        // Stop statistics thread
        stats_thread.join();

        // Final statistics
        std::cout << "\n--- Final Statistics ---" << std::endl;
        std::cout << "Total Orders Sent: " << plugin->getOrdersSent() << std::endl;
        std::cout << "Total Orders Accepted: " << plugin->getOrdersAccepted() << std::endl;
        std::cout << "Total Orders Rejected: " << plugin->getOrdersRejected() << std::endl;
        std::cout << "Total Executions: " << plugin->getExecutions() << std::endl;
        std::cout << "Final Average Latency: " << plugin->getAverageLatency() << " μs" << std::endl;
        std::cout << "Event Handler - Orders: " << event_handler->getOrderCount()
                  << ", Executions: " << event_handler->getExecutionCount() << std::endl;

        // Shutdown plugin
        plugin->shutdown();

        std::cout << "Shutdown complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
