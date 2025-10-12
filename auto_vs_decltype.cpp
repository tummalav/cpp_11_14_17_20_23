#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <type_traits>
#include <memory>
#include <algorithm>
#include <typeinfo>
#include <cxxabi.h>
#include <cmath>  // For std::sqrt

// ================================
// AUTO VS DECLTYPE DEMONSTRATIONS
// ================================

// Helper function to get readable type names
std::string getTypeName(const char* mangled) {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    if (status == 0) {
        std::string result(demangled);
        std::free(demangled);
        return result;
    }
    return mangled;
}

// Macro to print type information
#define PRINT_TYPE(var) \
   std::cout << "Type of " << #var << " is " << getTypeName(typeid(var).name()) << "\n";

// ================================
// BASIC AUTO EXAMPLES
// ================================

void demonstrateBasicAuto() {
    std::cout << "\n=== BASIC AUTO USAGE ===\n";

    // Type deduction from initializers
    auto x = 42;                    // int
    auto y = 3.14;                  // double
    auto z = "Hello";               // const char*
    auto str = std::string("World"); // std::string

    PRINT_TYPE(x);
    PRINT_TYPE(y);
    PRINT_TYPE(z);
    PRINT_TYPE(str);

    // Auto with references and pointers
    int value = 100;
    auto& ref = value;              // int&
    auto* ptr = &value;             // int*
    const auto& cref = value;       // const int&

    PRINT_TYPE(ref);
    PRINT_TYPE(ptr);
    PRINT_TYPE(cref);

    std::cout << "value = " << value << ", ref = " << ref << ", *ptr = " << *ptr << "\n";
}

void demonstrateAutoInContainers() {
    std::cout << "\n=== AUTO WITH CONTAINERS ===\n";

    std::vector<int> vec{1, 2, 3, 4, 5};
    std::map<std::string, int> myMap{{"one", 1}, {"two", 2}, {"three", 3}};

    // Iterator type deduction (very useful!)
    auto it = vec.begin();          // std::vector<int>::iterator
    auto mapIt = myMap.find("two"); // std::map<std::string, int>::iterator

    PRINT_TYPE(it);
    PRINT_TYPE(mapIt);

    // Range-based for loops
    std::cout << "Vector contents: ";
    for (auto element : vec) {      // int (copy)
        std::cout << element << " ";
    }
    std::cout << "\n";

    std::cout << "Map contents: ";
    for (const auto& pair : myMap) { // const std::pair<const std::string, int>&
        std::cout << pair.first << ":" << pair.second << " ";
    }
    std::cout << "\n";

    // Structured bindings (C++17)
    for (const auto& [key, value] : myMap) {
        std::cout << key << " = " << value << "\n";
    }
}

void demonstrateAutoWithFunctions() {
    std::cout << "\n=== AUTO WITH FUNCTIONS ===\n";

    // Lambda type deduction
    auto lambda = [](int x, int y) { return x + y; };
    PRINT_TYPE(lambda);

    // Function pointer
    auto funcPtr = static_cast<int(*)(int, int)>([](int a, int b) { return a * b; });
    PRINT_TYPE(funcPtr);

    // std::function
    auto stdFunc = std::function<int(int, int)>([](int a, int b) { return a - b; });
    PRINT_TYPE(stdFunc);

    std::cout << "lambda(3, 4) = " << lambda(3, 4) << "\n";
    std::cout << "funcPtr(3, 4) = " << funcPtr(3, 4) << "\n";
    std::cout << "stdFunc(3, 4) = " << stdFunc(3, 4) << "\n";
}

// ================================
// BASIC DECLTYPE EXAMPLES
// ================================

void demonstrateBasicDecltype() {
    std::cout << "\n=== BASIC DECLTYPE USAGE ===\n";

    int x = 42;
    double y = 3.14;

    // decltype preserves the exact type
    decltype(x) a = 10;             // int
    decltype(y) b = 2.71;           // double
    decltype(x + y) c = x + y;      // double (result of int + double)

    PRINT_TYPE(a);
    PRINT_TYPE(b);
    PRINT_TYPE(c);

    // decltype with expressions
    int arr[5] = {1, 2, 3, 4, 5};
    decltype(arr[0]) element = arr[2]; // int&
    decltype((x)) ref = x;             // int& (parentheses make it an lvalue expression)

    PRINT_TYPE(element);
    PRINT_TYPE(ref);

    std::cout << "element = " << element << ", ref = " << ref << "\n";
}

