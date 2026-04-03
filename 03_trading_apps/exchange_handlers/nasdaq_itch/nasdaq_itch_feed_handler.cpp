#include "nasdaq_itch_feed_handler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <map>
#include <set>

namespace nasdaq::itch {

// Ultra-low latency NASDAQ ITCH Feed Handler Implementation
class NASDAQITCHFeedHandler : public IITCHPlugin {
private:
    // Configuration
    ITCHSessionConfig config_;

    // Network
    int multicast_socket_fd_;
    bool is_connected_;

    // Threading
    std::thread receive_thread_;
    std::thread processing_thread_;
    std::thread order_book_thread_;
    std::atomic<bool> should_stop_;

    // Lock-free message queues
    ITCHSPSCRingBuffer<std::vector<uint8_t>, 32768> raw_data_queue_;
    ITCHSPSCRingBuffer<std::vector<uint8_t>, 16384> processed_data_queue_;

    // Memory pools
    ITCHMessagePool<AddOrderMessage, 8192> add_order_pool_;
    ITCHMessagePool<OrderExecutedMessage, 4096> executed_pool_;
    ITCHMessagePool<TradeMessage, 4096> trade_pool_;
    ITCHMessagePool<std::vector<uint8_t>, 16384> buffer_pool_;

    // Order and book management
    std::unordered_map<std::string, OrderBook> order_books_;
    std::unordered_map<uint64_t, OrderInfo> orders_;      // Order ref -> Order info
    std::unordered_map<std::string, std::string> stock_directory_; // Symbol -> Details
    std::mutex order_books_mutex_;
    std::mutex orders_mutex_;

    // Subscription management
    std::set<std::string> subscribed_symbols_;
    std::mutex subscription_mutex_;
    bool subscribe_all_;

    // Statistics
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> messages_processed_{0};
    std::atomic<uint64_t> orders_tracked_{0};
    std::atomic<uint64_t> trades_processed_{0};
    std::atomic<uint64_t> packets_dropped_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
    std::atomic<uint64_t> latency_samples_{0};

    // Event handler
    std::shared_ptr<IITCHEventHandler> event_handler_;

    // Session state
    uint64_t session_start_time_;
    uint64_t last_message_time_;

public:
    NASDAQITCHFeedHandler() : multicast_socket_fd_(-1), is_connected_(false),
                              should_stop_(false), subscribe_all_(false),
                              session_start_time_(0), last_message_time_(0) {}

    ~NASDAQITCHFeedHandler() {
        shutdown();
    }

    const char* getPluginName() const override {
        return "NASDAQ_ITCH_FeedHandler";
    }

    const char* getPluginVersion() const override {
        return "5.0.1";
    }

    bool isReady() const override {
        return is_connected_;
    }

    bool initialize(const std::string& config_json) override {
        // Parse configuration (simplified - in production use proper JSON parser)
        config_.network.multicast_ip = "233.54.12.0";       // NASDAQ ITCH Multicast
        config_.network.multicast_port = 26400;
        config_.network.interface_ip = "192.168.1.100";     // Local interface
        config_.network.receive_buffer_size = 2097152;      // 2MB
        config_.network.socket_timeout_ms = 1000;
        config_.network.enable_timestamping = true;
        config_.network.enable_packet_filtering = true;
        config_.network.enable_mold_udp = true;

        config_.session_id = "ITCH_SESSION_001";
        config_.enable_order_book_building = true;
        config_.enable_statistics_calculation = true;
        config_.max_order_book_levels = 20;
        config_.enable_latency_measurement = true;
        config_.enable_message_recovery = false;
        config_.recovery_timeout_ms = 5000;

        session_start_time_ = ITCHTimestampUtils::getNanosecondTimestamp();

        return true;
    }

    void shutdown() override {
        should_stop_.store(true);

        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }

        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }

        if (order_book_thread_.joinable()) {
            order_book_thread_.join();
        }

        if (multicast_socket_fd_ >= 0) {
            close(multicast_socket_fd_);
            multicast_socket_fd_ = -1;
        }

