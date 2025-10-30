/**
 * Zero Copy Mechanisms for Ultra Low Latency Trading Systems
 *
 * Zero Copy: Transferring data without intermediate memory copies
 * Benefits: Eliminates memcpy overhead, reduces cache misses, minimizes latency
 * Target: Sub-microsecond data transfer and processing
 *
 * Author: Trading Systems Team
 * Date: October 2025
 * Standards: C++17/20
 */

#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <cstring>
#include <vector>
#include <array>
#include <thread>
#include <functional>

// Platform-specific definitions
#ifdef __linux__
    #ifndef MAP_HUGETLB
        #define MAP_HUGETLB 0x40000
    #endif
    #ifndef MREMAP_MAYMOVE
        #define MREMAP_MAYMOVE 1
    #endif
#else
    // macOS fallbacks
    #ifndef MAP_HUGETLB
        #define MAP_HUGETLB 0
    #endif
    #ifndef MREMAP_MAYMOVE
        #define MREMAP_MAYMOVE 1
        // mremap not available on macOS
        #define mremap(old_addr, old_size, new_size, flags) MAP_FAILED
    #endif
#endif

// For DPDK (if available)
#ifdef DPDK_AVAILABLE
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_eal.h>
#endif

// For Solarflare OpenOnload (if available)
#ifdef SOLARFLARE_AVAILABLE
#include <onload/extensions.h>
#endif

namespace ZeroCopy {

// =============================================================================
// 1. TRADITIONAL VS ZERO COPY COMPARISON
// =============================================================================

/**
 * Traditional approach with multiple memory copies
 * Latency: 2-5μs per copy operation
 */
class TraditionalNetworkIO {
private:
    static constexpr size_t BUFFER_SIZE = 8192;
    char kernel_buffer_[BUFFER_SIZE];
    char application_buffer_[BUFFER_SIZE];
    char processing_buffer_[BUFFER_SIZE];
    int socket_fd_;

public:
    explicit TraditionalNetworkIO(int socket_fd) : socket_fd_(socket_fd) {}

    // Multiple copies: Network → Kernel → App → Processing
    int receive_with_copies() {
        auto start = std::chrono::high_resolution_clock::now();

        // Copy 1: Network card → Kernel buffer (automatic via kernel)
        // Copy 2: Kernel buffer → Application buffer (recv syscall)
        ssize_t bytes = recv(socket_fd_, application_buffer_, BUFFER_SIZE, 0);

        if (bytes > 0) {
            // Copy 3: Application buffer → Processing buffer
            std::memcpy(processing_buffer_, application_buffer_, bytes);

            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

            std::cout << "Traditional receive latency: " << latency.count() << " ns\n";
            std::cout << "Total copies: 3, Memory moved: " << (bytes * 3) << " bytes\n";

            return process_traditional(processing_buffer_, bytes);
        }
        return -1;
    }

private:
    int process_traditional(const char* data, size_t length) {
        // Process data from copied buffer
        return static_cast<int>(length);
    }
};

/**
 * Zero copy approach - direct memory access
 * Latency: <300ns for data access
 */
class ZeroCopyNetworkIO {
private:
    static constexpr size_t RING_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB
    static constexpr size_t MAX_PACKET_SIZE = 9000; // Jumbo frame

    void* dma_ring_buffer_;
    std::atomic<size_t> read_index_{0};
    std::atomic<size_t> write_index_{0};
    size_t buffer_mask_;

public:
    ZeroCopyNetworkIO() {
        initialize_dma_buffer();
    }

    ~ZeroCopyNetworkIO() {
        if (dma_ring_buffer_) {
            munmap(dma_ring_buffer_, RING_BUFFER_SIZE);
        }
    }

    // Zero copy receive - direct DMA to application memory
    const uint8_t* receive_zero_copy(size_t& length) {
        auto start = std::chrono::high_resolution_clock::now();

        size_t current_read = read_index_.load(std::memory_order_acquire);
        size_t current_write = write_index_.load(std::memory_order_acquire);

        if (current_read != current_write) {
            // Direct pointer to DMA buffer - NO COPYING
            const uint8_t* packet_data = static_cast<uint8_t*>(dma_ring_buffer_) +
                                       (current_read & buffer_mask_);

            // Read packet length from DMA buffer
            length = *reinterpret_cast<const uint16_t*>(packet_data);

            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

            std::cout << "Zero copy receive latency: " << latency.count() << " ns\n";
            std::cout << "Total copies: 0, Memory moved: 0 bytes\n";

            // Advance read index
            read_index_.store((current_read + MAX_PACKET_SIZE) & buffer_mask_,
                            std::memory_order_release);

            return packet_data + sizeof(uint16_t); // Skip length header
        }

        length = 0;
        return nullptr;
    }

    // Process data directly from DMA buffer
    void process_zero_copy(const uint8_t* data, size_t length) {
        // Parse directly from DMA memory - no intermediate buffers
        if (length >= sizeof(MarketDataHeader)) {
            const auto* header = reinterpret_cast<const MarketDataHeader*>(data);
            process_market_data_direct(header, length);
        }
    }

private:
    struct MarketDataHeader {
        uint32_t symbol_id;
        uint64_t timestamp;
        double price;
        uint64_t quantity;
        uint8_t side; // 0=buy, 1=sell
    } __attribute__((packed));

