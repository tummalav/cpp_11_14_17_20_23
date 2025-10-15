
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <functional>
#include <algorithm>
#include <tuple>
#include <array>
#include <unordered_map>
#include <initializer_list>
#include <type_traits>
#include <atomic>
#include <regex>
#include <optional>
#include <variant>
#include <string_view>
#include <filesystem>
#include <execution>

/*
 * ============================================================================
 * COMPREHENSIVE C++ STANDARDS FEATURES OVERVIEW
 * C++11, C++14, C++17, C++20, C++23 Features List with Examples
 * ============================================================================
 */

// =============================================================================
// C++11 FEATURES (ISO/IEC 14882:2011)
// =============================================================================

namespace cpp11_features {

    void demonstrate_auto() {
        std::cout << "\n--- C++11: auto keyword ---\n";
        auto x = 42;              // int
        auto y = 3.14;           // double
        auto z = "hello";        // const char*

        std::vector<int> vec = {1, 2, 3};
        auto it = vec.begin();   // std::vector<int>::iterator

        std::cout << "auto deduced types work seamlessly\n";
    }

    void demonstrate_range_based_for() {
        std::cout << "\n--- C++11: Range-based for loops ---\n";
        std::vector<int> numbers = {1, 2, 3, 4, 5};

        for (const auto& num : numbers) {
            std::cout << num << " ";
        }
        std::cout << "\n";
    }

    void demonstrate_lambda_expressions() {
        std::cout << "\n--- C++11: Lambda expressions ---\n";
        auto lambda = [](int x, int y) { return x + y; };

        std::vector<int> vec = {3, 1, 4, 1, 5};
        std::sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

        std::cout << "Lambda result: " << lambda(5, 3) << "\n";
    }

    void demonstrate_smart_pointers() {
        std::cout << "\n--- C++11: Smart pointers ---\n";
        auto unique = std::make_unique<int>(42);    // C++14 feature used here
        auto shared = std::shared_ptr<int>(new int(100));
        std::weak_ptr<int> weak = shared;

        std::cout << "Smart pointers manage memory automatically\n";
    }

    void demonstrate_move_semantics() {
        std::cout << "\n--- C++11: Move semantics ---\n";
        std::vector<int> source = {1, 2, 3, 4, 5};
        std::vector<int> destination = std::move(source);

        std::cout << "Move semantics enable efficient transfers\n";
        std::cout << "Source size after move: " << source.size() << "\n";
        std::cout << "Destination size: " << destination.size() << "\n";
    }

    void demonstrate_uniform_initialization() {
        std::cout << "\n--- C++11: Uniform initialization ---\n";
        int x{42};
        std::vector<int> vec{1, 2, 3, 4, 5};
        std::string str{"Hello, World!"};

        struct Point { int x, y; };
        Point p{10, 20};

        std::cout << "Uniform initialization syntax works everywhere\n";
    }

    void demonstrate_nullptr() {
        std::cout << "\n--- C++11: nullptr ---\n";
        int* ptr = nullptr;  // Type-safe null pointer

        if (ptr == nullptr) {
            std::cout << "nullptr is type-safe replacement for NULL\n";
        }
    }

    void demonstrate_enum_class() {
        std::cout << "\n--- C++11: Scoped enums (enum class) ---\n";
        enum class Color { RED, GREEN, BLUE };
        enum class Size { SMALL, MEDIUM, LARGE };

        Color c = Color::RED;
        // Size s = Size::RED;  // Error: no implicit conversion

        std::cout << "Scoped enums prevent naming conflicts\n";
    }

    void demonstrate_constexpr() {
        std::cout << "\n--- C++11: constexpr ---\n";
        constexpr int square(int x) { return x * x; }
        constexpr int result = square(5);  // Computed at compile time

        std::cout << "constexpr enables compile-time computation: " << result << "\n";
    }

    void demonstrate_threading() {
        std::cout << "\n--- C++11: Threading support ---\n";

        auto task = []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return 42;
        };

        std::thread t([](){ std::cout << "Thread executed\n"; });
        t.join();

