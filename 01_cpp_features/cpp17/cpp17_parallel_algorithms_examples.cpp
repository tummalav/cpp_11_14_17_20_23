#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <execution>
#include <chrono>
#include <random>
#include <thread>
#include <future>
#include <functional>

/*
 * C++17 Parallel Algorithms and Execution Policies
 *
 * CONCURRENCY vs PARALLELIZATION - Key Differences:
 *
 * CONCURRENCY:
 * - Multiple tasks making progress at the same time (interleaved execution)
 * - Can run on single-core systems through time-slicing
 * - About dealing with multiple things at once (coordination/scheduling)
 * - Focus on task management, synchronization, communication
 * - Examples: Threading, async operations, coroutines
 *
 * PARALLELIZATION:
 * - Multiple tasks actually executing simultaneously on different cores
 * - Requires multi-core systems for true parallelism
 * - About doing multiple things at once (simultaneous execution)
 * - Focus on dividing work across processing units
 * - Examples: SIMD operations, parallel algorithms, GPU computing
 *
 * C++17 introduced parallel execution policies that allow standard algorithms
 * to run in parallel automatically. The execution policies are:
 *
 * 1. std::execution::seq        - Sequential execution (default)
 * 2. std::execution::par        - Parallel execution
 * 3. std::execution::par_unseq  - Parallel and vectorized execution
 * 4. std::execution::unseq      - Vectorized execution (C++20)
 */

namespace parallel_examples {

// Helper function to measure execution time
template<typename Func>
auto measure_time(const std::string& name, Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = func();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << name << " took: " << duration.count() << "ms\n";
    return result;
}

// 1. Basic Parallel Algorithm Examples
void basic_parallel_examples() {
    std::cout << "\n=== Basic Parallel Algorithm Examples ===\n";

    // Create large vector for testing
    const size_t size = 10'000'000;
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), 1);

    // Sequential vs Parallel std::for_each
    std::cout << "\n1. std::for_each comparison:\n";

    auto seq_result = measure_time("Sequential for_each", [&]() {
        std::vector<int> temp = data;
        std::for_each(std::execution::seq, temp.begin(), temp.end(),
                     [](int& x) { x = x * x; });
        return temp.size();
    });

    auto par_result = measure_time("Parallel for_each", [&]() {
        std::vector<int> temp = data;
        std::for_each(std::execution::par, temp.begin(), temp.end(),
                     [](int& x) { x = x * x; });
        return temp.size();
    });

    auto par_unseq_result = measure_time("Parallel+Vectorized for_each", [&]() {
        std::vector<int> temp = data;
        std::for_each(std::execution::par_unseq, temp.begin(), temp.end(),
                     [](int& x) { x = x * x; });
        return temp.size();
    });
}

// 2. Parallel Transform Examples
void parallel_transform_examples() {
    std::cout << "\n=== Parallel Transform Examples ===\n";

    const size_t size = 5'000'000;
    std::vector<double> input(size);
    std::vector<double> output(size);

    // Fill with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(1.0, 100.0);
    std::generate(input.begin(), input.end(), [&]() { return dis(gen); });

    std::cout << "\n2. std::transform - Mathematical operations:\n";

    // Complex mathematical operation
    auto math_operation = [](double x) -> double {
        return std::sqrt(x) + std::log(x) + std::sin(x);
    };

    measure_time("Sequential transform", [&]() {
        std::transform(std::execution::seq, input.begin(), input.end(),
                      output.begin(), math_operation);
        return output[0];
    });

    measure_time("Parallel transform", [&]() {
        std::transform(std::execution::par, input.begin(), input.end(),
                      output.begin(), math_operation);
        return output[0];
    });

    measure_time("Parallel+Vectorized transform", [&]() {
        std::transform(std::execution::par_unseq, input.begin(), input.end(),
                      output.begin(), math_operation);
        return output[0];
    });
}

