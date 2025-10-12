// =============================
// LAMBDA EXPRESSIONS USE CASES (C++11+)
// =============================
/*
LAMBDA EXPRESSION SYNTAX AND RULES:

1. BASIC SYNTAX: [capture](parameters) -> return_type { body }
   - [capture]: capture clause (how to access external variables)
   - (parameters): parameter list (optional if no parameters)
   - -> return_type: trailing return type (optional, auto-deduced)
   - { body }: function body

2. CAPTURE MODES:
   - []           // Capture nothing
   - [=]          // Capture all by copy
   - [&]          // Capture all by reference
   - [var]        // Capture 'var' by copy
   - [&var]       // Capture 'var' by reference
   - [=, &var]    // Capture all by copy, 'var' by reference
   - [&, var]     // Capture all by reference, 'var' by copy
   - [this]       // Capture current object by reference
   - [*this]      // Capture current object by copy (C++17+)

3. PARAMETER VARIATIONS:
   - []() { ... }                    // No parameters
   - [](int x) { ... }               // Single parameter
   - [](int x, double y) { ... }     // Multiple parameters
   - [](auto x) { ... }              // Generic lambda (C++14+)
   - [](auto&&... args) { ... }      // Variadic generic lambda

4. RETURN TYPE:
   - [](int x) { return x * 2; }     // Auto-deduced return type
   - [](int x) -> int { return x * 2; } // Explicit return type
   - [](int x) -> decltype(x * 2) { return x * 2; } // Decltype return

5. MUTABLE LAMBDAS:
   - [var](int x) mutable { var++; return var + x; } // Can modify captured copies

6. LAMBDA EVOLUTION:
   - C++11: Basic lambdas
   - C++14: Generic lambdas, init capture, auto parameters
   - C++17: constexpr lambdas, *this capture
   - C++20: Template lambdas, pack expansion in capture
*/

// =============================
// COMPREHENSIVE LAMBDA EXAMPLES
// =============================

#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <numeric>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <future>
#include <thread>

// 1. BASIC LAMBDA SYNTAX AND USAGE
void basic_lambda_examples() {
    std::cout << "=== BASIC LAMBDA SYNTAX AND USAGE ===\n";

    // Simplest lambda - no capture, no parameters
    auto simple_lambda = []() {
        std::cout << "Hello from lambda!\n";
    };
    simple_lambda();

    // Lambda with parameters
    auto add = [](int a, int b) {
        return a + b;
    };
    std::cout << "5 + 3 = " << add(5, 3) << "\n";

    // Lambda with explicit return type
    auto divide = [](double a, double b) -> double {
        return b != 0 ? a / b : 0.0;
    };
    std::cout << "10.0 / 3.0 = " << divide(10.0, 3.0) << "\n";

    // Lambda without parentheses (no parameters)
    auto get_random = [] {
        return rand() % 100;
    };
    std::cout << "Random number: " << get_random() << "\n";

    // Immediately invoked lambda
    int result = [](int x) { return x * x; }(5);
    std::cout << "Square of 5: " << result << "\n";

    std::cout << "\n";
}

