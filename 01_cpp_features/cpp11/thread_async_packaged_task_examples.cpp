#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <vector>
#include <string>
#include <mutex>
#include <functional>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <random>
#include <algorithm>
#include <numeric>

/*
 * Comprehensive comparison of std::thread, std::async, and std::packaged_task
 * Use cases, benefits, and practical examples in modern C++
 */

// =============================================================================
// 1. BASIC THREAD USAGE AND EXAMPLES
// =============================================================================

namespace thread_examples {

    void simple_task(int id, const std::string& name) {
        std::cout << "Thread " << id << " (" << name << ") is running on thread ID: "
                  << std::this_thread::get_id() << "\n";

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "Thread " << id << " (" << name << ") completed\n";
    }

    void basic_thread_usage() {
        std::cout << "\n=== BASIC THREAD USAGE ===\n";

        std::cout << "Main thread ID: " << std::this_thread::get_id() << "\n";

        // Create and start threads
        std::thread t1(simple_task, 1, "Worker1");
        std::thread t2(simple_task, 2, "Worker2");
        std::thread t3(simple_task, 3, "Worker3");

        // Wait for threads to complete
        t1.join();
        t2.join();
        t3.join();

        std::cout << "All threads completed\n";
    }

    // Thread with lambda
    void lambda_thread_example() {
        std::cout << "\n=== THREAD WITH LAMBDA ===\n";

        std::mutex mtx;
        int shared_counter = 0;

        auto worker = [&mtx, &shared_counter](int thread_id, int iterations) {
            for (int i = 0; i < iterations; ++i) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    ++shared_counter;
                    std::cout << "Thread " << thread_id << " incremented counter to "
                              << shared_counter << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        };

        std::thread t1(worker, 1, 3);
        std::thread t2(worker, 2, 3);

        t1.join();
        t2.join();

        std::cout << "Final counter value: " << shared_counter << "\n";
    }

    // Thread with member function
    class TaskRunner {
        std::string name_;

    public:
        TaskRunner(const std::string& name) : name_(name) {}

        void run_task(int duration_ms) {
            std::cout << "TaskRunner " << name_ << " starting task\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
            std::cout << "TaskRunner " << name_ << " completed task\n";
        }

        void complex_task(int value) const {
            std::cout << "TaskRunner " << name_ << " processing value: " << value << "\n";
            // Simulate complex work
            std::this_thread::sleep_for(std::chrono::milliseconds(value * 10));
            std::cout << "TaskRunner " << name_ << " finished processing " << value << "\n";
        }
    };

    void member_function_thread() {
        std::cout << "\n=== THREAD WITH MEMBER FUNCTIONS ===\n";

        TaskRunner runner1("Runner1");
        TaskRunner runner2("Runner2");

        // Start threads with member functions
        std::thread t1(&TaskRunner::run_task, &runner1, 200);
        std::thread t2(&TaskRunner::complex_task, &runner2, 15);

        t1.join();
        t2.join();
    }
}

// =============================================================================
// 2. ASYNC USAGE AND EXAMPLES
// =============================================================================

namespace async_examples {

    int compute_factorial(int n) {
        std::cout << "Computing factorial of " << n << " on thread: "
                  << std::this_thread::get_id() << "\n";

        if (n <= 1) return 1;

        int result = 1;
        for (int i = 2; i <= n; ++i) {
            result *= i;
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
        }

        std::cout << "Factorial of " << n << " = " << result << "\n";
        return result;
    }

    void basic_async_usage() {
        std::cout << "\n=== BASIC ASYNC USAGE ===\n";

        std::cout << "Main thread ID: " << std::this_thread::get_id() << "\n";

        // Launch async tasks
        auto future1 = std::async(std::launch::async, compute_factorial, 5);
        auto future2 = std::async(std::launch::async, compute_factorial, 6);
        auto future3 = std::async(std::launch::deferred, compute_factorial, 4); // Deferred execution

        std::cout << "Tasks launched, doing other work...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Get results
        std::cout << "Getting results:\n";
        int result1 = future1.get();
        int result2 = future2.get();
        int result3 = future3.get(); // This will execute now (deferred)

        std::cout << "Results: " << result1 << ", " << result2 << ", " << result3 << "\n";
    }

