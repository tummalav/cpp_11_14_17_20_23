# Modern C++ Features (C++11/14/17/20/23)

A comprehensive guide to modern C++ features introduced in C++11, C++14, C++17, C++20, and C++23, with in-depth explanations and practical examples.

## Table of Contents

- [C++11 Features](#c11-features)
- [C++14 Features](#c14-features)
- [C++17 Features](#c17-features)
- [C++20 Features](#c20-features)
- [C++23 Features](#c23-features)
- [Building Examples](#building-examples)

---

## C++11 Features

C++11 was a major update that modernized C++ with many new features.

### 1. Auto Type Deduction

The `auto` keyword allows the compiler to automatically deduce the type of a variable from its initializer.

```cpp
auto x = 5;              // int
auto y = 3.14;           // double
auto z = "hello";        // const char*
auto vec = std::vector<int>{1, 2, 3};  // std::vector<int>
```

**Benefits:**
- Reduces verbosity for complex types
- Makes code more maintainable
- Helps avoid type mismatches

### 2. Range-Based For Loops

Simplifies iteration over containers and arrays.

```cpp
std::vector<int> numbers = {1, 2, 3, 4, 5};

// Old way
for (std::vector<int>::iterator it = numbers.begin(); it != numbers.end(); ++it) {
    std::cout << *it << " ";
}

// C++11 way
for (auto num : numbers) {
    std::cout << num << " ";
}

// With reference (to modify elements)
for (auto& num : numbers) {
    num *= 2;
}
```

### 3. Lambda Expressions

Anonymous functions that can capture variables from their surrounding scope.

```cpp
// Basic lambda
auto add = [](int a, int b) { return a + b; };
std::cout << add(3, 4) << std::endl;  // Output: 7

// Lambda with capture
int multiplier = 3;
auto multiply = [multiplier](int x) { return x * multiplier; };
std::cout << multiply(5) << std::endl;  // Output: 15

// Capture by reference
int counter = 0;
auto increment = [&counter]() { counter++; };
increment();
std::cout << counter << std::endl;  // Output: 1

// Generic lambda (C++14 feature, but related)
auto generic = [](auto a, auto b) { return a + b; };
```

### 4. Smart Pointers

Automatic memory management to prevent memory leaks.

```cpp
#include <memory>

// unique_ptr - exclusive ownership
std::unique_ptr<int> ptr1 = std::make_unique<int>(42);
// std::unique_ptr<int> ptr2 = ptr1;  // Error: cannot copy
std::unique_ptr<int> ptr2 = std::move(ptr1);  // OK: transfer ownership

// shared_ptr - shared ownership
std::shared_ptr<int> sptr1 = std::make_shared<int>(100);
std::shared_ptr<int> sptr2 = sptr1;  // OK: reference count = 2
std::cout << sptr1.use_count() << std::endl;  // Output: 2

// weak_ptr - non-owning reference
std::weak_ptr<int> wptr = sptr1;
if (auto locked = wptr.lock()) {  // Check if still valid
    std::cout << *locked << std::endl;
}
```

### 5. Move Semantics and Rvalue References

Enables efficient transfer of resources without copying.

```cpp
class MyString {
    char* data;
    size_t length;
public:
    // Constructor
    MyString(const char* str) {
        length = strlen(str);
        data = new char[length + 1];
        strcpy(data, str);
    }
    
    // Copy constructor
    MyString(const MyString& other) {
        length = other.length;
        data = new char[length + 1];
        strcpy(data, other.data);
    }
    
    // Move constructor (C++11)
    MyString(MyString&& other) noexcept {
        data = other.data;
        length = other.length;
        other.data = nullptr;
        other.length = 0;
    }
    
    ~MyString() {
        delete[] data;
    }
};

MyString str1("hello");
MyString str2(std::move(str1));  // Move, not copy
```

### 6. nullptr

A type-safe null pointer constant.

```cpp
// Old way
int* ptr1 = NULL;   // NULL is actually 0
void* ptr2 = 0;

// C++11 way
int* ptr3 = nullptr;
void* ptr4 = nullptr;

// Benefits: prevents ambiguity
void func(int x) { std::cout << "int" << std::endl; }
void func(char* ptr) { std::cout << "pointer" << std::endl; }

// func(NULL);     // Ambiguous! Calls func(int)
func(nullptr);     // Clear: calls func(char*)
```

### 7. Strongly Typed Enums (enum class)

Enums that don't implicitly convert to integers.

```cpp
// Old style enum
enum Color { RED, GREEN, BLUE };
int x = RED;  // OK, but not type-safe

// C++11 enum class
enum class NewColor { RED, GREEN, BLUE };
// int y = NewColor::RED;  // Error: no implicit conversion
int y = static_cast<int>(NewColor::RED);  // Explicit conversion needed

// Can specify underlying type
enum class Status : uint8_t { OK, ERROR, PENDING };
```

### 8. Static Assertions

Compile-time assertions.

```cpp
static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");

template<typename T>
class MyClass {
    static_assert(std::is_pod<T>::value, "T must be a POD type");
};
```

### 9. Variadic Templates

Templates that accept variable number of arguments.

```cpp
// Base case
void print() {
    std::cout << std::endl;
}

// Recursive case
template<typename T, typename... Args>
void print(T first, Args... args) {
    std::cout << first << " ";
    print(args...);
}

print(1, 2.5, "hello", 'c');  // Output: 1 2.5 hello c
```

### 10. Initializer Lists

Uniform initialization syntax.

```cpp
// Traditional
std::vector<int> v1;
v1.push_back(1);
v1.push_back(2);

// C++11 initializer list
std::vector<int> v2 = {1, 2, 3, 4, 5};
std::vector<int> v3{1, 2, 3, 4, 5};

// Works with custom classes
struct Point {
    int x, y;
};
Point p{10, 20};

// Prevents narrowing conversions
int x = 7.9;      // OK (but loses precision)
// int y{7.9};    // Error: narrowing conversion
```

### 11. Delegating Constructors

Constructors can call other constructors.

```cpp
class Rectangle {
    int width, height;
public:
    Rectangle() : Rectangle(0, 0) {}  // Delegates to below
    Rectangle(int w, int h) : width(w), height(h) {}
};
```

### 12. Threading Support

Built-in threading library.

```cpp
#include <thread>
#include <mutex>

std::mutex mtx;
int shared_counter = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);
    shared_counter++;
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    std::cout << shared_counter << std::endl;  // Output: 2
}
```

---

## C++14 Features

C++14 provided incremental improvements to C++11.

### 1. Generic Lambdas

Lambdas with `auto` parameters.

```cpp
auto add = [](auto a, auto b) { return a + b; };

std::cout << add(5, 3) << std::endl;        // int + int = 8
std::cout << add(2.5, 1.5) << std::endl;    // double + double = 4.0
std::cout << add(std::string("Hello"), std::string(" World")) << std::endl;  // string concatenation
```

### 2. Return Type Deduction

Functions can deduce their return type.

```cpp
// C++11: must specify trailing return type
auto func1() -> int { return 42; }

// C++14: return type deduced
auto func2() { return 42; }  // Returns int

auto func3(bool flag) {
    if (flag) return 1;      // int
    else return 2;           // int
    // else return 2.5;      // Error: inconsistent types
}
```

### 3. Binary Literals

Binary number literals with `0b` prefix.

```cpp
int binary = 0b1010;           // 10 in decimal
int large = 0b1111'0000;       // With digit separators (C++14)
std::cout << binary << std::endl;  // Output: 10
```

### 4. Digit Separators

Use single quotes to separate digits for readability.

```cpp
int million = 1'000'000;
double pi = 3.141'592'653'589;
int hex = 0xFF'FF'FF'FF;
int binary = 0b1010'1010'1010;
```

### 5. make_unique

Factory function for creating unique_ptr (was missing in C++11).

```cpp
auto ptr1 = std::make_unique<int>(42);
auto ptr2 = std::make_unique<std::vector<int>>(10, 0);  // Vector of 10 zeros
```

### 6. User-Defined Literals

Create custom literal suffixes.

```cpp
// Define custom literal
constexpr long double operator"" _deg(long double deg) {
    return deg * 3.141592 / 180;
}

auto angle = 90.0_deg;  // Converts degrees to radians

// Standard library example
using namespace std::chrono_literals;
auto duration = 5s;      // 5 seconds
auto wait = 100ms;       // 100 milliseconds
```

### 7. Variable Templates

Templates for variables.

```cpp
template<typename T>
constexpr T pi = T(3.1415926535897932385);

std::cout << pi<float> << std::endl;   // 3.14159f
std::cout << pi<double> << std::endl;  // 3.14159265358979
```

### 8. [[deprecated]] Attribute

Mark entities as deprecated.

```cpp
[[deprecated("Use newFunction() instead")]]
void oldFunction() {
    // ...
}

// Warning when used:
// oldFunction();  // Warning: 'oldFunction' is deprecated: Use newFunction() instead
```

---

## C++17 Features

C++17 added many quality-of-life improvements.

### 1. Structured Bindings

Decompose tuples, pairs, and structs.

```cpp
// With std::pair
std::pair<int, std::string> person = {25, "John"};
auto [age, name] = person;
std::cout << name << " is " << age << " years old" << std::endl;

// With std::tuple
std::tuple<int, double, std::string> data = {1, 2.5, "hello"};
auto [i, d, s] = data;

// With structs
struct Point { int x; int y; };
Point p{10, 20};
auto [px, py] = p;
std::cout << "x=" << px << ", y=" << py << std::endl;

// With maps
std::map<std::string, int> myMap = {{"one", 1}, {"two", 2}};
for (const auto& [key, value] : myMap) {
    std::cout << key << ": " << value << std::endl;
}
```

### 2. if and switch with Initializers

Initialize variables within if/switch statements.

```cpp
// if with initializer
if (auto result = calculate(); result > 0) {
    std::cout << "Positive: " << result << std::endl;
} else {
    std::cout << "Non-positive: " << result << std::endl;
}
// 'result' is out of scope here

// With maps
std::map<std::string, int> myMap = {{"key", 42}};
if (auto it = myMap.find("key"); it != myMap.end()) {
    std::cout << "Found: " << it->second << std::endl;
}
```

### 3. std::optional

Represents a value that may or may not be present.

```cpp
#include <optional>

std::optional<int> divide(int a, int b) {
    if (b == 0) return std::nullopt;
    return a / b;
}

auto result = divide(10, 2);
if (result) {
    std::cout << "Result: " << *result << std::endl;  // Output: Result: 5
} else {
    std::cout << "Division by zero!" << std::endl;
}

// value_or provides default
std::cout << divide(10, 0).value_or(-1) << std::endl;  // Output: -1
```

### 4. std::variant

Type-safe union.

```cpp
#include <variant>

std::variant<int, double, std::string> data;

data = 42;                      // Holds int
data = 3.14;                    // Now holds double
data = "hello";                 // Now holds string

// Access using get
std::cout << std::get<std::string>(data) << std::endl;

// Safe access using get_if
if (auto ptr = std::get_if<std::string>(&data)) {
    std::cout << *ptr << std::endl;
}

// Visitor pattern
std::visit([](auto&& arg) {
    std::cout << arg << std::endl;
}, data);
```

### 5. std::any

Type-safe container for single values of any type.

```cpp
#include <any>

std::any value;

value = 42;
value = 3.14;
value = std::string("hello");

// Retrieve value
try {
    std::string s = std::any_cast<std::string>(value);
    std::cout << s << std::endl;
} catch (const std::bad_any_cast& e) {
    std::cout << "Bad cast!" << std::endl;
}

// Check type
if (value.type() == typeid(std::string)) {
    std::cout << "Holds a string" << std::endl;
}
```

### 6. std::string_view

Non-owning reference to a string.

```cpp
#include <string_view>

void print(std::string_view sv) {
    std::cout << sv << std::endl;
}

std::string str = "Hello, World!";
print(str);              // Works with std::string
print("Hello");          // Works with string literals
print(str.substr(0, 5)); // Works with substrings (no copy!)

// Benefits: no allocation, efficient substring operations
std::string_view sv = "Hello, World!";
std::string_view hello = sv.substr(0, 5);  // No allocation!
```

### 7. Filesystem Library

Standard library for filesystem operations.

```cpp
#include <filesystem>
namespace fs = std::filesystem;

// Check if file exists
if (fs::exists("file.txt")) {
    std::cout << "File exists" << std::endl;
}

// Get file size
std::cout << "Size: " << fs::file_size("file.txt") << " bytes" << std::endl;

// Iterate directory
for (const auto& entry : fs::directory_iterator(".")) {
    std::cout << entry.path() << std::endl;
}

// Create directory
fs::create_directory("new_folder");

// Copy file
fs::copy("source.txt", "destination.txt");
```

### 8. constexpr if

Compile-time if statements.

```cpp
template<typename T>
auto getValue(T t) {
    if constexpr (std::is_pointer_v<T>) {
        return *t;  // Dereference pointers
    } else {
        return t;   // Return value as-is
    }
}

int x = 10;
int* ptr = &x;
std::cout << getValue(x) << std::endl;    // Output: 10
std::cout << getValue(ptr) << std::endl;  // Output: 10
```

### 9. Fold Expressions

Simplify variadic template operations.

```cpp
// Sum all arguments
template<typename... Args>
auto sum(Args... args) {
    return (args + ...);  // Unary right fold
}

std::cout << sum(1, 2, 3, 4, 5) << std::endl;  // Output: 15

// Print all arguments
template<typename... Args>
void print(Args... args) {
    (std::cout << ... << args) << std::endl;
}

print(1, " ", 2.5, " ", "hello");  // Output: 1 2.5 hello
```

### 10. Class Template Argument Deduction (CTAD)

Automatically deduce template arguments.

```cpp
// C++14 and earlier
std::pair<int, double> p1(42, 3.14);
std::vector<int> v1 = {1, 2, 3};

// C++17: template arguments deduced
std::pair p2(42, 3.14);              // std::pair<int, double>
std::vector v2 = {1, 2, 3};          // std::vector<int>
std::tuple t(1, 2.5, "hello");       // std::tuple<int, double, const char*>
```

### 11. inline Variables

Variables can be marked inline.

```cpp
// header.h
inline int global_counter = 0;  // Can be in header without ODR violations

class MyClass {
    inline static int class_counter = 0;  // C++17: can initialize here
};
```

---

## C++20 Features

C++20 is another major update with groundbreaking features.

### 1. Concepts

Constraints on template parameters.

```cpp
#include <concepts>

// Define a concept
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

// Use concept as constraint
template<Numeric T>
T add(T a, T b) {
    return a + b;
}

add(5, 3);        // OK: int is numeric
add(2.5, 1.5);    // OK: double is numeric
// add("hello", "world");  // Error: string is not numeric

// Concept with requires clause
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template<Addable T>
T sum(T a, T b) {
    return a + b;
}
```

### 2. Ranges

Composable algorithms and views.

```cpp
#include <ranges>
#include <vector>
#include <iostream>

std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// Filter even numbers, square them, take first 3
auto result = numbers 
    | std::views::filter([](int n) { return n % 2 == 0; })
    | std::views::transform([](int n) { return n * n; })
    | std::views::take(3);

for (int n : result) {
    std::cout << n << " ";  // Output: 4 16 36
}
```

### 3. Coroutines

Functions that can suspend and resume execution.

```cpp
#include <coroutine>
#include <iostream>

// Simple generator
struct Generator {
    struct promise_type {
        int current_value;
        
        auto get_return_object() { return Generator{this}; }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
        
        auto yield_value(int value) {
            current_value = value;
            return std::suspend_always{};
        }
    };
    
    std::coroutine_handle<promise_type> handle;
    
    Generator(promise_type* p) : handle(std::coroutine_handle<promise_type>::from_promise(*p)) {}
    ~Generator() { if (handle) handle.destroy(); }
    
    bool next() {
        handle.resume();
        return !handle.done();
    }
    
    int value() { return handle.promise().current_value; }
};

Generator counter(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

// Usage
auto gen = counter(1, 5);
while (gen.next()) {
    std::cout << gen.value() << " ";  // Output: 1 2 3 4
}
```

### 4. Three-Way Comparison Operator (<=>)

The spaceship operator for simplified comparisons.

```cpp
#include <compare>

struct Point {
    int x, y;
    
    // C++20: default generates all comparison operators
    auto operator<=>(const Point&) const = default;
};

Point p1{1, 2}, p2{1, 3};

// All comparison operators available
bool eq = (p1 == p2);   // false
bool ne = (p1 != p2);   // true
bool lt = (p1 < p2);    // true
bool le = (p1 <= p2);   // true
bool gt = (p1 > p2);    // false
bool ge = (p1 >= p2);   // false
```

### 5. Modules

Modern alternative to header files.

```cpp
// math.ixx (module interface)
export module math;

export int add(int a, int b) {
    return a + b;
}

export int subtract(int a, int b) {
    return a - b;
}

// main.cpp
import math;
import std;  // Standard library as module

int main() {
    std::cout << add(5, 3) << std::endl;  // Output: 8
}
```

### 6. constinit

Ensures constant initialization.

```cpp
constinit int global = 42;  // Initialized at compile-time
// constinit int bad = get_value();  // Error if get_value() is not constexpr

// Prevents static initialization order fiasco
constinit std::atomic<int> counter{0};
```

### 7. consteval

Forces compile-time evaluation.

```cpp
consteval int square(int n) {
    return n * n;
}

int x = square(5);        // OK: evaluated at compile-time
// int y = 5;
// int z = square(y);     // Error: y is not a constant expression

constexpr int cube(int n) {  // Can run at compile or runtime
    return n * n * n;
}
```

### 8. std::span

Non-owning view over contiguous sequence.

```cpp
#include <span>

void print(std::span<int> data) {
    for (int n : data) {
        std::cout << n << " ";
    }
}

int arr[] = {1, 2, 3, 4, 5};
std::vector<int> vec = {6, 7, 8, 9, 10};

print(arr);              // Works with C arrays
print(vec);              // Works with std::vector
print(std::span{arr, 3});  // First 3 elements
```

### 9. std::format

Type-safe string formatting (like Python's format).

```cpp
#include <format>

std::string name = "Alice";
int age = 30;

std::string msg = std::format("Hello, {}! You are {} years old.", name, age);
// Output: "Hello, Alice! You are 30 years old."

// With positional arguments
std::string msg2 = std::format("{1} is {0} years old", age, name);

// With formatting specifications
std::string formatted = std::format("{:.2f}", 3.14159);  // "3.14"
```

### 10. Designated Initializers

Initialize struct members by name.

```cpp
struct Point {
    int x;
    int y;
    int z;
};

// C++20: designated initializers
Point p = {.x = 10, .y = 20, .z = 30};
Point p2 = {.x = 5, .z = 15};  // y is default-initialized to 0
```

### 11. std::source_location

Capture source location information.

```cpp
#include <source_location>

void log(std::string_view message, 
         const std::source_location& loc = std::source_location::current()) {
    std::cout << "File: " << loc.file_name() << "\n"
              << "Line: " << loc.line() << "\n"
              << "Function: " << loc.function_name() << "\n"
              << "Message: " << message << std::endl;
}

log("Something happened");  // Automatically captures location
```

---

## C++23 Features

C++23 continues to improve the language with modern features.

### 1. std::expected

Represents a value or an error.

```cpp
#include <expected>

std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return std::unexpected("Division by zero");
    }
    return a / b;
}

auto result = divide(10, 2);
if (result) {
    std::cout << "Result: " << *result << std::endl;
} else {
    std::cout << "Error: " << result.error() << std::endl;
}
```

### 2. std::print and std::println

Simpler output functions.

```cpp
#include <print>

std::print("Hello, {}!\n", "World");
std::println("The answer is {}", 42);
std::println("Multiple values: {}, {}, {}", 1, 2, 3);

// Compared to std::format + std::cout
std::cout << std::format("Hello, {}!\n", "World");
```

### 3. if consteval

Detect compile-time vs runtime context.

```cpp
constexpr int compute(int x) {
    if consteval {
        // Executed at compile-time
        return x * x;
    } else {
        // Executed at runtime
        return x + x;
    }
}
```

### 4. Multidimensional Subscript Operator

Support for multiple subscripts.

```cpp
template<typename T>
class Matrix {
public:
    // C++23: multiple subscripts
    T& operator[](size_t row, size_t col) {
        return data[row * cols + col];
    }
private:
    std::vector<T> data;
    size_t rows, cols;
};

Matrix<int> mat(3, 3);
mat[1, 2] = 42;  // C++23 syntax
```

### 5. Deducing this

Simplifies CRTP and reduces code duplication.

```cpp
struct MyClass {
    // C++23: explicit object parameter
    void func(this MyClass& self) {
        // 'self' is the object
    }
    
    // Works with templates
    template<typename Self>
    auto&& get_value(this Self&& self) {
        return std::forward<Self>(self).value;
    }
};
```

### 6. std::flat_map and std::flat_set

Contiguous memory alternatives to std::map/set.

```cpp
#include <flat_map>

std::flat_map<int, std::string> myMap;
myMap[1] = "one";
myMap[2] = "two";

// Better cache locality than std::map
// Faster iteration, slower insertion/deletion
```

### 7. std::mdspan

Multi-dimensional array view.

```cpp
#include <mdspan>

int data[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

// Create 3x4 view
std::mdspan mat(data, 3, 4);

std::cout << mat[1, 2] << std::endl;  // Access element at row 1, col 2

// Different layouts possible
std::mdspan<int, std::extents<int, 3, 4>> mat2(data);
```

### 8. std::stacktrace

Capture and display stack traces.

```cpp
#include <stacktrace>

void print_trace() {
    auto trace = std::stacktrace::current();
    std::cout << trace << std::endl;
}

void func_c() { print_trace(); }
void func_b() { func_c(); }
void func_a() { func_b(); }

// Shows full call stack
```

### 9. Literal Suffix for size_t

`uz` suffix for size_t literals.

```cpp
auto size = 100uz;  // Type is size_t
std::vector<int> vec(100uz);

// Prevents warnings with comparisons
for (size_t i = 0; i < vec.size(); ++i) {  // No warning
    // ...
}
```

### 10. Enhanced Enumerations

More features for enums.

```cpp
#include <utility>

enum class Color { RED, GREEN, BLUE };

// C++23: to_underlying
int value = std::to_underlying(Color::RED);

// Works with any enum type
enum OldEnum { A, B, C };
int old_value = std::to_underlying(A);
```

---

## Building Examples

To build and run the example code:

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- CMake 3.20 or higher

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/tummalav/cpp_11_14_17_20_23.git
cd cpp_11_14_17_20_23

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run examples
./cpp11_examples
./cpp14_examples
./cpp17_examples
./cpp20_examples
./cpp23_examples
```

### Compiler Flags

Make sure to use the appropriate C++ standard flag:

```bash
# GCC/Clang
g++ -std=c++11 cpp11_example.cpp
g++ -std=c++14 cpp14_example.cpp
g++ -std=c++17 cpp17_example.cpp
g++ -std=c++20 cpp20_example.cpp
g++ -std=c++23 cpp23_example.cpp

# MSVC
cl /std:c++11 cpp11_example.cpp
cl /std:c++14 cpp14_example.cpp
cl /std:c++17 cpp17_example.cpp
cl /std:c++20 cpp20_example.cpp
cl /std:c++latest cpp23_example.cpp
```

---

## Contributing

Contributions are welcome! If you have improvements or additional examples, please:

1. Fork the repository
2. Create a feature branch
3. Add your changes with clear examples
4. Submit a pull request

## License

This project is provided for educational purposes. Feel free to use and modify the examples.

## Resources

- [cppreference.com](https://en.cppreference.com/) - Comprehensive C++ reference
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/) - Best practices
- [Compiler Support](https://en.cppreference.com/w/cpp/compiler_support) - Feature support by compiler

---

**Note:** Some C++23 features may not be fully supported by all compilers yet. Check your compiler's documentation for the latest support status.