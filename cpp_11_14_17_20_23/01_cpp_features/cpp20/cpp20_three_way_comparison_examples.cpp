/*
 * C++20 Three-Way Comparison Operator (<=> "Spaceship Operator") Use Cases and Examples
 *
 * The three-way comparison operator provides a unified way to define all comparison
 * operations with a single operator, making code more maintainable and less error-prone.
 *
 * Key Benefits:
 * 1. Single operator defines all six comparison operations (==, !=, <, <=, >, >=)
 * 2. Automatic generation of comparison operators
 * 3. Consistent and efficient comparisons
 * 4. Better compiler optimizations
 * 5. Support for partial ordering (useful for floating-point, complex numbers)
 * 6. Simplified implementation of comparable types
 */

#include <iostream>
#include <compare>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <variant>
#include <memory>
#include <algorithm>
#include <numeric>
#include <cmath>

// ============================================================================
// 1. BASIC THREE-WAY COMPARISON
// ============================================================================

struct Point {
    int x, y;

    // C++20: Single spaceship operator replaces all 6 comparison operators
    auto operator<=>(const Point& other) const = default;

    // Still need to explicitly define == for most cases
    bool operator==(const Point& other) const = default;
};

// Before C++20 (tedious and error-prone):
struct PointOld {
    int x, y;

    bool operator==(const PointOld& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const PointOld& other) const {
        return !(*this == other);
    }

    bool operator<(const PointOld& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }

    bool operator<=(const PointOld& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const PointOld& other) const {
        return !(*this <= other);
    }

    bool operator>=(const PointOld& other) const {
        return !(*this < other);
    }
};

void demonstrate_basic_three_way_comparison() {
    std::cout << "\n=== Basic Three-Way Comparison ===\n";

    Point p1{1, 2};
    Point p2{1, 3};
    Point p3{1, 2};

    std::cout << "Point p1{1, 2}, p2{1, 3}, p3{1, 2}\n";

    // All six comparison operators work automatically
    std::cout << "p1 == p3: " << std::boolalpha << (p1 == p3) << "\n";
    std::cout << "p1 != p2: " << (p1 != p2) << "\n";
    std::cout << "p1 < p2: " << (p1 < p2) << "\n";
    std::cout << "p1 <= p2: " << (p1 <= p2) << "\n";
    std::cout << "p2 > p1: " << (p2 > p1) << "\n";
    std::cout << "p2 >= p1: " << (p2 >= p1) << "\n";

    // Direct use of spaceship operator
    auto result = p1 <=> p2;
    if (result < 0) {
        std::cout << "p1 is less than p2\n";
    } else if (result > 0) {
        std::cout << "p1 is greater than p2\n";
    } else {
        std::cout << "p1 is equal to p2\n";
    }
}

// ============================================================================
// 2. ORDERING CATEGORIES
// ============================================================================

// Strong ordering: a < b, a == b, or a > b (exactly one is true)
struct StrongOrderExample {
    int value;

    std::strong_ordering operator<=>(const StrongOrderExample& other) const {
        return value <=> other.value;
    }

    bool operator==(const StrongOrderExample& other) const = default;
};

// Weak ordering: equivalent but not identical values
struct WeakOrderExample {
    std::string name;
    int priority;

    std::weak_ordering operator<=>(const WeakOrderExample& other) const {
        // Compare by priority first, ignore name for ordering
        auto result = priority <=> other.priority;
        if (result != 0) return result;

        // Same priority: weakly equivalent (regardless of name)
        return std::weak_ordering::equivalent;
    }

    bool operator==(const WeakOrderExample& other) const {
        // For equality, both priority and name must match
        return priority == other.priority && name == other.name;
    }
};

// Partial ordering: some values may be incomparable (like NaN in floating-point)
struct PartialOrderExample {
    double value;

    std::partial_ordering operator<=>(const PartialOrderExample& other) const {
        // Uses floating-point comparison which handles NaN correctly
        return value <=> other.value;
    }

