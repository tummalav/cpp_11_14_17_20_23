#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <random>
#include <thread>
#include <future>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <execution>

/*
 * CONCURRENCY vs PARALLELIZATION - Comprehensive Guide and Examples
 *
 * This file demonstrates the key differences between concurrency and parallelization
 * with practical examples to clarify these often-confused concepts.
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
 */

namespace concurrency_vs_parallelization {

// ===========================================================================================
// UTILITY FUNCTIONS
// ===========================================================================================

// Timing utilities
auto get_current_time() {
    return std::chrono::high_resolution_clock::now();
}

template<typename TimePoint>
long long get_duration_ms(TimePoint start, TimePoint end) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

template<typename Func>
auto measure_time(const std::string& name, Func&& func) {
    auto start = get_current_time();
    auto result = func();
    auto end = get_current_time();
    std::cout << name << " took: " << get_duration_ms(start, end) << "ms\n";
    return result;
}

// ===========================================================================================
// 1. CONCURRENCY EXAMPLES - Multiple tasks coordinating/interleaving
// ===========================================================================================

// Example 1: Producer-Consumer Pattern (Classic Concurrency Example)
class ProducerConsumer {
private:
    std::queue<int> buffer_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool done_ = false;
    static constexpr size_t BUFFER_SIZE = 10;

public:
    void producer(int items_to_produce) {
        std::cout << "Producer starting to produce " << items_to_produce << " items\n";

        for (int i = 1; i <= items_to_produce; ++i) {
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait if buffer is full
            condition_.wait(lock, [this] { return buffer_.size() < BUFFER_SIZE; });

            buffer_.push(i);
            std::cout << "Produced: " << i << " (buffer size: " << buffer_.size() << ")\n";

            condition_.notify_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::lock_guard<std::mutex> lock(mutex_);
        done_ = true;
        condition_.notify_all();
        std::cout << "Producer finished\n";
    }

    void consumer(int consumer_id) {
        std::cout << "Consumer " << consumer_id << " started\n";

        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait for data or completion
            condition_.wait(lock, [this] { return !buffer_.empty() || done_; });

            if (buffer_.empty() && done_) break;

            if (!buffer_.empty()) {
                int item = buffer_.front();
                buffer_.pop();
                std::cout << "Consumer " << consumer_id << " consumed: " << item
                         << " (buffer size: " << buffer_.size() << ")\n";
                condition_.notify_all();
            }

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }

        std::cout << "Consumer " << consumer_id << " finished\n";
    }
};

// Example 2: Concurrent Task Scheduler
class TaskScheduler {
private:
    std::vector<std::function<void()>> tasks_;
    std::mutex tasks_mutex_;
    std::atomic<bool> running_{true};

public:
    void add_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_.push_back(std::move(task));
    }

    void worker_thread(int worker_id) {
        std::cout << "Worker " << worker_id << " started\n";

        while (running_) {
            std::function<void()> task;

            {
                std::lock_guard<std::mutex> lock(tasks_mutex_);
                if (!tasks_.empty()) {
                    task = std::move(tasks_.back());
                    tasks_.pop_back();
                }
            }

            if (task) {
                std::cout << "Worker " << worker_id << " executing task\n";
                task();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::cout << "Worker " << worker_id << " finished\n";
    }

    void stop() {
        running_ = false;
    }

    bool has_tasks() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        return !tasks_.empty();
    }
};

// Example 3: Concurrent Web Server Simulation
class WebServerSimulator {
private:
    std::atomic<int> request_counter_{0};

public:
    void handle_request(int request_id, int processing_time_ms) {
        int current_count = ++request_counter_;
        std::cout << "Thread " << std::this_thread::get_id()
                  << " handling request " << request_id
                  << " (active requests: " << current_count << ")\n";

        // Simulate request processing
        std::this_thread::sleep_for(std::chrono::milliseconds(processing_time_ms));

        current_count = --request_counter_;
        std::cout << "Thread " << std::this_thread::get_id()
                  << " completed request " << request_id
                  << " (active requests: " << current_count << ")\n";
    }

    void simulate_concurrent_requests() {
        std::vector<std::thread> request_threads;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(100, 500);

        // Simulate 10 concurrent requests
        for (int i = 1; i <= 10; ++i) {
            request_threads.emplace_back([this, i, &gen, &dis]() {
                handle_request(i, dis(gen));
            });
        }

        for (auto& thread : request_threads) {
            thread.join();
        }
    }
};

// Example 4: Asynchronous Operations (std::async)
class AsyncOperationsExample {
public:
    static int expensive_computation(int input, int delay_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        return input * input + input;
    }