// 2. CAPTURE MODES COMPREHENSIVE EXAMPLES
void capture_modes_examples() {
    std::cout << "=== CAPTURE MODES EXAMPLES ===\n";

    int x = 10;
    int y = 20;
    std::string msg = "Hello";

    // No capture
    auto no_capture = []() {
        return 42;
        // Cannot access x, y, or msg
    };
    std::cout << "No capture: " << no_capture() << "\n";

    // Capture by copy
    auto capture_by_copy = [x, y]() {
        return x + y;  // x and y are copied
        // x++; // Error: cannot modify copied values (unless mutable)
    };
    std::cout << "Capture by copy: " << capture_by_copy() << "\n";

    // Capture by reference
    auto capture_by_ref = [&x, &y]() {
        x += 5;  // Modifies original x
        y += 5;  // Modifies original y
        return x + y;
    };
    std::cout << "Before ref capture: x=" << x << ", y=" << y << "\n";
    std::cout << "Capture by reference: " << capture_by_ref() << "\n";
    std::cout << "After ref capture: x=" << x << ", y=" << y << "\n";

    // Reset values
    x = 10; y = 20;

    // Capture all by copy
    auto capture_all_copy = [=]() {
        return x + y + static_cast<int>(msg.length());
    };
    std::cout << "Capture all by copy: " << capture_all_copy() << "\n";

    // Capture all by reference
    auto capture_all_ref = [&]() {
        x *= 2;
        y *= 2;
        msg += " World";
        return x + y;
    };
    std::cout << "Before all ref: x=" << x << ", y=" << y << ", msg='" << msg << "'\n";
    std::cout << "Capture all by reference: " << capture_all_ref() << "\n";
    std::cout << "After all ref: x=" << x << ", y=" << y << ", msg='" << msg << "'\n";

    // Reset values
    x = 10; y = 20; msg = "Hello";

    // Mixed capture modes
    auto mixed_capture = [=, &msg](int multiplier) {
        msg += " Modified";  // Reference capture
        return (x + y) * multiplier;  // x, y captured by copy
    };
    std::cout << "Mixed capture: " << mixed_capture(3) << "\n";
    std::cout << "Message after mixed: '" << msg << "'\n";

    std::cout << "\n";
}

// 3. MUTABLE LAMBDAS
void mutable_lambda_examples() {
    std::cout << "=== MUTABLE LAMBDA EXAMPLES ===\n";

    int counter = 0;

    // Mutable lambda - can modify captured copies
    auto mutable_counter = [counter](int increment) mutable {
        counter += increment;  // Modifies the copy, not original
        return counter;
    };

    std::cout << "Original counter: " << counter << "\n";
    std::cout << "Mutable lambda call 1: " << mutable_counter(5) << "\n";
    std::cout << "Mutable lambda call 2: " << mutable_counter(3) << "\n";
    std::cout << "Original counter after calls: " << counter << "\n";

    // Creating a stateful lambda
    auto accumulator = [sum = 0](int value) mutable {
        sum += value;
        return sum;
    };

    std::cout << "\nStateful accumulator:\n";
    std::cout << "Add 10: " << accumulator(10) << "\n";
    std::cout << "Add 20: " << accumulator(20) << "\n";
    std::cout << "Add 5: " << accumulator(5) << "\n";

    // Comparison with reference capture
    int total = 0;
    auto ref_accumulator = [&total](int value) {
        total += value;
        return total;
    };

    std::cout << "\nReference accumulator:\n";
    std::cout << "Add 10: " << ref_accumulator(10) << "\n";
    std::cout << "Add 20: " << ref_accumulator(20) << "\n";
    std::cout << "Total variable: " << total << "\n";

    std::cout << "\n";
}

// 4. GENERIC LAMBDAS (C++14+)
void generic_lambda_examples() {
    std::cout << "=== GENERIC LAMBDA EXAMPLES (C++14+) ===\n";

    // Generic lambda with auto parameters
    auto generic_add = [](auto a, auto b) {
        return a + b;
    };

    std::cout << "Generic add (int): " << generic_add(5, 3) << "\n";
    std::cout << "Generic add (double): " << generic_add(2.5, 3.7) << "\n";
    std::cout << "Generic add (string): " << generic_add(std::string("Hello"), std::string(" World")) << "\n";

    // Generic lambda with type constraints
    auto generic_multiply = [](auto a, auto b) -> decltype(a * b) {
        return a * b;
    };

    std::cout << "Generic multiply: " << generic_multiply(4, 2.5) << "\n";

    // Variadic generic lambda
    auto variadic_sum = [](auto... args) {
        return (args + ...);  // C++17 fold expression
    };

    std::cout << "Variadic sum: " << variadic_sum(1, 2, 3, 4, 5) << "\n";
    std::cout << "Variadic sum (mixed): " << variadic_sum(1.5, 2, 3.7) << "\n";

    // Generic lambda for container operations
    auto print_container = [](const auto& container) {
        std::cout << "Container contents: ";
        for (const auto& item : container) {
            std::cout << item << " ";
        }
        std::cout << "\n";
    };

    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::vector<std::string> strings = {"hello", "world", "lambda"};

    print_container(vec);
    print_container(strings);

    std::cout << "\n";
}

