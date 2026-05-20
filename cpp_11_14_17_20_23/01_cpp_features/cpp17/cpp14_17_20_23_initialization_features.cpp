// =============================
// C++14/17/20/23 INITIALIZATION FEATURES
// =============================
/*
This file demonstrates the new initialization features introduced in:
- C++14: Binary literals, digit separators, auto return type deduction
- C++17: Structured bindings, inline variables, auto template parameters
- C++20: Designated initializers, aggregate initialization with inheritance
- C++23: auto(x) syntax, multidimensional subscript operator
*/

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <tuple>
#include <array>
#include <memory>
#include <optional>
#include <variant>

// =============================
// C++14 FEATURES
// =============================

// C++14: Binary literals and digit separators
void cpp14_literal_initialization() {
    std::cout << "=== C++14 LITERAL INITIALIZATION ===\n";

    // Binary literals (C++14)
    auto binary_value = 0b1010'1100;        // Binary literal with digit separator
    auto large_number = 1'000'000;          // Digit separators for readability
    auto hex_with_sep = 0xFF'AA'BB'CC;      // Hexadecimal with separators

    std::cout << "Binary 0b1010'1100: " << binary_value << "\n";
    std::cout << "Large number 1'000'000: " << large_number << "\n";
    std::cout << "Hex 0xFF'AA'BB'CC: " << std::hex << hex_with_sep << std::dec << "\n\n";
}

// C++14: Auto return type deduction
auto cpp14_auto_return_function() {
    return std::vector<int>{1, 2, 3, 4, 5}; // Auto deduces return type
}

auto cpp14_auto_lambda = [](auto x, auto y) { // Generic lambda with auto parameters
    return x + y;
};

void cpp14_auto_features() {
    std::cout << "=== C++14 AUTO ENHANCEMENTS ===\n";

    // Auto return type deduction
    auto vec = cpp14_auto_return_function();
    std::cout << "Auto return function vector size: " << vec.size() << "\n";

    // Generic lambdas with auto parameters
    auto result1 = cpp14_auto_lambda(5, 10);        // int + int
    auto result2 = cpp14_auto_lambda(3.14, 2.86);   // double + double
    auto result3 = cpp14_auto_lambda(std::string("Hello"), std::string(" World"));

    std::cout << "Generic lambda (5, 10): " << result1 << "\n";
    std::cout << "Generic lambda (3.14, 2.86): " << result2 << "\n";
    std::cout << "Generic lambda (\"Hello\", \" World\"): " << result3 << "\n\n";
}

// =============================
// C++17 FEATURES
// =============================

// C++17: Structured bindings
struct Point3D {
    double x, y, z;
};

std::tuple<int, std::string, double> get_person_data() {
    return {25, "Alice", 5.6};
}

void cpp17_structured_bindings() {
    std::cout << "=== C++17 STRUCTURED BINDINGS ===\n";

    // Structured bindings with tuples
    auto [age, name, height] = get_person_data();
    std::cout << "Person: " << name << ", " << age << " years, " << height << "ft\n";

    // Structured bindings with structs
    Point3D point{1.0, 2.0, 3.0};
    auto [x, y, z] = point;
    std::cout << "Point: (" << x << ", " << y << ", " << z << ")\n";

    // Structured bindings with arrays
    int arr[3] = {10, 20, 30};
    auto [a, b, c] = arr;
    std::cout << "Array elements: " << a << ", " << b << ", " << c << "\n";

    // Structured bindings with pairs
    std::pair<std::string, int> p{"key", 42};
    auto [key, value] = p;
    std::cout << "Pair: " << key << " = " << value << "\n";

    // Structured bindings with maps (range-based for)
    std::map<std::string, int> grades{{"Math", 95}, {"Science", 88}};
    for (const auto& [subject, grade] : grades) {
        std::cout << subject << ": " << grade << "\n";
    }
    std::cout << "\n";
}

// C++17: Inline variables
class Cpp17InlineStatic {
public:
    static inline int counter = 0;              // Inline static variable (C++17)
    static inline std::vector<int> data{1, 2, 3}; // Inline static container
    static inline const std::string name = "InlineClass"; // Inline static const
};

void cpp17_inline_variables() {
    std::cout << "=== C++17 INLINE STATIC VARIABLES ===\n";

    Cpp17InlineStatic::counter++;
    std::cout << "Inline static counter: " << Cpp17InlineStatic::counter << "\n";
    std::cout << "Inline static vector size: " << Cpp17InlineStatic::data.size() << "\n";
    std::cout << "Inline static name: " << Cpp17InlineStatic::name << "\n\n";
}

// C++17: Auto template parameters
template<auto Value>  // C++17: auto as template parameter
class AutoTemplate {
public:
    static constexpr auto value = Value;

    void print() const {
        std::cout << "Template value: " << value << "\n";
    }
};