    bool operator==(const PartialOrderExample& other) const {
        // NaN != NaN, so this handles that case
        return value == other.value;
    }
};

void demonstrate_ordering_categories() {
    std::cout << "\n=== Ordering Categories ===\n";

    // Strong ordering
    std::cout << "Strong Ordering:\n";
    StrongOrderExample s1{10}, s2{20}, s3{10};
    std::cout << "s1{10} < s2{20}: " << (s1 < s2) << "\n";
    std::cout << "s1{10} == s3{10}: " << (s1 == s3) << "\n";

    // Weak ordering
    std::cout << "\nWeak Ordering (by priority only):\n";
    WeakOrderExample w1{"Alice", 1}, w2{"Bob", 1}, w3{"Charlie", 2};
    std::cout << "w1{Alice,1} == w2{Bob,1}: " << (w1 == w2) << " (false - names differ)\n";
    std::cout << "w1{Alice,1} < w3{Charlie,2}: " << (w1 < w3) << " (true - priority differs)\n";

    // Check if they're equivalent for ordering (same priority)
    auto weak_result = w1 <=> w2;
    std::cout << "w1 <=> w2 equivalent: " << (weak_result == 0) << "\n";

    // Partial ordering
    std::cout << "\nPartial Ordering (with NaN):\n";
    PartialOrderExample p1{1.0}, p2{2.0}, p3{std::numeric_limits<double>::quiet_NaN()};
    std::cout << "p1{1.0} < p2{2.0}: " << (p1 < p2) << "\n";
    std::cout << "p1{1.0} == p3{NaN}: " << (p1 == p3) << "\n";

    // NaN comparisons
    auto partial_result = p1 <=> p3;
    std::cout << "p1 <=> p3 (NaN) is unordered: "
              << (partial_result == std::partial_ordering::unordered) << "\n";
}

// ============================================================================
// 3. CUSTOM ORDERING LOGIC
// ============================================================================

struct Person {
    std::string first_name;
    std::string last_name;
    int age;

    // Custom ordering: by last name, then first name, then age
    std::strong_ordering operator<=>(const Person& other) const {
        if (auto result = last_name <=> other.last_name; result != 0) {
            return result;
        }
        if (auto result = first_name <=> other.first_name; result != 0) {
            return result;
        }
        return age <=> other.age;
    }

    bool operator==(const Person& other) const = default;
};

// Financial instrument with complex ordering
struct Bond {
    std::string issuer;
    double yield;
    int maturity_years;
    double credit_rating; // Higher number = better rating

    // Order by: credit rating (desc), then yield (desc), then maturity (asc)
    std::strong_ordering operator<=>(const Bond& other) const {
        // Credit rating: higher is better (reverse order)
        if (auto result = other.credit_rating <=> credit_rating; result != 0) {
            return result;
        }

        // Yield: higher is better (reverse order)
        if (auto result = other.yield <=> yield; result != 0) {
            return result;
        }

        // Maturity: shorter is better (normal order)
        return maturity_years <=> other.maturity_years;
    }

    bool operator==(const Bond& other) const = default;
};

void demonstrate_custom_ordering() {
    std::cout << "\n=== Custom Ordering Logic ===\n";

    // Person ordering
    std::vector<Person> people = {
        {"John", "Smith", 30},
        {"Jane", "Smith", 25},
        {"Bob", "Jones", 35},
        {"Alice", "Smith", 28}
    };

    std::sort(people.begin(), people.end());

    std::cout << "People sorted by last name, first name, age:\n";
    for (const auto& person : people) {
        std::cout << person.last_name << ", " << person.first_name
                  << " (age " << person.age << ")\n";
    }

    // Bond ordering
    std::vector<Bond> bonds = {
        {"Government", 2.5, 10, 9.5},  // High rating, low yield
        {"Corporate", 4.0, 5, 7.0},    // Medium rating, medium yield
        {"Junk", 8.0, 3, 3.0},         // Low rating, high yield
        {"Government", 2.8, 20, 9.5}   // High rating, low yield, long maturity
    };

    std::sort(bonds.begin(), bonds.end());

    std::cout << "\nBonds sorted by rating (desc), yield (desc), maturity (asc):\n";
    for (const auto& bond : bonds) {
        std::cout << bond.issuer << " - Rating: " << bond.credit_rating
                  << ", Yield: " << bond.yield << "%, Maturity: "
                  << bond.maturity_years << " years\n";
    }
}

