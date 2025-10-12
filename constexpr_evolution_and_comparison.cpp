#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <memory>
#include <type_traits>

/*
 * constexpr Evolution (C++11 to C++23) and constexpr vs consteval vs constinit
 *
 * Timeline:
 * C++11: Basic constexpr functions and variables
 * C++14: Relaxed constexpr (loops, conditionals, multiple statements)
 * C++17: constexpr if, constexpr lambdas
 * C++20: constexpr destructors, virtual functions, dynamic allocation
 * C++23: constexpr std::unique_ptr, more standard library support
 *
 * Keywords Comparison:
 * - constexpr: Can be evaluated at compile-time OR runtime
 * - consteval: MUST be evaluated at compile-time (immediate functions)
 * - constinit: Ensures compile-time initialization (C++20)
 */

// =============================================================================
// 1. C++11 CONSTEXPR - BASIC FUNCTIONALITY
// =============================================================================

namespace cpp11_features {

    // Simple constexpr function (single return statement only in C++11)
    constexpr int factorial_cpp11(int n) {
        return (n <= 1) ? 1 : n * factorial_cpp11(n - 1);
    }

    // constexpr variables
    constexpr int global_const = 42;
    constexpr double pi = 3.14159265359;

    // constexpr with user-defined types
    struct Point {
        constexpr Point(int x, int y) : x_(x), y_(y) {}
        constexpr int get_x() const { return x_; }
        constexpr int get_y() const { return y_; }

    private:
        int x_, y_;
    };

    constexpr Point origin(0, 0);

    void demonstrate_cpp11_constexpr() {
        std::cout << "\n=== C++11 constexpr Features ===\n";

        // Compile-time evaluation
        constexpr int fact5 = factorial_cpp11(5);
        std::cout << "5! = " << fact5 << "\n";

        // constexpr objects
        constexpr Point p(3, 4);
        std::cout << "Point: (" << p.get_x() << ", " << p.get_y() << ")\n";

        // Can still be used at runtime
        int runtime_n = 6;
        int runtime_fact = factorial_cpp11(runtime_n);  // Runtime evaluation
        std::cout << "Runtime 6! = " << runtime_fact << "\n";
    }
}

// =============================================================================
// 2. C++14 CONSTEXPR - RELAXED RESTRICTIONS
// =============================================================================

namespace cpp14_features {

    // Multiple statements allowed in C++14
    constexpr int factorial_cpp14(int n) {
        int result = 1;
        for (int i = 2; i <= n; ++i) {
            result *= i;
        }
        return result;
    }

    // Loops and conditionals
    constexpr int fibonacci(int n) {
        if (n <= 1) return n;

        int a = 0, b = 1;
        for (int i = 2; i <= n; ++i) {
            int temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }

    // Mutable variables in constexpr functions
    constexpr int count_digits(int n) {
        int count = 0;
        while (n > 0) {
            n /= 10;
            ++count;
        }
        return count;
    }

    // constexpr member functions can modify objects
    struct Counter {
        constexpr Counter() : value_(0) {}
        constexpr void increment() { ++value_; }
        constexpr int get_value() const { return value_; }

    private:
        int value_;
    };

    void demonstrate_cpp14_constexpr() {
        std::cout << "\n=== C++14 constexpr Features ===\n";

        constexpr int fact7 = factorial_cpp14(7);
        std::cout << "7! = " << fact7 << "\n";

        constexpr int fib10 = fibonacci(10);
        std::cout << "Fibonacci(10) = " << fib10 << "\n";

        constexpr int digits = count_digits(12345);
        std::cout << "Digits in 12345: " << digits << "\n";

        // Mutable constexpr operations
        constexpr auto get_counter_value = []() {
            Counter c;
            c.increment();
            c.increment();
            return c.get_value();
        };

        constexpr int counter_val = get_counter_value();
        std::cout << "Counter value: " << counter_val << "\n";
    }
}

// =============================================================================
// 3. C++17 CONSTEXPR - IF CONSTEXPR AND LAMBDAS
// =============================================================================

namespace cpp17_features {

    // constexpr if for conditional compilation
    template<typename T>
    constexpr auto stringify(T&& value) {
        if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
            return std::to_string(value);
        } else {
            return std::string(value);
        }
    }

    // constexpr lambdas
    constexpr auto square_lambda = [](int x) constexpr { return x * x; };

