// C++20 Feature Examples
#include <iostream>
#include <vector>
#include <string>
#include <concepts>
#include <compare>
#include <span>

// Example 1: Concepts
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
T add(T a, T b) {
    return a + b;
}

void conceptsExample() {
    std::cout << "\n=== Concepts ===" << std::endl;
    
    std::cout << "5 + 3 = " << add(5, 3) << std::endl;
    std::cout << "2.5 + 1.5 = " << add(2.5, 1.5) << std::endl;
    // add("hello", "world");  // Compile error: string is not numeric
}

// Example 2: Three-way comparison operator
struct Point {
    int x, y;
    
    // C++20: default generates all comparison operators
    auto operator<=>(const Point&) const = default;
};

void threeWayComparisonExample() {
    std::cout << "\n=== Three-Way Comparison (<=>) ===" << std::endl;
    
    Point p1{1, 2}, p2{1, 3}, p3{1, 2};
    
    std::cout << "p1 == p2: " << (p1 == p2) << std::endl;
    std::cout << "p1 == p3: " << (p1 == p3) << std::endl;
    std::cout << "p1 < p2: " << (p1 < p2) << std::endl;
    std::cout << "p1 <= p2: " << (p1 <= p2) << std::endl;
}

// Example 3: constinit
constinit int global_value = 42;

void constinitExample() {
    std::cout << "\n=== constinit ===" << std::endl;
    std::cout << "Global value: " << global_value << std::endl;
}

// Example 4: consteval
consteval int square(int n) {
    return n * n;
}

void constevalExample() {
    std::cout << "\n=== consteval ===" << std::endl;
    
    constexpr int x = square(5);  // Evaluated at compile-time
    std::cout << "5^2 = " << x << std::endl;
    
    // int y = 5;
    // int z = square(y);  // Error: y is not a constant expression
}

// Example 5: std::span
void printSpan(std::span<int> data) {
    for (int n : data) {
        std::cout << n << " ";
    }
    std::cout << std::endl;
}

void spanExample() {
    std::cout << "\n=== std::span ===" << std::endl;
    
    int arr[] = {1, 2, 3, 4, 5};
    std::vector<int> vec = {6, 7, 8, 9, 10};
    
    std::cout << "Array: ";
    printSpan(arr);
    
    std::cout << "Vector: ";
    printSpan(vec);
    
    std::cout << "First 3 elements: ";
    printSpan(std::span{arr, 3});
}

// Example 6: Designated initializers
struct Config {
    int width;
    int height;
    std::string title;
    bool fullscreen;
};

void designatedInitializersExample() {
    std::cout << "\n=== Designated Initializers ===" << std::endl;
    
    Config cfg = {
        .width = 1920,
        .height = 1080,
        .title = "My Window",
        .fullscreen = false
    };
    
    std::cout << "Config: " << cfg.width << "x" << cfg.height 
              << " - " << cfg.title << std::endl;
}

// Example 7: constexpr improvements
constexpr int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

void constexprImprovementsExample() {
    std::cout << "\n=== constexpr Improvements ===" << std::endl;
    
    constexpr int fact5 = factorial(5);
    std::cout << "5! = " << fact5 << std::endl;
    
    // C++20 allows more complex operations in constexpr
    constexpr auto compute = [](int x) {
        int result = 1;
        for (int i = 1; i <= x; ++i) {
            result *= i;
        }
        return result;
    };
    
    constexpr int fact6 = compute(6);
    std::cout << "6! = " << fact6 << std::endl;
}

// Example 8: Template syntax improvements
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template<Addable T>
T sum(T a, T b) {
    return a + b;
}

void templateSyntaxExample() {
    std::cout << "\n=== Template Syntax Improvements ===" << std::endl;
    std::cout << "Sum: " << sum(5, 3) << std::endl;
    std::cout << "Sum: " << sum(2.5, 1.5) << std::endl;
}

// Example 9: Lambda improvements
void lambdaImprovementsExample() {
    std::cout << "\n=== Lambda Improvements ===" << std::endl;
    
    // Template lambda
    auto add = []<typename T>(T a, T b) {
        return a + b;
    };
    
    std::cout << "5 + 3 = " << add(5, 3) << std::endl;
    std::cout << "2.5 + 1.5 = " << add(2.5, 1.5) << std::endl;
    
    // Lambda with explicit template
    auto print = []<typename... Args>(Args... args) {
        ((std::cout << args << " "), ...);
        std::cout << std::endl;
    };
    
    print(1, 2.5, "hello");
}

// Example 10: Using enum
enum class Color { RED, GREEN, BLUE };

void usingEnumExample() {
    std::cout << "\n=== Using Enum ===" << std::endl;
    
    using enum Color;
    
    Color c = RED;  // Don't need Color::RED
    
    switch (c) {
        case RED:   std::cout << "Red" << std::endl; break;
        case GREEN: std::cout << "Green" << std::endl; break;
        case BLUE:  std::cout << "Blue" << std::endl; break;
    }
}

// Example 11: Likely and unlikely attributes
int processValue(int x) {
    if (x > 0) [[likely]] {
        return x * 2;
    } else [[unlikely]] {
        return x;
    }
}

void attributesExample() {
    std::cout << "\n=== [[likely]] and [[unlikely]] Attributes ===" << std::endl;
    std::cout << "Process 5: " << processValue(5) << std::endl;
    std::cout << "Process -3: " << processValue(-3) << std::endl;
}

// Example 12: char8_t
void char8tExample() {
    std::cout << "\n=== char8_t ===" << std::endl;
    
    char8_t utf8_char = u8'A';
    const char8_t* utf8_str = u8"Hello, UTF-8!";
    
    std::cout << "char8_t character: " << static_cast<char>(utf8_char) << std::endl;
    std::cout << "char8_t string: " << reinterpret_cast<const char*>(utf8_str) << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     C++20 Feature Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    
    conceptsExample();
    threeWayComparisonExample();
    constinitExample();
    constevalExample();
    spanExample();
    designatedInitializersExample();
    constexprImprovementsExample();
    templateSyntaxExample();
    lambdaImprovementsExample();
    usingEnumExample();
    attributesExample();
    char8tExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     All C++20 examples completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
