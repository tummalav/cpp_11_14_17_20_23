// =============================
// DECLTYPE USE CASES AND RULES (C++)
// =============================
/*
DECLTYPE RULES:

1. BASIC RULE: decltype(expr) deduces the exact type of the expression
   - NO reference removal, NO const removal
   - Preserves all type qualifiers and value categories

2. DECLTYPE VS AUTO:
   - auto: removes references and const qualifiers (with exceptions)
   - decltype: preserves everything exactly as written

3. EXPRESSION CATEGORIES:
   - decltype(variable) -> type of variable
   - decltype((variable)) -> reference to variable type
   - decltype(expression) -> depends on value category

4. VALUE CATEGORIES AND DECLTYPE:
   - lvalue expression -> T&
   - xvalue expression -> T&&
   - prvalue expression -> T

5. FUNCTION CALLS:
   - decltype(func()) -> return type of func
   - Works with overloaded functions in context

6. MEMBER ACCESS:
   - decltype(obj.member) -> type of member
   - decltype(ptr->member) -> type of member

7. DECLTYPE(AUTO) (C++14+):
   - Combines decltype deduction with auto convenience
   - Used in return type deduction and variable declarations
*/

// =============================
// COMPREHENSIVE DECLTYPE EXAMPLES
// =============================

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <functional>
#include <memory>

// Helper to print type information
template<typename T>
void print_type() {
    std::cout << "Type: " << typeid(T).name();
    if (std::is_reference<T>::value) {
        std::cout << " (reference)";
    }
    if (std::is_const<std::remove_reference_t<T>>::value) {
        std::cout << " (const)";
    }
    std::cout << std::endl;
}

// Helper macros for type analysis
#define PRINT_DECLTYPE(expr) \
    std::cout << #expr << " -> "; \
    print_type<decltype(expr)>();

// 1. BASIC DECLTYPE DEDUCTION
void basic_decltype_examples() {
    std::cout << "=== BASIC DECLTYPE DEDUCTION ===\n";

    int x = 42;
    const int cx = 10;
    int& rx = x;
    const int& rcx = cx;

    // Basic variable deduction
    PRINT_DECLTYPE(x);     // int
    PRINT_DECLTYPE(cx);    // const int
    PRINT_DECLTYPE(rx);    // int& (reference preserved!)
    PRINT_DECLTYPE(rcx);   // const int& (const and reference preserved!)

    // Expression in parentheses
    PRINT_DECLTYPE((x));   // int& (lvalue expression)
    PRINT_DECLTYPE((cx));  // const int& (const lvalue expression)

    // Arithmetic expressions
    PRINT_DECLTYPE(x + 1); // int (prvalue)
    PRINT_DECLTYPE(x += 1); // int& (lvalue, assignment returns reference)

    std::cout << "\n";
}

// 2. DECLTYPE VS AUTO COMPARISON
void decltype_vs_auto() {
    std::cout << "=== DECLTYPE VS AUTO COMPARISON ===\n";

    const int x = 42;
    int& rx = const_cast<int&>(x);

    // AUTO behavior
    auto a1 = x;        // int (const removed)
    auto a2 = rx;       // int (reference removed)
    auto& a3 = x;       // const int& (reference added, const preserved)

    // DECLTYPE behavior
    decltype(x) d1 = x;     // const int (exact type)
    decltype(rx) d2 = rx;   // int& (exact type)
    decltype((x)) d3 = x;   // const int& (lvalue expression)

    std::cout << "auto examples:\n";
    PRINT_DECLTYPE(a1);  // int
    PRINT_DECLTYPE(a2);  // int
    PRINT_DECLTYPE(a3);  // const int&

    std::cout << "\ndecltype examples:\n";
    PRINT_DECLTYPE(d1);  // const int
    PRINT_DECLTYPE(d2);  // int&
    PRINT_DECLTYPE(d3);  // const int&

    std::cout << "\n";
}