// 5. LAMBDAS WITH STL ALGORITHMS
void lambdas_with_stl_algorithms() {
    std::cout << "=== LAMBDAS WITH STL ALGORITHMS ===\n";

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // std::for_each
    std::cout << "Original numbers: ";
    std::for_each(numbers.begin(), numbers.end(), [](int n) {
        std::cout << n << " ";
    });
    std::cout << "\n";

    // std::transform
    std::vector<int> squared;
    std::transform(numbers.begin(), numbers.end(), std::back_inserter(squared),
                   [](int n) { return n * n; });

    std::cout << "Squared numbers: ";
    for (int n : squared) std::cout << n << " ";
    std::cout << "\n";

    // std::find_if
    auto it = std::find_if(numbers.begin(), numbers.end(),
                          [](int n) { return n > 5; });
    if (it != numbers.end()) {
        std::cout << "First number > 5: " << *it << "\n";
    }

    // std::count_if
    int even_count = std::count_if(numbers.begin(), numbers.end(),
                                  [](int n) { return n % 2 == 0; });
    std::cout << "Even numbers count: " << even_count << "\n";

    // std::remove_if
    std::vector<int> filtered = numbers;
    filtered.erase(
        std::remove_if(filtered.begin(), filtered.end(),
                      [](int n) { return n % 3 == 0; }),
        filtered.end()
    );

    std::cout << "Numbers not divisible by 3: ";
    for (int n : filtered) std::cout << n << " ";
    std::cout << "\n";

    // std::sort with custom comparator
    std::vector<std::string> words = {"banana", "apple", "cherry", "date"};
    std::sort(words.begin(), words.end(),
              [](const std::string& a, const std::string& b) {
                  return a.length() < b.length();
              });

    std::cout << "Words sorted by length: ";
    for (const auto& word : words) std::cout << word << " ";
    std::cout << "\n";

    // std::accumulate with lambda
    int sum = std::accumulate(numbers.begin(), numbers.end(), 0,
                             [](int acc, int n) { return acc + n * n; });
    std::cout << "Sum of squares: " << sum << "\n";

    std::cout << "\n";
}

// 6. INIT CAPTURE (C++14+)
void init_capture_examples() {
    std::cout << "=== INIT CAPTURE EXAMPLES (C++14+) ===\n";

    // Init capture - move semantics
    auto unique_ptr = std::make_unique<int>(42);
    auto lambda_with_move = [ptr = std::move(unique_ptr)](int x) {
        return *ptr + x;
    };

    std::cout << "Lambda with moved unique_ptr: " << lambda_with_move(8) << "\n";
    // unique_ptr is now nullptr

    // Init capture - computed values
    auto lambda_with_computed = [factorial = [](int n) {
        int result = 1;
        for (int i = 1; i <= n; ++i) result *= i;
        return result;
    }(5)](int x) {
        return factorial + x;
    };

    std::cout << "Lambda with computed factorial: " << lambda_with_computed(10) << "\n";

    // Init capture - complex initialization
    std::vector<int> vec = {1, 2, 3, 4, 5};
    auto lambda_with_copy = [vec_copy = vec, size = vec.size()](int index) {
        return index < size ? vec_copy[index] : -1;
    };

    std::cout << "Lambda with copied vector: " << lambda_with_copy(2) << "\n";

    // Init capture with reference
    int counter = 0;
    auto incrementer = [&counter, step = 5]() {
        counter += step;
        return counter;
    };

    std::cout << "Incrementer calls: ";
    std::cout << incrementer() << " " << incrementer() << " " << incrementer() << "\n";

    std::cout << "\n";
}