// 3. Parallel Reduce Examples
void parallel_reduce_examples() {
    std::cout << "\n=== Parallel Reduce Examples ===\n";

    const size_t size = 50'000'000;
    std::vector<int> data(size, 1);

    std::cout << "\n3. std::reduce - Sum calculation:\n";

    auto seq_sum = measure_time("Sequential reduce", [&]() {
        return std::reduce(std::execution::seq, data.begin(), data.end(), 0LL);
    });

    auto par_sum = measure_time("Parallel reduce", [&]() {
        return std::reduce(std::execution::par, data.begin(), data.end(), 0LL);
    });

    auto par_unseq_sum = measure_time("Parallel+Vectorized reduce", [&]() {
        return std::reduce(std::execution::par_unseq, data.begin(), data.end(), 0LL);
    });

    std::cout << "Sequential sum: " << seq_sum << "\n";
    std::cout << "Parallel sum: " << par_sum << "\n";
    std::cout << "Par+Unseq sum: " << par_unseq_sum << "\n";
}

// 4. Parallel Sort Examples
void parallel_sort_examples() {
    std::cout << "\n=== Parallel Sort Examples ===\n";

    const size_t size = 10'000'000;
    std::random_device rd;
    std::mt19937 gen(rd());

    auto create_random_data = [&]() {
        std::vector<int> data(size);
        std::uniform_int_distribution<int> dis(1, 1000000);
        std::generate(data.begin(), data.end(), [&]() { return dis(gen); });
        return data;
    };

    std::cout << "\n4. std::sort comparison:\n";

    auto seq_data = create_random_data();
    measure_time("Sequential sort", [&]() {
        std::sort(std::execution::seq, seq_data.begin(), seq_data.end());
        return seq_data.size();
    });

    auto par_data = create_random_data();
    measure_time("Parallel sort", [&]() {
        std::sort(std::execution::par, par_data.begin(), par_data.end());
        return par_data.size();
    });

    auto par_unseq_data = create_random_data();
    measure_time("Parallel+Vectorized sort", [&]() {
        std::sort(std::execution::par_unseq, par_unseq_data.begin(), par_unseq_data.end());
        return par_unseq_data.size();
    });
}

// 5. Custom Parallel Function Example
template<typename ExecutionPolicy, typename Iterator, typename Predicate>
auto parallel_count_if_custom(ExecutionPolicy&& policy, Iterator first, Iterator last, Predicate pred) {
    // This demonstrates how standard algorithms can accept execution policies
    return std::count_if(std::forward<ExecutionPolicy>(policy), first, last, pred);
}

// 6. Writing Custom Parallel Algorithm
template<typename ExecutionPolicy, typename Iterator, typename Function>
void custom_parallel_apply(ExecutionPolicy&& policy, Iterator first, Iterator last, Function func) {
    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        // Sequential execution
        std::for_each(first, last, func);
    } else if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy> ||
                        std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_unsequenced_policy>) {
        // Parallel execution using std::for_each with policy
        std::for_each(std::forward<ExecutionPolicy>(policy), first, last, func);
    }
}

// 7. Manual Parallel Implementation using std::async
template<typename Iterator, typename Function>
void manual_parallel_apply(Iterator first, Iterator last, Function func, size_t num_threads = std::thread::hardware_concurrency()) {
    const size_t total_size = std::distance(first, last);
    if (total_size == 0) return;

    const size_t chunk_size = total_size / num_threads;
    std::vector<std::future<void>> futures;

    auto current = first;
    for (size_t i = 0; i < num_threads - 1; ++i) {
        auto next = current;
        std::advance(next, chunk_size);

        futures.push_back(std::async(std::launch::async, [current, next, func]() {
            std::for_each(current, next, func);
        }));

        current = next;
    }

    // Handle remaining elements in main thread
    std::for_each(current, last, func);

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
}

// 8. Parallel Algorithm Use Cases
void parallel_algorithm_use_cases() {
    std::cout << "\n=== Parallel Algorithm Use Cases ===\n";

    const size_t size = 1'000'000;
    std::vector<double> prices(size);

    // Fill with sample price data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(100.0, 200.0);
    std::generate(prices.begin(), prices.end(), [&]() { return dis(gen); });

    // Use case 1: Calculate moving averages in parallel
    std::cout << "\n8. Financial calculations with parallel algorithms:\n";

    std::vector<double> returns(size - 1);
    measure_time("Parallel return calculation", [&]() {
        std::transform(std::execution::par, prices.begin(), prices.end() - 1,
                      prices.begin() + 1, returns.begin(),
                      [](double prev, double curr) { return (curr - prev) / prev; });
        return returns.size();
    });

    // Use case 2: Find extreme values
    auto [min_price, max_price] = measure_time("Parallel minmax_element", [&]() {
        return std::minmax_element(std::execution::par, prices.begin(), prices.end());
    });

    std::cout << "Min price: " << *min_price << ", Max price: " << *max_price << "\n";

    // Use case 3: Count elements meeting criteria
    auto high_price_count = measure_time("Parallel count_if", [&]() {
        return std::count_if(std::execution::par, prices.begin(), prices.end(),
                           [](double price) { return price > 150.0; });
    });

    std::cout << "Prices above 150: " << high_price_count << "\n";
}