        is_connected_ = false;
    }

    bool connect() override {
        if (!connectMulticast()) {
            std::cerr << "Failed to connect to ITCH multicast feed" << std::endl;
            return false;
        }

        // Start worker threads
        startThreads();

        is_connected_ = true;
        std::cout << "NASDAQ ITCH feed handler connected successfully" << std::endl;
        return true;
    }

    bool disconnect() override {
        should_stop_.store(true);
        is_connected_ = false;
        return true;
    }

    bool isConnected() const override {
        return is_connected_;
    }

    bool subscribe(const std::string& symbol) override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribed_symbols_.insert(symbol);

        // Initialize order book if needed
        if (config_.enable_order_book_building) {
            std::lock_guard<std::mutex> ob_lock(order_books_mutex_);
            if (order_books_.find(symbol) == order_books_.end()) {
                order_books_[symbol] = OrderBook();
                order_books_[symbol].stock = symbol;
            }
        }

        std::cout << "Subscribed to symbol: " << symbol << std::endl;
        return true;
    }

    bool unsubscribe(const std::string& symbol) override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribed_symbols_.erase(symbol);

        std::cout << "Unsubscribed from symbol: " << symbol << std::endl;
        return true;
    }

    bool subscribeAll() override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribe_all_ = true;
        std::cout << "Subscribed to all symbols" << std::endl;
        return true;
    }

    bool unsubscribeAll() override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribe_all_ = false;
        subscribed_symbols_.clear();
        std::cout << "Unsubscribed from all symbols" << std::endl;
        return true;
    }

    const OrderBook* getOrderBook(const std::string& symbol) const override {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto it = order_books_.find(symbol);
        return (it != order_books_.end()) ? &it->second : nullptr;
    }

    std::vector<std::string> getSubscribedSymbols() const override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        return std::vector<std::string>(subscribed_symbols_.begin(), subscribed_symbols_.end());
    }

    void registerEventHandler(std::shared_ptr<IITCHEventHandler> handler) override {
        event_handler_ = handler;
    }

    void unregisterEventHandler() override {
        event_handler_.reset();
    }

    // Statistics
    uint64_t getMessagesReceived() const override { return messages_received_.load(); }
    uint64_t getMessagesProcessed() const override { return messages_processed_.load(); }
    uint64_t getOrdersTracked() const override { return orders_tracked_.load(); }
    uint64_t getTradesProcessed() const override { return trades_processed_.load(); }
    uint64_t getPacketsDropped() const override { return packets_dropped_.load(); }

    double getAverageLatency() const override {
        uint64_t samples = latency_samples_.load();
        return samples > 0 ? static_cast<double>(total_latency_ns_.load()) / samples / 1000.0 : 0.0;
    }

