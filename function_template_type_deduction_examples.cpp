//
// Created by Krapa Haritha on 14/10/25.
/*
===========================================================================================
FUNCTION TEMPLATE TYPE DEDUCTION - COMPREHENSIVE EXAMPLES
===========================================================================================

This file demonstrates various scenarios of function template type deduction in C++11/14/17/20:
1. By Value Parameter Deduction
2. Lvalue Reference Parameter Deduction
3. Rvalue/Universal Reference Parameter Deduction
4. Auto Type Deduction (related)
5. Real-world Use Cases

Key Principles:
- Template argument deduction follows specific rules based on parameter types
- Reference collapsing rules apply to universal references
- CV qualifiers (const/volatile) behavior differs by parameter type
- Perfect forwarding requires understanding of these rules

===========================================================================================
*/

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>
#include <utility>

// ============================================================================
// UTILITY FUNCTIONS FOR TYPE INSPECTION
// ============================================================================

// Helper to print deduced types at compile time
template<typename T>
struct TypeDisplayer;

// Helper to print type information at runtime
template<typename T>
const char* get_type_name() {
    return typeid(T).name();
}

// More readable type display
template<typename T>
void print_type_info(const char* context) {
    std::cout << context << ": ";

    if (std::is_lvalue_reference_v<T>) {
        std::cout << "lvalue reference to ";
        using base_type = std::remove_reference_t<T>;
        if (std::is_const_v<base_type>) std::cout << "const ";
        std::cout << typeid(base_type).name();
    }
    else if (std::is_rvalue_reference_v<T>) {
        std::cout << "rvalue reference to ";
        using base_type = std::remove_reference_t<T>;
        if (std::is_const_v<base_type>) std::cout << "const ";
        std::cout << typeid(base_type).name();
    }
    else {
        if (std::is_const_v<T>) std::cout << "const ";
        std::cout << typeid(T).name() << " (by value)";
    }
    std::cout << std::endl;
}

// ============================================================================
// 1. BY VALUE PARAMETER DEDUCTION
// ============================================================================

/*
BY VALUE DEDUCTION RULES:
- Reference-ness is ignored
- const/volatile are ignored (top-level only)
- Arrays decay to pointers
- Function names decay to function pointers
*/

template<typename T>
void by_value_function(T param) {
    print_type_info<T>("T deduced as");
    std::cout << "  Value: " << param << std::endl;
}

void demonstrate_by_value_deduction() {
    std::cout << "\n=== BY VALUE PARAMETER DEDUCTION ===\n";

    // Case 1: Basic types
    int x = 42;
    const int cx = x;
    const int& rx = x;

    std::cout << "\n1. Basic types:\n";
    by_value_function(x);    // T = int
    by_value_function(cx);   // T = int (const ignored)
    by_value_function(rx);   // T = int (const& becomes int)

    // Case 2: Pointers
    std::cout << "\n2. Pointers:\n";
    const int* px = &cx;
    by_value_function(px);   // T = const int* (pointer to const, not const pointer)

    int* const cpx = &x;
    by_value_function(cpx);  // T = int* (const pointer becomes regular pointer)

    // Case 3: Arrays (decay to pointers)
    std::cout << "\n3. Arrays:\n";
    const char name[] = "J. P. Briggs";
    by_value_function(name); // T = const char*

    int arr[5] = {1, 2, 3, 4, 5};
    by_value_function(arr);  // T = int*
}

// ============================================================================
// 2. LVALUE REFERENCE PARAMETER DEDUCTION
// ============================================================================

/*
LVALUE REFERENCE DEDUCTION RULES:
- Only lvalues can bind to lvalue references
- const-ness is preserved
- Reference collapsing doesn't apply (T& is always T&)
*/

template<typename T>
void lvalue_ref_function(T& param) {
    print_type_info<T>("T deduced as");
    std::cout << "  param type: T&" << std::endl;
    // param can be modified (unless T is const)
}

template<typename T>
void const_lvalue_ref_function(const T& param) {
    print_type_info<T>("T deduced as");
    std::cout << "  param type: const T&" << std::endl;
    // param cannot be modified
}