        std::future<int> future = std::async(std::launch::async, task);
        std::cout << "Async result: " << future.get() << "\n";
    }

    void demonstrate() {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "C++11 FEATURES DEMONSTRATION\n";
        std::cout << std::string(80, '=') << "\n";

        demonstrate_auto();
        demonstrate_range_based_for();
        demonstrate_lambda_expressions();
        demonstrate_smart_pointers();
        demonstrate_move_semantics();
        demonstrate_uniform_initialization();
        demonstrate_nullptr();
        demonstrate_enum_class();
        demonstrate_constexpr();
        demonstrate_threading();

        std::cout << "\n=== C++11 Complete Feature List ===\n";
        std::cout << "Core Language Features:\n";
        std::cout << "• auto keyword for type deduction\n";
        std::cout << "• decltype operator\n";
        std::cout << "• Range-based for loops\n";
        std::cout << "• Lambda expressions\n";
        std::cout << "• Rvalue references and move semantics\n";
        std::cout << "• Uniform initialization (braced-init-list)\n";
        std::cout << "• nullptr keyword\n";
        std::cout << "• Scoped enums (enum class)\n";
        std::cout << "• constexpr keyword\n";
        std::cout << "• static_assert\n";
        std::cout << "• Template aliases\n";
        std::cout << "• Variadic templates\n";
        std::cout << "• Default and deleted functions\n";
        std::cout << "• Override and final specifiers\n";
        std::cout << "• Delegating constructors\n";
        std::cout << "• Inheriting constructors\n";
        std::cout << "• Extended friend declarations\n";
        std::cout << "• Explicit conversion operators\n";
        std::cout << "• Raw string literals\n";
        std::cout << "• Unicode string literals\n";
        std::cout << "• User-defined literals\n";
        std::cout << "• Attributes [[noreturn]]\n";
        std::cout << "• Alignment support (alignas, alignof)\n";
        std::cout << "• Thread-local storage\n";

        std::cout << "\nStandard Library Features:\n";
        std::cout << "• Smart pointers (unique_ptr, shared_ptr, weak_ptr)\n";
        std::cout << "• Threading library (thread, mutex, condition_variable)\n";
        std::cout << "• Atomic operations library\n";
        std::cout << "• Time utilities (chrono)\n";
        std::cout << "• Random number generation\n";
        std::cout << "• Regular expressions\n";
        std::cout << "• Function objects (std::function, std::bind)\n";
        std::cout << "• Type traits\n";
        std::cout << "• Arrays (std::array)\n";
        std::cout << "• Unordered containers (unordered_map, unordered_set)\n";
        std::cout << "• Tuple\n";
        std::cout << "• initializer_list\n";
        std::cout << "• Forward list\n";
        std::cout << "• emplace methods\n";
        std::cout << "• Algorithm improvements\n";
    }
}

// =============================================================================
// C++14 FEATURES (ISO/IEC 14882:2014)
// =============================================================================

namespace cpp14_features {

    void demonstrate_generic_lambdas() {
        std::cout << "\n--- C++14: Generic lambdas ---\n";
        auto generic_lambda = [](auto x, auto y) { return x + y; };

        std::cout << "Generic lambda with ints: " << generic_lambda(5, 3) << "\n";
        std::cout << "Generic lambda with doubles: " << generic_lambda(2.5, 1.5) << "\n";
        std::cout << "Generic lambda with strings: " << generic_lambda(std::string("Hello"), std::string(" World")) << "\n";
    }

    void demonstrate_auto_return_type() {
        std::cout << "\n--- C++14: Auto return type deduction ---\n";

        auto multiply = [](int x, int y) {
            return x * y;  // Return type deduced as int
        };

        auto get_value = []() {
            if (true) return 42;
            else return 0;  // Both returns must have same type
        };

        std::cout << "Auto return type: " << multiply(6, 7) << "\n";
    }

    void demonstrate_variable_templates() {
        std::cout << "\n--- C++14: Variable templates ---\n";

        template<typename T>
        constexpr T pi = T(3.14159265358979323846);

        std::cout << "pi<float>: " << pi<float> << "\n";
        std::cout << "pi<double>: " << pi<double> << "\n";
    }

    void demonstrate_binary_literals() {
        std::cout << "\n--- C++14: Binary literals and digit separators ---\n";

        auto binary = 0b1010101;           // Binary literal
        auto hex = 0xFF'FF'FF'FF;          // Digit separators
        auto decimal = 1'000'000;          // Digit separators

        std::cout << "Binary 0b1010101 = " << binary << "\n";
        std::cout << "Hex with separators = " << hex << "\n";
        std::cout << "Decimal with separators = " << decimal << "\n";
    }