// 3. VALUE CATEGORIES AND DECLTYPE
void value_categories_decltype() {
    std::cout << "=== VALUE CATEGORIES AND DECLTYPE ===\n";

    int x = 10;
    int arr[5] = {1, 2, 3, 4, 5};

    // LVALUE EXPRESSIONS -> T&
    std::cout << "LVALUE EXPRESSIONS (result in T&):\n";
    PRINT_DECLTYPE(x);                    // int variable name -> int
    PRINT_DECLTYPE((x));                  // int& (parenthesized variable -> lvalue)
    PRINT_DECLTYPE(++x);                  // int& (pre-increment returns lvalue)
    PRINT_DECLTYPE(--x);                  // int& (pre-decrement returns lvalue)
    PRINT_DECLTYPE(x = 5);                // int& (assignment returns lvalue)
    PRINT_DECLTYPE(x += 1);               // int& (compound assignment returns lvalue)
    PRINT_DECLTYPE(x -= 1);               // int& (compound assignment returns lvalue)
    PRINT_DECLTYPE(x *= 2);               // int& (compound assignment returns lvalue)
    PRINT_DECLTYPE(arr[0]);               // int& (subscript returns lvalue)
    PRINT_DECLTYPE(*(&x));                // int& (dereference returns lvalue)

    std::string str = "hello";
    PRINT_DECLTYPE(str);                  // std::string variable name -> std::string
    PRINT_DECLTYPE((str));                // std::string& (parenthesized variable -> lvalue)
    PRINT_DECLTYPE(str[0]);               // char& (string subscript returns lvalue)

    std::vector<int> vec = {1, 2, 3};
    PRINT_DECLTYPE(vec[0]);               // int& (vector subscript returns lvalue)
    PRINT_DECLTYPE(vec.front());          // int& (front() returns lvalue reference)
    PRINT_DECLTYPE(vec.back());           // int& (back() returns lvalue reference)

    // Function calls that return references are lvalues
    static int static_var = 42;
    auto get_ref = []() -> int& { return static_var; };
    PRINT_DECLTYPE(get_ref());            // int& (function returning reference)

    // XVALUE EXPRESSIONS -> T&& (expiring values)
    std::cout << "\nXVALUE EXPRESSIONS (result in T&&):\n";
    PRINT_DECLTYPE(std::move(x));         // int&& (std::move creates xvalue)
    PRINT_DECLTYPE(std::move(str));       // std::string&& (std::move creates xvalue)
    PRINT_DECLTYPE(std::move(vec));       // std::vector<int>&& (std::move creates xvalue)
    PRINT_DECLTYPE(static_cast<int&&>(x)); // int&& (cast to rvalue reference)

    // Function calls that return rvalue references are xvalues
    auto get_rvalue_ref = []() -> int&& { return std::move(static_var); };
    PRINT_DECLTYPE(get_rvalue_ref());     // int&& (function returning rvalue reference)

    // Member access on xvalue
    struct Point { int x, y; };
    Point p{1, 2};
    PRINT_DECLTYPE(std::move(p).x);       // int&& (member access on xvalue)

    // PRVALUE EXPRESSIONS -> T (pure rvalues)
    std::cout << "\nPRVALUE EXPRESSIONS (result in T):\n";
    PRINT_DECLTYPE(42);                   // int (literal)
    PRINT_DECLTYPE(3.14);                 // double (literal)
    PRINT_DECLTYPE(true);                 // bool (literal)
    PRINT_DECLTYPE('A');                  // char (literal)
    PRINT_DECLTYPE("hello");              // const char* (string literal)
    PRINT_DECLTYPE(nullptr);              // std::nullptr_t (nullptr literal)

    // Arithmetic operations produce prvalues
    PRINT_DECLTYPE(x + 1);                // int (arithmetic result)
    PRINT_DECLTYPE(x - 5);                // int (arithmetic result)
    PRINT_DECLTYPE(x * 2);                // int (arithmetic result)
    PRINT_DECLTYPE(x / 3);                // int (arithmetic result)
    PRINT_DECLTYPE(x % 2);                // int (arithmetic result)

    // Postfix increment/decrement return prvalues
    PRINT_DECLTYPE(x++);                  // int (post-increment returns prvalue)
    PRINT_DECLTYPE(x--);                  // int (post-decrement returns prvalue)

    // Comparison operations produce prvalues
    PRINT_DECLTYPE(x == 5);               // bool (comparison result)
    PRINT_DECLTYPE(x != 10);              // bool (comparison result)
    PRINT_DECLTYPE(x < 20);               // bool (comparison result)
    PRINT_DECLTYPE(x > 0);                // bool (comparison result)

    // Logical operations produce prvalues
    PRINT_DECLTYPE(x && true);            // bool (logical AND result)
    PRINT_DECLTYPE(x || false);           // bool (logical OR result)
    PRINT_DECLTYPE(!x);                   // bool (logical NOT result)

    // Function calls that return by value are prvalues
    auto get_value = []() -> int { return 42; };
    PRINT_DECLTYPE(get_value());          // int (function returning by value)

    // Temporary objects are prvalues
    PRINT_DECLTYPE(std::string("temp"));  // std::string (temporary object)
    PRINT_DECLTYPE(Point{1, 2});          // Point (temporary object)
    PRINT_DECLTYPE(std::vector<int>{1, 2, 3}); // std::vector<int> (temporary object)

    // Type conversions often produce prvalues
    PRINT_DECLTYPE(static_cast<double>(x)); // double (type conversion)
    PRINT_DECLTYPE(int(3.14));            // int (functional cast)

    // Conditional operator - depends on operands
    std::cout << "\nCONDITIONAL OPERATOR EXAMPLES:\n";
    int y = 20;
    PRINT_DECLTYPE(true ? x : y);         // int& (both operands are lvalues -> lvalue)
    PRINT_DECLTYPE(true ? x : 42);        // int (mixed lvalue/prvalue -> prvalue)
    PRINT_DECLTYPE(true ? std::move(x) : std::move(y)); // int&& (both xvalues -> xvalue)

    std::cout << "\n";
}

