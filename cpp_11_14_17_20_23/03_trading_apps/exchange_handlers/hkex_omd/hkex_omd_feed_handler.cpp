#include "hkex_omd_feed_handler.hpp"
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

namespace hkex::omd {

// Ultra-low latency HKEX OMD Market Data Feed Handler Implementation
class HKEXOMDFeedHandler : public IOMDPlugin {
private:
    // Configuration
    MDSessionConfig config_;

    // Network
    int multicast_socket_fd_;
    int retransmission_socket_fd_;
    bool is_connected_;

    // Threading
    std::thread receive_thread_;
    std::thread processing_thread_;
    std::thread heartbeat_thread_;
    std::thread gap_fill_thread_;
    std::atomic<bool> should_stop_;

    // Lock-free message queues
    MDSPSCRingBuffer<std::vector<uint8_t>, 16384> raw_data_queue_;
    MDSPSCRingBuffer<std::vector<uint8_t>, 8192> processed_data_queue_;

    // Memory pools
    MDMessagePool<AddOrderMessage, 2048> add_order_pool_;
    MDMessagePool<ModifyOrderMessage, 2048> modify_order_pool_;
    MDMessagePool<DeleteOrderMessage, 2048> delete_order_pool_;
    MDMessagePool<TradeMessage, 2048> trade_pool_;
    MDMessagePool<std::vector<uint8_t>, 4096> buffer_pool_;

    // Order book management
    std::unordered_map<uint32_t, OrderBook> order_books_;
    std::unordered_map<uint64_t, uint32_t> order_id_to_security_;  // Order ID -> Security Code
    std::mutex order_books_mutex_;

    // Subscription management
    std::set<uint32_t> subscribed_securities_;
    std::mutex subscription_mutex_;
    bool subscribe_all_;

    // Sequence number management
    std::atomic<uint32_t> expected_seq_num_{1};
    std::atomic<uint32_t> last_received_seq_num_{0};
    std::set<uint32_t> missing_seq_numbers_;
    std::mutex gap_fill_mutex_;

    // Statistics
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> messages_processed_{0};
    std::atomic<uint64_t> sequence_errors_{0};
    std::atomic<uint64_t> packets_dropped_{0};
    std::atomic<uint64_t> heartbeats_received_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
    std::atomic<uint64_t> latency_samples_{0};

    // Event handler
    std::shared_ptr<IOMDEventHandler> event_handler_;

    // Session state
    uint64_t last_heartbeat_time_;
    uint64_t session_start_time_;

public:
    HKEXOMDFeedHandler() : multicast_socket_fd_(-1), retransmission_socket_fd_(-1),
                           is_connected_(false), should_stop_(false), subscribe_all_(false),
                           last_heartbeat_time_(0), session_start_time_(0) {}

    ~HKEXOMDFeedHandler() {
        shutdown();
    }

    const char* getPluginName() const override {
        return "HKEX_OMD_FeedHandler";
    }

    const char* getPluginVersion() const override {
        return "3.5.1";
    }

    bool isReady() const override {
        return is_connected_;
    }

    bool initialize(const std::string& config_json) override {
        // Parse configuration (simplified - in production use proper JSON parser)
        config_.network.multicast_ip = "233.54.12.1";        // HKEX OMD Multicast
        config_.network.multicast_port = 16900;
        config_.network.interface_ip = "192.168.1.100";      // Local interface
        config_.network.retransmission_ip = "203.194.103.60"; // HKEX Retransmission
        config_.network.retransmission_port = 18900;
        config_.network.enable_retransmission = true;
        config_.network.receive_buffer_size = 1048576;       // 1MB
        config_.network.socket_timeout_ms = 1000;
        config_.network.enable_timestamping = true;
        config_.network.enable_packet_filtering = true;

        config_.session_id = "OMD_SESSION_001";
        config_.max_gap_fill_requests = 100;
        config_.heartbeat_interval_ms = 30000;               // 30 seconds
        config_.enable_sequence_checking = true;
        config_.enable_market_data_replay = false;
        config_.enable_order_book_building = true;
        config_.enable_statistics_calculation = true;
        config_.max_order_book_levels = 10;
        config_.enable_latency_measurement = true;

        session_start_time_ = MDTimestampUtils::getNanosecondTimestamp();

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

        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }

        if (gap_fill_thread_.joinable()) {
            gap_fill_thread_.join();
        }

        if (multicast_socket_fd_ >= 0) {
            close(multicast_socket_fd_);
            multicast_socket_fd_ = -1;
        }

        if (retransmission_socket_fd_ >= 0) {
            close(retransmission_socket_fd_);
            retransmission_socket_fd_ = -1;
        }

        is_connected_ = false;
    }

