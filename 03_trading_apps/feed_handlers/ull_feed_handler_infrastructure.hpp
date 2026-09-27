/*
 * ============================================================================
 * ULL FEED HANDLER INFRASTRUCTURE - C++17
 * ============================================================================
 *
 * Production-quality core for Virtu/Citadel/Jump-style feed handlers.
 *
 * Contents:
 *   §1  Fundamental types & fixed-point price
 *   §2  Cache constants & alignment helpers
 *   §3  RDTSC high-resolution timer
 *   §4  NUMA-aware allocation & CPU affinity (Linux)
 *   §5  SPSC lock-free ring buffer (1:1 thread)
 *   §6  SPMC Disruptor ring buffer (1:N fan-out to strategies)
 *   §7  Solarflare ef_vi transport abstraction (with UDP fallback)
 *   §8  Structure-of-Arrays (SoA) order book + seqlock
 *
 * Design invariants:
 *   - Every hot-path struct: hot fields in first 64-byte cache line,
 *     cold fields after. alignas(64) at struct level.
 *   - All shared state between threads: placed on separate cache lines
 *     (std::hardware_destructive_interference_size padding).
 *   - Zero heap allocation on hot path.
 *   - Lock-free everywhere (atomics, seqlocks, ring buffers).
 *
 * Build:
 *   # Without ef_vi (fallback UDP)
 *   g++ -std=c++17 -O3 -march=native -pthread -I. ull_feed_system_demo.cpp
 *
 *   # With Solarflare ef_vi
 *   g++ -std=c++17 -O3 -march=native -pthread -DEF_VI_AVAILABLE \
 *       -I/usr/include/etherfabric -I. ull_feed_system_demo.cpp \
 *       -letherfabric
 *
 *   # With NUMA support
 *   g++ -std=c++17 -O3 -march=native -pthread -DNUMA_AVAILABLE \
 *       -I. ull_feed_system_demo.cpp -lnuma
 * ============================================================================
 */
#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <climits>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <new>          // std::hardware_destructive_interference_size

#ifdef __linux__
#  include <pthread.h>
#  include <sched.h>
#  include <unistd.h>
#  include <sys/mman.h>
#endif
#ifdef NUMA_AVAILABLE
#  include <numa.h>
#  include <numaif.h>
#endif
#ifdef EF_VI_AVAILABLE
#  include <etherfabric/vi.h>
#  include <etherfabric/pd.h>
#  include <etherfabric/memreg.h>
#  include <etherfabric/ef_vi.h>
#else
// Fallback: standard UDP multicast sockets
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <sys/ioctl.h>
#  include <net/if.h>
#  include <unistd.h>   // ::close
#  include <fcntl.h>    // fcntl / O_NONBLOCK (portable non-blocking sockets)
#endif

