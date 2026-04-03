//

/*
===========================================================================================
SFINAE (SUBSTITUTION FAILURE IS NOT AN ERROR) - COMPREHENSIVE EXAMPLES
===========================================================================================

SFINAE is a fundamental C++ template metaprogramming technique that allows templates to
gracefully handle type substitution failures during template argument deduction.

Key Principle: When substituting template parameters fails, the compiler doesn't
immediately error - instead, it removes that overload from consideration.

This file demonstrates:
1. Basic SFINAE concepts and syntax
2. std::enable_if patterns
3. Expression SFINAE (C++11)
4. Detection idioms
5. Tag dispatching with SFINAE
6. SFINAE vs modern alternatives (if constexpr, concepts)
7. Real-world use cases
8. Performance considerations

===========================================================================================
*/

#include <iostream>
#include <type_traits>
#include <string>
#include <vector>
#include <iterator>
#include <utility>
#include <memory>
#include <chrono>

// ============================================================================
// 1. BASIC SFINAE CONCEPTS
// ============================================================================

namespace basic_sfinae {

// Example 1: Simple SFINAE with std::enable_if
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
safe_divide(T a, T b) {
    std::cout << "Integer division: ";
    return (b != 0) ? a / b : 0;
}

template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
safe_divide(T a, T b) {
    std::cout << "Floating-point division: ";
    return (b != 0.0) ? a / b : 0.0;
}

// C++14 shorthand with std::enable_if_t
template<typename T>
std::enable_if_t<std::is_same_v<T, std::string>, std::string>
safe_divide(const T& a, const T& b) {
    std::cout << "String 'division' (concatenation): ";
    return a + " / " + b;
}

void demonstrate_basic_sfinae() {
    std::cout << "\n=== BASIC SFINAE EXAMPLES ===\n";

    auto result1 = safe_divide(10, 3);
    std::cout << result1 << std::endl;

    auto result2 = safe_divide(10.5, 3.2);
    std::cout << result2 << std::endl;

    auto result3 = safe_divide(std::string("Hello"), std::string("World"));
    std::cout << result3 << std::endl;
}

} // namespace basic_sfinae

// ============================================================================
// 2. EXPRESSION SFINAE - DETECTING MEMBER FUNCTIONS
// ============================================================================

namespace expression_sfinae {

// SFINAE to detect if a type has a size() method
template<typename T>
class has_size_method {
private:
    // This will fail substitution for types without size() method
    template<typename U>
    static auto test(int) -> decltype(std::declval<U>().size(), std::true_type{});

