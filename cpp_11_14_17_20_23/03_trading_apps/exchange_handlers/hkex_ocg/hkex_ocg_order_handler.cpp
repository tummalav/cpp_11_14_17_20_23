#include "hkex_ocg_order_handler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
#include <mutex>

namespace hkex::ocg {

// Ultra-low latency HKEX OCG-C Plugin Implementation
class HKEXOCGPlugin : public IOCGPlugin {
private:
    // Configuration
    SessionConfig config_;

    // Network
    int socket_fd_;
    bool is_connected_;
    bool is_logged_in_;

    // Threading
    std::thread receive_thread_;
    std::thread send_thread_;
    std::thread heartbeat_thread_;
    std::atomic<bool> should_stop_;

    // Lock-free message queues
    SPSCRingBuffer<std::vector<uint8_t>, 4096> send_queue_;
    SPSCRingBuffer<std::vector<uint8_t>, 4096> receive_queue_;

    // Memory pools
    MessagePool<NewOrderSingle, 1024> new_order_pool_;
    MessagePool<OrderCancelRequest, 1024> cancel_pool_;
    MessagePool<OrderReplaceRequest, 1024> replace_pool_;
    MessagePool<std::vector<uint8_t>, 2048> buffer_pool_;

    // Order tracking
    std::unordered_map<std::string, OrderInfo> orders_;
    std::mutex orders_mutex_;

    // Statistics
    std::atomic<uint64_t> orders_sent_{0};
    std::atomic<uint64_t> orders_accepted_{0};
    std::atomic<uint64_t> orders_rejected_{0};
    std::atomic<uint64_t> executions_{0};
    std::atomic<uint64_t> heartbeats_sent_{0};
    std::atomic<uint64_t> heartbeats_received_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
    std::atomic<uint64_t> latency_samples_{0};

    // Sequence numbers
    std::atomic<uint32_t> outbound_seq_num_{1};
    std::atomic<uint32_t> expected_inbound_seq_num_{1};

    // Event handler
    std::shared_ptr<IOCGEventHandler> event_handler_;

    // Session state
    uint16_t session_id_;
    uint64_t last_heartbeat_time_;

public:
    HKEXOCGPlugin() : socket_fd_(-1), is_connected_(false), is_logged_in_(false),
                      should_stop_(false), session_id_(0), last_heartbeat_time_(0) {}

    ~HKEXOCGPlugin() {
        shutdown();
    }

    const char* getPluginName() const override {
        return "HKEX_OCG_Plugin";
    }

    const char* getPluginVersion() const override {
        return "4.9.1";
    }

    bool isReady() const override {
        return is_connected_ && is_logged_in_;
    }

    bool initialize([[maybe_unused]] const std::string& config_json) override {
        // Parse configuration (simplified - in production use proper JSON parser)
        config_.network.primary_ip = "203.194.103.50";  // HKEX OCG-C Primary
        config_.network.primary_port = 15001;
        config_.network.backup_ip = "203.194.103.51";   // HKEX OCG-C Backup
        config_.network.backup_port = 15001;
        config_.network.so_rcvbuf_size = 262144;  // 256KB
        config_.network.so_sndbuf_size = 262144;  // 256KB
        config_.network.tcp_nodelay = 1;
        config_.network.enable_quick_ack = true;
        config_.network.enable_tcp_user_timeout = true;
        config_.network.tcp_user_timeout_ms = 5000;
        config_.network.username = "TESTUSER";
        config_.network.password = "TESTPASS";
        config_.network.firm_id = "HKEX";
        config_.network.client_id = "CLIENT001";
        config_.network.heartbeat_interval_ms = 30000;  // 30 seconds
        config_.max_orders_per_second = 10000;
        config_.enable_order_tracking = true;
        config_.enable_latency_tracking = true;

        return connectToExchange();
    }

    void shutdown() override {
        should_stop_.store(true);

        if (is_logged_in_) {
            logout();
        }

        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }

        if (send_thread_.joinable()) {
            send_thread_.join();
        }

        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }

        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }

        is_connected_ = false;
        is_logged_in_ = false;
    }

    bool login() override {
        if (!is_connected_) return false;

        LogonRequest logon{};

        // Fill message header
        logon.header.msg_length = sizeof(LogonRequest);
        logon.header.msg_type = static_cast<uint8_t>(MessageType::LOGON_REQUEST);
        logon.header.msg_cat = 0x01;  // Administrative
        logon.header.session_id = session_id_;
        logon.header.sequence_number = outbound_seq_num_.fetch_add(1);
        logon.header.sending_time = TimestampUtils::getNanosecondTimestamp();

        // Fill logon details
        std::strncpy(logon.username.data(), config_.network.username.c_str(), 16);
        std::strncpy(logon.password.data(), config_.network.password.c_str(), 16);
        std::strncpy(logon.firm_id.data(), config_.network.firm_id.c_str(), 8);
        std::strncpy(logon.client_id.data(), config_.network.client_id.c_str(), 32);
        logon.heartbeat_interval = config_.network.heartbeat_interval_ms / 1000;
        logon.reset_seq_num_flag = 1;  // Reset sequence numbers
        logon.encryption_method = 0;   // No encryption

        return sendMessage(reinterpret_cast<const uint8_t*>(&logon), sizeof(LogonRequest));
    }

    bool logout() override {
        if (!is_logged_in_) return false;

        MessageHeader logout_msg{};
        logout_msg.msg_length = sizeof(MessageHeader);
        logout_msg.msg_type = static_cast<uint8_t>(MessageType::LOGOUT_REQUEST);
        logout_msg.msg_cat = 0x01;
        logout_msg.session_id = session_id_;
        logout_msg.sequence_number = outbound_seq_num_.fetch_add(1);
        logout_msg.sending_time = TimestampUtils::getNanosecondTimestamp();

        bool result = sendMessage(reinterpret_cast<const uint8_t*>(&logout_msg), sizeof(MessageHeader));
        is_logged_in_ = false;
        return result;
    }

    bool isLoggedIn() const override {
        return is_logged_in_;
    }

    bool sendNewOrder(const NewOrderSingle& order) override {
        if (!isReady()) return false;

        // Validate order parameters
        if (order.order_qty == 0) {
            return false; // Invalid quantity
        }

        if (order.price == 0 && order.ord_type == OrderType::LIMIT) {
            return false; // Invalid price for limit order
        }

        // Rate limiting check
        static thread_local uint64_t last_second = 0;
        static thread_local uint32_t orders_this_second = 0;

        uint64_t current_second = TimestampUtils::getNanosecondTimestamp() / 1000000000ULL;
        if (current_second != last_second) {
            last_second = current_second;
            orders_this_second = 0;
        }

        if (++orders_this_second > config_.max_orders_per_second) {
            return false; // Rate limit exceeded
        }

        // Create message with proper header
        NewOrderSingle msg = order;
        msg.header.msg_length = sizeof(NewOrderSingle);
        msg.header.msg_type = static_cast<uint8_t>(MessageType::NEW_ORDER_SINGLE);
        msg.header.msg_cat = 0x02;  // Order Management
        msg.header.session_id = session_id_;
        msg.header.sequence_number = outbound_seq_num_.fetch_add(1);
        msg.header.sending_time = TimestampUtils::getNanosecondTimestamp();
        msg.transact_time = msg.header.sending_time;

        // Track order if enabled
        if (config_.enable_order_tracking) {
            OrderInfo order_info{};
            std::memcpy(order_info.cl_ord_id.data(), msg.cl_ord_id.data(), 20);
            std::memcpy(order_info.security_id.data(), msg.security_id.data(), 12);
            order_info.side = msg.side;
            order_info.original_qty = msg.order_qty;
            order_info.remaining_qty = msg.order_qty;
            order_info.executed_qty = 0;
            order_info.price = msg.price;
            order_info.status = OrderStatus::PENDING_NEW;
            order_info.submit_time = std::chrono::high_resolution_clock::now();

            std::lock_guard<std::mutex> lock(orders_mutex_);
            orders_[std::string(msg.cl_ord_id.data(), 20)] = order_info;
        }

        orders_sent_.fetch_add(1);
        return sendMessage(reinterpret_cast<const uint8_t*>(&msg), sizeof(NewOrderSingle));
    }

    bool sendCancelOrder(const OrderCancelRequest& cancel) override {
        if (!isReady()) return false;

        OrderCancelRequest msg = cancel;
        msg.header.msg_length = sizeof(OrderCancelRequest);
        msg.header.msg_type = static_cast<uint8_t>(MessageType::ORDER_CANCEL_REQUEST);
        msg.header.msg_cat = 0x02;
        msg.header.session_id = session_id_;
        msg.header.sequence_number = outbound_seq_num_.fetch_add(1);
        msg.header.sending_time = TimestampUtils::getNanosecondTimestamp();
        msg.transact_time = msg.header.sending_time;

        return sendMessage(reinterpret_cast<const uint8_t*>(&msg), sizeof(OrderCancelRequest));
    }

    bool sendReplaceOrder(const OrderReplaceRequest& replace) override {
        if (!isReady()) return false;

        OrderReplaceRequest msg = replace;
        msg.header.msg_length = sizeof(OrderReplaceRequest);
        msg.header.msg_type = static_cast<uint8_t>(MessageType::ORDER_REPLACE_REQUEST);
        msg.header.msg_cat = 0x02;
        msg.header.session_id = session_id_;
        msg.header.sequence_number = outbound_seq_num_.fetch_add(1);
        msg.header.sending_time = TimestampUtils::getNanosecondTimestamp();
        msg.transact_time = msg.header.sending_time;

        return sendMessage(reinterpret_cast<const uint8_t*>(&msg), sizeof(OrderReplaceRequest));
    }

    void registerEventHandler(std::shared_ptr<IOCGEventHandler> handler) override {
        event_handler_ = handler;
    }

    void unregisterEventHandler() override {
        event_handler_.reset();
    }

    // Statistics
    uint64_t getOrdersSent() const override { return orders_sent_.load(); }
    uint64_t getOrdersAccepted() const override { return orders_accepted_.load(); }
    uint64_t getOrdersRejected() const override { return orders_rejected_.load(); }
    uint64_t getExecutions() const override { return executions_.load(); }
    uint64_t getHeartbeatsSent() const override { return heartbeats_sent_.load(); }
    uint64_t getHeartbeatsReceived() const override { return heartbeats_received_.load(); }

    double getAverageLatency() const override {
        uint64_t samples = latency_samples_.load();
        return samples > 0 ? static_cast<double>(total_latency_ns_.load()) / samples / 1000.0 : 0.0;
    }

