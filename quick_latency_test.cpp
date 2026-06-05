/*
 * Quick Latency Benchmark Test
 * Simplified version for testing basic functionality
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <iomanip>

namespace quick_test {

class QuickTimer {
private:
    std::chrono::high_resolution_clock::time_point start_;

public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    uint64_t elapsed_ns() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
    }
};

void test_basic_timing() {
    std::cout << "=== Basic Timing Test ===\n";

    QuickTimer timer;
    timer.start();

    // Simple computation
    volatile int sum = 0;
    for (int i = 0; i < 1000000; ++i) {
        sum = sum + i;
    }

    auto elapsed = timer.elapsed_ns();
    std::cout << "1M iterations took: " << elapsed << " ns\n";
    std::cout << "Average per iteration: " << elapsed / 1000000.0 << " ns\n";
}

void test_atomic_operations() {
    std::cout << "\n=== Atomic Operations Test ===\n";

    std::atomic<int> counter{0};
    QuickTimer timer;
    timer.start();

    for (int i = 0; i < 100000; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }

    auto elapsed = timer.elapsed_ns();
    std::cout << "100K atomic increments took: " << elapsed << " ns\n";
    std::cout << "Average per atomic op: " << elapsed / 100000.0 << " ns\n";
    std::cout << "Final counter value: " << counter.load() << "\n";
}

void test_memory_access() {
    std::cout << "\n=== Memory Access Test ===\n";

    constexpr size_t ARRAY_SIZE = 1024 * 1024; // 1MB
    std::vector<int> data(ARRAY_SIZE);

    // Initialize
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        data[i] = static_cast<int>(i);
    }

    QuickTimer timer;
    timer.start();

    volatile long long sum = 0;
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        sum = sum + data[i];
    }

    auto elapsed = timer.elapsed_ns();
    std::cout << "Sequential access of " << ARRAY_SIZE << " elements took: " << elapsed << " ns\n";
    std::cout << "Average per access: " << elapsed / static_cast<double>(ARRAY_SIZE) << " ns\n";
    std::cout << "Sum: " << sum << "\n";
}

void test_multithreading() {
    std::cout << "\n=== Multithreading Test ===\n";

    std::atomic<int> shared_counter{0};
    constexpr int NUM_THREADS = 4;
    constexpr int INCREMENTS_PER_THREAD = 100000;

    std::vector<std::thread> threads;

    QuickTimer timer;
    timer.start();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&shared_counter]() {
            for (int i = 0; i < INCREMENTS_PER_THREAD; ++i) {
                shared_counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto elapsed = timer.elapsed_ns();
    std::cout << NUM_THREADS << " threads, " << INCREMENTS_PER_THREAD
              << " increments each took: " << elapsed << " ns\n";
    std::cout << "Average per increment: "
              << elapsed / static_cast<double>(NUM_THREADS * INCREMENTS_PER_THREAD) << " ns\n";
    std::cout << "Final counter: " << shared_counter.load()
              << " (expected: " << NUM_THREADS * INCREMENTS_PER_THREAD << ")\n";
}

} // namespace quick_test

int main() {
    std::cout << "Quick Latency Benchmark Test\n";
    std::cout << "============================\n";

    quick_test::test_basic_timing();
    quick_test::test_atomic_operations();
    quick_test::test_memory_access();
    quick_test::test_multithreading();

    std::cout << "\n=== Test Completed Successfully ===\n";
    return 0;
}