    // Fallback - always matches but with lower priority
    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
constexpr bool has_size_method_v = has_size_method<T>::value;

// SFINAE to detect if a type has push_back method
template<typename T>
class has_push_back {
private:
    template<typename U>
    static auto test(int) -> decltype(
        std::declval<U>().push_back(std::declval<typename U::value_type>()),
        std::true_type{}
    );

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
constexpr bool has_push_back_v = has_push_back<T>::value;

// SFINAE to detect if a type is iterable
template<typename T>
class is_iterable {
private:
    template<typename U>
    static auto test(int) -> decltype(
        std::begin(std::declval<U>()),
        std::end(std::declval<U>()),
        std::true_type{}
    );

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
constexpr bool is_iterable_v = is_iterable<T>::value;

// Function that works differently based on container capabilities
template<typename Container>
std::enable_if_t<has_size_method_v<Container>, size_t>
get_container_info(const Container& c) {
    std::cout << "Container with size() method - size: ";
    return c.size();
}

template<typename Container>
std::enable_if_t<!has_size_method_v<Container> && is_iterable_v<Container>, size_t>
get_container_info(const Container& c) {
    std::cout << "Iterable container without size() - counted size: ";
    return std::distance(std::begin(c), std::end(c));
}

template<typename T>
std::enable_if_t<!is_iterable_v<T>, size_t>
get_container_info(const T&) {
    std::cout << "Not a container - returning 0: ";
    return 0;
}

void demonstrate_expression_sfinae() {
    std::cout << "\n=== EXPRESSION SFINAE - MEMBER DETECTION ===\n";

    std::vector<int> vec{1, 2, 3, 4, 5};
    int arr[] = {1, 2, 3};
    int single_value = 42;

    std::cout << "Vector: " << get_container_info(vec) << std::endl;
    std::cout << "Array: " << get_container_info(arr) << std::endl;
    std::cout << "Single value: " << get_container_info(single_value) << std::endl;

    std::cout << "\nType traits:\n";
    std::cout << "std::vector has size(): " << has_size_method_v<std::vector<int>> << std::endl;
    std::cout << "std::vector has push_back(): " << has_push_back_v<std::vector<int>> << std::endl;
    std::cout << "int[] is iterable: " << is_iterable_v<int[]> << std::endl;
    std::cout << "int is iterable: " << is_iterable_v<int> << std::endl;
}

} // namespace expression_sfinae

// ============================================================================
// 3. SFINAE WITH RETURN TYPE DEDUCTION
// ============================================================================

namespace return_type_sfinae {

// Different implementations based on iterator category
template<typename Iterator>
auto advance_impl(Iterator& it, std::ptrdiff_t n, std::random_access_iterator_tag)
    -> decltype(it += n, void()) {
    it += n;
    std::cout << "Used random access advance (O(1))\n";
}

template<typename Iterator>
auto advance_impl(Iterator& it, std::ptrdiff_t n, std::bidirectional_iterator_tag)
    -> decltype(++it, --it, void()) {
    if (n >= 0) {
        while (n-- > 0) ++it;
    } else {
        while (n++ < 0) --it;
    }
    std::cout << "Used bidirectional advance (O(n))\n";
}

template<typename Iterator>
auto advance_impl(Iterator& it, std::ptrdiff_t n, std::input_iterator_tag)
    -> decltype(++it, void()) {
    while (n-- > 0) ++it;
    std::cout << "Used input iterator advance (O(n), forward only)\n";
}

template<typename Iterator>
void smart_advance(Iterator& it, std::ptrdiff_t n) {
    advance_impl(it, n, typename std::iterator_traits<Iterator>::iterator_category{});
}

// SFINAE for different serialization strategies
template<typename T>
auto serialize(const T& obj) -> decltype(obj.serialize(), std::string{}) {
    return "Custom: " + obj.serialize();
}

template<typename T>
auto serialize(const T& obj) -> std::enable_if_t<std::is_arithmetic_v<T>, std::string> {
    return "Arithmetic: " + std::to_string(obj);
}

template<typename T>
auto serialize(const T& obj) -> std::enable_if_t<
    !std::is_arithmetic_v<T> &&
    std::is_convertible_v<T, std::string>,
    std::string
> {
    return "String-like: " + std::string(obj);
}

// Custom class with serialize method
class CustomObject {
public:
    std::string serialize() const {
        return "CustomObject data";
    }
};

void demonstrate_return_type_sfinae() {
    std::cout << "\n=== RETURN TYPE SFINAE ===\n";

    // Iterator advancement
    std::vector<int> vec{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto it1 = vec.begin();
    smart_advance(it1, 3);
    std::cout << "Vector iterator advanced to: " << *it1 << std::endl;

    std::list<int> lst{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto it2 = lst.begin();
    smart_advance(it2, 3);
    std::cout << "List iterator advanced to: " << *it2 << std::endl;

    // Serialization
    CustomObject obj;
    int number = 42;
    std::string text = "Hello";

    std::cout << serialize(obj) << std::endl;
    std::cout << serialize(number) << std::endl;
    std::cout << serialize(text) << std::endl;
}

} // namespace return_type_sfinae

// ============================================================================
// 4. SFINAE VS TAG DISPATCHING
// ============================================================================

namespace sfinae_vs_tag_dispatch {

// SFINAE approach
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
process_sfinae(T value) {
    std::cout << "SFINAE: Processing integer: ";
    return value * 2;
}

template<typename T>
std::enable_if_t<std::is_floating_point_v<T>, T>
process_sfinae(T value) {
    std::cout << "SFINAE: Processing float: ";
    return value * 1.5;
}

// Tag dispatching approach
namespace tags {
    struct integral_tag {};
    struct floating_point_tag {};
    struct other_tag {};
}

template<typename T>
constexpr auto get_type_tag() {
    if constexpr (std::is_integral_v<T>) {
        return tags::integral_tag{};
    } else if constexpr (std::is_floating_point_v<T>) {
        return tags::floating_point_tag{};
    } else {
        return tags::other_tag{};
    }
}

template<typename T>
T process_impl(T value, tags::integral_tag) {
    std::cout << "Tag dispatch: Processing integer: ";
    return value * 2;
}

template<typename T>
T process_impl(T value, tags::floating_point_tag) {
    std::cout << "Tag dispatch: Processing float: ";
    return value * 1.5;
}

template<typename T>
T process_impl(T value, tags::other_tag) {
    std::cout << "Tag dispatch: Processing other type: ";
    return value;
}

template<typename T>
auto process_tag_dispatch(T value) {
    return process_impl(value, get_type_tag<T>());
}

void demonstrate_sfinae_vs_tag_dispatch() {
    std::cout << "\n=== SFINAE VS TAG DISPATCHING ===\n";

    int int_val = 10;
    double double_val = 3.14;

    std::cout << process_sfinae(int_val) << std::endl;
    std::cout << process_sfinae(double_val) << std::endl;

    std::cout << process_tag_dispatch(int_val) << std::endl;
    std::cout << process_tag_dispatch(double_val) << std::endl;
}

} // namespace sfinae_vs_tag_dispatch

// ============================================================================
// 5. SFINAE WITH TEMPLATE SPECIALIZATION
// ============================================================================

namespace template_specialization_sfinae {

// Primary template
template<typename T, typename Enable = void>
struct TypeProcessor {
    static std::string process(const T&) {
        return "Generic processing";
    }
};

// Specialization for arithmetic types
template<typename T>
struct TypeProcessor<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static std::string process(const T& value) {
        return "Arithmetic: " + std::to_string(value);
    }
};

// Specialization for string-like types
template<typename T>
struct TypeProcessor<T, std::enable_if_t<
    std::is_convertible_v<T, std::string> &&
    !std::is_arithmetic_v<T>
>> {
    static std::string process(const T& value) {
        return "String-like: " + std::string(value);
    }
};

// Specialization for containers
template<typename T>
struct TypeProcessor<T, std::enable_if_t<
    expression_sfinae::has_size_method_v<T> &&
    expression_sfinae::is_iterable_v<T>
>> {
    static std::string process(const T& container) {
        return "Container with " + std::to_string(container.size()) + " elements";
    }
};

void demonstrate_template_specialization_sfinae() {
    std::cout << "\n=== TEMPLATE SPECIALIZATION WITH SFINAE ===\n";

    int number = 42;
    std::string text = "Hello World";
    std::vector<int> vec{1, 2, 3, 4, 5};

    struct CustomType {} custom;

    std::cout << TypeProcessor<int>::process(number) << std::endl;
    std::cout << TypeProcessor<std::string>::process(text) << std::endl;
    std::cout << TypeProcessor<std::vector<int>>::process(vec) << std::endl;
    std::cout << TypeProcessor<CustomType>::process(custom) << std::endl;
}

} // namespace template_specialization_sfinae

// ============================================================================
// 6. MODERN ALTERNATIVES TO SFINAE
// ============================================================================

namespace modern_alternatives {

// C++17: if constexpr replaces many SFINAE use cases
template<typename T>
auto modern_process(T value) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "Modern: Processing integer: ";
        return value * 2;
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "Modern: Processing float: ";
        return value * 1.5;
    } else if constexpr (std::is_convertible_v<T, std::string>) {
        std::cout << "Modern: Processing string-like: ";
        return std::string(value) + " (processed)";
    } else {
        std::cout << "Modern: Cannot process this type\n";
        return value;
    }
}

#if __cplusplus >= 202002L
// C++20: Concepts provide cleaner syntax
#include <concepts>

template<std::integral T>
T concept_process(T value) {
    std::cout << "Concept: Processing integer: ";
    return value * 2;
}

template<std::floating_point T>
T concept_process(T value) {
    std::cout << "Concept: Processing float: ";
    return value * 1.5;
}

// Custom concept
template<typename T>
concept StringLike = std::convertible_to<T, std::string> &&
                    !std::arithmetic<T>;

template<StringLike T>
std::string concept_process(const T& value) {
    std::cout << "Concept: Processing string-like: ";
    return std::string(value) + " (processed)";
}
#endif

void demonstrate_modern_alternatives() {
    std::cout << "\n=== MODERN ALTERNATIVES TO SFINAE ===\n";

    int int_val = 20;
    double double_val = 2.71;
    std::string string_val = "Modern C++";

    // C++17 if constexpr
    std::cout << modern_process(int_val) << std::endl;
    std::cout << modern_process(double_val) << std::endl;
    std::cout << modern_process(string_val) << std::endl;

#if __cplusplus >= 202002L
    // C++20 concepts
    std::cout << concept_process(int_val) << std::endl;
    std::cout << concept_process(double_val) << std::endl;
    std::cout << concept_process(string_val) << std::endl;
#else
    std::cout << "C++20 concepts not available in this compiler\n";
#endif
}

} // namespace modern_alternatives

// ============================================================================
// 7. REAL-WORLD USE CASES
// ============================================================================

namespace real_world_examples {

// Use case 1: Generic algorithm that adapts to container capabilities
template<typename Container, typename Value>
std::enable_if_t<expression_sfinae::has_push_back_v<Container>>
generic_add(Container& container, const Value& value) {
    container.push_back(value);
    std::cout << "Added using push_back\n";
}

template<typename Container, typename Value>
std::enable_if_t<
    !expression_sfinae::has_push_back_v<Container> &&
    expression_sfinae::is_iterable_v<Container>
>
generic_add(Container& container, const Value& value) {
    container.insert(value);
    std::cout << "Added using insert\n";
}

// Use case 2: Smart pointer detection and handling
template<typename T>
class is_smart_pointer : public std::false_type {};

template<typename T>
class is_smart_pointer<std::unique_ptr<T>> : public std::true_type {};

template<typename T>
class is_smart_pointer<std::shared_ptr<T>> : public std::true_type {};

template<typename T>
constexpr bool is_smart_pointer_v = is_smart_pointer<T>::value;

template<typename T>
std::enable_if_t<is_smart_pointer_v<T>, void>
safe_use(const T& ptr) {
    if (ptr) {
        std::cout << "Smart pointer is valid, using it\n";
        // Use *ptr or ptr->method()
    } else {
        std::cout << "Smart pointer is null\n";
    }
}

template<typename T>
std::enable_if_t<!is_smart_pointer_v<T>, void>
safe_use(const T& obj) {
    std::cout << "Regular object, using directly\n";
    // Use obj directly
}

// Use case 3: Conditional member function availability
template<typename T>
class ConditionalAPI {
public:
    T data_;

