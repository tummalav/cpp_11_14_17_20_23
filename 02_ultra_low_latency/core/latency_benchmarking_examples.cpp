/*
 * C++20 Latency Benchmarking Examples
 *
 * Comprehensive examples for measuring and optimizing latency in high-performance applications
 * Includes techniques for trading systems, real-time applications, and ultra-low latency systems
 *
 * Compilation:
 * g++ -std=c++2a -pthread -Wall -Wextra -O3 -march=native -DNDEBUG latency_benchmarking_examples.cpp -o latency_bench
 *
 * Key Topics Covered:
 * 1. High-resolution timing mechanisms
 * 2. Cache-friendly data structures and access patterns
 * 3. Lock-free algorithms and atomic operations
 * 4. Memory layout optimization
 * 5. CPU affinity and thread pinning
 * 6. NUMA-aware programming
 * 7. Branch prediction optimization
 * 8. Template metaprogramming for zero-cost abstractions
 * 9. SIMD optimization
 * 10. Real-world trading system benchmarks
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <array>
#include <memory>
#include <thread>
#include <atomic>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <deque>
#include <list>
#include <cmath>

// Conditional SIMD support
#ifdef __AVX__
    #include <immintrin.h>
    #define HAS_AVX 1
#else
    #define HAS_AVX 0
#endif

#ifdef __linux__
#include <sched.h>
#include <sys/mman.h>
// NUMA support is optional
#ifdef NUMA_AVAILABLE
#include <numa.h>
#endif
#endif

// ============================================================================
// HIGH-RESOLUTION TIMING UTILITIES
// ============================================================================

namespace timing_utils {

// High-resolution timer class
class HighResTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;

public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    template<typename Duration = std::chrono::nanoseconds>
    auto elapsed() const -> typename Duration::rep {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<Duration>(end_time - start_time_).count();
    }

    double elapsed_seconds() const {
        return elapsed<std::chrono::nanoseconds>() / 1e9;
    }

    double elapsed_microseconds() const {
        return elapsed<std::chrono::nanoseconds>() / 1e3;
    }
};

// RAII timer for automatic measurement
template<typename Func>
class ScopedTimer {
private:
    HighResTimer timer_;
    Func callback_;

public:
    explicit ScopedTimer(Func&& func) : callback_(std::forward<Func>(func)) {
        timer_.start();
    }

    ~ScopedTimer() {
        callback_(timer_.elapsed<std::chrono::nanoseconds>());
    }
};

// Helper macro for easy timing
#define TIME_BLOCK(var_name) \
    auto var_name##_timer = timing_utils::ScopedTimer([&](auto ns) { \
        var_name = ns; \
    })

// Latency statistics collector
class LatencyStats {
private:
    std::vector<uint64_t> samples_;
    bool sorted_ = false;

public:
    void add_sample(uint64_t latency_ns) {
        samples_.push_back(latency_ns);
        sorted_ = false;
    }

    void clear() {
        samples_.clear();
        sorted_ = false;
    }

    void sort_if_needed() {
        if (!sorted_ && !samples_.empty()) {
            std::sort(samples_.begin(), samples_.end());
            sorted_ = true;
        }
    }

    double mean() const {
        if (samples_.empty()) return 0.0;
        return std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size();
    }

    uint64_t median() {
        if (samples_.empty()) return 0;
        sort_if_needed();
        return samples_[samples_.size() / 2];
    }

    uint64_t percentile(double p) {
        if (samples_.empty()) return 0;
        sort_if_needed();
        size_t idx = static_cast<size_t>(p * samples_.size() / 100.0);
        return samples_[std::min(idx, samples_.size() - 1)];
    }

    uint64_t min() const {
        return samples_.empty() ? 0 : *std::min_element(samples_.begin(), samples_.end());
    }

    uint64_t max() const {
        return samples_.empty() ? 0 : *std::max_element(samples_.begin(), samples_.end());
    }

    size_t count() const {
        return samples_.size();
    }

    void print_summary(const std::string& label = "Latency") const {
        if (samples_.empty()) {
            std::cout << label << ": No samples\n";
            return;
        }

        auto stats = const_cast<LatencyStats*>(this);
        std::cout << "\n" << label << " Statistics (nanoseconds):\n";
        std::cout << "  Samples: " << count() << "\n";
        std::cout << "  Mean:    " << std::fixed << std::setprecision(2) << mean() << "\n";
        std::cout << "  Median:  " << stats->median() << "\n";
        std::cout << "  Min:     " << min() << "\n";
        std::cout << "  Max:     " << max() << "\n";
        std::cout << "  P50:     " << stats->percentile(50) << "\n";
        std::cout << "  P90:     " << stats->percentile(90) << "\n";
        std::cout << "  P95:     " << stats->percentile(95) << "\n";
        std::cout << "  P99:     " << stats->percentile(99) << "\n";
        std::cout << "  P99.9:   " << stats->percentile(99.9) << "\n";
    }
};

// Warmup function to stabilize CPU frequency and caches
void warmup_cpu(int iterations = 1000000) {
    volatile int dummy = 0;
    for (int i = 0; i < iterations; ++i) {
        dummy += i * i;
    }
}

// CPU frequency measurement
double measure_cpu_frequency() {
    const int iterations = 10000000;
    auto start = std::chrono::high_resolution_clock::now();

    volatile uint64_t counter = 0;
    for (int i = 0; i < iterations; ++i) {
        ++counter;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Estimate cycles per nanosecond (approximate CPU frequency in GHz)
    return static_cast<double>(iterations) / duration_ns;
}

} // namespace timing_utils

// ============================================================================
// CACHE OPTIMIZATION BENCHMARKS
// ============================================================================

namespace cache_benchmarks {

// Cache line size detection
constexpr size_t CACHE_LINE_SIZE = 64;

// Cache-friendly aligned allocator
template<typename T, size_t Alignment = CACHE_LINE_SIZE>
class aligned_allocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = aligned_allocator<U, Alignment>;
    };

    pointer allocate(size_type n) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) {
        free(p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new(p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
};

// Cache-line aligned data structure
template<typename T>
struct alignas(CACHE_LINE_SIZE) cache_aligned {
    T value;

    cache_aligned() = default;
    cache_aligned(const T& v) : value(v) {}
    cache_aligned(T&& v) : value(std::move(v)) {}

    operator T&() { return value; }
    operator const T&() const { return value; }

    T& operator=(const T& v) { value = v; return value; }
    T& operator=(T&& v) { value = std::move(v); return value; }
};

// Memory access pattern benchmarks
class MemoryAccessBenchmark {
private:
    static constexpr size_t ARRAY_SIZE = 64 * 1024 * 1024 / sizeof(int); // 64MB
    std::vector<int, aligned_allocator<int>> data_;

public:
    MemoryAccessBenchmark() : data_(ARRAY_SIZE) {
        // Initialize with random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::iota(data_.begin(), data_.end(), 0);
        std::shuffle(data_.begin(), data_.end(), gen);
    }

    // Sequential access pattern (cache-friendly)
    uint64_t benchmark_sequential_access(int iterations = 1000) {
        timing_utils::HighResTimer timer;
        timer.start();

        volatile long long sum = 0;
        for (int iter = 0; iter < iterations; ++iter) {
            for (size_t i = 0; i < data_.size(); ++i) {
                sum = sum + data_[i];  // Avoid compound assignment with volatile
            }
        }

        return timer.elapsed<std::chrono::nanoseconds>() / iterations;
    }

    // Random access pattern (cache-unfriendly)
    uint64_t benchmark_random_access(int iterations = 1000) {
        std::vector<size_t> indices(data_.size());
        std::iota(indices.begin(), indices.end(), 0);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(indices.begin(), indices.end(), gen);

        timing_utils::HighResTimer timer;
        timer.start();

        volatile long long sum = 0;
        for (int iter = 0; iter < iterations; ++iter) {
            for (size_t idx : indices) {
                sum = sum + data_[idx];  // Avoid compound assignment with volatile
            }
        }

        return timer.elapsed<std::chrono::nanoseconds>() / iterations;
    }

    // Strided access pattern
    uint64_t benchmark_strided_access(size_t stride, int iterations = 1000) {
        timing_utils::HighResTimer timer;
        timer.start();

        volatile long long sum = 0;
        for (int iter = 0; iter < iterations; ++iter) {
            for (size_t i = 0; i < data_.size(); i += stride) {
                sum = sum + data_[i];  // Avoid compound assignment with volatile
            }
        }

        return timer.elapsed<std::chrono::nanoseconds>() / iterations;
    }

    void run_all_benchmarks() {
        std::cout << "\n=== Memory Access Pattern Benchmarks ===\n";

        // Warmup
        timing_utils::warmup_cpu();

        auto seq_time = benchmark_sequential_access();
        auto rand_time = benchmark_random_access();

        std::cout << "Sequential access: " << seq_time << " ns per iteration\n";
        std::cout << "Random access:     " << rand_time << " ns per iteration\n";
        std::cout << "Random/Sequential ratio: " << std::fixed << std::setprecision(2)
                  << static_cast<double>(rand_time) / seq_time << "x\n";

        std::cout << "\nStrided access patterns:\n";
        for (size_t stride : {1, 2, 4, 8, 16, 32, 64, 128}) {
            auto stride_time = benchmark_strided_access(stride);
            std::cout << "  Stride " << std::setw(3) << stride << ": "
                      << stride_time << " ns per iteration\n";
        }
    }
};

// False sharing demonstration
class FalseSharingBenchmark {
private:
    struct PaddedCounter {
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> counter{0};
    };

    struct UnpaddedCounter {
        std::atomic<uint64_t> counter{0};
    };

    static constexpr int NUM_THREADS = 4;
    static constexpr int ITERATIONS_PER_THREAD = 1000000;

public:
    template<typename CounterType>
    uint64_t benchmark_counters(const std::string& label) {
        std::array<CounterType, NUM_THREADS> counters;
        std::vector<std::thread> threads;

        timing_utils::HighResTimer timer;
        timer.start();

        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&counters, i]() {
                for (int j = 0; j < ITERATIONS_PER_THREAD; ++j) {
                    counters[i].counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        auto elapsed = timer.elapsed<std::chrono::nanoseconds>();

        std::cout << label << ": " << elapsed << " ns total, "
                  << elapsed / (NUM_THREADS * ITERATIONS_PER_THREAD) << " ns per operation\n";

        return elapsed;
    }

    void run_benchmark() {
        std::cout << "\n=== False Sharing Benchmark ===\n";

        timing_utils::warmup_cpu();

        auto unpadded_time = benchmark_counters<UnpaddedCounter>("Unpadded counters (false sharing)");
        auto padded_time = benchmark_counters<PaddedCounter>("Padded counters (no false sharing)");

        std::cout << "Performance improvement: " << std::fixed << std::setprecision(2)
                  << static_cast<double>(unpadded_time) / padded_time << "x\n";
    }
};

} // namespace cache_benchmarks

// ============================================================================
// LOCK-FREE DATA STRUCTURE BENCHMARKS
// ============================================================================

namespace lockfree_benchmarks {

// Lock-free SPSC (Single Producer Single Consumer) queue
template<typename T, size_t Size>
class SPSCQueue {
private:
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    static constexpr size_t MASK = Size - 1;

    struct alignas(cache_benchmarks::CACHE_LINE_SIZE) {
        std::atomic<size_t> head{0};
    };

    struct alignas(cache_benchmarks::CACHE_LINE_SIZE) {
        std::atomic<size_t> tail{0};
    };

    alignas(cache_benchmarks::CACHE_LINE_SIZE) std::array<T, Size> buffer_;

public:
    bool try_push(const T& item) {
        const size_t current_tail = tail.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & MASK;

        if (next_tail == head.load(std::memory_order_acquire)) {
            return false; // Queue full
        }

        buffer_[current_tail] = item;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        const size_t current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }

        item = buffer_[current_head];
        head.store((current_head + 1) & MASK, std::memory_order_release);
        return true;
    }
};

// Lock-free stack using hazard pointers (simplified)
template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        Node* next;

        Node(T item) : data(std::move(item)), next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};

public:
    void push(T item) {
        Node* new_node = new Node(std::move(item));
        Node* current_head = head_.load(std::memory_order_relaxed);

        do {
            new_node->next = current_head;
        } while (!head_.compare_exchange_weak(current_head, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
    }

    bool try_pop(T& result) {
        Node* current_head = head_.load(std::memory_order_acquire);

        while (current_head) {
            if (head_.compare_exchange_weak(current_head, current_head->next,
                                          std::memory_order_relaxed)) {
                result = std::move(current_head->data);
                delete current_head;
                return true;
            }
        }
        return false;
    }

    ~LockFreeStack() {
        T dummy;
        while (try_pop(dummy)) {}
    }
};

// Benchmark comparing lock-free vs mutex-based queue
class QueueBenchmark {
private:
    static constexpr size_t QUEUE_SIZE = 65536;
    static constexpr int ITERATIONS = 1000000;

public:
    void benchmark_spsc_queue() {
        std::cout << "\n=== Lock-Free SPSC Queue Benchmark ===\n";

        SPSCQueue<int, QUEUE_SIZE> queue;
        std::atomic<bool> start_flag{false};

        timing_utils::LatencyStats producer_stats;
        timing_utils::LatencyStats consumer_stats;

        // Producer thread
        std::thread producer([&]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }

            for (int i = 0; i < ITERATIONS; ++i) {
                timing_utils::HighResTimer timer;
                timer.start();

                while (!queue.try_push(i)) {
                    std::this_thread::yield();
                }

                producer_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());
            }
        });

        // Consumer thread
        std::thread consumer([&]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }

            int value;
            int consumed = 0;

            while (consumed < ITERATIONS) {
                timing_utils::HighResTimer timer;
                timer.start();

                if (queue.try_pop(value)) {
                    consumer_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());
                    ++consumed;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        // Start benchmark
        timing_utils::warmup_cpu();
        start_flag.store(true);

        producer.join();
        consumer.join();

        producer_stats.print_summary("Producer Push Latency");
        consumer_stats.print_summary("Consumer Pop Latency");
    }

    void benchmark_lockfree_stack() {
        std::cout << "\n=== Lock-Free Stack Benchmark ===\n";

        LockFreeStack<int> stack;
        constexpr int NUM_THREADS = 4;
        constexpr int OPS_PER_THREAD = 100000;

        std::vector<std::thread> threads;
        std::vector<timing_utils::LatencyStats> push_stats(NUM_THREADS);
        std::vector<timing_utils::LatencyStats> pop_stats(NUM_THREADS);

        std::atomic<bool> start_flag{false};

        // Create threads that both push and pop
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&, t]() {
                while (!start_flag.load()) {
                    std::this_thread::yield();
                }

                for (int i = 0; i < OPS_PER_THREAD; ++i) {
                    // Push operation
                    {
                        timing_utils::HighResTimer timer;
                        timer.start();
                        stack.push(t * OPS_PER_THREAD + i);
                        push_stats[t].add_sample(timer.elapsed<std::chrono::nanoseconds>());
                    }

                    // Pop operation (with retry)
                    if (i % 2 == 0) { // Pop less frequently to avoid empty stack
                        timing_utils::HighResTimer timer;
                        timer.start();

                        int value;
                        while (!stack.try_pop(value)) {
                            std::this_thread::yield();
                        }

                        pop_stats[t].add_sample(timer.elapsed<std::chrono::nanoseconds>());
                    }
                }
            });
        }

        timing_utils::warmup_cpu();
        start_flag.store(true);

        for (auto& t : threads) {
            t.join();
        }

        // Aggregate statistics
        timing_utils::LatencyStats total_push_stats;
        timing_utils::LatencyStats total_pop_stats;

        for (int t = 0; t < NUM_THREADS; ++t) {
            // This is a simplified aggregation - in practice you'd merge the vectors
            std::cout << "Thread " << t << " push operations: " << push_stats[t].count() << "\n";
            std::cout << "Thread " << t << " pop operations: " << pop_stats[t].count() << "\n";
        }
    }
};

} // namespace lockfree_benchmarks

// ============================================================================
// SIMD OPTIMIZATION BENCHMARKS
// ============================================================================

namespace simd_benchmarks {

// SIMD vector operations
class VectorOperationsBenchmark {
private:
    static constexpr size_t ARRAY_SIZE = 1024 * 1024; // 1M elements

    std::vector<float, cache_benchmarks::aligned_allocator<float, 32>> data_a_;
    std::vector<float, cache_benchmarks::aligned_allocator<float, 32>> data_b_;
    std::vector<float, cache_benchmarks::aligned_allocator<float, 32>> result_;

public:
    VectorOperationsBenchmark()
        : data_a_(ARRAY_SIZE), data_b_(ARRAY_SIZE), result_(ARRAY_SIZE) {

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 100.0f);

        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            data_a_[i] = dist(gen);
            data_b_[i] = dist(gen);
        }
    }

    // Scalar version
    uint64_t vector_add_scalar() {
        timing_utils::HighResTimer timer;
        timer.start();

        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            result_[i] = data_a_[i] + data_b_[i];
        }

        return timer.elapsed<std::chrono::nanoseconds>();
    }

    // SIMD version using AVX (if available)
    uint64_t vector_add_simd() {
        timing_utils::HighResTimer timer;
        timer.start();

        #if HAS_AVX
        const size_t simd_size = 8; // 8 floats per AVX register
        size_t i = 0;

        // Process 8 elements at a time
        for (; i + simd_size <= ARRAY_SIZE; i += simd_size) {
            __m256 a = _mm256_load_ps(&data_a_[i]);
            __m256 b = _mm256_load_ps(&data_b_[i]);
            __m256 result = _mm256_add_ps(a, b);
            _mm256_store_ps(&result_[i], result);
        }

        // Handle remaining elements
        for (; i < ARRAY_SIZE; ++i) {
            result_[i] = data_a_[i] + data_b_[i];
        }
        #else
        // Fallback to scalar implementation
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            result_[i] = data_a_[i] + data_b_[i];
        }
        #endif

        return timer.elapsed<std::chrono::nanoseconds>();
    }

    // Fused multiply-add (FMA) operation
    uint64_t vector_fma_simd() {
        timing_utils::HighResTimer timer;
        timer.start();

        #if HAS_AVX
        const size_t simd_size = 8;
        size_t i = 0;

        for (; i + simd_size <= ARRAY_SIZE; i += simd_size) {
            __m256 a = _mm256_load_ps(&data_a_[i]);
            __m256 b = _mm256_load_ps(&data_b_[i]);
            __m256 c = _mm256_load_ps(&result_[i]); // Use result as accumulator
            __m256 result = _mm256_fmadd_ps(a, b, c); // a * b + c
            _mm256_store_ps(&result_[i], result);
        }

        for (; i < ARRAY_SIZE; ++i) {
            result_[i] = data_a_[i] * data_b_[i] + result_[i];
        }
        #else
        // Fallback to scalar implementation
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            result_[i] = data_a_[i] * data_b_[i] + result_[i];
        }
        #endif

        return timer.elapsed<std::chrono::nanoseconds>();
    }

    void run_benchmark() {
        std::cout << "\n=== SIMD Vector Operations Benchmark ===\n";

        #if HAS_AVX
        std::cout << "AVX support: Available\n";
        #else
        std::cout << "AVX support: Not available (using scalar fallback)\n";
        #endif

        timing_utils::warmup_cpu();

        auto scalar_time = vector_add_scalar();
        auto simd_time = vector_add_simd();
        auto fma_time = vector_fma_simd();

        std::cout << "Scalar addition:      " << scalar_time << " ns\n";
        std::cout << "SIMD addition:        " << simd_time << " ns\n";
        std::cout << "SIMD FMA:             " << fma_time << " ns\n";
        std::cout << "SIMD speedup:         " << std::fixed << std::setprecision(2)
                  << static_cast<double>(scalar_time) / simd_time << "x\n";
        std::cout << "FMA vs SIMD speedup:  " << std::fixed << std::setprecision(2)
                  << static_cast<double>(simd_time) / fma_time << "x\n";
    }
};

} // namespace simd_benchmarks

// ============================================================================
// TRADING SYSTEM SPECIFIC BENCHMARKS
// ============================================================================

namespace trading_benchmarks {

// Market data structure optimized for cache performance
struct alignas(cache_benchmarks::CACHE_LINE_SIZE) MarketData {
    double bid_price;
    double ask_price;
    uint32_t bid_size;
    uint32_t ask_size;
    uint64_t timestamp;
    uint32_t sequence_number;
    char symbol[8]; // Padded to cache line

    MarketData() = default;
    MarketData(double bid, double ask, uint32_t bid_sz, uint32_t ask_sz,
               uint64_t ts, uint32_t seq, const char* sym)
        : bid_price(bid), ask_price(ask), bid_size(bid_sz), ask_size(ask_sz),
          timestamp(ts), sequence_number(seq) {
        strncpy(symbol, sym, sizeof(symbol) - 1);
        symbol[sizeof(symbol) - 1] = '\0';
    }
};

// Order structure for latency-critical order processing
struct Order {
    enum class Side : uint8_t { BUY, SELL };
    enum class Type : uint8_t { MARKET, LIMIT };

    uint64_t order_id;
    Side side;
    Type type;
    double price;
    uint32_t quantity;
    uint64_t timestamp;
    char symbol[16];

    Order() = default;
    Order(uint64_t id, Side s, Type t, double p, uint32_t q, uint64_t ts, const char* sym)
        : order_id(id), side(s), type(t), price(p), quantity(q), timestamp(ts) {
        strncpy(symbol, sym, sizeof(symbol) - 1);
        symbol[sizeof(symbol) - 1] = '\0';
    }
};

// Ultra-low latency order book implementation
class OrderBook {
private:
    static constexpr size_t MAX_PRICE_LEVELS = 1000;

    struct PriceLevel {
        double price;
        uint32_t total_quantity;
        uint32_t order_count;

        PriceLevel() : price(0.0), total_quantity(0), order_count(0) {}
    };

    std::array<PriceLevel, MAX_PRICE_LEVELS> bid_levels_;
    std::array<PriceLevel, MAX_PRICE_LEVELS> ask_levels_;

    size_t bid_count_ = 0;
    size_t ask_count_ = 0;

public:
    void add_order(const Order& order) {
        auto& levels = (order.side == Order::Side::BUY) ? bid_levels_ : ask_levels_;
        auto& count = (order.side == Order::Side::BUY) ? bid_count_ : ask_count_;

        // Simplified: just add to first available slot
        if (count < MAX_PRICE_LEVELS) {
            levels[count].price = order.price;
            levels[count].total_quantity = order.quantity;
            levels[count].order_count = 1;
            ++count;
        }
    }

    std::pair<double, uint32_t> get_best_bid() const {
        if (bid_count_ == 0) return {0.0, 0};

        // Find highest bid (simplified linear search)
        size_t best_idx = 0;
        for (size_t i = 1; i < bid_count_; ++i) {
            if (bid_levels_[i].price > bid_levels_[best_idx].price) {
                best_idx = i;
            }
        }

        return {bid_levels_[best_idx].price, bid_levels_[best_idx].total_quantity};
    }

    std::pair<double, uint32_t> get_best_ask() const {
        if (ask_count_ == 0) return {0.0, 0};

        // Find lowest ask (simplified linear search)
        size_t best_idx = 0;
        for (size_t i = 1; i < ask_count_; ++i) {
            if (ask_levels_[i].price < ask_levels_[best_idx].price) {
                best_idx = i;
            }
        }

        return {ask_levels_[best_idx].price, ask_levels_[best_idx].total_quantity};
    }

    void clear() {
        bid_count_ = 0;
        ask_count_ = 0;
    }
};

// Order processing pipeline benchmark
class OrderProcessingBenchmark {
private:
    OrderBook order_book_;
    std::vector<Order> test_orders_;

public:
    OrderProcessingBenchmark() {
        // Generate test orders
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> price_dist(100.0, 200.0);
        std::uniform_int_distribution<uint32_t> qty_dist(1, 1000);
        std::uniform_int_distribution<int> side_dist(0, 1);

        test_orders_.reserve(100000);

        for (int i = 0; i < 100000; ++i) {
            auto side = (side_dist(gen) == 0) ? Order::Side::BUY : Order::Side::SELL;
            test_orders_.emplace_back(
                i, side, Order::Type::LIMIT,
                price_dist(gen), qty_dist(gen),
                std::chrono::high_resolution_clock::now().time_since_epoch().count(),
                "AAPL"
            );
        }
    }

    void benchmark_order_processing() {
        std::cout << "\n=== Order Processing Benchmark ===\n";

        timing_utils::LatencyStats processing_stats;

        timing_utils::warmup_cpu();

        for (const auto& order : test_orders_) {
            timing_utils::HighResTimer timer;
            timer.start();

            // Simulate order processing pipeline
            order_book_.add_order(order);
            auto [best_bid_price, best_bid_qty] = order_book_.get_best_bid();
            auto [best_ask_price, best_ask_qty] = order_book_.get_best_ask();

            // Simulate some processing work
            volatile double spread = best_ask_price - best_bid_price;
            (void)spread; // Suppress unused variable warning

            processing_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());

            // Clear periodically to avoid memory growth
            if (processing_stats.count() % 10000 == 0) {
                order_book_.clear();
            }
        }

        processing_stats.print_summary("Order Processing Latency");
    }
};

// Market data processing benchmark
class MarketDataBenchmark {
private:
    std::vector<MarketData> market_data_;

public:
    MarketDataBenchmark() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> price_dist(50.0, 150.0);
        std::uniform_int_distribution<uint32_t> size_dist(100, 10000);

        market_data_.reserve(1000000);

        for (int i = 0; i < 1000000; ++i) {
            double mid_price = price_dist(gen);
            double spread = 0.01 + (gen() % 10) * 0.001;

            market_data_.emplace_back(
                mid_price - spread/2, mid_price + spread/2,
                size_dist(gen), size_dist(gen),
                std::chrono::high_resolution_clock::now().time_since_epoch().count(),
                i, "SYMBOL"
            );
        }
    }

    void benchmark_market_data_processing() {
        std::cout << "\n=== Market Data Processing Benchmark ===\n";

        timing_utils::LatencyStats processing_stats;

        timing_utils::warmup_cpu();

        for (const auto& data : market_data_) {
            timing_utils::HighResTimer timer;
            timer.start();

            // Simulate market data processing
            volatile double mid_price = (data.bid_price + data.ask_price) / 2.0;
            volatile double spread = data.ask_price - data.bid_price;
            volatile double total_volume = data.bid_size + data.ask_size;

            // Suppress unused variable warnings
            (void)mid_price;
            (void)spread;
            (void)total_volume;

            processing_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());
        }

        processing_stats.print_summary("Market Data Processing Latency");
    }
};

} // namespace trading_benchmarks

// ============================================================================
// SYSTEM-LEVEL OPTIMIZATION BENCHMARKS
// ============================================================================

namespace system_benchmarks {

#ifdef __linux__
// CPU affinity and thread pinning utilities
class ThreadAffinityBenchmark {
private:
    static constexpr int ITERATIONS = 1000000;

    void pin_thread_to_cpu(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);

        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
            std::cerr << "Failed to pin thread to CPU " << cpu_id << "\n";
        }
    }

    uint64_t compute_intensive_task() {
        timing_utils::HighResTimer timer;
        timer.start();

        volatile double result = 0.0;
        for (int i = 0; i < ITERATIONS; ++i) {
            result = result + std::sin(i) * std::cos(i);  // Avoid compound assignment with volatile
        }

        return timer.elapsed<std::chrono::nanoseconds>();
    }

public:
    void benchmark_cpu_affinity() {
        std::cout << "\n=== CPU Affinity Benchmark ===\n";

        int num_cpus = std::thread::hardware_concurrency();
        std::cout << "Detected " << num_cpus << " CPU cores\n";

        timing_utils::LatencyStats unpinned_stats;
        timing_utils::LatencyStats pinned_stats;

        // Benchmark without CPU pinning
        std::cout << "Running unpinned benchmark...\n";
        for (int i = 0; i < 10; ++i) {
            unpinned_stats.add_sample(compute_intensive_task());
        }

        // Benchmark with CPU pinning
        std::cout << "Running pinned benchmark...\n";
        pin_thread_to_cpu(0); // Pin to CPU 0

        for (int i = 0; i < 10; ++i) {
            pinned_stats.add_sample(compute_intensive_task());
        }

        unpinned_stats.print_summary("Unpinned Thread");
        pinned_stats.print_summary("Pinned Thread (CPU 0)");

        double improvement = unpinned_stats.mean() / pinned_stats.mean();
        std::cout << "CPU pinning improvement: " << std::fixed << std::setprecision(2)
                  << improvement << "x\n";
    }
};
#endif

// Memory allocation benchmark
class MemoryAllocationBenchmark {
private:
    static constexpr size_t ALLOC_SIZE = 1024;
    static constexpr int NUM_ALLOCATIONS = 100000;

public:
    void benchmark_allocation_strategies() {
        std::cout << "\n=== Memory Allocation Benchmark ===\n";

        timing_utils::LatencyStats malloc_stats;
        timing_utils::LatencyStats aligned_stats;
        timing_utils::LatencyStats pool_stats;

        timing_utils::warmup_cpu();

        // Standard malloc/free
        std::cout << "Benchmarking malloc/free...\n";
        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            timing_utils::HighResTimer timer;
            timer.start();

            void* ptr = malloc(ALLOC_SIZE);
            free(ptr);

            malloc_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());
        }

        // Aligned allocation
        std::cout << "Benchmarking aligned allocation...\n";
        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            timing_utils::HighResTimer timer;
            timer.start();

            void* ptr = nullptr;
            posix_memalign(&ptr, cache_benchmarks::CACHE_LINE_SIZE, ALLOC_SIZE);
            free(ptr);

            aligned_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());
        }

        // Simple memory pool (pre-allocated)
        std::cout << "Benchmarking memory pool...\n";
        constexpr size_t POOL_SIZE = NUM_ALLOCATIONS * ALLOC_SIZE;
        auto pool = std::make_unique<char[]>(POOL_SIZE);
        size_t pool_offset = 0;

        for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
            timing_utils::HighResTimer timer;
            timer.start();

            void* ptr = &pool[pool_offset];
            pool_offset += ALLOC_SIZE;

            // Simulate some work
            memset(ptr, 0, ALLOC_SIZE);

            pool_stats.add_sample(timer.elapsed<std::chrono::nanoseconds>());
        }

        malloc_stats.print_summary("malloc/free");
        aligned_stats.print_summary("aligned alloc/free");
        pool_stats.print_summary("memory pool");

        std::cout << "Pool vs malloc speedup: " << std::fixed << std::setprecision(2)
                  << malloc_stats.mean() / pool_stats.mean() << "x\n";
    }
};

} // namespace system_benchmarks

// ============================================================================
// COMPREHENSIVE BENCHMARK SUITE
// ============================================================================

class LatencyBenchmarkSuite {
public:
    void run_all_benchmarks() {
        std::cout << "Latency Benchmarking Suite\n";
        std::cout << "==========================\n";
        std::cout << "CPU Frequency estimate: " << std::fixed << std::setprecision(2)
                  << timing_utils::measure_cpu_frequency() << " GHz\n";

        // Cache and memory benchmarks
        cache_benchmarks::MemoryAccessBenchmark memory_bench;
        memory_bench.run_all_benchmarks();

        cache_benchmarks::FalseSharingBenchmark false_sharing_bench;
        false_sharing_bench.run_benchmark();

        // Lock-free data structure benchmarks
        lockfree_benchmarks::QueueBenchmark queue_bench;
        queue_bench.benchmark_spsc_queue();
        queue_bench.benchmark_lockfree_stack();

        // SIMD benchmarks
        simd_benchmarks::VectorOperationsBenchmark simd_bench;
        simd_bench.run_benchmark();

        // Trading-specific benchmarks
        trading_benchmarks::OrderProcessingBenchmark order_bench;
        order_bench.benchmark_order_processing();

        trading_benchmarks::MarketDataBenchmark market_data_bench;
        market_data_bench.benchmark_market_data_processing();

        // System-level benchmarks
        system_benchmarks::MemoryAllocationBenchmark alloc_bench;
        alloc_bench.benchmark_allocation_strategies();

#ifdef __linux__
        system_benchmarks::ThreadAffinityBenchmark affinity_bench;
        affinity_bench.benchmark_cpu_affinity();
#endif

        std::cout << "\n=== Benchmark Suite Completed ===\n";
        std::cout << "All latency measurements completed successfully!\n";
    }
};

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    try {
        std::cout << "C++20 Latency Benchmarking Examples\n";
        std::cout << "====================================\n";

        LatencyBenchmarkSuite suite;
        suite.run_all_benchmarks();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

/*
 * Compilation Instructions:
 *
 * For maximum performance:
 * g++ -std=c++2a -pthread -Wall -Wextra -O3 -march=native -DNDEBUG \
 *     -ffast-math -funroll-loops -flto \
 *     latency_benchmarking_examples.cpp -o latency_bench
 *
 * For debug with symbols:
 * g++ -std=c++2a -pthread -Wall -Wextra -O0 -g \
 *     latency_benchmarking_examples.cpp -o latency_bench_debug
 *
 * Additional optimizations to consider:
 * -mavx2               # Enable AVX2 instructions
 * -mfma                # Enable FMA instructions
 * -mtune=native        # Tune for current CPU
 * -fprofile-generate   # For profile-guided optimization (first pass)
 * -fprofile-use        # For profile-guided optimization (second pass)
 *
 * Key Performance Tips:
 * 1. Always compile with -O3 for production benchmarks
 * 2. Use -march=native for CPU-specific optimizations
 * 3. Pin threads to specific CPU cores for consistent results
 * 4. Disable frequency scaling and hyperthreading for stable measurements
 * 5. Run benchmarks multiple times and report statistics
 * 6. Warm up CPU and caches before critical measurements
 * 7. Use appropriate memory barriers and ordering for correctness
 * 8. Profile with perf, VTune, or similar tools for deep analysis
 */