private:
    bool connectMulticast() {
        multicast_socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (multicast_socket_fd_ < 0) {
            std::cerr << "Failed to create multicast socket: " << strerror(errno) << std::endl;
            return false;
        }

        // Set socket options for ultra-low latency
        int reuse = 1;
        if (setsockopt(multicast_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        }

        // Set receive buffer size
        if (setsockopt(multicast_socket_fd_, SOL_SOCKET, SO_RCVBUF,
                      &config_.network.receive_buffer_size, sizeof(config_.network.receive_buffer_size)) < 0) {
            std::cerr << "Failed to set SO_RCVBUF: " << strerror(errno) << std::endl;
        }

        // Enable hardware timestamping if available
        if (config_.network.enable_timestamping) {
            int timestamp_flags = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
            if (setsockopt(multicast_socket_fd_, SOL_SOCKET, SO_TIMESTAMPING,
                          &timestamp_flags, sizeof(timestamp_flags)) < 0) {
                std::cerr << "Hardware timestamping not available, using software timestamps" << std::endl;
            }
        }

        // Bind to multicast address
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(config_.network.multicast_port);

        if (bind(multicast_socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "Failed to bind multicast socket: " << strerror(errno) << std::endl;
            close(multicast_socket_fd_);
            return false;
        }

        // Join multicast group
        struct ip_mreq mreq{};
        inet_pton(AF_INET, config_.network.multicast_ip.c_str(), &mreq.imr_multiaddr);
        inet_pton(AF_INET, config_.network.interface_ip.c_str(), &mreq.imr_interface);

        if (setsockopt(multicast_socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            std::cerr << "Failed to join multicast group: " << strerror(errno) << std::endl;
            close(multicast_socket_fd_);
            return false;
        }

        std::cout << "Connected to NASDAQ ITCH multicast: "
                  << config_.network.multicast_ip << ":" << config_.network.multicast_port << std::endl;
        return true;
    }

    void startThreads() {
        should_stop_.store(false);

        // Start receive thread (CPU core 0)
        receive_thread_ = std::thread([this]() {
            setCPUAffinity(0);
            receiveThreadMain();
        });

        // Start processing thread (CPU core 1)
        processing_thread_ = std::thread([this]() {
            setCPUAffinity(1);
            processingThreadMain();
        });

        // Start order book thread (CPU core 2)
        order_book_thread_ = std::thread([this]() {
            setCPUAffinity(2);
            orderBookThreadMain();
        });
    }

    void setCPUAffinity([[maybe_unused]] int cpu_id) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(__APPLE__)
        // macOS doesn't support CPU affinity in the same way
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif
    }

    void receiveThreadMain() {
        std::array<uint8_t, 65536> buffer;
        uint64_t receive_timestamp;

        while (!should_stop_.load()) {
            if (!is_connected_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Receive multicast data
            ssize_t bytes_received = recv(multicast_socket_fd_, buffer.data(), buffer.size(), MSG_DONTWAIT);
            receive_timestamp = ITCHTimestampUtils::getNanosecondTimestamp();

            if (bytes_received > 0) {
                messages_received_.fetch_add(1);

                // Create timestamped packet
                std::vector<uint8_t> packet;
                packet.reserve(bytes_received + sizeof(uint64_t));

                // Add timestamp header
                const uint8_t* ts_bytes = reinterpret_cast<const uint8_t*>(&receive_timestamp);
                packet.insert(packet.end(), ts_bytes, ts_bytes + sizeof(uint64_t));

                // Add packet data
                packet.insert(packet.end(), buffer.data(), buffer.data() + bytes_received);

                // Push to processing queue
                if (!raw_data_queue_.try_push(packet)) {
                    packets_dropped_.fetch_add(1);
                }
            } else if (bytes_received == 0) {
                // Connection closed
                handleDisconnection("Multicast connection closed");
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // Real error
                handleDisconnection("Receive error: " + std::string(strerror(errno)));
                break;
            }

            // Yield CPU briefly
            std::this_thread::sleep_for(std::chrono::nanoseconds(50));
        }
    }

    void processingThreadMain() {
        std::vector<uint8_t> packet;

        while (!should_stop_.load()) {
            if (raw_data_queue_.try_pop(packet)) {
                processRawPacket(packet);
            } else {
                // Yield CPU briefly when no data
                std::this_thread::sleep_for(std::chrono::nanoseconds(50));
            }
        }
    }

    void orderBookThreadMain() {
        std::vector<uint8_t> message_data;

        while (!should_stop_.load()) {
            if (processed_data_queue_.try_pop(message_data)) {
                processOrderBookUpdate(message_data);
            } else {
                // Yield CPU briefly when no data
                std::this_thread::sleep_for(std::chrono::nanoseconds(50));
            }
        }
    }

    void processRawPacket(const std::vector<uint8_t>& packet) {
        if (packet.size() < sizeof(uint64_t) + sizeof(ITCHMessageHeader)) {
            return; // Invalid packet
        }

        // Extract receive timestamp
        uint64_t receive_timestamp;
        std::memcpy(&receive_timestamp, packet.data(), sizeof(uint64_t));

        // Extract packet data (handle MoldUDP64 if enabled)
        const uint8_t* packet_data = packet.data() + sizeof(uint64_t);
        size_t packet_size = packet.size() - sizeof(uint64_t);

        if (config_.network.enable_mold_udp) {
            // MoldUDP64 header processing
            if (packet_size < 20) return; // MoldUDP64 header is 20 bytes

            // Skip MoldUDP64 header for now (sequence number, message count, etc.)
            packet_data += 20;
            packet_size -= 20;
        }

        // Process ITCH messages in the packet
        size_t offset = 0;
        while (offset + sizeof(uint16_t) < packet_size) {
            // Read message length
            uint16_t msg_length;
            std::memcpy(&msg_length, packet_data + offset, sizeof(uint16_t));
            msg_length = ntohs(msg_length); // Convert from network byte order

            if (offset + msg_length > packet_size) {
                break; // Incomplete message
            }

            // Process individual ITCH message
            processITCHMessage(packet_data + offset, msg_length, receive_timestamp);

            offset += msg_length;
        }

        messages_processed_.fetch_add(1);
    }

    void processITCHMessage(const uint8_t* data, [[maybe_unused]] size_t length, uint64_t receive_timestamp) {
        if (length < sizeof(ITCHMessageHeader)) {
            return;
        }

        const ITCHMessageHeader* header = reinterpret_cast<const ITCHMessageHeader*>(data);

        // Calculate latency if enabled
        if (config_.enable_latency_measurement) {
            uint64_t message_timestamp = ITCHTimestampUtils::convertITCHTimestamp(header->timestamp);
            uint64_t latency = receive_timestamp - message_timestamp;
            total_latency_ns_.fetch_add(latency);
            latency_samples_.fetch_add(1);
        }

        MessageType msg_type = header->message_type;

        switch (msg_type) {
            case MessageType::SYSTEM_EVENT:
                processSystemEvent(reinterpret_cast<const SystemEventMessage*>(data));
                break;

            case MessageType::STOCK_DIRECTORY:
                processStockDirectory(reinterpret_cast<const StockDirectoryMessage*>(data));
                break;

            case MessageType::STOCK_TRADING_ACTION:
                processStockTradingAction(reinterpret_cast<const StockTradingActionMessage*>(data));
                break;

            case MessageType::ADD_ORDER:
                processAddOrder(reinterpret_cast<const AddOrderMessage*>(data));
                break;

            case MessageType::ADD_ORDER_WITH_MPID:
                processAddOrderWithMPID(reinterpret_cast<const AddOrderWithMPIDMessage*>(data));
                break;

            case MessageType::ORDER_EXECUTED:
                processOrderExecuted(reinterpret_cast<const OrderExecutedMessage*>(data));
                break;

            case MessageType::ORDER_EXECUTED_WITH_PRICE:
                processOrderExecutedWithPrice(reinterpret_cast<const OrderExecutedWithPriceMessage*>(data));
                break;

            case MessageType::ORDER_CANCEL:
                processOrderCancel(reinterpret_cast<const OrderCancelMessage*>(data));
                break;

            case MessageType::ORDER_DELETE:
                processOrderDelete(reinterpret_cast<const OrderDeleteMessage*>(data));
                break;

            case MessageType::ORDER_REPLACE:
                processOrderReplace(reinterpret_cast<const OrderReplaceMessage*>(data));
                break;

            case MessageType::TRADE_NON_CROSS:
                processTrade(reinterpret_cast<const TradeMessage*>(data));
                break;

            case MessageType::TRADE_CROSS:
                processCrossTrade(reinterpret_cast<const CrossTradeMessage*>(data));
                break;

            case MessageType::BROKEN_TRADE:
                processBrokenTrade(reinterpret_cast<const BrokenTradeMessage*>(data));
                break;

            case MessageType::NOII:
                processNOII(reinterpret_cast<const NOIIMessage*>(data));
                break;

            default:
                // std::cerr << "Unknown ITCH message type: " << static_cast<char>(msg_type) << std::endl;
                break;
        }

        last_message_time_ = receive_timestamp;
    }

    bool isSubscribed(const std::string& symbol) const {
        if (subscribe_all_) return true;

        std::lock_guard<std::mutex> lock(subscription_mutex_);
        return subscribed_symbols_.find(symbol) != subscribed_symbols_.end();
    }

    std::string extractStock(const std::array<char, 8>& stock_array) const {
        std::string stock(stock_array.data(), 8);
        // Remove trailing spaces
        stock.erase(stock.find_last_not_of(" \t\n\r\f\v") + 1);
        return stock;
    }

    void processSystemEvent(const SystemEventMessage* msg) {
        if (event_handler_) {
            event_handler_->onSystemEvent(*msg);
        }
    }

    void processStockDirectory(const StockDirectoryMessage* msg) {
        std::string stock = extractStock(msg->stock);

        // Store stock directory information
        stock_directory_[stock] = "Directory entry"; // Simplified

        if (event_handler_) {
            event_handler_->onStockDirectory(*msg);
        }
    }

    void processStockTradingAction(const StockTradingActionMessage* msg) {
        if (event_handler_) {
            event_handler_->onStockTradingAction(*msg);
        }
    }

    void processAddOrder(const AddOrderMessage* msg) {
        std::string stock = extractStock(msg->stock);

        if (!isSubscribed(stock)) return;

        // Track order
        OrderInfo order_info;
        order_info.order_reference_number = msg->order_reference_number;
        order_info.stock = stock;
        order_info.side = msg->buy_sell_indicator;
        order_info.original_shares = msg->shares;
        order_info.remaining_shares = msg->shares;
        order_info.price = msg->price;
        order_info.add_time = std::chrono::high_resolution_clock::now();

        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            orders_[msg->order_reference_number] = order_info;
        }

        orders_tracked_.fetch_add(1);

        // Queue for order book update
        if (config_.enable_order_book_building) {
            std::vector<uint8_t> msg_data(reinterpret_cast<const uint8_t*>(msg),
                                         reinterpret_cast<const uint8_t*>(msg) + sizeof(AddOrderMessage));
            processed_data_queue_.try_push(msg_data);
        }

        if (event_handler_) {
            event_handler_->onAddOrder(*msg);
        }
    }

    void processAddOrderWithMPID(const AddOrderWithMPIDMessage* msg) {
        std::string stock = extractStock(msg->stock);

        if (!isSubscribed(stock)) return;

        // Similar to processAddOrder but with MPID
        OrderInfo order_info;
        order_info.order_reference_number = msg->order_reference_number;
        order_info.stock = stock;
        order_info.side = msg->buy_sell_indicator;
        order_info.original_shares = msg->shares;
        order_info.remaining_shares = msg->shares;
        order_info.price = msg->price;
        order_info.add_time = std::chrono::high_resolution_clock::now();

        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            orders_[msg->order_reference_number] = order_info;
        }

        orders_tracked_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onAddOrderWithMPID(*msg);
        }
    }

    void processOrderExecuted(const OrderExecutedMessage* msg) {
        // Update order information
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(msg->order_reference_number);
            if (it != orders_.end()) {
                it->second.remaining_shares -= msg->executed_shares;
            }
        }

        trades_processed_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onOrderExecuted(*msg);
        }
    }

    void processOrderExecutedWithPrice(const OrderExecutedWithPriceMessage* msg) {
        // Update order information
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(msg->order_reference_number);
            if (it != orders_.end()) {
                it->second.remaining_shares -= msg->executed_shares;
            }
        }

        trades_processed_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onOrderExecutedWithPrice(*msg);
        }
    }

    void processOrderCancel(const OrderCancelMessage* msg) {
        // Update order information
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(msg->order_reference_number);
            if (it != orders_.end()) {
                it->second.remaining_shares -= msg->cancelled_shares;
            }
        }

        if (event_handler_) {
            event_handler_->onOrderCancel(*msg);
        }
    }

    void processOrderDelete(const OrderDeleteMessage* msg) {
        // Remove order
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            orders_.erase(msg->order_reference_number);
        }

        if (event_handler_) {
            event_handler_->onOrderDelete(*msg);
        }
    }

    void processOrderReplace(const OrderReplaceMessage* msg) {
        // Update order information
        {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(msg->original_order_reference_number);
            if (it != orders_.end()) {
                OrderInfo new_order = it->second;
                new_order.order_reference_number = msg->new_order_reference_number;
                new_order.remaining_shares = msg->shares;
                new_order.price = msg->price;

                orders_.erase(it);
                orders_[msg->new_order_reference_number] = new_order;
            }
        }

        if (event_handler_) {
            event_handler_->onOrderReplace(*msg);
        }
    }

    void processTrade(const TradeMessage* msg) {
        std::string stock = extractStock(msg->stock);

        if (!isSubscribed(stock)) return;

        trades_processed_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onTrade(*msg);
        }
    }

    void processCrossTrade(const CrossTradeMessage* msg) {
        std::string stock = extractStock(msg->stock);

        if (!isSubscribed(stock)) return;

        trades_processed_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onCrossTrade(*msg);
        }
    }

    void processBrokenTrade(const BrokenTradeMessage* msg) {
        if (event_handler_) {
            event_handler_->onBrokenTrade(*msg);
        }
    }

    void processNOII(const NOIIMessage* msg) {
        std::string stock = extractStock(msg->stock);

        if (!isSubscribed(stock)) return;

        if (event_handler_) {
            event_handler_->onNOII(*msg);
        }
    }

    void processOrderBookUpdate([[maybe_unused]] const std::vector<uint8_t>& message_data) {
        // Simplified order book building - in production this would be much more sophisticated
        // This would reconstruct the order book from the message data

        // For demonstration purposes, we'll just update the last update time
        if (message_data.size() >= sizeof(AddOrderMessage)) {
            const AddOrderMessage* msg = reinterpret_cast<const AddOrderMessage*>(message_data.data());
            std::string stock = extractStock(msg->stock);

            std::lock_guard<std::mutex> lock(order_books_mutex_);
            auto& book = order_books_[stock];
            book.last_update_time = std::chrono::high_resolution_clock::now();

            // Simplified price level update
            if (msg->buy_sell_indicator == Side::BUY) {
                // Update bid levels
                bool found = false;
                for (auto& level : book.bid_levels) {
                    if (level.price == msg->price) {
                        level.shares += msg->shares;
                        level.order_count++;
                        found = true;
                        break;
                    }
                }
                if (!found && book.bid_levels.size() < config_.max_order_book_levels) {
                    book.bid_levels.emplace_back(msg->price, msg->shares, 1);
                    // Sort by price descending (best bid first)
                    std::sort(book.bid_levels.begin(), book.bid_levels.end(),
                             [](const PriceLevel& a, const PriceLevel& b) { return a.price > b.price; });
                }
            } else {
                // Update ask levels
                bool found = false;
                for (auto& level : book.ask_levels) {
                    if (level.price == msg->price) {
                        level.shares += msg->shares;
                        level.order_count++;
                        found = true;
                        break;
                    }
                }
                if (!found && book.ask_levels.size() < config_.max_order_book_levels) {
                    book.ask_levels.emplace_back(msg->price, msg->shares, 1);
                    // Sort by price ascending (best ask first)
                    std::sort(book.ask_levels.begin(), book.ask_levels.end(),
                             [](const PriceLevel& a, const PriceLevel& b) { return a.price < b.price; });
                }
            }
        }
    }

    void handleDisconnection(const std::string& reason) {
        is_connected_ = false;

        std::cerr << "Disconnected: " << reason << std::endl;

        if (event_handler_) {
            event_handler_->onDisconnect(reason);
        }
    }
};

// Plugin factory function
std::unique_ptr<IITCHPlugin> createNASDAQITCHPlugin() {
    return std::make_unique<NASDAQITCHFeedHandler>();
}

} // namespace nasdaq::itch