    ConditionalAPI(T data) : data_(std::move(data)) {}

    // Only available for numeric types
    template<typename U = T>
    std::enable_if_t<std::is_arithmetic_v<U>, U>
    get_double() const {
        return data_ * 2;
    }

    // Only available for containers
    template<typename U = T>
    std::enable_if_t<expression_sfinae::has_size_method_v<U>, size_t>
    get_size() const {
        return data_.size();
    }

    // Only available for string-like types
    template<typename U = T>
    std::enable_if_t<std::is_convertible_v<U, std::string>, std::string>
    get_upper() const {
        std::string str = data_;
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
    }
};

void demonstrate_real_world_examples() {
    std::cout << "\n=== REAL-WORLD SFINAE USE CASES ===\n";

    // Generic container operations
    std::vector<int> vec;
    std::set<int> set;

    generic_add(vec, 42);
    generic_add(set, 42);

    // Smart pointer handling
    auto smart_ptr = std::make_unique<int>(42);
    int regular_value = 42;

    safe_use(smart_ptr);
    safe_use(regular_value);

    // Conditional API
    ConditionalAPI<int> api_int(42);
    ConditionalAPI<std::vector<int>> api_vec(std::vector<int>{1, 2, 3});
    ConditionalAPI<std::string> api_str(std::string("hello"));

    std::cout << "Integer API - double: " << api_int.get_double() << std::endl;
    std::cout << "Vector API - size: " << api_vec.get_size() << std::endl;
    std::cout << "String API - upper: " << api_str.get_upper() << std::endl;
}

} // namespace real_world_examples

// ============================================================================
// 8. PERFORMANCE CONSIDERATIONS
// ============================================================================

namespace performance_considerations {

// Performance comparison: SFINAE vs if constexpr vs concepts
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
sfinae_multiply(T value) {
    return value * 2;
}

template<typename T>
std::enable_if_t<std::is_floating_point_v<T>, T>
sfinae_multiply(T value) {
    return static_cast<T>(value * 1.5);
}

template<typename T>
auto constexpr_multiply(T value) {
    if constexpr (std::is_integral_v<T>) {
        return value * 2;
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(value * 1.5);
    }
    return value;
}

void performance_benchmark() {
    std::cout << "\n=== PERFORMANCE CONSIDERATIONS ===\n";

    const int iterations = 10000000;
    int int_value = 42;
    double double_value = 3.14;

    // SFINAE performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        volatile auto result1 = sfinae_multiply(int_value);
        volatile auto result2 = sfinae_multiply(double_value);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto sfinae_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // if constexpr performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        volatile auto result1 = constexpr_multiply(int_value);
        volatile auto result2 = constexpr_multiply(double_value);
    }
    end = std::chrono::high_resolution_clock::now();
    auto constexpr_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "SFINAE time: " << sfinae_time.count() << " μs\n";
    std::cout << "constexpr if time: " << constexpr_time.count() << " μs\n";
    std::cout << "Note: Both should compile to identical optimized assembly\n";