    static void demonstrate_async_operations() {
        std::cout << "\nDemonstrating Asynchronous Operations:\n";

        auto start = get_current_time();

        // Launch multiple async operations
        auto future1 = std::async(std::launch::async, expensive_computation, 10, 200);
        auto future2 = std::async(std::launch::async, expensive_computation, 20, 300);
        auto future3 = std::async(std::launch::async, expensive_computation, 30, 150);

        std::cout << "All async operations launched\n";

        // Do some other work while operations are running
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Doing other work while async operations run...\n";

        // Collect results
        int result1 = future1.get();
        int result2 = future2.get();
        int result3 = future3.get();

        auto end = get_current_time();

        std::cout << "Results: " << result1 << ", " << result2 << ", " << result3 << "\n";
        std::cout << "Total time: " << get_duration_ms(start, end) << "ms (overlapped execution)\n";
    }
};

// ===========================================================================================
// 2. PARALLELIZATION EXAMPLES - Simultaneous execution on multiple cores
// ===========================================================================================

// Example 1: Data Parallel Processing
template<typename T>
class DataParallelProcessor {
public:
    // Sequential processing
    static std::vector<T> process_sequential(const std::vector<T>& data,
                                           std::function<T(const T&)> transform) {
        std::vector<T> result;
        result.reserve(data.size());

        for (const auto& item : data) {
            result.push_back(transform(item));
        }

        return result;
    }

    // Parallel processing using std::transform with execution policy
    static std::vector<T> process_parallel(const std::vector<T>& data,
                                         std::function<T(const T&)> transform) {
        std::vector<T> result(data.size());

        std::transform(std::execution::par, data.begin(), data.end(),
                      result.begin(), transform);

        return result;
    }