// ============================================================================
// 4. MIXED-TYPE COMPARISONS
// ============================================================================

struct Temperature {
    double celsius;

    Temperature(double c) : celsius(c) {}

    // Compare with other Temperature objects
    std::strong_ordering operator<=>(const Temperature& other) const {
        return celsius <=> other.celsius;
    }

    // Compare with raw double (Fahrenheit)
    std::strong_ordering operator<=>(double fahrenheit) const {
        double other_celsius = (fahrenheit - 32.0) * 5.0 / 9.0;
        return celsius <=> other_celsius;
    }

    bool operator==(const Temperature& other) const = default;
    bool operator==(double fahrenheit) const {
        double other_celsius = (fahrenheit - 32.0) * 5.0 / 9.0;
        return std::abs(celsius - other_celsius) < 0.001; // floating-point tolerance
    }
};

// Free function for reverse comparison (double with Temperature)
std::strong_ordering operator<=>(double fahrenheit, const Temperature& temp) {
    return 0 <=> (temp <=> fahrenheit); // Reverse the comparison
}

bool operator==(double fahrenheit, const Temperature& temp) {
    return temp == fahrenheit;
}

void demonstrate_mixed_type_comparisons() {
    std::cout << "\n=== Mixed-Type Comparisons ===\n";

    Temperature room_temp(20.0);      // 20°C
    Temperature body_temp(37.0);      // 37°C

    double fahrenheit_room = 68.0;    // 68°F ≈ 20°C
    double fahrenheit_hot = 100.0;    // 100°F ≈ 37.8°C

    std::cout << "Room temperature: 20°C vs 68°F\n";
    std::cout << "20°C == 68°F: " << (room_temp == fahrenheit_room) << "\n";
    std::cout << "68°F == 20°C: " << (fahrenheit_room == room_temp) << "\n";

    std::cout << "\nBody temperature: 37°C vs 100°F\n";
    std::cout << "37°C < 100°F: " << (body_temp < fahrenheit_hot) << "\n";
    std::cout << "100°F > 37°C: " << (fahrenheit_hot > body_temp) << "\n";
}

// ============================================================================
// 5. CONTAINER AND STANDARD LIBRARY INTEGRATION
// ============================================================================

struct StockPrice {
    std::string symbol;
    double price;
    int volume;

    auto operator<=>(const StockPrice& other) const = default;
};

struct Portfolio {
    std::vector<StockPrice> stocks;

    // Lexicographical comparison of portfolios
    auto operator<=>(const Portfolio& other) const = default;
};

void demonstrate_container_integration() {
    std::cout << "\n=== Container and Standard Library Integration ===\n";

    std::vector<StockPrice> prices = {
        {"AAPL", 150.0, 1000},
        {"GOOGL", 2800.0, 500},
        {"MSFT", 300.0, 800},
        {"AAPL", 149.5, 1200},  // Same symbol, different price
    };

    // Sort by symbol, then price, then volume (all automatic with <=>)
    std::sort(prices.begin(), prices.end());

    std::cout << "Stock prices sorted:\n";
    for (const auto& stock : prices) {
        std::cout << stock.symbol << ": $" << stock.price
                  << " (vol: " << stock.volume << ")\n";
    }

    // Binary search works automatically
    StockPrice target{"MSFT", 300.0, 800};
    auto it = std::lower_bound(prices.begin(), prices.end(), target);

    if (it != prices.end() && *it == target) {
        std::cout << "\nFound exact match for " << target.symbol << "\n";
    }

    // Portfolio comparison
    Portfolio p1{prices};
    Portfolio p2{prices};
    p2.stocks.push_back({"TSLA", 800.0, 600});

    std::cout << "\nPortfolio comparison:\n";
    std::cout << "p1 == p2: " << (p1 == p2) << "\n";
    std::cout << "p1 < p2: " << (p1 < p2) << " (lexicographical)\n";
}