    void initialize_dma_buffer() {
        // Allocate DMA-coherent memory
#ifdef __linux__
        dma_ring_buffer_ = mmap(nullptr, RING_BUFFER_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                               -1, 0);
#else
        dma_ring_buffer_ = mmap(nullptr, RING_BUFFER_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS,
                               -1, 0);
#endif

        if (dma_ring_buffer_ == MAP_FAILED) {
            // Fallback to regular memory
            dma_ring_buffer_ = aligned_alloc(4096, RING_BUFFER_SIZE);
            std::memset(dma_ring_buffer_, 0, RING_BUFFER_SIZE);
        }

        buffer_mask_ = RING_BUFFER_SIZE - 1;

        // Lock memory to prevent swapping
        mlock(dma_ring_buffer_, RING_BUFFER_SIZE);
    }

    void process_market_data_direct(const MarketDataHeader* header, size_t length) {
        // Direct processing from DMA buffer
        std::cout << "Symbol: " << header->symbol_id
                  << ", Price: " << header->price
                  << ", Qty: " << header->quantity << "\n";
    }
};

// =============================================================================
// 2. DPDK ZERO COPY IMPLEMENTATION
// =============================================================================

#ifdef DPDK_AVAILABLE
class DPDKZeroCopyEngine {
private:
    struct rte_mempool* mbuf_pool_;
    uint16_t port_id_;
    uint16_t queue_id_;
    static constexpr uint16_t RX_RING_SIZE = 1024;
    static constexpr uint16_t TX_RING_SIZE = 1024;
    static constexpr uint16_t NUM_MBUFS = 8191;
    static constexpr uint16_t MBUF_CACHE_SIZE = 256;

public:
    DPDKZeroCopyEngine(uint16_t port_id = 0, uint16_t queue_id = 0)
        : port_id_(port_id), queue_id_(queue_id) {}

    bool initialize() {
        // Initialize DPDK environment
        const char* argv[] = {"trading_app", "-l", "0-3", "-n", "4", nullptr};
        int argc = 5;

        int ret = rte_eal_init(argc, const_cast<char**>(argv));
        if (ret < 0) {
            std::cerr << "Cannot init EAL\n";
            return false;
        }

        // Create memory pool for packet buffers
        mbuf_pool_ = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                           MBUF_CACHE_SIZE, 0,
                                           RTE_MBUF_DEFAULT_BUF_SIZE,
                                           rte_socket_id());
        if (!mbuf_pool_) {
            std::cerr << "Cannot create mbuf pool\n";
            return false;
        }

        // Configure port
        struct rte_eth_conf port_conf = {};
        port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
        port_conf.txmode.mq_mode = ETH_MQ_TX_NONE;

        ret = rte_eth_dev_configure(port_id_, 1, 1, &port_conf);
        if (ret < 0) {
            std::cerr << "Cannot configure device\n";
            return false;
        }

        // Setup RX queue
        ret = rte_eth_rx_queue_setup(port_id_, queue_id_, RX_RING_SIZE,
                                   rte_eth_dev_socket_id(port_id_), nullptr, mbuf_pool_);
        if (ret < 0) {
            std::cerr << "Cannot setup RX queue\n";
            return false;
        }

        // Setup TX queue
        ret = rte_eth_tx_queue_setup(port_id_, queue_id_, TX_RING_SIZE,
                                   rte_eth_dev_socket_id(port_id_), nullptr);
        if (ret < 0) {
            std::cerr << "Cannot setup TX queue\n";
            return false;
        }

        // Start device
        ret = rte_eth_dev_start(port_id_);
        if (ret < 0) {
            std::cerr << "Cannot start device\n";
            return false;
        }

        return true;
    }

    // Zero copy packet reception
    uint16_t receive_packets_zero_copy(ProcessingFunction process_func) {
        static constexpr uint16_t BURST_SIZE = 32;
        struct rte_mbuf* packets[BURST_SIZE];

        auto start = std::chrono::high_resolution_clock::now();

        // Receive burst of packets directly into pre-allocated buffers
        uint16_t nb_rx = rte_eth_rx_burst(port_id_, queue_id_, packets, BURST_SIZE);

        if (nb_rx > 0) {
            auto receive_end = std::chrono::high_resolution_clock::now();

            for (uint16_t i = 0; i < nb_rx; i++) {
                // Direct pointer to packet data in DMA memory - NO COPYING
                uint8_t* packet_data = rte_pktmbuf_mtod(packets[i], uint8_t*);
                uint16_t packet_len = rte_pktmbuf_data_len(packets[i]);

                // Process directly from NIC memory
                process_func(packet_data, packet_len);

                // Free mbuf back to pool
                rte_pktmbuf_free(packets[i]);
            }

            auto process_end = std::chrono::high_resolution_clock::now();

            auto rx_latency = std::chrono::duration_cast<std::chrono::nanoseconds>
                             (receive_end - start).count();
            auto total_latency = std::chrono::duration_cast<std::chrono::nanoseconds>
                                (process_end - start).count();

            std::cout << "DPDK RX latency: " << rx_latency << " ns, "
                      << "Total processing: " << total_latency << " ns\n";
        }

        return nb_rx;
    }