    // More complex constexpr if example
    template<typename T>
    constexpr bool process_type() {
        if constexpr (std::is_integral_v<T>) {
            std::cout << "Processing integral type\n";
            return true;
        } else if constexpr (std::is_floating_point_v<T>) {
            std::cout << "Processing floating point type\n";
            return true;
        } else {
            std::cout << "Processing other type\n";
            return false;
        }
    }

    // constexpr string operations (limited)
    constexpr size_t string_length(const char* str) {
        size_t len = 0;
        while (str[len] != '\0') {
            ++len;
        }
        return len;
    }

    void demonstrate_cpp17_constexpr() {
        std::cout << "\n=== C++17 constexpr Features ===\n";

        // constexpr lambda
        constexpr int squared = square_lambda(5);
        std::cout << "5^2 = " << squared << "\n";

        // constexpr if with different types
        process_type<int>();
        process_type<double>();
        process_type<std::string>();

        // constexpr string operations
        constexpr size_t len = string_length("Hello, World!");
        std::cout << "String length: " << len << "\n";

        // Runtime stringify (C++17 constexpr if)
        std::cout << "Stringify 42: " << stringify(42) << "\n";
        std::cout << "Stringify hello: " << stringify("hello") << "\n";
    }
}

// =============================================================================
// 4. C++20 CONSTEXPR - DESTRUCTORS, VIRTUAL FUNCTIONS, DYNAMIC ALLOCATION
// =============================================================================

namespace cpp20_features {

    // constexpr destructors
    struct Resource {
        constexpr Resource(int val) : value_(val) {
            std::cout << "Resource constructed with " << val << "\n";
        }

        constexpr ~Resource() {
            std::cout << "Resource destroyed\n";
        }

        constexpr int get_value() const { return value_; }

    private:
        int value_;
    };

    // constexpr dynamic allocation (new/delete)
    constexpr int test_dynamic_allocation() {
        int* ptr = new int(42);
        int value = *ptr;
        delete ptr;
        return value;
    }

    // constexpr virtual functions
    struct Base {
        constexpr virtual ~Base() = default;
        constexpr virtual int get_value() const = 0;
    };

    struct Derived : Base {
        constexpr Derived(int val) : value_(val) {}
        constexpr int get_value() const override { return value_; }

    private:
        int value_;
    };

    constexpr int test_virtual_functions() {
        Derived d(100);
        const Base& base = d;
        return base.get_value();
    }

    // constexpr std::vector operations
    constexpr int test_vector_operations() {
        std::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);

        int sum = 0;
        for (int val : vec) {
            sum += val;
        }
        return sum;
    }

    // constexpr algorithms
    constexpr int test_algorithms() {
        std::array<int, 5> arr{5, 2, 8, 1, 9};
        std::sort(arr.begin(), arr.end());
        return arr[0];  // Should be 1 (minimum)
    }

    void demonstrate_cpp20_constexpr() {
        std::cout << "\n=== C++20 constexpr Features ===\n";

        // constexpr dynamic allocation
        constexpr int dynamic_val = test_dynamic_allocation();
        std::cout << "Dynamic allocation result: " << dynamic_val << "\n";

        // constexpr virtual functions
        constexpr int virtual_val = test_virtual_functions();
        std::cout << "Virtual function result: " << virtual_val << "\n";

        // constexpr container operations
        constexpr int vector_sum = test_vector_operations();
        std::cout << "Vector sum: " << vector_sum << "\n";

        // constexpr algorithms
        constexpr int min_val = test_algorithms();
        std::cout << "Minimum value after sort: " << min_val << "\n";

        // constexpr destructors in action
        {
            constexpr auto test_destructor = []() {
                Resource r(999);
                return r.get_value();
            };
            constexpr int val = test_destructor();
            std::cout << "Resource value: " << val << "\n";
        }
    }
}

// =============================================================================
// 5. C++23 CONSTEXPR - ENHANCED STANDARD LIBRARY SUPPORT
// =============================================================================

namespace cpp23_features {

    // constexpr std::unique_ptr (C++23)
    constexpr int test_unique_ptr() {
        auto ptr = std::make_unique<int>(42);
        int value = *ptr;
        return value;
    }

    // More constexpr standard library functions
    constexpr bool test_string_operations() {
        std::string str = "Hello";
        str += " World";
        return str.size() == 11;
    }