void demonstrate_lvalue_reference_deduction() {
    std::cout << "\n=== LVALUE REFERENCE PARAMETER DEDUCTION ===\n";

    int x = 42;
    const int cx = x;
    const int& rx = x;

    std::cout << "\n1. Non-const lvalue reference T&:\n";
    lvalue_ref_function(x);    // T = int, param type = int&
    lvalue_ref_function(cx);   // T = const int, param type = const int&
    lvalue_ref_function(rx);   // T = const int, param type = const int&

    // lvalue_ref_function(42);  // ERROR! Can't bind rvalue to lvalue reference

    std::cout << "\n2. Const lvalue reference const T&:\n";
    const_lvalue_ref_function(x);    // T = int, param type = const int&
    const_lvalue_ref_function(cx);   // T = int, param type = const int&
    const_lvalue_ref_function(rx);   // T = int, param type = const int&
    const_lvalue_ref_function(42);   // T = int, param type = const int& (rvalue can bind)

    // Case 3: Arrays don't decay with references
    std::cout << "\n3. Arrays (no decay with references):\n";
    int arr[5] = {1, 2, 3, 4, 5};
    lvalue_ref_function(arr);  // T = int[5], param type = int(&)[5]
}

// ============================================================================
// 3. RVALUE/UNIVERSAL REFERENCE PARAMETER DEDUCTION
// ============================================================================

/*
UNIVERSAL REFERENCE DEDUCTION RULES (T&&):
- If argument is lvalue: T = T&, param type = T& (reference collapsing)
- If argument is rvalue: T = T, param type = T&&
- This enables perfect forwarding
*/

template<typename T>
void universal_ref_function(T&& param) {
    print_type_info<T>("T deduced as");

    if (std::is_lvalue_reference_v<T>) {
        std::cout << "  param type: T& (lvalue reference - reference collapsing)" << std::endl;
    } else {
        std::cout << "  param type: T&& (rvalue reference)" << std::endl;
    }

    // Perfect forwarding demonstration
    std::cout << "  Forwarding as: ";
    if (std::is_lvalue_reference_v<T>) {
        std::cout << "lvalue" << std::endl;
    } else {
        std::cout << "rvalue" << std::endl;
    }
}

void demonstrate_universal_reference_deduction() {
    std::cout << "\n=== UNIVERSAL REFERENCE PARAMETER DEDUCTION (T&&) ===\n";

    int x = 42;
    const int cx = x;
    const int& rx = x;

    std::cout << "\n1. Lvalue arguments:\n";
    universal_ref_function(x);    // T = int&, param type = int& & = int&
    universal_ref_function(cx);   // T = const int&, param type = const int&
    universal_ref_function(rx);   // T = const int&, param type = const int&

    std::cout << "\n2. Rvalue arguments:\n";
    universal_ref_function(42);           // T = int, param type = int&&
    universal_ref_function(std::move(x)); // T = int, param type = int&&

    std::cout << "\n3. String examples:\n";
    std::string name = "Scott";
    universal_ref_function(name);                    // T = std::string&
    universal_ref_function(std::string("Meyers"));   // T = std::string
    universal_ref_function(std::move(name));         // T = std::string
}

// ============================================================================
// 4. REFERENCE COLLAPSING RULES DEMONSTRATION
// ============================================================================

/*
REFERENCE COLLAPSING RULES:
- T& &   → T&    (lvalue ref to lvalue ref = lvalue ref)
- T& &&  → T&    (rvalue ref to lvalue ref = lvalue ref)
- T&& &  → T&    (lvalue ref to rvalue ref = lvalue ref)
- T&& && → T&&   (rvalue ref to rvalue ref = rvalue ref)

Summary: If either reference is lvalue, result is lvalue reference
*/

template<typename T>
void demonstrate_reference_collapsing() {
    std::cout << "\n=== REFERENCE COLLAPSING DEMONSTRATION ===\n";

    using lvalue_ref = T&;
    using rvalue_ref = T&&;

    std::cout << "For type T = " << typeid(T).name() << ":\n";

    // These would be the types after reference collapsing
    std::cout << "T& &   = " << typeid(lvalue_ref&).name() << std::endl;
    std::cout << "T&& &  = " << typeid(rvalue_ref&).name() << std::endl;
    std::cout << "T& &&  = " << typeid(lvalue_ref&&).name() << std::endl;
    std::cout << "T&& && = " << typeid(rvalue_ref&&).name() << std::endl;
}