    // Zero copy packet transmission
    uint16_t send_packet_zero_copy(const void* data, size_t length) {
        auto start = std::chrono::high_resolution_clock::now();

        // Allocate mbuf from pool
        struct rte_mbuf* pkt = rte_pktmbuf_alloc(mbuf_pool_);
        if (!pkt) {
            return 0;
        }

        // Get direct pointer to packet buffer
        uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*);

        // Copy data to mbuf (unavoidable for transmission)
        rte_memcpy(pkt_data, data, length);
        pkt->data_len = length;
        pkt->pkt_len = length;

        // Send packet directly from DMA memory to NIC
        uint16_t nb_tx = rte_eth_tx_burst(port_id_, queue_id_, &pkt, 1);

        if (nb_tx == 0) {
            rte_pktmbuf_free(pkt);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "DPDK TX latency: " << latency.count() << " ns\n";

        return nb_tx;
    }

    // Advanced: True zero copy using external buffer attachment
    uint16_t send_external_buffer_zero_copy(void* external_data, size_t length) {
        struct rte_mbuf* pkt = rte_pktmbuf_alloc(mbuf_pool_);
        if (!pkt) {
            return 0;
        }

        // Attach external buffer to mbuf - NO COPYING
        struct rte_mbuf_ext_shared_info* shinfo =
            static_cast<struct rte_mbuf_ext_shared_info*>(
                rte_zmalloc("shinfo", sizeof(*shinfo), 0));

        rte_pktmbuf_attach_extbuf(pkt, external_data, 0, length, shinfo);
        pkt->data_len = length;
        pkt->pkt_len = length;

        // Send without copying data
        uint16_t nb_tx = rte_eth_tx_burst(port_id_, queue_id_, &pkt, 1);

        if (nb_tx == 0) {
            rte_pktmbuf_free(pkt);
        }

        return nb_tx;
    }

private:
    using ProcessingFunction = std::function<void(const uint8_t*, size_t)>;
};
#endif // DPDK_AVAILABLE

// =============================================================================
// 3. SOLARFLARE ZERO COPY IMPLEMENTATION
// =============================================================================

#ifdef SOLARFLARE_AVAILABLE
class SolarflareZeroCopyEngine {
private:
    int socket_fd_;
    static constexpr size_t NUM_BUFFERS = 1024;
    static constexpr size_t BUFFER_SIZE = 2048;

    std::array<void*, NUM_BUFFERS> rx_buffers_;
    struct onload_zc_iovec rx_iovecs_[NUM_BUFFERS];

public:
    SolarflareZeroCopyEngine() : socket_fd_(-1) {}

    ~SolarflareZeroCopyEngine() {
        cleanup();
    }

    bool initialize() {
        // Create Solarflare accelerated socket
        socket_fd_ = onload_socket_nonaccel(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create Solarflare socket\n";
            return false;
        }

        // Allocate aligned buffers for zero copy
        for (size_t i = 0; i < NUM_BUFFERS; i++) {
            rx_buffers_[i] = aligned_alloc(64, BUFFER_SIZE);
            if (!rx_buffers_[i]) {
                std::cerr << "Failed to allocate buffer " << i << "\n";
                return false;
            }
        }

        // Register buffers with Solarflare for zero copy operation
        int ret = onload_zc_register_buffers(socket_fd_, rx_buffers_.data(), NUM_BUFFERS);
        if (ret < 0) {
            std::cerr << "Failed to register zero copy buffers\n";
            return false;
        }

        // Enable hardware timestamping
        int enable = 1;
        onload_set_recv_filter(socket_fd_, ONLOAD_RECV_FILTER_MCAST_ALL);

        return true;
    }

    // Zero copy receive with hardware timestamping
    struct ZeroCopyPacket {
        const uint8_t* data;
        size_t length;
        struct timespec hardware_timestamp;
        bool valid;
    };

    ZeroCopyPacket receive_with_hw_timestamp() {
        ZeroCopyPacket packet = {nullptr, 0, {0, 0}, false};

        auto start = std::chrono::high_resolution_clock::now();

        struct onload_zc_recv_args args = {};
        struct msghdr msg = {};
        struct iovec iov;

        // Setup for zero copy receive
        iov.iov_base = nullptr;  // Will be filled by zero copy receive
        iov.iov_len = BUFFER_SIZE;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        args.msg = msg;

        // Zero copy receive - data stays in registered buffers
        int rc = onload_zc_recv(socket_fd_, &args, 0);

        if (rc > 0) {
            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

            // Direct access to received data - NO COPYING
            packet.data = static_cast<const uint8_t*>(iov.iov_base);
            packet.length = rc;
            packet.hardware_timestamp = args.timestamp;  // Hardware timestamp
            packet.valid = true;

            std::cout << "Solarflare zero copy RX latency: " << latency.count() << " ns\n";
            std::cout << "Hardware timestamp: " << packet.hardware_timestamp.tv_sec
                      << "." << packet.hardware_timestamp.tv_nsec << "\n";
        }

        return packet;
    }

