#include <iostream>
#include <type_traits>
#include <memory>
#include <functional>

/*
 * nullptr vs NULL vs 0 - Comprehensive Guide
 *
 * Key Points:
 * - nullptr: C++11 keyword, type std::nullptr_t, only converts to pointer types
 * - NULL: Macro, typically defined as 0 or ((void*)0), can cause ambiguity
 * - 0: Integer literal, can convert to both pointers and integers
 */

// =============================================================================
// 1. BASIC DEFINITIONS AND TYPES
// =============================================================================

void demonstrate_basic_types() {
    std::cout << "\n=== Basic Types and Definitions ===\n";

    // nullptr is a keyword with type std::nullptr_t
    auto null_ptr = nullptr;
    std::cout << "Type of nullptr: " << typeid(nullptr).name() << "\n";
    std::cout << "Type of null_ptr: " << typeid(null_ptr).name() << "\n";

    // NULL is a macro (usually 0 or ((void*)0))
    auto null_macro = NULL;
    std::cout << "Type of NULL: " << typeid(NULL).name() << "\n";
    std::cout << "Type of null_macro: " << typeid(null_macro).name() << "\n";

    // 0 is an integer literal
    auto zero = 0;
    std::cout << "Type of 0: " << typeid(0).name() << "\n";
    std::cout << "Type of zero: " << typeid(zero).name() << "\n";

    // Size comparison
    std::cout << "Size of nullptr: " << sizeof(nullptr) << " bytes\n";
    std::cout << "Size of NULL: " << sizeof(NULL) << " bytes\n";
    std::cout << "Size of 0: " << sizeof(0) << " bytes\n";
}

// =============================================================================
// 2. FUNCTION OVERLOADING AMBIGUITY ISSUES
// =============================================================================

void process_pointer(int* ptr) {
    std::cout << "Called process_pointer(int*)\n";
}

void process_integer(int value) {
    std::cout << "Called process_integer(int)\n";
}

void demonstrate_overloading_issues() {
    std::cout << "\n=== Function Overloading Ambiguity ===\n";

    // This works correctly - calls pointer version
    process_pointer(nullptr);

    // These are ambiguous and won't compile:
    // process_pointer(NULL);  // ERROR: ambiguous call
    // process_pointer(0);     // ERROR: ambiguous call

    // To resolve ambiguity with NULL/0, explicit cast needed:
    process_pointer(static_cast<int*>(NULL));
    process_pointer(static_cast<int*>(0));

    // Or call integer version explicitly:
    process_integer(NULL);
    process_integer(0);
}

// =============================================================================
// 3. TEMPLATE DEDUCTION DIFFERENCES
// =============================================================================

template<typename T>
void template_function(T param) {
    std::cout << "Template called with type: " << typeid(T).name() << "\n";

    if constexpr (std::is_pointer_v<T>) {
        std::cout << "Parameter is a pointer type\n";
    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        std::cout << "Parameter is nullptr_t\n";
    } else if constexpr (std::is_integral_v<T>) {
        std::cout << "Parameter is an integral type\n";
    }
}

void demonstrate_template_deduction() {
    std::cout << "\n=== Template Type Deduction ===\n";

    template_function(nullptr);    // Deduces std::nullptr_t
    template_function(NULL);       // Deduces int (or long)
    template_function(0);          // Deduces int

    int* ptr = nullptr;
    template_function(ptr);        // Deduces int*
}

// =============================================================================
// 4. PERFECT FORWARDING SCENARIOS
// =============================================================================

template<typename T>
void forward_to_function(T&& param) {
    std::cout << "Forwarding type: " << typeid(T).name() << "\n";

    // Forward to another function
    if constexpr (std::is_same_v<std::decay_t<T>, std::nullptr_t>) {
        std::cout << "Forwarding nullptr_t\n";
    } else if constexpr (std::is_pointer_v<std::decay_t<T>>) {
        std::cout << "Forwarding pointer\n";
    } else {
        std::cout << "Forwarding non-pointer\n";
    }
}