// 9. Exception Safety with Parallel Algorithms
void parallel_exception_safety() {
    std::cout << "\n=== Exception Safety with Parallel Algorithms ===\n";

    std::vector<int> data = {1, 2, 3, 0, 5, 6, 7, 8, 9, 10}; // Note: contains 0

    try {
        // This might throw due to division by zero
        std::for_each(std::execution::par, data.begin(), data.end(),
                     [](int& x) {
                         if (x == 0) throw std::runtime_error("Division by zero");
                         x = 100 / x;
                     });
    } catch (const std::exception& e) {
        std::cout << "Caught exception in parallel algorithm: " << e.what() << "\n";
        std::cout << "Note: With parallel execution, exception handling behavior may vary\n";
    }
}

// 10. Performance Considerations and Best Practices
void performance_considerations() {
    std::cout << "\n=== Performance Considerations ===\n";

    // Small dataset - parallel overhead might not be worth it
    std::vector<int> small_data(1000);
    std::iota(small_data.begin(), small_data.end(), 1);

    std::cout << "\n10. Small dataset (1000 elements):\n";

    measure_time("Sequential (small)", [&]() {
        return std::reduce(std::execution::seq, small_data.begin(), small_data.end(), 0);
    });

    measure_time("Parallel (small)", [&]() {
        return std::reduce(std::execution::par, small_data.begin(), small_data.end(), 0);
    });

    // Large dataset - parallel should be beneficial
    std::vector<int> large_data(10'000'000);
    std::iota(large_data.begin(), large_data.end(), 1);

    std::cout << "\n10. Large dataset (10M elements):\n";

    measure_time("Sequential (large)", [&]() {
        return std::reduce(std::execution::seq, large_data.begin(), large_data.end(), 0LL);
    });

    measure_time("Parallel (large)", [&]() {
        return std::reduce(std::execution::par, large_data.begin(), large_data.end(), 0LL);
    });

    std::cout << "\nBest Practices:\n";
    std::cout << "1. Use parallel algorithms for computationally expensive operations\n";
    std::cout << "2. Ensure sufficient data size to overcome parallelization overhead\n";
    std::cout << "3. Avoid shared state and race conditions\n";
    std::cout << "4. Be careful with exception safety in parallel contexts\n";
    std::cout << "5. par_unseq requires vectorization-safe operations\n";
    std::cout << "6. Consider memory access patterns for cache efficiency\n";
}

} // namespace parallel_examples

