/*
 * C++20 Coroutines Use Cases and Examples
 *
 * Coroutines provide a way to write asynchronous code that looks like
 * synchronous code, making it easier to read and maintain.
 *
 * Key Benefits:
 * 1. Asynchronous programming without callback hell
 * 2. Cooperative multitasking
 * 3. Lazy evaluation and generators
 * 4. Simplified async/await patterns
 * 5. Memory efficient (stackless coroutines)
 * 6. Better error handling in async code
 */

#include <iostream>
#include <coroutine>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <optional>
#include <exception>
#include <queue>
#include <random>
#include <fstream>
#include <sstream>

// ============================================================================
// 1. BASIC GENERATOR COROUTINE
// ============================================================================

template<typename T>
struct Generator {
    struct promise_type {
        T current_value;

        Generator get_return_object() {
            return Generator{Handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }

        void return_void() {}
        void unhandled_exception() {}
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle h_;

    explicit Generator(Handle h) : h_(h) {}
    ~Generator() { if (h_) h_.destroy(); }

    // Move only
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& other) noexcept : h_(other.h_) { other.h_ = {}; }
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = other.h_;
            other.h_ = {};
        }
        return *this;
    }

    // Iterator interface
    struct iterator {
        Handle h_;

        iterator(Handle h) : h_(h) {}

        iterator& operator++() {
            h_.resume();
            return *this;
        }

        T operator*() const {
            return h_.promise().current_value;
        }

        bool operator==(const iterator& other) const {
            return h_.done() == other.h_.done();
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    iterator begin() {
        if (h_) h_.resume();
        return iterator{h_};
    }

    iterator end() {
        return iterator{Handle{}};
    }
};

// Simple number generator
Generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        auto temp = a;
        a = b;
        b += temp;
    }
}

Generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

