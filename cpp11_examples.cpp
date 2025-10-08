// C++11 Feature Examples
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>

// Example 1: Auto type deduction
void autoExample() {
    std::cout << "\n=== Auto Type Deduction ===" << std::endl;
    auto x = 5;
    auto y = 3.14;
    auto z = "hello";
    std::cout << "x (int): " << x << std::endl;
    std::cout << "y (double): " << y << std::endl;
    std::cout << "z (const char*): " << z << std::endl;
}

// Example 2: Range-based for loops
void rangeForExample() {
    std::cout << "\n=== Range-Based For Loops ===" << std::endl;
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    
    std::cout << "Original: ";
    for (auto num : numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
    
    // Modify elements
    for (auto& num : numbers) {
        num *= 2;
    }
    
    std::cout << "Doubled: ";
    for (const auto& num : numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
}

// Example 3: Lambda expressions
void lambdaExample() {
    std::cout << "\n=== Lambda Expressions ===" << std::endl;
    
    // Basic lambda
    auto add = [](int a, int b) { return a + b; };
    std::cout << "5 + 3 = " << add(5, 3) << std::endl;
    
    // Lambda with capture
    int multiplier = 3;
    auto multiply = [multiplier](int x) { return x * multiplier; };
    std::cout << "5 * 3 = " << multiply(5) << std::endl;
    
    // Using lambda with algorithms
    std::vector<int> nums = {1, 2, 3, 4, 5};
    std::for_each(nums.begin(), nums.end(), [](int n) {
        std::cout << n * n << " ";
    });
    std::cout << std::endl;
}

// Example 4: Smart pointers
void smartPointerExample() {
    std::cout << "\n=== Smart Pointers ===" << std::endl;
    
    // unique_ptr
    std::unique_ptr<int> ptr1 = std::unique_ptr<int>(new int(42));
    std::cout << "unique_ptr value: " << *ptr1 << std::endl;
    
    // shared_ptr
    std::shared_ptr<int> sptr1 = std::make_shared<int>(100);
    std::shared_ptr<int> sptr2 = sptr1;
    std::cout << "shared_ptr value: " << *sptr1 << std::endl;
    std::cout << "Reference count: " << sptr1.use_count() << std::endl;
}

// Example 5: nullptr
void nullptrExample() {
    std::cout << "\n=== nullptr ===" << std::endl;
    int* ptr = nullptr;
    
    if (ptr == nullptr) {
        std::cout << "Pointer is null" << std::endl;
    }
    
    ptr = new int(42);
    std::cout << "Pointer value: " << *ptr << std::endl;
    delete ptr;
}

// Example 6: Enum class
enum class Color { RED, GREEN, BLUE };
enum class Status : uint8_t { OK, ERROR, PENDING };

void enumClassExample() {
    std::cout << "\n=== Strongly Typed Enums ===" << std::endl;
    Color c = Color::RED;
    Status s = Status::OK;
    
    // Must use scope
    std::cout << "Color: " << static_cast<int>(c) << std::endl;
    std::cout << "Status: " << static_cast<int>(s) << std::endl;
}

// Example 7: Static assertions
template<typename T>
class NumericContainer {
    static_assert(std::is_arithmetic<T>::value, "T must be a numeric type");
    T value;
public:
    NumericContainer(T v) : value(v) {}
    T get() const { return value; }
};

void staticAssertExample() {
    std::cout << "\n=== Static Assertions ===" << std::endl;
    NumericContainer<int> container(42);
    std::cout << "Container value: " << container.get() << std::endl;
    // NumericContainer<std::string> bad("fail"); // Compile error
}

// Example 8: Variadic templates
void print() {
    std::cout << std::endl;
}

template<typename T, typename... Args>
void print(T first, Args... args) {
    std::cout << first << " ";
    print(args...);
}

void variadicTemplateExample() {
    std::cout << "\n=== Variadic Templates ===" << std::endl;
    std::cout << "Print multiple values: ";
    print(1, 2.5, "hello", 'c', 42);
}

// Example 9: Initializer lists
void initializerListExample() {
    std::cout << "\n=== Initializer Lists ===" << std::endl;
    std::vector<int> v = {1, 2, 3, 4, 5};
    
    std::cout << "Vector elements: ";
    for (auto n : v) {
        std::cout << n << " ";
    }
    std::cout << std::endl;
}

// Example 10: Delegating constructors
class Rectangle {
    int width, height;
public:
    Rectangle() : Rectangle(0, 0) {
        std::cout << "Default constructor (delegates)" << std::endl;
    }
    Rectangle(int w, int h) : width(w), height(h) {
        std::cout << "Parameterized constructor: " << width << "x" << height << std::endl;
    }
    int area() const { return width * height; }
};

void delegatingConstructorExample() {
    std::cout << "\n=== Delegating Constructors ===" << std::endl;
    Rectangle r1;
    Rectangle r2(10, 20);
    std::cout << "Area of r2: " << r2.area() << std::endl;
}

// Example 11: Threading
std::mutex mtx;
int shared_counter = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);
    ++shared_counter;
}

void threadingExample() {
    std::cout << "\n=== Threading Support ===" << std::endl;
    shared_counter = 0;
    
    std::thread t1(increment);
    std::thread t2(increment);
    std::thread t3(increment);
    
    t1.join();
    t2.join();
    t3.join();
    
    std::cout << "Counter value: " << shared_counter << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     C++11 Feature Examples" << std::endl;
    std::cout << "========================================" << std::endl;
    
    autoExample();
    rangeForExample();
    lambdaExample();
    smartPointerExample();
    nullptrExample();
    enumClassExample();
    staticAssertExample();
    variadicTemplateExample();
    initializerListExample();
    delegatingConstructorExample();
    threadingExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "     All C++11 examples completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