// Custom Parallel Algorithm Implementation Examples
namespace custom_parallel_algorithms {

// 1. Parallel Map-Reduce Implementation
template<typename Iterator, typename MapFunc, typename ReduceFunc, typename T>
T parallel_map_reduce(Iterator first, Iterator last, MapFunc map_func,
                     ReduceFunc reduce_func, T init_value,
                     size_t num_threads = std::thread::hardware_concurrency()) {

    const size_t total_size = std::distance(first, last);
    if (total_size == 0) return init_value;

    const size_t chunk_size = total_size / num_threads;
    std::vector<std::future<T>> futures;

    auto current = first;
    for (size_t i = 0; i < num_threads - 1; ++i) {
        auto next = current;
        std::advance(next, chunk_size);

        futures.push_back(std::async(std::launch::async, [current, next, map_func, reduce_func, init_value]() {
            T local_result = init_value;
            for (auto it = current; it != next; ++it) {
                local_result = reduce_func(local_result, map_func(*it));
            }
            return local_result;
        }));

        current = next;
    }

    // Handle remaining elements
    T result = init_value;
    for (auto it = current; it != last; ++it) {
        result = reduce_func(result, map_func(*it));
    }

    // Combine results from all threads
    for (auto& future : futures) {
        result = reduce_func(result, future.get());
    }

    return result;
}

// 2. Parallel Filter Implementation
template<typename Iterator, typename Predicate>
auto parallel_filter(Iterator first, Iterator last, Predicate pred,
                    size_t num_threads = std::thread::hardware_concurrency()) {
    using ValueType = typename std::iterator_traits<Iterator>::value_type;

    const size_t total_size = std::distance(first, last);
    if (total_size == 0) return std::vector<ValueType>{};

    const size_t chunk_size = total_size / num_threads;
    std::vector<std::future<std::vector<ValueType>>> futures;

    auto current = first;
    for (size_t i = 0; i < num_threads - 1; ++i) {
        auto next = current;
        std::advance(next, chunk_size);

        futures.push_back(std::async(std::launch::async, [current, next, pred]() {
            std::vector<ValueType> local_result;
            for (auto it = current; it != next; ++it) {
                if (pred(*it)) {
                    local_result.push_back(*it);
                }
            }
            return local_result;
        }));

        current = next;
    }

    // Handle remaining elements
    std::vector<ValueType> result;
    for (auto it = current; it != last; ++it) {
        if (pred(*it)) {
            result.push_back(*it);
        }
    }

    // Combine results from all threads
    for (auto& future : futures) {
        auto local_result = future.get();
        result.insert(result.end(), local_result.begin(), local_result.end());
    }

    return result;
}

// 3. Example usage of custom parallel algorithms
void test_custom_algorithms() {
    std::cout << "\n=== Custom Parallel Algorithms ===\n";

    const size_t size = 1'000'000;
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), 1);

    // Test parallel map-reduce
    auto sum_of_squares = parallel_examples::measure_time("Custom parallel map-reduce", [&]() {
        return parallel_map_reduce(data.begin(), data.end(),
                                 [](int x) { return x * x; },        // Map: square
                                 [](long long a, int b) { return a + b; }, // Reduce: sum
                                 0LL);
    });
    std::cout << "Sum of squares: " << sum_of_squares << "\n";

    // Test parallel filter
    auto even_numbers = parallel_examples::measure_time("Custom parallel filter", [&]() {
        return parallel_filter(data.begin(), data.end(),
                             [](int x) { return x % 2 == 0; });
    });
    std::cout << "Even numbers found: " << even_numbers.size() << "\n";
}

} // namespace custom_parallel_algorithms

int main() {
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "               C++17 PARALLEL ALGORITHMS EXAMPLES\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "\nNote: For concurrency vs parallelization concepts and examples,\n";
    std::cout << "see the separate file: concurrency_vs_parallelization_examples.cpp\n";
    std::cout << std::string(80, '=') << "\n";

    // Run all parallel algorithm examples
    parallel_examples::basic_parallel_examples();
    parallel_examples::parallel_transform_examples();
    parallel_examples::parallel_reduce_examples();
    parallel_examples::parallel_sort_examples();
    parallel_examples::parallel_algorithm_use_cases();
    parallel_examples::parallel_exception_safety();
    parallel_examples::performance_considerations();

    custom_parallel_algorithms::test_custom_algorithms();

    std::cout << "\n=== Summary ===\n";
    std::cout << "C++17 Execution Policies:\n";
    std::cout << "- std::execution::seq: Sequential execution\n";
    std::cout << "- std::execution::par: Parallel execution\n";
    std::cout << "- std::execution::par_unseq: Parallel + vectorized execution\n";
    std::cout << "\nKey Points:\n";
    std::cout << "1. Parallel algorithms can provide significant speedup for large datasets\n";
    std::cout << "2. Overhead exists, so small datasets might not benefit\n";
    std::cout << "3. par_unseq requires vectorization-safe operations (no locks, atomics, etc.)\n";
    std::cout << "4. Exception handling in parallel contexts can be tricky\n";
    std::cout << "5. Custom parallel algorithms can be built using std::async or thread pools\n";

    return 0;
}

/*
 * Compilation Notes:
 *
 * For GCC/Clang:
 * g++ -std=c++17 -O3 -ltbb cpp17_parallel_algorithms_examples.cpp -o parallel_example
 *
 * Note: You might need to install Intel TBB library for parallel execution:
 * - Ubuntu/Debian: sudo apt-get install libtbb-dev
 * - macOS: brew install tbb
 * - Windows: vcpkg install tbb
 *
 * Alternative compilation without TBB (sequential execution only):
 * g++ -std=c++17 -O3 cpp17_parallel_algorithms_examples.cpp -o parallel_example
 */