void demonstrate_basic_generators() {
    std::cout << "\n=== Basic Generator Coroutines ===\n";

    // Fibonacci sequence
    std::cout << "First 10 Fibonacci numbers: ";
    auto fib = fibonacci();
    int count = 0;
    for (auto value : fib) {
        if (count++ >= 10) break;
        std::cout << value << " ";
    }
    std::cout << "\n";

    // Range generator
    std::cout << "Range 5 to 10: ";
    for (auto value : range(5, 10)) {
        std::cout << value << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 2. ASYNC TASK COROUTINE
// ============================================================================

template<typename T>
struct Task {
    struct promise_type {
        T result;
        std::exception_ptr exception_;

        Task get_return_object() {
            return Task{Handle::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) {
            result = std::move(value);
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle h_;

    explicit Task(Handle h) : h_(h) {}
    ~Task() { if (h_) h_.destroy(); }

    // Move only
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : h_(other.h_) { other.h_ = {}; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = other.h_;
            other.h_ = {};
        }
        return *this;
    }

    T get() {
        if (!h_.done()) {
            // In a real implementation, this would wait for completion
            while (!h_.done()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if (h_.promise().exception_) {
            std::rethrow_exception(h_.promise().exception_);
        }

        return h_.promise().result;
    }

    bool is_ready() const {
        return h_.done();
    }
};

// Simulate async operations
Task<std::string> fetch_data_async(const std::string& url) {
    std::cout << "Starting fetch from: " << url << "\n";

    // Simulate network delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    co_return "Data from " + url;
}

Task<int> calculate_async(int x, int y) {
    std::cout << "Starting calculation: " << x << " + " << y << "\n";

    // Simulate computation delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    co_return x + y;
}

void demonstrate_async_tasks() {
    std::cout << "\n=== Async Task Coroutines ===\n";

    // Launch async operations
    auto data_task = fetch_data_async("https://api.example.com/data");
    auto calc_task = calculate_async(42, 58);

    // Do other work while tasks run
    std::cout << "Doing other work...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Get results
    try {
        std::string data = data_task.get();
        int result = calc_task.get();

        std::cout << "Fetched: " << data << "\n";
        std::cout << "Calculated: " << result << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// ============================================================================
// 3. AWAITABLE COROUTINE FRAMEWORK
// ============================================================================

struct Awaitable {
    bool ready = false;
    std::string result;

    bool await_ready() { return ready; }

    void await_suspend(std::coroutine_handle<> h) {
        // Simulate async work
        std::thread([this, h]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            result = "Async result";
            ready = true;
            h.resume();
        }).detach();
    }

    std::string await_resume() { return result; }
};

Task<std::string> async_workflow() {
    std::cout << "Starting async workflow\n";

    // Await multiple async operations
    Awaitable op1, op2, op3;

    std::string result1 = co_await op1;
    std::cout << "Got result1: " << result1 << "\n";

    std::string result2 = co_await op2;
    std::cout << "Got result2: " << result2 << "\n";

    std::string result3 = co_await op3;
    std::cout << "Got result3: " << result3 << "\n";

    co_return result1 + ", " + result2 + ", " + result3;
}

void demonstrate_awaitable_framework() {
    std::cout << "\n=== Awaitable Coroutine Framework ===\n";

    auto workflow = async_workflow();
    std::string final_result = workflow.get();
    std::cout << "Final result: " << final_result << "\n";
}

// ============================================================================
// 4. LAZY EVALUATION COROUTINES
// ============================================================================

template<typename T>
struct Lazy {
    struct promise_type {
        T value;

        Lazy get_return_object() {
            return Lazy{Handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T val) {
            value = std::move(val);
        }

        void unhandled_exception() {}
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle h_;
    mutable bool evaluated = false;

    explicit Lazy(Handle h) : h_(h) {}
    ~Lazy() { if (h_) h_.destroy(); }

    // Move only
    Lazy(const Lazy&) = delete;
    Lazy& operator=(const Lazy&) = delete;
    Lazy(Lazy&& other) noexcept : h_(other.h_), evaluated(other.evaluated) {
        other.h_ = {};
        other.evaluated = false;
    }
    Lazy& operator=(Lazy&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = other.h_;
            evaluated = other.evaluated;
            other.h_ = {};
            other.evaluated = false;
        }
        return *this;
    }

    T get() const {
        if (!evaluated) {
            h_.resume();
            evaluated = true;
        }
        return h_.promise().value;
    }
};

Lazy<int> expensive_computation(int n) {
    std::cout << "Performing expensive computation for n=" << n << "\n";

    // Simulate expensive work
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int result = 1;
    for (int i = 1; i <= n; ++i) {
        result *= i;
    }

    std::cout << "Computation complete: " << n << "! = " << result << "\n";
    co_return result;
}

void demonstrate_lazy_evaluation() {
    std::cout << "\n=== Lazy Evaluation Coroutines ===\n";

    // Create lazy computations - not executed yet
    auto lazy1 = expensive_computation(5);
    auto lazy2 = expensive_computation(6);
    auto lazy3 = expensive_computation(7);

    std::cout << "Lazy computations created but not executed\n";

    // Only execute when needed
    std::cout << "Getting result for 5!: " << lazy1.get() << "\n";
    std::cout << "Getting result for 7!: " << lazy3.get() << "\n";

    // lazy2 is never evaluated, so expensive_computation(6) never runs
    std::cout << "lazy2 was never evaluated\n";
}

// ============================================================================
// 5. PIPELINE COROUTINES FOR DATA PROCESSING
// ============================================================================

template<typename T>
struct Pipeline {
    struct promise_type {
        std::optional<T> current_value;

        Pipeline get_return_object() {
            return Pipeline{Handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }

        void return_void() {
            current_value = std::nullopt;
        }

        void unhandled_exception() {}
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle h_;

    explicit Pipeline(Handle h) : h_(h) {}
    ~Pipeline() { if (h_) h_.destroy(); }

    // Move only
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& other) noexcept : h_(other.h_) { other.h_ = {}; }
    Pipeline& operator=(Pipeline&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = other.h_;
            other.h_ = {};
        }
        return *this;
    }

    struct iterator {
        Handle h_;

        iterator(Handle h) : h_(h) {}

        iterator& operator++() {
            h_.resume();
            return *this;
        }

        T operator*() const {
            return *h_.promise().current_value;
        }

        bool operator!=(const iterator&) const {
            return h_ && !h_.done() && h_.promise().current_value.has_value();
        }
    };

    iterator begin() {
        if (h_) h_.resume();
        return iterator{h_};
    }

    iterator end() {
        return iterator{Handle{}};
    }
};

// Data source
Pipeline<int> data_source(const std::vector<int>& data) {
    for (int value : data) {
        std::cout << "Source: " << value << "\n";
        co_yield value;
    }
}

// Filter stage
Pipeline<int> filter_even(Pipeline<int> input) {
    for (int value : input) {
        if (value % 2 == 0) {
            std::cout << "Filter: " << value << " (passed)\n";
            co_yield value;
        } else {
            std::cout << "Filter: " << value << " (filtered out)\n";
        }
    }
}

// Transform stage
Pipeline<int> square_values(Pipeline<int> input) {
    for (int value : input) {
        int squared = value * value;
        std::cout << "Transform: " << value << " -> " << squared << "\n";
        co_yield squared;
    }
}

void demonstrate_pipeline_coroutines() {
    std::cout << "\n=== Pipeline Coroutines for Data Processing ===\n";

    std::vector<int> input_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Create processing pipeline
    auto source = data_source(input_data);
    auto filtered = filter_even(std::move(source));
    auto transformed = square_values(std::move(filtered));

    // Process data through pipeline
    std::cout << "Final results: ";
    for (int result : transformed) {
        std::cout << result << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 6. FINANCIAL MARKET DATA STREAMING
// ============================================================================

struct MarketData {
    std::string symbol;
    double price;
    int volume;
    std::chrono::system_clock::time_point timestamp;

    MarketData(std::string sym, double p, int v)
        : symbol(std::move(sym)), price(p), volume(v),
          timestamp(std::chrono::system_clock::now()) {}
};

Pipeline<MarketData> market_data_feed(const std::vector<std::string>& symbols) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_dist(100.0, 200.0);
    std::uniform_int_distribution<> volume_dist(1000, 50000);

    for (int i = 0; i < 20; ++i) {  // Simulate 20 ticks
        for (const auto& symbol : symbols) {
            double price = price_dist(gen);
            int volume = volume_dist(gen);

            MarketData data(symbol, price, volume);
            std::cout << "Feed: " << symbol << " $" << price
                      << " vol:" << volume << "\n";

            co_yield data;

            // Simulate time delay between ticks
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

Pipeline<MarketData> price_filter(Pipeline<MarketData> input, double min_price) {
    for (auto data : input) {
        if (data.price >= min_price) {
            std::cout << "Price Filter: " << data.symbol
                      << " passed ($" << data.price << " >= $" << min_price << ")\n";
            co_yield data;
        }
    }
}

Pipeline<MarketData> volume_filter(Pipeline<MarketData> input, int min_volume) {
    for (auto data : input) {
        if (data.volume >= min_volume) {
            std::cout << "Volume Filter: " << data.symbol
                      << " passed (vol:" << data.volume << " >= " << min_volume << ")\n";
            co_yield data;
        }
    }
}

void demonstrate_market_data_streaming() {
    std::cout << "\n=== Financial Market Data Streaming ===\n";

    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT"};

    // Create market data processing pipeline
    auto feed = market_data_feed(symbols);
    auto price_filtered = price_filter(std::move(feed), 150.0);
    auto volume_filtered = volume_filter(std::move(price_filtered), 25000);

    // Process filtered market data
    std::cout << "Processed market data:\n";
    for (const auto& data : volume_filtered) {
        std::cout << "ALERT: " << data.symbol << " - $" << data.price
                  << " vol:" << data.volume << "\n";
    }
}

// ============================================================================
// 7. ASYNC FILE I/O COROUTINES
// ============================================================================

Task<std::string> read_file_async(const std::string& filename) {
    std::cout << "Starting async file read: " << filename << "\n";

    // Simulate async I/O
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }

    std::cout << "File read complete: " << filename << "\n";
    co_return content;
}

Task<void> write_file_async(const std::string& filename, const std::string& content) {
    std::cout << "Starting async file write: " << filename << "\n";

    // Simulate async I/O
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create file: " + filename);
    }

    file << content;

    std::cout << "File write complete: " << filename << "\n";
    co_return;
}

Task<std::string> process_files_async() {
    try {
        // Create a temporary file for demonstration
        auto write_task = write_file_async("/tmp/coroutine_test.txt",
                                         "Hello from C++20 Coroutines!\nThis is a test file.\n");
        write_task.get();  // Wait for write to complete

        // Read the file back
        auto read_task = read_file_async("/tmp/coroutine_test.txt");
        std::string content = read_task.get();

        co_return content;
    } catch (const std::exception& e) {
        std::cout << "File I/O error: " << e.what() << "\n";
        co_return "Error occurred during file processing";
    }
}

void demonstrate_async_file_io() {
    std::cout << "\n=== Async File I/O Coroutines ===\n";

    auto file_task = process_files_async();
    std::string result = file_task.get();

    std::cout << "File content:\n" << result << "\n";
}

// ============================================================================
// 8. COROUTINE-BASED STATE MACHINES
// ============================================================================

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

struct Connection {
    ConnectionState state = ConnectionState::DISCONNECTED;
    std::string address;

    Connection(std::string addr) : address(std::move(addr)) {}
};

Task<void> connection_state_machine(Connection& conn) {
    std::cout << "Connection state machine started for " << conn.address << "\n";

    // DISCONNECTED -> CONNECTING
    conn.state = ConnectionState::CONNECTING;
    std::cout << "State: CONNECTING to " << conn.address << "\n";

    // Simulate connection attempt
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // CONNECTING -> CONNECTED
    conn.state = ConnectionState::CONNECTED;
    std::cout << "State: CONNECTED to " << conn.address << "\n";

    // Stay connected for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // CONNECTED -> DISCONNECTING
    conn.state = ConnectionState::DISCONNECTING;
    std::cout << "State: DISCONNECTING from " << conn.address << "\n";

    // Simulate cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // DISCONNECTING -> DISCONNECTED
    conn.state = ConnectionState::DISCONNECTED;
    std::cout << "State: DISCONNECTED from " << conn.address << "\n";

    co_return;
}

void demonstrate_state_machines() {
    std::cout << "\n=== Coroutine-based State Machines ===\n";

    Connection conn1("192.168.1.1:8080");
    Connection conn2("10.0.0.1:9090");

    auto state_machine1 = connection_state_machine(conn1);
    auto state_machine2 = connection_state_machine(conn2);

    // Run state machines
    state_machine1.get();
    state_machine2.get();
}

// ============================================================================
// 9. COOPERATIVE MULTITASKING
// ============================================================================

struct Scheduler {
    std::queue<std::coroutine_handle<>> ready_queue;

    void schedule(std::coroutine_handle<> h) {
        ready_queue.push(h);
    }

    void run() {
        while (!ready_queue.empty()) {
            auto h = ready_queue.front();
            ready_queue.pop();

            if (!h.done()) {
                h.resume();
            }
        }
    }
};

Scheduler global_scheduler;

struct YieldAwaitable {
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        global_scheduler.schedule(h);
    }

    void await_resume() {}
};

YieldAwaitable yield() {
    return {};
}

Task<void> cooperative_task(const std::string& name, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        std::cout << name << " iteration " << i << "\n";
        co_await yield();  // Yield control to other tasks
    }

    std::cout << name << " completed\n";
    co_return;
}

void demonstrate_cooperative_multitasking() {
    std::cout << "\n=== Cooperative Multitasking ===\n";

    auto task1 = cooperative_task("Task1", 3);
    auto task2 = cooperative_task("Task2", 3);
    auto task3 = cooperative_task("Task3", 3);

    // Add initial tasks to scheduler
    global_scheduler.schedule(std::coroutine_handle<Task<void>::promise_type>::from_promise(*task1.h_.promise()));
    global_scheduler.schedule(std::coroutine_handle<Task<void>::promise_type>::from_promise(*task2.h_.promise()));
    global_scheduler.schedule(std::coroutine_handle<Task<void>::promise_type>::from_promise(*task3.h_.promise()));

    std::cout << "Running cooperative scheduler:\n";
    global_scheduler.run();
}

// ============================================================================
// 10. ERROR HANDLING IN COROUTINES
// ============================================================================

Task<int> risky_operation(bool should_fail) {
    std::cout << "Starting risky operation (fail=" << std::boolalpha << should_fail << ")\n";

    if (should_fail) {
        throw std::runtime_error("Operation failed!");
    }

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    co_return 42;
}

Task<std::string> error_handling_workflow() {
    try {
        // This will succeed
        int result1 = co_await risky_operation(false);
        std::cout << "First operation succeeded: " << result1 << "\n";

        // This will fail
        int result2 = co_await risky_operation(true);
        std::cout << "Second operation succeeded: " << result2 << "\n";

        co_return "All operations completed successfully";
    } catch (const std::exception& e) {
        std::cout << "Caught exception in coroutine: " << e.what() << "\n";
        co_return "Error occurred: " + std::string(e.what());
    }
}

void demonstrate_error_handling() {
    std::cout << "\n=== Error Handling in Coroutines ===\n";

    auto workflow = error_handling_workflow();
    std::string result = workflow.get();
    std::cout << "Workflow result: " << result << "\n";
}

// ============================================================================
// 11. CO_YIELD vs CO_RETURN - DETAILED COMPARISON
// ============================================================================

/*
 * Key Differences between co_yield and co_return:
 *
 * co_yield:
 * - Suspends the coroutine and produces a value
 * - Coroutine can be resumed later to continue execution
 * - Used in generators and pipelines for producing sequences
 * - Can be called multiple times in the same coroutine
 * - Returns control to the caller but keeps coroutine state alive
 *
 * co_return:
 * - Terminates the coroutine and produces a final value
 * - Coroutine cannot be resumed after co_return
 * - Used to complete the coroutine with a result
 * - Can only be called once (terminates execution)
 * - Destroys coroutine state after returning the value
 */

// Example 1: Generator using co_yield (can produce multiple values)
Generator<int> number_sequence_with_yield() {
    std::cout << "Generator: Starting sequence\n";

    for (int i = 1; i <= 5; ++i) {
        std::cout << "Generator: About to yield " << i << "\n";
        co_yield i;  // Suspend and produce value, can be resumed
        std::cout << "Generator: Resumed after yielding " << i << "\n";
    }

    std::cout << "Generator: Sequence complete\n";
    // Implicit co_return; here (generators typically don't return values)
}

// Example 2: Task using co_return (produces single final value)
Task<int> computation_with_return() {
    std::cout << "Task: Starting computation\n";

    int sum = 0;
    for (int i = 1; i <= 5; ++i) {
        std::cout << "Task: Adding " << i << " to sum\n";
        sum += i;
        // Cannot use co_yield here in a Task that returns a value
        // co_yield i; // This would be invalid for this Task type
    }

    std::cout << "Task: Computation complete, returning result\n";
    co_return sum;  // Terminate and return final value

    // This code would never be reached:
    // std::cout << "This will never print\n";
}

// Example 3: Demonstrating the lifecycle differences
Generator<std::string> lifecycle_demo_yield() {
    std::cout << "Yield Demo: Coroutine started\n";

    co_yield "First value";     // Suspend here, can resume
    std::cout << "Yield Demo: Resumed after first yield\n";

    co_yield "Second value";    // Suspend here, can resume
    std::cout << "Yield Demo: Resumed after second yield\n";

    co_yield "Third value";     // Suspend here, can resume
    std::cout << "Yield Demo: Resumed after third yield\n";

    std::cout << "Yield Demo: About to finish\n";
    // No explicit co_return needed for generators
}

Task<std::string> lifecycle_demo_return() {
    std::cout << "Return Demo: Coroutine started\n";

    std::string result = "Processing";

    // Simulate some work
    for (int i = 1; i <= 3; ++i) {
        std::cout << "Return Demo: Step " << i << "\n";
        result += " step" + std::to_string(i);
    }

    std::cout << "Return Demo: About to return final result\n";
    co_return result;  // Terminates coroutine completely

    // This code is unreachable:
    // std::cout << "This will never execute\n";
}

// Example 4: Mixed usage - Generator that can also return a value
template<typename T>
struct GeneratorWithReturn {
    struct promise_type {
        T current_value;
        std::optional<T> return_value;

        GeneratorWithReturn get_return_object() {
            return GeneratorWithReturn{Handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }

        void return_value(T value) {  // Handle co_return
            return_value = value;
        }

        void unhandled_exception() {}
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle h_;

    explicit GeneratorWithReturn(Handle h) : h_(h) {}
    ~GeneratorWithReturn() { if (h_) h_.destroy(); }

    // Move only
    GeneratorWithReturn(const GeneratorWithReturn&) = delete;
    GeneratorWithReturn& operator=(const GeneratorWithReturn&) = delete;
    GeneratorWithReturn(GeneratorWithReturn&& other) noexcept : h_(other.h_) { other.h_ = {}; }
    GeneratorWithReturn& operator=(GeneratorWithReturn&& other) noexcept {
        if (this != &other) {
            if (h_) h_.destroy();
            h_ = other.h_;
            other.h_ = {};
        }
        return *this;
    }

    struct iterator {
        Handle h_;

        iterator(Handle h) : h_(h) {}

        iterator& operator++() {
            h_.resume();
            return *this;
        }

        T operator*() const {
            return h_.promise().current_value;
        }

        bool operator!=(const iterator&) const {
            return h_ && !h_.done();
        }
    };

    iterator begin() {
        if (h_) h_.resume();
        return iterator{h_};
    }

    iterator end() {
        return iterator{Handle{}};
    }

    std::optional<T> get_return_value() const {
        if (h_ && h_.done()) {
            return h_.promise().return_value;
        }
        return std::nullopt;
    }
};

GeneratorWithReturn<int> mixed_yield_return_demo() {
    std::cout << "Mixed Demo: Starting\n";

    // Yield some values first
    co_yield 10;
    std::cout << "Mixed Demo: Yielded 10, continuing\n";

    co_yield 20;
    std::cout << "Mixed Demo: Yielded 20, continuing\n";

    co_yield 30;
    std::cout << "Mixed Demo: Yielded 30, about to return final value\n";

    // Return a final summary value
    co_return 999;  // This terminates the generator
}

// Example 5: Error handling differences
Generator<int> error_in_yield_demo() {
    try {
        co_yield 1;
        co_yield 2;

        // Simulate an error
        throw std::runtime_error("Error after yielding 2");

        co_yield 3;  // This won't be reached
    } catch (const std::exception& e) {
        std::cout << "Caught in yield demo: " << e.what() << "\n";
        // Can still yield after catching exception
        co_yield -1;  // Error indicator
    }
}

Task<int> error_in_return_demo() {
    try {
        int sum = 0;

        sum += 10;
        sum += 20;

        // Simulate an error
        throw std::runtime_error("Error during computation");

        sum += 30;  // This won't be reached either
        co_return sum;  // This won't be reached either
    } catch (const std::exception& e) {
        std::cout << "Caught in return demo: " << e.what() << "\n";
        co_return -1;  // Return error indicator and terminate
    }
}

void demonstrate_yield_vs_return() {
    std::cout << "\n=== CO_YIELD vs CO_RETURN Comparison ===\n";

    // 1. Basic difference demonstration
    std::cout << "\n1. Basic Generator with co_yield:\n";
    auto generator = number_sequence_with_yield();
    for (auto value : generator) {
        std::cout << "Main: Received yielded value: " << value << "\n";
    }

    std::cout << "\n2. Basic Task with co_return:\n";
    auto task = computation_with_return();
    int result = task.get();
    std::cout << "Main: Received final result: " << result << "\n";

    // 2. Lifecycle demonstration
    std::cout << "\n3. Lifecycle with co_yield (multiple suspensions):\n";
    auto yield_demo = lifecycle_demo_yield();
    for (const auto& value : yield_demo) {
        std::cout << "Main: Got from yield: " << value << "\n";
    }

    std::cout << "\n4. Lifecycle with co_return (single termination):\n";
    auto return_demo = lifecycle_demo_return();
    std::string final_result = return_demo.get();
    std::cout << "Main: Got from return: " << final_result << "\n";

    // 3. Mixed usage
    std::cout << "\n5. Mixed co_yield and co_return:\n";
    auto mixed_demo = mixed_yield_return_demo();
    std::cout << "Main: Iterating through yielded values:\n";
    for (auto value : mixed_demo) {
        std::cout << "Main: Yielded value: " << value << "\n";
    }
    auto return_val = mixed_demo.get_return_value();
    if (return_val) {
        std::cout << "Main: Final return value: " << *return_val << "\n";
    }

    // 4. Error handling differences
    std::cout << "\n6. Error handling with co_yield:\n";
    auto error_yield = error_in_yield_demo();
    for (auto value : error_yield) {
        std::cout << "Main: Received (possibly error) value: " << value << "\n";
    }

    std::cout << "\n7. Error handling with co_return:\n";
    auto error_return = error_in_return_demo();
    int error_result = error_return.get();
    std::cout << "Main: Final result (possibly error): " << error_result << "\n";
}

// ============================================================================
// 12. PRACTICAL SCENARIOS: WHEN TO USE CO_YIELD vs CO_RETURN
// ============================================================================

/*
 * Use co_yield when:
 * - Creating sequences or streams of data
 * - Building data processing pipelines
 * - Implementing iterators or generators
 * - Need to produce multiple values over time
 * - Want lazy evaluation of sequences
 *
 * Use co_return when:
 * - Performing async operations that produce a single result
 * - Computing a final value after processing
 * - Implementing async functions similar to std::future
 * - Need to terminate and clean up coroutine state
 * - Returning status or summary information
 */

// Scenario 1: File processing - co_yield for streaming lines
Generator<std::string> read_file_lines_yield(const std::string& content) {
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        std::cout << "File Reader: Yielding line: " << line << "\n";
        co_yield line;  // Stream each line as it's read
    }

    std::cout << "File Reader: No more lines\n";
}

// Scenario 2: File processing - co_return for final content
Task<std::string> read_file_content_return(const std::string& content) {
    std::cout << "File Reader: Processing entire file\n";

    // Process all content at once
    std::string processed = "PROCESSED: " + content;

    std::cout << "File Reader: File processing complete\n";
    co_return processed;  // Return the entire processed content
}

// Scenario 3: Mathematical sequences - co_yield for infinite series
Generator<double> fibonacci_ratios() {
    double a = 1.0, b = 1.0;

    while (true) {
        double ratio = b / a;
        co_yield ratio;  // Infinite sequence of ratios

        double temp = a;
        a = b;
        b += temp;
    }
}

// Scenario 4: Mathematical computation - co_return for final result
Task<double> calculate_pi_approximation(int iterations) {
    std::cout << "Pi Calculator: Starting with " << iterations << " iterations\n";

    double pi_approx = 0.0;
    for (int i = 0; i < iterations; ++i) {
        pi_approx += (i % 2 == 0 ? 1.0 : -1.0) / (2.0 * i + 1.0);
    }
    pi_approx *= 4.0;

    std::cout << "Pi Calculator: Calculation complete\n";
    co_return pi_approx;  // Single final approximation
}

// Scenario 5: Market data - co_yield for streaming prices
Generator<double> stock_price_stream(double initial_price, int num_ticks) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> price_change(0.0, 0.5);  // Mean 0, StdDev 0.5

    double current_price = initial_price;

    for (int i = 0; i < num_ticks; ++i) {
        current_price += price_change(gen);
        current_price = std::max(0.01, current_price);  // Ensure positive price

        std::cout << "Price Feed: Tick " << i << " - $" << current_price << "\n";
        co_yield current_price;  // Stream each price update

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Scenario 6: Market data - co_return for analysis result
Task<double> calculate_average_price(const std::vector<double>& prices) {
    std::cout << "Price Analyzer: Calculating average of " << prices.size() << " prices\n";

    if (prices.empty()) {
        co_return 0.0;
    }

    double sum = 0.0;
    for (double price : prices) {
        sum += price;
    }

    double average = sum / prices.size();
    std::cout << "Price Analyzer: Average calculated: $" << average << "\n";

    co_return average;  // Single final analysis result
}

void demonstrate_practical_scenarios() {
    std::cout << "\n=== Practical Scenarios: When to Use co_yield vs co_return ===\n";

    // File processing comparison
    std::cout << "\n1. File Processing - Streaming vs Batch:\n";
    std::string file_content = "Line 1\nLine 2\nLine 3\nLine 4";

    std::cout << "Using co_yield for streaming lines:\n";
    auto line_stream = read_file_lines_yield(file_content);
    for (const auto& line : line_stream) {
        std::cout << "Main: Processing line: " << line << "\n";
    }

    std::cout << "\nUsing co_return for batch processing:\n";
    auto content_task = read_file_content_return(file_content);
    std::string processed_content = content_task.get();
    std::cout << "Main: Got processed content: " << processed_content << "\n";

    // Mathematical computation comparison
    std::cout << "\n2. Mathematical Operations - Sequences vs Results:\n";

    std::cout << "Using co_yield for Fibonacci ratios (first 10):\n";
    auto ratios = fibonacci_ratios();
    int count = 0;
    for (auto ratio : ratios) {
        if (count++ >= 10) break;
        std::cout << "Main: Fibonacci ratio: " << ratio << "\n";
    }

    std::cout << "\nUsing co_return for Pi approximation:\n";
    auto pi_task = calculate_pi_approximation(1000000);
    double pi_approx = pi_task.get();
    std::cout << "Main: Pi approximation: " << pi_approx << "\n";

    // Market data comparison
    std::cout << "\n3. Market Data - Streaming vs Analysis:\n";

    std::cout << "Using co_yield for streaming prices:\n";
    std::vector<double> collected_prices;
    auto price_stream = stock_price_stream(100.0, 5);
    for (auto price : price_stream) {
        std::cout << "Main: Received price update: $" << price << "\n";
        collected_prices.push_back(price);
    }

    std::cout << "\nUsing co_return for price analysis:\n";
    auto analysis_task = calculate_average_price(collected_prices);
    double avg_price = analysis_task.get();
    std::cout << "Main: Average price analysis: $" << avg_price << "\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 Coroutines Use Cases and Examples\n";
    std::cout << "=======================================\n";

    demonstrate_basic_generators();
    demonstrate_async_tasks();
    demonstrate_awaitable_framework();
    demonstrate_lazy_evaluation();
    demonstrate_pipeline_coroutines();
    demonstrate_market_data_streaming();
    demonstrate_async_file_io();
    demonstrate_state_machines();
    // Note: Cooperative multitasking demo disabled due to complexity
    // demonstrate_cooperative_multitasking();
    demonstrate_error_handling();
    demonstrate_yield_vs_return();
    demonstrate_practical_scenarios();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Coroutines enable writing asynchronous code that looks synchronous\n";
    std::cout << "2. Generators provide lazy evaluation and infinite sequences\n";
    std::cout << "3. Tasks enable async/await patterns for concurrent programming\n";
    std::cout << "4. Pipeline coroutines excellent for data processing workflows\n";
    std::cout << "5. State machines become much cleaner with coroutines\n";
    std::cout << "6. Error handling works naturally with try/catch\n";
    std::cout << "7. Memory efficient - stackless coroutines\n";
    std::cout << "8. Cooperative multitasking without complex thread management\n";
    std::cout << "9. Perfect for I/O bound operations\n";
    std::cout << "10. Excellent for financial data processing and streaming\n";
    std::cout << "11. co_yield for sequences/streams, co_return for final results\n";
    std::cout << "12. co_yield suspends and resumes, co_return terminates\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler with coroutine support
 * - GCC 11+, Clang 14+, or MSVC 2019+
 * - Use -std=c++20 -fcoroutines flags
 *
 * Example compilation:
 * g++ -std=c++20 -fcoroutines -pthread cpp20_coroutines_use_cases_examples.cpp -o coroutines_demo
 *
 * Key Coroutine Components:
 * 1. promise_type - Defines coroutine behavior
 * 2. co_await - Suspend and wait for result
 * 3. co_yield - Suspend and produce value
 * 4. co_return - Return value and complete
 * 5. std::coroutine_handle - Handle to coroutine
 * 6. std::suspend_always/never - Control suspension
 *
 * Common Use Cases:
 * - Generators and lazy sequences
 * - Async I/O operations
 * - Data processing pipelines
 * - State machines
 * - Cooperative multitasking
 * - Event-driven programming
 * - Network programming
 * - Real-time data streaming
 */