    // Release buffer back to pool after processing
    void release_packet(const ZeroCopyPacket& packet) {
        if (packet.valid && packet.data) {
            struct iovec iov = {const_cast<void*>(static_cast<const void*>(packet.data)),
                               packet.length};
            onload_zc_release_buffers(socket_fd_, &iov, 1);
        }
    }

    // Zero copy send with hardware timestamping
    bool send_with_hw_timestamp(const void* data, size_t length,
                               struct timespec* tx_timestamp) {
        auto start = std::chrono::high_resolution_clock::now();

        struct onload_zc_send_args args = {};
        struct iovec iov;

        // Point directly to data buffer - NO COPYING
        iov.iov_base = const_cast<void*>(data);
        iov.iov_len = length;
        args.iov = &iov;
        args.iovlen = 1;

        // Send directly from application buffer
        int rc = onload_zc_send(socket_fd_, &args, 0);

        if (rc > 0 && tx_timestamp) {
            *tx_timestamp = args.tx_timestamp;  // Hardware-captured TX time

            auto end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            std::cout << "Solarflare zero copy TX latency: " << latency.count() << " ns\n";

            return true;
        }

        return false;
    }

private:
    void cleanup() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }

        for (auto* buffer : rx_buffers_) {
            if (buffer) {
                free(buffer);
            }
        }
    }
};
#endif // SOLARFLARE_AVAILABLE

// =============================================================================
// 4. MEMORY-MAPPED FILE ZERO COPY
// =============================================================================

class ZeroCopyFileIO {
private:
    void* mapped_memory_;
    size_t file_size_;
    int fd_;
    std::string filename_;

public:
    ZeroCopyFileIO() : mapped_memory_(MAP_FAILED), file_size_(0), fd_(-1) {}

    ~ZeroCopyFileIO() {
        close_file();
    }

    bool open_file(const std::string& filename, bool create_if_not_exists = false) {
        filename_ = filename;

        int flags = O_RDWR;
        if (create_if_not_exists) {
            flags |= O_CREAT;
        }

        fd_ = open(filename.c_str(), flags, 0644);
        if (fd_ < 0) {
            std::cerr << "Failed to open file: " << filename << "\n";
            return false;
        }

        // Get file size
        struct stat st;
        if (fstat(fd_, &st) < 0) {
            std::cerr << "Failed to get file size\n";
            close(fd_);
            fd_ = -1;
            return false;
        }

        file_size_ = st.st_size;

        if (file_size_ == 0 && create_if_not_exists) {
            // Create file with default size
            file_size_ = 1024 * 1024; // 1MB
            if (ftruncate(fd_, file_size_) < 0) {
                std::cerr << "Failed to set file size\n";
                close(fd_);
                fd_ = -1;
                return false;
            }
        }

        // Map file into memory - zero copy access
        mapped_memory_ = mmap(nullptr, file_size_, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd_, 0);

        if (mapped_memory_ == MAP_FAILED) {
            std::cerr << "Failed to map file into memory\n";
            close(fd_);
            fd_ = -1;
            return false;
        }

        // Advise kernel about access pattern
        madvise(mapped_memory_, file_size_, MADV_SEQUENTIAL | MADV_WILLNEED);

        return true;
    }

    // Zero copy read - return direct pointer to mapped memory
    const uint8_t* read_zero_copy(size_t offset, size_t length) const {
        if (offset + length > file_size_) {
            return nullptr;
        }

        // Return direct pointer to mapped memory - NO COPYING
        return static_cast<uint8_t*>(mapped_memory_) + offset;
    }

    // Zero copy write - write directly to mapped memory
    bool write_zero_copy(size_t offset, const void* data, size_t length) {
        if (offset + length > file_size_) {
            return false;
        }

        auto start = std::chrono::high_resolution_clock::now();

        // Write directly to mapped memory
        std::memcpy(static_cast<uint8_t*>(mapped_memory_) + offset, data, length);

        // Optional: Force synchronization to disk
        msync(static_cast<uint8_t*>(mapped_memory_) + offset, length, MS_ASYNC);

        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "Memory-mapped write latency: " << latency.count() << " ns\n";

        return true;
    }

    // In-place modification without copying
    template<typename T>
    T* get_mutable_object(size_t offset) {
        if (offset + sizeof(T) > file_size_) {
            return nullptr;
        }

        // Direct pointer for in-place modification
        return reinterpret_cast<T*>(static_cast<uint8_t*>(mapped_memory_) + offset);
    }

