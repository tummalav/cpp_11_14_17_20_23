7 /*
 * Simple C++20 Atomic Test
 * Testing basic C++20 atomic features
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <concepts>

template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
class SimpleAtomicCounter {
private:
    std::atomic<T> value_{T{}};

public:
    void add(T val) {
        if constexpr (std::is_floating_point_v<T>) {
            // C++20: fetch_add for floating point
            value_.fetch_add(val, std::memory_order_relaxed);
        } else {
            value_.fetch_add(val, std::memory_order_relaxed);
        }
    }

    T get() const {
        return value_.load(std::memory_order_acquire);
    }
};

int main() {
    std::cout << "C++20 Simple Atomic Test\n";
    std::cout << "========================\n";

    SimpleAtomicCounter<int> int_counter;
    SimpleAtomicCounter<double> double_counter;

    std::vector<std::thread> threads;

    // Test integer counter
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

    std::cout << "Integer counter result: " << int_counter.get() << "\n";

    // Test double counter
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

    std::cout << "Double counter result: " << double_counter.get() << "\n";

    std::cout << "C++20 atomic test completed successfully!\n";

    return 0;
}
