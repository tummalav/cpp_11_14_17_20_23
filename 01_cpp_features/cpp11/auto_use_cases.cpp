// =============================
// AUTO TYPE DEDUCTION RULES (C++)
// =============================
/*
1. BASIC RULE: auto deduces the type from the initializer expression
   - Similar to template type deduction
   - auto x = expr; // x has the type of expr (with adjustments)

2. REFERENCE REMOVAL: auto removes references unless explicitly specified
   int a = 5;
   int& b = a;
   auto x = b;      // x is int (not int&) - reference removed
   auto& y = b;     // y is int& - reference preserved
   auto&& z = b;    // z is int& - universal reference (forwarding reference)

3. CV-QUALIFIER REMOVAL: auto drops const/volatile unless explicitly specified
   const int c = 10;
   auto d = c;        // d is int - const removed
   const auto e = c;  // e is const int - const preserved
   auto& f = c;       // f is const int& - const preserved with reference

4. POINTER AND ARRAY DECAY:
   int arr[3] = {1, 2, 3};
   auto p = arr;      // p is int* - array decays to pointer
   auto& r = arr;     // r is int (&)[3] - array reference, no decay

   const char str[] = "hello";
   auto s = str;      // s is const char* - array decays to pointer
   auto& sr = str;    // sr is const char (&)[6] - array reference

5. BRACED INITIALIZER SPECIAL CASES:
   auto x = {1, 2, 3};  // x is std::initializer_list<int>
   auto y{1};           // y is int (C++17+), was initializer_list in C++11/14
   auto z = {1};        // z is std::initializer_list<int>
   // auto w{1, 2};     // Error in C++17+ (multiple values in direct-init)

6. FUNCTION TYPES:
   void func(int) {}
   auto fp = func;      // fp is void(*)(int) - function decays to pointer
   auto& fr = func;     // fr is void(&)(int) - function reference

7. AUTO WITH RETURN TYPES (C++14+):
   auto add(int a, int b) -> int { return a + b; }  // Trailing return (C++11+)
   auto multiply(int a, int b) { return a * b; }    // Auto return (C++14+)

8. UNIVERSAL REFERENCES (FORWARDING REFERENCES):
   template<typename T>
   void func(T&& param) {
       auto&& local = param;  // Universal reference, preserves value category
   }

9. MULTIPLE DECLARATORS - ALL MUST DEDUCE TO SAME TYPE:
   auto a = 1, b = 2;     // OK: both int
   // auto c = 1, d = 2.0; // Error: different types (int vs double)

10. LAMBDA AND STRUCTURED BINDINGS REQUIREMENTS:
    auto lambda = [](int x) { return x * 2; };  // auto required for lambdas
    auto [x, y] = std::make_pair(1, 2);         // auto required for structured bindings (C++17+)
*/

// =============================
// COMPREHENSIVE AUTO EXAMPLES
// =============================

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <typeinfo>
#include <tuple>
#include <memory>
#include <algorithm>
#include <functional>
#include <chrono>

// Helper function to print type information
template<typename T>
void print_type_info(const char* name) {
    std::cout << name << " type: " << typeid(T).name() << std::endl;
}