void demonstrateDecltypeWithFunctions() {
    std::cout << "\n=== DECLTYPE WITH FUNCTIONS ===\n";

    auto getValue = []() -> int { return 42; };
    auto getReference = [](int& x) -> int& { return x; };

    int value = 100;

    // decltype with function calls
    decltype(getValue()) result1 = getValue();           // int
    decltype(getReference(value)) result2 = getReference(value); // int&

    PRINT_TYPE(result1);
    PRINT_TYPE(result2);

    std::cout << "result1 = " << result1 << ", result2 = " << result2 << "\n";

    // Modifying through reference
    result2 = 200;
    std::cout << "After modification: value = " << value << ", result2 = " << result2 << "\n";
}

// ================================
// AUTO VS DECLTYPE COMPARISONS
// ================================

void demonstrateAutoVsDecltype() {
    std::cout << "\n=== AUTO VS DECLTYPE COMPARISON ===\n";

    int x = 42;
    const int cx = 100;
    int& rx = x;
    const int& crx = cx;

    // AUTO removes const and reference qualifiers
    auto a1 = cx;        // int (const removed)
    auto a2 = rx;        // int (reference removed)
    auto a3 = crx;       // int (const and reference removed)

    // DECLTYPE preserves exact types
    decltype(cx) d1 = cx;   // const int
    decltype(rx) d2 = x;    // int&
    decltype(crx) d3 = cx;  // const int&

    std::cout << "AUTO deductions:\n";
    PRINT_TYPE(a1);
    PRINT_TYPE(a2);
    PRINT_TYPE(a3);

    std::cout << "\nDECLTYPE deductions:\n";
    PRINT_TYPE(d1);
    PRINT_TYPE(d2);
    PRINT_TYPE(d3);

    // AUTO with explicit qualifiers
    const auto& ca1 = cx;   // const int&
    auto& ra1 = x;          // int&

    std::cout << "\nAUTO with explicit qualifiers:\n";
    PRINT_TYPE(ca1);
    PRINT_TYPE(ra1);
}

void demonstrateArrayAndPointerDifferences() {
    std::cout << "\n=== ARRAY AND POINTER DIFFERENCES ===\n";

    int arr[5] = {1, 2, 3, 4, 5};

    // AUTO decays arrays to pointers
    auto a1 = arr;          // int*
    auto& a2 = arr;         // int(&)[5]

    // DECLTYPE preserves array types
    decltype(arr) d1;       // int[5] (note: uninitialized for demo)
    decltype((arr)) d2 = arr; // int(&)[5]

    std::cout << "AUTO with arrays:\n";
    PRINT_TYPE(a1);
    PRINT_TYPE(a2);

    std::cout << "\nDECLTYPE with arrays:\n";
    PRINT_TYPE(d1);
    PRINT_TYPE(d2);

    std::cout << "Array size through reference: " << sizeof(a2) / sizeof(a2[0]) << "\n";
    std::cout << "Pointer size: " << sizeof(a1) << " bytes\n";
}

// ================================
// ADVANCED USE CASES
// ================================

// Template function using decltype for return type deduction
template<typename T, typename U>
auto add(T&& t, U&& u) -> decltype(t + u) {
    return t + u;
}

// C++14 auto return type deduction
template<typename T, typename U>
auto multiply(T&& t, U&& u) {
    return t * u;
}

void demonstrateTemplateUsage() {
    std::cout << "\n=== TEMPLATE USAGE ===\n";

    auto result1 = add(3, 4.5);        // double
    auto result2 = add(std::string("Hello "), std::string("World"));
    auto result3 = multiply(2, 3.14);  // double

    PRINT_TYPE(result1);
    PRINT_TYPE(result2);
    PRINT_TYPE(result3);

    std::cout << "add(3, 4.5) = " << result1 << "\n";
    std::cout << "add(strings) = " << result2 << "\n";
    std::cout << "multiply(2, 3.14) = " << result3 << "\n";
}

// Perfect forwarding example
template<typename Func, typename... Args>
auto callAndTime(Func&& func, Args&&... args) -> decltype(func(std::forward<Args>(args)...)) {
    std::cout << "Calling function...\n";
    return func(std::forward<Args>(args)...);
}

void demonstratePerfectForwarding() {
    std::cout << "\n=== PERFECT FORWARDING ===\n";

    auto lambda = [](const std::string& s, int n) {
        return s + " " + std::to_string(n);
    };

    auto result = callAndTime(lambda, std::string("Number"), 42);
    std::cout << "Result: " << result << "\n";
    PRINT_TYPE(result);
}

