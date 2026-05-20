/*
 * C++20 Ranges Use Cases and Examples
 *
 * Ranges provide a composable, lazy-evaluated way to work with sequences of data.
 * They offer a functional programming approach with better performance and
 * more readable code than traditional iterators.
 *
 * Key Benefits:
 * 1. Composable operations (chain operations together)
 * 2. Lazy evaluation (operations performed only when needed)
 * 3. Better performance (fewer temporary objects)
 * 4. More readable code (functional style)
 * 5. Type safety and concepts integration
 * 6. Seamless integration with existing STL algorithms
 */

#include <iostream>
#include <vector>
#include <string>
#include <ranges>
#include <algorithm>
#include <numeric>
#include <functional>
#include <map>
#include <set>
#include <deque>
#include <list>
#include <array>
#include <chrono>
#include <random>
#include <memory>

// ============================================================================
// 1. BASIC RANGES OPERATIONS
// ============================================================================

void demonstrate_basic_ranges() {
    std::cout << "\n=== Basic Ranges Operations ===\n";

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Traditional approach
    std::cout << "Traditional approach:\n";
    std::vector<int> evens_traditional;
    std::copy_if(numbers.begin(), numbers.end(), std::back_inserter(evens_traditional),
                 [](int n) { return n % 2 == 0; });

    std::vector<int> doubled_traditional;
    std::transform(evens_traditional.begin(), evens_traditional.end(),
                   std::back_inserter(doubled_traditional),
                   [](int n) { return n * 2; });

    std::cout << "Evens doubled (traditional): ";
    for (int n : doubled_traditional) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // C++20 Ranges approach - composable and lazy
    std::cout << "\nC++20 Ranges approach:\n";
    auto evens_doubled = numbers
                        | std::views::filter([](int n) { return n % 2 == 0; })
                        | std::views::transform([](int n) { return n * 2; });

    std::cout << "Evens doubled (ranges): ";
    for (int n : evens_doubled) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // More complex chaining
    auto complex_pipeline = numbers
                           | std::views::filter([](int n) { return n > 3; })
                           | std::views::transform([](int n) { return n * n; })
                           | std::views::take(4);

    std::cout << "Numbers > 3, squared, take 4: ";
    for (int n : complex_pipeline) {
        std::cout << n << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 2. RANGES VIEWS AND ADAPTORS
// ============================================================================

void demonstrate_ranges_views() {
    std::cout << "\n=== Ranges Views and Adaptors ===\n";

    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    // take - first N elements
    std::cout << "Take first 5: ";
    for (int n : data | std::views::take(5)) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // drop - skip first N elements
    std::cout << "Drop first 5: ";
    for (int n : data | std::views::drop(5)) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // take_while - take elements while condition is true
    std::cout << "Take while < 6: ";
    for (int n : data | std::views::take_while([](int x) { return x < 6; })) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // drop_while - skip elements while condition is true
    std::cout << "Drop while < 6: ";
    for (int n : data | std::views::drop_while([](int x) { return x < 6; })) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // reverse - reverse the sequence
    std::cout << "Reversed: ";
    for (int n : data | std::views::reverse) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // split - split range by delimiter
    std::string text = "apple,banana,cherry,date";
    std::cout << "Split by comma: ";
    for (auto word : text | std::views::split(',')) {
        std::string str(word.begin(), word.end());
        std::cout << "[" << str << "] ";
    }
    std::cout << "\n";

    // join - flatten nested ranges
    std::vector<std::vector<int>> nested = {{1, 2}, {3, 4, 5}, {6}};
    std::cout << "Joined nested vectors: ";
    for (int n : nested | std::views::join) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // enumerate - get index-value pairs
    std::vector<std::string> fruits = {"apple", "banana", "cherry"};
    std::cout << "Enumerated: ";
    for (auto [index, fruit] : fruits | std::views::enumerate) {
        std::cout << index << ":" << fruit << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 3. FINANCIAL DATA PROCESSING WITH RANGES
// ============================================================================

struct Trade {
    std::string symbol;
    double price;
    int quantity;
    std::chrono::system_clock::time_point timestamp;

    Trade(std::string sym, double p, int q)
        : symbol(std::move(sym)), price(p), quantity(q),
          timestamp(std::chrono::system_clock::now()) {}
};

struct MarketData {
    std::string symbol;
    double bid;
    double ask;
    int volume;

    MarketData(std::string sym, double b, double a, int v)
        : symbol(std::move(sym)), bid(b), ask(a), volume(v) {}

    double spread() const { return ask - bid; }
    double mid_price() const { return (bid + ask) / 2.0; }
};

void demonstrate_financial_ranges() {
    std::cout << "\n=== Financial Data Processing with Ranges ===\n";

    // Sample trade data
    std::vector<Trade> trades = {
        Trade("AAPL", 150.25, 1000),
        Trade("GOOGL", 2800.50, 200),
        Trade("AAPL", 150.30, 500),
        Trade("MSFT", 300.75, 800),
        Trade("AAPL", 150.20, 1200),
        Trade("TSLA", 800.00, 300),
        Trade("GOOGL", 2805.25, 150),
        Trade("MSFT", 301.00, 600)
    };

    // Filter AAPL trades and calculate total volume
    auto aapl_trades = trades
                      | std::views::filter([](const Trade& t) { return t.symbol == "AAPL"; });

    int aapl_total_volume = 0;
    double aapl_total_value = 0.0;

    for (const auto& trade : aapl_trades) {
        aapl_total_volume += trade.quantity;
        aapl_total_value += trade.price * trade.quantity;
    }

    double aapl_vwap = aapl_total_value / aapl_total_volume;
    std::cout << "AAPL VWAP: $" << std::fixed << std::setprecision(2) << aapl_vwap << "\n";
    std::cout << "AAPL Total Volume: " << aapl_total_volume << "\n";

    // Find high-value trades (> $100,000)
    auto high_value_trades = trades
                            | std::views::filter([](const Trade& t) {
                                return t.price * t.quantity > 100000.0;
                            })
                            | std::views::transform([](const Trade& t) {
                                return std::make_tuple(t.symbol, t.price, t.quantity,
                                                     t.price * t.quantity);
                            });

    std::cout << "\nHigh-value trades (>$100k):\n";
    for (const auto& [symbol, price, quantity, value] : high_value_trades) {
        std::cout << "  " << symbol << ": " << quantity << " @ $" << price
                  << " = $" << value << "\n";
    }

    // Market data processing
    std::vector<MarketData> market_data = {
        MarketData("AAPL", 150.20, 150.25, 50000),
        MarketData("GOOGL", 2800.00, 2800.50, 10000),
        MarketData("MSFT", 300.70, 300.80, 30000),
        MarketData("TSLA", 799.50, 800.50, 20000),
        MarketData("AMZN", 3200.00, 3201.00, 8000)
    };

    // Find tight spreads (< $0.50)
    auto tight_spreads = market_data
                        | std::views::filter([](const MarketData& md) {
                            return md.spread() < 0.50;
                        })
                        | std::views::transform([](const MarketData& md) {
                            return std::make_tuple(md.symbol, md.spread(), md.mid_price());
                        });

    std::cout << "\nTight spreads (<$0.50):\n";
    for (const auto& [symbol, spread, mid] : tight_spreads) {
        std::cout << "  " << symbol << ": spread $" << spread
                  << ", mid $" << mid << "\n";
    }

    // Top 3 by volume
    auto top_volume = market_data
                     | std::views::transform([](const MarketData& md) {
                         return std::make_pair(md.symbol, md.volume);
                     });

    // Convert to vector for sorting
    std::vector<std::pair<std::string, int>> volume_vec(top_volume.begin(), top_volume.end());
    std::ranges::sort(volume_vec, [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    std::cout << "\nTop 3 by volume:\n";
    for (const auto& [symbol, volume] : volume_vec | std::views::take(3)) {
        std::cout << "  " << symbol << ": " << volume << "\n";
    }
}

// ============================================================================
// 4. LAZY EVALUATION AND PERFORMANCE
// ============================================================================

void demonstrate_lazy_evaluation() {
    std::cout << "\n=== Lazy Evaluation and Performance ===\n";

    // Large dataset
    std::vector<int> large_data;
    for (int i = 1; i <= 1000000; ++i) {
        large_data.push_back(i);
    }

    std::cout << "Working with " << large_data.size() << " elements\n";

    // Lazy pipeline - no intermediate containers created
    auto start_time = std::chrono::high_resolution_clock::now();

    auto lazy_result = large_data
                      | std::views::filter([](int n) { return n % 1000 == 0; })
                      | std::views::transform([](int n) { return n * n; })
                      | std::views::take(5);

    std::cout << "First 5 multiples of 1000, squared: ";
    for (int n : lazy_result) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "Lazy evaluation time: " << duration.count() << " microseconds\n";

    // Traditional approach with intermediate containers
    start_time = std::chrono::high_resolution_clock::now();

    std::vector<int> filtered;
    std::copy_if(large_data.begin(), large_data.end(), std::back_inserter(filtered),
                 [](int n) { return n % 1000 == 0; });

    std::vector<int> transformed;
    std::transform(filtered.begin(), filtered.end(), std::back_inserter(transformed),
                   [](int n) { return n * n; });

    std::vector<int> taken;
    auto take_end = std::next(transformed.begin(), std::min(5, (int)transformed.size()));
    std::copy(transformed.begin(), take_end, std::back_inserter(taken));

    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "Traditional approach time: " << duration.count() << " microseconds\n";

    std::cout << "Traditional result: ";
    for (int n : taken) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // Infinite ranges with iota
    std::cout << "\nInfinite range example (first 10 squares of odd numbers):\n";
    auto infinite_odds_squared = std::views::iota(1)
                                 | std::views::filter([](int n) { return n % 2 == 1; })
                                 | std::views::transform([](int n) { return n * n; })
                                 | std::views::take(10);

    for (int n : infinite_odds_squared) {
        std::cout << n << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 5. RANGES ALGORITHMS
// ============================================================================

void demonstrate_ranges_algorithms() {
    std::cout << "\n=== Ranges Algorithms ===\n";

    std::vector<int> numbers = {64, 34, 25, 12, 22, 11, 90, 5, 77, 30};
    std::vector<std::string> words = {"banana", "apple", "cherry", "date", "elderberry"};

    // Sorting with ranges
    std::cout << "Original numbers: ";
    for (int n : numbers) std::cout << n << " ";
    std::cout << "\n";

    auto numbers_copy = numbers;
    std::ranges::sort(numbers_copy);
    std::cout << "Sorted: ";
    for (int n : numbers_copy) std::cout << n << " ";
    std::cout << "\n";

    // Partial sort
    numbers_copy = numbers;
    std::ranges::partial_sort(numbers_copy, numbers_copy.begin() + 5);
    std::cout << "Partial sort (first 5): ";
    for (int n : numbers_copy) std::cout << n << " ";
    std::cout << "\n";

    // Find operations
    auto it = std::ranges::find(numbers, 25);
    if (it != numbers.end()) {
        std::cout << "Found 25 at position " << std::distance(numbers.begin(), it) << "\n";
    }

    auto it2 = std::ranges::find_if(numbers, [](int n) { return n > 50; });
    if (it2 != numbers.end()) {
        std::cout << "First number > 50: " << *it2 << "\n";
    }

    // Count operations
    int count_even = std::ranges::count_if(numbers, [](int n) { return n % 2 == 0; });
    std::cout << "Count of even numbers: " << count_even << "\n";

    // Min/Max operations
    auto [min_it, max_it] = std::ranges::minmax_element(numbers);
    std::cout << "Min: " << *min_it << ", Max: " << *max_it << "\n";

    // String operations
    std::cout << "\nString operations:\n";
    std::cout << "Original words: ";
    for (const auto& word : words) std::cout << word << " ";
    std::cout << "\n";

    auto words_copy = words;
    std::ranges::sort(words_copy);
    std::cout << "Sorted words: ";
    for (const auto& word : words_copy) std::cout << word << " ";
    std::cout << "\n";

    // Sort by length
    words_copy = words;
    std::ranges::sort(words_copy, {}, [](const std::string& s) { return s.length(); });
    std::cout << "Sorted by length: ";
    for (const auto& word : words_copy) std::cout << word << " ";
    std::cout << "\n";

    // Binary search
    std::ranges::sort(numbers_copy);
    bool found = std::ranges::binary_search(numbers_copy, 25);
    std::cout << "Binary search for 25: " << std::boolalpha << found << "\n";

    // Transform and copy
    std::vector<int> squares;
    std::ranges::transform(numbers, std::back_inserter(squares),
                          [](int n) { return n * n; });
    std::cout << "Squares: ";
    for (int n : squares) std::cout << n << " ";
    std::cout << "\n";
}

// ============================================================================
// 6. CUSTOM RANGES AND RANGE ADAPTORS
// ============================================================================

// Custom range for generating Fibonacci numbers
class FibonacciRange {
private:
    class Iterator {
    private:
        long long a_, b_;
        size_t count_;
        size_t max_count_;

    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = long long;
        using difference_type = std::ptrdiff_t;
        using pointer = const long long*;
        using reference = long long;

        Iterator(size_t max_count, bool is_end = false)
            : a_(0), b_(1), count_(0), max_count_(max_count) {
            if (is_end) count_ = max_count;
        }

        long long operator*() const { return a_; }

        Iterator& operator++() {
            if (count_ < max_count_) {
                long long temp = a_;
                a_ = b_;
                b_ += temp;
                ++count_;
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const Iterator& other) const {
            return count_ == other.count_;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    size_t count_;

public:
    explicit FibonacciRange(size_t count) : count_(count) {}

    Iterator begin() const { return Iterator(count_); }
    Iterator end() const { return Iterator(count_, true); }
};

// Custom range adaptor for squaring values
namespace custom_views {
    struct SquareAdaptor : std::ranges::range_adaptor_closure<SquareAdaptor> {
        template<std::ranges::viewable_range Range>
        constexpr auto operator()(Range&& range) const {
            return std::forward<Range>(range)
                   | std::views::transform([](auto&& x) { return x * x; });
        }
    };

    inline constexpr SquareAdaptor square{};
}

void demonstrate_custom_ranges() {
    std::cout << "\n=== Custom Ranges and Adaptors ===\n";

    // Fibonacci range
    std::cout << "First 10 Fibonacci numbers: ";
    FibonacciRange fib(10);
    for (long long n : fib) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // Using Fibonacci range with standard algorithms
    auto fib_sum = std::ranges::fold_left(FibonacciRange(8), 0LL, std::plus<>{});
    std::cout << "Sum of first 8 Fibonacci numbers: " << fib_sum << "\n";

    // Custom square adaptor
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::cout << "Original: ";
    for (int n : numbers) std::cout << n << " ";
    std::cout << "\n";

    std::cout << "Squared using custom adaptor: ";
    for (int n : numbers | custom_views::square) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // Chaining with custom adaptor
    std::cout << "Evens squared: ";
    for (int n : numbers
                 | std::views::filter([](int x) { return x % 2 == 0; })
                 | custom_views::square) {
        std::cout << n << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 7. RANGES WITH CONTAINERS AND ALGORITHMS
// ============================================================================

void demonstrate_ranges_with_containers() {
    std::cout << "\n=== Ranges with Different Containers ===\n";

    // Different container types
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::list<int> lst = {6, 7, 8, 9, 10};
    std::deque<int> deq = {11, 12, 13, 14, 15};
    std::array<int, 5> arr = {16, 17, 18, 19, 20};

    // Ranges work seamlessly with all container types
    auto process_container = [](auto&& container, const std::string& name) {
        std::cout << name << " doubled: ";
        for (int n : container | std::views::transform([](int x) { return x * 2; })) {
            std::cout << n << " ";
        }
        std::cout << "\n";
    };

    process_container(vec, "Vector");
    process_container(lst, "List");
    process_container(deq, "Deque");
    process_container(arr, "Array");

    // Combining different containers
    std::cout << "\nCombining containers:\n";
    auto combined = std::views::concat(vec, lst, deq);
    std::cout << "Combined (vec + list + deque): ";
    for (int n : combined) {
        std::cout << n << " ";
    }
    std::cout << "\n";

    // Map operations
    std::map<std::string, int> stock_prices = {
        {"AAPL", 150}, {"GOOGL", 2800}, {"MSFT", 300}, {"TSLA", 800}
    };

    std::cout << "\nStock prices > $500:\n";
    for (const auto& [symbol, price] : stock_prices
                                      | std::views::filter([](const auto& pair) {
                                          return pair.second > 500;
                                      })) {
        std::cout << "  " << symbol << ": $" << price << "\n";
    }

    // Keys and values views
    std::cout << "Symbols: ";
    for (const auto& symbol : stock_prices | std::views::keys) {
        std::cout << symbol << " ";
    }
    std::cout << "\n";

    std::cout << "Prices: ";
    for (int price : stock_prices | std::views::values) {
        std::cout << "$" << price << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// 8. REAL-WORLD EXAMPLE: PORTFOLIO ANALYSIS
// ============================================================================

struct Position {
    std::string symbol;
    int shares;
    double cost_basis;
    double current_price;

    Position(std::string sym, int sh, double cost, double current)
        : symbol(std::move(sym)), shares(sh), cost_basis(cost), current_price(current) {}

    double market_value() const { return shares * current_price; }
    double unrealized_pnl() const { return shares * (current_price - cost_basis); }
    double unrealized_pnl_percent() const {
        return (current_price - cost_basis) / cost_basis * 100.0;
    }
};

void demonstrate_portfolio_analysis() {
    std::cout << "\n=== Real-World Example: Portfolio Analysis ===\n";

    std::vector<Position> portfolio = {
        Position("AAPL", 100, 140.00, 150.25),
        Position("GOOGL", 50, 2600.00, 2800.50),
        Position("MSFT", 75, 280.00, 300.75),
        Position("TSLA", 25, 850.00, 800.00),
        Position("AMZN", 30, 3100.00, 3200.00),
        Position("META", 40, 320.00, 280.00),
        Position("NVDA", 20, 200.00, 450.00)
    };

    // Total portfolio value
    double total_value = std::ranges::fold_left(
        portfolio | std::views::transform(&Position::market_value),
        0.0, std::plus<>{}
    );
    std::cout << "Total portfolio value: $" << std::fixed << std::setprecision(2)
              << total_value << "\n";

    // Total unrealized P&L
    double total_pnl = std::ranges::fold_left(
        portfolio | std::views::transform(&Position::unrealized_pnl),
        0.0, std::plus<>{}
    );
    std::cout << "Total unrealized P&L: $" << total_pnl << "\n";

    // Winners (positive P&L)
    std::cout << "\nWinning positions:\n";
    for (const auto& pos : portfolio
                          | std::views::filter([](const Position& p) {
                              return p.unrealized_pnl() > 0;
                          })) {
        std::cout << "  " << pos.symbol << ": $" << pos.unrealized_pnl()
                  << " (" << pos.unrealized_pnl_percent() << "%)\n";
    }

    // Losers (negative P&L)
    std::cout << "\nLosing positions:\n";
    for (const auto& pos : portfolio
                          | std::views::filter([](const Position& p) {
                              return p.unrealized_pnl() < 0;
                          })) {
        std::cout << "  " << pos.symbol << ": $" << pos.unrealized_pnl()
                  << " (" << pos.unrealized_pnl_percent() << "%)\n";
    }

    // Top 3 positions by value
    auto positions_by_value = portfolio;
    std::ranges::sort(positions_by_value, [](const Position& a, const Position& b) {
        return a.market_value() > b.market_value();
    });

    std::cout << "\nTop 3 positions by value:\n";
    for (const auto& pos : positions_by_value | std::views::take(3)) {
        double weight = pos.market_value() / total_value * 100.0;
        std::cout << "  " << pos.symbol << ": $" << pos.market_value()
                  << " (" << weight << "% of portfolio)\n";
    }

    // Positions with > 10% gains
    std::cout << "\nPositions with >10% gains:\n";
    for (const auto& pos : portfolio
                          | std::views::filter([](const Position& p) {
                              return p.unrealized_pnl_percent() > 10.0;
                          })) {
        std::cout << "  " << pos.symbol << ": " << pos.unrealized_pnl_percent() << "%\n";
    }

    // Risk analysis - positions with large losses
    double portfolio_at_risk = std::ranges::fold_left(
        portfolio
        | std::views::filter([](const Position& p) { return p.unrealized_pnl_percent() < -5.0; })
        | std::views::transform(&Position::market_value),
        0.0, std::plus<>{}
    );

    double risk_percentage = portfolio_at_risk / total_value * 100.0;
    std::cout << "\nRisk analysis:\n";
    std::cout << "Value at risk (>5% loss): $" << portfolio_at_risk
              << " (" << risk_percentage << "% of portfolio)\n";
}

// ============================================================================
// 9. RANGES AND PARALLEL ALGORITHMS
// ============================================================================

void demonstrate_ranges_parallel() {
    std::cout << "\n=== Ranges with Parallel Algorithms ===\n";

    // Large dataset for parallel processing
    std::vector<int> large_dataset;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000);

    for (int i = 0; i < 100000; ++i) {
        large_dataset.push_back(dis(gen));
    }

    std::cout << "Processing " << large_dataset.size() << " elements\n";

    // Sequential processing
    auto start = std::chrono::high_resolution_clock::now();

    auto sequential_result = std::ranges::count_if(large_dataset, [](int n) {
        return n > 500;
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto sequential_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Sequential count (>500): " << sequential_result << "\n";
    std::cout << "Sequential time: " << sequential_time.count() << "ms\n";

    // Parallel processing
    start = std::chrono::high_resolution_clock::now();

    auto parallel_result = std::count_if(std::execution::par,
                                        large_dataset.begin(), large_dataset.end(),
                                        [](int n) { return n > 500; });

    end = std::chrono::high_resolution_clock::now();
    auto parallel_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Parallel count (>500): " << parallel_result << "\n";
    std::cout << "Parallel time: " << parallel_time.count() << "ms\n";

    // Ranges with parallel sort
    auto dataset_copy = large_dataset;
    start = std::chrono::high_resolution_clock::now();

    std::ranges::sort(dataset_copy);

    end = std::chrono::high_resolution_clock::now();
    auto ranges_sort_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Ranges sort time: " << ranges_sort_time.count() << "ms\n";

    // Traditional parallel sort
    dataset_copy = large_dataset;
    start = std::chrono::high_resolution_clock::now();

    std::sort(std::execution::par, dataset_copy.begin(), dataset_copy.end());

    end = std::chrono::high_resolution_clock::now();
    auto parallel_sort_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Parallel sort time: " << parallel_sort_time.count() << "ms\n";
}

// ============================================================================
// 10. BEST PRACTICES AND PERFORMANCE TIPS
// ============================================================================

void demonstrate_best_practices() {
    std::cout << "\n=== Best Practices and Performance Tips ===\n";

    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // 1. Prefer ranges algorithms over traditional ones
    std::cout << "1. Use ranges algorithms for better composability:\n";

    // Good: ranges algorithm
    auto max_element = std::ranges::max_element(data);
    std::cout << "   Max element (ranges): " << *max_element << "\n";

    // Less preferred: traditional algorithm
    auto max_traditional = std::max_element(data.begin(), data.end());
    std::cout << "   Max element (traditional): " << *max_traditional << "\n";

    // 2. Chain operations efficiently
    std::cout << "\n2. Chain operations for readability:\n";

    // Good: chained operations
    auto processed = data
                    | std::views::filter([](int n) { return n % 2 == 0; })
                    | std::views::transform([](int n) { return n * n; })
                    | std::views::take(3);

    std::cout << "   Chained result: ";
    for (int n : processed) std::cout << n << " ";
    std::cout << "\n";

    // 3. Use views for lazy evaluation
    std::cout << "\n3. Leverage lazy evaluation:\n";

    // Lazy - no computation until iteration
    auto lazy_view = data
                    | std::views::transform([](int n) {
                        std::cout << "   Processing " << n << "\n";
                        return n * 2;
                    });

    std::cout << "   View created (no processing yet)\n";
    std::cout << "   First element: " << *lazy_view.begin() << "\n";

    // 4. Avoid unnecessary materialization
    std::cout << "\n4. Avoid creating unnecessary intermediate containers:\n";

    // Bad: creates intermediate vector
    std::vector<int> filtered;
    std::ranges::copy_if(data, std::back_inserter(filtered),
                        [](int n) { return n > 5; });
    std::cout << "   Intermediate container size: " << filtered.size() << "\n";

    // Good: use view directly
    auto view_filtered = data | std::views::filter([](int n) { return n > 5; });
    auto count = std::ranges::distance(view_filtered);
    std::cout << "   View count: " << count << "\n";

    // 5. Use appropriate range concepts
    std::cout << "\n5. Understanding range categories:\n";
    std::cout << "   Vector is random_access_range: "
              << std::boolalpha << std::ranges::random_access_range<std::vector<int>> << "\n";
    std::cout << "   List is bidirectional_range: "
              << std::ranges::bidirectional_range<std::list<int>> << "\n";
    std::cout << "   Filter view is input_range: "
              << std::ranges::input_range<decltype(view_filtered)> << "\n";

    // 6. Compose views for complex operations
    std::cout << "\n6. Compose views for complex transformations:\n";

    std::vector<std::string> sentences = {
        "Hello world", "C++ ranges", "are powerful", "and efficient"
    };

    // Complex composition: split sentences into words, filter long words, uppercase
    auto complex_view = sentences
                       | std::views::transform([](const std::string& s) {
                           return s | std::views::split(' ');
                       })
                       | std::views::join
                       | std::views::filter([](auto word_range) {
                           return std::ranges::distance(word_range) > 3;
                       })
                       | std::views::transform([](auto word_range) {
                           std::string word(word_range.begin(), word_range.end());
                           std::ranges::transform(word, word.begin(), ::toupper);
                           return word;
                       });

    std::cout << "   Long words (>3 chars) in uppercase: ";
    for (const auto& word : complex_view) {
        std::cout << word << " ";
    }
    std::cout << "\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 Ranges Use Cases and Examples\n";
    std::cout << "===================================\n";

    demonstrate_basic_ranges();
    demonstrate_ranges_views();
    demonstrate_financial_ranges();
    demonstrate_lazy_evaluation();
    demonstrate_ranges_algorithms();
    demonstrate_custom_ranges();
    demonstrate_ranges_with_containers();
    demonstrate_portfolio_analysis();
    demonstrate_ranges_parallel();
    demonstrate_best_practices();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Ranges provide composable, lazy-evaluated data processing\n";
    std::cout << "2. Views are lightweight adaptors that don't own data\n";
    std::cout << "3. Pipe operator (|) enables functional-style chaining\n";
    std::cout << "4. Lazy evaluation improves performance by avoiding intermediate containers\n";
    std::cout << "5. Ranges algorithms are more expressive than traditional STL algorithms\n";
    std::cout << "6. Custom ranges and adaptors extend functionality\n";
    std::cout << "7. Excellent for financial data processing and analysis\n";
    std::cout << "8. Seamless integration with existing containers\n";
    std::cout << "9. Better readability and maintainability than iterator-based code\n";
    std::cout << "10. Performance benefits through lazy evaluation and composition\n";
    std::cout << "11. Type safety through concepts and strong typing\n";
    std::cout << "12. Infinite ranges possible with lazy evaluation\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler with ranges support
 * - GCC 12+, Clang 15+, or MSVC 2019+ (latest versions)
 * - Use -std=c++20 flag
 *
 * Example compilation:
 * g++ -std=c++20 -Wall -Wextra -O2 cpp20_ranges_use_cases_examples.cpp -o ranges_demo
 *
 * Key Ranges Components:
 * 1. Views (std::views::*) - Lightweight range adaptors
 * 2. Range algorithms (std::ranges::*) - Algorithm overloads for ranges
 * 3. Range concepts - Type requirements for ranges
 * 4. Pipe operator (|) - Functional composition
 * 5. Range adaptors - Transform ranges into other ranges
 * 6. Range factories - Create ranges from scratch
 *
 * Common Views:
 * - filter, transform, take, drop, reverse
 * - take_while, drop_while, split, join
 * - enumerate, keys, values, elements
 * - iota (infinite sequences)
 *
 * Benefits over Traditional STL:
 * - More readable functional-style code
 * - Lazy evaluation for better performance
 * - Composable operations
 * - No intermediate containers needed
 * - Better error messages with concepts
 * - Easier to reason about data transformations
 */