// LITERAL TYPE DEDUCTION WITH AUTO
void literal_type_deduction() {
    std::cout << "=== AUTO TYPE DEDUCTION WITH LITERALS ===\n";

    // INTEGER LITERALS
    auto int_decimal = 42;          // int (decimal literal)
    auto int_octal = 052;           // int (octal literal)
    auto int_hex = 0x2A;            // int (hexadecimal literal)
    auto int_binary = 0b101010;     // int (binary literal, C++14+)

    // Integer literal suffixes affect deduced type
    auto unsigned_int = 42u;        // unsigned int
    auto long_int = 42l;            // long
    auto long_long = 42ll;          // long long
    auto unsigned_long = 42ul;      // unsigned long
    auto unsigned_long_long = 42ull; // unsigned long long

    // FLOATING-POINT LITERALS
    auto float_val = 3.14f;         // float (with f suffix)
    auto double_val = 3.14;         // double (default for floating literals)
    auto long_double_val = 3.14l;   // long double (with l suffix)
    auto scientific = 1.23e4;       // double (scientific notation)
    auto hex_float = 0x1.4p3;       // double (hexadecimal float, C++17+)

    // CHARACTER LITERALS
    auto char_literal = 'A';        // char
    auto wide_char = L'A';          // wchar_t
    auto char16_literal = u'A';     // char16_t (C++11+)
    auto char32_literal = U'A';     // char32_t (C++11+)
    auto utf8_char = u8'A';         // char (C++17+) or char8_t (C++20+)

    // STRING LITERALS
    auto string_literal = "hello";         // const char*
    auto wide_string = L"hello";           // const wchar_t*
    auto utf16_string = u"hello";          // const char16_t*
    auto utf32_string = U"hello";          // const char32_t*
    auto utf8_string = u8"hello";          // const char* (C++17) or const char8_t* (C++20+)
    auto raw_string = R"(hello\nworld)";   // const char* (raw string)

    // STRING LITERAL SUFFIXES (C++14+)
    using namespace std::string_literals;
    auto std_string = "hello"s;            // std::string
    auto string_view = "hello"sv;          // std::string_view (C++17+)

    // BOOLEAN LITERALS
    auto bool_true = true;          // bool
    auto bool_false = false;        // bool

    // NULLPTR LITERAL
    auto null_ptr = nullptr;        // std::nullptr_t

    // LARGE INTEGER LITERALS - compiler chooses appropriate type
    auto large_int = 2147483648;    // long (if int can't hold it)
    auto very_large = 9223372036854775808ull; // unsigned long long

    // DIGIT SEPARATORS (C++14+)
    auto separated = 1'000'000;     // int (digit separators for readability)
    auto hex_separated = 0xFF'FF'FF'FF; // int

    std::cout << "Integer literals:\n";
    std::cout << "decimal 42: " << int_decimal << " (int)\n";
    std::cout << "octal 052: " << int_octal << " (int)\n";
    std::cout << "hex 0x2A: " << int_hex << " (int)\n";
    std::cout << "binary 0b101010: " << int_binary << " (int)\n";
    std::cout << "42u: " << unsigned_int << " (unsigned int)\n";
    std::cout << "42ll: " << long_long << " (long long)\n";

    std::cout << "\nFloating-point literals:\n";
    std::cout << "3.14f: " << float_val << " (float)\n";
    std::cout << "3.14: " << double_val << " (double)\n";
    std::cout << "3.14l: " << long_double_val << " (long double)\n";
    std::cout << "1.23e4: " << scientific << " (double)\n";

    std::cout << "\nCharacter literals:\n";
    std::cout << "'A': " << char_literal << " (char)\n";
    std::cout << "L'A': " << wide_char << " (wchar_t)\n";

    std::cout << "\nString literals:\n";
    std::cout << "\"hello\": " << string_literal << " (const char*)\n";
    std::cout << "\"hello\"s: " << std_string << " (std::string)\n";

    std::cout << "\nBoolean literals:\n";
    std::cout << "true: " << bool_true << " (bool)\n";
    std::cout << "false: " << bool_false << " (bool)\n";

    std::cout << "\nOther literals:\n";
    std::cout << "nullptr: " << (null_ptr == nullptr) << " (std::nullptr_t)\n";
    std::cout << "1'000'000: " << separated << " (int with digit separators)\n";

    std::cout << "\n";
}

