// C++23 Feature Examples
#include <iostream>
#include <string>
#include <vector>
#include <utility>

// Note: Many C++23 features may not be fully supported by all compilers yet
// This file demonstrates the features with simplified examples

// Example 1: if consteval
constexpr int compute(int x) {
    if consteval {
        // Executed at compile-time
        return x * x;
    } else {
        // Executed at runtime
        return x + x;
    }
}

void ifConstevalExample() {
    std::cout << "\n=== if consteval ===" << std::endl;
    
    constexpr int compile_time = compute(5);
    std::cout << "Compile-time result: " << compile_time << std::endl;
    
    int runtime_value = 7;
    int runtime_result = compute(runtime_value);
    std::cout << "Runtime result: " << runtime_result << std::endl;
}

// Example 2: Deducing this (simplified conceptual example)
struct MyClass {
    int value = 42;
    
    // Traditional approach
    int getValue() const {
        return value;
    }
    
    // C++23 would allow: void func(this MyClass& self)
    // This is a forward-looking feature
};

void deducingThisExample() {
    std::cout << "\n=== Deducing this (Concept) ===" << std::endl;
    
    MyClass obj;
    std::cout << "Value: " << obj.getValue() << std::endl;
    std::cout << "Note: Deducing 'this' is a C++23 feature" << std::endl;
}

// Example 3: std::to_underlying for enums
enum class Color { RED, GREEN, BLUE };

void toUnderlyingExample() {
    std::cout << "\n=== std::to_underlying ===" << std::endl;
    
    Color c = Color::RED;
    int value = std::to_underlying(c);
    
    std::cout << "Color::RED as int: " << value << std::endl;
    std::cout << "Color::GREEN as int: " << std::to_underlying(Color::GREEN) << std::endl;
    std::cout << "Color::BLUE as int: " << std::to_underlying(Color::BLUE) << std::endl;
}

// Example 4: size_t literal suffix
void sizeTLiteralExample() {
    std::cout << "\n=== size_t Literal Suffix (uz) ===" << std::endl;
    
    // C++23: uz suffix for size_t
    auto size = 100uz;  // Type is size_t
    std::cout << "Size value: " << size << std::endl;
    
    std::vector<int> vec(100);
    // Prevents warnings with comparisons
    for (size_t i = 0; i < vec.size(); ++i) {
        vec[i] = static_cast<int>(i);
    }
    std::cout << "Vector filled with " << vec.size() << " elements" << std::endl;
}

// Example 5: Enhanced multidimensional subscript operator
template<typename T>
class Matrix {
public:
    Matrix(size_t r, size_t c) : rows(r), cols(c), data(r * c) {}
    
    // C++23 allows multiple subscripts
    // For now, using traditional single parameter approach
    T& operator[](size_t index) {
        return data[index];
    }
    
    T& at(size_t row, size_t col) {
        return data[row * cols + col];
    }
    
    size_t getRows() const { return rows; }
    size_t getCols() const { return cols; }
    
private:
    std::vector<T> data;
    size_t rows, cols;
};

void multidimensionalSubscriptExample() {
    std::cout << "\n=== Multidimensional Subscript (Concept) ===" << std::endl;
    
    Matrix<int> mat(3, 3);
    mat.at(1, 2) = 42;
    
    std::cout << "Matrix[1, 2] = " << mat.at(1, 2) << std::endl;
    std::cout << "Note: C++23 allows mat[1, 2] syntax directly" << std::endl;
}

// Example 6: constexpr improvements
constexpr int stringLength(const char* s) {
    int len = 0;
    while (s[len] != '\0') ++len;
    return len;
}

void constexprStringExample() {
    std::cout << "\n=== constexpr String Improvements ===" << std::endl;
    
    constexpr const char* str = "Hello, World!";
    constexpr int len = stringLength(str);
    std::cout << "Constexpr string: " << str << std::endl;
    std::cout << "Length: " << len << std::endl;
}

// Example 7: Assume attribute
int processWithAssumption(int x) {
    [[assume(x > 0)]];  // Compiler can optimize assuming x is always positive
    return x * 2;
}

void assumeAttributeExample() {
    std::cout << "\n=== [[assume]] Attribute ===" << std::endl;
    
    int value = 5;
    std::cout << "Process value: " << processWithAssumption(value) << std::endl;
    std::cout << "Compiler assumes input is positive for optimization" << std::endl;
}

// Example 8: String contains function
void stringContainsExample() {
    std::cout << "\n=== String contains() ===" << std::endl;
    
    std::string text = "Hello, World!";
    
    // C++23 adds contains() method
    std::cout << "Text: " << text << std::endl;
    std::cout << "Contains 'World': " << (text.find("World") != std::string::npos) << std::endl;
    std::cout << "Note: C++23 adds text.contains(\"World\") method" << std::endl;
}

// Example 9: Ranges improvements
void rangesExample() {
    std::cout << "\n=== Ranges Improvements ===" << std::endl;
    
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    std::cout << "Numbers: ";
    for (int n : numbers) {
        std::cout << n << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Note: C++23 adds more range adaptors and views" << std::endl;
}

// Example 10: Static operator[]
class Container {
public:
    static constexpr int staticData[] = {1, 2, 3, 4, 5};
    
    static constexpr int operator[](size_t index) {
        return staticData[index];
    }
};

void staticSubscriptExample() {
    std::cout << "\n=== Static operator[] ===" << std::endl;
    
    std::cout << "Container[0] = " << Container::operator[](0) << std::endl;
    std::cout << "Container[2] = " << Container::operator[](2) << std::endl;
}

// Example 11: constexpr cmath functions
constexpr double calculateCircleArea(double radius) {
    constexpr double pi = 3.14159265358979323846;
    return pi * radius * radius;
}

void constexprMathExample() {
    std::cout << "\n=== constexpr Math Functions ===" << std::endl;
    
    constexpr double area = calculateCircleArea(5.0);
    std::cout << "Circle area (r=5): " << area << std::endl;
}

// Example 12: Enhanced enums
enum Status { STATUS_OK = 0, STATUS_WARNING = 1, STATUS_ERROR = 2 };

void enhancedEnumsExample() {
    std::cout << "\n=== Enhanced Enumerations ===" << std::endl;
    
    Status s = STATUS_OK;
    std::cout << "Status: " << s << std::endl;
    
    // C++23 improvements make working with enums easier
    switch (s) {
        case STATUS_OK:      std::cout << "Everything is OK" << std::endl; break;
        case STATUS_WARNING: std::cout << "Warning!" << std::endl; break;
        case STATUS_ERROR:   std::cout << "Error!" << std::endl; break;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     C++23 Feature Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nNote: Some C++23 features may not be fully" << std::endl;
    std::cout << "supported by all compilers yet." << std::endl;
    
    ifConstevalExample();
    deducingThisExample();
    toUnderlyingExample();
    sizeTLiteralExample();
    multidimensionalSubscriptExample();
    constexprStringExample();
    assumeAttributeExample();
    stringContainsExample();
    rangesExample();
    staticSubscriptExample();
    constexprMathExample();
    enhancedEnumsExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     All C++23 examples completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