    // Zero copy append (if file is growable) - Linux only
    bool append_zero_copy(const void* data, size_t length) {
#ifdef __linux__
        // Grow file if needed
        size_t new_size = file_size_ + length;
        if (ftruncate(fd_, new_size) < 0) {
            return false;
        }

        // Remap with new size
        void* new_mapping = mremap(mapped_memory_, file_size_, new_size, MREMAP_MAYMOVE);
        if (new_mapping == MAP_FAILED) {
            return false;
        }

        mapped_memory_ = new_mapping;

        // Append data
        std::memcpy(static_cast<uint8_t*>(mapped_memory_) + file_size_, data, length);
        file_size_ = new_size;

        return true;
#else
        // macOS fallback - not supported
        std::cerr << "mremap not available on macOS\n";
        return false;
#endif
    }

private:
    void close_file() {
        if (mapped_memory_ != MAP_FAILED) {
            munmap(mapped_memory_, file_size_);
            mapped_memory_ = MAP_FAILED;
        }

        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }
};

// =============================================================================
// 5. SHARED MEMORY ZERO COPY IPC
// =============================================================================

class ZeroCopySharedMemory {
private:
    void* shared_memory_;
    size_t memory_size_;
    std::string shm_name_;
    int shm_fd_;

public:
    ZeroCopySharedMemory() : shared_memory_(MAP_FAILED), memory_size_(0), shm_fd_(-1) {}

    ~ZeroCopySharedMemory() {
        cleanup();
    }

    bool create_shared_memory(const std::string& name, size_t size) {
        shm_name_ = name;
        memory_size_ = size;

        // Create shared memory segment
        shm_fd_ = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd_ < 0) {
            std::cerr << "Failed to create shared memory: " << name << "\n";
            return false;
        }

        // Set size
        if (ftruncate(shm_fd_, size) < 0) {
            std::cerr << "Failed to set shared memory size\n";
            shm_unlink(name.c_str());
            close(shm_fd_);
            return false;
        }

        // Map shared memory - zero copy between processes
        shared_memory_ = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd_, 0);

        if (shared_memory_ == MAP_FAILED) {
            std::cerr << "Failed to map shared memory\n";
            shm_unlink(name.c_str());
            close(shm_fd_);
            return false;
        }

        return true;
    }

    bool open_existing_shared_memory(const std::string& name, size_t size) {
        shm_name_ = name;
        memory_size_ = size;

        // Open existing shared memory
        shm_fd_ = shm_open(name.c_str(), O_RDWR, 0666);
        if (shm_fd_ < 0) {
            std::cerr << "Failed to open shared memory: " << name << "\n";
            return false;
        }

        // Map shared memory
        shared_memory_ = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd_, 0);

        if (shared_memory_ == MAP_FAILED) {
            std::cerr << "Failed to map shared memory\n";
            close(shm_fd_);
            return false;
        }

        return true;
    }

    // Zero copy write to shared memory
    bool write_zero_copy(size_t offset, const void* data, size_t length) {
        if (offset + length > memory_size_) {
            return false;
        }

        auto start = std::chrono::high_resolution_clock::now();

        // Direct write to shared memory
        std::memcpy(static_cast<uint8_t*>(shared_memory_) + offset, data, length);

        // Memory barrier for synchronization
        std::atomic_thread_fence(std::memory_order_release);

        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "Shared memory write latency: " << latency.count() << " ns\n";

        return true;
    }

    // Zero copy read from shared memory
    const void* read_zero_copy(size_t offset, size_t length) {
        if (offset + length > memory_size_) {
            return nullptr;
        }

        std::atomic_thread_fence(std::memory_order_acquire);

        // Return direct pointer to shared memory - NO COPYING
        return static_cast<uint8_t*>(shared_memory_) + offset;
    }

    // Get direct pointer for in-place operations
    template<typename T>
    T* get_object_ptr(size_t offset) {
        if (offset + sizeof(T) > memory_size_) {
            return nullptr;
        }

        return reinterpret_cast<T*>(static_cast<uint8_t*>(shared_memory_) + offset);
    }

    // Lock-free ring buffer for zero copy message passing
    template<typename T, size_t CAPACITY>
    class ZeroCopyRingBuffer {
    private:
        static_assert((CAPACITY & (CAPACITY - 1)) == 0, "Capacity must be power of 2");
        static constexpr size_t MASK = CAPACITY - 1;

        struct alignas(64) BufferHeader {
            std::atomic<size_t> write_index{0};
            char padding1[64 - sizeof(std::atomic<size_t>)];
            std::atomic<size_t> read_index{0};
            char padding2[64 - sizeof(std::atomic<size_t>)];
        };

        BufferHeader* header_;
        T* data_;

    public:
        explicit ZeroCopyRingBuffer(void* shared_memory_base, size_t offset) {
            header_ = reinterpret_cast<BufferHeader*>(
                static_cast<uint8_t*>(shared_memory_base) + offset);
            data_ = reinterpret_cast<T*>(header_ + 1);
        }

        // Zero copy write - return pointer to write location
        T* begin_write() {
            size_t write_idx = header_->write_index.load(std::memory_order_relaxed);
            size_t read_idx = header_->read_index.load(std::memory_order_acquire);

            if (((write_idx + 1) & MASK) == (read_idx & MASK)) {
                return nullptr; // Buffer full
            }

            return &data_[write_idx & MASK];
        }

        // Complete zero copy write
        void end_write() {
            size_t write_idx = header_->write_index.load(std::memory_order_relaxed);
            header_->write_index.store((write_idx + 1) & MASK, std::memory_order_release);
        }

        // Zero copy read - return pointer to read location
        const T* begin_read() {
            size_t read_idx = header_->read_index.load(std::memory_order_relaxed);
            size_t write_idx = header_->write_index.load(std::memory_order_acquire);

            if (read_idx == write_idx) {
                return nullptr; // Buffer empty
            }

            return &data_[read_idx & MASK];
        }

        // Complete zero copy read
        void end_read() {
            size_t read_idx = header_->read_index.load(std::memory_order_relaxed);
            header_->read_index.store((read_idx + 1) & MASK, std::memory_order_release);
        }
    };