// ============================================================================
// 6. OPTIONAL AND VARIANT COMPARISONS
// ============================================================================

struct OptionalData {
    std::optional<int> value;

    auto operator<=>(const OptionalData& other) const = default;
};

using NumberVariant = std::variant<int, double, std::string>;

struct VariantContainer {
    NumberVariant data;

    // Custom comparison for variants
    std::strong_ordering operator<=>(const VariantContainer& other) const {
        // First compare by variant index (type)
        if (auto result = data.index() <=> other.data.index(); result != 0) {
            return result;
        }

        // Same type, compare values
        return std::visit([&other](const auto& value) -> std::strong_ordering {
            using T = std::decay_t<decltype(value)>;
            const T& other_value = std::get<T>(other.data);
            return value <=> other_value;
        }, data);
    }

    bool operator==(const VariantContainer& other) const {
        return data == other.data;
    }
};

void demonstrate_optional_variant_comparisons() {
    std::cout << "\n=== Optional and Variant Comparisons ===\n";

    // Optional comparisons
    std::vector<OptionalData> optional_data = {
        {std::nullopt},
        {42},
        {std::nullopt},
        {10},
        {42}
    };

    std::sort(optional_data.begin(), optional_data.end());

    std::cout << "Optional data sorted (nullopt < any value):\n";
    for (const auto& data : optional_data) {
        if (data.value) {
            std::cout << "Value: " << *data.value << "\n";
        } else {
            std::cout << "Value: nullopt\n";
        }
    }

    // Variant comparisons
    std::vector<VariantContainer> variant_data = {
        {std::string("hello")},
        {42},
        {3.14},
        {std::string("world")},
        {10}
    };

    std::sort(variant_data.begin(), variant_data.end());

    std::cout << "\nVariant data sorted (by type index, then by value):\n";
    for (const auto& data : variant_data) {
        std::visit([](const auto& value) {
            std::cout << "Value: " << value << " (type index: "
                      << typeid(value).name() << ")\n";
        }, data.data);
    }
}

// ============================================================================
// 7. PERFORMANCE CONSIDERATIONS
// ============================================================================

struct FastComparison {
    int primary;
    int secondary;
    std::string data; // Expensive to compare

    // Optimized comparison: check cheap fields first
    std::strong_ordering operator<=>(const FastComparison& other) const {
        // Compare cheap fields first
        if (auto result = primary <=> other.primary; result != 0) {
            return result;
        }
        if (auto result = secondary <=> other.secondary; result != 0) {
            return result;
        }
        // Only compare expensive field if necessary
        return data <=> other.data;
    }

    bool operator==(const FastComparison& other) const = default;
};

// Avoiding unnecessary comparisons
struct LazyComparison {
    int key;
    mutable std::optional<std::string> cached_expensive_value;

    const std::string& get_expensive_value() const {
        if (!cached_expensive_value) {
            // Simulate expensive computation
            cached_expensive_value = "computed_" + std::to_string(key * key);
        }
        return *cached_expensive_value;
    }

    std::strong_ordering operator<=>(const LazyComparison& other) const {
        // Compare key first
        if (auto result = key <=> other.key; result != 0) {
            return result;
        }
        // Only compute expensive value if keys are equal
        return get_expensive_value() <=> other.get_expensive_value();
    }

    bool operator==(const LazyComparison& other) const {
        if (key != other.key) return false;
        return get_expensive_value() == other.get_expensive_value();
    }
};