// ============================================================================
// §1  FUNDAMENTAL TYPES & FIXED-POINT PRICE
// ============================================================================
namespace ull {

// Fixed-point price: 9 decimal places (1e9 = CME, 1e7 viable)
static constexpr int64_t PRICE_SCALE = 1'000'000'000LL;  // 9 dp

[[nodiscard]] inline int64_t to_fp(double p) noexcept {
    return static_cast<int64_t>(p * PRICE_SCALE + 0.5);
}
[[nodiscard]] inline double from_fp(int64_t fp) noexcept {
    return static_cast<double>(fp) / PRICE_SCALE;
}
// Multiply two fixed-point values: (a_fp * b_fp) / SCALE
[[nodiscard]] inline int64_t fpmul(int64_t a, int64_t b) noexcept {
    return (__int128_t(a) * b) / PRICE_SCALE;
}

enum class Side : uint8_t { BUY = 0, SELL = 1 };
enum class MDAction : uint8_t { New = 0, Change = 1, Delete = 2 };

enum class MarketID : uint8_t {
    HKEX_OMDCDQ = 0,  // HKEX OMD-C / OMD-D
    ASX_ITCH    = 1,  // ASX ITCH 1.1
    SGX_ITCH    = 2,  // SGX (NASDAQ ITCH-conformant)
    CME_MDP3    = 3,  // CME MDP 3.0 (SBE/UDP)
    OSAKA_ITCH  = 4,  // OSE (Osaka/JPX) ITCH
    KRX_DDCM    = 5,  // Korea Exchange DDCM
};

// Instrument ID — 32-bit dense index assigned by feed container on startup
using InstrumentID = uint32_t;
static constexpr InstrumentID INVALID_INSTRUMENT = UINT32_MAX;

// Packed symbol: 8 printable ASCII bytes, stack only, no std::string
struct Symbol {
    char data[8]{};
    explicit Symbol(std::string_view s) noexcept {
        size_t n = std::min(s.size(), size_t(7));
        std::memcpy(data, s.data(), n);
        data[n] = '\0';
    }
    Symbol() noexcept { data[0] = '\0'; }
    std::string_view view() const noexcept { return {data, std::strlen(data)}; }
    bool operator==(const Symbol& o) const noexcept {
        return std::memcmp(data, o.data, 8) == 0;
    }
};

// ============================================================================
// §2  CACHE CONSTANTS & ALIGNMENT HELPERS
// ============================================================================
static constexpr size_t CACHE_LINE = 64;
// Use compiler constant to avoid ODR issues
static constexpr size_t DINTERFERENCE =
#if defined(__cpp_lib_hardware_interference_size)
    std::hardware_destructive_interference_size;
#else
    64;
#endif

// Pad to next multiple of CACHE_LINE
template<size_t N>
static constexpr size_t pad_to_cl = (N + CACHE_LINE - 1) & ~(CACHE_LINE - 1);

// Helper: one cache line of padding bytes
struct alignas(CACHE_LINE) CacheLinePad {
    char pad[CACHE_LINE];
};

// Macro: assert struct size is N cache lines
#define ASSERT_CACHE_LINES(T, N) \
    static_assert(sizeof(T) == (N) * CACHE_LINE, #T " must be " #N " cache line(s)")

// ============================================================================
// §3  RDTSC HIGH-RESOLUTION TIMER
// ============================================================================
[[nodiscard]] inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t(hi) << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    return uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

[[nodiscard]] inline uint64_t rdtscp() noexcept {
#if defined(__x86_64__)
    uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (uint64_t(hi) << 32) | lo;
#else
    return rdtsc();
#endif
}

// Compiler + CPU fence: prevent reordering across this point
inline void compiler_fence() noexcept { std::atomic_signal_fence(std::memory_order_seq_cst); }
inline void cpu_fence()      noexcept { std::atomic_thread_fence(std::memory_order_seq_cst); }

// Busy-wait pause (x86 PAUSE, reduces pipeline contention in spin loops)
inline void spin_pause() noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield" ::: "memory");
#endif
}

// ============================================================================
// §4  NUMA-AWARE ALLOCATION & CPU AFFINITY
// ============================================================================

// Allocate 2MB-aligned memory using huge pages where available, else mmap
[[nodiscard]] inline void* alloc_huge(size_t bytes) noexcept {
#ifdef __linux__
    void* p = mmap(nullptr, bytes,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                   -1, 0);
    if (p != MAP_FAILED) { mlock(p, bytes); return p; }
    // Fallback to regular mmap (still lock in memory)
    p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != MAP_FAILED) { mlock(p, bytes); return p; }
#endif
    return std::aligned_alloc(CACHE_LINE, (bytes + CACHE_LINE - 1) & ~(CACHE_LINE - 1));
}

inline void free_huge(void* p, size_t bytes) noexcept {
#ifdef __linux__
    munmap(p, bytes);
#else
    std::free(p);
#endif
}

// NUMA-aware allocator (falls back to alloc_huge on non-NUMA systems)
[[nodiscard]] inline void* alloc_on_node(size_t bytes, int numa_node) noexcept {
#ifdef NUMA_AVAILABLE
    if (numa_available() >= 0) {
        void* p = numa_alloc_onnode(bytes, numa_node);
        if (p) { mlock(p, bytes); return p; }
    }
#endif
    (void)numa_node;
    return alloc_huge(bytes);
}

inline void free_numa(void* p, size_t bytes) noexcept {
#ifdef NUMA_AVAILABLE
    if (numa_available() >= 0) { numa_free(p, bytes); return; }
#endif
    free_huge(p, bytes);
}

// Pin the calling thread to a specific CPU core
inline bool pin_thread_to_cpu(int cpu_id) noexcept {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_id, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
    (void)cpu_id; return false;
#endif
}

// Set real-time scheduling (SCHED_FIFO), requires root/CAP_SYS_NICE
inline bool set_realtime(int priority = 80) noexcept {
#ifdef __linux__
    struct sched_param sp{};
    sp.sched_priority = priority;
    return sched_setscheduler(0, SCHED_FIFO, &sp) == 0;
#else
    (void)priority; return false;
#endif
}

// Get NUMA node for current CPU
inline int current_numa_node() noexcept {
#ifdef NUMA_AVAILABLE
    if (numa_available() >= 0) return numa_node_of_cpu(sched_getcpu());
#endif
    return 0;
}

// ============================================================================
// §5  SPSC LOCK-FREE RING BUFFER
//     Single Producer / Single Consumer — 1:1 thread connectivity.
//     Used for: NIC-recv thread → book-builder thread.
//     Capacity must be power-of-2.  Each slot is one cache line (64 bytes).
// ============================================================================
template<typename T, uint32_t CAPACITY>
class alignas(CACHE_LINE) SpscRing {
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be power-of-2");
    // T should be cache-line-aligned but may be larger than one cache line
    // (e.g. RawPacket contains a full 1400-byte UDP payload by design).
    static_assert(sizeof(T) % CACHE_LINE == 0 || sizeof(T) < CACHE_LINE,
                  "T size should be a multiple of CACHE_LINE for optimal slot layout");
    static constexpr uint32_t MASK = CAPACITY - 1;