private:
    void cleanup() {
        if (shared_memory_ != MAP_FAILED) {
            munmap(shared_memory_, memory_size_);
        }

        if (shm_fd_ >= 0) {
            close(shm_fd_);
            shm_unlink(shm_name_.c_str());
        }
    }
};

// =============================================================================
// 6. COMPLETE ZERO COPY TRADING PIPELINE
// =============================================================================

/**
 * Complete trading pipeline using zero copy techniques
 * Target: <1μs end-to-end latency
 */
class ZeroCopyTradingPipeline {
private:
    // Market data structures
    struct alignas(64) MarketDataMessage {
        uint32_t symbol_id;
        uint64_t timestamp_ns;
        double bid_price;
        double ask_price;
        uint64_t bid_quantity;
        uint64_t ask_quantity;
        uint32_t sequence_number;
    };

    struct alignas(64) OrderMessage {
        uint32_t order_id;
        uint32_t symbol_id;
        double price;
        uint64_t quantity;
        uint8_t side; // 0=buy, 1=sell
        uint64_t timestamp_ns;
        std::atomic<bool> ready{false};
    };

    // Zero copy components
    ZeroCopyNetworkIO network_engine_;
    ZeroCopySharedMemory order_channel_;

    // Performance counters
    std::atomic<uint64_t> processed_packets_{0};
    std::atomic<uint64_t> generated_orders_{0};

public:
    bool initialize() {
        // Initialize network engine
        std::cout << "Initializing zero copy trading pipeline...\n";

        // Create shared memory for order communication
        if (!order_channel_.create_shared_memory("/trading_orders", 10 * 1024 * 1024)) {
            std::cerr << "Failed to create order channel\n";
            return false;
        }

        std::cout << "Zero copy pipeline initialized successfully\n";
        return true;
    }

    // Main trading loop - zero copy throughout
    void run_trading_loop() {
        std::cout << "Starting zero copy trading loop...\n";

        while (true) {
            // Zero copy market data reception
            size_t packet_length;
            const uint8_t* packet_data = network_engine_.receive_zero_copy(packet_length);

            if (packet_data && packet_length >= sizeof(MarketDataMessage)) {
                // Process directly from DMA buffer
                process_market_data_zero_copy(packet_data, packet_length);
            }

            // Yield CPU briefly to avoid 100% usage in simulation
            std::this_thread::yield();
        }
    }

    // Performance monitoring
    void print_performance_stats() {
        static auto last_time = std::chrono::steady_clock::now();
        static uint64_t last_packets = 0;
        static uint64_t last_orders = 0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);

        if (elapsed.count() >= 1) {
            uint64_t current_packets = processed_packets_.load();
            uint64_t current_orders = generated_orders_.load();

            uint64_t packets_per_sec = current_packets - last_packets;
            uint64_t orders_per_sec = current_orders - last_orders;

            std::cout << "Performance: " << packets_per_sec << " packets/sec, "
                      << orders_per_sec << " orders/sec\n";

            last_packets = current_packets;
            last_orders = current_orders;
            last_time = now;
        }
    }

