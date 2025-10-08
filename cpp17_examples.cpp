// C++17 Feature Examples
#include <iostream>
#include <string>
#include <optional>
#include <variant>
#include <any>
#include <string_view>
#include <map>
#include <tuple>
#include <vector>

// Example 1: Structured bindings
void structuredBindingsExample() {
    std::cout << "\n=== Structured Bindings ===" << std::endl;
    
    // With std::pair
    std::pair<int, std::string> person = {25, "John"};
    auto [age, name] = person;
    std::cout << name << " is " << age << " years old" << std::endl;
    
    // With std::tuple
    std::tuple<int, double, std::string> data = {1, 2.5, "hello"};
    auto [i, d, s] = data;
    std::cout << "Tuple: " << i << ", " << d << ", " << s << std::endl;
    
    // With struct
    struct Point { int x; int y; };
    Point p{10, 20};
    auto [px, py] = p;
    std::cout << "Point: x=" << px << ", y=" << py << std::endl;
    
    // With maps
    std::map<std::string, int> myMap = {{"one", 1}, {"two", 2}, {"three", 3}};
    for (const auto& [key, value] : myMap) {
        std::cout << key << ": " << value << std::endl;
    }
}

// Example 2: if with initializer
void ifWithInitializerExample() {
    std::cout << "\n=== if with Initializer ===" << std::endl;
    
    std::map<std::string, int> myMap = {{"key", 42}};
    
    if (auto it = myMap.find("key"); it != myMap.end()) {
        std::cout << "Found: " << it->second << std::endl;
    } else {
        std::cout << "Not found" << std::endl;
    }
    
    if (auto it = myMap.find("nonexistent"); it != myMap.end()) {
        std::cout << "Found: " << it->second << std::endl;
    } else {
        std::cout << "Not found" << std::endl;
    }
}

// Example 3: std::optional
std::optional<int> divide(int a, int b) {
    if (b == 0) return std::nullopt;
    return a / b;
}

void optionalExample() {
    std::cout << "\n=== std::optional ===" << std::endl;
    
    auto result1 = divide(10, 2);
    if (result1) {
        std::cout << "10 / 2 = " << *result1 << std::endl;
    }
    
    auto result2 = divide(10, 0);
    if (result2) {
        std::cout << "Result: " << *result2 << std::endl;
    } else {
        std::cout << "Division by zero!" << std::endl;
    }
    
    // value_or provides default
    std::cout << "10 / 0 with default: " << divide(10, 0).value_or(-1) << std::endl;
}

// Example 4: std::variant
void variantExample() {
    std::cout << "\n=== std::variant ===" << std::endl;
    
    std::variant<int, double, std::string> data;
    
    data = 42;
    std::cout << "Holds int: " << std::get<int>(data) << std::endl;
    
    data = 3.14;
    std::cout << "Holds double: " << std::get<double>(data) << std::endl;
    
    data = "hello";
    std::cout << "Holds string: " << std::get<std::string>(data) << std::endl;
    
    // Visitor pattern
    std::visit([](auto&& arg) {
        std::cout << "Current value: " << arg << std::endl;
    }, data);
}

// Example 5: std::any
void anyExample() {
    std::cout << "\n=== std::any ===" << std::endl;
    
    std::any value;
    
    value = 42;
    std::cout << "Holds int: " << std::any_cast<int>(value) << std::endl;
    
    value = 3.14;
    std::cout << "Holds double: " << std::any_cast<double>(value) << std::endl;
    
    value = std::string("hello");
    std::cout << "Holds string: " << std::any_cast<std::string>(value) << std::endl;
    
    // Check type
    if (value.type() == typeid(std::string)) {
        std::cout << "Currently holds a string" << std::endl;
    }
}

// Example 6: std::string_view
void printStringView(std::string_view sv) {
    std::cout << sv << std::endl;
}

void stringViewExample() {
    std::cout << "\n=== std::string_view ===" << std::endl;
    
    std::string str = "Hello, World!";
    printStringView(str);              // Works with std::string
    printStringView("Hello");          // Works with string literals
    
    std::string_view sv = "Hello, World!";
    std::string_view hello = sv.substr(0, 5);  // No allocation!
    std::cout << "Substring: " << hello << std::endl;
}

// Example 7: constexpr if
template<typename T>
auto getValue(T t) {
    if constexpr (std::is_pointer<T>::value) {
        return *t;  // Dereference pointers
    } else {
        return t;   // Return value as-is
    }
}

void constexprIfExample() {
    std::cout << "\n=== constexpr if ===" << std::endl;
    
    int x = 10;
    int* ptr = &x;
    
    std::cout << "Value: " << getValue(x) << std::endl;
    std::cout << "Pointer dereferenced: " << getValue(ptr) << std::endl;
}

// Example 8: Fold expressions
template<typename... Args>
auto sum(Args... args) {
    return (args + ...);  // Unary right fold
}

template<typename... Args>
void printAll(Args... args) {
    (std::cout << ... << args) << std::endl;
}

void foldExpressionsExample() {
    std::cout << "\n=== Fold Expressions ===" << std::endl;
    
    std::cout << "Sum: " << sum(1, 2, 3, 4, 5) << std::endl;
    
    std::cout << "Print all: ";
    printAll(1, " ", 2.5, " ", "hello", " ", 'c');
}

// Example 9: Class template argument deduction (CTAD)
void ctadExample() {
    std::cout << "\n=== Class Template Argument Deduction ===" << std::endl;
    
    // C++17: template arguments deduced
    std::pair p(42, 3.14);              // std::pair<int, double>
    std::vector v = {1, 2, 3};          // std::vector<int>
    std::tuple t(1, 2.5, "hello");      // std::tuple<int, double, const char*>
    
    std::cout << "Pair: " << p.first << ", " << p.second << std::endl;
    std::cout << "Vector size: " << v.size() << std::endl;
}

// Example 10: inline variables
inline int global_counter = 0;  // Can be in header without ODR violations

void inlineVariablesExample() {
    std::cout << "\n=== inline Variables ===" << std::endl;
    
    global_counter = 42;
    std::cout << "Global counter: " << global_counter << std::endl;
}

// Example 11: Nested namespace definition
namespace A::B::C {
    void func() {
        std::cout << "Inside A::B::C" << std::endl;
    }
}

void nestedNamespaceExample() {
    std::cout << "\n=== Nested Namespace ===" << std::endl;
    A::B::C::func();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     C++17 Feature Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    
    structuredBindingsExample();
    ifWithInitializerExample();
    optionalExample();
    variantExample();
    anyExample();
    stringViewExample();
    constexprIfExample();
    foldExpressionsExample();
    ctadExample();
    inlineVariablesExample();
    nestedNamespaceExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     All C++17 examples completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