    // constexpr optional and variant support enhanced
    constexpr int test_optional() {
        std::optional<int> opt = 42;
        return opt.value_or(0);
    }

    void demonstrate_cpp23_constexpr() {
        std::cout << "\n=== C++23 constexpr Features ===\n";

        // constexpr std::unique_ptr
        constexpr int unique_val = test_unique_ptr();
        std::cout << "unique_ptr value: " << unique_val << "\n";

        // Enhanced string operations
        constexpr bool string_result = test_string_operations();
        std::cout << "String operation result: " << string_result << "\n";

        // Optional support
        constexpr int opt_val = test_optional();
        std::cout << "Optional value: " << opt_val << "\n";
    }
}

// =============================================================================
// 6. CONSTEVAL - IMMEDIATE FUNCTIONS (C++20)
// =============================================================================

namespace consteval_examples {

    // consteval functions MUST be evaluated at compile-time
    consteval int must_be_compile_time(int x) {
        return x * x;
    }

    // Useful for compile-time checks and assertions
    consteval bool is_power_of_two(int n) {
        return n > 0 && (n & (n - 1)) == 0;
    }

    // consteval with templates
    template<int N>
    consteval int factorial_consteval() {
        static_assert(N >= 0, "Factorial requires non-negative number");
        if constexpr (N == 0) {
            return 1;
        } else {
            return N * factorial_consteval<N-1>();
        }
    }

    // Error demonstration (uncomment to see compilation error)
    /*
    void runtime_error_example() {
        int runtime_value = 5;
        // int result = must_be_compile_time(runtime_value);  // ERROR: not constant expression
    }
    */

    void demonstrate_consteval() {
        std::cout << "\n=== consteval Features ===\n";

        // These MUST be evaluated at compile-time
        constexpr int compile_time_square = must_be_compile_time(5);
        std::cout << "Compile-time square of 5: " << compile_time_square << "\n";

        constexpr bool is_16_power_of_2 = is_power_of_two(16);
        constexpr bool is_15_power_of_2 = is_power_of_two(15);
        std::cout << "Is 16 power of 2: " << is_16_power_of_2 << "\n";
        std::cout << "Is 15 power of 2: " << is_15_power_of_2 << "\n";

        constexpr int fact5 = factorial_consteval<5>();
        std::cout << "Factorial of 5: " << fact5 << "\n";
    }
}

// =============================================================================
// 7. CONSTINIT - COMPILE-TIME INITIALIZATION (C++20)
// =============================================================================

namespace constinit_examples {

    // constinit ensures compile-time initialization
    constinit int global_var = 42;

    // Can be combined with static
    constinit static int static_var = 100;

    // Thread-local with constinit
    constinit thread_local int thread_var = 200;

    // Function that returns a compile-time constant
    constexpr int get_initial_value() {
        return 999;
    }

    constinit int initialized_var = get_initial_value();

    // constinit with user-defined types
    struct Config {
        int value;
        const char* name;

        constexpr Config(int v, const char* n) : value(v), name(n) {}
    };

    constinit Config app_config{42, "MyApp"};

    // Error example (uncomment to see compilation error)
    /*
    int runtime_function() { return 42; }
    constinit int error_var = runtime_function();  // ERROR: not constant expression
    */

    void demonstrate_constinit() {
        std::cout << "\n=== constinit Features ===\n";

        std::cout << "Global var: " << global_var << "\n";
        std::cout << "Static var: " << static_var << "\n";
        std::cout << "Thread var: " << thread_var << "\n";
        std::cout << "Initialized var: " << initialized_var << "\n";

        std::cout << "Config: " << app_config.name << " = " << app_config.value << "\n";

        // constinit variables can be modified at runtime
        global_var = 84;
        std::cout << "Modified global var: " << global_var << "\n";
    }
}

// =============================================================================
// 8. COMPARISON: CONSTEXPR VS CONSTEVAL VS CONSTINIT
// =============================================================================

namespace comparison_examples {

    // constexpr: Can be evaluated at compile-time OR runtime
    constexpr int constexpr_func(int x) {
        return x * 2;
    }

    // consteval: MUST be evaluated at compile-time
    consteval int consteval_func(int x) {
        return x * 3;
    }

    // constinit: Ensures compile-time initialization of variables
    constinit int constinit_var = 42;