// Type traits and SFINAE
template<typename T>
auto processContainerSized(T&& container) -> decltype(container.size(), void()) {
    std::cout << "Processing container with size: " << container.size() << "\n";
}

template<typename T>
auto processSingleValue(T&& value) -> std::enable_if_t<!std::is_same_v<decltype(value.size()), decltype(value.size())>, void> {
    std::cout << "Processing single value: " << value << "\n";
}

// Simplified version to avoid overload ambiguity
void processValue(const std::vector<int>& container) {
    std::cout << "Processing vector with size: " << container.size() << "\n";
}

void processValue(const std::string& container) {
    std::cout << "Processing string with size: " << container.size() << "\n";
}

void processValue(int value) {
    std::cout << "Processing single value: " << value << "\n";
}

void demonstrateSFINAE() {
    std::cout << "\n=== SFINAE WITH DECLTYPE ===\n";

    std::vector<int> vec{1, 2, 3};
    std::string str = "Hello";
    int value = 42;

    processValue(vec);    // Calls vector version
    processValue(str);    // Calls string version
    processValue(value);  // Calls int version
}

// ================================
// REAL-WORLD EXAMPLES
// ================================

class DataProcessor {
private:
    std::map<std::string, std::function<double(double)>> operations;

public:
    DataProcessor() {
        // Using auto for complex lambda types
        auto square = [](double x) { return x * x; };
        auto cube = [](double x) { return x * x * x; };
        auto sqrt_op = [](double x) { return std::sqrt(x); };

        operations["square"] = square;
        operations["cube"] = cube;
        operations["sqrt"] = sqrt_op;
    }

    // Using decltype for return type deduction
    template<typename T>
    auto process(const std::string& op, T&& value) -> decltype(operations[op](value)) {
        auto it = operations.find(op);
        if (it != operations.end()) {
            return it->second(value);
        }
        throw std::runtime_error("Unknown operation: " + op);
    }

    // Auto for iterator types
    auto getOperations() const {
        return operations;
    }
};

void demonstrateRealWorldExample() {
    std::cout << "\n=== REAL-WORLD EXAMPLE ===\n";

    DataProcessor processor;

    // Auto for complex return types
    auto ops = processor.getOperations();

    std::cout << "Available operations:\n";
    for (const auto& [name, func] : ops) {
        std::cout << "  " << name << "\n";
    }

    // Using the processor
    auto result1 = processor.process("square", 5.0);
    auto result2 = processor.process("cube", 3.0);
    auto result3 = processor.process("sqrt", 16.0);

    std::cout << "square(5.0) = " << result1 << "\n";
    std::cout << "cube(3.0) = " << result2 << "\n";
    std::cout << "sqrt(16.0) = " << result3 << "\n";
}

// Generic factory function
template<typename T, typename... Args>
auto makeUnique(Args&&... args) -> decltype(std::make_unique<T>(std::forward<Args>(args)...)) {
    std::cout << "Creating unique_ptr<" << getTypeName(typeid(T).name()) << ">\n";
    return std::make_unique<T>(std::forward<Args>(args)...);
}

void demonstrateFactoryPattern() {
    std::cout << "\n=== FACTORY PATTERN ===\n";

    // Auto for smart pointer types
    auto intPtr = makeUnique<int>(42);
    auto stringPtr = makeUnique<std::string>("Hello Factory");
    auto vectorPtr = makeUnique<std::vector<int>>(5, 100);

    PRINT_TYPE(intPtr);
    PRINT_TYPE(stringPtr);
    PRINT_TYPE(vectorPtr);

    std::cout << "*intPtr = " << *intPtr << "\n";
    std::cout << "*stringPtr = " << *stringPtr << "\n";
    std::cout << "vectorPtr->size() = " << vectorPtr->size() << "\n";
}

// ================================
// BEST PRACTICES AND GUIDELINES
// ================================