    void demonstrate_make_unique() {
        std::cout << "\n--- C++14: std::make_unique ---\n";

        auto ptr = std::make_unique<int>(42);
        auto arr = std::make_unique<int[]>(10);

        std::cout << "make_unique provides exception-safe object creation\n";
        std::cout << "Value: " << *ptr << "\n";
    }

    void demonstrate() {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "C++14 FEATURES DEMONSTRATION\n";
        std::cout << std::string(80, '=') << "\n";

        demonstrate_generic_lambdas();
        demonstrate_auto_return_type();
        demonstrate_variable_templates();
        demonstrate_binary_literals();
        demonstrate_make_unique();

        std::cout << "\n=== C++14 Complete Feature List ===\n";
        std::cout << "Core Language Features:\n";
        std::cout << "• Generic lambdas (auto parameters)\n";
        std::cout << "• Lambda capture initializers\n";
        std::cout << "• Auto return type deduction for functions\n";
        std::cout << "• decltype(auto)\n";
        std::cout << "• Variable templates\n";
        std::cout << "• Extended constexpr\n";
        std::cout << "• Binary literals (0b prefix)\n";
        std::cout << "• Digit separators (apostrophe)\n";
        std::cout << "• [[deprecated]] attribute\n";
        std::cout << "• Sized deallocation\n";

        std::cout << "\nStandard Library Features:\n";
        std::cout << "• std::make_unique\n";
        std::cout << "• std::shared_timed_mutex\n";
        std::cout << "• std::integer_sequence\n";
        std::cout << "• std::exchange\n";
        std::cout << "• std::quoted\n";
        std::cout << "• User-defined literals for standard library types\n";
        std::cout << "• Improved type traits\n";
        std::cout << "• std::equal, std::mismatch with 4 iterator overloads\n";
    }
}

// =============================================================================
// C++17 FEATURES (ISO/IEC 14882:2017)
// =============================================================================

namespace cpp17_features {

    void demonstrate_structured_bindings() {
        std::cout << "\n--- C++17: Structured bindings ---\n";

        std::tuple<int, std::string, double> data{42, "hello", 3.14};
        auto [id, name, value] = data;

        std::cout << "Structured binding: " << id << ", " << name << ", " << value << "\n";

        std::pair<int, std::string> p{100, "world"};
        auto [first, second] = p;
        std::cout << "Pair binding: " << first << ", " << second << "\n";
    }

    void demonstrate_if_constexpr() {
        std::cout << "\n--- C++17: if constexpr ---\n";

        auto process = [](auto value) {
            if constexpr (std::is_integral_v<decltype(value)>) {
                std::cout << "Processing integer: " << value << "\n";
            } else if constexpr (std::is_floating_point_v<decltype(value)>) {
                std::cout << "Processing float: " << value << "\n";
            } else {
                std::cout << "Processing other type\n";
            }
        };

        process(42);
        process(3.14);
        process("hello");
    }

    void demonstrate_optional() {
        std::cout << "\n--- C++17: std::optional ---\n";

        auto find_value = [](bool found) -> std::optional<int> {
            if (found) return 42;
            return std::nullopt;
        };

        if (auto result = find_value(true); result.has_value()) {
            std::cout << "Found value: " << *result << "\n";
        }

        if (auto result = find_value(false); !result) {
            std::cout << "Value not found\n";
        }
    }

    void demonstrate_variant() {
        std::cout << "\n--- C++17: std::variant ---\n";

        std::variant<int, std::string, double> var;

        var = 42;
        std::cout << "Variant holds int: " << std::get<int>(var) << "\n";

        var = std::string("hello");
        std::cout << "Variant holds string: " << std::get<std::string>(var) << "\n";

        var = 3.14;
        std::cout << "Variant holds double: " << std::get<double>(var) << "\n";
    }

    void demonstrate_string_view() {
        std::cout << "\n--- C++17: std::string_view ---\n";

        auto process_string = [](std::string_view sv) {
            std::cout << "Processing: " << sv << " (length: " << sv.length() << ")\n";
        };

        std::string str = "Hello, World!";
        const char* cstr = "C-style string";

        process_string(str);
        process_string(cstr);
        process_string("String literal");
    }