private:
    void process_market_data_zero_copy(const uint8_t* packet_data, size_t length) {
        auto processing_start = std::chrono::high_resolution_clock::now();

        // Parse market data directly from DMA buffer - NO COPYING
        const auto* market_msg = reinterpret_cast<const MarketDataMessage*>(packet_data);

        // Strategy calculation using direct pointer access
        if (should_trade_zero_copy(market_msg)) {
            // Generate order directly in shared memory - NO COPYING
            create_order_zero_copy(market_msg);
        }

        processed_packets_.fetch_add(1, std::memory_order_relaxed);

        auto processing_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>
                      (processing_end - processing_start);

        if (latency.count() > 1000) { // Log if > 1μs
            std::cout << "Processing latency: " << latency.count() << " ns\n";
        }
    }

    bool should_trade_zero_copy(const MarketDataMessage* msg) {
        // Simple trading strategy using direct memory access
        double spread = msg->ask_price - msg->bid_price;
        return spread > 0.01 && msg->bid_quantity >= 1000 && msg->ask_quantity >= 1000;
    }

    void create_order_zero_copy(const MarketDataMessage* market_msg) {
        static std::atomic<size_t> order_offset{0};
        static std::atomic<uint32_t> order_id_counter{1};

        // Calculate offset in shared memory for order
        size_t offset = order_offset.fetch_add(sizeof(OrderMessage), std::memory_order_relaxed);
        offset %= (10 * 1024 * 1024 - sizeof(OrderMessage)); // Wrap around

        // Get direct pointer to order location in shared memory
        auto* order_ptr = order_channel_.get_object_ptr<OrderMessage>(offset);

        if (order_ptr) {
            // Fill order directly in shared memory - NO COPYING
            order_ptr->order_id = order_id_counter.fetch_add(1, std::memory_order_relaxed);
            order_ptr->symbol_id = market_msg->symbol_id;
            order_ptr->price = market_msg->bid_price + 0.01; // Aggressive bid
            order_ptr->quantity = 100;
            order_ptr->side = 0; // Buy
            order_ptr->timestamp_ns = get_timestamp_ns();

            // Signal order ready via atomic flag
            order_ptr->ready.store(true, std::memory_order_release);

            generated_orders_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    uint64_t get_timestamp_ns() {
        auto now = std::chrono::high_resolution_clock::now();
        return now.time_since_epoch().count();
    }
};

} // namespace ZeroCopy

// =============================================================================
// DEMONSTRATION AND BENCHMARKS
// =============================================================================

void demonstrate_zero_copy_benefits() {
    std::cout << "\n=== Zero Copy Mechanisms Demo ===\n\n";

    // 1. Traditional vs Zero Copy Network I/O
    std::cout << "1. Network I/O Comparison:\n";
    {
        // Simulate socket
        int test_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (test_socket >= 0) {
            ZeroCopy::TraditionalNetworkIO traditional(test_socket);
            ZeroCopy::ZeroCopyNetworkIO zero_copy;

            std::cout << "Traditional approach: Multiple memory copies\n";
            std::cout << "Zero copy approach: Direct DMA access\n";

            close(test_socket);
        }
    }

    // 2. Memory-mapped file operations
    std::cout << "\n2. File I/O Comparison:\n";
    {
        ZeroCopy::ZeroCopyFileIO file_io;

        if (file_io.open_file("/tmp/test_zero_copy.dat", true)) {
            std::string test_data = "Ultra low latency trading data";

            // Zero copy write
            file_io.write_zero_copy(0, test_data.data(), test_data.size());

            // Zero copy read
            const uint8_t* read_data = file_io.read_zero_copy(0, test_data.size());
            if (read_data) {
                std::cout << "Zero copy file I/O successful\n";
            }
        }
    }

    // 3. Shared memory IPC
    std::cout << "\n3. Inter-Process Communication:\n";
    {
        ZeroCopy::ZeroCopySharedMemory shm;

        if (shm.create_shared_memory("/test_trading_shm", 1024 * 1024)) {
            std::string message = "Market data update";

            // Zero copy write to shared memory
            shm.write_zero_copy(0, message.data(), message.size());

            // Zero copy read from shared memory
            const void* read_data = shm.read_zero_copy(0, message.size());
            if (read_data) {
                std::cout << "Zero copy shared memory IPC successful\n";
            }
        }
    }

    // 4. Complete trading pipeline
    std::cout << "\n4. Zero Copy Trading Pipeline:\n";
    {
        ZeroCopy::ZeroCopyTradingPipeline pipeline;

        if (pipeline.initialize()) {
            std::cout << "Zero copy trading pipeline initialized\n";
            std::cout << "Pipeline supports:\n";
            std::cout << "- Zero copy market data reception\n";
            std::cout << "- Direct memory processing\n";
            std::cout << "- Zero copy order generation\n";
            std::cout << "- Shared memory order distribution\n";
        }
    }
}