    // Async with different launch policies
    void launch_policy_comparison() {
        std::cout << "\n=== ASYNC LAUNCH POLICIES ===\n";

        auto start_time = std::chrono::high_resolution_clock::now();

        // std::launch::async - guaranteed to run asynchronously
        auto future_async = std::async(std::launch::async, []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Async result";
        });

        // std::launch::deferred - lazy evaluation (runs when get() is called)
        auto future_deferred = std::async(std::launch::deferred, []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Deferred result";
        });

        // Default behavior (implementation-defined)
        auto future_default = std::async([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Default result";
        });

        std::cout << "Futures created, waiting for results...\n";

        // Get results (deferred will execute here)
        std::cout << future_async.get() << "\n";
        std::cout << future_deferred.get() << "\n";
        std::cout << future_default.get() << "\n";

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Total time: " << duration.count() << "ms\n";
    }

    // Exception handling with async
    int risky_computation(int value) {
        if (value < 0) {
            throw std::invalid_argument("Negative values not allowed");
        }

        if (value > 100) {
            throw std::runtime_error("Value too large");
        }

        return value * value;
    }

    void async_exception_handling() {
        std::cout << "\n=== ASYNC EXCEPTION HANDLING ===\n";

        std::vector<std::future<int>> futures;
        std::vector<int> test_values = {5, -10, 150, 8};

        // Launch async tasks
        for (int value : test_values) {
            futures.push_back(std::async(std::launch::async, risky_computation, value));
        }

        // Collect results and handle exceptions
        for (size_t i = 0; i < futures.size(); ++i) {
            try {
                int result = futures[i].get();
                std::cout << "Result for value " << test_values[i] << ": " << result << "\n";
            } catch (const std::exception& e) {
                std::cout << "Exception for value " << test_values[i] << ": " << e.what() << "\n";
            }
        }
    }

    // Parallel algorithms simulation
    void parallel_processing_example() {
        std::cout << "\n=== PARALLEL PROCESSING WITH ASYNC ===\n";

        std::vector<int> data(1000);
        std::iota(data.begin(), data.end(), 1); // Fill with 1, 2, 3, ..., 1000

        auto process_chunk = [](std::vector<int>::iterator start,
                               std::vector<int>::iterator end) -> long long {
            long long sum = 0;
            for (auto it = start; it != end; ++it) {
                sum += (*it) * (*it); // Square each element and sum
                // Simulate some work
                if (sum % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
            return sum;
        };

        auto start_time = std::chrono::high_resolution_clock::now();

        // Process in parallel chunks
        const size_t chunk_size = data.size() / 4;
        std::vector<std::future<long long>> futures;

        for (size_t i = 0; i < 4; ++i) {
            auto start_it = data.begin() + i * chunk_size;
            auto end_it = (i == 3) ? data.end() : start_it + chunk_size;

            futures.push_back(std::async(std::launch::async, process_chunk, start_it, end_it));
        }

        // Collect results
        long long total_sum = 0;
        for (auto& future : futures) {
            total_sum += future.get();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "Parallel processing completed\n";
        std::cout << "Total sum of squares: " << total_sum << "\n";
        std::cout << "Processing time: " << duration.count() << "ms\n";
    }
}

// =============================================================================
// 3. PACKAGED_TASK USAGE AND EXAMPLES
// =============================================================================

namespace packaged_task_examples {

    // Task queue using packaged_task
    class ThreadPool {
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;

    public:
        ThreadPool(size_t num_threads) : stop(false) {
            for (size_t i = 0; i < num_threads; ++i) {
                workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] { return stop || !tasks.empty(); });

                            if (stop && tasks.empty()) return;

                            task = std::move(tasks.front());
                            tasks.pop();
                        }

                        task();
                    }
                });
            }
        }

        template<typename F, typename... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
            using return_type = typename std::result_of<F(Args...)>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> result = task->get_future();

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                tasks.emplace([task] { (*task)(); });
            }

            condition.notify_one();
            return result;
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }

            condition.notify_all();

            for (std::thread& worker : workers) {
                worker.join();
            }
        }
    };

    int heavy_computation(int n) {
        std::cout << "Heavy computation " << n << " starting on thread: "
                  << std::this_thread::get_id() << "\n";

        // Simulate heavy work
        int result = 0;
        for (int i = 0; i < n * 1000; ++i) {
            result += i % 1000;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "Heavy computation " << n << " completed\n";
        return result;
    }

    void threadpool_with_packaged_task() {
        std::cout << "\n=== THREAD POOL WITH PACKAGED_TASK ===\n";

        ThreadPool pool(3); // 3 worker threads

        std::vector<std::future<int>> results;

        // Enqueue tasks
        for (int i = 1; i <= 6; ++i) {
            results.emplace_back(pool.enqueue(heavy_computation, i));
        }

        // Add some lambda tasks
        results.emplace_back(pool.enqueue([]() -> int {
            std::cout << "Lambda task executing\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return 42;
        }));

        // Collect results
        std::cout << "Collecting results:\n";
        for (size_t i = 0; i < results.size(); ++i) {
            int result = results[i].get();
            std::cout << "Task " << i + 1 << " result: " << result << "\n";
        }
    }

    // Manual packaged_task usage
    void manual_packaged_task_usage() {
        std::cout << "\n=== MANUAL PACKAGED_TASK USAGE ===\n";

        // Create packaged_task
        std::packaged_task<int(int, int)> task([](int a, int b) -> int {
            std::cout << "Packaged task executing: " << a << " + " << b << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return a + b;
        });

        // Get future before moving task
        std::future<int> result = task.get_future();

        // Run task in separate thread
        std::thread worker(std::move(task), 10, 20);

        std::cout << "Task launched, doing other work...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Get result
        std::cout << "Result: " << result.get() << "\n";

        worker.join();
    }

    // Exception handling with packaged_task
    void packaged_task_exception_handling() {
        std::cout << "\n=== PACKAGED_TASK EXCEPTION HANDLING ===\n";

        auto risky_task = [](int value) -> int {
            if (value < 0) {
                throw std::invalid_argument("Negative value not allowed");
            }
            return value * 2;
        };

        std::packaged_task<int(int)> task1(risky_task);
        std::packaged_task<int(int)> task2(risky_task);

        auto future1 = task1.get_future();
        auto future2 = task2.get_future();

        // Run tasks
        std::thread t1(std::move(task1), 5);
        std::thread t2(std::move(task2), -10);

        // Handle results and exceptions
        try {
            std::cout << "Task 1 result: " << future1.get() << "\n";
        } catch (const std::exception& e) {
            std::cout << "Task 1 exception: " << e.what() << "\n";
        }

        try {
            std::cout << "Task 2 result: " << future2.get() << "\n";
        } catch (const std::exception& e) {
            std::cout << "Task 2 exception: " << e.what() << "\n";
        }

        t1.join();
        t2.join();
    }
}