    // Producer state: written by producer, read by both
    alignas(CACHE_LINE) std::atomic<uint32_t> write_idx_{0};
    char                                       _pad0[CACHE_LINE - sizeof(std::atomic<uint32_t>)];

    // Consumer state: written by consumer, read by both
    alignas(CACHE_LINE) std::atomic<uint32_t> read_idx_{0};
    char                                       _pad1[CACHE_LINE - sizeof(std::atomic<uint32_t>)];

    // Ring slots: each cache-line aligned to prevent false sharing
    alignas(CACHE_LINE) T slots_[CAPACITY];

public:
    SpscRing() = default;
    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    // Producer: returns slot pointer to write into, nullptr if full.
    // Caller fills the slot, then calls publish().
    [[nodiscard]] __attribute__((hot))
    T* try_claim() noexcept {
        uint32_t w = write_idx_.load(std::memory_order_relaxed);
        uint32_t r = read_idx_.load(std::memory_order_acquire);
        if (w - r >= CAPACITY) return nullptr;  // full
        return &slots_[w & MASK];
    }

    __attribute__((hot))
    void publish() noexcept {
        write_idx_.fetch_add(1, std::memory_order_release);
    }

    // Consumer: returns pointer to next available slot, nullptr if empty.
    // Caller processes the slot, then calls consume() to advance cursor.
    [[nodiscard]] __attribute__((hot))
    const T* try_peek() const noexcept {
        uint32_t r = read_idx_.load(std::memory_order_relaxed);
        uint32_t w = write_idx_.load(std::memory_order_acquire);
        if (r == w) return nullptr;  // empty
        return &slots_[r & MASK];
    }

    __attribute__((hot))
    void consume() noexcept {
        read_idx_.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] uint32_t size() const noexcept {
        return write_idx_.load(std::memory_order_relaxed)
             - read_idx_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    [[nodiscard]] bool full()  const noexcept { return size() == CAPACITY; }
};

// ============================================================================
// §6  SPMC DISRUPTOR RING BUFFER
//     Single Producer / Multiple Consumers (LMAX Disruptor pattern).
//     Used for: book-builder → N strategy threads (fan-out).
//
//     Key properties:
//       - One writer cursor (cache-line padded)
//       - One consumer cursor per consumer (each cache-line padded)
//         → NO false sharing between consumers
//       - Consumers are independent: each reads at its own pace
//       - BusySpin wait strategy for < 100 ns latency on isolated cores
//       - Back-pressure: writer waits for slowest consumer before overwrite
//
//     Usage pattern:
//       Writer:  seq = ring.claim();
//                ring.slot(seq) = data;
//                ring.publish(seq);
//
//       Reader:  if (ring.available_for(consumer_id)) {
//                    auto& slot = ring.slot(ring.cursor(consumer_id));
//                    process(slot);
//                    ring.advance(consumer_id);
//                }
// ============================================================================
static constexpr size_t DISRUPTOR_MAX_CONSUMERS = 32;

template<typename T, uint32_t CAPACITY, uint32_t MAX_CONSUMERS = 16>
class alignas(CACHE_LINE) SpmcDisruptor {
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be power-of-2");
    static_assert(MAX_CONSUMERS <= DISRUPTOR_MAX_CONSUMERS);
    static constexpr uint64_t MASK = CAPACITY - 1;
    static constexpr uint64_t INITIAL_SEQ = uint64_t(-1);  // published = 0 after first publish

    // ---- Writer cursor: single sequence number -------------------------
    struct alignas(CACHE_LINE) WriterCursor {
        std::atomic<uint64_t> published{INITIAL_SEQ};   // last published sequence
        std::atomic<uint64_t> claimed  {INITIAL_SEQ};   // last claimed (may be ahead)
        char _pad[CACHE_LINE - 2*sizeof(std::atomic<uint64_t>)];
    } writer_;

    // ---- Consumer cursors: one per consumer, each on its own cache line -
    struct alignas(CACHE_LINE) ConsumerCursor {
        std::atomic<uint64_t> seq{INITIAL_SEQ};   // last consumed
        std::atomic<bool>     active{false};
        char _pad[CACHE_LINE - sizeof(std::atomic<uint64_t>) - sizeof(std::atomic<bool>)];
    } consumers_[MAX_CONSUMERS];

    // ---- Ring slots (each cache-line sized) ----------------------------
    alignas(CACHE_LINE) T slots_[CAPACITY];