void demonstrate_performance_considerations() {
    std::cout << "\n=== Performance Considerations ===\n";

    std::vector<FastComparison> fast_data = {
        {1, 100, "expensive_string_z"},
        {1, 50, "expensive_string_a"},
        {2, 25, "expensive_string_m"},
        {1, 75, "expensive_string_x"}
    };

    std::cout << "Fast comparison (cheap fields first):\n";
    std::sort(fast_data.begin(), fast_data.end());

    for (const auto& item : fast_data) {
        std::cout << "(" << item.primary << ", " << item.secondary
                  << ", " << item.data << ")\n";
    }

    std::vector<LazyComparison> lazy_data = {
        {5}, {2}, {5}, {1}, {3}
    };

    std::cout << "\nLazy comparison (expensive computation only when needed):\n";
    std::sort(lazy_data.begin(), lazy_data.end());

    for (const auto& item : lazy_data) {
        std::cout << "Key: " << item.key
                  << ", Expensive: " << item.get_expensive_value() << "\n";
    }
}

// ============================================================================
// 8. FINANCIAL TRADING APPLICATIONS
// ============================================================================

struct Order {
    enum Type { BUY, SELL };

    Type type;
    std::string symbol;
    double price;
    int quantity;
    std::chrono::system_clock::time_point timestamp;

    // Priority ordering for order book:
    // 1. BUY orders: higher price first (price DESC)
    // 2. SELL orders: lower price first (price ASC)
    // 3. Same price: earlier timestamp first (time ASC)
    std::strong_ordering operator<=>(const Order& other) const {
        // Different symbols are incomparable in this context
        if (symbol != other.symbol) {
            return symbol <=> other.symbol;
        }

        // Different order types
        if (type != other.type) {
            return type <=> other.type;
        }

        // Same type: apply type-specific price ordering
        if (type == BUY) {
            // BUY orders: higher price has priority (reverse price order)
            if (auto result = other.price <=> price; result != 0) {
                return result;
            }
        } else { // SELL
            // SELL orders: lower price has priority (normal price order)
            if (auto result = price <=> other.price; result != 0) {
                return result;
            }
        }

        // Same price: earlier timestamp has priority
        return timestamp <=> other.timestamp;
    }

    bool operator==(const Order& other) const = default;
};

struct Trade {
    std::string symbol;
    double price;
    int quantity;
    std::chrono::system_clock::time_point timestamp;

    // Natural ordering: by timestamp, then symbol, then price
    auto operator<=>(const Trade& other) const = default;
};

struct MarketDataTick {
    std::string symbol;
    double bid_price;
    double ask_price;
    int bid_volume;
    int ask_volume;
    std::chrono::system_clock::time_point timestamp;

    // Spread calculation for comparison
    double spread() const {
        return ask_price - bid_price;
    }

    // Compare by spread (tighter spreads first), then timestamp
    std::strong_ordering operator<=>(const MarketDataTick& other) const {
        if (symbol != other.symbol) {
            return symbol <=> other.symbol;
        }

        if (auto result = spread() <=> other.spread(); result != 0) {
            return result;
        }

        return timestamp <=> other.timestamp;
    }

    bool operator==(const MarketDataTick& other) const = default;
};

void demonstrate_financial_applications() {
    std::cout << "\n=== Financial Trading Applications ===\n";

    auto now = std::chrono::system_clock::now();
    using namespace std::chrono_literals;

    // Order book ordering
    std::vector<Order> orders = {
        {Order::BUY, "AAPL", 150.25, 100, now - 3s},
        {Order::BUY, "AAPL", 150.30, 200, now - 1s},  // Higher price, later time
        {Order::SELL, "AAPL", 150.35, 150, now - 2s},
        {Order::SELL, "AAPL", 150.32, 100, now},      // Lower price, later time
        {Order::BUY, "AAPL", 150.25, 50, now - 5s},   // Same price, earlier time
    };

    std::sort(orders.begin(), orders.end());

    std::cout << "Orders sorted for order book priority:\n";
    for (const auto& order : orders) {
        std::cout << (order.type == Order::BUY ? "BUY " : "SELL")
                  << " " << order.symbol << " " << order.quantity
                  << "@" << order.price << "\n";
    }

    // Market data comparison
    std::vector<MarketDataTick> market_data = {
        {"AAPL", 150.20, 150.25, 1000, 800, now - 2s},  // Spread: 0.05
        {"AAPL", 150.22, 150.24, 1200, 900, now - 1s},  // Spread: 0.02 (tighter)
        {"AAPL", 150.18, 150.28, 800, 700, now},        // Spread: 0.10
    };

    std::sort(market_data.begin(), market_data.end());

    std::cout << "\nMarket data sorted by spread (tighter first):\n";
    for (const auto& tick : market_data) {
        std::cout << tick.symbol << " Bid: " << tick.bid_price
                  << " Ask: " << tick.ask_price
                  << " Spread: " << tick.spread() << "\n";
    }
}