    std::cout << "\nKey performance insights:\n";
    std::cout << "1. SFINAE has no runtime overhead when optimized\n";
    std::cout << "2. Compile-time impact: SFINAE > if constexpr > concepts\n";
    std::cout << "3. Error message quality: concepts > if constexpr > SFINAE\n";
    std::cout << "4. Code readability: concepts > if constexpr > SFINAE\n";
}

} // namespace performance_considerations

// ============================================================================
// 9. COMMON PITFALLS AND BEST PRACTICES
// ============================================================================

namespace pitfalls_and_practices {

// Pitfall 1: Overly complex SFINAE expressions
// BAD: Complex nested SFINAE
template<typename T>
typename std::enable_if<
    std::is_arithmetic<T>::value &&
    !std::is_same<T, bool>::value &&
    (std::is_integral<T>::value || std::is_floating_point<T>::value),
    T
>::type
complex_bad_example(T value) {
    return value;
}

// GOOD: Use type traits and clear naming
template<typename T>
using is_numeric = std::conjunction<
    std::is_arithmetic<T>,
    std::negation<std::is_same<T, bool>>
>;

template<typename T>
std::enable_if_t<is_numeric<T>::value, T>
complex_good_example(T value) {
    return value;
}

// Pitfall 2: Hard substitution failures vs soft failures
namespace substitution_failures {
    // This causes hard error - compilation failure
    template<typename T>
    void hard_failure_example(T) {
        static_assert(std::is_integral_v<T>, "Must be integral");  // Hard error
    }