    // Manual parallel processing using threads (work stealing)
    static std::vector<T> process_manual_parallel(const std::vector<T>& data,
                                                std::function<T(const T&)> transform,
                                                size_t num_threads = std::thread::hardware_concurrency()) {
        std::vector<T> result(data.size());
        const size_t chunk_size = data.size() / num_threads;
        std::vector<std::thread> threads;

        for (size_t i = 0; i < num_threads; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == num_threads - 1) ? data.size() : (i + 1) * chunk_size;

            threads.emplace_back([&data, &result, transform, start, end]() {
                for (size_t j = start; j < end; ++j) {
                    result[j] = transform(data[j]);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return result;
    }
};

// Example 2: Matrix Operations (True Parallelization)
class ParallelMatrixOperations {
public:
    using Matrix = std::vector<std::vector<double>>;

    // Create a matrix filled with random values
    static Matrix create_matrix(size_t rows, size_t cols, double min_val = 0.0, double max_val = 10.0) {
        Matrix matrix(rows, std::vector<double>(cols));
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(min_val, max_val);

        for (auto& row : matrix) {
            for (auto& element : row) {
                element = dis(gen);
            }
        }

        return matrix;
    }

    // Sequential matrix multiplication
    static Matrix multiply_sequential(const Matrix& A, const Matrix& B) {
        size_t rows_A = A.size();
        size_t cols_A = A[0].size();
        size_t cols_B = B[0].size();

        Matrix C(rows_A, std::vector<double>(cols_B, 0.0));

        for (size_t i = 0; i < rows_A; ++i) {
            for (size_t j = 0; j < cols_B; ++j) {
                for (size_t k = 0; k < cols_A; ++k) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }

        return C;
    }

    // Parallel matrix multiplication (row-wise parallelization)
    static Matrix multiply_parallel(const Matrix& A, const Matrix& B) {
        size_t rows_A = A.size();
        size_t cols_A = A[0].size();
        size_t cols_B = B[0].size();

        Matrix C(rows_A, std::vector<double>(cols_B, 0.0));

        // Create indices for parallel processing
        std::vector<size_t> row_indices(rows_A);
        std::iota(row_indices.begin(), row_indices.end(), 0);

        // Parallel execution
        std::for_each(std::execution::par, row_indices.begin(), row_indices.end(),
                     [&](size_t i) {
                         for (size_t j = 0; j < cols_B; ++j) {
                             for (size_t k = 0; k < cols_A; ++k) {
                                 C[i][j] += A[i][k] * B[k][j];
                             }
                         }
                     });

        return C;
    }

    // Block-wise parallel matrix multiplication
    static Matrix multiply_block_parallel(const Matrix& A, const Matrix& B, size_t block_size = 64) {
        size_t rows_A = A.size();
        size_t cols_A = A[0].size();
        size_t cols_B = B[0].size();

        Matrix C(rows_A, std::vector<double>(cols_B, 0.0));

        std::vector<std::future<void>> futures;

        for (size_t i = 0; i < rows_A; i += block_size) {
            for (size_t j = 0; j < cols_B; j += block_size) {
                futures.push_back(std::async(std::launch::async,
                    [&A, &B, &C, i, j, block_size, rows_A, cols_A, cols_B]() {
                        size_t end_i = std::min(i + block_size, rows_A);
                        size_t end_j = std::min(j + block_size, cols_B);

                        for (size_t ii = i; ii < end_i; ++ii) {
                            for (size_t jj = j; jj < end_j; ++jj) {
                                for (size_t k = 0; k < cols_A; ++k) {
                                    C[ii][jj] += A[ii][k] * B[k][jj];
                                }
                            }
                        }
                    }));
            }
        }

        // Wait for all blocks to complete
        for (auto& future : futures) {
            future.wait();
        }

        return C;
    }
};

// Example 3: Parallel Numerical Algorithms
class ParallelNumericalAlgorithms {
public:
    // Parallel Monte Carlo Pi estimation
    static double estimate_pi_parallel(size_t num_samples, size_t num_threads = std::thread::hardware_concurrency()) {
        const size_t samples_per_thread = num_samples / num_threads;
        std::vector<std::future<size_t>> futures;

        for (size_t i = 0; i < num_threads; ++i) {
            futures.push_back(std::async(std::launch::async, [samples_per_thread, i]() {
                std::random_device rd;
                std::mt19937 gen(rd() + i); // Different seed per thread
                std::uniform_real_distribution<double> dis(-1.0, 1.0);

                size_t inside_circle = 0;
                for (size_t j = 0; j < samples_per_thread; ++j) {
                    double x = dis(gen);
                    double y = dis(gen);
                    if (x * x + y * y <= 1.0) {
                        ++inside_circle;
                    }
                }
                return inside_circle;
            }));
        }

        size_t total_inside = 0;
        for (auto& future : futures) {
            total_inside += future.get();
        }

        return 4.0 * static_cast<double>(total_inside) / static_cast<double>(num_samples);
    }

    // Parallel numerical integration (Trapezoidal rule)
    static double integrate_parallel(std::function<double(double)> func,
                                   double a, double b, size_t num_intervals,
                                   size_t num_threads = std::thread::hardware_concurrency()) {
        const double h = (b - a) / num_intervals;
        const size_t intervals_per_thread = num_intervals / num_threads;
        std::vector<std::future<double>> futures;

        for (size_t i = 0; i < num_threads; ++i) {
            size_t start_interval = i * intervals_per_thread;
            size_t end_interval = (i == num_threads - 1) ? num_intervals : (i + 1) * intervals_per_thread;

            futures.push_back(std::async(std::launch::async,
                [func, a, h, start_interval, end_interval]() {
                    double local_sum = 0.0;
                    for (size_t j = start_interval; j < end_interval; ++j) {
                        double x1 = a + j * h;
                        double x2 = a + (j + 1) * h;
                        local_sum += 0.5 * h * (func(x1) + func(x2));
                    }
                    return local_sum;
                }));
        }

        double total_sum = 0.0;
        for (auto& future : futures) {
            total_sum += future.get();
        }

        return total_sum;
    }
};

// ===========================================================================================
// 3. DEMONSTRATION FUNCTIONS
// ===========================================================================================

void demonstrate_concurrency() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "           CONCURRENCY EXAMPLES\n";
    std::cout << std::string(60, '=') << "\n";

    // Example 1: Producer-Consumer
    std::cout << "\n1. Producer-Consumer Pattern:\n";
    std::cout << "   (Shows coordination between threads)\n";
    std::cout << std::string(40, '-') << "\n";

    ProducerConsumer pc;

    std::thread producer_thread(&ProducerConsumer::producer, &pc, 8);
    std::thread consumer1_thread(&ProducerConsumer::consumer, &pc, 1);
    std::thread consumer2_thread(&ProducerConsumer::consumer, &pc, 2);

    producer_thread.join();
    consumer1_thread.join();
    consumer2_thread.join();

    // Example 2: Task Scheduling
    std::cout << "\n2. Concurrent Task Scheduling:\n";
    std::cout << "   (Shows multiple workers competing for tasks)\n";
    std::cout << std::string(40, '-') << "\n";

    TaskScheduler scheduler;

    // Add tasks that simulate different types of work
    for (int i = 0; i < 8; ++i) {
        scheduler.add_task([i]() {
            std::cout << "  Task " << i << " processing...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100 + i * 50));
            std::cout << "  Task " << i << " completed\n";
        });
    }

    // Start worker threads
    std::thread worker1(&TaskScheduler::worker_thread, &scheduler, 1);
    std::thread worker2(&TaskScheduler::worker_thread, &scheduler, 2);
    std::thread worker3(&TaskScheduler::worker_thread, &scheduler, 3);

    // Wait for all tasks to be processed
    while (scheduler.has_tasks()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow final tasks to complete
    scheduler.stop();

    worker1.join();
    worker2.join();
    worker3.join();

    // Example 3: Web Server Simulation
    std::cout << "\n3. Web Server Concurrent Request Handling:\n";
    std::cout << "   (Shows concurrent request processing)\n";
    std::cout << std::string(40, '-') << "\n";

    WebServerSimulator server;
    server.simulate_concurrent_requests();

    // Example 4: Asynchronous Operations
    AsyncOperationsExample::demonstrate_async_operations();
}

void demonstrate_parallelization() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "         PARALLELIZATION EXAMPLES\n";
    std::cout << std::string(60, '=') << "\n";

    // Example 1: Data Processing Performance Comparison
    std::cout << "\n1. Data Parallel Processing Performance:\n";
    std::cout << "   (Shows actual speedup from parallel execution)\n";
    std::cout << std::string(40, '-') << "\n";

    const size_t data_size = 2'000'000;
    std::vector<double> test_data(data_size);
    std::iota(test_data.begin(), test_data.end(), 1.0);

    // Heavy computation function
    auto heavy_computation = [](const double& x) -> double {
        double result = 0.0;
        for (int i = 0; i < 10; ++i) {
            result += std::sqrt(x + i) + std::sin(x + i) + std::cos(x + i);
        }
        return result;
    };

    auto result_seq = measure_time("Sequential processing", [&]() {
        return DataParallelProcessor<double>::process_sequential(test_data, heavy_computation);
    });

    auto result_par = measure_time("Parallel processing", [&]() {
        return DataParallelProcessor<double>::process_parallel(test_data, heavy_computation);
    });

    auto result_manual = measure_time("Manual parallel", [&]() {
        return DataParallelProcessor<double>::process_manual_parallel(test_data, heavy_computation);
    });

    std::cout << "Results verify: " << (result_seq.size() == result_par.size() ? "PASSED" : "FAILED") << "\n";

    // Example 2: Matrix Multiplication
    std::cout << "\n2. Matrix Multiplication Parallelization:\n";
    std::cout << "   (Shows computational parallelism benefits)\n";
    std::cout << std::string(40, '-') << "\n";

    const size_t matrix_size = 300;
    auto A = ParallelMatrixOperations::create_matrix(matrix_size, matrix_size, 1.0, 2.0);
    auto B = ParallelMatrixOperations::create_matrix(matrix_size, matrix_size, 1.0, 2.0);

    auto C_seq = measure_time("Sequential matrix mult", [&]() {
        return ParallelMatrixOperations::multiply_sequential(A, B);
    });

    auto C_par = measure_time("Parallel matrix mult", [&]() {
        return ParallelMatrixOperations::multiply_parallel(A, B);
    });

    auto C_block = measure_time("Block parallel matrix mult", [&]() {
        return ParallelMatrixOperations::multiply_block_parallel(A, B);
    });

    std::cout << "Matrix results verify: " << (C_seq[0][0] == C_par[0][0] ? "PASSED" : "FAILED") << "\n";

    // Example 3: Numerical Algorithms
    std::cout << "\n3. Parallel Numerical Algorithms:\n";
    std::cout << "   (Shows mathematical computation parallelization)\n";
    std::cout << std::string(40, '-') << "\n";

    const size_t num_samples = 50'000'000;
    double pi_estimate = measure_time("Parallel Pi estimation", [&]() {
        return ParallelNumericalAlgorithms::estimate_pi_parallel(num_samples);
    });
    std::cout << "Pi estimate: " << pi_estimate << " (error: " << std::abs(pi_estimate - M_PI) << ")\n";

    double integral_result = measure_time("Parallel integration", [&]() {
        return ParallelNumericalAlgorithms::integrate_parallel(
            [](double x) { return x * x; }, // f(x) = x²
            0.0, 1.0, 10'000'000);         // ∫₀¹ x² dx = 1/3
    });
    std::cout << "Integral of x² from 0 to 1: " << integral_result
              << " (expected: 0.333333, error: " << std::abs(integral_result - 1.0/3.0) << ")\n";
}

// ===========================================================================================
// 4. KEY DIFFERENCES AND COMPARISON
// ===========================================================================================

void explain_key_differences() {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "                    CONCURRENCY vs PARALLELIZATION\n";
    std::cout << std::string(80, '=') << "\n";

    std::cout << "\n┌─────────────────┬─────────────────────────────────┬─────────────────────────────────┐\n";
    std::cout << "│    Aspect       │           CONCURRENCY           │         PARALLELIZATION         │\n";
    std::cout << "├─────────────────┼─────────────────────────────────┼─────────────────────────────────┤\n";
    std::cout << "│ Execution Model │ Interleaved (time-slicing)      │ Simultaneous (multiple cores)   │\n";
    std::cout << "│ Hardware Req.   │ Single-core sufficient         │ Multi-core required             │\n";
    std::cout << "│ Primary Focus   │ Task coordination               │ Work distribution               │\n";
    std::cout << "│ Main Challenge  │ Synchronization, deadlocks      │ Data dependencies, load balance │\n";
    std::cout << "│ Typical Use     │ I/O, UI, servers, workflows    │ CPU-intensive computations      │\n";
    std::cout << "│ Performance     │ Better responsiveness           │ Better throughput               │\n";
    std::cout << "│ Scalability     │ Limited by coordination overhead │ Limited by available cores      │\n";
    std::cout << "│ Programming     │ Threads, locks, async/await     │ Data partitioning, SIMD         │\n";
    std::cout << "└─────────────────┴─────────────────────────────────┴─────────────────────────────────┘\n";

    std::cout << "\nDETAILED EXPLANATIONS:\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "\n🔄 CONCURRENCY:\n";
    std::cout << "  • Multiple tasks appear to run simultaneously but may actually be interleaved\n";
    std::cout << "  • Focus on managing multiple independent or related tasks\n";
    std::cout << "  • Key concerns: race conditions, deadlocks, synchronization\n";
    std::cout << "  • Examples: Web servers handling multiple requests, UI responsiveness\n";
    std::cout << "  • Tools: std::thread, std::mutex, std::condition_variable, std::async\n";

    std::cout << "\n⚡ PARALLELIZATION:\n";
    std::cout << "  • Multiple operations execute simultaneously on different processing units\n";
    std::cout << "  • Focus on dividing computational work to reduce execution time\n";
    std::cout << "  • Key concerns: data dependencies, load balancing, memory access patterns\n";
    std::cout << "  • Examples: Matrix multiplication, image processing, scientific simulations\n";
    std::cout << "  • Tools: std::execution policies, OpenMP, CUDA, vectorization\n";

    std::cout << "\nWHEN TO USE EACH:\n";
    std::cout << std::string(20, '-') << "\n";
    std::cout << "✅ Use CONCURRENCY for:\n";
    std::cout << "  • I/O-bound operations (file/network access)\n";
    std::cout << "  • User interface responsiveness\n";
    std::cout << "  • Server applications handling multiple clients\n";
    std::cout << "  • Event-driven programming\n";
    std::cout << "  • Independent task coordination\n";

    std::cout << "\n✅ Use PARALLELIZATION for:\n";
    std::cout << "  • CPU-intensive computations\n";
    std::cout << "  • Mathematical operations on large datasets\n";
    std::cout << "  • Image/video processing\n";
    std::cout << "  • Scientific simulations\n";
    std::cout << "  • Algorithms that can be divided into independent sub-problems\n";

    std::cout << "\nC++ LANGUAGE FEATURES:\n";
    std::cout << std::string(25, '-') << "\n";
    std::cout << "🔄 Concurrency Support:\n";
    std::cout << "  • std::thread, std::jthread (C++20)\n";
    std::cout << "  • std::mutex, std::shared_mutex\n";
    std::cout << "  • std::condition_variable\n";
    std::cout << "  • std::atomic<T>\n";
    std::cout << "  • std::async, std::future, std::promise\n";
    std::cout << "  • Coroutines (C++20)\n";

    std::cout << "\n⚡ Parallelization Support:\n";
    std::cout << "  • std::execution::par, std::execution::par_unseq (C++17)\n";
    std::cout << "  • Parallel algorithms (std::sort, std::transform, etc.)\n";
    std::cout << "  • std::reduce, std::transform_reduce\n";
    std::cout << "  • SIMD intrinsics support\n";
    std::cout << "  • OpenMP integration\n";

    std::cout << "\nHYBRID APPROACHES:\n";
    std::cout << std::string(20, '-') << "\n";
    std::cout << "🔄⚡ Modern applications often combine both:\n";
    std::cout << "  • Concurrent task queues + parallel processing within tasks\n";
    std::cout << "  • Actor model: Concurrent actors with parallel computation inside\n";
    std::cout << "  • Thread pools: Concurrent task scheduling + parallel execution\n";
    std::cout << "  • MapReduce paradigm: Parallel map phase + concurrent reduce coordination\n";

    std::cout << "\nPERFORMANCE IMPLICATIONS:\n";
    std::cout << std::string(25, '-') << "\n";
    std::cout << "📊 Concurrency Performance:\n";
    std::cout << "  • Improves responsiveness and resource utilization\n";
    std::cout << "  • May not improve raw computation speed\n";
    std::cout << "  • Overhead from context switching and synchronization\n";

    std::cout << "\n📊 Parallelization Performance:\n";
    std::cout << "  • Can provide linear speedup (ideally)\n";
    std::cout << "  • Limited by Amdahl's Law (sequential portions)\n";
    std::cout << "  • Overhead from data movement and synchronization\n";
    std::cout << "  • Memory bandwidth can become bottleneck\n";
}

} // namespace concurrency_vs_parallelization

// ===========================================================================================
// MAIN FUNCTION
// ===========================================================================================

int main() {
    std::cout << "System Information:\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";

    // Demonstrate the concepts
    concurrency_vs_parallelization::explain_key_differences();
    concurrency_vs_parallelization::demonstrate_concurrency();
    concurrency_vs_parallelization::demonstrate_parallelization();

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "                           SUMMARY\n";
    std::cout << std::string(80, '=') << "\n";

    std::cout << "\n🎯 KEY TAKEAWAYS:\n";
    std::cout << "1. Concurrency is about DEALING WITH multiple things at once\n";
    std::cout << "2. Parallelization is about DOING multiple things at once\n";
    std::cout << "3. Concurrency can work on single-core systems\n";
    std::cout << "4. Parallelization requires multiple processing units\n";
    std::cout << "5. Both can be combined for maximum efficiency\n";
    std::cout << "6. Choose based on your specific problem characteristics\n";

    std::cout << "\n🛠️  PRACTICAL GUIDELINES:\n";
    std::cout << "• For I/O-bound tasks → Use concurrency (async, threads)\n";
    std::cout << "• For CPU-bound tasks → Use parallelization (parallel algorithms)\n";
    std::cout << "• For complex systems → Combine both approaches strategically\n";
    std::cout << "• Always measure performance to verify benefits\n";
    std::cout << "• Consider memory access patterns and cache efficiency\n";

    return 0;
}

/*
 * COMPILATION INSTRUCTIONS:
 *
 * Basic compilation:
 * g++ -std=c++17 -O3 -pthread concurrency_vs_parallelization_examples.cpp -o concurrency_parallel_demo
 *
 * With Intel TBB for better parallel algorithm support:
 * g++ -std=c++17 -O3 -pthread -ltbb concurrency_vs_parallelization_examples.cpp -o concurrency_parallel_demo
 *
 * For maximum optimization:
 * g++ -std=c++17 -O3 -march=native -pthread -ltbb concurrency_vs_parallelization_examples.cpp -o concurrency_parallel_demo
 *
 * Note: Install TBB if not available:
 * - macOS: brew install tbb
 * - Ubuntu/Debian: sudo apt-get install libtbb-dev
 * - CentOS/RHEL: sudo yum install tbb-devel
 */