void cpp17_auto_template_parameters() {
    std::cout << "=== C++17 AUTO TEMPLATE PARAMETERS ===\n";

    AutoTemplate<42> int_template;
    AutoTemplate<3.14> double_template;
    AutoTemplate<'X'> char_template;

    int_template.print();
    double_template.print();
    char_template.print();
    std::cout << "\n";
}

// C++17: Class template argument deduction (CTAD)
void cpp17_ctad() {
    std::cout << "=== C++17 CLASS TEMPLATE ARGUMENT DEDUCTION ===\n";

    // Before C++17: std::vector<int> vec{1, 2, 3};
    std::vector vec{1, 2, 3, 4, 5};         // Deduces std::vector<int>
    std::pair p{42, "answer"};              // Deduces std::pair<int, const char*>
    std::tuple t{1, 2.0, "three"};          // Deduces std::tuple<int, double, const char*>

    // Custom deduction guides can be provided for user-defined types
    std::cout << "CTAD vector size: " << vec.size() << "\n";
    std::cout << "CTAD pair: (" << p.first << ", " << p.second << ")\n";
    std::cout << "CTAD tuple size: " << std::tuple_size_v<decltype(t)> << "\n\n";
}

// =============================
// C++20 FEATURES
// =============================

// C++20: Designated initializers
struct Config {
    std::string server_name = "localhost";
    int port = 8080;
    bool ssl_enabled = false;
    double timeout = 30.0;
};

void cpp20_designated_initializers() {
    std::cout << "=== C++20 DESIGNATED INITIALIZERS ===\n";

    // Designated initializers (C++20)
    Config config1{
        .server_name = "production.example.com",
        .port = 443,
        .ssl_enabled = true
        // timeout uses default value
    };

    Config config2{
        .port = 9000,
        .timeout = 60.0
        // other fields use default values
    };

    std::cout << "Config1: " << config1.server_name << ":" << config1.port
              << " (SSL: " << config1.ssl_enabled << ")\n";
    std::cout << "Config2: " << config2.server_name << ":" << config2.port
              << " (timeout: " << config2.timeout << ")\n\n";
}

// C++20: Aggregate initialization with inheritance
struct Base {
    int base_value;
};

struct Derived : Base {
    std::string derived_name;
    double derived_data;
};

void cpp20_aggregate_inheritance() {
    std::cout << "=== C++20 AGGREGATE INITIALIZATION WITH INHERITANCE ===\n";

    // C++20: Aggregate initialization works with inheritance
    Derived obj{
        {42},           // Initialize Base part
        "derived",      // Initialize derived_name
        3.14           // Initialize derived_data
    };

    std::cout << "Derived object: base=" << obj.base_value
              << ", name=" << obj.derived_name
              << ", data=" << obj.derived_data << "\n\n";
}

// C++20: Concepts and constrained auto
#if __cplusplus >= 202002L
#include <concepts>

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

void cpp20_constrained_auto() {
    std::cout << "=== C++20 CONSTRAINED AUTO ===\n";

    Numeric auto x = 42;        // Constrained auto with concept
    Numeric auto y = 3.14;      // Constrained auto with concept
    // Numeric auto z = "hello"; // Would be a compile error

    std::cout << "Constrained auto int: " << x << "\n";
    std::cout << "Constrained auto double: " << y << "\n\n";
}
#endif

// C++20: Range-based for with initializer
void cpp20_range_for_initializer() {
    std::cout << "=== C++20 RANGE-BASED FOR WITH INITIALIZER ===\n";

    // C++20: Range-based for loop with initializer
    for (auto vec = std::vector{1, 2, 3, 4, 5}; auto& element : vec) {
        element *= 2;
        std::cout << element << " ";
    }
    std::cout << "\n\n";
}

// =============================
// C++23 FEATURES
// =============================

// C++23: auto(x) and auto{x} syntax
void cpp23_auto_syntax() {
    std::cout << "=== C++23 AUTO(X) SYNTAX ===\n";

    int x = 42;
    const int& ref = x;

    // C++23: auto(x) - explicit copy, removes cv-qualifiers and references
    auto copied1 = auto(ref);       // Equivalent to auto copied1 = ref; but more explicit
    auto copied2 = auto{ref};       // Same as above but with braces

    // Useful for perfect forwarding and template metaprogramming
    std::cout << "Original ref: " << ref << "\n";
    std::cout << "Copied value: " << copied1 << "\n";
    std::cout << "Copied value (braces): " << copied2 << "\n\n";
}

// C++23: Multidimensional subscript operator
class Matrix23 {
private:
    std::vector<std::vector<int>> data;

public:
    Matrix23(size_t rows, size_t cols) : data(rows, std::vector<int>(cols, 0)) {}

    // C++23: Multidimensional subscript operator
    int& operator[](size_t row, size_t col) {
        return data[row][col];
    }