    void demonstrate_filesystem() {
        std::cout << "\n--- C++17: Filesystem library ---\n";

        namespace fs = std::filesystem;

        try {
            fs::path current = fs::current_path();
            std::cout << "Current directory: " << current << "\n";

            if (fs::exists(current)) {
                std::cout << "Directory exists and has "
                         << std::distance(fs::directory_iterator(current), fs::directory_iterator{})
                         << " entries\n";
            }
        } catch (const fs::filesystem_error& e) {
            std::cout << "Filesystem error: " << e.what() << "\n";
        }
    }

    void demonstrate_parallel_algorithms() {
        std::cout << "\n--- C++17: Parallel algorithms ---\n";

        std::vector<int> vec(1000);
        std::iota(vec.begin(), vec.end(), 1);

        // Sequential execution
        std::sort(std::execution::seq, vec.begin(), vec.end(), std::greater<int>());

        // Parallel execution (if supported)
        std::sort(std::execution::par, vec.begin(), vec.end());

        std::cout << "Parallel algorithms enable automatic parallelization\n";
        std::cout << "First few elements: ";
        for (int i = 0; i < 5; ++i) {
            std::cout << vec[i] << " ";
        }
        std::cout << "\n";
    }

    void demonstrate() {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "C++17 FEATURES DEMONSTRATION\n";
        std::cout << std::string(80, '=') << "\n";

        demonstrate_structured_bindings();
        demonstrate_if_constexpr();
        demonstrate_optional();
        demonstrate_variant();
        demonstrate_string_view();
        demonstrate_filesystem();
        demonstrate_parallel_algorithms();

        std::cout << "\n=== C++17 Complete Feature List ===\n";
        std::cout << "Core Language Features:\n";
        std::cout << "• Structured bindings\n";
        std::cout << "• if constexpr\n";
        std::cout << "• Template argument deduction for class templates\n";
        std::cout << "• Inline variables\n";
        std::cout << "• Constexpr if\n";
        std::cout << "• Fold expressions\n";
        std::cout << "• auto template parameters\n";
        std::cout << "• Class template argument deduction\n";
        std::cout << "• if and switch with initializers\n";
        std::cout << "• [[fallthrough]], [[nodiscard]], [[maybe_unused]] attributes\n";
        std::cout << "• Nested namespace declarations\n";
        std::cout << "• Exception specifications as part of type system\n";
        std::cout << "• Lambda capture of *this\n";
        std::cout << "• constexpr lambda\n";
        std::cout << "• Hexadecimal floating-point literals\n";

        std::cout << "\nStandard Library Features:\n";
        std::cout << "• std::optional\n";
        std::cout << "• std::variant\n";
        std::cout << "• std::any\n";
        std::cout << "• std::string_view\n";
        std::cout << "• Filesystem library\n";
        std::cout << "• Parallel algorithms\n";
        std::cout << "• Mathematical special functions\n";
        std::cout << "• std::byte\n";
        std::cout << "• std::invoke\n";
        std::cout << "• std::apply\n";
        std::cout << "• std::make_from_tuple\n";
        std::cout << "• Searchers for std::search\n";
        std::cout << "• std::clamp\n";
        std::cout << "• std::gcd, std::lcm\n";
        std::cout << "• std::sample\n";
        std::cout << "• Container deduction guides\n";
        std::cout << "• Polymorphic memory resources\n";
    }
}

// =============================================================================
// C++20 FEATURES (ISO/IEC 14882:2020)
// =============================================================================

namespace cpp20_features {

    // Concepts example
    template<typename T>
    concept Arithmetic = std::is_arithmetic_v<T>;

    template<Arithmetic T>
    T add(T a, T b) {
        return a + b;
    }

    void demonstrate_concepts() {
        std::cout << "\n--- C++20: Concepts ---\n";

        std::cout << "Concepts constrain templates: ";
        std::cout << add(5, 3) << ", " << add(2.5, 1.5) << "\n";
        // add("hello", "world");  // Error: string doesn't satisfy Arithmetic concept
    }

    void demonstrate_ranges() {
        std::cout << "\n--- C++20: Ranges ---\n";

        std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        // Range-based algorithms
        auto even_numbers = numbers
                          | std::views::filter([](int n) { return n % 2 == 0; })
                          | std::views::transform([](int n) { return n * n; });

        std::cout << "Even squares: ";
        for (int n : even_numbers) {
            std::cout << n << " ";
        }
        std::cout << "\n";
    }

