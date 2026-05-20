#include "hkex_ocg_order_handler.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <random>

using namespace hkex::ocg;

// Forward declaration for the concrete implementation
namespace hkex::ocg {
    class HKEXOCGPlugin;
}

// Declare the factory function
std::unique_ptr<IOCGPlugin> createHKEXOCGPlugin();

// Example event handler implementation
class ExampleEventHandler : public IOCGEventHandler {
public:
    void onLogonResponse(bool success, const std::string& reason) override {
        std::cout << "Logon " << (success ? "successful" : "failed") << ": " << reason << std::endl;
    }

    void onExecutionReport(const ExecutionReport& exec_report) override {
        std::cout << "Execution Report - "
                  << "ClOrdID: " << std::string(exec_report.cl_ord_id.data(), 20) << ", "
                  << "ExecType: " << static_cast<char>(exec_report.exec_type) << ", "
                  << "OrdStatus: " << static_cast<char>(exec_report.ord_status) << ", "
                  << "Symbol: " << std::string(exec_report.symbol.data(), 3) << ", "
                  << "Side: " << static_cast<char>(exec_report.side) << ", "
                  << "LastQty: " << exec_report.last_qty << ", "
                  << "LastPx: " << exec_report.last_px << ", "
                  << "CumQty: " << exec_report.cum_qty << ", "
                  << "AvgPx: " << exec_report.avg_px << std::endl;
    }

    void onOrderCancelReject(const OrderCancelReject& cancel_reject) override {
        std::cout << "Order Cancel Reject - "
                  << "ClOrdID: " << std::string(cancel_reject.cl_ord_id.data(), 20) << ", "
                  << "Reason: " << static_cast<int>(cancel_reject.cxl_rej_reason) << ", "
                  << "Text: " << std::string(cancel_reject.text.data(), 32) << std::endl;
    }

    void onBusinessReject(const std::string& reason) override {
        std::cout << "Business Reject: " << reason << std::endl;
    }

    void onDisconnect(const std::string& reason) override {
        std::cout << "Disconnected: " << reason << std::endl;
    }

    void onHeartbeat() override {
        std::cout << "Heartbeat received" << std::endl;
    }
};

// Helper function to create order ID
std::string generateOrderId() {
    static std::atomic<uint64_t> counter{1};
    return "ORD" + std::to_string(counter.fetch_add(1));
}

// Helper function to create new order
NewOrderSingle createSampleOrder(const std::string& symbol, Side side, uint64_t qty, uint64_t price) {
    NewOrderSingle order{};

    std::string cl_ord_id = generateOrderId();
    std::strncpy(order.cl_ord_id.data(), cl_ord_id.c_str(), 20);
    std::strncpy(order.security_id.data(), symbol.c_str(), 12);
    std::strncpy(order.security_id_source.data(), "8", 4);  // Exchange Symbol
    std::strncpy(order.symbol.data(), symbol.c_str(), 3);
    order.side = side;
    order.order_qty = qty;
    order.ord_type = OrderType::LIMIT;
    order.price = price;
    order.time_in_force = TimeInForce::DAY;
    std::strncpy(order.account.data(), "TEST001", 8);
    std::strncpy(order.investor_id.data(), "INV001", 16);
    order.capacity = 1;  // Agency
    order.min_qty = 0;
    order.max_floor = 0;
    std::strncpy(order.text.data(), "Sample order", 32);
    order.market_segment = MarketSegment::MAIN_BOARD;
    order.price_type = 1;  // Limit price
    order.disclosed_qty = 0;  // No iceberg
    std::strncpy(order.party_id.data(), "PARTY001", 16);

    return order;
}