// ============================================================================
// 5. PERFECT FORWARDING IMPLEMENTATION
// ============================================================================

// Example of perfect forwarding using universal references
template<typename T>
void target_function(T&& param) {
    std::cout << "Target function called with: ";
    print_type_info<T>("parameter type");
}

template<typename T>
void forwarding_function(T&& param) {
    std::cout << "Forwarding function received: ";
    print_type_info<T>("parameter type");

    // Perfect forwarding: preserve value category
    target_function(std::forward<T>(param));
}

void demonstrate_perfect_forwarding() {
    std::cout << "\n=== PERFECT FORWARDING DEMONSTRATION ===\n";

    int x = 42;

    std::cout << "\n1. Forwarding lvalue:\n";
    forwarding_function(x);  // Preserves lvalue-ness

    std::cout << "\n2. Forwarding rvalue:\n";
    forwarding_function(42); // Preserves rvalue-ness

    std::cout << "\n3. Forwarding moved value:\n";
    forwarding_function(std::move(x)); // Preserves rvalue-ness
}

// ============================================================================
// 6. COMMON PITFALLS AND EDGE CASES
// ============================================================================

// Pitfall 1: Auto vs template deduction differences
void demonstrate_auto_vs_template_deduction() {
    std::cout << "\n=== AUTO VS TEMPLATE DEDUCTION DIFFERENCES ===\n";

    // Braced initializers
    auto x1 = {1, 2, 3};  // x1 is std::initializer_list<int>
    std::cout << "auto x1 = {1, 2, 3}; // x1 type: " << typeid(x1).name() << std::endl;

    // Template functions can't deduce from braced initializers
    template<typename T>
    void template_func(T param) {}

    // template_func({1, 2, 3}); // ERROR! Cannot deduce T

    std::cout << "template_func({1, 2, 3}); // ERROR: Cannot deduce T\n";
}

// Pitfall 2: Array and function parameter deduction
template<typename T, std::size_t N>
constexpr std::size_t array_size(T (&)[N]) noexcept {
    return N;
}

// Function for demonstration
void some_function(int) {}

template<typename T>
void func_param_by_value(T param) {
    print_type_info<T>("Function parameter by value");
}

template<typename T>
void func_param_by_ref(T& param) {
    print_type_info<T>("Function parameter by reference");
}

void demonstrate_array_function_deduction() {
    std::cout << "\n=== ARRAY AND FUNCTION DEDUCTION ===\n";

    // Array size deduction
    int arr[10];
    constexpr auto size = array_size(arr);
    std::cout << "Array size deduced: " << size << std::endl;

    std::cout << "\nFunction parameter deduction:\n";
    func_param_by_value(some_function);  // T = void(*)(int)
    func_param_by_ref(some_function);    // T = void(int), param = void(&)(int)
}

// ============================================================================
// 7. REAL-WORLD USE CASES
// ============================================================================

// Use Case 1: Factory function with perfect forwarding
template<typename T, typename... Args>
std::unique_ptr<T> make_unique_wrapper(Args&&... args) {
    std::cout << "Creating object with " << sizeof...(args) << " arguments\n";
    return std::make_unique<T>(std::forward<Args>(args)...);
}

// Use Case 2: Wrapper function that preserves value category
template<typename Func, typename... Args>
auto time_function_call(Func&& func, Args&&... args)
    -> decltype(std::forward<Func>(func)(std::forward<Args>(args)...)) {

    auto start = std::chrono::high_resolution_clock::now();
    auto result = std::forward<Func>(func)(std::forward<Args>(args)...);
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Function execution time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
              << " microseconds\n";

    return result;
}

// Use Case 3: Type-erased function wrapper
class function_wrapper {
private:
    class concept_base {
    public:
        virtual ~concept_base() = default;
        virtual void call() = 0;
    };