// 7. LAMBDA IN CLASS CONTEXT
class LambdaInClass {
private:
    int value = 100;
    std::string name = "LambdaClass";

public:
    void demonstrate_this_capture() {
        std::cout << "=== THIS CAPTURE IN CLASS CONTEXT ===\n";

        // Capture this by reference
        auto lambda_this_ref = [this](int x) {
            return this->value + x;  // Access member through this
        };

        std::cout << "Lambda with this capture: " << lambda_this_ref(50) << "\n";

        // Modify member through lambda
        auto modify_member = [this](int new_value) {
            this->value = new_value;
            this->name += "_modified";
        };

        modify_member(200);
        std::cout << "After modification - value: " << value << ", name: " << name << "\n";

        // C++17: Capture *this (copy of entire object)
        auto lambda_this_copy = [*this](int x) mutable {
            value += x;  // Modifies copy, not original
            name += "_copy";
            return value;
        };

        std::cout << "Lambda with *this copy: " << lambda_this_copy(25) << "\n";
        std::cout << "Original after *this copy - value: " << value << ", name: " << name << "\n";

        std::cout << "\n";
    }

    // Member function returning lambda
    auto get_multiplier(int factor) {
        return [this, factor](int x) {
            return x * factor + this->value;
        };
    }

    // Lambda as member variable
    std::function<int(int)> member_lambda = [this](int x) {
        return x + value;
    };
};

void class_context_examples() {
    LambdaInClass obj;
    obj.demonstrate_this_capture();

    // Using member function that returns lambda
    auto multiplier = obj.get_multiplier(3);
    std::cout << "Member function lambda: " << multiplier(10) << "\n";

    // Using lambda member variable
    std::cout << "Member lambda variable: " << obj.member_lambda(15) << "\n";

    std::cout << "\n";
}

// 8. RECURSIVE LAMBDAS
void recursive_lambda_examples() {
    std::cout << "=== RECURSIVE LAMBDA EXAMPLES ===\n";

    // Recursive lambda using std::function
    std::function<int(int)> factorial = [&factorial](int n) -> int {
        return n <= 1 ? 1 : n * factorial(n - 1);
    };

    std::cout << "Factorial of 5: " << factorial(5) << "\n";

    // Recursive lambda for Fibonacci
    std::function<long long(int)> fibonacci = [&fibonacci](int n) -> long long {
        return n <= 1 ? n : fibonacci(n - 1) + fibonacci(n - 2);
    };

    std::cout << "Fibonacci of 10: " << fibonacci(10) << "\n";

    // Y-combinator style (advanced)
    auto y_combinator = [](auto f) {
        return [f](auto&&... args) {
            return f(f, std::forward<decltype(args)>(args)...);
        };
    };

    auto gcd = y_combinator([](auto self, int a, int b) -> int {
        return b == 0 ? a : self(b, a % b);
    });

    std::cout << "GCD of 48 and 18: " << gcd(48, 18) << "\n";

    std::cout << "\n";
}

// 9. LAMBDA WITH PERFECT FORWARDING
void perfect_forwarding_examples() {
    std::cout << "=== PERFECT FORWARDING EXAMPLES ===\n";

    // Perfect forwarding lambda
    auto perfect_forwarder = [](auto&& func, auto&&... args) {
        return std::invoke(std::forward<decltype(func)>(func),
                          std::forward<decltype(args)>(args)...);
    };

    // Test functions
    auto add_func = [](int a, int b) { return a + b; };
    auto print_func = [](const std::string& msg) { std::cout << msg << "\n"; };

    std::cout << "Perfect forwarding add: " << perfect_forwarder(add_func, 5, 3) << "\n";
    perfect_forwarder(print_func, "Hello from perfect forwarding!");

    // Lambda factory with perfect forwarding
    auto make_logger = [](auto prefix) {
        return [prefix = std::move(prefix)](auto&&... args) {
            std::cout << prefix << ": ";
            ((std::cout << std::forward<decltype(args)>(args) << " "), ...);
            std::cout << "\n";
        };
    };

    auto info_logger = make_logger("INFO");
    auto error_logger = make_logger("ERROR");

    info_logger("Lambda", "logging", "system");
    error_logger("Error", "code:", 404);

    std::cout << "\n";
}

