#pragma once

#include "ouch_asx_order_handler.hpp"
#include <dlfcn.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

namespace asx::ouch {

// Plugin manager for dynamic loading of OUCH handlers
class OUCHPluginManager {
private:
    struct PluginInfo {
        void* handle;
        std::unique_ptr<IOUCHPlugin> plugin;
        std::string name;
        std::string version;
        std::string path;
    };

    std::unordered_map<std::string, PluginInfo> loaded_plugins_;

    // Function pointers for plugin factory functions
    typedef IOUCHPlugin* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(IOUCHPlugin*);

public:
    ~OUCHPluginManager() {
        unloadAllPlugins();
    }

    bool loadPlugin(const std::string& plugin_name, const std::string& library_path) {
        // Check if plugin already loaded
        if (loaded_plugins_.find(plugin_name) != loaded_plugins_.end()) {
            return false;
        }

        // Load the shared library
        void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
        if (!handle) {
            return false;
        }

        // Get factory functions
        CreatePluginFunc createPlugin = (CreatePluginFunc)dlsym(handle, "createOUCHPlugin");
        if (!createPlugin) {
            dlclose(handle);
            return false;
        }

        // Create plugin instance
        std::unique_ptr<IOUCHPlugin> plugin(createPlugin());
        if (!plugin) {
            dlclose(handle);
            return false;
        }

        // Store plugin info
        PluginInfo info;
        info.handle = handle;
        info.plugin = std::move(plugin);
        info.name = info.plugin->getPluginName();
        info.version = info.plugin->getPluginVersion();
        info.path = library_path;

        loaded_plugins_[plugin_name] = std::move(info);
        return true;
    }

    bool unloadPlugin(const std::string& plugin_name) {
        auto it = loaded_plugins_.find(plugin_name);
        if (it == loaded_plugins_.end()) {
            return false;
        }

        // Shutdown plugin
        it->second.plugin->shutdown();

        // Get destroy function
        DestroyPluginFunc destroyPlugin =
            (DestroyPluginFunc)dlsym(it->second.handle, "destroyOUCHPlugin");

        if (destroyPlugin) {
            destroyPlugin(it->second.plugin.release());
        }

        // Close library
        dlclose(it->second.handle);

        loaded_plugins_.erase(it);
        return true;
    }

    void unloadAllPlugins() {
        for (auto& [name, info] : loaded_plugins_) {
            info.plugin->shutdown();

            DestroyPluginFunc destroyPlugin =
                (DestroyPluginFunc)dlsym(info.handle, "destroyOUCHPlugin");

            if (destroyPlugin) {
                destroyPlugin(info.plugin.release());
            }

            dlclose(info.handle);
        }
        loaded_plugins_.clear();
    }

    IOUCHPlugin* getPlugin(const std::string& plugin_name) {
        auto it = loaded_plugins_.find(plugin_name);
        return (it != loaded_plugins_.end()) ? it->second.plugin.get() : nullptr;
    }

    std::vector<std::string> getLoadedPlugins() const {
        std::vector<std::string> names;
        names.reserve(loaded_plugins_.size());

        for (const auto& [name, info] : loaded_plugins_) {
            names.push_back(name);
        }

        return names;
    }

    bool initializePlugin(const std::string& plugin_name, const std::string& config) {
        auto* plugin = getPlugin(plugin_name);
        return plugin ? plugin->initialize(config) : false;
    }
};

// Order builder utility for easy order creation
class OrderBuilder {
private:
    EnterOrderMessage order_;

public:
    OrderBuilder() {
        memset(&order_, 0, sizeof(order_));
        order_.header.length = sizeof(EnterOrderMessage);
        order_.header.message_type = static_cast<uint8_t>(MessageType::ENTER_ORDER);
    }

    OrderBuilder& setOrderToken(const std::string& token) {
        size_t copy_len = std::min(token.length(), order_.order_token.size());
        std::copy(token.begin(), token.begin() + copy_len, order_.order_token.begin());
        return *this;
    }

