#include "ouch_asx_order_handler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#endif
#ifdef _MSC_VER
#include <immintrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <x86intrin.h>
#endif

namespace asx::ouch {

// Lock-free SPSC ring buffer for ultra-low latency message passing
template<typename T, size_t Size>
class SPSCRingBuffer {
private:
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

    alignas(64) std::array<T, Size> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

public:
    bool push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & (Size - 1);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }

        item = buffer_[current_head];
        head_.store((current_head + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    size_t size() const noexcept {
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t head = head_.load(std::memory_order_acquire);
        return (tail - head) & (Size - 1);
    }
};

// High-performance timestamp utility
class TimestampUtils {
public:
    static uint64_t getNanoseconds() noexcept {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }

    static uint64_t getRdtsc() noexcept {
        #if defined(__x86_64__) || defined(_M_X64)
        return __rdtsc();
        #else
        // Fallback for non-x86 architectures
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        #endif
    }

    // Cache line optimized timestamp for ultra-low latency
    static inline uint64_t getFastTimestamp() noexcept {
        return getRdtsc();
    }
};

// Ultra-low latency TCP socket wrapper
class FastSocket {
private:
    int socket_fd_;
    struct sockaddr_in server_addr_;
    bool connected_;

public:
    FastSocket() : socket_fd_(-1), connected_(false) {}

    ~FastSocket() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }

    bool connect(const NetworkConfig& config) {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            return false;
        }

        // Ultra-low latency socket optimizations
        int flag = 1;
        if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            close(socket_fd_);
            return false;
        }

        // Set socket buffer sizes
        int rcvbuf = config.so_rcvbuf_size;
        int sndbuf = config.so_sndbuf_size;
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        // Enable quick ACK (Linux specific, skip on macOS)
        #ifdef TCP_QUICKACK
        flag = 1;
        setsockopt(socket_fd_, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag));
        #endif

        // Prepare server address
        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(config.server_port);
        inet_pton(AF_INET, config.server_ip.c_str(), &server_addr_.sin_addr);

        // Connect to server
        if (::connect(socket_fd_, (struct sockaddr*)&server_addr_,
                     sizeof(server_addr_)) < 0) {
            close(socket_fd_);
            return false;
        }

        connected_ = true;
        return true;
    }

    ssize_t send(const void* data, size_t len) noexcept {
        if (!connected_) return -1;
        return ::send(socket_fd_, data, len, MSG_NOSIGNAL);
    }

    ssize_t recv(void* buffer, size_t len) noexcept {
        if (!connected_) return -1;
        return ::recv(socket_fd_, buffer, len, 0);
    }

    void disconnect() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        connected_ = false;
    }

    bool isConnected() const noexcept { return connected_; }
    int getFd() const noexcept { return socket_fd_; }
};

// Order token generator for ultra-low latency
class OrderTokenGenerator {
private:
    std::atomic<uint64_t> counter_{0};
    std::array<char, 4> prefix_;

public:
    OrderTokenGenerator(const std::string& firm_id) {
        std::fill(prefix_.begin(), prefix_.end(), '0');
        size_t copy_len = std::min(firm_id.length(), prefix_.size());
        std::copy(firm_id.begin(), firm_id.begin() + copy_len, prefix_.begin());
    }

    void generateToken(std::array<char, 14>& token) noexcept {
        uint64_t seq = counter_.fetch_add(1, std::memory_order_relaxed);

        // Format: FIRM + 10-digit sequence number
        std::copy(prefix_.begin(), prefix_.end(), token.begin());

        // Convert sequence to string (faster than sprintf)
        char* ptr = token.data() + 4;
        for (int i = 9; i >= 0; --i) {
            ptr[i] = '0' + (seq % 10);
            seq /= 10;
        }
    }
};

// Message builder for optimal performance
class MessageBuilder {
private:
    MessagePool<uint8_t, 4096> byte_pool_;

public:
    template<typename MessageType>
    MessageType* createMessage() noexcept {
        auto* buffer = byte_pool_.acquire();
        if (!buffer) return nullptr;

        auto* msg = reinterpret_cast<MessageType*>(buffer);
        memset(msg, 0, sizeof(MessageType));

        // Set common header fields
        msg->header.length = sizeof(MessageType);
        msg->header.timestamp = TimestampUtils::getFastTimestamp();

        return msg;
    }