// =============================================================================
// 4. COMPARISON AND USE CASE SCENARIOS
// =============================================================================

namespace comparison_examples {

    // Scenario 1: Simple fire-and-forget tasks
    void fire_and_forget_comparison() {
        std::cout << "\n=== FIRE-AND-FORGET COMPARISON ===\n";

        std::cout << "1. Using std::thread (manual management):\n";
        {
            std::thread t([]() {
                std::cout << "Thread: Fire and forget task\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
            t.join(); // Must join or detach
        }

        std::cout << "2. Using std::async (automatic management):\n";
        {
            auto future = std::async(std::launch::async, []() {
                std::cout << "Async: Fire and forget task\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
            // Future destructor waits for completion
        }

        std::cout << "3. Using std::packaged_task (more complex setup):\n";
        {
            std::packaged_task<void()> task([]() {
                std::cout << "Packaged_task: Fire and forget task\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });

            auto future = task.get_future();
            std::thread t(std::move(task));
            future.get(); // Wait for completion
            t.join();
        }
    }

    // Scenario 2: Getting return values
    void return_value_comparison() {
        std::cout << "\n=== RETURN VALUE COMPARISON ===\n";

        auto computation = [](int x) -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return x * x;
        };

        std::cout << "1. Using std::thread (requires external storage):\n";
        {
            int result = 0;
            std::mutex mtx;

            std::thread t([&result, &mtx, computation]() {
                int local_result = computation(5);
                std::lock_guard<std::mutex> lock(mtx);
                result = local_result;
            });

            t.join();
            std::cout << "Thread result: " << result << "\n";
        }

        std::cout << "2. Using std::async (natural return value):\n";
        {
            auto future = std::async(std::launch::async, computation, 5);
            std::cout << "Async result: " << future.get() << "\n";
        }

        std::cout << "3. Using std::packaged_task (future-based):\n";
        {
            std::packaged_task<int(int)> task(computation);
            auto future = task.get_future();

            std::thread t(std::move(task), 5);
            std::cout << "Packaged_task result: " << future.get() << "\n";
            t.join();
        }
    }

    // Scenario 3: Exception handling
    void exception_handling_comparison() {
        std::cout << "\n=== EXCEPTION HANDLING COMPARISON ===\n";

        auto risky_function = []() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            throw std::runtime_error("Something went wrong");
        };

        std::cout << "1. Using std::thread (manual exception handling):\n";
        {
            std::exception_ptr eptr = nullptr;
            int result = 0;

            std::thread t([&eptr, &result, risky_function]() {
                try {
                    result = risky_function();
                } catch (...) {
                    eptr = std::current_exception();
                }
            });

            t.join();

            if (eptr) {
                try {
                    std::rethrow_exception(eptr);
                } catch (const std::exception& e) {
                    std::cout << "Thread caught exception: " << e.what() << "\n";
                }
            }
        }

        std::cout << "2. Using std::async (automatic exception propagation):\n";
        {
            auto future = std::async(std::launch::async, risky_function);
            try {
                int result = future.get();
                std::cout << "Async result: " << result << "\n";
            } catch (const std::exception& e) {
                std::cout << "Async caught exception: " << e.what() << "\n";
            }
        }

        std::cout << "3. Using std::packaged_task (future-based exception handling):\n";
        {
            std::packaged_task<int()> task(risky_function);
            auto future = task.get_future();

            std::thread t(std::move(task));

            try {
                int result = future.get();
                std::cout << "Packaged_task result: " << result << "\n";
            } catch (const std::exception& e) {
                std::cout << "Packaged_task caught exception: " << e.what() << "\n";
            }

            t.join();
        }
    }

    // Performance comparison
    void performance_comparison() {
        std::cout << "\n=== PERFORMANCE COMPARISON ===\n";

        const int num_tasks = 1000;
        auto simple_task = []() { return 42; };

        // Measure thread creation overhead
        auto start = std::chrono::high_resolution_clock::now();
        {
            std::vector<std::thread> threads;
            std::vector<int> results(num_tasks);

            for (int i = 0; i < num_tasks; ++i) {
                threads.emplace_back([&results, i, simple_task]() {
                    results[i] = simple_task();
                });
            }

            for (auto& t : threads) {
                t.join();
            }
        }
        auto thread_time = std::chrono::high_resolution_clock::now() - start;

        // Measure async overhead
        start = std::chrono::high_resolution_clock::now();
        {
            std::vector<std::future<int>> futures;

            for (int i = 0; i < num_tasks; ++i) {
                futures.push_back(std::async(std::launch::async, simple_task));
            }

            for (auto& f : futures) {
                f.get();
            }
        }
        auto async_time = std::chrono::high_resolution_clock::now() - start;

        std::cout << "Thread creation time: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(thread_time).count()
                  << "ms\n";
        std::cout << "Async creation time: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(async_time).count()
                  << "ms\n";

        std::cout << "Note: For many small tasks, consider using a thread pool\n";
    }
}

// =============================================================================
// 5. BEST PRACTICES AND GUIDELINES
// =============================================================================

namespace best_practices {

    void when_to_use_each() {
        std::cout << "\n=== WHEN TO USE EACH CONCURRENCY TOOL ===\n";

        std::cout << "USE std::thread WHEN:\n";
        std::cout << "- You need fine-grained control over thread lifecycle\n";
        std::cout << "- Implementing custom threading patterns\n";
        std::cout << "- Building thread pools or worker threads\n";
        std::cout << "- Long-running background threads\n";
        std::cout << "- Need to set thread-specific properties (priority, affinity)\n";

        std::cout << "\nUSE std::async WHEN:\n";
        std::cout << "- You want simple parallel execution\n";
        std::cout << "- Need return values from concurrent tasks\n";
        std::cout << "- Want automatic exception propagation\n";
        std::cout << "- Prefer higher-level abstraction\n";
        std::cout << "- Don't need fine control over thread management\n";

        std::cout << "\nUSE std::packaged_task WHEN:\n";
        std::cout << "- Building custom task scheduling systems\n";
        std::cout << "- Need to decouple task creation from execution\n";
        std::cout << "- Implementing thread pools with futures\n";
        std::cout << "- Want to store tasks for later execution\n";
        std::cout << "- Need type-erased callable objects\n";
    }

    void common_pitfalls() {
        std::cout << "\n=== COMMON PITFALLS AND HOW TO AVOID THEM ===\n";

        std::cout << "1. THREAD PITFALLS:\n";
        std::cout << "- Forgetting to join() or detach() threads\n";
        std::cout << "- Data races and shared state modification\n";
        std::cout << "- Exception safety in thread functions\n";
        std::cout << "- Resource management with RAII\n";

        std::cout << "\n2. ASYNC PITFALLS:\n";
        std::cout << "- Not understanding launch policies\n";
        std::cout << "- Assuming async always creates new threads\n";
        std::cout << "- Not calling get() on futures (tasks may not execute)\n";
        std::cout << "- Overuse leading to thread exhaustion\n";

        std::cout << "\n3. PACKAGED_TASK PITFALLS:\n";
        std::cout << "- Moving task after getting future\n";
        std::cout << "- Not handling task execution properly\n";
        std::cout << "- Complexity for simple use cases\n";
        std::cout << "- Lifetime management of task objects\n";
    }

    void performance_guidelines() {
        std::cout << "\n=== PERFORMANCE GUIDELINES ===\n";

        std::cout << "THREAD CREATION OVERHEAD:\n";
        std::cout << "- Thread creation/destruction is expensive\n";
        std::cout << "- Use thread pools for many short-lived tasks\n";
        std::cout << "- Consider std::async for automatic management\n";

        std::cout << "\nCONCURRENCY CONSIDERATIONS:\n";
        std::cout << "- Don't create more threads than CPU cores for CPU-bound tasks\n";
        std::cout << "- Use more threads for I/O-bound tasks\n";
        std::cout << "- Consider work-stealing algorithms\n";

        std::cout << "\nMEMORY CONSIDERATIONS:\n";
        std::cout << "- Each thread has its own stack (default ~1MB)\n";
        std::cout << "- Shared data requires synchronization overhead\n";
        std::cout << "- False sharing can degrade performance\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL EXAMPLES
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE COMPARISON: std::thread vs std::async vs std::packaged_task\n";
    std::cout << "=============================================================================\n";

    // Basic thread examples
    thread_examples::basic_thread_usage();
    thread_examples::lambda_thread_example();
    thread_examples::member_function_thread();

    // Async examples
    async_examples::basic_async_usage();
    async_examples::launch_policy_comparison();
    async_examples::async_exception_handling();
    async_examples::parallel_processing_example();

    // Packaged_task examples
    packaged_task_examples::manual_packaged_task_usage();
    packaged_task_examples::packaged_task_exception_handling();
    packaged_task_examples::threadpool_with_packaged_task();

    // Comparison scenarios
    comparison_examples::fire_and_forget_comparison();
    comparison_examples::return_value_comparison();
    comparison_examples::exception_handling_comparison();
    comparison_examples::performance_comparison();

    // Best practices
    best_practices::when_to_use_each();
    best_practices::common_pitfalls();
    best_practices::performance_guidelines();

    std::cout << "\n=============================================================================\n";
    std::cout << "KEY TAKEAWAYS:\n";
    std::cout << "1. std::thread: Low-level, full control, manual management\n";
    std::cout << "2. std::async: High-level, automatic management, return values\n";
    std::cout << "3. std::packaged_task: Flexible, task scheduling, future-based\n";
    std::cout << "4. Choose based on your specific use case and requirements\n";
    std::cout << "5. Consider thread pools for many short-lived tasks\n";
    std::cout << "6. Always handle exceptions and resource cleanup properly\n";
    std::cout << "7. Profile to understand performance characteristics\n";
    std::cout << "=============================================================================\n";

    return 0;
}