// 10. LAMBDA PERFORMANCE AND OPTIMIZATION
void performance_examples() {
    std::cout << "=== LAMBDA PERFORMANCE EXAMPLES ===\n";

    const size_t size = 1000000;
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), 1);

    // Lambda vs function pointer performance
    auto lambda_square = [](int x) { return x * x; };
    int (*func_ptr)(int) = [](int x) { return x * x; };

    // Transform using lambda (typically inlined)
    std::vector<int> result1(size);
    std::transform(data.begin(), data.end(), result1.begin(), lambda_square);

    // Transform using function pointer (may not be inlined)
    std::vector<int> result2(size);
    std::transform(data.begin(), data.end(), result2.begin(), func_ptr);

    std::cout << "Lambda and function pointer transformations completed\n";
    std::cout << "Lambdas are typically optimized better by compilers\n";

    // Capture optimization - avoid unnecessary captures
    int multiplier = 2;

    // Good: capture only what's needed
    auto good_lambda = [multiplier](int x) { return x * multiplier; };

    // Bad: capture everything unnecessarily
    auto bad_lambda = [=](int x) { return x * multiplier; };

    std::cout << "Prefer specific captures over capture-all for better performance\n";

    // Stateless lambda can convert to function pointer
    auto stateless = [](int x, int y) { return x + y; };
    int (*converted_ptr)(int, int) = stateless;  // Valid conversion

    std::cout << "Stateless lambdas can convert to function pointers\n";

    std::cout << "\n";
}

// 11. ADVANCED LAMBDA PATTERNS
void advanced_lambda_patterns() {
    std::cout << "=== ADVANCED LAMBDA PATTERNS ===\n";

    // RAII lambda
    auto raii_lambda = [](auto resource, auto deleter) {
        return [resource = std::move(resource), deleter = std::move(deleter)](auto&& func) mutable {
            auto cleanup = [&]() { deleter(resource); };
            try {
                return func(resource);
            } catch (...) {
                cleanup();
                throw;
            }
        };
    };

    // Lambda composition
    auto compose = [](auto f, auto g) {
        return [f = std::move(f), g = std::move(g)](auto&& x) {
            return f(g(std::forward<decltype(x)>(x)));
        };
    };

    auto add_one = [](int x) { return x + 1; };
    auto multiply_two = [](int x) { return x * 2; };
    auto composed = compose(add_one, multiply_two);

    std::cout << "Composed function (add_one . multiply_two)(5): " << composed(5) << "\n";

    // Curry pattern
    auto curry = [](auto f) {
        return [f](auto&& first) {
            return [f, first = std::forward<decltype(first)>(first)](auto&&... rest) {
                return f(first, std::forward<decltype(rest)>(rest)...);
            };
        };
    };

    auto add3 = [](int a, int b, int c) { return a + b + c; };
    auto curried_add = curry(add3);
    auto add_with_5 = curried_add(5);

    std::cout << "Curried addition 5 + 3 + 2: " << add_with_5(3, 2) << "\n";

    // Memoization pattern
    auto memoize = [](auto func) {
        return [func, cache = std::map<int, int>{}](int n) mutable {
            auto it = cache.find(n);
            if (it != cache.end()) {
                return it->second;
            }
            auto result = func(func, n);
            cache[n] = result;
            return result;
        };
    };

    auto fib_func = [](auto self, int n) -> int {
        return n <= 1 ? n : self(self, n-1) + self(self, n-2);
    };

    auto memoized_fib = memoize(fib_func);
    std::cout << "Memoized Fibonacci(20): " << memoized_fib(20) << "\n";

    std::cout << "\n";
}