    std::atomic<uint32_t> num_consumers_{0};

public:
    SpmcDisruptor() = default;
    SpmcDisruptor(const SpmcDisruptor&) = delete;
    SpmcDisruptor& operator=(const SpmcDisruptor&) = delete;

    // ---- Consumer registration (called at setup time, not hot path) ----
    // Returns consumer_id (index into consumers_ array)
    [[nodiscard]] int register_consumer() noexcept {
        uint32_t id = num_consumers_.fetch_add(1, std::memory_order_relaxed);
        assert(id < MAX_CONSUMERS);
        // Start consumer at current published sequence
        consumers_[id].seq.store(
            writer_.published.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        consumers_[id].active.store(true, std::memory_order_release);
        return static_cast<int>(id);
    }

    void unregister_consumer(int id) noexcept {
        consumers_[id].active.store(false, std::memory_order_release);
    }

    // ---- Writer API (single thread only) --------------------------------
    // Claim the next slot.  Blocks (busy-waits) if slowest consumer is
    // CAPACITY behind (i.e., would overwrite unprocessed data).
    [[nodiscard]] __attribute__((hot))
    uint64_t claim() noexcept {
        uint64_t seq = writer_.claimed.load(std::memory_order_relaxed) + 1;
        // Back-pressure: wait until slowest active consumer has passed
        // the slot we're about to overwrite.
        uint64_t wrap_point = seq - CAPACITY;
        uint32_t nc = num_consumers_.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < nc; ++i) {
            if (!consumers_[i].active.load(std::memory_order_relaxed)) continue;
            while (consumers_[i].seq.load(std::memory_order_acquire) < wrap_point)
                spin_pause();
        }
        writer_.claimed.store(seq, std::memory_order_relaxed);
        return seq;
    }

    // Try-claim: non-blocking, returns UINT64_MAX if blocked
    [[nodiscard]] __attribute__((hot))
    uint64_t try_claim() noexcept {
        uint64_t seq = writer_.claimed.load(std::memory_order_relaxed) + 1;
        uint64_t wrap_point = seq - CAPACITY;
        uint32_t nc = num_consumers_.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < nc; ++i) {
            if (!consumers_[i].active.load(std::memory_order_relaxed)) continue;
            if (consumers_[i].seq.load(std::memory_order_acquire) < wrap_point)
                return UINT64_MAX;
        }
        writer_.claimed.store(seq, std::memory_order_relaxed);
        return seq;
    }

    // Get reference to slot for claimed sequence
    [[nodiscard]] __attribute__((hot))
    T& slot(uint64_t seq) noexcept {
        return slots_[seq & MASK];
    }
    [[nodiscard]] __attribute__((hot))
    const T& slot(uint64_t seq) const noexcept {
        return slots_[seq & MASK];
    }

    // Publish: make slot visible to consumers
    __attribute__((hot))
    void publish(uint64_t seq) noexcept {
        writer_.published.store(seq, std::memory_order_release);
    }

    // ---- Consumer API ---------------------------------------------------
    // Returns true if consumer `id` has a new slot available
    [[nodiscard]] __attribute__((hot))
    bool available_for(int id) const noexcept {
        uint64_t next = consumers_[id].seq.load(std::memory_order_relaxed) + 1;
        return next <= writer_.published.load(std::memory_order_acquire);
    }

    // Next sequence to consume for consumer `id`
    [[nodiscard]] __attribute__((hot))
    uint64_t next_seq(int id) const noexcept {
        return consumers_[id].seq.load(std::memory_order_relaxed) + 1;
    }