// Additional helper function to demonstrate value categories in practice
void value_categories_practical_examples() {
    std::cout << "=== PRACTICAL VALUE CATEGORY EXAMPLES ===\n";

    // Example 1: Perfect forwarding relies on value categories
    auto perfect_forwarder = [](auto&& arg) {
        std::cout << "Received argument of type: ";
        print_type<decltype(arg)>();
        // Forward preserving value category
        return std::forward<decltype(arg)>(arg);
    };

    int x = 42;
    std::cout << "Forwarding lvalue:\n";
    auto result1 = perfect_forwarder(x);         // lvalue -> T&
    std::cout << "Result type: ";
    print_type<decltype(result1)>();

    std::cout << "\nForwarding rvalue:\n";
    auto result2 = perfect_forwarder(42);        // prvalue -> T
    std::cout << "Result type: ";
    print_type<decltype(result2)>();

    std::cout << "\nForwarding xvalue:\n";
    auto result3 = perfect_forwarder(std::move(x)); // xvalue -> T&&
    std::cout << "Result type: ";
    print_type<decltype(result3)>();

    // Example 2: Overload resolution based on value categories
    std::cout << "\n=== OVERLOAD RESOLUTION BY VALUE CATEGORY ===\n";

    auto overloaded_func = [](int& x) {
        std::cout << "Called with lvalue reference\n";
    };
    auto overloaded_func2 = [](const int& x) {
        std::cout << "Called with const lvalue reference\n";
    };
    auto overloaded_func3 = [](int&& x) {
        std::cout << "Called with rvalue reference\n";
    };

    int var = 10;
    const int cvar = 20;

    // These would call different overloads based on value category
    std::cout << "var (lvalue): ";
    // overloaded_func(var);     // Would call lvalue ref version
    std::cout << "Lvalue\n";

    std::cout << "cvar (const lvalue): ";
    // overloaded_func2(cvar);   // Would call const lvalue ref version
    std::cout << "Const lvalue\n";

    std::cout << "42 (prvalue): ";
    // overloaded_func3(42);     // Would call rvalue ref version
    std::cout << "Prvalue\n";

    std::cout << "std::move(var) (xvalue): ";
    // overloaded_func3(std::move(var)); // Would call rvalue ref version
    std::cout << "Xvalue\n";

    std::cout << "\n";
}

int main() {
    basic_decltype_examples();
    decltype_vs_auto();
    value_categories_decltype();
    value_categories_practical_examples();
    function_return_types();
    member_access_decltype();
    template_decltype_examples();
    decltype_auto_examples();
    advanced_decltype_patterns();
    sfinae_decltype_examples();
    practical_decltype_examples();

    std::cout << "=== DECLTYPE SUMMARY ===\n";
    std::cout << "- decltype preserves exact types including references and const\n";
    std::cout << "- decltype(variable) gives the declared type\n";
    std::cout << "- decltype((variable)) gives reference to the variable type\n";
    std::cout << "- decltype(expression) depends on value category:\n";
    std::cout << "  * lvalue -> T&\n";
    std::cout << "  * xvalue -> T&&\n";
    std::cout << "  * prvalue -> T\n";
    std::cout << "- VALUE CATEGORIES EXAMPLES:\n";
    std::cout << "  * lvalue: variables, (expr), ++x, x=5, arr[0], *ptr\n";
    std::cout << "  * xvalue: std::move(x), static_cast<T&&>(x), func() returning T&&\n";
    std::cout << "  * prvalue: literals, x+1, x++, func() returning T, temporaries\n";
    std::cout << "- decltype(auto) combines decltype rules with auto convenience\n";
    std::cout << "- Perfect for template metaprogramming and generic code\n";
    std::cout << "- Essential for perfect forwarding and SFINAE techniques\n";
    std::cout << "- Use when you need exact type preservation\n";

    return 0;
}