// 12. LAMBDA PITFALLS AND BEST PRACTICES
void lambda_pitfalls_and_best_practices() {
    std::cout << "=== LAMBDA PITFALLS AND BEST PRACTICES ===\n";

    std::cout << "COMMON PITFALLS:\n";

    // Pitfall 1: Dangling references
    std::cout << "1. Dangling references:\n";
    std::function<int()> dangerous_lambda;
    {
        int local_var = 42;
        dangerous_lambda = [&local_var]() { return local_var; };
        // local_var goes out of scope here!
    }
    // dangerous_lambda() would be undefined behavior
    std::cout << "   Avoid capturing local variables by reference that outlive the lambda\n";

    // Pitfall 2: Capture by copy vs reference
    std::cout << "\n2. Unexpected capture behavior:\n";
    std::vector<std::function<int()>> lambdas;
    for (int i = 0; i < 3; ++i) {
        // Wrong: captures i by reference - all lambdas see final value
        // lambdas.push_back([&i]() { return i; });

        // Correct: capture by copy or init capture
        lambdas.push_back([i]() { return i; });
    }

    std::cout << "   Captured values: ";
    for (auto& lambda : lambdas) {
        std::cout << lambda() << " ";
    }
    std::cout << "\n";

    // Pitfall 3: Implicit this capture
    std::cout << "\n3. Implicit this capture in member functions:\n";
    std::cout << "   Always be explicit about capturing 'this' or '*this'\n";

    std::cout << "\nBEST PRACTICES:\n";
    std::cout << "1. Prefer specific captures over capture-all ([=] or [&])\n";
    std::cout << "2. Use const auto& for read-only parameters\n";
    std::cout << "3. Consider std::function overhead for simple lambdas\n";
    std::cout << "4. Use init capture for move semantics (C++14+)\n";
    std::cout << "5. Be explicit about mutable when modifying captures\n";
    std::cout << "6. Use generic lambdas for flexibility (C++14+)\n";
    std::cout << "7. Consider lambda lifetime vs captured references\n";

    std::cout << "\n";
}

int main() {
    basic_lambda_examples();
    capture_modes_examples();
    mutable_lambda_examples();
    generic_lambda_examples();
    lambdas_with_stl_algorithms();
    init_capture_examples();
    class_context_examples();
    recursive_lambda_examples();
    perfect_forwarding_examples();
    performance_examples();
    advanced_lambda_patterns();
    lambda_pitfalls_and_best_practices();

    std::cout << "=== LAMBDA EXPRESSIONS SUMMARY ===\n";
    std::cout << "SYNTAX: [capture](parameters) -> return_type { body }\n\n";

    std::cout << "CAPTURE MODES:\n";
    std::cout << "- []           // No capture\n";
    std::cout << "- [=]          // Capture all by copy\n";
    std::cout << "- [&]          // Capture all by reference\n";
    std::cout << "- [var]        // Capture 'var' by copy\n";
    std::cout << "- [&var]       // Capture 'var' by reference\n";
    std::cout << "- [=, &var]    // Mixed capture modes\n";
    std::cout << "- [this]       // Capture current object\n";
    std::cout << "- [*this]      // Capture object by copy (C++17+)\n";
    std::cout << "- [var = expr] // Init capture (C++14+)\n\n";

    std::cout << "KEY FEATURES BY VERSION:\n";
    std::cout << "- C++11: Basic lambdas, capture modes\n";
    std::cout << "- C++14: Generic lambdas (auto), init capture\n";
    std::cout << "- C++17: constexpr lambdas, *this capture\n";
    std::cout << "- C++20: Template lambdas, pack expansion\n\n";

    std::cout << "COMMON USE CASES:\n";
    std::cout << "- STL algorithms (transform, find_if, sort)\n";
    std::cout << "- Event handling and callbacks\n";
    std::cout << "- Functional programming patterns\n";
    std::cout << "- Custom comparators and predicates\n";
    std::cout << "- Async operations and threading\n";
    std::cout << "- RAII and resource management\n";

    return 0;
}