private:
    bool connectToExchange() {
        // Try primary first, then backup
        if (connectToServer(config_.network.primary_ip, config_.network.primary_port)) {
            std::cout << "Connected to HKEX OCG-C Primary: "
                      << config_.network.primary_ip << ":" << config_.network.primary_port << std::endl;
        } else if (connectToServer(config_.network.backup_ip, config_.network.backup_port)) {
            std::cout << "Connected to HKEX OCG-C Backup: "
                      << config_.network.backup_ip << ":" << config_.network.backup_port << std::endl;
        } else {
            std::cerr << "Failed to connect to HKEX OCG-C servers" << std::endl;
            return false;
        }

        // Start worker threads
        startThreads();

        return true;
    }

    bool connectToServer(const std::string& ip, uint16_t port) {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }

        // Ultra-low latency socket optimizations
        int flag = 1;

        // Disable Nagle's algorithm
        if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            std::cerr << "Failed to set TCP_NODELAY: " << strerror(errno) << std::endl;
        }

        // Enable quick ACK (Linux only)
#ifdef TCP_QUICKACK
        if (config_.network.enable_quick_ack) {
            if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag)) < 0) {
                std::cerr << "Failed to set TCP_QUICKACK: " << strerror(errno) << std::endl;
            }
        }
#endif

        // Set TCP user timeout (Linux only)
#ifdef TCP_USER_TIMEOUT
        if (config_.network.enable_tcp_user_timeout) {
            unsigned int timeout = config_.network.tcp_user_timeout_ms;
            if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout)) < 0) {
                std::cerr << "Failed to set TCP_USER_TIMEOUT: " << strerror(errno) << std::endl;
            }
        }