// Market making simulation
void marketMakingSimulation(IOCGPlugin* plugin) {
    std::cout << "\n=== Starting Market Making Simulation ===" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> price_dis(10000, 15000);  // Price range in ticks
    std::uniform_int_distribution<> qty_dis(100, 1000);       // Quantity range

    std::vector<std::string> symbols = {"700", "005", "941", "1299", "2318"};  // Popular HKEX stocks

    for (int i = 0; i < 10; ++i) {
        for (const auto& symbol : symbols) {
            // Place buy order
            uint64_t bid_price = price_dis(gen);
            uint64_t bid_qty = qty_dis(gen);
            auto buy_order = createSampleOrder(symbol, Side::BUY, bid_qty, bid_price);

            if (plugin->sendNewOrder(buy_order)) {
                std::cout << "Sent BUY order for " << symbol << " - Qty: " << bid_qty
                          << ", Price: " << bid_price << std::endl;
            }

            // Place sell order
            uint64_t ask_price = bid_price + 10;  // 10 tick spread
            uint64_t ask_qty = qty_dis(gen);
            auto sell_order = createSampleOrder(symbol, Side::SELL, ask_qty, ask_price);

            if (plugin->sendNewOrder(sell_order)) {
                std::cout << "Sent SELL order for " << symbol << " - Qty: " << ask_qty
                          << ", Price: " << ask_price << std::endl;
            }

            // Small delay between orders to avoid overwhelming the exchange
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Wait between rounds
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Performance benchmarking
void performanceBenchmark(IOCGPlugin* plugin) {
    std::cout << "\n=== Performance Benchmark ===" << std::endl;

    const int num_orders = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_orders; ++i) {
        auto order = createSampleOrder("700", Side::BUY, 100, 10000 + i);
        plugin->sendNewOrder(order);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    double orders_per_second = static_cast<double>(num_orders) / (duration.count() / 1000000.0);
    double avg_latency_us = static_cast<double>(duration.count()) / num_orders;

    std::cout << "Performance Results:" << std::endl;
    std::cout << "- Orders sent: " << num_orders << std::endl;
    std::cout << "- Total time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "- Orders per second: " << std::fixed << std::setprecision(2) << orders_per_second << std::endl;
    std::cout << "- Average latency: " << std::fixed << std::setprecision(2) << avg_latency_us << " microseconds" << std::endl;
}

// Print statistics
void printStatistics(IOCGPlugin* plugin) {
    std::cout << "\n=== Session Statistics ===" << std::endl;
    std::cout << "Orders Sent: " << plugin->getOrdersSent() << std::endl;
    std::cout << "Orders Accepted: " << plugin->getOrdersAccepted() << std::endl;
    std::cout << "Orders Rejected: " << plugin->getOrdersRejected() << std::endl;
    std::cout << "Executions: " << plugin->getExecutions() << std::endl;
    std::cout << "Heartbeats Sent: " << plugin->getHeartbeatsSent() << std::endl;
    std::cout << "Heartbeats Received: " << plugin->getHeartbeatsReceived() << std::endl;
    std::cout << "Average Latency: " << std::fixed << std::setprecision(2)
              << plugin->getAverageLatency() << " microseconds" << std::endl;
}

int main() {
    std::cout << "HKEX OCG-C Order Entry Plugin Example Application" << std::endl;
    std::cout << "=================================================" << std::endl;

    // Create plugin instance
    auto plugin = createHKEXOCGPlugin();

    // Create and register event handler
    auto event_handler = std::make_shared<ExampleEventHandler>();
    plugin->registerEventHandler(event_handler);

    // Initialize plugin
    std::cout << "Initializing plugin..." << std::endl;
    if (!plugin->initialize("{}")) {  // Empty config for demo
        std::cerr << "Failed to initialize plugin" << std::endl;
        return 1;
    }

    std::cout << "Plugin initialized successfully" << std::endl;
    std::cout << "Plugin Name: " << plugin->getPluginName() << std::endl;
    std::cout << "Plugin Version: " << plugin->getPluginVersion() << std::endl;

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Login
    std::cout << "\nLogging in..." << std::endl;
    if (!plugin->login()) {
        std::cerr << "Failed to send login request" << std::endl;
        return 1;
    }

    // Wait for login response
    std::this_thread::sleep_for(std::chrono::seconds(3));

    if (!plugin->isReady()) {
        std::cerr << "Plugin not ready for trading" << std::endl;
        return 1;
    }

    std::cout << "Plugin ready for trading" << std::endl;

    // Run example scenarios
    try {
        // 1. Send a simple order
        std::cout << "\n=== Sending Simple Order ===" << std::endl;
        auto simple_order = createSampleOrder("700", Side::BUY, 100, 35000);  // Tencent buy order
        if (plugin->sendNewOrder(simple_order)) {
            std::cout << "Simple order sent successfully" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 2. Market making simulation
        marketMakingSimulation(plugin.get());

        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 3. Performance benchmark
        performanceBenchmark(plugin.get());

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 4. Order cancellation example
        std::cout << "\n=== Order Cancellation Example ===" << std::endl;
        auto cancel_order = createSampleOrder("005", Side::SELL, 200, 12000);  // HSBC sell order
        std::string cancel_cl_ord_id = std::string(cancel_order.cl_ord_id.data(), 20);

        if (plugin->sendNewOrder(cancel_order)) {
            std::cout << "Order to be cancelled sent: " << cancel_cl_ord_id << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Send cancel request
            OrderCancelRequest cancel_req{};
            std::strncpy(cancel_req.orig_cl_ord_id.data(), cancel_cl_ord_id.c_str(), 20);
            std::strncpy(cancel_req.cl_ord_id.data(), generateOrderId().c_str(), 20);
            std::strncpy(cancel_req.security_id.data(), "005", 12);
            cancel_req.side = Side::SELL;
            std::strncpy(cancel_req.text.data(), "User requested", 32);

            if (plugin->sendCancelOrder(cancel_req)) {
                std::cout << "Cancel request sent for order: " << cancel_cl_ord_id << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));

    } catch (const std::exception& e) {
        std::cerr << "Exception during trading: " << e.what() << std::endl;
    }

    // Print final statistics
    printStatistics(plugin.get());

    // Logout and cleanup
    std::cout << "\nLogging out..." << std::endl;
    plugin->logout();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Shutting down..." << std::endl;
    plugin->shutdown();

    std::cout << "Application completed successfully" << std::endl;
    return 0;
}

// Example of advanced usage with order management
class AdvancedOrderManager {
private:
    IOCGPlugin* plugin_;
    std::unordered_map<std::string, std::string> pending_orders_;  // cl_ord_id -> order_id
    std::mutex orders_mutex_;

public:
    explicit AdvancedOrderManager(IOCGPlugin* plugin) : plugin_(plugin) {}

    bool submitMarketMakingPair(const std::string& symbol, uint64_t mid_price,
                               uint64_t spread, uint64_t quantity) {
        // Calculate bid and ask prices
        uint64_t bid_price = mid_price - spread / 2;
        uint64_t ask_price = mid_price + spread / 2;

        // Create buy order
        auto buy_order = createSampleOrder(symbol, Side::BUY, quantity, bid_price);

        // Create sell order
        auto sell_order = createSampleOrder(symbol, Side::SELL, quantity, ask_price);

        // Submit both orders
        bool buy_success = plugin_->sendNewOrder(buy_order);
        bool sell_success = plugin_->sendNewOrder(sell_order);

        if (buy_success && sell_success) {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            pending_orders_[std::string(buy_order.cl_ord_id.data(), 20)] = "";
            pending_orders_[std::string(sell_order.cl_ord_id.data(), 20)] = "";
            return true;
        }

        return false;
    }

    void cancelAllPendingOrders() {
        std::lock_guard<std::mutex> lock(orders_mutex_);

        for (const auto& [cl_ord_id, order_id] : pending_orders_) {
            OrderCancelRequest cancel_req{};
            std::strncpy(cancel_req.orig_cl_ord_id.data(), cl_ord_id.c_str(), 20);
            std::strncpy(cancel_req.cl_ord_id.data(), generateOrderId().c_str(), 20);
            std::strncpy(cancel_req.text.data(), "Mass cancel", 32);

            plugin_->sendCancelOrder(cancel_req);
        }

        pending_orders_.clear();
    }
};