// ============================================================================
// 9. DEBUGGING AND INTROSPECTION
// ============================================================================

template<typename T>
void print_comparison_result(const T& a, const T& b) {
    auto result = a <=> b;

    std::cout << "Comparison result: ";
    if (result < 0) {
        std::cout << "a < b\n";
    } else if (result > 0) {
        std::cout << "a > b\n";
    } else {
        std::cout << "a == b\n";
    }

    // Show ordering category
    if constexpr (std::is_same_v<decltype(result), std::strong_ordering>) {
        std::cout << "Ordering: strong\n";
    } else if constexpr (std::is_same_v<decltype(result), std::weak_ordering>) {
        std::cout << "Ordering: weak\n";
    } else if constexpr (std::is_same_v<decltype(result), std::partial_ordering>) {
        std::cout << "Ordering: partial\n";
        if (result == std::partial_ordering::unordered) {
            std::cout << "Values are unordered (e.g., involving NaN)\n";
        }
    }
}

struct DebuggableComparison {
    int value;
    std::string name;

    std::strong_ordering operator<=>(const DebuggableComparison& other) const {
        std::cout << "Comparing " << name << "(" << value << ") with "
                  << other.name << "(" << other.value << ")\n";

        return value <=> other.value;
    }

    bool operator==(const DebuggableComparison& other) const {
        std::cout << "Equality check: " << name << " == " << other.name << "\n";
        return value == other.value && name == other.name;
    }
};

void demonstrate_debugging_introspection() {
    std::cout << "\n=== Debugging and Introspection ===\n";

    Point p1{5, 10};
    Point p2{3, 15};

    std::cout << "Point comparison debugging:\n";
    print_comparison_result(p1, p2);

    std::cout << "\nPartial ordering with NaN:\n";
    PartialOrderExample nan1{std::numeric_limits<double>::quiet_NaN()};
    PartialOrderExample regular{5.0};
    print_comparison_result(nan1, regular);

    std::cout << "\nDebuggable comparison trace:\n";
    std::vector<DebuggableComparison> debug_data = {
        {3, "three"},
        {1, "one"},
        {2, "two"}
    };

    std::cout << "Sorting with trace:\n";
    std::sort(debug_data.begin(), debug_data.end());
}

// ============================================================================
// 10. BEST PRACTICES AND COMMON PITFALLS
// ============================================================================

// GOOD: Consistent ordering
struct GoodPractice {
    int primary;
    std::string secondary;

    auto operator<=>(const GoodPractice& other) const = default;
};

// PITFALL: Inconsistent ordering with equality
struct BadPractice {
    double value;

    // BAD: This creates inconsistent behavior
    std::strong_ordering operator<=>(const BadPractice& other) const {
        // Using integer comparison for floating-point
        return static_cast<int>(value) <=> static_cast<int>(other.value);
    }

    bool operator==(const BadPractice& other) const {
        // Using exact floating-point comparison
        return value == other.value;
    }
};

// GOOD: Handling floating-point comparisons properly
struct FloatingPointComparison {
    double value;
    static constexpr double epsilon = 1e-9;

    std::weak_ordering operator<=>(const FloatingPointComparison& other) const {
        if (std::abs(value - other.value) < epsilon) {
            return std::weak_ordering::equivalent;
        }
        return value <=> other.value;
    }