    template<typename F>
    class concept_impl : public concept_base {
        F func_;
    public:
        template<typename Func>
        concept_impl(Func&& f) : func_(std::forward<Func>(f)) {}

        void call() override { func_(); }
    };

    std::unique_ptr<concept_base> pimpl_;

public:
    template<typename F>
    function_wrapper(F&& f) : pimpl_(std::make_unique<concept_impl<std::decay_t<F>>>(std::forward<F>(f))) {}

    void operator()() { pimpl_->call(); }
};

void demonstrate_real_world_use_cases() {
    std::cout << "\n=== REAL-WORLD USE CASES ===\n";

    // Use Case 1: Factory function
    std::cout << "\n1. Factory function with perfect forwarding:\n";
    auto vec_ptr = make_unique_wrapper<std::vector<int>>(10, 42);
    std::cout << "Created vector with size: " << vec_ptr->size() << std::endl;

    // Use Case 2: Function timing wrapper
    std::cout << "\n2. Function timing wrapper:\n";
    auto expensive_computation = []() -> int {
        int sum = 0;
        for (int i = 0; i < 1000000; ++i) {
            sum += i;
        }
        return sum;
    };

    auto result = time_function_call(expensive_computation);
    std::cout << "Computation result: " << result << std::endl;

    // Use Case 3: Type-erased wrapper
    std::cout << "\n3. Type-erased function wrapper:\n";
    function_wrapper wrapper([]() { std::cout << "Lambda executed!\n"; });
    wrapper();
}

// ============================================================================
// 8. C++20 CONCEPTS WITH TEMPLATE DEDUCTION
// ============================================================================

#if __cplusplus >= 202002L
#include <concepts>

// Concept-constrained template deduction
template<std::integral T>
void constrained_function(T value) {
    std::cout << "Integral value: " << value << std::endl;
}

template<std::floating_point T>
void constrained_function(T value) {
    std::cout << "Floating-point value: " << value << std::endl;
}

void demonstrate_cpp20_concepts() {
    std::cout << "\n=== C++20 CONCEPTS WITH TEMPLATE DEDUCTION ===\n";

    constrained_function(42);      // Calls integral version
    constrained_function(3.14);    // Calls floating-point version
    // constrained_function("hello"); // ERROR: No matching overload
}
#endif

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "FUNCTION TEMPLATE TYPE DEDUCTION EXAMPLES\n";
    std::cout << "==========================================\n";

    // Demonstrate each type deduction scenario
    demonstrate_by_value_deduction();
    demonstrate_lvalue_reference_deduction();
    demonstrate_universal_reference_deduction();

    // Reference collapsing
    demonstrate_reference_collapsing<int>();

    // Perfect forwarding
    demonstrate_perfect_forwarding();

    // Common pitfalls
    demonstrate_auto_vs_template_deduction();
    demonstrate_array_function_deduction();

    // Real-world use cases
    demonstrate_real_world_use_cases();

    // C++20 concepts (if available)
#if __cplusplus >= 202002L
    demonstrate_cpp20_concepts();
#endif

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Key takeaways:\n";
    std::cout << "1. By-value deduction ignores references and top-level const\n";
    std::cout << "2. Lvalue reference deduction preserves const-ness\n";
    std::cout << "3. Universal references enable perfect forwarding via reference collapsing\n";
    std::cout << "4. Understanding these rules is crucial for writing efficient generic code\n";

    return 0;
}

/*
===========================================================================================
COMPILATION AND TESTING NOTES
===========================================================================================

Compile with:
g++ -std=c++17 -O2 function_template_type_deduction_examples.cpp -o template_deduction

For C++20 features:
g++ -std=c++20 -O2 function_template_type_deduction_examples.cpp -o template_deduction

Expected output will show:
- Detailed type deduction for each scenario
- Demonstration of reference collapsing
- Perfect forwarding examples
- Real-world use cases
- Performance timing for template instantiation

Key learning points:
1. Template argument deduction follows specific rules based on parameter form
2. Universal references (T&&) are the foundation of perfect forwarding
3. Reference collapsing rules determine final parameter types
4. Understanding these concepts is essential for modern C++ generic programming

===========================================================================================
*/
