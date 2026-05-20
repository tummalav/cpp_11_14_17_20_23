/*
 * C++20 Concepts Use Cases and Examples
 *
 * Concepts provide a way to specify constraints on template parameters,
 * making template code more readable, providing better error messages,
 * and enabling more precise overload resolution.
 *
 * Key Benefits:
 * 1. Better error messages for template instantiation failures
 * 2. Self-documenting template interfaces
 * 3. More precise overload resolution
 * 4. Compile-time constraint checking
 * 5. Cleaner template syntax
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <concepts>
#include <iterator>
#include <ranges>

// ============================================================================
// 1. BASIC CONCEPTS DEFINITION AND USAGE
// ============================================================================

// Simple concept using requires expression
template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

// Using concepts in function templates
template<Integral T>
T add_integers(T a, T b) {
    return a + b;
}

template<FloatingPoint T>
T add_floats(T a, T b) {
    return a + b;
}

// Generic arithmetic function
template<Arithmetic T>
T multiply(T a, T b) {
    return a * b;
}

void demonstrate_basic_concepts() {
    std::cout << "\n=== Basic Concepts Usage ===\n";

    // These work - satisfy the concepts
    std::cout << "add_integers(5, 3) = " << add_integers(5, 3) << "\n";
    std::cout << "add_floats(3.14, 2.86) = " << add_floats(3.14, 2.86) << "\n";
    std::cout << "multiply(4, 5) = " << multiply(4, 5) << "\n";
    std::cout << "multiply(2.5, 3.0) = " << multiply(2.5, 3.0) << "\n";

    // These would cause compilation errors:
    // add_integers(3.14, 2.86);  // Error: float doesn't satisfy Integral
    // add_floats(5, 3);          // Error: int doesn't satisfy FloatingPoint
}

// ============================================================================
// 2. COMPLEX CONCEPTS WITH REQUIRES EXPRESSIONS
// ============================================================================

// Concept checking for specific operations
template<typename T>
concept Printable = requires(T t) {
    std::cout << t;  // Must be printable to std::cout
};

template<typename T>
concept Incrementable = requires(T t) {
    ++t;     // Must support pre-increment
    t++;     // Must support post-increment
};

template<typename T>
concept Comparable = requires(T a, T b) {
    { a == b } -> std::same_as<bool>;  // Must return bool
    { a != b } -> std::same_as<bool>;
    { a < b } -> std::same_as<bool>;
    { a > b } -> std::same_as<bool>;
};

template<typename Container>
concept SequenceContainer = requires(Container c) {
    typename Container::value_type;
    typename Container::iterator;
    { c.begin() } -> std::same_as<typename Container::iterator>;
    { c.end() } -> std::same_as<typename Container::iterator>;
    { c.size() } -> std::convertible_to<std::size_t>;
    { c.empty() } -> std::same_as<bool>;
};

template<Printable T>
void print_value(const T& value) {
    std::cout << "Value: " << value << "\n";
}

template<Incrementable T>
T increment_twice(T value) {
    ++value;
    return value++;
}

template<SequenceContainer Container>
void print_container_info(const Container& container) {
    std::cout << "Container size: " << container.size()
              << ", empty: " << std::boolalpha << container.empty() << "\n";
}

void demonstrate_complex_concepts() {
    std::cout << "\n=== Complex Concepts with Requires Expressions ===\n";

    // Printable concept
    print_value(42);
    print_value(3.14);
    print_value(std::string("Hello"));

    // Incrementable concept
    int x = 5;
    std::cout << "increment_twice(5) = " << increment_twice(x) << "\n";

    // SequenceContainer concept
    std::vector<int> vec = {1, 2, 3, 4, 5};
    print_container_info(vec);

    std::string str = "Hello";
    print_container_info(str);
}

// ============================================================================
// 3. STANDARD LIBRARY CONCEPTS
// ============================================================================

// Using standard library concepts
template<std::ranges::range Range>
void process_range(Range&& r) {
    std::cout << "Processing range with " << std::ranges::size(r) << " elements\n";
    for (const auto& element : r) {
        std::cout << element << " ";
    }
    std::cout << "\n";
}

template<std::input_iterator Iterator>
void advance_iterator(Iterator& it, std::iter_difference_t<Iterator> n) {
    std::advance(it, n);
    std::cout << "Advanced iterator by " << n << " positions\n";
}

template<std::regular T>
class SimpleContainer {
private:
    std::vector<T> data;

public:
    void add(const T& item) {
        data.push_back(item);
    }

    void add(T&& item) {
        data.push_back(std::move(item));
    }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    std::size_t size() const { return data.size(); }
};

void demonstrate_standard_concepts() {
    std::cout << "\n=== Standard Library Concepts ===\n";

    // std::ranges::range concept
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    process_range(numbers);

    std::string text = "Hello";
    process_range(text);

    // std::input_iterator concept
    auto it = numbers.begin();
    advance_iterator(it, 2);
    std::cout << "Iterator now points to: " << *it << "\n";

    // std::regular concept (copyable, movable, default constructible, equality comparable)
    SimpleContainer<int> container;
    container.add(1);
    container.add(2);
    container.add(3);

    std::cout << "SimpleContainer size: " << container.size() << "\n";
}

// ============================================================================
// 4. CONCEPT COMPOSITION AND LOGICAL OPERATIONS
// ============================================================================

template<typename T>
concept SignedIntegral = std::signed_integral<T>;

template<typename T>
concept UnsignedIntegral = std::unsigned_integral<T>;

// Combining concepts with logical operators
template<typename T>
concept NumericType = std::integral<T> || std::floating_point<T>;

template<typename T>
concept SmallInteger = std::integral<T> && sizeof(T) <= 4;

template<typename T>
concept NotPointer = !std::is_pointer_v<T>;

// Concept subsumption example
template<typename T>
concept Animal = requires(T t) {
    t.eat();
    t.sleep();
};

template<typename T>
concept Mammal = Animal<T> && requires(T t) {
    t.give_birth();
};

template<typename T>
concept Dog = Mammal<T> && requires(T t) {
    t.bark();
    t.wag_tail();
};

// Function overloading with concept constraints
template<Animal T>
void care_for(const T& animal) {
    std::cout << "Providing basic animal care\n";
}

template<Mammal T>
void care_for(const T& mammal) {
    std::cout << "Providing mammal-specific care\n";
}

template<Dog T>
void care_for(const T& dog) {
    std::cout << "Providing dog-specific care\n";
}

// Example classes for demonstration
class GenericAnimal {
public:
    void eat() { std::cout << "Eating...\n"; }
    void sleep() { std::cout << "Sleeping...\n"; }
};

class Cat {
public:
    void eat() { std::cout << "Cat eating...\n"; }
    void sleep() { std::cout << "Cat sleeping...\n"; }
    void give_birth() { std::cout << "Cat giving birth...\n"; }
};

class GoldenRetriever {
public:
    void eat() { std::cout << "Dog eating...\n"; }
    void sleep() { std::cout << "Dog sleeping...\n"; }
    void give_birth() { std::cout << "Dog giving birth...\n"; }
    void bark() { std::cout << "Woof!\n"; }
    void wag_tail() { std::cout << "Wagging tail!\n"; }
};

void demonstrate_concept_composition() {
    std::cout << "\n=== Concept Composition and Logical Operations ===\n";

    // Numeric type concept
    auto process_numeric = []<NumericType T>(T value) {
        std::cout << "Processing numeric value: " << value << "\n";
    };

    process_numeric(42);      // int
    process_numeric(3.14);    // double
    process_numeric(5u);      // unsigned int

    // Small integer concept
    auto process_small_int = []<SmallInteger T>(T value) {
        std::cout << "Processing small integer: " << value
                  << " (size: " << sizeof(T) << " bytes)\n";
    };

    process_small_int(static_cast<short>(100));
    process_small_int(42);

    // Concept subsumption - most specific overload is chosen
    std::cout << "\nConcept subsumption examples:\n";
    GenericAnimal animal;
    Cat cat;
    GoldenRetriever dog;

    care_for(animal);  // Calls Animal version
    care_for(cat);     // Calls Mammal version
    care_for(dog);     // Calls Dog version (most specific)
}

// ============================================================================
// 5. ABBREVIATED FUNCTION TEMPLATES
// ============================================================================

// Abbreviated function template syntax (C++20)
void print_comparable(Comparable auto value) {
    std::cout << "Comparable value: " << value << "\n";
}

void process_container(SequenceContainer auto& container) {
    std::cout << "Container has " << container.size() << " elements\n";
    for (const auto& item : container) {
        std::cout << item << " ";
    }
    std::cout << "\n";
}

// Multiple concept constraints
void advanced_math(std::floating_point auto x, std::integral auto n) {
    auto result = x;
    for (int i = 0; i < n; ++i) {
        result *= x;
    }
    std::cout << x << "^" << n << " = " << result << "\n";
}

// Concept with auto return type
Arithmetic auto safe_divide(Arithmetic auto a, Arithmetic auto b) {
    if (b == 0) {
        throw std::invalid_argument("Division by zero");
    }
    return a / b;
}

void demonstrate_abbreviated_templates() {
    std::cout << "\n=== Abbreviated Function Templates ===\n";

    // Comparable values
    print_comparable(42);
    print_comparable(std::string("Hello"));

    // Container processing
    std::vector<int> vec = {1, 2, 3, 4, 5};
    process_container(vec);

    // Multiple constraints
    advanced_math(2.0, 3);
    advanced_math(1.5, 4);

    // Safe division
    try {
        std::cout << "safe_divide(10, 3) = " << safe_divide(10, 3) << "\n";
        std::cout << "safe_divide(7.5, 2.5) = " << safe_divide(7.5, 2.5) << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

// ============================================================================
// 6. CONCEPTS FOR SFINAE REPLACEMENT
// ============================================================================

// Old SFINAE approach (C++17 and earlier)
template<typename T>
std::enable_if_t<std::is_arithmetic_v<T>, T>
old_square(T value) {
    return value * value;
}

// New concept-based approach (C++20)
template<Arithmetic T>
T new_square(T value) {
    return value * value;
}

// More complex SFINAE replacement
template<typename T>
concept HasSizeMethod = requires(T t) {
    { t.size() } -> std::convertible_to<std::size_t>;
};

template<typename T>
concept HasLengthMethod = requires(T t) {
    { t.length() } -> std::convertible_to<std::size_t>;
};

// Overload resolution based on available methods
std::size_t get_size(HasSizeMethod auto&& container) {
    return container.size();
}

std::size_t get_size(HasLengthMethod auto&& container) {
    return container.length();
}

void demonstrate_sfinae_replacement() {
    std::cout << "\n=== Concepts for SFINAE Replacement ===\n";

    // Both approaches work the same
    std::cout << "old_square(5) = " << old_square(5) << "\n";
    std::cout << "new_square(5) = " << new_square(5) << "\n";
    std::cout << "new_square(3.14) = " << new_square(3.14) << "\n";

    // Method-based overload resolution
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::string str = "Hello World";

    std::cout << "Vector size: " << get_size(vec) << "\n";
    std::cout << "String length: " << get_size(str) << "\n";
}

// ============================================================================
// 7. ADVANCED CONCEPTS FOR ALGORITHMS
// ============================================================================

template<typename Func, typename... Args>
concept Invocable = std::invocable<Func, Args...>;

template<typename Func, typename T>
concept Predicate = std::predicate<Func, T>;

template<typename Func, typename T>
concept UnaryFunction = requires(Func f, T t) {
    { f(t) } -> std::convertible_to<T>;
};

template<typename Func, typename T>
concept BinaryFunction = requires(Func f, T a, T b) {
    { f(a, b) } -> std::convertible_to<T>;
};

// Generic algorithms using concepts
template<std::ranges::range Range, Predicate<std::ranges::range_value_t<Range>> Pred>
auto filter(Range&& range, Pred predicate) {
    using ValueType = std::ranges::range_value_t<Range>;
    std::vector<ValueType> result;

    std::ranges::copy_if(range, std::back_inserter(result), predicate);
    return result;
}

template<std::ranges::range Range, typename Func>
requires std::invocable<Func, std::ranges::range_value_t<Range>>
auto transform(Range&& range, Func function) {
    using InputType = std::ranges::range_value_t<Range>;
    using OutputType = std::invoke_result_t<Func, InputType>;

    std::vector<OutputType> result;
    result.reserve(std::ranges::size(range));

    std::ranges::transform(range, std::back_inserter(result), function);
    return result;
}

template<std::ranges::range Range, BinaryFunction<std::ranges::range_value_t<Range>> BinOp>
auto reduce(Range&& range, BinOp binary_op) {
    auto it = std::ranges::begin(range);
    if (it == std::ranges::end(range)) {
        throw std::invalid_argument("Cannot reduce empty range");
    }

    auto result = *it++;
    return std::accumulate(it, std::ranges::end(range), result, binary_op);
}

void demonstrate_algorithm_concepts() {
    std::cout << "\n=== Advanced Concepts for Algorithms ===\n";

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Filter even numbers
    auto evens = filter(numbers, [](int x) { return x % 2 == 0; });
    std::cout << "Even numbers: ";
    for (int n : evens) std::cout << n << " ";
    std::cout << "\n";

    // Transform to squares
    auto squares = transform(numbers, [](int x) { return x * x; });
    std::cout << "Squares: ";
    for (int n : squares) std::cout << n << " ";
    std::cout << "\n";

    // Reduce to sum
    auto sum = reduce(numbers, [](int a, int b) { return a + b; });
    std::cout << "Sum: " << sum << "\n";

    // Transform strings to lengths
    std::vector<std::string> words = {"hello", "world", "concepts", "rock"};
    auto lengths = transform(words, [](const std::string& s) { return s.length(); });
    std::cout << "Word lengths: ";
    for (auto len : lengths) std::cout << len << " ";
    std::cout << "\n";
}

// ============================================================================
// 8. CONCEPTS FOR CLASS TEMPLATES
// ============================================================================

template<typename T>
concept DefaultConstructible = std::default_initializable<T>;

template<typename T>
concept CopyConstructible = std::copy_constructible<T>;

template<typename T>
concept MoveConstructible = std::move_constructible<T>;

template<typename T>
concept EqualityComparable = std::equality_comparable<T>;

// Generic container with concept constraints
template<typename T>
requires DefaultConstructible<T> && CopyConstructible<T> && EqualityComparable<T>
class GenericContainer {
private:
    std::vector<T> data;

public:
    void add(const T& item) {
        data.push_back(item);
    }

    void add(T&& item) requires MoveConstructible<T> {
        data.push_back(std::move(item));
    }

    bool contains(const T& item) const {
        return std::find(data.begin(), data.end(), item) != data.end();
    }

    std::size_t count(const T& item) const {
        return std::count(data.begin(), data.end(), item);
    }

    void remove_all(const T& item) {
        data.erase(std::remove(data.begin(), data.end(), item), data.end());
    }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    std::size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }
};

// Specialized container for numeric types
template<Arithmetic T>
class NumericContainer {
private:
    std::vector<T> data;

public:
    void add(T value) {
        data.push_back(value);
    }

    T sum() const {
        return std::accumulate(data.begin(), data.end(), T{});
    }

    T average() const {
        if (data.empty()) return T{};
        return sum() / static_cast<T>(data.size());
    }

    T min() const {
        if (data.empty()) return T{};
        return *std::min_element(data.begin(), data.end());
    }

    T max() const {
        if (data.empty()) return T{};
        return *std::max_element(data.begin(), data.end());
    }

    std::size_t size() const { return data.size(); }
};

void demonstrate_class_template_concepts() {
    std::cout << "\n=== Concepts for Class Templates ===\n";

    // Generic container with strings
    GenericContainer<std::string> string_container;
    string_container.add("hello");
    string_container.add("world");
    string_container.add("hello");

    std::cout << "String container size: " << string_container.size() << "\n";
    std::cout << "Contains 'hello': " << std::boolalpha
              << string_container.contains("hello") << "\n";
    std::cout << "Count of 'hello': " << string_container.count("hello") << "\n";

    // Numeric container with integers
    NumericContainer<int> int_container;
    int_container.add(10);
    int_container.add(20);
    int_container.add(30);
    int_container.add(40);
    int_container.add(50);

    std::cout << "Numeric container sum: " << int_container.sum() << "\n";
    std::cout << "Numeric container average: " << int_container.average() << "\n";
    std::cout << "Numeric container min: " << int_container.min() << "\n";
    std::cout << "Numeric container max: " << int_container.max() << "\n";

    // Numeric container with doubles
    NumericContainer<double> double_container;
    double_container.add(1.5);
    double_container.add(2.7);
    double_container.add(3.8);

    std::cout << "Double container average: " << double_container.average() << "\n";
}

// ============================================================================
// 9. CONCEPTS FOR FINANCIAL/TRADING APPLICATIONS
// ============================================================================

template<typename T>
concept Price = std::floating_point<T> && requires(T price) {
    price >= T{0};  // Prices should be non-negative
};

template<typename T>
concept Quantity = std::arithmetic<T> && requires(T qty) {
    { qty > T{0} } -> std::same_as<bool>;  // Quantities should be positive
};

template<typename T>
concept Instrument = requires(T instrument) {
    { instrument.symbol() } -> std::convertible_to<std::string>;
    { instrument.price() } -> Price;
    { instrument.is_valid() } -> std::same_as<bool>;
};

template<typename T>
concept Order = requires(T order) {
    { order.instrument() } -> Instrument;
    { order.quantity() } -> Quantity;
    { order.price() } -> Price;
    { order.side() } -> std::convertible_to<std::string>;  // "BUY" or "SELL"
};

// Example trading classes
class Stock {
private:
    std::string symbol_;
    double price_;
    bool valid_;

public:
    Stock(std::string sym, double p) : symbol_(std::move(sym)), price_(p), valid_(p > 0) {}

    std::string symbol() const { return symbol_; }
    double price() const { return price_; }
    bool is_valid() const { return valid_; }

    void update_price(double new_price) {
        if (new_price > 0) {
            price_ = new_price;
            valid_ = true;
        }
    }
};

class TradeOrder {
private:
    Stock instrument_;
    int quantity_;
    double price_;
    std::string side_;

public:
    TradeOrder(Stock inst, int qty, double p, std::string s)
        : instrument_(std::move(inst)), quantity_(qty), price_(p), side_(std::move(s)) {}

    const Stock& instrument() const { return instrument_; }
    int quantity() const { return quantity_; }
    double price() const { return price_; }
    std::string side() const { return side_; }
};

// Trading functions using concepts
template<Price PriceType>
PriceType calculate_vwap(const std::vector<std::pair<PriceType, int>>& trades) {
    PriceType total_value = 0;
    int total_quantity = 0;

    for (const auto& [price, quantity] : trades) {
        total_value += price * quantity;
        total_quantity += quantity;
    }

    return total_quantity > 0 ? total_value / total_quantity : PriceType{0};
}

template<Order OrderType>
void process_order(const OrderType& order) {
    std::cout << "Processing " << order.side() << " order: "
              << order.quantity() << " shares of " << order.instrument().symbol()
              << " at $" << order.price() << "\n";
}

template<Instrument InstrumentType>
void print_market_data(const InstrumentType& instrument) {
    std::cout << "Symbol: " << instrument.symbol()
              << ", Price: $" << instrument.price()
              << ", Valid: " << std::boolalpha << instrument.is_valid() << "\n";
}

void demonstrate_trading_concepts() {
    std::cout << "\n=== Concepts for Financial/Trading Applications ===\n";

    // Create instruments
    Stock apple("AAPL", 150.25);
    Stock google("GOOGL", 2800.50);

    print_market_data(apple);
    print_market_data(google);

    // Create orders
    TradeOrder buy_order(apple, 100, 150.30, "BUY");
    TradeOrder sell_order(google, 50, 2799.75, "SELL");

    process_order(buy_order);
    process_order(sell_order);

    // Calculate VWAP
    std::vector<std::pair<double, int>> trades = {
        {150.25, 100},
        {150.30, 200},
        {150.28, 150},
        {150.32, 75}
    };

    double vwap = calculate_vwap(trades);
    std::cout << "VWAP: $" << vwap << "\n";
}

// ============================================================================
// 10. ERROR MESSAGES COMPARISON
// ============================================================================

void demonstrate_error_messages() {
    std::cout << "\n=== Better Error Messages with Concepts ===\n";
    std::cout << "Concepts provide much clearer error messages compared to SFINAE:\n\n";

    std::cout << "Traditional SFINAE error (C++17):\n";
    std::cout << "- Long, cryptic template instantiation errors\n";
    std::cout << "- Difficult to understand what went wrong\n";
    std::cout << "- Deep template instantiation stack traces\n\n";

    std::cout << "Concepts error messages (C++20):\n";
    std::cout << "- Clear constraint violation messages\n";
    std::cout << "- Points directly to the failed requirement\n";
    std::cout << "- Much shorter and more readable\n\n";

    std::cout << "Example concept error:\n";
    std::cout << "  error: cannot call function 'process_numeric'\n";
    std::cout << "  note: constraints not satisfied\n";
    std::cout << "  note: concept 'Arithmetic<std::string>' evaluated to false\n";
    std::cout << "  note: 'std::string' is not an arithmetic type\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 Concepts Use Cases and Examples\n";
    std::cout << "=====================================\n";

    demonstrate_basic_concepts();
    demonstrate_complex_concepts();
    demonstrate_standard_concepts();
    demonstrate_concept_composition();
    demonstrate_abbreviated_templates();
    demonstrate_sfinae_replacement();
    demonstrate_algorithm_concepts();
    demonstrate_class_template_concepts();
    demonstrate_trading_concepts();
    demonstrate_error_messages();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Concepts make template constraints explicit and readable\n";
    std::cout << "2. Much better error messages than SFINAE\n";
    std::cout << "3. Enable more precise overload resolution\n";
    std::cout << "4. Self-documenting template interfaces\n";
    std::cout << "5. Abbreviated function template syntax\n";
    std::cout << "6. Concept composition with logical operators\n";
    std::cout << "7. Standard library provides many useful concepts\n";
    std::cout << "8. Excellent replacement for complex SFINAE patterns\n";
    std::cout << "9. Particularly useful for generic algorithms and containers\n";
    std::cout << "10. Domain-specific concepts improve code clarity\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
 * - Use -std=c++20 flag for compilation
 *
 * Example compilation:
 * g++ -std=c++20 -Wall -Wextra cpp20_concepts_use_cases_examples.cpp -o concepts_demo
 *
 * Key Standard Library Concepts (from <concepts>):
 * - std::same_as<T, U>
 * - std::derived_from<Derived, Base>
 * - std::convertible_to<From, To>
 * - std::integral<T>
 * - std::signed_integral<T>
 * - std::unsigned_integral<T>
 * - std::floating_point<T>
 * - std::default_initializable<T>
 * - std::move_constructible<T>
 * - std::copy_constructible<T>
 * - std::equality_comparable<T>
 * - std::totally_ordered<T>
 * - std::invocable<F, Args...>
 * - std::predicate<F, Args...>
 * - std::regular<T>
 * - std::semiregular<T>
 */