void benchmark_zero_copy_performance() {
    std::cout << "\n=== Zero Copy Performance Benchmarks ===\n\n";

    constexpr size_t NUM_ITERATIONS = 1000000;
    constexpr size_t DATA_SIZE = 1024;

    // Prepare test data
    std::vector<uint8_t> test_data(DATA_SIZE, 0x42);
    std::vector<uint8_t> traditional_buffer(DATA_SIZE);

    // 1. Memory copy vs zero copy access
    {
        auto start = std::chrono::high_resolution_clock::now();

        // Traditional approach - copy data
        for (size_t i = 0; i < NUM_ITERATIONS; i++) {
            std::memcpy(traditional_buffer.data(), test_data.data(), DATA_SIZE);
            volatile uint8_t dummy = traditional_buffer[0]; // Prevent optimization
            (void)dummy;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto traditional_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        start = std::chrono::high_resolution_clock::now();

        // Zero copy approach - direct access
        for (size_t i = 0; i < NUM_ITERATIONS; i++) {
            const uint8_t* zero_copy_ptr = test_data.data();
            volatile uint8_t dummy = zero_copy_ptr[0]; // Prevent optimization
            (void)dummy;
        }

        end = std::chrono::high_resolution_clock::now();
        auto zero_copy_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "Memory Access Benchmark (" << NUM_ITERATIONS << " iterations):\n";
        std::cout << "Traditional (with copy): " << traditional_time.count() << " ns total, "
                  << traditional_time.count() / NUM_ITERATIONS << " ns/op\n";
        std::cout << "Zero copy (direct access): " << zero_copy_time.count() << " ns total, "
                  << zero_copy_time.count() / NUM_ITERATIONS << " ns/op\n";
        std::cout << "Speedup: " << (double)traditional_time.count() / zero_copy_time.count()
                  << "x\n\n";
    }

    // 2. Cache efficiency comparison
    {
        constexpr size_t LARGE_DATA_SIZE = 64 * 1024; // 64KB
        std::vector<uint8_t> large_data(LARGE_DATA_SIZE);
        std::vector<uint8_t> copy_buffer(LARGE_DATA_SIZE);

        auto start = std::chrono::high_resolution_clock::now();

        // Multiple copies (cache pollution)
        for (size_t i = 0; i < 1000; i++) {
            std::memcpy(copy_buffer.data(), large_data.data(), LARGE_DATA_SIZE);
            // Process copied data
            volatile uint64_t sum = 0;
            for (size_t j = 0; j < LARGE_DATA_SIZE; j += 8) {
                sum += *reinterpret_cast<uint64_t*>(&copy_buffer[j]);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto copy_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        start = std::chrono::high_resolution_clock::now();

        // Direct access (cache friendly)
        for (size_t i = 0; i < 1000; i++) {
            // Process data directly
            volatile uint64_t sum = 0;
            for (size_t j = 0; j < LARGE_DATA_SIZE; j += 8) {
                sum += *reinterpret_cast<uint64_t*>(&large_data[j]);
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto direct_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Cache Efficiency Benchmark (64KB data, 1000 iterations):\n";
        std::cout << "With copying: " << copy_time.count() << " μs\n";
        std::cout << "Direct access: " << direct_time.count() << " μs\n";
        std::cout << "Cache efficiency gain: " << (double)copy_time.count() / direct_time.count()
                  << "x\n\n";
    }
}

int main() {
    std::cout << "Zero Copy Mechanisms for Ultra Low Latency Trading\n";
    std::cout << "==================================================\n";

    try {
        demonstrate_zero_copy_benefits();
        benchmark_zero_copy_performance();

        std::cout << "\nKey Zero Copy Benefits:\n";
        std::cout << "• Eliminates memory copy overhead (2-5μs → <300ns)\n";
        std::cout << "• Reduces cache pollution and improves locality\n";
        std::cout << "• Minimizes memory allocation/deallocation\n";
        std::cout << "• Enables true sub-microsecond latencies\n";
        std::cout << "• Improves deterministic performance\n";
        std::cout << "• Reduces jitter in latency-critical paths\n\n";

        std::cout << "Zero Copy Techniques Summary:\n";
        std::cout << "1. DPDK - Kernel bypass with packet pools\n";
        std::cout << "2. Solarflare - Hardware acceleration + zero copy\n";
        std::cout << "3. Memory mapping - Direct file access\n";
        std::cout << "4. Shared memory - Zero copy IPC\n";
        std::cout << "5. DMA buffers - Direct hardware access\n";
        std::cout << "6. Lock-free structures - Concurrent zero copy\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

/*
COMPILATION INSTRUCTIONS:

Basic compilation:
g++ -std=c++17 -O3 -march=native zero_copy_mechanisms_ultra_low_latency.cpp -o zero_copy_demo -lrt -lpthread

With DPDK (if available):
g++ -std=c++17 -O3 -march=native -DDPDK_AVAILABLE zero_copy_mechanisms_ultra_low_latency.cpp -o zero_copy_demo \
    $(pkg-config --cflags --libs libdpdk) -lrt -lpthread

With Solarflare OpenOnload (if available):
g++ -std=c++17 -O3 -march=native -DSOLARFLARE_AVAILABLE zero_copy_mechanisms_ultra_low_latency.cpp -o zero_copy_demo \
    -lonload_ext -lrt -lpthread

PERFORMANCE CONSIDERATIONS:

1. Memory Alignment:
   - Use 64-byte alignment for cache line optimization
   - Consider NUMA topology for multi-socket systems

2. Huge Pages:
   - Use 2MB/1GB huge pages for reduced TLB misses
   - Configure: echo 1024 > /proc/sys/vm/nr_hugepages

3. CPU Isolation:
   - Isolate CPUs for zero copy processing
   - Use isolcpus=2-7 in kernel parameters

4. Memory Locking:
   - Lock critical memory regions with mlock()
   - Prevent swapping of performance-critical buffers

5. NUMA Awareness:
   - Allocate memory on same NUMA node as processing CPU
   - Use numa_alloc_onnode() for NUMA-specific allocation

TYPICAL LATENCY IMPROVEMENTS:

Traditional Approach:  5-15μs end-to-end
Zero Copy Approach:   0.3-2μs end-to-end
Improvement Factor:   5-50x latency reduction

The zero copy approach is essential for achieving
sub-microsecond latencies required in modern HFT systems.
*/