    void demonstrate_comparison() {
        std::cout << "\n=== constexpr vs consteval vs constinit Comparison ===\n";

        // constexpr usage
        constexpr int compile_time_result1 = constexpr_func(5);  // Compile-time
        int runtime_value = 5;
        int runtime_result1 = constexpr_func(runtime_value);     // Runtime

        std::cout << "constexpr function:\n";
        std::cout << "  Compile-time: " << compile_time_result1 << "\n";
        std::cout << "  Runtime: " << runtime_result1 << "\n";

        // consteval usage
        constexpr int compile_time_result2 = consteval_func(5);  // Must be compile-time
        // int runtime_result2 = consteval_func(runtime_value);  // ERROR: not allowed

        std::cout << "consteval function:\n";
        std::cout << "  Compile-time only: " << compile_time_result2 << "\n";

        // constinit usage
        std::cout << "constinit variable:\n";
        std::cout << "  Initial value: " << constinit_var << "\n";
        constinit_var = 84;  // Can be modified at runtime
        std::cout << "  Modified value: " << constinit_var << "\n";

        // Summary table
        std::cout << "\nSummary:\n";
        std::cout << "┌──────────────┬─────────────────┬─────────────────┬──────────────────────┐\n";
        std::cout << "│ Keyword      │ Compile-time    │ Runtime         │ Primary Use Case     │\n";
        std::cout << "├──────────────┼─────────────────┼─────────────────┼──────────────────────┤\n";
        std::cout << "│ constexpr    │ Possible        │ Possible        │ Flexible evaluation  │\n";
        std::cout << "│ consteval    │ Required        │ Not allowed     │ Forced compile-time  │\n";
        std::cout << "│ constinit    │ Required (init) │ Modifiable      │ Guaranteed init      │\n";
        std::cout << "└──────────────┴─────────────────┴─────────────────┴──────────────────────┘\n";
    }
}

// =============================================================================
// 9. PRACTICAL USE CASES AND BEST PRACTICES
// =============================================================================

namespace practical_examples {

    // 1. Compile-time configuration
    consteval int get_buffer_size() {
        #ifdef DEBUG
            return 1024;
        #else
            return 4096;
        #endif
    }

    constinit size_t BUFFER_SIZE = get_buffer_size();

    // 2. Compile-time string hashing
    consteval size_t string_hash(const char* str) {
        size_t hash = 5381;
        while (*str) {
            hash = ((hash << 5) + hash) + *str++;
        }
        return hash;
    }

    // 3. Type-safe enums with constexpr
    enum class Color { Red, Green, Blue };

    constexpr const char* color_to_string(Color c) {
        switch (c) {
            case Color::Red:   return "Red";
            case Color::Green: return "Green";
            case Color::Blue:  return "Blue";
        }
        return "Unknown";
    }

    // 4. Compile-time mathematical constants
    constexpr double calculate_pi(int iterations = 1000000) {
        double pi = 0.0;
        for (int i = 0; i < iterations; ++i) {
            double term = 1.0 / (2 * i + 1);
            if (i % 2 == 0) {
                pi += term;
            } else {
                pi -= term;
            }
        }
        return pi * 4.0;
    }

    constinit double PI_APPROXIMATION = calculate_pi(10000);

    // 5. Template metaprogramming helpers
    template<size_t N>
    consteval bool is_prime() {
        if constexpr (N < 2) return false;
        if constexpr (N == 2) return true;
        if constexpr (N % 2 == 0) return false;

        for (size_t i = 3; i * i <= N; i += 2) {
            if (N % i == 0) return false;
        }
        return true;
    }

    void demonstrate_practical_examples() {
        std::cout << "\n=== Practical Use Cases ===\n";

        std::cout << "Buffer size: " << BUFFER_SIZE << "\n";

        constexpr size_t hello_hash = string_hash("hello");
        constexpr size_t world_hash = string_hash("world");
        std::cout << "Hash of 'hello': " << hello_hash << "\n";
        std::cout << "Hash of 'world': " << world_hash << "\n";

        constexpr auto red_name = color_to_string(Color::Red);
        std::cout << "Color name: " << red_name << "\n";

        std::cout << "PI approximation: " << PI_APPROXIMATION << "\n";

        constexpr bool is_17_prime = is_prime<17>();
        constexpr bool is_18_prime = is_prime<18>();
        std::cout << "Is 17 prime: " << is_17_prime << "\n";
        std::cout << "Is 18 prime: " << is_18_prime << "\n";
    }
}