// LITERAL DEDUCTION IN DIFFERENT CONTEXTS
void literal_contexts() {
    std::cout << "=== LITERAL DEDUCTION IN DIFFERENT CONTEXTS ===\n";

    // In arrays
    auto array = {1, 2, 3, 4, 5};          // std::initializer_list<int>
    auto int_array[] = {1, 2, 3};          // int[3]

    // In function calls
    auto max_val = std::max(10, 20);       // int (deduced from function return)
    auto min_val = std::min(5.5, 3.3);     // double

    // With arithmetic operations
    auto sum = 10 + 20;                    // int
    auto division = 10.0 / 3;              // double
    auto mixed = 5 + 2.5;                  // double (int promoted to double)

    // Conditional operator affects type
    auto conditional1 = true ? 10 : 20;    // int
    auto conditional2 = true ? 10 : 20.5;  // double (common type)
    auto conditional3 = true ? 10u : 20;   // unsigned int (common type)

    // Comparison operators
    auto comparison = (10 > 5);            // bool
    auto equality = (3.14 == 3.14f);       // bool

    std::cout << "Array initializer {1,2,3,4,5}: initializer_list<int>\n";
    std::cout << "max(10, 20): " << max_val << " (int)\n";
    std::cout << "min(5.5, 3.3): " << min_val << " (double)\n";
    std::cout << "10 + 20: " << sum << " (int)\n";
    std::cout << "10.0 / 3: " << division << " (double)\n";
    std::cout << "5 + 2.5: " << mixed << " (double)\n";
    std::cout << "true ? 10 : 20.5: " << conditional2 << " (double)\n";
    std::cout << "10 > 5: " << comparison << " (bool)\n";

    std::cout << "\n";
}

// LITERAL SUFFIX EXAMPLES
void literal_suffix_examples() {
    std::cout << "=== LITERAL SUFFIXES AND AUTO DEDUCTION ===\n";

    // Integer suffixes
    auto i = 123;      // int
    auto u = 123u;     // unsigned int
    auto l = 123l;     // long
    auto ul = 123ul;   // unsigned long
    auto ll = 123ll;   // long long
    auto ull = 123ull; // unsigned long long

    // Floating-point suffixes
    auto f = 3.14f;    // float
    auto d = 3.14;     // double (no suffix)
    auto ld = 3.14l;   // long double

    // Character and string suffixes
    using namespace std::string_literals;
    using namespace std::chrono_literals;

    auto str = "text"s;           // std::string
    auto sv = "text"sv;           // std::string_view (C++17+)
    auto duration = 42s;          // std::chrono::seconds
    auto millisecs = 100ms;       // std::chrono::milliseconds

    std::cout << "Literal suffixes demonstrate explicit type control:\n";
    std::cout << "123 -> int, 123u -> unsigned int, 123ll -> long long\n";
    std::cout << "3.14 -> double, 3.14f -> float, 3.14l -> long double\n";
    std::cout << "\"text\"s -> std::string, \"text\"sv -> std::string_view\n";
    std::cout << "42s -> std::chrono::seconds, 100ms -> std::chrono::milliseconds\n";

    std::cout << "\n";
}

// =============================
// COMPREHENSIVE AUTO EXAMPLES
// =============================

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <typeinfo>
#include <tuple>
#include <memory>
#include <algorithm>
#include <functional>
#include <chrono>

// Helper function to print type information
template<typename T>
void print_type_info(const char* name) {
    std::cout << name << " type: " << typeid(T).name() << std::endl;
}

// 1. AUTO TYPE DEDUCTION DEMONSTRATION
void auto_deduction_demo() {
    std::cout << "=== AUTO TYPE DEDUCTION DEMONSTRATION ===\n";

    // Basic deduction
    auto a = 42;           // int
    auto b = 3.14;         // double
    auto c = "hello";      // const char*
    auto d = std::string("world"); // std::string

    // Reference and const behavior
    int x = 10;
    const int cx = 20;
    int& rx = x;
    const int& rcx = cx;

    auto e = x;            // int (copy)
    auto f = cx;           // int (const removed)
    auto g = rx;           // int (reference removed)
    auto h = rcx;          // int (const and reference removed)

    auto& i = x;           // int&
    auto& j = cx;          // const int& (const preserved with reference)
    const auto& k = x;     // const int&

    // Pointer deduction
    auto* ptr1 = &x;       // int*
    auto ptr2 = &x;        // int* (equivalent to above)

    std::cout << "Basic deduction completed\n\n";
}

