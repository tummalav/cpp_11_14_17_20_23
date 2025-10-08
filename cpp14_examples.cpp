// C++14 Feature Examples
#include <iostream>
#include <string>
#include <memory>
#include <vector>

// Example 1: Generic lambdas
void genericLambdaExample() {
    std::cout << "\n=== Generic Lambdas ===" << std::endl;
    
    auto add = [](auto a, auto b) { return a + b; };
    
    std::cout << "5 + 3 = " << add(5, 3) << std::endl;
    std::cout << "2.5 + 1.5 = " << add(2.5, 1.5) << std::endl;
    std::cout << "Hello + World = " 
              << add(std::string("Hello "), std::string("World")) << std::endl;
}

// Example 2: Return type deduction
auto multiply(int a, int b) {
    return a * b;  // Return type deduced as int
}

auto getVector() {
    return std::vector<int>{1, 2, 3, 4, 5};
}

void returnTypeDeductionExample() {
    std::cout << "\n=== Return Type Deduction ===" << std::endl;
    std::cout << "5 * 7 = " << multiply(5, 7) << std::endl;
    
    auto vec = getVector();
    std::cout << "Vector size: " << vec.size() << std::endl;
}

// Example 3: Binary literals and digit separators
void binaryLiteralsExample() {
    std::cout << "\n=== Binary Literals & Digit Separators ===" << std::endl;
    
    int binary = 0b1010;
    int large_binary = 0b1111'0000;
    int million = 1'000'000;
    double pi = 3.141'592'653;
    
    std::cout << "Binary 0b1010 = " << binary << std::endl;
    std::cout << "Binary 0b1111'0000 = " << large_binary << std::endl;
    std::cout << "One million: " << million << std::endl;
    std::cout << "Pi: " << pi << std::endl;
}

// Example 4: make_unique
void makeUniqueExample() {
    std::cout << "\n=== make_unique ===" << std::endl;
    
    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<std::string>("Hello, C++14!");
    auto ptr3 = std::make_unique<std::vector<int>>(5, 10);
    
    std::cout << "ptr1: " << *ptr1 << std::endl;
    std::cout << "ptr2: " << *ptr2 << std::endl;
    std::cout << "ptr3 size: " << ptr3->size() << std::endl;
}

// Example 5: User-defined literals
constexpr long double operator"" _deg(long double deg) {
    return deg * 3.141592653589793238 / 180.0;
}

constexpr long double operator"" _rad(long double rad) {
    return rad;
}

void userDefinedLiteralsExample() {
    std::cout << "\n=== User-Defined Literals ===" << std::endl;
    
    auto angle1 = 90.0_deg;
    auto angle2 = 180.0_deg;
    
    std::cout << "90 degrees in radians: " << angle1 << std::endl;
    std::cout << "180 degrees in radians: " << angle2 << std::endl;
}

// Example 6: Variable templates
template<typename T>
constexpr T pi = T(3.1415926535897932385);

template<typename T>
constexpr T e = T(2.7182818284590452354);

void variableTemplatesExample() {
    std::cout << "\n=== Variable Templates ===" << std::endl;
    
    std::cout << "pi<float>: " << pi<float> << std::endl;
    std::cout << "pi<double>: " << pi<double> << std::endl;
    std::cout << "e<float>: " << e<float> << std::endl;
    std::cout << "e<double>: " << e<double> << std::endl;
}

// Example 7: Deprecated attribute
[[deprecated("Use newFunction() instead")]]
void oldFunction() {
    std::cout << "This is the old function" << std::endl;
}

void newFunction() {
    std::cout << "This is the new function" << std::endl;
}

void deprecatedAttributeExample() {
    std::cout << "\n=== [[deprecated]] Attribute ===" << std::endl;
    // oldFunction();  // Generates compiler warning
    newFunction();
}

// Example 8: Relaxed constexpr
constexpr int factorial(int n) {
    // C++14 allows loops and multiple statements in constexpr
    int result = 1;
    for (int i = 1; i <= n; ++i) {
        result *= i;
    }
    return result;
}

void relaxedConstexprExample() {
    std::cout << "\n=== Relaxed constexpr ===" << std::endl;
    
    constexpr int fact5 = factorial(5);  // Computed at compile time
    std::cout << "5! = " << fact5 << std::endl;
    
    int n = 6;
    int fact6 = factorial(n);  // Can also run at runtime
    std::cout << "6! = " << fact6 << std::endl;
}

// Example 9: Generic lambdas with algorithm
void genericLambdaAlgorithmExample() {
    std::cout << "\n=== Generic Lambdas with Algorithms ===" << std::endl;
    
    std::vector<int> ints = {1, 2, 3, 4, 5};
    std::vector<double> doubles = {1.1, 2.2, 3.3, 4.4, 5.5};
    
    auto print = [](const auto& container) {
        for (const auto& element : container) {
            std::cout << element << " ";
        }
        std::cout << std::endl;
    };
    
    std::cout << "Integers: ";
    print(ints);
    
    std::cout << "Doubles: ";
    print(doubles);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     C++14 Feature Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    
    genericLambdaExample();
    returnTypeDeductionExample();
    binaryLiteralsExample();
    makeUniqueExample();
    userDefinedLiteralsExample();
    variableTemplatesExample();
    deprecatedAttributeExample();
    relaxedConstexprExample();
    genericLambdaAlgorithmExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     All C++14 examples completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