    // This causes soft failure - SFINAE kicks in
    template<typename T>
    std::enable_if_t<std::is_integral_v<T>>
    soft_failure_example(T) {
        // Only instantiated for integral types
    }
}

// Best practice: Use detection idioms for complex checks
namespace detection_idiom {
    // Standard detection idiom pattern
    template<typename T, typename = void>
    struct has_serialize : std::false_type {};

    template<typename T>
    struct has_serialize<T, std::void_t<decltype(std::declval<T>().serialize())>>
        : std::true_type {};

    template<typename T>
    constexpr bool has_serialize_v = has_serialize<T>::value;

    // Usage with SFINAE
    template<typename T>
    std::enable_if_t<has_serialize_v<T>, std::string>
    get_serialized(const T& obj) {
        return obj.serialize();
    }

    template<typename T>
    std::enable_if_t<!has_serialize_v<T>, std::string>
    get_serialized(const T&) {
        return "Object without serialize method";
    }
}

void demonstrate_pitfalls_and_practices() {
    std::cout << "\n=== COMMON PITFALLS AND BEST PRACTICES ===\n";

    std::cout << "1. Use clear, well-named type traits\n";
    std::cout << "2. Prefer detection idioms for complex member detection\n";
    std::cout << "3. Consider modern alternatives (if constexpr, concepts)\n";
    std::cout << "4. Be aware of soft vs hard substitution failures\n";
    std::cout << "5. Use std::void_t for detection idioms\n";

    // Example of good practice
    int number = 42;
    std::cout << "Result: " << complex_good_example(number) << std::endl;

    // Detection idiom example
    struct HasSerialize {
        std::string serialize() const { return "serialized"; }
    };

    struct NoSerialize {};

    HasSerialize obj1;
    NoSerialize obj2;

    std::cout << detection_idiom::get_serialized(obj1) << std::endl;
    std::cout << detection_idiom::get_serialized(obj2) << std::endl;
}

} // namespace pitfalls_and_practices

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "SFINAE (SUBSTITUTION FAILURE IS NOT AN ERROR) EXAMPLES\n";
    std::cout << "======================================================\n";