void demonstrate_perfect_forwarding() {
    std::cout << "\n=== Perfect Forwarding ===\n";

    forward_to_function(nullptr);
    forward_to_function(NULL);
    forward_to_function(0);

    int* ptr = nullptr;
    forward_to_function(ptr);
    forward_to_function(std::move(ptr));
}

// =============================================================================
// 5. COMPARISON AND EQUALITY
// =============================================================================

void demonstrate_comparisons() {
    std::cout << "\n=== Comparisons and Equality ===\n";

    int* ptr1 = nullptr;
    int* ptr2 = NULL;
    int* ptr3 = 0;

    // All these comparisons are true
    std::cout << "ptr1 == nullptr: " << (ptr1 == nullptr) << "\n";
    std::cout << "ptr2 == nullptr: " << (ptr2 == nullptr) << "\n";
    std::cout << "ptr3 == nullptr: " << (ptr3 == nullptr) << "\n";

    std::cout << "ptr1 == NULL: " << (ptr1 == NULL) << "\n";
    std::cout << "ptr2 == NULL: " << (ptr2 == NULL) << "\n";
    std::cout << "ptr3 == NULL: " << (ptr3 == NULL) << "\n";

    // Direct comparisons
    std::cout << "nullptr == NULL: " << (nullptr == NULL) << "\n";
    std::cout << "nullptr == 0: " << (nullptr == 0) << "\n";

    // Boolean conversion
    std::cout << "!nullptr: " << !nullptr << "\n";
    std::cout << "!NULL: " << !NULL << "\n";
    std::cout << "!0: " << !0 << "\n";
}

// =============================================================================
// 6. SMART POINTER INTERACTIONS
// =============================================================================

void demonstrate_smart_pointers() {
    std::cout << "\n=== Smart Pointer Interactions ===\n";

    // unique_ptr initialization
    std::unique_ptr<int> up1(nullptr);     // Clear intent
    std::unique_ptr<int> up2(NULL);        // Works but not ideal
    // std::unique_ptr<int> up3(0);        // ERROR: won't compile

    // shared_ptr initialization
    std::shared_ptr<int> sp1(nullptr);     // Clear intent
    std::shared_ptr<int> sp2(NULL);        // Works but not ideal

    // Reset operations
    up1.reset(nullptr);     // Clear
    up1.reset(NULL);        // Less clear

    // Comparisons
    std::cout << "up1 == nullptr: " << (up1 == nullptr) << "\n";
    std::cout << "sp1 == nullptr: " << (sp1 == nullptr) << "\n";

    // Assignment
    up1 = nullptr;          // Clear intent
    sp1 = nullptr;          // Clear intent
}

// =============================================================================
// 7. FUNCTION POINTER SCENARIOS
// =============================================================================

void sample_function() {
    std::cout << "Sample function called\n";
}

void demonstrate_function_pointers() {
    std::cout << "\n=== Function Pointer Scenarios ===\n";

    // Function pointer declarations
    void (*func_ptr1)() = nullptr;     // Clear null function pointer
    void (*func_ptr2)() = NULL;        // Works but less clear
    void (*func_ptr3)() = 0;           // Works but ambiguous

    // Assignment
    func_ptr1 = sample_function;
    func_ptr1 = nullptr;               // Clear reset

    // Conditionals
    if (func_ptr1 != nullptr) {
        func_ptr1();
    }

    // std::function with nullptr
    std::function<void()> std_func = nullptr;
    std::cout << "std_func is null: " << !std_func << "\n";
}

// =============================================================================
// 8. MEMBER POINTER SCENARIOS
// =============================================================================

struct TestClass {
    int member_var = 42;
    void member_func() { std::cout << "Member function called\n"; }
};