#endif

        // Set receive buffer size
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF,
                      &config_.network.so_rcvbuf_size, sizeof(config_.network.so_rcvbuf_size)) < 0) {
            std::cerr << "Failed to set SO_RCVBUF: " << strerror(errno) << std::endl;
        }

        // Set send buffer size
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF,
                      &config_.network.so_sndbuf_size, sizeof(config_.network.so_sndbuf_size)) < 0) {
            std::cerr << "Failed to set SO_SNDBUF: " << strerror(errno) << std::endl;
        }

        // Connect to server
        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

        if (connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
            std::cerr << "Failed to connect to " << ip << ":" << port << " - " << strerror(errno) << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        is_connected_ = true;
        return true;
    }

    void startThreads() {
        should_stop_.store(false);

        // Start receive thread (CPU core 0)
        receive_thread_ = std::thread([this]() {
            setCPUAffinity(0);
            receiveThreadMain();
        });

        // Start send thread (CPU core 1)
        send_thread_ = std::thread([this]() {
            setCPUAffinity(1);
            sendThreadMain();
        });

        // Start heartbeat thread (CPU core 2)
        heartbeat_thread_ = std::thread([this]() {
            setCPUAffinity(2);
            heartbeatThreadMain();
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
        // Use thread priorities instead
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif
    }

    void receiveThreadMain() {
        std::array<uint8_t, 65536> buffer;

        while (!should_stop_.load()) {
            if (!is_connected_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Non-blocking receive
            ssize_t bytes_received = recv(socket_fd_, buffer.data(), buffer.size(), MSG_DONTWAIT);

            if (bytes_received > 0) {
                processIncomingData(buffer.data(), static_cast<size_t>(bytes_received));
            } else if (bytes_received == 0) {
                // Connection closed
                handleDisconnection("Connection closed by peer");
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

    void sendThreadMain() {
        std::vector<uint8_t> message;

        while (!should_stop_.load()) {
            if (!is_connected_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Process send queue
            if (send_queue_.try_pop(message)) {
                ssize_t bytes_sent = send(socket_fd_, message.data(), message.size(), MSG_NOSIGNAL);

                if (bytes_sent < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        handleDisconnection("Send error: " + std::string(strerror(errno)));
                        break;
                    }
                } else if (static_cast<size_t>(bytes_sent) != message.size()) {
                    std::cerr << "Partial send: " << bytes_sent << "/" << message.size() << std::endl;
                }
            }

            // Yield CPU briefly
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    }

    void heartbeatThreadMain() {
        while (!should_stop_.load()) {
            if (is_logged_in_) {
                uint64_t current_time = TimestampUtils::getNanosecondTimestamp();
                uint64_t time_since_last = (current_time - last_heartbeat_time_) / 1000000; // Convert to ms

                if (time_since_last >= config_.network.heartbeat_interval_ms) {
                    sendHeartbeat();
                    last_heartbeat_time_ = current_time;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Check every second
        }
    }

    bool sendMessage(const uint8_t* data, size_t length) {
        std::vector<uint8_t> message(data, data + length);
        return send_queue_.try_push(message);
    }

    void sendHeartbeat() {
        MessageHeader heartbeat{};
        heartbeat.msg_length = sizeof(MessageHeader);
        heartbeat.msg_type = static_cast<uint8_t>(MessageType::HEARTBEAT);
        heartbeat.msg_cat = 0x01;
        heartbeat.session_id = session_id_;
        heartbeat.sequence_number = outbound_seq_num_.fetch_add(1);
        heartbeat.sending_time = TimestampUtils::getNanosecondTimestamp();

        if (sendMessage(reinterpret_cast<const uint8_t*>(&heartbeat), sizeof(MessageHeader))) {
            heartbeats_sent_.fetch_add(1);
        }
    }

    void processIncomingData(const uint8_t* data, size_t length) {
        size_t offset = 0;

        while (offset + sizeof(MessageHeader) <= length) {
            const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data + offset);

            if (offset + header->msg_length > length) {
                // Incomplete message, need to buffer
                break;
            }

            processMessage(data + offset, header->msg_length);
            offset += header->msg_length;
        }
    }

    void processMessage(const uint8_t* data, [[maybe_unused]] size_t length) {
        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);
        uint64_t receive_time = TimestampUtils::getNanosecondTimestamp();

        // Validate sequence number
        if (header->sequence_number != expected_inbound_seq_num_.load()) {
            std::cerr << "Sequence number gap. Expected: " << expected_inbound_seq_num_.load()
                      << ", Received: " << header->sequence_number << std::endl;
            // Handle sequence number recovery
        }
        expected_inbound_seq_num_.store(header->sequence_number + 1);

        MessageType msg_type = static_cast<MessageType>(header->msg_type);

        switch (msg_type) {
            case MessageType::LOGON_RESPONSE:
                processLogonResponse(data, receive_time);
                break;

            case MessageType::LOGOUT_RESPONSE:
                processLogoutResponse(data, receive_time);
                break;

            case MessageType::EXECUTION_REPORT:
                processExecutionReport(reinterpret_cast<const ExecutionReport*>(data), receive_time);
                break;

            case MessageType::ORDER_CANCEL_REJECT:
                processOrderCancelReject(reinterpret_cast<const OrderCancelReject*>(data), receive_time);
                break;

            case MessageType::BUSINESS_MESSAGE_REJECT_RESPONSE:
                processBusinessReject(data, receive_time);
                break;

            case MessageType::HEARTBEAT:
                processHeartbeat(receive_time);
                break;

            case MessageType::TEST_REQUEST:
                processTestRequest(data, receive_time);
                break;

            default:
                std::cerr << "Unknown message type: " << static_cast<int>(header->msg_type) << std::endl;
                break;
        }
    }

    void processLogonResponse(const uint8_t* data, [[maybe_unused]] uint64_t receive_time) {
        // Parse logon response (simplified)
        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

        // In real implementation, parse the full logon response message
        // For now, assume success if we receive a logon response
        is_logged_in_ = true;
        session_id_ = header->session_id;

        if (event_handler_) {
            event_handler_->onLogonResponse(true, "Login successful");
        }

        std::cout << "Login successful. Session ID: " << session_id_ << std::endl;
    }

    void processLogoutResponse([[maybe_unused]] const uint8_t* data, [[maybe_unused]] uint64_t receive_time) {
        is_logged_in_ = false;
        std::cout << "Logout acknowledged" << std::endl;
    }

    void processExecutionReport(const ExecutionReport* exec_report, [[maybe_unused]] uint64_t receive_time) {
        executions_.fetch_add(1);

        // Calculate latency if order tracking is enabled
        if (config_.enable_latency_tracking) {
            std::string cl_ord_id(exec_report->cl_ord_id.data(), 20);

            std::lock_guard<std::mutex> lock(orders_mutex_);
            auto it = orders_.find(cl_ord_id);
            if (it != orders_.end()) {
                auto& order_info = it->second;

                // Update order info
                std::memcpy(order_info.order_id.data(), exec_report->order_id.data(), 20);
                order_info.status = exec_report->ord_status;
                order_info.executed_qty = exec_report->cum_qty;
                order_info.remaining_qty = order_info.original_qty - exec_report->cum_qty;
                order_info.avg_px = exec_report->avg_px;
                order_info.last_update_time = std::chrono::high_resolution_clock::now();

                // Calculate latency
                auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    order_info.last_update_time - order_info.submit_time).count();

                total_latency_ns_.fetch_add(latency_ns);
                latency_samples_.fetch_add(1);
            }
        }

        // Update statistics
        if (exec_report->ord_status == OrderStatus::NEW || exec_report->ord_status == OrderStatus::ACCEPTED_FOR_BIDDING) {
            orders_accepted_.fetch_add(1);
        }

        if (event_handler_) {
            event_handler_->onExecutionReport(*exec_report);
        }
    }

    void processOrderCancelReject(const OrderCancelReject* cancel_reject, [[maybe_unused]] uint64_t receive_time) {
        if (event_handler_) {
            event_handler_->onOrderCancelReject(*cancel_reject);
        }
    }

    void processBusinessReject([[maybe_unused]] const uint8_t* data, [[maybe_unused]] uint64_t receive_time) {
        orders_rejected_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onBusinessReject("Business message rejected");
        }
    }

    void processHeartbeat([[maybe_unused]] uint64_t receive_time) {
        heartbeats_received_.fetch_add(1);

        if (event_handler_) {
            event_handler_->onHeartbeat();
        }
    }

    void processTestRequest([[maybe_unused]] const uint8_t* data, [[maybe_unused]] uint64_t receive_time) {
        // Respond to test request with heartbeat
        sendHeartbeat();
    }

    void handleDisconnection(const std::string& reason) {
        is_connected_ = false;
        is_logged_in_ = false;

        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }

        std::cerr << "Disconnected: " << reason << std::endl;

        if (event_handler_) {
            event_handler_->onDisconnect(reason);
        }

        // TODO: Implement automatic reconnection logic
    }
};

// Plugin factory function
std::unique_ptr<IOCGPlugin> createHKEXOCGPlugin() {
    return std::make_unique<HKEXOCGPlugin>();
}

} // namespace hkex::ocg
