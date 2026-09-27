/*
 * ============================================================================
 * C++14 Features Overview - Comprehensive Examples
 * ============================================================================
 *
 * C++14 is a refinement of C++11, often called "C++11 done right".
 * It focused on fixing deficiencies and adding small quality-of-life improvements.
 *
 * Key C++14 Features:
 *  1.  Generic lambdas (auto parameters)
 *  2.  Lambda capture initializers (generalized lambda capture)
 *  3.  Return type deduction for functions
 *  4.  Variable templates
 *  5.  Binary literals and digit separators
 *  6.  std::make_unique
 *  7.  Relaxed constexpr (if/for/while in constexpr)
 *  8.  std::exchange
 *  9.  std::integer_sequence / std::index_sequence
 *  10. Deprecated attribute [[deprecated]]
 *  11. Aggregate member initialization
 *  12. std::shared_timed_mutex + std::shared_lock
 *  13. std::quoted
 *  14. Heterogeneous lookup in std::map/set
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <algorithm>
#include <numeric>
#include <type_traits>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <shared_mutex>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <chrono>
#include <thread>

// ============================================================================
// 1. GENERIC LAMBDAS (auto parameters)
// ============================================================================
// C++11 lambdas required explicit parameter types.
// C++14 allows 'auto' parameters, making lambdas polymorphic.

void demonstrate_generic_lambdas() {
    std::cout << "\n=== 1. Generic Lambdas ===\n";

    // C++11 style: explicit type
    auto add_int_cpp11 = [](int a, int b) { return a + b; };

    // C++14 style: auto parameters
    auto add_generic = [](auto a, auto b) { return a + b; };

    std::cout << "add_generic(1, 2)        = " << add_generic(1, 2)        << "\n";
    std::cout << "add_generic(1.5, 2.3)    = " << add_generic(1.5, 2.3)    << "\n";
    std::cout << "add_generic(\"foo\", \"bar\") = " << add_generic(std::string("foo"), std::string("bar")) << "\n";

    // Sorting with generic lambda
    std::vector<int>    ints    = {5, 3, 1, 4, 2};
    std::vector<double> doubles = {5.5, 3.3, 1.1, 4.4, 2.2};

    auto less_than = [](auto a, auto b) { return a < b; };
    std::sort(ints.begin(),    ints.end(),    less_than);
    std::sort(doubles.begin(), doubles.end(), less_than);

    std::cout << "Sorted ints: ";
    for (auto v : ints)    std::cout << v << " ";
    std::cout << "\nSorted doubles: ";
    for (auto v : doubles) std::cout << v << " ";
    std::cout << "\n";

    // Generic lambda with perfect forwarding
    auto print_pair = [](auto&& first, auto&& second) {
        std::cout << "(" << std::forward<decltype(first)>(first)
                  << ", " << std::forward<decltype(second)>(second) << ")\n";
    };
    print_pair(42, "hello");
    print_pair(3.14, true);
}

// ============================================================================
// 2. LAMBDA CAPTURE INITIALIZERS (Generalized Lambda Capture)
// ============================================================================
// C++14 allows lambda captures with initialization expressions.
// This enables move-capture (impossible in C++11) and computed captures.

void demonstrate_lambda_capture_init() {
    std::cout << "\n=== 2. Lambda Capture Initializers ===\n";

    // Move-capture: unique_ptr cannot be copied, but can be moved into lambda
    auto ptr = std::make_unique<int>(42);
    auto lambda_with_move = [p = std::move(ptr)]() {
        std::cout << "Moved-into lambda: *p = " << *p << "\n";
    };
    lambda_with_move();
    // ptr is now null
    std::cout << "ptr after move: " << (ptr ? "valid" : "null") << "\n";

    // Compute a value at capture time
    int base = 10;
    auto add_base = [b = base * 2](int x) { return x + b; };
    std::cout << "add_base(5) = " << add_base(5) << "\n";  // 5 + 20 = 25

    // Rename a captured variable
    int value = 100;
    auto print_val = [v = value]() {
        std::cout << "Captured as 'v': " << v << "\n";
    };
    value = 999;  // modifying original does NOT affect lambda
    print_val();  // prints 100

    // Capture this members with new name (useful for async)
    struct Worker {
        int id = 7;
        std::function<void()> make_task() {
            return [id_copy = this->id]() {
                std::cout << "Task running for worker id: " << id_copy << "\n";
            };
        }
    };
    Worker w;
    auto task = w.make_task();
    w.id = 999;  // changing original
    task();      // still prints 7
}

// ============================================================================
// 3. RETURN TYPE DEDUCTION FOR FUNCTIONS
// ============================================================================
// C++14 extends auto return type deduction to regular functions.
// No need for trailing return type when the function body defines the type.

auto fibonacci(int n) {  // return type deduced as int
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// Works with templates too
template<typename T, typename U>
auto multiply(T t, U u) {
    return t * u;
}

// Recursive with deduced return type - all return statements must agree
auto factorial(int n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

void demonstrate_return_type_deduction() {
    std::cout << "\n=== 3. Return Type Deduction ===\n";
    std::cout << "fibonacci(10) = " << fibonacci(10) << "\n";
    std::cout << "factorial(7)  = " << factorial(7)  << "\n";
    std::cout << "multiply(3, 2.5) = " << multiply(3, 2.5) << "\n";  // double

    // Lambda with complex return type deduction
    auto make_adder = [](int offset) {
        return [offset](int x) { return x + offset; };  // returns a lambda
    };
    auto add10 = make_adder(10);
    std::cout << "add10(5) = " << add10(5) << "\n";
}

// ============================================================================
// 4. VARIABLE TEMPLATES
// ============================================================================
// Variables can now be templated, useful for mathematical constants.

template<typename T>
constexpr T pi = T(3.14159265358979323846L);

template<typename T>
constexpr T e = T(2.71828182845904523536L);

template<typename T>
constexpr T golden_ratio = T(1.61803398874989484820L);

// Variable template for type traits
template<typename T>
constexpr bool is_integral_v = std::is_integral<T>::value;  // Predates std::is_integral_v

template<typename T>
constexpr bool is_pointer_v = std::is_pointer<T>::value;

void demonstrate_variable_templates() {
    std::cout << "\n=== 4. Variable Templates ===\n";
    std::cout << "pi<float>  = " << pi<float>  << "\n";
    std::cout << "pi<double> = " << pi<double> << "\n";
    std::cout << "e<double>  = " << e<double>  << "\n";
    std::cout << "golden_ratio<double> = " << golden_ratio<double> << "\n";

    std::cout << "is_integral_v<int>       = " << is_integral_v<int>    << "\n";
    std::cout << "is_integral_v<double>    = " << is_integral_v<double> << "\n";
    std::cout << "is_pointer_v<int*>       = " << is_pointer_v<int*>    << "\n";

    // Variable template used in computation
    auto circle_area = [](double r) { return pi<double> * r * r; };
    std::cout << "Area of circle r=5: " << circle_area(5.0) << "\n";
}

// ============================================================================
// 5. BINARY LITERALS AND DIGIT SEPARATORS
// ============================================================================

void demonstrate_binary_literals_digit_separators() {
    std::cout << "\n=== 5. Binary Literals & Digit Separators ===\n";

    // Binary literals (prefix 0b or 0B)
    constexpr int flags    = 0b1010'1010;  // 170
    constexpr int mask_low = 0b0000'1111;  // 15
    constexpr int mask_hi  = 0b1111'0000;  // 240

    std::cout << "flags    = " << flags    << " (0b" << std::bitset<8>(flags) << ")\n";
    std::cout << "mask_low = " << mask_low << "\n";
    std::cout << "mask_hi  = " << mask_hi  << "\n";
    std::cout << "flags & mask_low = " << (flags & mask_low) << "\n";

    // Digit separators in decimal, hex, octal, binary
    constexpr long long  one_billion = 1'000'000'000LL;
    constexpr double     planck      = 6.626'070'040e-34;
    constexpr uint32_t   hex_color   = 0xFF'AA'BB'CC;
    constexpr int        binary_byte = 0b1100'0011;

    std::cout << "one_billion = " << one_billion << "\n";
    std::cout << "planck      = " << planck      << "\n";
    std::cout << "hex_color   = 0x" << std::hex << hex_color << std::dec << "\n";
    std::cout << "binary_byte = " << binary_byte << "\n";

    // Trading-relevant: tick sizes, price constants
    constexpr int64_t max_notional  = 1'000'000'000'000LL;  // 1 trillion
    constexpr int     max_qty       = 1'000'000;             // 1 million shares
    constexpr int     price_scale   = 1'000'000;             // 6 decimal places (FIX)
    std::cout << "max_notional = " << max_notional  << "\n";
    std::cout << "max_qty      = " << max_qty       << "\n";
    std::cout << "price_scale  = " << price_scale   << "\n";
}

// Need to include bitset
#include <bitset>

// ============================================================================
// 6. std::make_unique
// ============================================================================
// C++11 had make_shared but NOT make_unique (oversight fixed in C++14).

struct Resource {
    std::string name;
    int         value;
    Resource(std::string n, int v) : name(std::move(n)), value(v) {
        std::cout << "  Resource(" << name << ", " << value << ") constructed\n";
    }
    ~Resource() {
        std::cout << "  Resource(" << name << ") destroyed\n";
    }
};

void demonstrate_make_unique() {
    std::cout << "\n=== 6. std::make_unique ===\n";

    // Single object
    {
        auto r1 = std::make_unique<Resource>("Alpha", 1);
        std::cout << "r1->name = " << r1->name << "\n";
    }  // destroyed here

    // Array form
    {
        auto arr = std::make_unique<int[]>(5);
        for (int i = 0; i < 5; ++i) arr[i] = i * i;
        std::cout << "arr[3] = " << arr[3] << "\n";
    }

    // Exception safety: make_unique is exception safe unlike 'new'
    // Bad (C++11 era):   f(unique_ptr<A>(new A()), g());  // potential leak
    // Good (C++14):      f(make_unique<A>(), g());         // safe

    // Factory function
    auto make_resource = [](const std::string& name, int value) {
        return std::make_unique<Resource>(name, value);
    };
    auto r2 = make_resource("Beta", 42);
    std::cout << "r2->value = " << r2->value << "\n";
}

// ============================================================================
// 7. RELAXED CONSTEXPR
// ============================================================================
// C++11 constexpr functions had severe restrictions (only one return statement).
// C++14 relaxes these: allows if/else, loops, local variables, and more.

// C++11 style (single expression only)
constexpr int cpp11_abs(int x) {
    return x < 0 ? -x : x;
}

// C++14 style (multi-statement, loops, local variables)
constexpr int cpp14_factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

constexpr int cpp14_gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

constexpr bool cpp14_is_prime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; ++i) {
        if (n % i == 0) return false;
    }
    return true;
}

void demonstrate_relaxed_constexpr() {
    std::cout << "\n=== 7. Relaxed constexpr ===\n";

    // All computed at compile time
    static_assert(cpp14_factorial(10)  == 3628800, "factorial(10) wrong");
    static_assert(cpp14_gcd(48, 18)    == 6,       "gcd(48,18) wrong");
    static_assert(cpp14_is_prime(17)   == true,    "17 is prime");
    static_assert(cpp14_is_prime(15)   == false,   "15 is not prime");

    std::cout << "12! = "       << cpp14_factorial(12)  << " (computed at compile time)\n";
    std::cout << "gcd(48,18) = " << cpp14_gcd(48, 18)   << "\n";
    std::cout << "is_prime(97) = " << (cpp14_is_prime(97) ? "yes" : "no") << "\n";

    // Constexpr array filled at compile time
    constexpr auto primes_under_20 = []() {
        std::array<int, 8> result{};
        int idx = 0;
        for (int n = 2; n < 20 && idx < 8; ++n) {
            if (cpp14_is_prime(n)) result[idx++] = n;
        }
        return result;
    }();
    std::cout << "Primes under 20: ";
    for (auto p : primes_under_20) std::cout << p << " ";
    std::cout << "\n";
}

// ============================================================================
// 8. std::exchange
// ============================================================================
// std::exchange(obj, new_val) assigns new_val to obj and returns the OLD value.
// Useful for implementing move constructors, swap-based patterns.

struct MovableBuffer {
    int*  data;
    size_t size;

    MovableBuffer(size_t n) : data(new int[n]), size(n) {
        for (size_t i = 0; i < n; ++i) data[i] = static_cast<int>(i);
    }

    // Move constructor using std::exchange
    MovableBuffer(MovableBuffer&& other) noexcept
        : data(std::exchange(other.data, nullptr)),
          size(std::exchange(other.size, 0)) {}

    // Move assignment using std::exchange
    MovableBuffer& operator=(MovableBuffer&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = std::exchange(other.data, nullptr);
            size = std::exchange(other.size, 0);
        }
        return *this;
    }

    ~MovableBuffer() { delete[] data; }
};

void demonstrate_exchange() {
    std::cout << "\n=== 8. std::exchange ===\n";

    // Basic exchange
    int x = 10;
    int old_x = std::exchange(x, 42);
    std::cout << "old_x = " << old_x << ", x = " << x << "\n";

    // Move constructor/assignment demo
    MovableBuffer buf(5);
    std::cout << "buf.data[3] = " << buf.data[3] << ", size = " << buf.size << "\n";

    MovableBuffer buf2 = std::move(buf);
    std::cout << "After move:\n";
    std::cout << "  buf.data  = " << (buf.data ? "valid" : "null") << ", size = " << buf.size << "\n";
    std::cout << "  buf2.data[3] = " << buf2.data[3] << ", size = " << buf2.size << "\n";

    // Trading use case: atomic state transition using exchange
    int order_state = 0;  // 0 = NEW
    int prev_state  = std::exchange(order_state, 1);  // transition to PENDING
    std::cout << "Order state: " << prev_state << " -> " << order_state << "\n";
}

// ============================================================================
// 9. std::integer_sequence / std::index_sequence
// ============================================================================
// Compile-time integer sequences for metaprogramming, especially
// unpacking tuples into function arguments.

// Print all elements of a tuple
template<typename Tuple, std::size_t... I>
void print_tuple_impl(const Tuple& t, std::index_sequence<I...>) {
    std::cout << "(";
    ((std::cout << (I == 0 ? "" : ", ") << std::get<I>(t)), ...);  // C++17 fold; C++14 workaround below
    std::cout << ")\n";
}

template<typename... Args>
void print_tuple(const std::tuple<Args...>& t) {
    print_tuple_impl(t, std::index_sequence_for<Args...>{});
}

// Apply a function to each element of a tuple
template<typename F, typename Tuple, std::size_t... I>
void apply_to_each_impl(F&& f, const Tuple& t, std::index_sequence<I...>) {
    (f(std::get<I>(t)), ...);  // C++17 fold expression (supported in C++17+)
}

template<typename F, typename... Args>
void apply_to_each(F&& f, const std::tuple<Args...>& t) {
    apply_to_each_impl(std::forward<F>(f), t, std::index_sequence_for<Args...>{});
}

// Call a function with tuple elements as arguments
template<typename F, typename Tuple, std::size_t... I>
auto call_with_tuple_impl(F&& f, Tuple&& t, std::index_sequence<I...>) {
    return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}

template<typename F, typename... Args>
auto call_with_tuple(F&& f, std::tuple<Args...>&& t) {
    return call_with_tuple_impl(
        std::forward<F>(f),
        std::move(t),
        std::index_sequence_for<Args...>{}
    );
}

void demonstrate_integer_sequence() {
    std::cout << "\n=== 9. std::integer_sequence / std::index_sequence ===\n";

    // std::integer_sequence<T, Ns...> - compile-time sequence of integers
    using seq3 = std::integer_sequence<int, 0, 1, 2>;
    using seq5 = std::make_integer_sequence<int, 5>;  // 0,1,2,3,4
    using idx4 = std::make_index_sequence<4>;         // 0,1,2,3 (size_t)

    std::cout << "seq5 size = " << seq5::size() << "\n";
    std::cout << "idx4 size = " << idx4::size() << "\n";

    // Print tuple
    auto t = std::make_tuple(1, 3.14, std::string("hello"), true);
    std::cout << "Tuple: ";
    print_tuple(t);

    // Call function with tuple args
    auto sum3 = [](int a, int b, int c) { return a + b + c; };
    auto args = std::make_tuple(10, 20, 30);
    int result = call_with_tuple(sum3, std::move(args));
    std::cout << "sum(10, 20, 30) via tuple = " << result << "\n";
}

// ============================================================================
// 10. [[deprecated]] ATTRIBUTE
// ============================================================================

[[deprecated("Use new_api() instead")]]
void old_api(int x) {
    std::cout << "old_api(" << x << ") called\n";
}

void new_api(int x) {
    std::cout << "new_api(" << x << ") called\n";
}

[[deprecated]]
int legacy_function() { return 42; }

void demonstrate_deprecated() {
    std::cout << "\n=== 10. [[deprecated]] Attribute ===\n";
    new_api(42);
    // Calling old_api(42); would produce a compiler warning:
    // warning: 'old_api' is deprecated: Use new_api() instead
    std::cout << "[[deprecated]] generates compiler warnings (see source)\n";
}

// ============================================================================
// 11. std::shared_timed_mutex + std::shared_lock (Reader-Writer Lock)
// ============================================================================
// C++14 introduces shared_timed_mutex for multiple-readers/single-writer pattern.
// C++17 adds std::shared_mutex (without timed operations).

class ThreadSafeCache {
public:
    // Multiple threads can read simultaneously
    int get(const std::string& key) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : -1;
    }

    // Only one thread can write at a time (exclusive lock)
    void put(const std::string& key, int value) {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        data_[key] = value;
    }

    // Try to acquire read lock within timeout
    bool try_get(const std::string& key, int& out, std::chrono::milliseconds timeout) const {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_, std::defer_lock);
        if (!lock.try_lock_for(timeout)) return false;
        auto it = data_.find(key);
        if (it == data_.end()) return false;
        out = it->second;
        return true;
    }

private:
    mutable std::shared_timed_mutex     mutex_;
    std::map<std::string, int>          data_;
};

void demonstrate_shared_timed_mutex() {
    std::cout << "\n=== 11. std::shared_timed_mutex + std::shared_lock ===\n";

    ThreadSafeCache cache;
    cache.put("AAPL", 150);
    cache.put("GOOG", 2800);

    // Concurrent reads
    std::vector<std::thread> readers;
    std::atomic<int> read_count{0};
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&, i]() {
            int val = cache.get(i % 2 == 0 ? "AAPL" : "GOOG");
            ++read_count;
        });
    }
    for (auto& t : readers) t.join();
    std::cout << "All " << read_count.load() << " readers completed\n";

    // Try with timeout
    int value = -1;
    bool found = cache.try_get("AAPL", value, std::chrono::milliseconds(100));
    std::cout << "try_get AAPL: " << (found ? "found" : "not found")
              << ", value = " << value << "\n";
}

// ============================================================================
// 12. std::quoted
// ============================================================================
// For reading/writing quoted strings with proper escape handling.

void demonstrate_quoted() {
    std::cout << "\n=== 12. std::quoted ===\n";

    // Writing quoted strings (useful for CSV, JSON-like output)
    std::string name = "hello world";
    std::cout << "Without quoted: " << name << "\n";
    std::cout << "With quoted:    " << std::quoted(name) << "\n";

    // Reading: handles embedded quotes and escapes
    std::istringstream iss(R"("embedded \"quotes\" here")");
    std::string result;
    iss >> std::quoted(result);
    std::cout << "Read with quoted: " << result << "\n";

    // Custom delimiter and escape
    std::string csv_field = "value,with,commas";
    std::ostringstream oss;
    oss << std::quoted(csv_field, '\'', '\\');
    std::cout << "Custom-quoted: " << oss.str() << "\n";
}

// ============================================================================
// 13. HETEROGENEOUS LOOKUP IN std::map / std::set
// ============================================================================
// C++14 adds find/count/lower_bound/upper_bound overloads accepting
// any comparable type (not just key_type), avoiding temporary construction.

struct StringLess {
    using is_transparent = void;  // REQUIRED tag to enable heterogeneous lookup

    bool operator()(const std::string& a, const std::string& b) const {
        return a < b;
    }
    bool operator()(const std::string& a, const char* b) const {
        return a < b;
    }
    bool operator()(const char* a, const std::string& b) const {
        return a < b;
    }
};

void demonstrate_heterogeneous_lookup() {
    std::cout << "\n=== 13. Heterogeneous Lookup ===\n";

    std::map<std::string, int, StringLess> symbol_map;
    symbol_map["AAPL"] = 150;
    symbol_map["GOOG"] = 2800;
    symbol_map["MSFT"] = 330;

    // C++11: would create a temporary std::string("AAPL")
    // C++14 with transparent comparator: no temporary created!
    const char* key = "AAPL";
    auto it = symbol_map.find(key);  // no std::string construction
    if (it != symbol_map.end()) {
        std::cout << "Found " << key << " = " << it->second
                  << " (no temporary string created)\n";
    }

    // std::set with heterogeneous lookup
    std::set<std::string, StringLess> symbols = {"AAPL", "GOOG", "MSFT", "TSLA"};
    std::cout << "Contains 'GOOG': " << (symbols.count("GOOG") ? "yes" : "no") << "\n";
    std::cout << "Contains 'META': " << (symbols.count("META") ? "yes" : "no") << "\n";

    // Lower bound for range queries
    auto lb = symbols.lower_bound("G");
    auto ub = symbols.upper_bound("M");
    std::cout << "Symbols between G and M: ";
    for (auto i = lb; i != ub; ++i) std::cout << *i << " ";
    std::cout << "\n";
}

// ============================================================================
// 14. C++14 STANDARD LIBRARY ADDITIONS
// ============================================================================

void demonstrate_stdlib_additions() {
    std::cout << "\n=== 14. C++14 Standard Library Additions ===\n";

    // std::rbegin / std::rend / std::cbegin / std::cend for arrays
    int arr[] = {1, 2, 3, 4, 5};
    std::cout << "Reverse: ";
    for (auto it = std::rbegin(arr); it != std::rend(arr); ++it)
        std::cout << *it << " ";
    std::cout << "\n";

    // std::is_null_pointer
    std::cout << "is_null_pointer<std::nullptr_t>: "
              << std::is_null_pointer<std::nullptr_t>::value << "\n";

    // std::tuple_element_t, std::tuple_size (alias templates)
    using T = std::tuple<int, double, std::string>;
    static_assert(std::is_same<std::tuple_element_t<0, T>, int>::value,    "");
    static_assert(std::is_same<std::tuple_element_t<1, T>, double>::value, "");
    std::cout << "tuple_size<T> = " << std::tuple_size<T>::value << "\n";

    // std::enable_if_t, std::conditional_t, std::decay_t (alias templates)
    template_alias_demo();
}

template<typename T>
std::enable_if_t<std::is_integral<T>::value, std::string>
type_name(T) { return "integral"; }

template<typename T>
std::enable_if_t<std::is_floating_point<T>::value, std::string>
type_name(T) { return "floating_point"; }

void template_alias_demo() {
    std::cout << "type_name(42)   = " << type_name(42)   << "\n";
    std::cout << "type_name(3.14) = " << type_name(3.14) << "\n";

    // std::decay_t removes refs, cv-qualifiers, and decays arrays/functions
    static_assert(std::is_same<std::decay_t<const int&>, int>::value, "");
    static_assert(std::is_same<std::decay_t<int[5]>,     int*>::value, "");
    std::cout << "std::decay_t checks passed\n";
}

// ============================================================================
// TRADING SYSTEM PRACTICAL EXAMPLE - C++14 Features Combined
// ============================================================================
// Demonstrates C++14 features in a trading-system context:
// generic lambdas, make_unique, heterogeneous lookup, constexpr

enum class OrderSide : uint8_t { BUY, SELL };
enum class OrderType : uint8_t { LIMIT, MARKET };

struct Order {
    uint64_t    id;
    std::string symbol;
    OrderSide   side;
    OrderType   type;
    double      price;
    uint64_t    qty;
    uint64_t    filled_qty = 0;

    [[nodiscard]] uint64_t remaining() const noexcept { return qty - filled_qty; }
    [[nodiscard]] bool     is_filled() const noexcept { return filled_qty >= qty; }
};

// Generic comparator with heterogeneous lookup
struct SymbolLess {
    using is_transparent = void;
    bool operator()(const std::string& a, const std::string& b) const { return a < b; }
    bool operator()(const std::string& a, const char*        b) const { return a < b; }
    bool operator()(const char*        a, const std::string& b) const { return a < b; }
};

class SimpleOrderManager {
public:
    uint64_t submit(std::string symbol, OrderSide side, OrderType type,
                    double price, uint64_t qty) {
        uint64_t id = ++next_id_;
        auto order = std::make_unique<Order>(Order{
            id, std::move(symbol), side, type, price, qty
        });
        orders_by_id_[id] = order.get();
        symbol_to_orders_[order->symbol].push_back(std::move(order));
        return id;
    }

    void cancel(uint64_t id) {
        auto it = orders_by_id_.find(id);
        if (it != orders_by_id_.end()) {
            std::cout << "Cancelled order " << id << "\n";
            orders_by_id_.erase(it);
        }
    }

    // Generic lambda for filtering orders
    template<typename Predicate>
    std::vector<uint64_t> find_orders(Predicate&& pred) const {
        std::vector<uint64_t> result;
        for (auto& [id, ptr] : orders_by_id_) {
            if (pred(*ptr)) result.push_back(id);
        }
        return result;
    }

    void print_summary() const {
        std::cout << "Active orders: " << orders_by_id_.size() << "\n";
        for (auto& [id, ptr] : orders_by_id_) {
            std::cout << "  [" << id << "] " << ptr->symbol
                      << (ptr->side == OrderSide::BUY ? " BUY " : " SELL ")
                      << ptr->qty << " @ " << ptr->price << "\n";
        }
    }

private:
    uint64_t                                        next_id_{0};
    std::map<uint64_t, Order*>                      orders_by_id_;
    std::map<std::string, std::vector<std::unique_ptr<Order>>, SymbolLess>
                                                    symbol_to_orders_;
};

void demonstrate_trading_example() {
    std::cout << "\n=== Trading Example (C++14 Features Combined) ===\n";

    SimpleOrderManager mgr;
    mgr.submit("AAPL", OrderSide::BUY,  OrderType::LIMIT, 150.50, 100);
    mgr.submit("AAPL", OrderSide::SELL, OrderType::LIMIT, 151.00, 50);
    mgr.submit("GOOG", OrderSide::BUY,  OrderType::MARKET, 0.0,  200);
    mgr.submit("MSFT", OrderSide::BUY,  OrderType::LIMIT, 330.00, 75);

    mgr.print_summary();

    // Generic lambda for filtering
    auto buy_orders = mgr.find_orders([](const Order& o) {
        return o.side == OrderSide::BUY;
    });
    std::cout << "BUY order IDs: ";
    for (auto id : buy_orders) std::cout << id << " ";
    std::cout << "\n";

    // Market orders
    auto market_orders = mgr.find_orders([](const Order& o) {
        return o.type == OrderType::MARKET;
    });
    std::cout << "MARKET order count: " << market_orders.size() << "\n";

    mgr.cancel(2);
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "============================================\n";
    std::cout << "        C++14 Features Overview\n";
    std::cout << "============================================\n";

    demonstrate_generic_lambdas();
    demonstrate_lambda_capture_init();
    demonstrate_return_type_deduction();
    demonstrate_variable_templates();
    demonstrate_binary_literals_digit_separators();
    demonstrate_make_unique();
    demonstrate_relaxed_constexpr();
    demonstrate_exchange();
    demonstrate_integer_sequence();
    demonstrate_deprecated();
    demonstrate_shared_timed_mutex();
    demonstrate_quoted();
    demonstrate_heterogeneous_lookup();
    demonstrate_stdlib_additions();
    demonstrate_trading_example();

    std::cout << "\n=== All C++14 Features Demonstrated ===\n";
    return 0;
}