    bool operator==(const FloatingPointComparison& other) const {
        return std::abs(value - other.value) < epsilon;
    }
};

void demonstrate_best_practices() {
    std::cout << "\n=== Best Practices and Common Pitfalls ===\n";

    std::cout << "Good practice - consistent default comparison:\n";
    std::vector<GoodPractice> good_data = {
        {2, "beta"},
        {1, "alpha"},
        {2, "gamma"}
    };

    std::sort(good_data.begin(), good_data.end());
    for (const auto& item : good_data) {
        std::cout << "(" << item.primary << ", " << item.secondary << ")\n";
    }

    std::cout << "\nBad practice - inconsistent ordering/equality:\n";
    BadPractice bad1{2.7};
    BadPractice bad2{2.3};

    std::cout << "bad1{2.7} == bad2{2.3}: " << (bad1 == bad2) << "\n";
    std::cout << "bad1{2.7} <=> bad2{2.3} == 0: " << ((bad1 <=> bad2) == 0) << "\n";
    std::cout << "This violates the rule: (a <=> b) == 0 should imply a == b\n";

    std::cout << "\nGood practice - proper floating-point comparison:\n";
    FloatingPointComparison fp1{1.0000000001};
    FloatingPointComparison fp2{1.0000000002};

    std::cout << "fp1{1.0000000001} == fp2{1.0000000002}: " << (fp1 == fp2) << "\n";
    std::cout << "fp1 <=> fp2 == 0: " << ((fp1 <=> fp2) == 0) << "\n";
    std::cout << "Consistent: both treat as equivalent within epsilon\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 Three-Way Comparison Operator (<=> \"Spaceship\") Examples\n";
    std::cout << "=============================================================\n";

    demonstrate_basic_three_way_comparison();
    demonstrate_ordering_categories();
    demonstrate_custom_ordering();
    demonstrate_mixed_type_comparisons();
    demonstrate_container_integration();
    demonstrate_optional_variant_comparisons();
    demonstrate_performance_considerations();
    demonstrate_financial_applications();
    demonstrate_debugging_introspection();
    demonstrate_best_practices();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Single <=> operator replaces 6 comparison operators\n";
    std::cout << "2. Three ordering categories: strong, weak, partial\n";
    std::cout << "3. Automatic generation with = default for simple cases\n";
    std::cout << "4. Custom ordering logic for complex business rules\n";
    std::cout << "5. Mixed-type comparisons supported\n";
    std::cout << "6. Seamless integration with STL algorithms\n";
    std::cout << "7. Performance benefits from single comparison function\n";
    std::cout << "8. Excellent for financial data ordering (order books, trades)\n";
    std::cout << "9. Proper handling of floating-point and NaN values\n";
    std::cout << "10. Must maintain consistency between <=> and ==\n";
    std::cout << "11. Consider performance when designing comparison logic\n";
    std::cout << "12. Use appropriate ordering category for your use case\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler
 * - GCC 10+, Clang 10+, or MSVC 2019+
 * - Use -std=c++20 flag
 *
 * Example compilation:
 * g++ -std=c++20 -Wall -Wextra cpp20_three_way_comparison_examples.cpp -o spaceship_demo
 *
 * Key Three-Way Comparison Features:
 * 1. operator<=> - The spaceship operator
 * 2. std::strong_ordering - Total ordering (like integers)
 * 3. std::weak_ordering - Equivalence classes (like case-insensitive strings)
 * 4. std::partial_ordering - Some elements incomparable (like floating-point with NaN)
 * 5. Automatic operator generation
 * 6. = default for automatic implementation
 * 7. Mixed-type comparisons
 * 8. STL integration
 *
 * Benefits over Traditional Comparisons:
 * - Reduced code duplication (1 vs 6 operators)
 * - Consistency guaranteed
 * - Better compiler optimizations
 * - Less error-prone
 * - Cleaner interfaces
 * - Better performance in some cases
 */
