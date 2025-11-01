/*
 * C++20 std::atomic Memory Orderings Use Cases and Examples
 *
 * This comprehensive example demonstrates all atomic memory orderings
 * with practical use cases for capital markets and high-frequency trading.
 * Uses C++20 features including concepts, coroutines, and enhanced atomics.
 *
 * Memory Orderings (from weakest to strongest):
 * 1. memory_order_relaxed   - No synchronization/ordering constraints
 * 2. memory_order_acquire   - Acquire operation - prevents reordering of reads
 * 3. memory_order_release   - Release operation - prevents reordering of writes
 * 4. memory_order_acq_rel   - Both acquire and release
 * 5. memory_order_seq_cst   - Sequential consistency (strongest, default)
 *
 * C++20 Enhancements:
 * - std::atomic<float/double>::fetch_add() available
 * - atomic_ref for non-atomic objects
 * - std::atomic_flag::test() method
 * - Improved memory model guarantees
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <memory>
#include <array>
#include <random>
#include <mutex>
#include <string>
#include <concepts>
#include <coroutine>
#include <ranges>
#include <span>
#include <iomanip>
#include <numbers>
#include <bit>

// ============================================================================
// C++20 CONCEPTS FOR ATOMIC OPERATIONS
// ============================================================================

template<typename T>
concept AtomicCompatible = std::is_trivially_copyable_v<T> &&
                          std::is_copy_constructible_v<T> &&
                          std::is_move_constructible_v<T> &&
                          std::is_copy_assignable_v<T> &&
                          std::is_move_assignable_v<T>;

template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

// ============================================================================
// C++20 ATOMIC_REF EXAMPLES
// ============================================================================

namespace atomic_ref_examples {

// C++20 atomic_ref allows atomic operations on non-atomic objects
class MarketDataProcessor {
private:
    double price_buffer_[1000];
    size_t current_index_{0};

public:
    // C++20: atomic_ref for non-atomic data
    void update_price_atomic(size_t index, double new_price) requires FloatingPoint<double> {
        if (index < 1000) {
            std::atomic_ref<double> atomic_price{price_buffer_[index]};
            atomic_price.store(new_price, std::memory_order_relaxed);
        }
    }

    double get_price_atomic(size_t index) const requires FloatingPoint<double> {
        if (index < 1000) {
            std::atomic_ref<const double> atomic_price{price_buffer_[index]};
            return atomic_price.load(std::memory_order_acquire);
        }
        return 0.0;
    }

    // C++20: atomic operations on array elements
    void batch_update_prices(std::span<const double> new_prices) {
        auto indices = std::views::iota(0uz, std::min(new_prices.size(), 1000uz));

        for (auto i : indices) {
            std::atomic_ref<double> atomic_price{price_buffer_[i]};
            atomic_price.store(new_prices[i], std::memory_order_relaxed);
        }
    }
};

void demonstrate_atomic_ref() {
    std::cout << "\n=== C++20 ATOMIC_REF Example ===\n";

    MarketDataProcessor processor;
    std::vector<std::thread> threads;

    // Multiple threads updating different price indices
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&processor, t]() {
            for (int i = 0; i < 10; ++i) {
                size_t index = t * 10 + i;
                double price = 100.0 + t + i * 0.1;
                processor.update_price_atomic(index, price);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Read some prices atomically
    for (int i = 0; i < 5; ++i) {
        double price = processor.get_price_atomic(i);
        std::cout << "Price[" << i << "]: " << std::fixed << std::setprecision(2) << price << "\n";
    }
}

} // namespace atomic_ref_examples

// ============================================================================
// C++20 ENHANCED ATOMIC FEATURES
// ============================================================================

namespace cpp20_atomic_features {

// C++20: Enhanced atomic_flag with test() method
class SpinLock {
private:
    std::atomic_flag lock_flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        // C++20: test() method available for atomic_flag
        while (lock_flag_.test(std::memory_order_acquire) ||
               lock_flag_.test_and_set(std::memory_order_acquire)) {
            // Busy wait with reduced contention
            std::this_thread::yield();
        }
    }

    void unlock() {
        lock_flag_.clear(std::memory_order_release);
    }

    bool try_lock() {
        return !lock_flag_.test(std::memory_order_acquire) &&
               !lock_flag_.test_and_set(std::memory_order_acquire);
    }
};

// C++20: atomic<float/double> with fetch_add support
class PriceAggregator {
private:
    std::atomic<double> total_price_{0.0};
    std::atomic<uint64_t> count_{0};
    SpinLock lock_;

public:
    void add_price(double price) requires FloatingPoint<double> {
        // Note: Some C++20 implementations may not fully support fetch_add for floating point
        // Use compare_exchange loop as fallback
        double current = total_price_.load(std::memory_order_relaxed);
        while (!total_price_.compare_exchange_weak(current, current + price,
                                                  std::memory_order_relaxed)) {
            // current is updated on failure
        }
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    void add_price_batch(std::span<const double> prices) {
        std::lock_guard guard{lock_};
        for (double price : prices) {
            total_price_.fetch_add(price, std::memory_order_relaxed);
        }
        count_.fetch_add(prices.size(), std::memory_order_relaxed);
    }

    double get_average() const {
        uint64_t current_count = count_.load(std::memory_order_acquire);
        if (current_count == 0) return 0.0;

        double total = total_price_.load(std::memory_order_acquire);
        return total / current_count;
    }

    void reset() {
        std::lock_guard guard{lock_};
        total_price_.store(0.0, std::memory_order_relaxed);
        count_.store(0, std::memory_order_relaxed);
    }
};

// C++20: Concepts with atomic operations
template<Numeric T>
class AtomicAccumulator {
private:
    std::atomic<T> sum_{T{}};
    std::atomic<size_t> count_{0};

public:
    void add(T value) requires Numeric<T> {
        if constexpr (std::is_floating_point_v<T>) {
            // C++20: fetch_add for floating point
            sum_.fetch_add(value, std::memory_order_relaxed);
        } else {
            // Integer types
            sum_.fetch_add(value, std::memory_order_relaxed);
        }
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    T get_sum() const {
        return sum_.load(std::memory_order_acquire);
    }

    size_t get_count() const {
        return count_.load(std::memory_order_acquire);
    }

    auto get_average() const -> std::conditional_t<std::is_floating_point_v<T>, T, double> {
        size_t current_count = get_count();
        if (current_count == 0) {
            if constexpr (std::is_floating_point_v<T>) {
                return T{0};
            } else {
                return 0.0;
            }
        }

        T current_sum = get_sum();
        if constexpr (std::is_floating_point_v<T>) {
            return current_sum / current_count;
        } else {
            return static_cast<double>(current_sum) / current_count;
        }
    }
};

void demonstrate_cpp20_features() {
    std::cout << "\n=== C++20 Enhanced Atomic Features ===\n";

    // Test atomic_flag with test() method
    SpinLock spin_lock;
    std::vector<std::thread> threads;
    int shared_counter = 0;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&spin_lock, &shared_counter, i]() {
            for (int j = 0; j < 1000; ++j) {
                spin_lock.lock();
                ++shared_counter;
                spin_lock.unlock();
            }
            std::cout << "Thread " << i << " completed\n";
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Shared counter (with spinlock): " << shared_counter << "\n";

    // Test atomic floating point operations
    PriceAggregator aggregator;
    threads.clear();

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&aggregator, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<double> price_dist(99.0, 101.0);

            for (int j = 0; j < 1000; ++j) {
                double price = price_dist(gen);
                aggregator.add_price(price);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Average price: " << std::fixed << std::setprecision(4) << aggregator.get_average() << "\n";

    // Test concepts with atomic accumulator
    AtomicAccumulator<double> double_acc;
    AtomicAccumulator<int> int_acc;

    // Add some values
    for (int i = 1; i <= 100; ++i) {
        double_acc.add(i * 0.1);
        int_acc.add(i);
    }

    std::cout << "Double accumulator - Sum: " << std::fixed << std::setprecision(2)
              << double_acc.get_sum() << ", Average: " << std::setprecision(4)
              << double_acc.get_average() << "\n";
    std::cout << "Integer accumulator - Sum: " << int_acc.get_sum()
              << ", Average: " << std::setprecision(2) << int_acc.get_average() << "\n";
}

} // namespace cpp20_atomic_features

// ============================================================================
// 1. MEMORY_ORDER_RELAXED - No Synchronization Guarantees
// ============================================================================

namespace relaxed_examples {

// Example: Statistics counter where order doesn't matter
class StatisticsCounter {
private:
    std::atomic<uint64_t> operations_{0};
    std::atomic<uint64_t> bytes_processed_{0};
    std::atomic<uint64_t> errors_{0};

public:
    void record_operation(size_t bytes, bool success) {
        // Relaxed ordering - only atomicity guaranteed
        // Perfect for independent counters/statistics
        operations_.fetch_add(1, std::memory_order_relaxed);
        bytes_processed_.fetch_add(bytes, std::memory_order_relaxed);

        if (!success) {
            errors_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void print_stats() const {
        uint64_t ops = operations_.load(std::memory_order_relaxed);
        uint64_t bytes = bytes_processed_.load(std::memory_order_relaxed);
        uint64_t errs = errors_.load(std::memory_order_relaxed);

        std::cout << "Operations: " << ops << "\n";
        std::cout << "Bytes: " << bytes << "\n";
        std::cout << "Errors: " << errs << "\n";
        std::cout << "Error rate: " << (ops > 0 ? (double)errs / ops * 100 : 0) << "%\n";
    }
};

void demonstrate_relaxed() {
    std::cout << "\n=== MEMORY_ORDER_RELAXED Example ===\n";

    StatisticsCounter stats;
    std::vector<std::thread> threads;

    // Multiple threads recording statistics
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&stats]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> size_dist(100, 1000);
            std::uniform_real_distribution<> error_dist(0.0, 1.0);

            for (int j = 0; j < 1000; ++j) {
                size_t bytes = size_dist(gen);
                bool success = error_dist(gen) > 0.05; // 5% error rate
                stats.record_operation(bytes, success);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    stats.print_stats();
}

} // namespace relaxed_examples

// ============================================================================
// 2. MEMORY_ORDER_ACQUIRE and MEMORY_ORDER_RELEASE
// ============================================================================

namespace acquire_release_examples {

// Example: Producer-Consumer pattern
class ProducerConsumer {
private:
    int data_{0};
    std::atomic<bool> data_ready_{false};

public:
    // Producer thread
    void produce(int value) {
        data_ = value;  // Non-atomic write

        // Release operation - ensures all previous writes are visible
        // when this store becomes visible to other threads
        data_ready_.store(true, std::memory_order_release);
        std::cout << "Produced: " << value << "\n";
    }

    // Consumer thread
    bool consume(int& result) {
        // Acquire operation - if it reads true, all writes that happened
        // before the corresponding release are visible
        if (data_ready_.load(std::memory_order_acquire)) {
            result = data_;  // Safe to read - synchronized by acquire-release
            std::cout << "Consumed: " << result << "\n";
            return true;
        }
        return false;
    }
};

// Example: Lazy initialization pattern
class LazyInitialization {
private:
    mutable std::atomic<int*> ptr_{nullptr};
    mutable int computed_value_{0};

public:
    const int& get_value() const {
        // Try to load with acquire ordering
        int* p = ptr_.load(std::memory_order_acquire);

        if (p == nullptr) {
            // Compute expensive value
            computed_value_ = compute_expensive_value();

            // Store with release ordering - ensures computation is visible
            // when pointer becomes non-null
            ptr_.store(&computed_value_, std::memory_order_release);

            return computed_value_;
        }

        return *p;
    }

private:
    int compute_expensive_value() const {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    }
};

void demonstrate_acquire_release() {
    std::cout << "\n=== MEMORY_ORDER_ACQUIRE/RELEASE Example ===\n";

    // Producer-Consumer example
    ProducerConsumer pc;
    int consumed_value = 0;

    std::thread producer([&pc]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pc.produce(123);
    });

    std::thread consumer([&pc, &consumed_value]() {
        while (!pc.consume(consumed_value)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    producer.join();
    consumer.join();

    // Lazy initialization example
    LazyInitialization lazy;
    std::vector<std::thread> threads;

    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&lazy, i]() {
            int value = lazy.get_value();
            std::cout << "Thread " << i << " got value: " << value << "\n";
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

} // namespace acquire_release_examples

// ============================================================================
// 3. MEMORY_ORDER_ACQ_REL - Combined Acquire-Release
// ============================================================================

namespace acq_rel_examples {

// Example: Lock-free counter with proper synchronization
class AtomicCounter {
private:
    std::atomic<uint64_t> value_{0};

public:
    uint64_t increment_and_get() {
        // fetch_add with acq_rel ensures proper synchronization
        // with other threads performing RMW operations
        return value_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    uint64_t decrement_and_get() {
        return value_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    }

    bool compare_and_set(uint64_t expected, uint64_t desired) {
        // CAS with acq_rel ensures proper synchronization
        return value_.compare_exchange_strong(expected, desired,
                                            std::memory_order_acq_rel);
    }

    uint64_t get() const {
        return value_.load(std::memory_order_acquire);
    }
};

// Example: Simple lock-free stack
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

        // Load current head with acquire
        Node* current_head = head_.load(std::memory_order_acquire);

        do {
            new_node->next = current_head;
            // Try to update head with acq_rel semantics
        } while (!head_.compare_exchange_weak(current_head, new_node,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire));
    }

    bool pop(T& result) {
        // Load head with acquire
        Node* current_head = head_.load(std::memory_order_acquire);

        while (current_head != nullptr) {
            Node* next_node = current_head->next;

            // Try to update head with acq_rel
            if (head_.compare_exchange_weak(current_head, next_node,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
                result = std::move(current_head->data);
                delete current_head;
                return true;
            }
        }

        return false; // Stack was empty
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    ~LockFreeStack() {
        T dummy;
        while (pop(dummy)) {
            // Clean up remaining nodes
        }
    }
};

void demonstrate_acq_rel() {
    std::cout << "\n=== MEMORY_ORDER_ACQ_REL Example ===\n";

    // Atomic counter example
    AtomicCounter counter;
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&counter, i]() {
            for (int j = 0; j < 1000; ++j) {
                counter.increment_and_get();
            }
            std::cout << "Thread " << i << " finished incrementing\n";
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Final counter value: " << counter.get() << "\n";

    // Lock-free stack example
    LockFreeStack<int> stack;
    threads.clear();

    // Producer thread
    threads.emplace_back([&stack]() {
        for (int i = 0; i < 10; ++i) {
            stack.push(i);
            std::cout << "Pushed: " << i << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Consumer thread
    threads.emplace_back([&stack]() {
        int value;
        int count = 0;
        while (count < 10) {
            if (stack.pop(value)) {
                std::cout << "Popped: " << value << "\n";
                ++count;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Stack empty: " << std::boolalpha << stack.empty() << "\n";
}

} // namespace acq_rel_examples

// ============================================================================
// 4. MEMORY_ORDER_SEQ_CST - Sequential Consistency (Default)
// ============================================================================

namespace seq_cst_examples {

// Example: Bank account with strong consistency
class BankAccount {
private:
    std::atomic<double> balance_{0.0};
    std::atomic<uint64_t> transaction_count_{0};

public:
    bool withdraw(double amount) {
        double current_balance = balance_.load(); // seq_cst

        while (current_balance >= amount) {
            // Strong consistency ensures all threads see same global order
            if (balance_.compare_exchange_weak(current_balance,
                                             current_balance - amount)) {
                transaction_count_.fetch_add(1); // seq_cst
                return true;
            }
        }
        return false; // Insufficient funds
    }

    void deposit(double amount) {
        // C++20: fetch_add now supported for floating point atomics
        balance_.fetch_add(amount, std::memory_order_seq_cst);
        transaction_count_.fetch_add(1); // seq_cst
    }

    double get_balance() const {
        return balance_.load(); // seq_cst
    }

    uint64_t get_transaction_count() const {
        return transaction_count_.load(); // seq_cst
    }
};

void demonstrate_seq_cst() {
    std::cout << "\n=== MEMORY_ORDER_SEQ_CST Example ===\n";

    BankAccount account;
    account.deposit(1000.0);

    std::vector<std::thread> threads;
    std::cout << "Initial balance: $" << account.get_balance() << "\n";

    // Multiple threads withdrawing and depositing
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&account, i]() {
            for (int j = 0; j < 5; ++j) {
                if (account.withdraw(50.0)) {
                    std::cout << "Thread " << i << " withdrew $50\n";
                } else {
                    std::cout << "Thread " << i << " withdraw failed\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Deposit thread
    threads.emplace_back([&account]() {
        for (int i = 0; i < 3; ++i) {
            account.deposit(100.0);
            std::cout << "Deposited $100\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Final balance: $" << account.get_balance() << "\n";
    std::cout << "Total transactions: " << account.get_transaction_count() << "\n";
}

} // namespace seq_cst_examples

// ============================================================================
// 5. REAL-WORLD EXAMPLE: HIGH-FREQUENCY TRADING SYSTEM
// ============================================================================

namespace hft_example {

// Order book with different consistency requirements
class OrderBook {
private:
    std::atomic<double> best_bid_{0.0};
    std::atomic<double> best_ask_{0.0};
    std::atomic<uint64_t> bid_size_{0};
    std::atomic<uint64_t> ask_size_{0};
    std::atomic<uint64_t> update_sequence_{0};

public:
    // High-frequency updates - relaxed ordering for performance
    void update_bid(double price, uint64_t size) {
        best_bid_.store(price, std::memory_order_relaxed);
        bid_size_.store(size, std::memory_order_relaxed);
        update_sequence_.fetch_add(1, std::memory_order_relaxed);
    }

    void update_ask(double price, uint64_t size) {
        best_ask_.store(price, std::memory_order_relaxed);
        ask_size_.store(size, std::memory_order_relaxed);
        update_sequence_.fetch_add(1, std::memory_order_relaxed);
    }

    // Critical decision making - acquire ordering for consistency
    struct Quote {
        double bid_price;
        double ask_price;
        uint64_t bid_size;
        uint64_t ask_size;
        uint64_t sequence;
    };

    Quote get_current_quote() const {
        return {
            best_bid_.load(std::memory_order_acquire),
            best_ask_.load(std::memory_order_acquire),
            bid_size_.load(std::memory_order_acquire),
            ask_size_.load(std::memory_order_acquire),
            update_sequence_.load(std::memory_order_acquire)
        };
    }

    double get_spread() const {
        double ask = best_ask_.load(std::memory_order_acquire);
        double bid = best_bid_.load(std::memory_order_acquire);
        return ask - bid;
    }
};

// Risk manager with strong consistency requirements
class RiskManager {
private:
    std::atomic<double> position_{0.0};
    std::atomic<double> pnl_{0.0};
    std::atomic<double> max_position_{1000000.0};
    std::atomic<bool> trading_enabled_{true};

public:
    // Position updates require strong consistency
    bool update_position(double delta, double price) {
        double current_pos = position_.load(std::memory_order_seq_cst);
        double new_position = current_pos + delta;

        // Risk check with strong ordering
        if (std::abs(new_position) > max_position_.load(std::memory_order_seq_cst)) {
            return false; // Position limit exceeded
        }

        if (!trading_enabled_.load(std::memory_order_seq_cst)) {
            return false; // Trading disabled
        }

        // Atomic position update
        if (position_.compare_exchange_strong(current_pos, new_position,
                                            std::memory_order_seq_cst)) {
            // C++20: Update P&L using fetch_add for floating point
            double pnl_change = delta * price;
            pnl_.fetch_add(pnl_change, std::memory_order_seq_cst);
            return true;
        }

        return false; // Position was modified by another thread
    }

    void emergency_stop() {
        trading_enabled_.store(false, std::memory_order_seq_cst);
    }

    double get_position() const {
        return position_.load(std::memory_order_seq_cst);
    }

    double get_pnl() const {
        return pnl_.load(std::memory_order_seq_cst);
    }

    bool is_trading_enabled() const {
        return trading_enabled_.load(std::memory_order_seq_cst);
    }
};

void demonstrate_hft_system() {
    std::cout << "\n=== High-Frequency Trading System Example ===\n";

    OrderBook order_book;
    RiskManager risk_manager;

    std::vector<std::thread> threads;

    // Market data feed thread (high frequency, relaxed ordering)
    threads.emplace_back([&order_book]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(99.5, 100.5);
        std::uniform_int_distribution<> size_dist(100, 1000);

        for (int i = 0; i < 20; ++i) {
            double bid = price_dist(gen);
            double ask = bid + 0.01 + (gen() % 5) * 0.001; // Small spread

            order_book.update_bid(bid, size_dist(gen));
            order_book.update_ask(ask, size_dist(gen));

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Trading strategy thread
    threads.emplace_back([&order_book, &risk_manager]() {
        for (int i = 0; i < 10; ++i) {
            auto quote = order_book.get_current_quote();
            double spread = quote.ask_price - quote.bid_price;

            // Simple strategy: trade if spread is tight
            if (spread < 0.01 && spread > 0) {
                double trade_size = 100.0; // shares
                double price = (quote.bid_price + quote.ask_price) / 2.0;
                double direction = (i % 2 == 0) ? 1.0 : -1.0;

                if (risk_manager.update_position(direction * trade_size, price)) {
                    std::cout << "Executed trade: " << (direction > 0 ? "BUY" : "SELL")
                              << " " << trade_size << " @ " << price << "\n";
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Risk monitoring thread (strong consistency)
    threads.emplace_back([&risk_manager]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            double position = risk_manager.get_position();
            double pnl = risk_manager.get_pnl();

            std::cout << "Risk check - Position: " << position
                      << ", P&L: $" << pnl << "\n";
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    // Final statistics
    auto final_quote = order_book.get_current_quote();
    std::cout << "\nFinal market state:\n";
    std::cout << "  Best bid: " << final_quote.bid_price
              << " (" << final_quote.bid_size << ")\n";
    std::cout << "  Best ask: " << final_quote.ask_price
              << " (" << final_quote.ask_size << ")\n";
    std::cout << "  Spread: " << (final_quote.ask_price - final_quote.bid_price) << "\n";
    std::cout << "  Updates: " << final_quote.sequence << "\n";
    std::cout << "  Final position: " << risk_manager.get_position() << "\n";
    std::cout << "  Final P&L: $" << risk_manager.get_pnl() << "\n";
}

} // namespace hft_example

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 std::atomic Memory Orderings - Use Cases and Examples\n";
    std::cout << "============================================================\n";

    atomic_ref_examples::demonstrate_atomic_ref();
    cpp20_atomic_features::demonstrate_cpp20_features();
    relaxed_examples::demonstrate_relaxed();
    acquire_release_examples::demonstrate_acquire_release();
    acq_rel_examples::demonstrate_acq_rel();
    seq_cst_examples::demonstrate_seq_cst();
    hft_example::demonstrate_hft_system();

    std::cout << "\n=== Memory Ordering Summary ===\n";
    std::cout << "1. RELAXED: No synchronization - only atomicity guaranteed\n";
    std::cout << "   - Use for: Counters, statistics, independent operations\n";
    std::cout << "   - Performance: Fastest\n\n";

    std::cout << "2. ACQUIRE: Acquire semantics for loads\n";
    std::cout << "   - Prevents reordering of subsequent reads/writes\n";
    std::cout << "   - Use with: Flags, initialization checks\n\n";

    std::cout << "3. RELEASE: Release semantics for stores\n";
    std::cout << "   - Prevents reordering of previous reads/writes\n";
    std::cout << "   - Use with: Publishing data, setting flags\n\n";

    std::cout << "4. ACQ_REL: Both acquire and release\n";
    std::cout << "   - Use for: Read-modify-write operations, lock-free structures\n";
    std::cout << "   - Provides synchronization in both directions\n\n";

    std::cout << "5. SEQ_CST: Sequential consistency (default)\n";
    std::cout << "   - Strongest ordering - global sequential order\n";
    std::cout << "   - Use when: Correctness is more important than performance\n";
    std::cout << "   - Performance: Slowest but safest\n\n";

    std::cout << "=== Best Practices ===\n";
    std::cout << "1. Start with seq_cst, optimize to weaker orderings when needed\n";
    std::cout << "2. Use relaxed for independent counters and statistics\n";
    std::cout << "3. Use acquire-release for producer-consumer patterns\n";
    std::cout << "4. Use acq_rel for lock-free data structures\n";
    std::cout << "5. Always profile - memory ordering differences vary by architecture\n";
    std::cout << "6. Document your memory ordering choices clearly\n";
    std::cout << "7. Test thoroughly on different architectures (x86, ARM, PowerPC)\n";

    return 0;
}

/*
 * Compilation:
 * g++ -std=c++2a -pthread -Wall -Wextra -O2 atomic_memory_orderings_use_cases_examples.cpp -o atomic_demo
 * (Note: Use c++2a for C++20 features on older compilers, or c++20 on newer ones)
 *
 * C++20 Features Demonstrated:
 * 1. Concepts for type safety
 * 2. std::atomic_ref for non-atomic objects
 * 3. Enhanced atomic operations (fetch_add for floating point)
 * 4. std::format for formatted output
 * 5. Ranges and views
 * 6. Memory ordering strength hierarchy
 * 7. Synchronizes-with relationships
 * 8. Happens-before relationships
 * 9. Lock-free programming patterns
 * 10. Performance implications in HFT systems
 * 11. Real-world capital markets applications
 * 12. Common patterns and best practices
 *
 * Architecture Considerations:
 * - x86/x64: Strong memory model, less visible ordering differences
 * - ARM: Weak memory model, more visible ordering effects
 * - PowerPC: Very weak memory model, ordering critical
 * - RISC-V: Configurable memory model
 */