    OrderBuilder& setSide(Side side) {
        order_.side = side;
        return *this;
    }

    OrderBuilder& setQuantity(uint32_t quantity) {
        order_.quantity = quantity;
        return *this;
    }

    OrderBuilder& setInstrument(const std::string& instrument) {
        size_t copy_len = std::min(instrument.length(), order_.instrument.size());
        std::copy(instrument.begin(), instrument.begin() + copy_len, order_.instrument.begin());
        return *this;
    }

    OrderBuilder& setPrice(uint64_t price) {
        order_.price = price;
        return *this;
    }

    OrderBuilder& setTimeInForce(TimeInForce tif) {
        order_.time_in_force = tif;
        return *this;
    }

    OrderBuilder& setFirm(const std::string& firm) {
        size_t copy_len = std::min(firm.length(), order_.firm.size());
        std::copy(firm.begin(), firm.begin() + copy_len, order_.firm.begin());
        return *this;
    }

    OrderBuilder& setDisplay(uint8_t display) {
        order_.display = display;
        return *this;
    }

    OrderBuilder& setMinimumQuantity(uint64_t min_qty) {
        order_.minimum_quantity = min_qty;
        return *this;
    }

    const EnterOrderMessage& build() const {
        return order_;
    }
};

// Performance monitor for ultra-low latency applications
class PerformanceMonitor {
private:
    struct LatencyStats {
        uint64_t min_ns = UINT64_MAX;
        uint64_t max_ns = 0;
        uint64_t sum_ns = 0;
        uint64_t count = 0;
        double avg_ns = 0.0;

        void update(uint64_t latency_ns) {
            min_ns = std::min(min_ns, latency_ns);
            max_ns = std::max(max_ns, latency_ns);
            sum_ns += latency_ns;
            count++;
            avg_ns = static_cast<double>(sum_ns) / count;
        }

        void reset() {
            min_ns = UINT64_MAX;
            max_ns = 0;
            sum_ns = 0;
            count = 0;
            avg_ns = 0.0;
        }
    };

    LatencyStats order_latency_;
    LatencyStats execution_latency_;
    std::atomic<uint64_t> orders_per_second_{0};
    std::atomic<uint64_t> executions_per_second_{0};

public:
    void recordOrderLatency(uint64_t latency_ns) {
        order_latency_.update(latency_ns);
    }

    void recordExecutionLatency(uint64_t latency_ns) {
        execution_latency_.update(latency_ns);
    }

    void incrementOrdersPerSecond() {
        orders_per_second_.fetch_add(1, std::memory_order_relaxed);
    }

    void incrementExecutionsPerSecond() {
        executions_per_second_.fetch_add(1, std::memory_order_relaxed);
    }

    double getAverageOrderLatencyMicros() const {
        return order_latency_.avg_ns / 1000.0;
    }

    double getMinOrderLatencyMicros() const {
        return order_latency_.min_ns / 1000.0;
    }

    double getMaxOrderLatencyMicros() const {
        return order_latency_.max_ns / 1000.0;
    }

    uint64_t getOrdersPerSecond() const {
        return orders_per_second_.load(std::memory_order_relaxed);
    }

    uint64_t getExecutionsPerSecond() const {
        return executions_per_second_.load(std::memory_order_relaxed);
    }

    void resetStats() {
        order_latency_.reset();
        execution_latency_.reset();
        orders_per_second_.store(0, std::memory_order_relaxed);
        executions_per_second_.store(0, std::memory_order_relaxed);
    }

    void printStats() const {
        std::cout << "Performance Statistics:\n";
        std::cout << "  Order Latency (μs): Min=" << getMinOrderLatencyMicros()
                  << ", Avg=" << getAverageOrderLatencyMicros()
                  << ", Max=" << getMaxOrderLatencyMicros() << "\n";
        std::cout << "  Orders/sec: " << getOrdersPerSecond() << "\n";
        std::cout << "  Executions/sec: " << getExecutionsPerSecond() << "\n";
    }
};

} // namespace asx::ouch