// 2. COMPLEX TYPE INFERENCE
void complex_type_inference() {
    std::cout << "=== COMPLEX TYPE INFERENCE ===\n";

    // STL containers with complex types
    std::vector<std::map<std::string, std::vector<int>>> complex_container = {
        {{"data", {1, 2, 3}}, {"values", {4, 5, 6}}}
    };

    // Without auto (verbose and error-prone)
    std::vector<std::map<std::string, std::vector<int>>>::iterator it1 = complex_container.begin();

    // With auto (clean and maintainable)
    auto it2 = complex_container.begin();
    auto& first_map = *it2;
    auto& data_vector = first_map["data"];

    // Smart pointers
    auto smart_ptr = std::make_unique<std::vector<int>>(std::initializer_list<int>{1, 2, 3, 4, 5});
    auto shared_ptr = std::make_shared<std::string>("Hello, World!");

    std::cout << "Complex type inference completed\n\n";
}

// 3. AUTO IN RANGE-BASED LOOPS (Enhanced)
void enhanced_range_loops() {
    std::cout << "=== ENHANCED RANGE-BASED LOOPS ===\n";

    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    std::map<int, std::string> id_map = {{1, "One"}, {2, "Two"}, {3, "Three"}};

    // Different auto variations in loops
    std::cout << "Copy semantics (auto): ";
    for (auto name : names) {  // Copy each element
        name += "!";  // Modifies copy, not original
        std::cout << name << " ";
    }
    std::cout << "\nOriginal names: ";
    for (const auto& name : names) {
        std::cout << name << " ";
    }

    std::cout << "\n\nReference semantics (auto&): ";
    for (auto& name : names) {  // Reference to each element
        name += "!";  // Modifies original
        std::cout << name << " ";
    }

    std::cout << "\n\nMap iteration with structured binding (C++17): ";
    for (const auto& [key, value] : id_map) {
        std::cout << key << ":" << value << " ";
    }
    std::cout << "\n\n";
}

// 4. AUTO WITH ALGORITHMS AND LAMBDAS
void auto_with_algorithms() {
    std::cout << "=== AUTO WITH ALGORITHMS AND LAMBDAS ===\n";

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Lambda with auto parameters (C++14+)
    auto is_even = [](auto n) { return n % 2 == 0; };
    auto square = [](auto x) { return x * x; };

    // Find using lambda and auto
    auto even_it = std::find_if(numbers.begin(), numbers.end(), is_even);
    if (even_it != numbers.end()) {
        std::cout << "First even number: " << *even_it << "\n";
    }

    // Transform with lambda
    std::vector<int> squared;
    squared.reserve(numbers.size());
    std::transform(numbers.begin(), numbers.end(), std::back_inserter(squared), square);

    std::cout << "Squared numbers: ";
    for (const auto& num : squared) {
        std::cout << num << " ";
    }
    std::cout << "\n\n";
}

// 5. AUTO RETURN TYPES (C++14+)
auto factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

auto get_container() {
    return std::vector<std::string>{"auto", "return", "type"};
}

// Template function with auto return type
template<typename T, typename U>
auto add_different_types(T a, U b) -> decltype(a + b) {
    return a + b;
}

void auto_return_types() {
    std::cout << "=== AUTO RETURN TYPES ===\n";

    auto result1 = factorial(5);
    auto container = get_container();
    auto mixed_result = add_different_types(3, 2.5);

    std::cout << "Factorial of 5: " << result1 << "\n";
    std::cout << "Container size: " << container.size() << "\n";
    std::cout << "Mixed addition (3 + 2.5): " << mixed_result << "\n\n";
}