    void demonstrate_coroutines() {
        std::cout << "\n--- C++20: Coroutines ---\n";
        std::cout << "Coroutines provide cooperative multitasking\n";
        std::cout << "(Example would require complex setup)\n";
    }

    void demonstrate_modules() {
        std::cout << "\n--- C++20: Modules ---\n";
        std::cout << "Modules replace header files with:\n";
        std::cout << "export module my_module;\n";
        std::cout << "import std.iostream;\n";
        std::cout << "(Compiler support still limited)\n";
    }

    void demonstrate_three_way_comparison() {
        std::cout << "\n--- C++20: Three-way comparison (spaceship operator) ---\n";

        struct Point {
            int x, y;
            auto operator<=>(const Point&) const = default;
        };

        Point p1{1, 2};
        Point p2{3, 4};

        std::cout << "Spaceship operator generates all comparison operators\n";
        std::cout << "p1 < p2: " << (p1 < p2) << "\n";
        std::cout << "p1 == p2: " << (p1 == p2) << "\n";
    }

    void demonstrate_designated_initializers() {
        std::cout << "\n--- C++20: Designated initializers ---\n";

        struct Config {
            int width = 800;
            int height = 600;
            bool fullscreen = false;
        };

        Config cfg{.width = 1920, .height = 1080, .fullscreen = true};

        std::cout << "Designated initializers: " << cfg.width << "x" << cfg.height
                  << ", fullscreen: " << cfg.fullscreen << "\n";
    }

    void demonstrate() {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "C++20 FEATURES DEMONSTRATION\n";
        std::cout << std::string(80, '=') << "\n";

        demonstrate_concepts();
        demonstrate_ranges();
        demonstrate_coroutines();
        demonstrate_modules();
        demonstrate_three_way_comparison();
        demonstrate_designated_initializers();

        std::cout << "\n=== C++20 Complete Feature List ===\n";
        std::cout << "Core Language Features:\n";
        std::cout << "• Concepts\n";
        std::cout << "• Modules\n";
        std::cout << "• Coroutines\n";
        std::cout << "• Three-way comparison operator (spaceship <=>)\n";
        std::cout << "• Designated initializers\n";
        std::cout << "• Template syntax for lambdas\n";
        std::cout << "• Pack expansion in lambda init-capture\n";
        std::cout << "• Default-constructible and assignable stateless lambdas\n";
        std::cout << "• Lambda capture of parameter pack\n";
        std::cout << "• consteval functions\n";
        std::cout << "• constinit\n";
        std::cout << "• using enum declarations\n";
        std::cout << "• Class types in non-type template parameters\n";
        std::cout << "• [[likely]] and [[unlikely]] attributes\n";
        std::cout << "• [[no_unique_address]] attribute\n";
        std::cout << "• Abbreviated function templates\n";
        std::cout << "• char8_t type\n";
        std::cout << "• Feature test macros\n";
        std::cout << "• Aggregate initialization with parentheses\n";
        std::cout << "• Allow virtual function calls in constant expressions\n";

        std::cout << "\nStandard Library Features:\n";
        std::cout << "• Ranges library\n";
        std::cout << "• Calendar and timezone additions to chrono\n";
        std::cout << "• std::span\n";
        std::cout << "• std::format\n";
        std::cout << "• std::source_location\n";
        std::cout << "• std::bit_cast\n";
        std::cout << "• std::midpoint, std::lerp\n";
        std::cout << "• std::ssize\n";
        std::cout << "• std::to_array\n";
        std::cout << "• String prefix/suffix checking\n";
        std::cout << "• std::erase/std::erase_if\n";
        std::cout << "• Math constants\n";
        std::cout << "• std::jthread\n";
        std::cout << "• Synchronization primitives (semaphore, latch, barrier)\n";
        std::cout << "• std::atomic_ref\n";
        std::cout << "• std::stop_token\n";
        std::cout << "• Safe integral comparisons\n";
        std::cout << "• std::assume_aligned\n";
        std::cout << "• Constexpr algorithms\n";
        std::cout << "• Constexpr containers\n";
    }
}

// =============================================================================
// C++23 FEATURES (ISO/IEC 14882:2023) - Preview
// =============================================================================

namespace cpp23_features {