void demonstrate_member_pointers() {
    std::cout << "\n=== Member Pointer Scenarios ===\n";

    // Member variable pointers
    int TestClass::* mem_var_ptr1 = nullptr;
    int TestClass::* mem_var_ptr2 = NULL;
    int TestClass::* mem_var_ptr3 = 0;

    // Member function pointers
    void (TestClass::* mem_func_ptr1)() = nullptr;
    void (TestClass::* mem_func_ptr2)() = NULL;
    void (TestClass::* mem_func_ptr3)() = 0;

    // Assignment
    mem_var_ptr1 = &TestClass::member_var;
    mem_func_ptr1 = &TestClass::member_func;

    // Usage
    TestClass obj;
    if (mem_var_ptr1 != nullptr) {
        std::cout << "Member value: " << obj.*mem_var_ptr1 << "\n";
    }

    if (mem_func_ptr1 != nullptr) {
        (obj.*mem_func_ptr1)();
    }

    // Reset
    mem_var_ptr1 = nullptr;
    mem_func_ptr1 = nullptr;
}

// =============================================================================
// 9. ARRAY AND POINTER ARITHMETIC
// =============================================================================

void demonstrate_arrays_and_arithmetic() {
    std::cout << "\n=== Arrays and Pointer Arithmetic ===\n";

    int arr[5] = {1, 2, 3, 4, 5};
    int* ptr = arr;

    // Pointer arithmetic with nullptr
    int* null_ptr = nullptr;

    // These are undefined behavior but demonstrate the concepts:
    // null_ptr + 0;     // Still nullptr
    // null_ptr + 1;     // Undefined behavior

    // Safe comparisons
    std::cout << "ptr != nullptr: " << (ptr != nullptr) << "\n";
    std::cout << "null_ptr == nullptr: " << (null_ptr == nullptr) << "\n";

    // Array bounds checking
    for (int* p = arr; p != arr + 5; ++p) {
        if (p != nullptr) {  // Always true for valid array elements
            std::cout << *p << " ";
        }
    }
    std::cout << "\n";
}

// =============================================================================
// 10. EXCEPTION SCENARIOS
// =============================================================================

void demonstrate_exception_scenarios() {
    std::cout << "\n=== Exception Scenarios ===\n";

    try {
        // Dereferencing nullptr throws exception in debug builds
        int* ptr = nullptr;

        // Safe check before dereferencing
        if (ptr != nullptr) {
            std::cout << "Value: " << *ptr << "\n";
        } else {
            std::cout << "Pointer is null, cannot dereference\n";
        }

        // Bad allocation might return nullptr (nothrow new)
        int* large_array = new(std::nothrow) int[SIZE_MAX];
        if (large_array == nullptr) {
            std::cout << "Allocation failed, got nullptr\n";
        } else {
            delete[] large_array;
        }

    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << "\n";
    }
}

// =============================================================================
// 11. PERFORMANCE IMPLICATIONS
// =============================================================================

void demonstrate_performance_implications() {
    std::cout << "\n=== Performance Implications ===\n";

    // nullptr is a compile-time constant
    constexpr auto compile_time_null = nullptr;

    // All these should have same runtime performance
    int* ptr1 = nullptr;
    int* ptr2 = NULL;
    int* ptr3 = 0;

    // Comparison performance (all should be equivalent)
    volatile bool result1 = (ptr1 == nullptr);
    volatile bool result2 = (ptr2 == NULL);
    volatile bool result3 = (ptr3 == 0);

    std::cout << "All pointers initialized and compared successfully\n";
    std::cout << "Performance should be identical at runtime\n";
}

// =============================================================================
// 12. BEST PRACTICES AND GUIDELINES
// =============================================================================

void demonstrate_best_practices() {
    std::cout << "\n=== Best Practices ===\n";

    // DO: Use nullptr for pointer initialization
    int* good_ptr = nullptr;
    std::unique_ptr<int> good_smart_ptr = nullptr;

    // DON'T: Use NULL or 0 for new code
    // int* avoid_ptr = NULL;
    // int* avoid_ptr2 = 0;

    // DO: Use nullptr in comparisons
    if (good_ptr != nullptr) {
        // Safe to use pointer
    }

    // DO: Use nullptr in function calls expecting pointers
    auto result = std::make_unique<int>(42);
    result.reset(nullptr);

    // DO: Use nullptr in template contexts
    template_function(nullptr);  // Clear intent

    std::cout << "Best practices demonstrated:\n";
    std::cout << "1. Always use nullptr for pointer null values\n";
    std::cout << "2. Prefer nullptr over NULL or 0\n";
    std::cout << "3. Use nullptr in template and overload contexts\n";
    std::cout << "4. nullptr provides type safety and clarity\n";
}