    template<typename MessageType>
    void releaseMessage(MessageType* msg) noexcept {
        if (msg) {
            byte_pool_.release(reinterpret_cast<uint8_t*>(msg));
        }
    }
};

// ASX OUCH Order Handler Implementation
class ASXOUCHOrderHandler : public IOUCHPlugin {
private:
    // Configuration
    SessionConfig config_;

    // Network and session
    std::unique_ptr<FastSocket> socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};

    // Threading
    std::thread receive_thread_;
    std::thread send_thread_;

    // Message handling
    MessageBuilder message_builder_;
    OrderTokenGenerator token_generator_;
    SPSCRingBuffer<std::array<uint8_t, 1024>, 4096> send_queue_;
    SPSCRingBuffer<std::array<uint8_t, 1024>, 4096> receive_queue_;

    // Event handling
    std::shared_ptr<IOrderEventHandler> event_handler_;

    // Order tracking
    std::unordered_map<std::string, OrderInfo> orders_;
    std::mutex orders_mutex_;

    // Statistics
    alignas(64) std::atomic<uint64_t> orders_sent_{0};
    alignas(64) std::atomic<uint64_t> orders_accepted_{0};
    alignas(64) std::atomic<uint64_t> orders_rejected_{0};
    alignas(64) std::atomic<uint64_t> executions_{0};
    alignas(64) std::atomic<uint64_t> total_latency_ns_{0};
    alignas(64) std::atomic<uint64_t> latency_samples_{0};

    // Receive buffer for ultra-low latency
    alignas(64) std::array<uint8_t, 65536> receive_buffer_;
    size_t buffer_pos_{0};