// =============================================================================
// 10. PERFORMANCE AND OPTIMIZATION CONSIDERATIONS
// =============================================================================

namespace performance_examples {

    // Benchmark helper
    template<typename F>
    void benchmark(const char* name, F&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << name << ": " << result << " (took " << duration.count() << " ns)\n";
    }

    // Compile-time vs runtime computation
    constexpr int expensive_calculation(int n) {
        int result = 0;
        for (int i = 0; i < n; ++i) {
            result += i * i;
        }
        return result;
    }

    void demonstrate_performance() {
        std::cout << "\n=== Performance Considerations ===\n";

        // Compile-time computation (zero runtime cost)
        constexpr int compile_time_result = expensive_calculation(1000);
        std::cout << "Compile-time result: " << compile_time_result << "\n";

        // Runtime computation for comparison
        int runtime_n = 1000;
        auto runtime_start = std::chrono::high_resolution_clock::now();
        int runtime_result = expensive_calculation(runtime_n);
        auto runtime_end = std::chrono::high_resolution_clock::now();
        auto runtime_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(runtime_end - runtime_start);

        std::cout << "Runtime result: " << runtime_result << " (took " << runtime_duration.count() << " ns)\n";
        std::cout << "Compile-time computation has ZERO runtime cost!\n";

        // Memory usage: constinit vs regular initialization
        std::cout << "\nMemory initialization:\n";
        std::cout << "constinit variables are initialized at program load, not first use\n";
        std::cout << "This can improve startup performance for frequently used globals\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATE ALL FEATURES
// =============================================================================

int main() {
    std::cout << "=== constexpr Evolution (C++11 to C++23) and Keyword Comparison ===\n";

    cpp11_features::demonstrate_cpp11_constexpr();
    cpp14_features::demonstrate_cpp14_constexpr();
    cpp17_features::demonstrate_cpp17_constexpr();
    cpp20_features::demonstrate_cpp20_constexpr();
    cpp23_features::demonstrate_cpp23_constexpr();

    consteval_examples::demonstrate_consteval();
    constinit_examples::demonstrate_constinit();
    comparison_examples::demonstrate_comparison();
    practical_examples::demonstrate_practical_examples();
    performance_examples::demonstrate_performance();

    std::cout << "\n=== Final Summary ===\n";
    std::cout << "Evolution Timeline:\n";
    std::cout << "C++11: Basic constexpr (single expression)\n";
    std::cout << "C++14: Relaxed constexpr (loops, multiple statements)\n";
    std::cout << "C++17: constexpr if, constexpr lambdas\n";
    std::cout << "C++20: constexpr destructors, virtual functions, new/delete, consteval, constinit\n";
    std::cout << "C++23: constexpr std::unique_ptr, enhanced standard library\n\n";

    std::cout << "Keyword Usage Guidelines:\n";
    std::cout << "• Use constexpr for functions that CAN be compile-time evaluated\n";
    std::cout << "• Use consteval for functions that MUST be compile-time evaluated\n";
    std::cout << "• Use constinit for global/static variables requiring compile-time initialization\n";
    std::cout << "• constexpr provides flexibility, consteval provides guarantees\n";
    std::cout << "• constinit prevents static initialization order fiasco\n";

    return 0;
}

/*
 * COMPILATION NOTES:
 *
 * For full C++23 features: g++ -std=c++23 -Wall -Wextra constexpr_evolution_and_comparison.cpp
 * For C++20 features: g++ -std=c++20 -Wall -Wextra constexpr_evolution_and_comparison.cpp
 * For earlier standards: g++ -std=c++17 -Wall -Wextra constexpr_evolution_and_comparison.cpp
 *
 * Key Insights:
 *
 * 1. constexpr Evolution:
 *    - Started simple in C++11, became very powerful by C++20
 *    - Each version added more flexibility and capabilities
 *    - C++20 was a major milestone with dynamic allocation support
 *
 * 2. Performance Benefits:
 *    - Compile-time evaluation eliminates runtime computation cost
 *    - Enables optimization opportunities for the compiler
 *    - Reduces binary size for constant expressions
 *
 * 3. Best Practices:
 *    - Prefer constexpr for maximum flexibility
 *    - Use consteval when you need compile-time guarantees
 *    - Use constinit for safe global variable initialization
 *    - Combine with templates for powerful metaprogramming
 */