    // Advance consumer cursor after processing
    __attribute__((hot))
    void advance(int id) noexcept {
        consumers_[id].seq.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] uint64_t published_seq() const noexcept {
        return writer_.published.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint32_t num_consumers() const noexcept {
        return num_consumers_.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// §7  SOLARFLARE ef_vi TRANSPORT ABSTRACTION
//     - Zero-copy: packets DMA'd directly into pre-pinned buffers
//     - No kernel involvement: completely bypasses the kernel network stack
//     - recv_packet() returns a pointer into the DMA buffer + length
//       → caller parses in-place (zero-copy)
//     - release_packet() returns the DMA buffer to the hardware for reuse
//     Fallback (no ef_vi): standard UDP socket with recvmmsg.
// ============================================================================
static constexpr size_t EF_VI_MTU          = 9000;   // jumbo frame support
static constexpr size_t EF_VI_DMA_BUFS     = 512;    // pre-allocated DMA buffers
static constexpr size_t EF_VI_DMA_BUF_SIZE = 2048;   // per-buffer bytes (aligned)

struct RecvPacket {
    const uint8_t* data;  // pointer into DMA buffer (zero-copy)
    uint16_t       len;   // UDP payload length
    uint64_t       timestamp_ns;   // hardware timestamp (ef_vi) or software
    int            buf_id;         // for release_packet()
};

#ifdef EF_VI_AVAILABLE
// ---- Solarflare ef_vi implementation ----
class EfViTransport {
public:
    EfViTransport() = default;
    ~EfViTransport() { close(); }

    bool open(const char* interface_name, const char* mcast_group, uint16_t port,
              int numa_node = 0) {
        // Open ef_vi driver handle
        if (ef_driver_open(&dh_) < 0) return false;
        // Allocate protection domain on specified NUMA node
        if (ef_pd_alloc_by_name(&pd_, dh_, interface_name,
                                EF_PD_DEFAULT | EF_PD_MCAST_LOOP) < 0) return false;
        // Allocate VI with RX only, timestamps enabled
        ef_vi_flags flags = (ef_vi_flags)(EF_VI_FLAGS_DEFAULT | EF_VI_RX_TIMESTAMPS);
        if (ef_vi_alloc_from_pd(&vi_, dh_, &pd_, dh_, -1, 0, -1,
                                nullptr, -1, flags) < 0) return false;
        // Pre-allocate and register DMA buffers
        size_t total = EF_VI_DMA_BUFS * EF_VI_DMA_BUF_SIZE;
        dma_mem_ = alloc_on_node(total, numa_node);
        if (!dma_mem_) return false;
        if (ef_memreg_alloc(&memreg_, dh_, &pd_, dh_,
                            dma_mem_, total) < 0) return false;
        // Post all buffers to receive ring
        ef_addr base_addr = ef_memreg_dma_addr(&memreg_, 0);
        for (int i = 0; i < (int)EF_VI_DMA_BUFS; ++i) {
            ef_vi_receive_post(&vi_, base_addr + i * EF_VI_DMA_BUF_SIZE, i);
        }
        // Join multicast group
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(mcast_group);
        mreq.imr_interface.s_addr = INADDR_ANY;
        // (ef_vi handles IP multicast join through the driver)
        mcast_group_ = mcast_group;
        port_        = port;
        open_        = true;
        return true;
    }

    // Poll for a received packet.  Returns true if packet available.
    // Fills `pkt` with a zero-copy pointer into the DMA buffer.
    [[nodiscard]] __attribute__((hot))
    bool recv_packet(RecvPacket& pkt) noexcept {
        ef_event evs[8];
        int n = ef_vi_receive_poll(&vi_, evs, 8);
        for (int i = 0; i < n; ++i) {
            if (EF_EVENT_TYPE(evs[i]) == EF_EVENT_TYPE_RX) {
                int    buf_id  = EF_EVENT_RX_RQ_ID(evs[i]);
                uint16_t len   = EF_EVENT_RX_BYTES(evs[i]);
                uint64_t ts_ns = 0;
                ef_vi_receive_get_timestamp_with_sync_flags(
                    &vi_, &evs[i], &ts_ns, nullptr);
                uint8_t* buf_ptr = static_cast<uint8_t*>(dma_mem_)
                                 + buf_id * EF_VI_DMA_BUF_SIZE;
                // Skip Ethernet + IP + UDP headers (42 bytes)
                pkt.data         = buf_ptr + 42;
                pkt.len          = len - 42;
                pkt.timestamp_ns = ts_ns;
                pkt.buf_id       = buf_id;
                return true;
            }
        }
        return false;
    }

    // Return DMA buffer to hardware
    __attribute__((hot))
    void release_packet(int buf_id) noexcept {
        ef_addr addr = ef_memreg_dma_addr(&memreg_, 0) + buf_id * EF_VI_DMA_BUF_SIZE;
        ef_vi_receive_post(&vi_, addr, buf_id);
    }

    void close() noexcept {
        if (!open_) return;
        ef_vi_free(&vi_, dh_);
        ef_memreg_free(&memreg_, dh_);
        ef_pd_free(&pd_, dh_);
        ef_driver_close(dh_);
        if (dma_mem_) { free_numa(dma_mem_, EF_VI_DMA_BUFS * EF_VI_DMA_BUF_SIZE); dma_mem_ = nullptr; }
        open_ = false;
    }

private:
    ef_driver_handle dh_{};
    ef_pd            pd_{};
    ef_vi            vi_{};
    ef_memreg        memreg_{};
    void*            dma_mem_{nullptr};
    std::string      mcast_group_;
    uint16_t         port_{0};
    bool             open_{false};
};

#else
// ---- UDP socket fallback (no ef_vi SDK) ----
class EfViTransport {
public:
    EfViTransport() = default;
    ~EfViTransport() { close(); }

    bool open(const char* interface_name, const char* mcast_group, uint16_t port,
              int /*numa_node*/ = 0) {
        // SOCK_NONBLOCK is Linux-only; use fcntl for portability (macOS/BSD)
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;
        {
            int flags = fcntl(fd_, F_GETFL, 0);
            if (flags != -1) fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }

        int reuse = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        // Receive buffer size: 16 MB
        int rcvbuf = 16 * 1024 * 1024;
        setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(port);
        if (bind(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) { close(); return false; }

        // Join multicast group
        ip_mreqn mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(mcast_group);
        mreq.imr_address.s_addr   = INADDR_ANY;
        mreq.imr_ifindex          = if_nametoindex(interface_name);
        if (setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            // Non-fatal if already joined or interface doesn't support multicast
        }
        open_ = true;
        return true;
    }

    [[nodiscard]] __attribute__((hot))
    bool recv_packet(RecvPacket& pkt) noexcept {
        ssize_t n = recvfrom(fd_, buf_, sizeof(buf_), 0, nullptr, nullptr);
        if (n <= 0) return false;
        pkt.data         = buf_;
        pkt.len          = static_cast<uint16_t>(n);
        pkt.timestamp_ns = rdtsc();   // software timestamp
        pkt.buf_id       = 0;
        return true;
    }

    void release_packet(int /*buf_id*/) noexcept {}  // no-op for socket path

    void close() noexcept {
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
        open_ = false;
    }

    bool is_open() const noexcept { return open_; }

private:
    int     fd_{-1};
    bool    open_{false};
    alignas(CACHE_LINE) uint8_t buf_[EF_VI_MTU];
};
#endif  // EF_VI_AVAILABLE

// ============================================================================
// §8  STRUCTURE-OF-ARRAYS (SoA) ORDER BOOK  +  SEQLOCK
//
//     Bids and asks stored as separate contiguous arrays:
//       bid_prices_fp[0..MAX_DEPTH-1]   <- all bid prices (sorted desc)
//       bid_qtys    [0..MAX_DEPTH-1]    <- all bid quantities
//       bid_orders  [0..MAX_DEPTH-1]    <- order counts
//       ask_prices_fp[0..MAX_DEPTH-1]   <- all ask prices (sorted asc)
//       ask_qtys    [0..MAX_DEPTH-1]    <- all ask quantities
//       ask_orders  [0..MAX_DEPTH-1]    <- order counts
//
//     Benefits:
//       - SIMD: can vectorize scan over all prices in one cache line
//       - Prefetch: only load the array you need (e.g., just prices)
//       - Memory bandwidth: avoid touching order counts when only need prices
//
//     Hot-Cold separation:
//       CL0 (64 bytes):  sequence, instrument_id, best_bid/ask, spread, flags
//       CL1-CL2 (128 bytes): bid_prices_fp (10 levels × 8 bytes = 80 bytes → 2 CLs)
//       CL3-CL4 (128 bytes): ask_prices_fp
//       CL5     (64 bytes):  bid_qtys + ask_qtys (10+10 × 4 bytes = 80 → 2 CLs)
//       ...
//
//     Seqlock for lock-free reader consistency:
//       Writer: seq.fetch_add(1, release) [make odd] → write → seq.fetch_add(1, release) [make even]
//       Reader: read seq, read data, read seq again; retry if either is odd or mismatch.
// ============================================================================

static constexpr int MAX_BOOK_DEPTH = 10;

// Seqlock: 64-bit sequence counter, even = stable, odd = being written
struct alignas(CACHE_LINE) Seqlock {
    std::atomic<uint64_t> seq{0};
    char _pad[CACHE_LINE - sizeof(std::atomic<uint64_t>)];

    // Writer: call before modifying, then after
    void write_begin() noexcept { seq.fetch_add(1, std::memory_order_release); }
    void write_end()   noexcept { seq.fetch_add(1, std::memory_order_release); }

    // Reader: returns seq value (must re-read and compare)
    [[nodiscard]] uint64_t read_begin() const noexcept {
        uint64_t s;
        do { s = seq.load(std::memory_order_acquire); spin_pause(); } while (s & 1);
        return s;
    }
    [[nodiscard]] bool read_retry(uint64_t s) const noexcept {
        return seq.load(std::memory_order_acquire) != s;
    }
};

// Per-instrument SoA order book (cache-line layout documented above)
struct alignas(CACHE_LINE) SoABook {
    // ---- Cache Line 0: HOT L1 fields (read on every signal check) ------
    uint64_t   seq_version;        //  8  updated on every book change
    InstrumentID instrument_id;    //  4
    uint32_t   update_count;       //  4  total number of updates
    int64_t    best_bid_fp;        //  8  best bid (fixed-point)
    int64_t    best_ask_fp;        //  8  best ask (fixed-point)
    int64_t    last_trade_px_fp;   //  8  last traded price
    uint32_t   last_trade_qty;     //  4
    uint8_t    bid_depth;          //  1  number of valid bid levels
    uint8_t    ask_depth;          //  1  number of valid ask levels
    uint8_t    flags;              //  1  bit0=in_sync, bit1=halted, bit2=opening
    char       symbol[5];          //  5  printable symbol (null-terminated)
    // Total CL0: 8+4+4+8+8+8+4+1+1+1+5 = 52 bytes → pad to 64
    char       _pad0[64 - 52];

    // ---- Cache Line 1-2: Bid prices (SoA, 10 × int64 = 80 bytes) -------
    alignas(CACHE_LINE) int64_t bid_prices_fp[MAX_BOOK_DEPTH];  // sorted descending
    int64_t                     _pad_bp[CACHE_LINE/8 - MAX_BOOK_DEPTH % (CACHE_LINE/8)];

    // ---- Cache Line 3-4: Ask prices -------------------------------------
    alignas(CACHE_LINE) int64_t ask_prices_fp[MAX_BOOK_DEPTH];  // sorted ascending
    int64_t                     _pad_ap[CACHE_LINE/8 - MAX_BOOK_DEPTH % (CACHE_LINE/8)];

    // ---- Cache Line 5: Bid quantities -----------------------------------
    alignas(CACHE_LINE) int32_t bid_qtys[MAX_BOOK_DEPTH];
    int32_t                     bid_orders[MAX_BOOK_DEPTH];

    // ---- Cache Line 6: Ask quantities -----------------------------------
    alignas(CACHE_LINE) int32_t ask_qtys[MAX_BOOK_DEPTH];
    int32_t                     ask_orders[MAX_BOOK_DEPTH];

    // ---- Non-hot: session statistics (CL 7+) ---------------------------
    alignas(CACHE_LINE) int64_t  open_px_fp;
    int64_t                      high_px_fp;
    int64_t                      low_px_fp;
    int64_t                      settlement_px_fp;
    int64_t                      session_volume;
    int64_t                      session_turnover_fp;
    uint64_t                     last_update_ns;   // hardware timestamp
    char                         _pad_stats[64 - 7*8];

    // ---- Seqlock (separate cache line to avoid writer→reader false share) -
    alignas(CACHE_LINE) Seqlock  lock;

    void init(InstrumentID iid, std::string_view sym) noexcept {
        std::memset(this, 0, sizeof(*this));
        instrument_id = iid;
        size_t n = std::min(sym.size(), size_t(4));
        std::memcpy(symbol, sym.data(), n);
        symbol[n] = '\0';
        // Initialise all prices to invalid sentinel
        for (int i = 0; i < MAX_BOOK_DEPTH; ++i) {
            bid_prices_fp[i] = INT64_MIN;
            ask_prices_fp[i] = INT64_MAX;
        }
    }

    // Writer: apply a single-level update (no lock needed if called from
    // single book-builder thread; seqlock protects readers)
    void update_bid(int level /*0-based*/, int64_t price_fp,
                    int32_t qty, int32_t orders) noexcept {
        lock.write_begin();
        bid_prices_fp[level] = price_fp;
        bid_qtys   [level]   = qty;
        bid_orders [level]   = orders;
        if (level == 0) best_bid_fp = price_fp;
        ++update_count;
        ++seq_version;
        lock.write_end();
    }

    void update_ask(int level, int64_t price_fp,
                    int32_t qty, int32_t orders) noexcept {
        lock.write_begin();
        ask_prices_fp[level] = price_fp;
        ask_qtys   [level]   = qty;
        ask_orders [level]   = orders;
        if (level == 0) best_ask_fp = price_fp;
        ++update_count;
        ++seq_version;
        lock.write_end();
    }

    void update_trade(int64_t price_fp, uint32_t qty, uint64_t ts_ns) noexcept {
        lock.write_begin();
        last_trade_px_fp = price_fp;
        last_trade_qty   = qty;
        last_update_ns   = ts_ns;
        session_volume  += qty;
        ++seq_version;
        lock.write_end();
    }

    // Bulk replace entire book side (from snapshot)
    void replace_bids(const int64_t* prices, const int32_t* qtys,
                      const int32_t* orders, int n) noexcept {
        lock.write_begin();
        bid_depth = static_cast<uint8_t>(std::min(n, MAX_BOOK_DEPTH));
        for (int i = 0; i < bid_depth; ++i) {
            bid_prices_fp[i] = prices[i];
            bid_qtys[i]      = qtys[i];
            bid_orders[i]    = orders[i];
        }
        best_bid_fp = (bid_depth > 0) ? bid_prices_fp[0] : INT64_MIN;
        ++seq_version;
        lock.write_end();
    }

    void replace_asks(const int64_t* prices, const int32_t* qtys,
                      const int32_t* orders, int n) noexcept {
        lock.write_begin();
        ask_depth = static_cast<uint8_t>(std::min(n, MAX_BOOK_DEPTH));
        for (int i = 0; i < ask_depth; ++i) {
            ask_prices_fp[i] = prices[i];
            ask_qtys[i]      = qtys[i];
            ask_orders[i]    = orders[i];
        }
        best_ask_fp = (ask_depth > 0) ? ask_prices_fp[0] : INT64_MAX;
        ++seq_version;
        lock.write_end();
    }

    // Reader: copy L1 snapshot atomically using seqlock
    struct L1 {
        int64_t bid_fp, ask_fp, mid_fp, spread_fp;
        int32_t bid_qty, ask_qty;
        bool    valid;
    };

    [[nodiscard]] __attribute__((hot))
    L1 read_l1() const noexcept {
        L1 out{};
        uint64_t s;
        do {
            s          = lock.read_begin();
            out.bid_fp = best_bid_fp;
            out.ask_fp = best_ask_fp;
            out.bid_qty = bid_qtys[0];
            out.ask_qty = ask_qtys[0];
        } while (lock.read_retry(s));
        if (out.bid_fp == INT64_MIN || out.ask_fp == INT64_MAX) {
            out.valid = false; return out;
        }
        out.mid_fp    = (out.bid_fp + out.ask_fp) / 2;
        out.spread_fp = out.ask_fp - out.bid_fp;
        out.valid     = (flags & 1);  // in_sync flag
        return out;
    }

    void set_in_sync(bool v) noexcept {
        lock.write_begin();
        if (v) flags |=  0x01;
        else   flags &= ~0x01;
        lock.write_end();
    }
    void set_halted(bool v) noexcept {
        lock.write_begin();
        if (v) flags |=  0x02;
        else   flags &= ~0x02;
        lock.write_end();
    }
    [[nodiscard]] bool in_sync()  const noexcept { return flags & 0x01; }
    [[nodiscard]] bool is_halted()const noexcept { return flags & 0x02; }
};

// ============================================================================
// §8b  BOOK REGISTRY
//      Central store of SoA books indexed by InstrumentID.
//      Allocated on NUMA node 1 (closer to strategy threads).
// ============================================================================
static constexpr uint32_t MAX_INSTRUMENTS = 4096;

class BookRegistry {
public:
    BookRegistry() {
        books_ = static_cast<SoABook*>(
            alloc_on_node(sizeof(SoABook) * MAX_INSTRUMENTS, /*numa=*/1));
        assert(books_);
        std::memset(books_, 0, sizeof(SoABook) * MAX_INSTRUMENTS);
    }
    ~BookRegistry() {
        free_numa(books_, sizeof(SoABook) * MAX_INSTRUMENTS);
    }

    // Register a new instrument, returns its InstrumentID
    InstrumentID register_instrument(std::string_view symbol) {
        InstrumentID id = next_id_++;
        assert(id < MAX_INSTRUMENTS);
        books_[id].init(id, symbol);
        return id;
    }

    [[nodiscard]] SoABook& book(InstrumentID id) noexcept {
        return books_[id];
    }
    [[nodiscard]] const SoABook& book(InstrumentID id) const noexcept {
        return books_[id];
    }
    [[nodiscard]] uint32_t count() const noexcept { return next_id_; }

private:
    SoABook* books_{nullptr};
    uint32_t next_id_{0};
};

// ============================================================================
// §8c  MARKET DATA EVENT (payload for SPMC ring buffer)
//      Every event is exactly 64 bytes (1 cache line).
// ============================================================================
enum class EventType : uint8_t {
    BookL1   = 0,  // L1 best bid/ask changed
    BookL2   = 1,  // L2 depth update (one level)
    Trade    = 2,  // trade execution
    Status   = 3,  // security status change (halt/resume/open/close)
    Snapshot = 4,  // full book snapshot applied
};

struct alignas(CACHE_LINE) MarketEvent {
    // ---- HOT: first 32 bytes (most fields needed for signal) -----------
    uint64_t     timestamp_ns;   //  8  hardware timestamp
    InstrumentID instrument_id;  //  4
    EventType    type;           //  1
    uint8_t      market_id;      //  1  MarketID cast to uint8
    uint8_t      side;           //  1  Side cast to uint8 (for trade/L2)
    uint8_t      flags;          //  1
    int64_t      price_fp;       //  8  bid/ask/trade price
    int32_t      qty;            //  4  bid/ask/trade qty
    int32_t      level;          //  4  book level (0-based) for L2

    // ---- Extra L2/snapshot context (second 32 bytes) -------------------
    int64_t      price2_fp;      //  8  ask price (for L1 events: best ask)
    int32_t      qty2;           //  4  ask qty
    int32_t      orders;         //  4  number of orders at level
    uint32_t     seq_num;        //  4  exchange sequence number
    uint32_t     reserved;       //  4  alignment padding / future use
    char         symbol[8];      //  8  null-terminated
};
static_assert(sizeof(MarketEvent) == CACHE_LINE, "MarketEvent must be 64 bytes");

}  // namespace ull