public:
    ASXOUCHOrderHandler() : token_generator_("ASX1") {}

    ~ASXOUCHOrderHandler() {
        shutdown();
    }

    bool initialize(const std::string& config_json) override {
        // Parse configuration (simplified - in production use proper JSON parser)
        config_.network.server_ip = "203.0.113.10";  // ASX test environment
        config_.network.server_port = 8080;
        config_.network.so_rcvbuf_size = 65536;
        config_.network.so_sndbuf_size = 65536;
        config_.firm_id = "ASX1";
        config_.max_orders_per_second = 1000;  // Default rate limit
        config_.enable_order_tracking = true;

        socket_ = std::make_unique<FastSocket>();

        if (!socket_->connect(config_.network)) {
            return false;
        }

        connected_.store(true, std::memory_order_release);
        running_.store(true, std::memory_order_release);

        // Start worker threads
        receive_thread_ = std::thread(&ASXOUCHOrderHandler::receiveWorker, this);
        send_thread_ = std::thread(&ASXOUCHOrderHandler::sendWorker, this);

        // Set thread affinity for ultra-low latency (Linux specific)
        #ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(2, &cpuset);  // Bind receive thread to CPU 2
        pthread_setaffinity_np(receive_thread_.native_handle(),
                              sizeof(cpu_set_t), &cpuset);

        CPU_ZERO(&cpuset);
        CPU_SET(3, &cpuset);  // Bind send thread to CPU 3
        pthread_setaffinity_np(send_thread_.native_handle(),
                              sizeof(cpu_set_t), &cpuset);
        #elif defined(__APPLE__)
        // macOS thread affinity - use thread_policy_set
        // Note: macOS doesn't provide direct CPU affinity like Linux
        // This is a placeholder for macOS-specific optimizations
        #endif

        return true;
    }

    void shutdown() override {
        running_.store(false, std::memory_order_release);

        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }

        if (send_thread_.joinable()) {
            send_thread_.join();
        }

        if (socket_) {
            socket_->disconnect();
        }

        connected_.store(false, std::memory_order_release);
    }

    const char* getPluginName() const override {
        return "ASX OUCH Order Handler";
    }

    const char* getPluginVersion() const override {
        return "1.0.0";
    }

    bool isReady() const override {
        return connected_.load(std::memory_order_acquire) &&
               running_.load(std::memory_order_acquire);
    }

    bool sendEnterOrder(const EnterOrderMessage& order) override {
        if (!isReady()) return false;

        // Validate order parameters
        if (order.quantity == 0) {
            return false; // Invalid quantity
        }

        if (order.price == 0 && static_cast<OrderType>(order.header.message_type) == OrderType::LIMIT) {
            return false; // Invalid price for limit order
        }

        // Rate limiting check
        static thread_local uint64_t last_second = 0;
        static thread_local uint32_t orders_this_second = 0;

        uint64_t current_second = TimestampUtils::getFastTimestamp() / 1000000000ULL;
        if (current_second != last_second) {
            last_second = current_second;
            orders_this_second = 0;
        }

        if (++orders_this_second > config_.max_orders_per_second) {
            return false; // Rate limit exceeded
        }

        // Serialize message to send queue
        std::array<uint8_t, 1024> message_buffer;
        memcpy(message_buffer.data(), &order, sizeof(order));

        if (!send_queue_.push(message_buffer)) {
            return false; // Queue full
        }

        orders_sent_.fetch_add(1, std::memory_order_relaxed);

        // Track order if enabled
        if (config_.enable_order_tracking) {
            std::lock_guard<std::mutex> lock(orders_mutex_);

            OrderInfo info;
            info.order_token = order.order_token;
            info.instrument = order.instrument;
            info.side = order.side;
            info.original_quantity = order.quantity;
            info.remaining_quantity = order.quantity;
            info.executed_quantity = 0;
            info.price = order.price;
            info.state = OrderState::PENDING_NEW;
            info.submit_time = std::chrono::high_resolution_clock::now();

            std::string token_str(order.order_token.begin(), order.order_token.end());
            orders_[token_str] = info;
        }

        return true;
    }

    bool sendReplaceOrder(const ReplaceOrderMessage& replace) override {
        if (!isReady()) return false;

        std::array<uint8_t, 1024> message_buffer;
        memcpy(message_buffer.data(), &replace, sizeof(replace));

        return send_queue_.push(message_buffer);
    }

    bool sendCancelOrder(const CancelOrderMessage& cancel) override {
        if (!isReady()) return false;

        std::array<uint8_t, 1024> message_buffer;
        memcpy(message_buffer.data(), &cancel, sizeof(cancel));

        return send_queue_.push(message_buffer);
    }

    void registerEventHandler(std::shared_ptr<IOrderEventHandler> handler) override {
        event_handler_ = handler;
    }

    void unregisterEventHandler() override {
        event_handler_.reset();
    }

    uint64_t getOrdersSent() const override {
        return orders_sent_.load(std::memory_order_relaxed);
    }

    uint64_t getOrdersAccepted() const override {
        return orders_accepted_.load(std::memory_order_relaxed);
    }

    uint64_t getOrdersRejected() const override {
        return orders_rejected_.load(std::memory_order_relaxed);
    }

    uint64_t getExecutions() const override {
        return executions_.load(std::memory_order_relaxed);
    }

    double getAverageLatency() const override {
        uint64_t samples = latency_samples_.load(std::memory_order_relaxed);
        if (samples == 0) return 0.0;

        uint64_t total = total_latency_ns_.load(std::memory_order_relaxed);
        return static_cast<double>(total) / samples / 1000.0; // Return in microseconds
    }