void demonstrateBestPractices() {
    std::cout << "\n=== BEST PRACTICES ===\n";

    std::cout << "1. Use AUTO for:\n";
    std::cout << "   - Complex iterator types\n";
    std::cout << "   - Lambda expressions\n";
    std::cout << "   - Template instantiations\n";
    std::cout << "   - When type is obvious from context\n\n";

    // Good auto usage
    std::vector<std::map<std::string, std::vector<int>>> complexContainer;
    auto it = complexContainer.begin(); // Much cleaner than full type

    auto lambda = [](const auto& x) { return x * 2; };

    std::cout << "2. Use DECLTYPE for:\n";
    std::cout << "   - Template return type deduction\n";
    std::cout << "   - Perfect forwarding\n";
    std::cout << "   - SFINAE and type traits\n";
    std::cout << "   - Preserving exact types\n\n";

    std::cout << "3. Avoid AUTO when:\n";
    std::cout << "   - Type is not obvious\n";
    std::cout << "   - Explicit type improves readability\n";
    std::cout << "   - Type conversion is important\n\n";

    // Examples where explicit type might be better
    // auto threshold = 0.1;  // Is this float or double?
    double threshold = 0.1;   // Clear intent

    // auto count = getCount(); // What type does this return?
    size_t count = static_cast<size_t>(complexContainer.size()); // Clear intent
}

void demonstrateCommonPitfalls() {
    std::cout << "\n=== COMMON PITFALLS ===\n";

    std::cout << "1. AUTO with initializer lists:\n";
    // auto list = {1, 2, 3};  // std::initializer_list<int>, not std::vector!
    auto vec = std::vector{1, 2, 3}; // Explicit type needed
    PRINT_TYPE(vec);

    std::cout << "\n2. AUTO with proxy objects:\n";
    std::vector<bool> boolVec{true, false, true};
    auto element = boolVec[0];  // Not bool, but std::vector<bool>::reference!
    bool actualBool = boolVec[0]; // Explicit conversion
    PRINT_TYPE(element);
    PRINT_TYPE(actualBool);

    std::cout << "\n3. DECLTYPE with expressions:\n";
    int x = 42;
    decltype(x) a = x;     // int
    decltype((x)) b = x;   // int& (note the parentheses!)
    PRINT_TYPE(a);
    PRINT_TYPE(b);

    std::cout << "\n4. AUTO doesn't preserve references by default:\n";
    int& ref = x;
    auto copy = ref;       // int (copy, not reference)
    auto& actualRef = ref; // int& (explicit reference)
    PRINT_TYPE(copy);
    PRINT_TYPE(actualRef);
}

// ================================
// MAIN FUNCTION
// ================================

int main() {
    std::cout << "C++ AUTO VS DECLTYPE COMPREHENSIVE GUIDE\n";
    std::cout << "=======================================\n";

    demonstrateBasicAuto();
    demonstrateAutoInContainers();
    demonstrateAutoWithFunctions();

    demonstrateBasicDecltype();
    demonstrateDecltypeWithFunctions();

    demonstrateAutoVsDecltype();
    demonstrateArrayAndPointerDifferences();

    demonstrateTemplateUsage();
    demonstratePerfectForwarding();
    demonstrateSFINAE();

    demonstrateRealWorldExample();
    demonstrateFactoryPattern();

    demonstrateBestPractices();
    demonstrateCommonPitfalls();

    std::cout << "\n=== END OF DEMONSTRATION ===\n";
}

/*
COMPREHENSIVE SUMMARY:

AUTO:
======
- Deduces type from initializer
- Removes const, volatile, and reference qualifiers
- Decays arrays to pointers
- Cannot be used for uninitialized variables
- Perfect for: iterators, lambdas, complex template types
- Makes code more maintainable and less verbose

DECLTYPE:
=========
- Deduces type from expression without evaluating it
- Preserves const, volatile, and reference qualifiers
- Preserves exact type including arrays
- Can be used with expressions and function calls
- Perfect for: template metaprogramming, return types, SFINAE

KEY DIFFERENCES:
===============
1. Type preservation: decltype preserves, auto strips qualifiers
2. Usage context: auto needs initializer, decltype can work with expressions
3. Array handling: auto decays to pointer, decltype preserves array type
4. Template usage: decltype for return types, auto for type deduction

BEST PRACTICES:
==============
1. Use auto for obvious cases and complex types
2. Use decltype for template programming and type preservation
3. Be explicit when type clarity is important
4. Watch out for proxy objects and initializer lists
5. Use auto& or const auto& when you need references

MODERN C++ EVOLUTION:
===================
- C++11: Introduction of auto and decltype
- C++14: Auto return type deduction for functions
- C++17: Structured bindings with auto
- C++20: Concepts to constrain auto
- C++23: Enhanced deduction guides and type inference
*/