// =============================================================================
// 13. CONVERSION RULES SUMMARY
// =============================================================================

void demonstrate_conversion_rules() {
    std::cout << "\n=== Conversion Rules Summary ===\n";

    // nullptr conversions
    int* ptr_from_nullptr = nullptr;           // OK: nullptr to pointer
    bool bool_from_nullptr = nullptr;          // OK: nullptr to bool (false)
    // int int_from_nullptr = nullptr;         // ERROR: nullptr to int not allowed

    // NULL conversions (NULL is typically 0)
    int* ptr_from_null = NULL;                 // OK: NULL to pointer
    bool bool_from_null = NULL;                // OK: NULL to bool
    int int_from_null = NULL;                  // OK: NULL to int (it's 0)

    // 0 conversions
    int* ptr_from_zero = 0;                    // OK: 0 to pointer
    bool bool_from_zero = 0;                   // OK: 0 to bool
    int int_from_zero = 0;                     // OK: 0 to int

    std::cout << "Conversion summary:\n";
    std::cout << "nullptr: Only converts to pointers and bool\n";
    std::cout << "NULL: Converts to pointers, bool, and integers\n";
    std::cout << "0: Converts to pointers, bool, and integers\n";

    // Demonstrate type safety
    std::cout << "\nType safety comparison:\n";
    std::cout << "nullptr is type-safe (std::nullptr_t)\n";
    std::cout << "NULL/0 can cause ambiguity in overloaded functions\n";
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATE ALL CASES
// =============================================================================

int main() {
    std::cout << "=== nullptr vs NULL vs 0 - Comprehensive Examples ===\n";

    demonstrate_basic_types();
    demonstrate_overloading_issues();
    demonstrate_template_deduction();
    demonstrate_perfect_forwarding();
    demonstrate_comparisons();
    demonstrate_smart_pointers();
    demonstrate_function_pointers();
    demonstrate_member_pointers();
    demonstrate_arrays_and_arithmetic();
    demonstrate_exception_scenarios();
    demonstrate_performance_implications();
    demonstrate_best_practices();
    demonstrate_conversion_rules();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Key Takeaways:\n";
    std::cout << "1. nullptr is type-safe and should be preferred\n";
    std::cout << "2. nullptr eliminates ambiguity in function overloading\n";
    std::cout << "3. nullptr has its own type (std::nullptr_t)\n";
    std::cout << "4. nullptr works better with templates and modern C++\n";
    std::cout << "5. NULL and 0 are legacy - avoid in new code\n";
    std::cout << "6. All three have same runtime performance\n";
    std::cout << "7. nullptr provides better code clarity and intent\n";

    return 0;
}

/*
 * COMPILATION AND EXECUTION NOTES:
 *
 * Compile with: g++ -std=c++17 -Wall -Wextra nullptr_vs_null_vs_zero.cpp
 *
 * Key Differences Summary:
 *
 * nullptr:
 * - Type: std::nullptr_t
 * - Type-safe, only converts to pointers
 * - Resolves function overloading ambiguity
 * - Modern C++11 standard
 * - Recommended for all new code
 *
 * NULL:
 * - Macro (usually 0 or ((void*)0))
 * - Can cause function overloading ambiguity
 * - Legacy from C
 * - Avoid in new C++ code
 *
 * 0:
 * - Integer literal
 * - Can convert to both pointers and integers
 * - Causes function overloading ambiguity
 * - Not clear in intent when used as null pointer
 *
 * Best Practice: Always use nullptr for null pointer values
 */