    bool connect() override {
        if (!connectMulticast()) {
            std::cerr << "Failed to connect to multicast feed" << std::endl;
            return false;
        }

        if (config_.network.enable_retransmission && !connectRetransmission()) {
            std::cerr << "Failed to connect to retransmission service" << std::endl;
        }

        // Start worker threads
        startThreads();

        is_connected_ = true;
        std::cout << "HKEX OMD feed handler connected successfully" << std::endl;
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

    bool subscribe(uint32_t security_code) override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribed_securities_.insert(security_code);

        // Initialize order book if needed
        if (config_.enable_order_book_building) {
            std::lock_guard<std::mutex> ob_lock(order_books_mutex_);
            if (order_books_.find(security_code) == order_books_.end()) {
                order_books_[security_code] = OrderBook();
                order_books_[security_code].security_code = security_code;
            }
        }

        std::cout << "Subscribed to security: " << security_code << std::endl;
        return true;
    }

    bool unsubscribe(uint32_t security_code) override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribed_securities_.erase(security_code);

        std::cout << "Unsubscribed from security: " << security_code << std::endl;
        return true;
    }

    bool subscribeAll() override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribe_all_ = true;
        std::cout << "Subscribed to all securities" << std::endl;
        return true;
    }

    bool unsubscribeAll() override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        subscribe_all_ = false;
        subscribed_securities_.clear();
        std::cout << "Unsubscribed from all securities" << std::endl;
        return true;
    }

    const OrderBook* getOrderBook(uint32_t security_code) const override {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto it = order_books_.find(security_code);
        return (it != order_books_.end()) ? &it->second : nullptr;
    }

    std::vector<uint32_t> getSubscribedSecurities() const override {
        std::lock_guard<std::mutex> lock(subscription_mutex_);
        return std::vector<uint32_t>(subscribed_securities_.begin(), subscribed_securities_.end());
    }

    void registerEventHandler(std::shared_ptr<IOMDEventHandler> handler) override {
        event_handler_ = handler;
    }

    void unregisterEventHandler() override {
        event_handler_.reset();
    }

    // Statistics
    uint64_t getMessagesReceived() const override { return messages_received_.load(); }
    uint64_t getMessagesProcessed() const override { return messages_processed_.load(); }
    uint64_t getSequenceErrors() const override { return sequence_errors_.load(); }
    uint64_t getPacketsDropped() const override { return packets_dropped_.load(); }
    uint64_t getHeartbeatsReceived() const override { return heartbeats_received_.load(); }
    uint32_t getCurrentSequenceNumber() const override { return last_received_seq_num_.load(); }

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

        std::cout << "Connected to HKEX OMD multicast: "
                  << config_.network.multicast_ip << ":" << config_.network.multicast_port << std::endl;
        return true;
    }

    bool connectRetransmission() {
        retransmission_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (retransmission_socket_fd_ < 0) {
            std::cerr << "Failed to create retransmission socket: " << strerror(errno) << std::endl;
            return false;
        }

        // Connect to retransmission server
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.network.retransmission_port);
        inet_pton(AF_INET, config_.network.retransmission_ip.c_str(), &addr.sin_addr);

        if (connect(retransmission_socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "Failed to connect to retransmission server: " << strerror(errno) << std::endl;
            close(retransmission_socket_fd_);
            retransmission_socket_fd_ = -1;
            return false;
        }

        std::cout << "Connected to HKEX OMD retransmission: "
                  << config_.network.retransmission_ip << ":" << config_.network.retransmission_port << std::endl;
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

        // Start heartbeat monitoring thread (CPU core 2)
        heartbeat_thread_ = std::thread([this]() {
            setCPUAffinity(2);
            heartbeatThreadMain();
        });

        // Start gap fill thread (CPU core 3)
        gap_fill_thread_ = std::thread([this]() {
            setCPUAffinity(3);
            gapFillThreadMain();
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
            receive_timestamp = MDTimestampUtils::getNanosecondTimestamp();

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
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    }

    void processingThreadMain() {
        std::vector<uint8_t> packet;

        while (!should_stop_.load()) {
            if (raw_data_queue_.try_pop(packet)) {
                processRawPacket(packet);
            } else {
                // Yield CPU briefly when no data
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
    }

    void heartbeatThreadMain() {
        while (!should_stop_.load()) {
            if (is_connected_) {
                uint64_t current_time = MDTimestampUtils::getNanosecondTimestamp();
                uint64_t time_since_last = (current_time - last_heartbeat_time_) / 1000000; // Convert to ms

                if (time_since_last >= config_.heartbeat_interval_ms * 2) {
                    // No heartbeat received for too long
                    std::cerr << "Heartbeat timeout detected" << std::endl;
                    handleDisconnection("Heartbeat timeout");
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Check every second
        }
    }

    void gapFillThreadMain() {
        while (!should_stop_.load()) {
            if (config_.enable_sequence_checking && !missing_seq_numbers_.empty()) {
                std::lock_guard<std::mutex> lock(gap_fill_mutex_);

                if (!missing_seq_numbers_.empty() && retransmission_socket_fd_ >= 0) {
                    requestGapFill(*missing_seq_numbers_.begin());
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void processRawPacket(const std::vector<uint8_t>& packet) {
        if (packet.size() < sizeof(uint64_t) + sizeof(PacketHeader)) {
            return; // Invalid packet
        }

        // Extract receive timestamp
        uint64_t receive_timestamp;
        std::memcpy(&receive_timestamp, packet.data(), sizeof(uint64_t));

        // Extract packet data
        const uint8_t* packet_data = packet.data() + sizeof(uint64_t);
        size_t packet_size = packet.size() - sizeof(uint64_t);

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(packet_data);

        // Validate packet size
        if (header->packet_size != packet_size) {
            std::cerr << "Invalid packet size" << std::endl;
            return;
        }

        // Check sequence number
        if (config_.enable_sequence_checking) {
            checkSequenceNumber(header->seq_num);
        }

        // Process messages in packet
        const uint8_t* msg_data = packet_data + sizeof(PacketHeader);
        size_t remaining_size = packet_size - sizeof(PacketHeader);

        for (uint8_t i = 0; i < header->msg_count && remaining_size > 0; ++i) {
            if (remaining_size < sizeof(MessageHeader)) {
                break;
            }

            const MessageHeader* msg_header = reinterpret_cast<const MessageHeader*>(msg_data);

            if (msg_header->msg_size > remaining_size) {
                std::cerr << "Invalid message size" << std::endl;
                break;
            }

            processMessage(msg_data, msg_header->msg_size, receive_timestamp);

            msg_data += msg_header->msg_size;
            remaining_size -= msg_header->msg_size;
        }

        messages_processed_.fetch_add(1);
    }

    void processMessage(const uint8_t* data, [[maybe_unused]] size_t size, uint64_t receive_timestamp) {
        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

        // Calculate latency if enabled
        if (config_.enable_latency_measurement) {
            uint64_t latency = receive_timestamp - header->send_time;
            total_latency_ns_.fetch_add(latency);
            latency_samples_.fetch_add(1);
        }

        // Check if we're subscribed to this security
        if (!isSubscribed(header->security_code)) {
            return;
        }

        MessageType msg_type = static_cast<MessageType>(header->msg_type);

        switch (msg_type) {
            case MessageType::ADD_ORDER:
                processAddOrder(reinterpret_cast<const AddOrderMessage*>(data));
                break;

            case MessageType::MODIFY_ORDER:
                processModifyOrder(reinterpret_cast<const ModifyOrderMessage*>(data));
                break;

            case MessageType::DELETE_ORDER:
                processDeleteOrder(reinterpret_cast<const DeleteOrderMessage*>(data));
                break;

            case MessageType::TRADE:
                processTrade(reinterpret_cast<const TradeMessage*>(data));
                break;

            case MessageType::TRADE_CANCEL:
                processTradeCancel(reinterpret_cast<const TradeCancelMessage*>(data));
                break;

            case MessageType::SECURITY_DEFINITION:
                processSecurityDefinition(reinterpret_cast<const SecurityDefinitionMessage*>(data));
                break;

            case MessageType::SECURITY_STATUS:
                processSecurityStatus(reinterpret_cast<const SecurityStatusMessage*>(data));
                break;

            case MessageType::STATISTICS:
                processStatistics(reinterpret_cast<const StatisticsMessage*>(data));
                break;

            case MessageType::INDEX_DATA:
                processIndexData(reinterpret_cast<const IndexDataMessage*>(data));
                break;

            case MessageType::MARKET_TURNOVER:
                processMarketTurnover(reinterpret_cast<const MarketTurnoverMessage*>(data));
                break;

            case MessageType::HEARTBEAT:
                processHeartbeat();
                break;

            case MessageType::SEQUENCE_RESET:
                processSequenceReset(header->security_code);
                break;

            default:
                std::cerr << "Unknown message type: " << static_cast<int>(header->msg_type) << std::endl;
                break;
        }
    }

    bool isSubscribed(uint32_t security_code) const {
        if (subscribe_all_) return true;

        std::lock_guard<std::mutex> lock(subscription_mutex_);
        return subscribed_securities_.find(security_code) != subscribed_securities_.end();
    }

    void processAddOrder(const AddOrderMessage* msg) {
        if (config_.enable_order_book_building) {
            updateOrderBook(msg);
        }

        // Track order ID to security mapping
        order_id_to_security_[msg->order_id] = msg->header.security_code;

        if (event_handler_) {
            event_handler_->onAddOrder(*msg);
        }
    }

    void processModifyOrder(const ModifyOrderMessage* msg) {
        if (config_.enable_order_book_building) {
            updateOrderBook(msg);
        }

        if (event_handler_) {
            event_handler_->onModifyOrder(*msg);
        }
    }

    void processDeleteOrder(const DeleteOrderMessage* msg) {
        if (config_.enable_order_book_building) {
            updateOrderBook(msg);
        }

        // Remove order ID tracking
        order_id_to_security_.erase(msg->order_id);

        if (event_handler_) {
            event_handler_->onDeleteOrder(*msg);
        }
    }

    void processTrade(const TradeMessage* msg) {
        if (config_.enable_order_book_building) {
            updateOrderBookWithTrade(msg);
        }

        if (event_handler_) {
            event_handler_->onTrade(*msg);
        }
    }

    void processTradeCancel(const TradeCancelMessage* msg) {
        if (event_handler_) {
            event_handler_->onTradeCancel(*msg);
        }
    }

    void processSecurityDefinition(const SecurityDefinitionMessage* msg) {
        if (event_handler_) {
            event_handler_->onSecurityDefinition(*msg);
        }
    }

    void processSecurityStatus(const SecurityStatusMessage* msg) {
        if (event_handler_) {
            event_handler_->onSecurityStatus(*msg);
        }
    }

    void processStatistics(const StatisticsMessage* msg) {
        if (event_handler_) {
            event_handler_->onStatistics(*msg);
        }
    }

    void processIndexData(const IndexDataMessage* msg) {
        if (event_handler_) {
            event_handler_->onIndexData(*msg);
        }
    }

    void processMarketTurnover(const MarketTurnoverMessage* msg) {
        if (event_handler_) {
            event_handler_->onMarketTurnover(*msg);
        }
    }

    void processHeartbeat() {
        last_heartbeat_time_ = MDTimestampUtils::getNanosecondTimestamp();
        heartbeats_received_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onHeartbeat();
        }
    }

    void processSequenceReset(uint32_t new_seq_num) {
        expected_seq_num_.store(new_seq_num);

        if (event_handler_) {
            event_handler_->onSequenceReset(new_seq_num);
        }
    }

    void checkSequenceNumber(uint32_t seq_num) {
        uint32_t expected = expected_seq_num_.load();

        if (seq_num == expected) {
            // Perfect sequence
            expected_seq_num_.store(expected + 1);
            last_received_seq_num_.store(seq_num);
        } else if (seq_num > expected) {
            // Gap detected
            std::lock_guard<std::mutex> lock(gap_fill_mutex_);
            for (uint32_t i = expected; i < seq_num; ++i) {
                missing_seq_numbers_.insert(i);
            }
            expected_seq_num_.store(seq_num + 1);
            last_received_seq_num_.store(seq_num);
            sequence_errors_.fetch_add(1);
        } else {
            // Out of order or duplicate - remove from missing set
            std::lock_guard<std::mutex> lock(gap_fill_mutex_);
            missing_seq_numbers_.erase(seq_num);
        }
    }

    void requestGapFill([[maybe_unused]] uint32_t start_seq_num) {
        // Implementation for gap fill request via retransmission service
        // This would send a request to the retransmission server
        std::cout << "Requesting gap fill for sequence: " << start_seq_num << std::endl;
    }

    void updateOrderBook(const AddOrderMessage* msg) {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto& book = order_books_[msg->header.security_code];

        // Add order logic would go here
        // This is a simplified version - full implementation would maintain price levels
        book.last_update_time = std::chrono::high_resolution_clock::now();
    }

    void updateOrderBook(const ModifyOrderMessage* msg) {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto& book = order_books_[msg->header.security_code];

        // Modify order logic would go here
        book.last_update_time = std::chrono::high_resolution_clock::now();
    }

    void updateOrderBook(const DeleteOrderMessage* msg) {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto& book = order_books_[msg->header.security_code];

        // Delete order logic would go here
        book.last_update_time = std::chrono::high_resolution_clock::now();
    }

    void updateOrderBookWithTrade(const TradeMessage* msg) {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto& book = order_books_[msg->header.security_code];

        book.last_trade_price = msg->price;
        book.last_trade_quantity = msg->quantity;
        book.total_volume += msg->quantity;
        book.total_turnover += msg->price * msg->quantity;
        book.last_update_time = std::chrono::high_resolution_clock::now();
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
std::unique_ptr<IOMDPlugin> createHKEXOMDPlugin() {
    return std::make_unique<HKEXOMDFeedHandler>();
}

} // namespace hkex::omd