    const int& operator[](size_t row, size_t col) const {
        return data[row][col];
    }

    void print() const {
        for (const auto& row : data) {
            for (int val : row) {
                std::cout << val << " ";
            }
            std::cout << "\n";
        }
    }
};

void cpp23_multidimensional_subscript() {
    std::cout << "=== C++23 MULTIDIMENSIONAL SUBSCRIPT ===\n";

    Matrix23 matrix(3, 3);

    // C++23: Multidimensional subscript syntax
    matrix[0, 0] = 1;
    matrix[0, 1] = 2;
    matrix[1, 0] = 3;
    matrix[1, 1] = 4;

    std::cout << "Matrix with multidimensional subscript:\n";
    matrix.print();
    std::cout << "\n";
}

// C++23: if consteval
void cpp23_if_consteval() {
    std::cout << "=== C++23 IF CONSTEVAL ===\n";

    auto compile_time_or_runtime = []() {
        if consteval {
            // This branch is taken during constant evaluation
            return "Compile-time evaluation";
        } else {
            // This branch is taken during runtime
            return "Runtime evaluation";
        }
    };

    constexpr auto compile_time = compile_time_or_runtime();
    auto runtime = compile_time_or_runtime();

    std::cout << "Constexpr call: " << compile_time << "\n";
    std::cout << "Runtime call: " << runtime << "\n\n";
}

// C++23: Extended aggregate initialization
struct ExtendedAggregate {
    int a;
    std::string b;
    std::vector<int> c;

    // C++23: Aggregates can have default member initializers
    double d = 3.14;
    bool flag = true;
};

void cpp23_extended_aggregates() {
    std::cout << "=== C++23 EXTENDED AGGREGATE INITIALIZATION ===\n";

    // C++23: More flexible aggregate initialization
    ExtendedAggregate agg{
        .a = 42,
        .b = "hello",
        .c = {1, 2, 3},
        // d and flag use default values
    };

    std::cout << "Extended aggregate: a=" << agg.a
              << ", b=" << agg.b
              << ", c.size=" << agg.c.size()
              << ", d=" << agg.d
              << ", flag=" << agg.flag << "\n\n";
}

// =============================
// MAIN FUNCTION
// =============================
int main() {
    std::cout << "C++14/17/20/23 INITIALIZATION FEATURES DEMONSTRATION\n";
    std::cout << "====================================================\n\n";

    // C++14 Features
    std::cout << "C++14 FEATURES:\n";
    std::cout << "===============\n";
    cpp14_literal_initialization();
    cpp14_auto_features();

    // C++17 Features
    std::cout << "C++17 FEATURES:\n";
    std::cout << "===============\n";
    cpp17_structured_bindings();
    cpp17_inline_variables();
    cpp17_auto_template_parameters();
    cpp17_ctad();

    // C++20 Features
    std::cout << "C++20 FEATURES:\n";
    std::cout << "===============\n";
    cpp20_designated_initializers();
    cpp20_aggregate_inheritance();
    cpp20_range_for_initializer();

#if __cplusplus >= 202002L
    cpp20_constrained_auto();
#else
    std::cout << "=== C++20 CONSTRAINED AUTO ===\n";
    std::cout << "Requires C++20 compiler support\n\n";
#endif

    // C++23 Features
    std::cout << "C++23 FEATURES:\n";
    std::cout << "===============\n";
    cpp23_auto_syntax();
    cpp23_multidimensional_subscript();
    cpp23_if_consteval();
    cpp23_extended_aggregates();

    std::cout << "=== SUMMARY OF MODERN C++ INITIALIZATION FEATURES ===\n";
    std::cout << "C++14:\n";
    std::cout << "- Binary literals (0b1010)\n";
    std::cout << "- Digit separators (1'000'000)\n";
    std::cout << "- Auto return type deduction\n";
    std::cout << "- Generic lambdas with auto parameters\n\n";

    std::cout << "C++17:\n";
    std::cout << "- Structured bindings: auto [a, b, c] = tuple;\n";
    std::cout << "- Inline static variables\n";
    std::cout << "- Auto template parameters: template<auto N>\n";
    std::cout << "- Class template argument deduction (CTAD)\n\n";

    std::cout << "C++20:\n";
    std::cout << "- Designated initializers: Point{.x=1, .y=2}\n";
    std::cout << "- Aggregate initialization with inheritance\n";
    std::cout << "- Constrained auto with concepts\n";
    std::cout << "- Range-based for with initializer\n\n";

    std::cout << "C++23:\n";
    std::cout << "- auto(x) and auto{x} explicit copy syntax\n";
    std::cout << "- Multidimensional subscript operator[i,j]\n";
    std::cout << "- if consteval for compile-time detection\n";
    std::cout << "- Extended aggregate initialization features\n";

    return 0;
}