    void demonstrate_preview() {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "C++23 FEATURES PREVIEW\n";
        std::cout << std::string(80, '=') << "\n";

        std::cout << "\n=== C++23 Major Features (Selected) ===\n";
        std::cout << "Core Language Features:\n";
        std::cout << "• if consteval\n";
        std::cout << "• Multidimensional subscript operator\n";
        std::cout << "• auto(x): decay-copy in the language\n";
        std::cout << "• Literal operator template for strings\n";
        std::cout << "• Extended floating-point types\n";
        std::cout << "• #elifdef and #elifndef\n";
        std::cout << "• Attributes on lambda expressions\n";
        std::cout << "• Optional extended floating-point types\n";
        std::cout << "• Named universal character names\n";
        std::cout << "• Relaxing requirements on wchar_t\n";

        std::cout << "\nStandard Library Features:\n";
        std::cout << "• std::expected\n";
        std::cout << "• std::flat_map, std::flat_set\n";
        std::cout << "• std::generator\n";
        std::cout << "• std::print, std::println\n";
        std::cout << "• Improved ranges support\n";
        std::cout << "• std::mdspan\n";
        std::cout << "• std::optional monadic operations\n";
        std::cout << "• std::string::contains\n";
        std::cout << "• std::to_underlying\n";
        std::cout << "• std::byteswap\n";
        std::cout << "• std::unreachable\n";
        std::cout << "• Stacktrace library\n";
        std::cout << "• std::move_only_function\n";
        std::cout << "• Associative container heterogeneous erasure\n";
        std::cout << "• std::reference_wrapper improvements\n";

        std::cout << "\nNote: C++23 support varies by compiler\n";
    }
}

// =============================================================================
// FEATURE EVOLUTION SUMMARY
// =============================================================================

void print_evolution_summary() {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "C++ EVOLUTION SUMMARY\n";
    std::cout << std::string(80, '=') << "\n";

    std::cout << "\nC++11 (2011): Modern C++ Foundation\n";
    std::cout << "• Introduced modern C++ with auto, lambdas, smart pointers\n";
    std::cout << "• Added move semantics and rvalue references\n";
    std::cout << "• Provided standard threading and concurrency support\n";
    std::cout << "• Established uniform initialization syntax\n";

    std::cout << "\nC++14 (2014): Refinements and Improvements\n";
    std::cout << "• Generic lambdas and improved type deduction\n";
    std::cout << "• Binary literals and digit separators\n";
    std::cout << "• std::make_unique and library improvements\n";

    std::cout << "\nC++17 (2017): Major Language Evolution\n";
    std::cout << "• Structured bindings for tuple-like objects\n";
    std::cout << "• Optional, variant, and string_view types\n";
    std::cout << "• Filesystem library and parallel algorithms\n";
    std::cout << "• if constexpr for template programming\n";

    std::cout << "\nC++20 (2020): Revolutionary Changes\n";
    std::cout << "• Concepts for constraining templates\n";
    std::cout << "• Modules to replace header files\n";
    std::cout << "• Coroutines for asynchronous programming\n";
    std::cout << "• Ranges library for functional programming\n";
    std::cout << "• Three-way comparison operator\n";

    std::cout << "\nC++23 (2023): Continued Evolution\n";
    std::cout << "• Expected type for error handling\n";
    std::cout << "• Flat associative containers\n";
    std::cout << "• Print functions and formatting improvements\n";
    std::cout << "• Monadic operations for optional\n";

    std::cout << "\nKey Themes Across Versions:\n";
    std::cout << "• Type safety and expressiveness\n";
    std::cout << "• Performance and zero-cost abstractions\n";
    std::cout << "• Easier generic programming\n";
    std::cout << "• Better standard library\n";
    std::cout << "• Improved compile-time computation\n";
    std::cout << "• Enhanced concurrency support\n";
}

// =============================================================================
// MAIN DEMONSTRATION
// =============================================================================

int main() {
    std::cout << "C++ STANDARDS FEATURES COMPREHENSIVE OVERVIEW\n";
    std::cout << std::string(80, '=') << "\n";

    try {
        cpp11_features::demonstrate();
        cpp14_features::demonstrate();
        cpp17_features::demonstrate();
        cpp20_features::demonstrate();
        cpp23_features::demonstrate_preview();

        print_evolution_summary();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "END OF C++ STANDARDS OVERVIEW\n";
    std::cout << std::string(80, '=') << "\n";

    return 0;
}
