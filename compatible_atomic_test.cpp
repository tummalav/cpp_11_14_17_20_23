/*
 * C++20/C++17 Compatible Atomic Test
 * Testing atomic features with fallback compatibility
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

// Check for C++20 concepts support
#if __cplusplus >= 202002L && __has_include(<concepts>)
    #include <concepts>
    #define HAS_CPP20_CONCEPTS 1

    template<typename T>
    concept Numeric = std::is_arithmetic_v<T>;

    template<typename T>
    concept FloatingPoint = std::is_floating_point_v<T>;
#else
    #define HAS_CPP20_CONCEPTS 0
    // C++17 fallback - use SFINAE instead of concepts
    template<typename T>
    using enable_if_numeric_t = std::enable_if_t<std::is_arithmetic_v<T>, T>;
#endif

// Check for C++20 atomic floating point fetch_add support
#if __cplusplus >= 202002L
    #define CPP20_AVAILABLE 1
#else
    #define CPP20_AVAILABLE 0
#endif

template<typename T>
class CompatibleAtomicCounter {
private:
    std::atomic<T> value_{T{}};

public:
    void add(T val) {
        if constexpr (std::is_floating_point_v<T>) {
            // Use safe compare_exchange loop for floating point
            // Even in C++20, atomic<double>::fetch_add may not be reliable on all platforms
            add_floating_point_safe(val);
        } else {
            // Integer types work fine with fetch_add in both C++17 and C++20
            value_.fetch_add(val, std::memory_order_relaxed);
        }
    }

    T get() const {
        return value_.load(std::memory_order_acquire);
    }

private:
    // Safe floating point atomic add using compare_exchange loop
    void add_floating_point_safe(T val) {
        T current = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(current, current + val,
                                           std::memory_order_relaxed)) {
            // current is updated on failure, retry
        }
    }
};

int main() {
    std::cout << "C++20/C++17 Compatible Atomic Test\n";
    std::cout << "===================================\n";

    #if CPP20_AVAILABLE
        std::cout << "Compiled with C++20 standard\n";
        #if HAS_CPP20_CONCEPTS
            std::cout << "C++20 concepts available\n";
        #endif
    #else
        std::cout << "Compiled with C++17 standard\n";
    #endif

    CompatibleAtomicCounter<int> int_counter;
    CompatibleAtomicCounter<double> double_counter;

    std::vector<std::thread> threads;

    // Test integer counter
    std::cout << "\nTesting integer atomic operations...\n";
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&int_counter, i]() {
            for (int j = 0; j < 1000; ++j) {
                int_counter.add(1);
            }
            std::cout << "Thread " << i << " completed integer counting\n";
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Integer counter result: " << int_counter.get() << " (expected: 4000)\n";

    // Test double counter
    std::cout << "\nTesting floating point atomic operations...\n";
    threads.clear();
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&double_counter, i]() {
            for (int j = 0; j < 1000; ++j) {
                double_counter.add(0.1);
            }
            std::cout << "Thread " << i << " completed double counting\n";
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Double counter result: " << double_counter.get() << " (expected: ~400.0)\n";

    std::cout << "\nCompatible atomic test completed successfully!\n";

    return 0;
}