    // Demonstrate all SFINAE concepts
    basic_sfinae::demonstrate_basic_sfinae();
    expression_sfinae::demonstrate_expression_sfinae();
    return_type_sfinae::demonstrate_return_type_sfinae();
    sfinae_vs_tag_dispatch::demonstrate_sfinae_vs_tag_dispatch();
    template_specialization_sfinae::demonstrate_template_specialization_sfinae();
    modern_alternatives::demonstrate_modern_alternatives();
    real_world_examples::demonstrate_real_world_examples();
    performance_considerations::performance_benchmark();
    pitfalls_and_practices::demonstrate_pitfalls_and_practices();

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Key SFINAE concepts:\n";
    std::cout << "1. SFINAE allows graceful handling of template substitution failures\n";
    std::cout << "2. std::enable_if is the primary tool for SFINAE\n";
    std::cout << "3. Expression SFINAE can detect member functions/types\n";
    std::cout << "4. Modern C++ provides alternatives: if constexpr (C++17), concepts (C++20)\n";
    std::cout << "5. SFINAE has no runtime cost but affects compilation time\n";
    std::cout << "6. Use detection idioms for complex type checking\n";
    std::cout << "7. Consider readability and maintainability when choosing techniques\n";

    return 0;
}

/*
===========================================================================================
COMPILATION AND TESTING NOTES
===========================================================================================

Compile with:
g++ -std=c++17 -O2 sfinae_use_cases_examples.cpp -o sfinae_demo

For C++20 features (concepts):
g++ -std=c++20 -O2 sfinae_use_cases_examples.cpp -o sfinae_demo

Expected output will demonstrate:
- Basic SFINAE patterns with std::enable_if
- Expression SFINAE for member function detection
- Return type SFINAE for algorithm selection
- Comparison with tag dispatching
- Template specialization with SFINAE
- Modern alternatives (if constexpr, concepts)
- Real-world use cases and patterns
- Performance characteristics
- Common pitfalls and best practices

Key learning points:
1. SFINAE is fundamental to C++ template metaprogramming
2. Enables conditional compilation based on type properties
3. Essential for writing flexible, generic code
4. Modern C++ provides cleaner alternatives for many use cases
5. Understanding SFINAE helps with template error messages
6. Critical for library development and generic programming

===========================================================================================
*/