// 6. AUTO WITH STRUCTURED BINDINGS (C++17+)
void structured_bindings_examples() {
    std::cout << "=== STRUCTURED BINDINGS WITH AUTO ===\n";

    // Tuple decomposition
    auto tuple_data = std::make_tuple(42, "hello", 3.14);
    auto [num, str, pi] = tuple_data;

    // Pair decomposition
    auto pair_data = std::make_pair("key", 100);
    auto [key, value] = pair_data;

    // Array decomposition
    int arr[3] = {1, 2, 3};
    auto [a, b, c] = arr;

    // Custom struct decomposition
    struct Point { int x, y; };
    Point p{10, 20};
    auto [x, y] = p;

    std::cout << "Tuple: " << num << ", " << str << ", " << pi << "\n";
    std::cout << "Pair: " << key << " = " << value << "\n";
    std::cout << "Array: " << a << ", " << b << ", " << c << "\n";
    std::cout << "Point: (" << x << ", " << y << ")\n\n";
}

// 7. AUTO LIMITATIONS AND PITFALLS
void auto_limitations() {
    std::cout << "=== AUTO LIMITATIONS AND PITFALLS ===\n";

    // Braced initializer behavior
    auto list1 = {1, 2, 3};        // std::initializer_list<int>
    auto list2{1};                 // int (C++17+)
    // auto list3{1, 2};           // Error in C++17+

    // Proxy object issues
    std::vector<bool> bool_vec = {true, false, true};
    auto bit_ref = bool_vec[0];    // std::vector<bool>::reference, not bool
    bool actual_bool = bool_vec[0]; // Explicit conversion needed for bool

    // Function pointer vs function reference
    void dummy_func(int) {}
    auto func_ptr = dummy_func;    // void(*)(int) - pointer
    auto& func_ref = dummy_func;   // void(&)(int) - reference

    std::cout << "Be careful with auto and proxy objects!\n";
    std::cout << "Always consider the actual deduced type.\n\n";
}

// 8. UNIVERSAL REFERENCES AND PERFECT FORWARDING
template<typename T>
void universal_reference_demo(T&& param) {
    auto&& local1 = param;         // Universal reference
    auto&& local2 = std::forward<T>(param); // Perfect forwarding

    std::cout << "Universal reference demonstration\n";
}

void forwarding_examples() {
    std::cout << "=== UNIVERSAL REFERENCES ===\n";

    int x = 42;
    universal_reference_demo(x);           // T = int&, param = int&
    universal_reference_demo(std::move(x)); // T = int, param = int&&
    universal_reference_demo(123);         // T = int, param = int&&

    std::cout << "\n";
}

int main() {
    literal_type_deduction();
    literal_contexts();
    literal_suffix_examples();
    auto_deduction_demo();
    complex_type_inference();
    enhanced_range_loops();
    auto_with_algorithms();
    auto_return_types();
    structured_bindings_examples();
    auto_limitations();
    forwarding_examples();

    std::cout << "=== SUMMARY ===\n";
    std::cout << "- auto simplifies code and reduces maintenance burden\n";
    std::cout << "- Be aware of reference and const removal rules\n";
    std::cout << "- Literal types are deduced based on their form and suffixes\n";
    std::cout << "- STRING LITERALS: auto doesn't 'remove' const from string literals\n";
    std::cout << "  because const is intrinsic to their type (const char[N] -> const char*)\n";
    std::cout << "  This is different from const variables where auto removes const qualifiers\n";
    std::cout << "- Use const auto& for read-only access to avoid copies\n";
    std::cout << "- Use auto& when you need to modify the original object\n";
    std::cout << "- Be careful with proxy objects and braced initializers\n";
    std::cout << "- auto is required for lambdas and structured bindings\n";
    std::cout << "- Consider using auto almost everywhere for cleaner code\n";

    return 0;
}