private:
    void receiveWorker() {
        while (running_.load(std::memory_order_acquire)) {
            ssize_t bytes_received = socket_->recv(
                receive_buffer_.data() + buffer_pos_,
                receive_buffer_.size() - buffer_pos_);

            if (bytes_received <= 0) {
                if (bytes_received < 0 && errno == EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    continue;
                }
                break; // Connection error
            }

            buffer_pos_ += bytes_received;

            // Process complete messages
            size_t processed = 0;
            while (processed + sizeof(MessageHeader) <= buffer_pos_) {
                auto* header = reinterpret_cast<MessageHeader*>(
                    receive_buffer_.data() + processed);

                if (processed + header->length > buffer_pos_) {
                    break; // Incomplete message
                }

                processIncomingMessage(receive_buffer_.data() + processed,
                                     header->length);
                processed += header->length;
            }

            // Move remaining data to front of buffer
            if (processed > 0) {
                memmove(receive_buffer_.data(),
                       receive_buffer_.data() + processed,
                       buffer_pos_ - processed);
                buffer_pos_ -= processed;
            }
        }
    }

    void sendWorker() {
        std::array<uint8_t, 1024> message_buffer;

        while (running_.load(std::memory_order_acquire)) {
            if (send_queue_.pop(message_buffer)) {
                auto* header = reinterpret_cast<MessageHeader*>(message_buffer.data());

                // Add timestamp just before sending
                header->timestamp = TimestampUtils::getFastTimestamp();

                ssize_t sent = socket_->send(message_buffer.data(), header->length);
                if (sent != header->length) {
                    // Handle send error
                    break;
                }
            } else {
                // Queue empty, yield briefly
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
    }

    void processIncomingMessage(const uint8_t* data, [[maybe_unused]] size_t length) {
        uint64_t receive_time = TimestampUtils::getFastTimestamp();

        auto* header = reinterpret_cast<const MessageHeader*>(data);

        switch (static_cast<MessageType>(header->message_type)) {
            case MessageType::ORDER_ACCEPTED:
                processOrderAccepted(
                    reinterpret_cast<const OrderAcceptedMessage*>(data),
                    receive_time);
                break;

            case MessageType::ORDER_EXECUTED:
                processOrderExecuted(
                    reinterpret_cast<const OrderExecutedMessage*>(data),
                    receive_time);
                break;

            case MessageType::ORDER_REJECTED:
                processOrderRejected(
                    reinterpret_cast<const OrderRejectedMessage*>(data),
                    receive_time);
                break;

            default:
                // Handle other message types
                break;
        }
    }

    void processOrderAccepted(const OrderAcceptedMessage* msg, uint64_t receive_time) {
        orders_accepted_.fetch_add(1, std::memory_order_relaxed);

        // Calculate latency if we have the send timestamp
        uint64_t latency = receive_time - msg->header.timestamp;
        total_latency_ns_.fetch_add(latency, std::memory_order_relaxed);
        latency_samples_.fetch_add(1, std::memory_order_relaxed);

        // Update order state
        if (config_.enable_order_tracking) {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            std::string token_str(msg->order_token.begin(), msg->order_token.end());

            auto it = orders_.find(token_str);
            if (it != orders_.end()) {
                it->second.state = OrderState::ACCEPTED;
                it->second.order_reference_number = msg->order_reference_number;
                it->second.last_update_time = std::chrono::high_resolution_clock::now();
            }
        }

        // Notify event handler
        if (event_handler_) {
            event_handler_->onOrderAccepted(*msg);
        }
    }

    void processOrderExecuted(const OrderExecutedMessage* msg, [[maybe_unused]] uint64_t receive_time) {
        executions_.fetch_add(1, std::memory_order_relaxed);

        // Update order state
        if (config_.enable_order_tracking) {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            std::string token_str(msg->order_token.begin(), msg->order_token.end());

            auto it = orders_.find(token_str);
            if (it != orders_.end()) {
                it->second.executed_quantity += msg->executed_quantity;
                it->second.remaining_quantity -= msg->executed_quantity;

                if (it->second.remaining_quantity == 0) {
                    it->second.state = OrderState::FILLED;
                } else {
                    it->second.state = OrderState::PARTIALLY_FILLED;
                }

                it->second.last_update_time = std::chrono::high_resolution_clock::now();
            }
        }

        // Notify event handler
        if (event_handler_) {
            event_handler_->onOrderExecuted(*msg);
        }
    }

    void processOrderRejected(const OrderRejectedMessage* msg, [[maybe_unused]] uint64_t receive_time) {
        orders_rejected_.fetch_add(1, std::memory_order_relaxed);

        // Update order state
        if (config_.enable_order_tracking) {
            std::lock_guard<std::mutex> lock(orders_mutex_);
            std::string token_str(msg->order_token.begin(), msg->order_token.end());

            auto it = orders_.find(token_str);
            if (it != orders_.end()) {
                it->second.state = OrderState::REJECTED;
                it->second.last_update_time = std::chrono::high_resolution_clock::now();
            }
        }

        // Notify event handler
        if (event_handler_) {
            event_handler_->onOrderRejected(*msg);
        }
    }
};

// Plugin factory function
extern "C" IOUCHPlugin* createOUCHPlugin() {
    return new ASXOUCHOrderHandler();
}

extern "C" void destroyOUCHPlugin(IOUCHPlugin* plugin) {
    delete plugin;
}

} // namespace asx::ouch
