/*
 * ============================================================================
 * CPU AFFINITY, NUMA AWARENESS & CORE ISOLATION - C++17 / Linux
 * ============================================================================
 *
 * For ultra-low latency trading systems, CPU affinity and NUMA tuning can
 * reduce latency from ~1-5 µs to < 100 ns by eliminating:
 *   - Context-switch overhead (OS preempting the trading thread)
 *   - Cache invalidation from OS moving threads between cores
 *   - NUMA remote memory access (2-4x slower than local node)
 *   - False sharing with OS kernel threads
 *
 * Topics Covered:
 *  1. CPU affinity (pthread_setaffinity_np, sched_setaffinity)
 *  2. NUMA topology discovery (libnuma)
 *  3. NUMA-aware memory allocation
 *  4. Huge pages (2MB / 1GB) for TLB pressure reduction
 *  5. Thread priority & scheduling policies (SCHED_FIFO, SCHED_RR, SCHED_DEADLINE)
 *  6. CPU isolation via /proc/sys/kernel/isolcpus
 *  7. RDTSC latency measurement
 *  8. Cache line alignment and false sharing prevention
 *  9. Memory prefetching
 *  10. Measuring NUMA effects
 *
 * Build:
 *   g++ -std=c++17 -O3 -march=native -pthread -lnuma \
 *       cpu_affinity_numa.cpp -o cpu_affinity_numa
 *
 * System Setup for Isolated Cores (root required):
 *   # In /etc/default/grub, add to GRUB_CMDLINE_LINUX:
 *   #   isolcpus=2,3,4,5 nohz_full=2,3,4,5 rcu_nocbs=2,3,4,5
 *   # Then: sudo update-grub && reboot
 * ============================================================================
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <functional>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <memory>
#include <fstream>
#include <optional>

// Linux-specific headers
#ifdef __linux__
#  include <pthread.h>
#  include <sched.h>
#  include <unistd.h>
#  include <sys/mman.h>
#  include <sys/syscall.h>
#  include <sys/resource.h>
#  include <linux/perf_event.h>
#  include <numaif.h>
#  include <numa.h>
#  pragma comment(lib, "numa")
#endif

// ============================================================================
// RDTSC - Read Time Stamp Counter (fastest timer, ~5 ns)
// ============================================================================
inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#elif defined(__aarch64__)
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

// Serializing RDTSC (prevents out-of-order execution)
inline uint64_t rdtscp() noexcept {
#if defined(__x86_64__)
    uint32_t lo, hi, aux;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    return rdtsc();
#endif
}

// Get TSC frequency (cycles per second) by calibrating against steady_clock
uint64_t measure_tsc_frequency() {
    const int WARMUP   = 3;
    const int SAMPLES  = 5;
    double    sum_hz   = 0;

    for (int s = 0; s < WARMUP + SAMPLES; ++s) {
        auto wall0 = std::chrono::steady_clock::now();
        uint64_t tsc0 = rdtsc();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint64_t tsc1 = rdtsc();
        auto wall1 = std::chrono::steady_clock::now();

        double wall_ns = std::chrono::duration<double, std::nano>(wall1 - wall0).count();
        double tsc_diff = static_cast<double>(tsc1 - tsc0);
        if (s >= WARMUP)
            sum_hz += tsc_diff / (wall_ns / 1e9);
    }
    return static_cast<uint64_t>(sum_hz / SAMPLES);
}

// Convert TSC cycles to nanoseconds
struct TscTimer {
    uint64_t tsc_hz;
    explicit TscTimer(uint64_t hz) : tsc_hz(hz) {}

    [[nodiscard]] double cycles_to_ns(uint64_t cycles) const noexcept {
        return static_cast<double>(cycles) * 1e9 / static_cast<double>(tsc_hz);
    }
    [[nodiscard]] uint64_t ns_to_cycles(double ns) const noexcept {
        return static_cast<uint64_t>(ns * static_cast<double>(tsc_hz) / 1e9);
    }
};

void demonstrate_rdtsc() {
    std::cout << "\n=== 1. RDTSC Latency Timer ===\n";

    // Calibrate
    std::cout << "Calibrating TSC frequency...\n";
    uint64_t hz = measure_tsc_frequency();
    TscTimer timer(hz);
    std::cout << "TSC frequency: " << hz / 1'000'000 << " MHz\n";

    // Measure RDTSC overhead itself
    {
        const int N = 1'000'000;
        std::vector<uint64_t> samples;
        samples.reserve(N);
        for (int i = 0; i < N; ++i) {
            uint64_t t0 = rdtsc();
            uint64_t t1 = rdtsc();
            samples.push_back(t1 - t0);
        }
        std::sort(samples.begin(), samples.end());
        std::cout << "RDTSC self-timing overhead:\n";
        std::cout << "  min  : " << samples.front() << " cycles ("
                  << timer.cycles_to_ns(samples.front())  << " ns)\n";
        std::cout << "  p50  : " << samples[N/2]    << " cycles ("
                  << timer.cycles_to_ns(samples[N/2])     << " ns)\n";
        std::cout << "  p99  : " << samples[static_cast<size_t>(N*0.99)] << " cycles\n";
    }

    // Measure a typical operation
    volatile int x = 0;
    std::vector<double> latencies;
    for (int i = 0; i < 100'000; ++i) {
        uint64_t t0 = rdtsc();
        x = x + i;  // simple arithmetic
        uint64_t t1 = rdtsc();
        latencies.push_back(timer.cycles_to_ns(t1 - t0));
    }
    std::sort(latencies.begin(), latencies.end());
    std::cout << "  x += i latency p50: " << latencies[50000] << " ns\n";
}

// ============================================================================
// 2. CPU AFFINITY
// ============================================================================

#ifdef __linux__
// Pin the calling thread to a specific CPU core
bool pin_to_cpu(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

// Pin to a set of CPUs
bool pin_to_cpus(const std::vector<int>& cpu_ids) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int id : cpu_ids) CPU_SET(id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

// Get current affinity mask
std::vector<int> get_affinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    std::vector<int> result;
    for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuset)) result.push_back(i);
    }
    return result;
}

// Get number of logical CPUs
int num_cpus() {
    return static_cast<int>(std::thread::hardware_concurrency());
}

void demonstrate_cpu_affinity() {
    std::cout << "\n=== 2. CPU Affinity ===\n";
    std::cout << "Logical CPUs: " << num_cpus() << "\n";

    auto before = get_affinity();
    std::cout << "Current affinity mask: CPUs ";
    for (int c : before) std::cout << c << " ";
    std::cout << "\n";

    // Pin to core 0
    if (pin_to_cpu(0)) {
        std::cout << "Pinned to CPU 0\n";
    } else {
        std::cout << "Warning: could not pin to CPU 0 (need CAP_SYS_NICE or root)\n";
    }

    // Restore original affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int c : before) CPU_SET(c, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // Demo: dedicated trading thread
    std::atomic<uint64_t> latency_sum{0};
    std::atomic<int>      count{0};

    std::thread trading_thread([&latency_sum, &count]() {
        // Pin to isolated core 1 (in production: isolated, dedicated core)
        if (!pin_to_cpu(1 % num_cpus())) {
            std::cout << "  Note: could not isolate to core 1\n";
        }

        // Set SCHED_FIFO (real-time scheduling) - requires root
        struct sched_param sp{};
        sp.sched_priority = 90;
        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
            // Silently fall back - not running as root
        }

        // Simulate tight trading loop
        for (int i = 0; i < 100'000; ++i) {
            uint64_t t0 = rdtsc();
            volatile int x = i * 42;
            uint64_t t1 = rdtsc();
            latency_sum.fetch_add(t1 - t0, std::memory_order_relaxed);
            count.fetch_add(1, std::memory_order_relaxed);
            (void)x;
        }
    });
    trading_thread.join();

    uint64_t avg_cycles = latency_sum.load() / static_cast<uint64_t>(count.load());
    std::cout << "Avg loop iteration: " << avg_cycles << " cycles\n";
}
#else
void demonstrate_cpu_affinity() {
    std::cout << "\n=== 2. CPU Affinity (Linux only) ===\n";
    std::cout << "Platform not supported (Linux required)\n";
}
#endif

// ============================================================================
// 3. NUMA Topology & Awareness
// ============================================================================
#ifdef __linux__
void demonstrate_numa() {
    std::cout << "\n=== 3. NUMA Topology ===\n";

    if (numa_available() < 0) {
        std::cout << "NUMA not available on this system\n";
        return;
    }

    int n_nodes = numa_num_configured_nodes();
    int n_cpus  = numa_num_configured_cpus();
    std::cout << "NUMA nodes: " << n_nodes << "\n";
    std::cout << "CPUs:       " << n_cpus  << "\n";

    for (int node = 0; node < n_nodes; ++node) {
        long free_mb = 0, total_mb = 0;
        auto total = numa_node_size64(node, &free_mb);
        total_mb = total / (1024 * 1024);
        free_mb  = free_mb / (1024 * 1024);

        std::cout << "Node " << node << ": total=" << total_mb
                  << " MB, free=" << free_mb << " MB  CPUs: ";
        struct bitmask* cpus = numa_allocate_cpumask();
        numa_node_to_cpus(node, cpus);
        for (int c = 0; c < n_cpus; ++c) {
            if (numa_bitmask_isbitset(cpus, c)) std::cout << c << " ";
        }
        numa_free_cpumask(cpus);
        std::cout << "\n";
    }

    // Which node does this thread's CPU belong to?
    int my_cpu  = sched_getcpu();
    int my_node = numa_node_of_cpu(my_cpu);
    std::cout << "This thread: CPU=" << my_cpu << " NUMA node=" << my_node << "\n";
}

// NUMA-aware memory allocation
struct NumaAllocator {
    static void* alloc_on_node(size_t bytes, int node) {
        void* ptr = numa_alloc_onnode(bytes, node);
        if (!ptr) throw std::bad_alloc();
        return ptr;
    }

    static void* alloc_local(size_t bytes) {
        void* ptr = numa_alloc_local(bytes);
        if (!ptr) throw std::bad_alloc();
        return ptr;
    }

    static void* alloc_interleaved(size_t bytes) {
        // Useful for data accessed from multiple NUMA nodes
        void* ptr = numa_alloc_interleaved(bytes);
        if (!ptr) throw std::bad_alloc();
        return ptr;
    }

    static void free(void* ptr, size_t bytes) {
        numa_free(ptr, bytes);
    }
};

void benchmark_numa_access() {
    if (numa_available() < 0) return;
    int n_nodes = numa_num_configured_nodes();
    if (n_nodes < 2) {
        std::cout << "  (Single NUMA node - skipping cross-node benchmark)\n";
        return;
    }

    const size_t SIZE = 64 * 1024 * 1024;  // 64 MB
    const int    ITER = 10;

    std::cout << "\n  NUMA memory access latency comparison:\n";

    for (int alloc_node = 0; alloc_node < n_nodes; ++alloc_node) {
        char* mem = static_cast<char*>(
            NumaAllocator::alloc_on_node(SIZE, alloc_node));

        // Warm up
        std::memset(mem, 0, SIZE);

        uint64_t t0 = rdtsc(), t1;
        for (int i = 0; i < ITER; ++i) {
            for (size_t j = 0; j < SIZE; j += 64) {
                volatile char dummy = mem[j];
                (void)dummy;
            }
        }
        t1 = rdtsc();

        double ns_per_cacheline = static_cast<double>(t1 - t0) /
            (ITER * SIZE / 64) * 1e9 / measure_tsc_frequency();

        int my_node = numa_node_of_cpu(sched_getcpu());
        std::cout << "  Allocate-node=" << alloc_node
                  << (alloc_node == my_node ? " [LOCAL]" : " [REMOTE]")
                  << " access_ns/CL=" << std::fixed << std::setprecision(1)
                  << ns_per_cacheline << "\n";

        NumaAllocator::free(mem, SIZE);
    }
}
#else
void demonstrate_numa() {
    std::cout << "\n=== 3. NUMA Topology (Linux only) ===\n";
}
void benchmark_numa_access() {}
#endif

// ============================================================================
// 4. Huge Pages
// ============================================================================
#ifdef __linux__
void* alloc_huge_pages(size_t bytes) {
    // 2MB huge pages: MAP_HUGETLB flag
    void* ptr = mmap(nullptr, bytes,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                     -1, 0);
    if (ptr == MAP_FAILED) {
        // Fallback to regular pages
        ptr = mmap(nullptr, bytes,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    }
    return (ptr == MAP_FAILED) ? nullptr : ptr;
}

void free_huge_pages(void* ptr, size_t bytes) {
    if (ptr) munmap(ptr, bytes);
}

void demonstrate_huge_pages() {
    std::cout << "\n=== 4. Huge Pages ===\n";

    // Check if huge pages are available
    std::ifstream hf("/proc/meminfo");
    if (hf) {
        std::string line;
        while (std::getline(hf, line)) {
            if (line.find("HugePages") != std::string::npos) {
                std::cout << "  " << line << "\n";
            }
        }
    }

    const size_t ALLOC = 2 * 1024 * 1024;  // 2MB = 1 huge page
    void* hp = alloc_huge_pages(ALLOC);
    if (hp) {
        std::cout << "Allocated 2MB huge page at " << hp << "\n";

        // Lock in memory (prevent swapping)
        if (mlock(hp, ALLOC) == 0) {
            std::cout << "Memory locked (no swap)\n";
        } else {
            std::cout << "mlock failed (need RLIMIT_MEMLOCK or root)\n";
        }

        // Write and read
        char* arr = static_cast<char*>(hp);
        std::memset(arr, 0xAB, ALLOC);
        volatile char probe = arr[ALLOC / 2];
        std::cout << "Probe value: 0x" << std::hex
                  << static_cast<int>(probe) << std::dec << "\n";

        free_huge_pages(hp, ALLOC);
    } else {
        std::cout << "Huge page allocation failed (run: sudo sysctl -w vm.nr_hugepages=16)\n";
    }
}
#else
void demonstrate_huge_pages() {
    std::cout << "\n=== 4. Huge Pages (Linux only) ===\n";
}
#endif

// ============================================================================
// 5. Thread Priority & Real-Time Scheduling
// ============================================================================
#ifdef __linux__

std::string sched_policy_name(int policy) {
    switch (policy) {
        case SCHED_OTHER:    return "SCHED_OTHER (default)";
        case SCHED_FIFO:     return "SCHED_FIFO (real-time)";
        case SCHED_RR:       return "SCHED_RR (round-robin RT)";
        case SCHED_BATCH:    return "SCHED_BATCH (batch)";
        case SCHED_IDLE:     return "SCHED_IDLE (idle)";
        default:             return "UNKNOWN";
    }
}

void demonstrate_scheduling() {
    std::cout << "\n=== 5. Thread Scheduling Policies ===\n";

    // Current policy
    int policy = sched_getscheduler(0);
    std::cout << "Current policy: " << sched_policy_name(policy) << "\n";

    struct sched_param sp{};
    sched_getparam(0, &sp);
    std::cout << "Current priority: " << sp.sched_priority << "\n";

    // Priority ranges
    std::cout << "SCHED_FIFO priority range: ["
              << sched_get_priority_min(SCHED_FIFO) << ", "
              << sched_get_priority_max(SCHED_FIFO) << "]\n";
    std::cout << "SCHED_RR   priority range: ["
              << sched_get_priority_min(SCHED_RR) << ", "
              << sched_get_priority_max(SCHED_RR) << "]\n";

    // Try to set SCHED_FIFO
    sp.sched_priority = 80;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) {
        std::cout << "Set SCHED_FIFO priority=80 (real-time)\n";
        // Restore
        sp.sched_priority = 0;
        sched_setscheduler(0, SCHED_OTHER, &sp);
    } else {
        std::cout << "Cannot set SCHED_FIFO (need CAP_SYS_NICE or root)\n";
        std::cout << "In production: run with 'chrt -f 80 ./trading_app'\n";
    }

    // Nice value (for non-RT)
    int nice_val = getpriority(PRIO_PROCESS, 0);
    std::cout << "Nice value: " << nice_val << "\n";
}
#else
void demonstrate_scheduling() {
    std::cout << "\n=== 5. Thread Scheduling (Linux only) ===\n";
}
#endif

// ============================================================================
// 6. Cache Line Alignment & False Sharing
// ============================================================================
static constexpr size_t CACHE_LINE = 64;

// BAD: multiple atomics on same cache line -> false sharing
struct PaddedCounters_BAD {
    std::atomic<uint64_t> counter1;  // offset 0
    std::atomic<uint64_t> counter2;  // offset 8 - SAME cache line!
    std::atomic<uint64_t> counter3;  // offset 16
    std::atomic<uint64_t> counter4;  // offset 24
};

// GOOD: each counter on its own cache line
struct alignas(CACHE_LINE) AlignedCounter {
    std::atomic<uint64_t> value{0};
    char pad[CACHE_LINE - sizeof(std::atomic<uint64_t>)];
};

struct PaddedCounters_GOOD {
    AlignedCounter counter1;
    AlignedCounter counter2;
    AlignedCounter counter3;
    AlignedCounter counter4;
};

// C++17: std::hardware_destructive_interference_size
struct alignas(std::hardware_destructive_interference_size) ProducerState {
    std::atomic<uint64_t> write_idx{0};
    std::atomic<uint64_t> sequence {0};
    char pad[std::hardware_destructive_interference_size
             - 2 * sizeof(std::atomic<uint64_t>)];
};

struct alignas(std::hardware_destructive_interference_size) ConsumerState {
    std::atomic<uint64_t> read_idx{0};
    char pad[std::hardware_destructive_interference_size
             - sizeof(std::atomic<uint64_t>)];
};

void benchmark_false_sharing() {
    std::cout << "\n=== 6. Cache Line Alignment & False Sharing ===\n";
    std::cout << "hardware_destructive_interference_size: "
              << std::hardware_destructive_interference_size << " bytes\n";
    std::cout << "hardware_constructive_interference_size: "
              << std::hardware_constructive_interference_size << " bytes\n";
    std::cout << "sizeof(AlignedCounter) = " << sizeof(AlignedCounter)
              << " (one cache line)\n";
    std::cout << "sizeof(ProducerState)  = " << sizeof(ProducerState)  << "\n";
    std::cout << "sizeof(ConsumerState)  = " << sizeof(ConsumerState)  << "\n";

    const int N        = 1'000'000;
    const int N_THREADS = 4;

    // Benchmark BAD (false sharing)
    PaddedCounters_BAD bad;
    bad.counter1 = bad.counter2 = bad.counter3 = bad.counter4 = 0;

    auto t0_bad = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    threads.emplace_back([&]{ for (int i=0;i<N;++i) bad.counter1.fetch_add(1, std::memory_order_relaxed); });
    threads.emplace_back([&]{ for (int i=0;i<N;++i) bad.counter2.fetch_add(1, std::memory_order_relaxed); });
    threads.emplace_back([&]{ for (int i=0;i<N;++i) bad.counter3.fetch_add(1, std::memory_order_relaxed); });
    threads.emplace_back([&]{ for (int i=0;i<N;++i) bad.counter4.fetch_add(1, std::memory_order_relaxed); });
    for (auto& t : threads) t.join();
    auto t1_bad = std::chrono::steady_clock::now();

    // Benchmark GOOD (padded)
    PaddedCounters_GOOD good;
    auto t0_good = std::chrono::steady_clock::now();
    threads.clear();
    threads.emplace_back([&]{ for (int i=0;i<N;++i) good.counter1.value.fetch_add(1, std::memory_order_relaxed); });
    threads.emplace_back([&]{ for (int i=0;i<N;++i) good.counter2.value.fetch_add(1, std::memory_order_relaxed); });
    threads.emplace_back([&]{ for (int i=0;i<N;++i) good.counter3.value.fetch_add(1, std::memory_order_relaxed); });
    threads.emplace_back([&]{ for (int i=0;i<N;++i) good.counter4.value.fetch_add(1, std::memory_order_relaxed); });
    for (auto& t : threads) t.join();
    auto t1_good = std::chrono::steady_clock::now();

    auto bad_ms  = std::chrono::duration<double, std::milli>(t1_bad  - t0_bad).count();
    auto good_ms = std::chrono::duration<double, std::milli>(t1_good - t0_good).count();

    std::cout << "False-sharing (BAD):  " << std::fixed << std::setprecision(1)
              << bad_ms  << " ms\n";
    std::cout << "Padded counters (GOOD): " << good_ms << " ms\n";
    std::cout << "Speedup: " << std::setprecision(2) << bad_ms / good_ms << "x\n";
}

// ============================================================================
// 7. Memory Prefetching
// ============================================================================
void demonstrate_prefetch() {
    std::cout << "\n=== 7. Memory Prefetch ===\n";

    const int N = 16 * 1024 * 1024;  // 16M elements
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);

    // Shuffle for random access
    std::mt19937 rng(42);
    std::shuffle(data.begin(), data.end(), rng);

    // Benchmark without prefetch
    volatile int64_t sum = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        sum += data[i];
    }
    auto t1 = std::chrono::steady_clock::now();

    // Benchmark with prefetch (ahead by 16 cache lines)
    sum = 0;
    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        if (i + 16 < N)
            __builtin_prefetch(&data[i + 16], 0, 1);  // prefetch for read, medium locality
        sum += data[i];
    }
    auto t3 = std::chrono::steady_clock::now();

    double no_pf = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double with_pf= std::chrono::duration<double, std::milli>(t3 - t2).count();

    std::cout << "Sequential sum (no prefetch): " << no_pf   << " ms\n";
    std::cout << "Sequential sum (with prefetch): " << with_pf << " ms\n";
    std::cout << "Speedup: " << std::setprecision(2) << no_pf / with_pf << "x\n";
    std::cout << "sum=" << sum << " (to prevent optimization)\n";

    // Prefetch variants:
    // __builtin_prefetch(addr, rw, locality)
    //   rw:       0=read, 1=write
    //   locality: 0=none (evict after use), 1=L3, 2=L2, 3=L1
    // x86: translates to prefetch, prefetchw, prefetchnta instructions
    std::cout << "Prefetch hints: __builtin_prefetch(ptr, 0=read|1=write, 0..3=locality)\n";
}

// ============================================================================
// 8. Memory Ordering Costs (cache coherence protocol)
// ============================================================================
void demonstrate_memory_ordering_costs() {
    std::cout << "\n=== 8. Atomic Memory Ordering Costs ===\n";

    std::atomic<uint64_t> counter{0};
    const int N = 10'000'000;

    auto bench = [&](auto fn, const char* name) {
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) fn();
        auto t1 = std::chrono::steady_clock::now();
        double ns_each = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
        std::cout << "  " << std::setw(40) << std::left << name
                  << ": " << std::fixed << std::setprecision(1) << ns_each << " ns/op\n";
        counter.store(0, std::memory_order_seq_cst);
    };

    bench([&]{ counter.load(std::memory_order_relaxed); },
          "load(relaxed)");
    bench([&]{ counter.load(std::memory_order_acquire); },
          "load(acquire)");
    bench([&]{ counter.load(std::memory_order_seq_cst); },
          "load(seq_cst)");
    bench([&]{ counter.store(1, std::memory_order_relaxed); },
          "store(relaxed)");
    bench([&]{ counter.store(1, std::memory_order_release); },
          "store(release)");
    bench([&]{ counter.store(1, std::memory_order_seq_cst); },
          "store(seq_cst)");
    bench([&]{ counter.fetch_add(1, std::memory_order_relaxed); },
          "fetch_add(relaxed)");
    bench([&]{ counter.fetch_add(1, std::memory_order_seq_cst); },
          "fetch_add(seq_cst)");

    uint64_t    exp = 0;
    bench([&]{
        exp = counter.load(std::memory_order_relaxed);
        counter.compare_exchange_weak(exp, exp+1, std::memory_order_relaxed);
    }, "CAS(relaxed, success)");

    // Memory fence costs
    bench([&]{ std::atomic_thread_fence(std::memory_order_acquire); },
          "fence(acquire)");
    bench([&]{ std::atomic_thread_fence(std::memory_order_seq_cst); },
          "fence(seq_cst) = MFENCE");
}

// ============================================================================
// 9. Isolated Core Detection from /proc/cpuinfo and isolcpus
// ============================================================================
void print_cpu_topology() {
    std::cout << "\n=== 9. CPU Topology ===\n";

#ifdef __linux__
    // Read /proc/cpuinfo for physical/logical core mapping
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo) { std::cout << "Cannot read /proc/cpuinfo\n"; return; }

    struct CpuInfo {
        int processor{-1}, physical_id{-1}, core_id{-1}, apicid{-1};
    };
    std::vector<CpuInfo> cpus;
    CpuInfo cur;

    std::string line;
    while (std::getline(cpuinfo, line)) {
        auto split = [](const std::string& s, char delim) -> std::pair<std::string, std::string> {
            auto pos = s.find(delim);
            if (pos == std::string::npos) return {s, ""};
            return {s.substr(0, pos), s.substr(pos + 1)};
        };
        auto [key, val] = split(line, ':');
        // Trim whitespace
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(0, 1);

        if (key == "processor")   cur.processor   = std::stoi(val);
        else if (key == "physical id") cur.physical_id = std::stoi(val);
        else if (key == "core id")     cur.core_id     = std::stoi(val);
        else if (line.empty()) {
            if (cur.processor >= 0) {
                cpus.push_back(cur);
                cur = {};
            }
        }
    }
    if (cur.processor >= 0) cpus.push_back(cur);

    std::cout << std::setw(12) << "Processor"
              << std::setw(14) << "Physical CPU"
              << std::setw(10) << "Core"
              << "\n";
    std::cout << std::string(36, '-') << "\n";
    for (auto& c : cpus) {
        std::cout << std::setw(12) << c.processor
                  << std::setw(14) << c.physical_id
                  << std::setw(10) << c.core_id << "\n";
    }

    // Check for isolated CPUs
    std::ifstream iso("/sys/devices/system/cpu/isolated");
    if (iso) {
        std::string isolated;
        std::getline(iso, isolated);
        std::cout << "Isolated CPUs: " << (isolated.empty() ? "(none)" : isolated) << "\n";
    }
#else
    std::cout << "CPU topology info (Linux only)\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << "\n";
#endif
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "================================================\n";
    std::cout << "  CPU Affinity, NUMA & Core Isolation Guide\n";
    std::cout << "================================================\n";

    demonstrate_rdtsc();
    demonstrate_cpu_affinity();
    demonstrate_numa();
    benchmark_numa_access();
    demonstrate_huge_pages();
    demonstrate_scheduling();
    benchmark_false_sharing();
    demonstrate_prefetch();
    demonstrate_memory_ordering_costs();
    print_cpu_topology();

    std::cout << "\n================================================\n";
    std::cout << "  Production Deployment Checklist\n";
    std::cout << "================================================\n";
    std::cout << "1. Isolate CPU cores:     isolcpus=N nohz_full=N rcu_nocbs=N\n";
    std::cout << "2. Pin process to core:   taskset -c N ./trading_app\n";
    std::cout << "3. Real-time scheduling:  chrt -f 80 ./trading_app\n";
    std::cout << "4. Huge pages:            vm.nr_hugepages=64\n";
    std::cout << "5. NUMA binding:          numactl --cpunodebind=0 --membind=0 ./app\n";
    std::cout << "6. Disable power mgmt:    cpufreq-set -g performance\n";
    std::cout << "7. Disable hyperthreading: echo off > /sys/.../ht/state\n";
    std::cout << "8. Disable interrupts:    service irqbalance stop + manual IRQ routing\n";
    std::cout << "9. Disable C-states:      intel_idle.max_cstate=0 processor.max_cstate=0\n";
    std::cout << "10. Transparent hugepages: echo never > /sys/kernel/mm/transparent_hugepage/enabled\n";
    std::cout << "================================================\n";

    return 0;
}

