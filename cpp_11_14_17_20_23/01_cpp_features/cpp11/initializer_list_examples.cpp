//

/*
===========================================================================================
STD::INITIALIZER_LIST - COMPREHENSIVE USAGE EXAMPLES
===========================================================================================

This file demonstrates various scenarios and use cases of std::initializer_list in C++11/14/17/20:
1. Basic std::initializer_list usage
2. Constructor overloading with initializer_list
3. Function parameters with initializer_list
4. STL container initialization
5. Custom container implementation
6. Template deduction with initializer_list
7. Performance considerations
8. Advanced use cases and patterns

Key Features:
- Enables uniform initialization syntax
- Provides lightweight view over array of objects
- Used extensively in STL containers
- Supports range-based for loops
- Immutable elements (const access only)

===========================================================================================
*/

#include <iostream>
#include <initializer_list>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <memory>
#include <type_traits>
#include <cassert>
td#include <map>
#include <array>
#include <chrono>

// ============================================================================
// 1. BASIC STD::INITIALIZER_LIST USAGE
// ============================================================================

void demonstrate_basic_initializer_list() {
    std::cout << "\n=== BASIC STD::INITIALIZER_LIST USAGE ===\n";

    // Creating an initializer_list
    std::initializer_list<int> numbers = {1, 2, 3, 4, 5};

    std::cout << "1. Basic properties:\n";
    std::cout << "   Size: " << numbers.size() << std::endl;
    std::cout << "   Elements: ";

    // Iterating through initializer_list
    for (const auto& num : numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    // Using iterators
    std::cout << "   Using iterators: ";
    for (auto it = numbers.begin(); it != numbers.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Key characteristics
    std::cout << "\n2. Key characteristics:\n";
    std::cout << "   - Elements are const: " << std::is_const_v<std::remove_reference_t<decltype(*numbers.begin())>> << std::endl;
    std::cout << "   - Lightweight view (like span)\n";
    std::cout << "   - Automatic type deduction from braced list\n";
}

// ============================================================================
// 2. CONSTRUCTOR OVERLOADING WITH INITIALIZER_LIST
// ============================================================================

// Example class demonstrating constructor overloading
class NumberContainer {
private:
    std::vector<int> data_;

public:
    // Default constructor
    NumberContainer() {
        std::cout << "Default constructor called\n";
    }

    // Single value constructor
    NumberContainer(int value) : data_(1, value) {
        std::cout << "Single value constructor called with: " << value << std::endl;
    }

    // Size + value constructor
    NumberContainer(size_t count, int value) : data_(count, value) {
        std::cout << "Size + value constructor called with count: " << count
                  << ", value: " << value << std::endl;
    }

    // Initializer list constructor
    NumberContainer(std::initializer_list<int> init_list) : data_(init_list) {
        std::cout << "Initializer list constructor called with " << init_list.size()
                  << " elements\n";
    }

    // Copy constructor
    NumberContainer(const NumberContainer& other) : data_(other.data_) {
        std::cout << "Copy constructor called\n";
    }

    // Move constructor
    NumberContainer(NumberContainer&& other) noexcept : data_(std::move(other.data_)) {
        std::cout << "Move constructor called\n";
    }

    // Display contents
    void display() const {
        std::cout << "   Contents: ";
        for (const auto& elem : data_) {
            std::cout << elem << " ";
        }
        std::cout << "(size: " << data_.size() << ")" << std::endl;
    }

    size_t size() const { return data_.size(); }
    const std::vector<int>& data() const { return data_; }
};

void demonstrate_constructor_overloading() {
    std::cout << "\n=== CONSTRUCTOR OVERLOADING WITH INITIALIZER_LIST ===\n";

    std::cout << "\n1. Different constructor calls:\n";

    // Default constructor
    NumberContainer c1;
    c1.display();

    // Single value constructor
    NumberContainer c2(42);
    c2.display();

    // Size + value constructor
    NumberContainer c3(3, 99);
    c3.display();

    // Initializer list constructor - this is preferred!
    NumberContainer c4{1, 2, 3, 4, 5};
    c4.display();

    // Edge case: single element initializer list
    NumberContainer c5{42};  // Calls initializer_list constructor, not single value!
    c5.display();

    // Empty initializer list
    NumberContainer c6{};  // Calls initializer_list constructor with empty list
    c6.display();

    std::cout << "\n2. Important note:\n";
    std::cout << "   NumberContainer{42} calls initializer_list constructor\n";
    std::cout << "   NumberContainer(42) calls single value constructor\n";
}

// ============================================================================
// 3. FUNCTION PARAMETERS WITH INITIALIZER_LIST
// ============================================================================

// Function accepting initializer_list
int sum_values(std::initializer_list<int> values) {
    return std::accumulate(values.begin(), values.end(), 0);
}

// Function with multiple parameters including initializer_list
void print_with_prefix(const std::string& prefix, std::initializer_list<std::string> items) {
    std::cout << prefix << ": ";
    for (const auto& item : items) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
}

// Template function with initializer_list
template<typename T>
T find_max(std::initializer_list<T> values) {
    return *std::max_element(values.begin(), values.end());
}

// Variadic template vs initializer_list comparison
template<typename... Args>
auto sum_variadic(Args... args) {
    return (args + ...);  // C++17 fold expression
}

void demonstrate_function_parameters() {
    std::cout << "\n=== FUNCTION PARAMETERS WITH INITIALIZER_LIST ===\n";

    std::cout << "\n1. Basic function calls:\n";

    // Direct initializer_list calls
    int result = sum_values({1, 2, 3, 4, 5});
    std::cout << "   Sum of {1, 2, 3, 4, 5}: " << result << std::endl;

    // Can't deduce from empty list without explicit type
    // sum_values({}); // ERROR: Cannot deduce type

    // Explicit type for empty list
    result = sum_values(std::initializer_list<int>{});
    std::cout << "   Sum of empty list: " << result << std::endl;

    std::cout << "\n2. Function with multiple parameters:\n";
    print_with_prefix("Languages", {"C++", "Python", "Rust", "Go"});
    print_with_prefix("Numbers", {"One", "Two", "Three"});

    std::cout << "\n3. Template function:\n";
    auto max_int = find_max({10, 5, 20, 15});
    std::cout << "   Max of {10, 5, 20, 15}: " << max_int << std::endl;

    auto max_double = find_max({3.14, 2.71, 1.41, 1.73});
    std::cout << "   Max of {3.14, 2.71, 1.41, 1.73}: " << max_double << std::endl;

    std::cout << "\n4. Variadic template vs initializer_list:\n";
    auto sum1 = sum_variadic(1, 2, 3, 4, 5);  // Compile-time evaluation
    auto sum2 = sum_values({1, 2, 3, 4, 5});  // Runtime evaluation
    std::cout << "   Variadic sum: " << sum1 << std::endl;
    std::cout << "   Initializer_list sum: " << sum2 << std::endl;
}

// ============================================================================
// 4. STL CONTAINER INITIALIZATION
// ============================================================================

void demonstrate_stl_container_initialization() {
    std::cout << "\n=== STL CONTAINER INITIALIZATION ===\n";

    std::cout << "\n1. Vector initialization:\n";

    // Different ways to initialize vectors
    std::vector<int> v1{1, 2, 3, 4, 5};                    // Initializer list
    std::vector<int> v2(5, 10);                            // Size + value
    std::vector<int> v3{v1.begin(), v1.end()};             // Range constructor? NO! Initializer list
    std::vector<int> v4(v1.begin(), v1.end());             // Range constructor

    auto print_vector = [](const std::string& name, const std::vector<int>& v) {
        std::cout << "   " << name << ": ";
        for (const auto& elem : v) std::cout << elem << " ";
        std::cout << "(size: " << v.size() << ")" << std::endl;
    };

    print_vector("v1{1,2,3,4,5}", v1);
    print_vector("v2(5,10)", v2);
    print_vector("v3{begin,end}", v3);  // Creates vector with 2 elements (the iterator values)!
    print_vector("v4(begin,end)", v4);

    std::cout << "\n2. Other container types:\n";

    // String initialization
    std::string s1{'H', 'e', 'l', 'l', 'o'};
    std::cout << "   String from chars: " << s1 << std::endl;

    // Array initialization (C++11)
    std::array<int, 5> arr{10, 20, 30, 40, 50};
    std::cout << "   Array: ";
    for (const auto& elem : arr) std::cout << elem << " ";
    std::cout << std::endl;

    // Map initialization
    std::map<std::string, int> ages{
        {"Alice", 25},
        {"Bob", 30},
        {"Charlie", 35}
    };
    std::cout << "   Map contents:\n";
    for (const auto& [name, age] : ages) {
        std::cout << "     " << name << ": " << age << std::endl;
    }
}

// ============================================================================
// 5. CUSTOM CONTAINER IMPLEMENTATION
// ============================================================================

// Custom container that supports initializer_list
template<typename T>
class SimpleVector {
private:
    std::unique_ptr<T[]> data_;
    size_t size_;
    size_t capacity_;

    void reallocate(size_t new_capacity) {
        auto new_data = std::make_unique<T[]>(new_capacity);
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data_[i]);
        }
        data_ = std::move(new_data);
        capacity_ = new_capacity;
    }

public:
    // Default constructor
    SimpleVector() : data_(nullptr), size_(0), capacity_(0) {}

    // Size constructor
    explicit SimpleVector(size_t count)
        : data_(std::make_unique<T[]>(count)), size_(count), capacity_(count) {
        // Default initialize elements
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = T{};
        }
    }

    // Initializer list constructor
    SimpleVector(std::initializer_list<T> init_list)
        : data_(std::make_unique<T[]>(init_list.size())),
          size_(init_list.size()),
          capacity_(init_list.size()) {

        size_t i = 0;
        for (const auto& elem : init_list) {
            data_[i++] = elem;
        }
    }

    // Copy constructor
    SimpleVector(const SimpleVector& other)
        : data_(std::make_unique<T[]>(other.capacity_)),
          size_(other.size_),
          capacity_(other.capacity_) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    // Move constructor
    SimpleVector(SimpleVector&& other) noexcept
        : data_(std::move(other.data_)),
          size_(other.size_),
          capacity_(other.capacity_) {
        other.size_ = 0;
        other.capacity_ = 0;
    }

    // Assignment operators
    SimpleVector& operator=(std::initializer_list<T> init_list) {
        if (init_list.size() > capacity_) {
            data_ = std::make_unique<T[]>(init_list.size());
            capacity_ = init_list.size();
        }

        size_ = init_list.size();
        size_t i = 0;
        for (const auto& elem : init_list) {
            data_[i++] = elem;
        }
        return *this;
    }

    // Element access
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }

    // Capacity
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    // Modifiers
    void push_back(const T& value) {
        if (size_ == capacity_) {
            reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        data_[size_++] = value;
    }

    // Iterator support
    T* begin() { return data_.get(); }
    T* end() { return data_.get() + size_; }
    const T* begin() const { return data_.get(); }
    const T* end() const { return data_.get() + size_; }

    // Display function
    void display(const std::string& name) const {
        std::cout << "   " << name << ": ";
        for (size_t i = 0; i < size_; ++i) {
            std::cout << data_[i] << " ";
        }
        std::cout << "(size: " << size_ << ", capacity: " << capacity_ << ")" << std::endl;
    }
};

void demonstrate_custom_container() {
    std::cout << "\n=== CUSTOM CONTAINER IMPLEMENTATION ===\n";

    std::cout << "\n1. Constructor usage:\n";

    // Different constructors
    SimpleVector<int> v1;                    // Default
    SimpleVector<int> v2(5);                 // Size constructor
    SimpleVector<int> v3{1, 2, 3, 4, 5};     // Initializer list

    v1.display("v1 (default)");
    v2.display("v2 (size 5)");
    v3.display("v3 (init list)");

    std::cout << "\n2. Assignment and modification:\n";

    // Assignment from initializer list
    v1 = {10, 20, 30};
    v1.display("v1 after assignment");

    // Adding elements
    v1.push_back(40);
    v1.push_back(50);
    v1.display("v1 after push_back");

    std::cout << "\n3. Range-based for loop:\n";
    std::cout << "   Elements: ";
    for (const auto& elem : v3) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
}

// ============================================================================
// 6. TEMPLATE TYPE DEDUCTION WITH INITIALIZER_LIST
// ============================================================================

// Template function that doesn't work with initializer_list
template<typename T>
void template_func_fails(T param) {
    std::cout << "Template function called\n";
}

// Specialized overload for initializer_list
template<typename T>
void template_func_works(std::initializer_list<T> param) {
    std::cout << "Initializer list template function called with "
              << param.size() << " elements\n";
}

// Auto deduction examples
void demonstrate_template_deduction() {
    std::cout << "\n=== TEMPLATE TYPE DEDUCTION WITH INITIALIZER_LIST ===\n";

    std::cout << "\n1. Template deduction issues:\n";

    // This works
    template_func_fails(42);
    template_func_fails(3.14);

    // This doesn't work - cannot deduce T from braced list
    // template_func_fails({1, 2, 3}); // ERROR!
    std::cout << "   template_func_fails({1, 2, 3}); // ERROR: Cannot deduce T\n";

    // This works - explicit specialization
    template_func_works({1, 2, 3, 4, 5});
    template_func_works({1.1, 2.2, 3.3});

    std::cout << "\n2. Auto deduction:\n";

    // Auto with braced lists
    auto list1 = {1, 2, 3, 4, 5};  // std::initializer_list<int>
    auto list2 = {1.1, 2.2, 3.3};  // std::initializer_list<double>

    std::cout << "   auto list1 = {1, 2, 3, 4, 5}; // Type: std::initializer_list<int>\n";
    std::cout << "   auto list2 = {1.1, 2.2, 3.3}; // Type: std::initializer_list<double>\n";

    // Mixed types don't work
    // auto list3 = {1, 2.2, 3}; // ERROR: Cannot deduce type
    std::cout << "   auto list3 = {1, 2.2, 3}; // ERROR: Mixed types\n";

    std::cout << "\n3. C++17 template argument deduction:\n";

    // C++17: Class template argument deduction
    std::vector v1{1, 2, 3, 4, 5};      // std::vector<int>
    std::vector v2{1.1, 2.2, 3.3};      // std::vector<double>

    std::cout << "   std::vector v1{1, 2, 3}; // C++17 CTAD: std::vector<int>\n";
    std::cout << "   std::vector v2{1.1, 2.2}; // C++17 CTAD: std::vector<double>\n";
}

// ============================================================================
// 7. PERFORMANCE CONSIDERATIONS
// ============================================================================

// Performance comparison function
void performance_comparison() {
    std::cout << "\n=== PERFORMANCE CONSIDERATIONS ===\n";

    const size_t N = 1000000;

    std::cout << "\n1. Memory characteristics:\n";
    std::cout << "   - Initializer_list elements are stored in static memory\n";
    std::cout << "   - Lightweight view (no ownership)\n";
    std::cout << "   - Elements are const (immutable)\n";
    std::cout << "   - Efficient for small, known-at-compile-time lists\n";

    std::cout << "\n2. Construction performance:\n";

    auto start = std::chrono::high_resolution_clock::now();

    // Initializer list construction
    for (size_t i = 0; i < N; ++i) {
        std::vector<int> v{1, 2, 3, 4, 5};
        (void)v; // Suppress unused variable warning
    }

    auto mid = std::chrono::high_resolution_clock::now();

    // Traditional construction
    for (size_t i = 0; i < N; ++i) {
        std::vector<int> v;
        v.reserve(5);
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        v.push_back(4);
        v.push_back(5);
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto init_list_time = std::chrono::duration_cast<std::chrono::microseconds>(mid - start);
    auto traditional_time = std::chrono::duration_cast<std::chrono::microseconds>(end - mid);

    std::cout << "   Initializer list: " << init_list_time.count() << " μs\n";
    std::cout << "   Traditional: " << traditional_time.count() << " μs\n";
    std::cout << "   Ratio: " << static_cast<double>(traditional_time.count()) / init_list_time.count() << "x\n";
}

// ============================================================================
// 8. ADVANCED USE CASES AND PATTERNS
// ============================================================================

// Builder pattern with initializer_list
class ConfigBuilder {
private:
    std::map<std::string, std::string> config_;

public:
    ConfigBuilder(std::initializer_list<std::pair<std::string, std::string>> init_list) {
        for (const auto& [key, value] : init_list) {
            config_[key] = value;
        }
    }

    void display() const {
        std::cout << "   Configuration:\n";
        for (const auto& [key, value] : config_) {
            std::cout << "     " << key << " = " << value << std::endl;
        }
    }
};

// Function object with initializer_list
class Multiplier {
private:
    std::vector<double> factors_;

public:
    Multiplier(std::initializer_list<double> factors) : factors_(factors) {}

    double operator()(double value) const {
        double result = value;
        for (double factor : factors_) {
            result *= factor;
        }
        return result;
    }
};

// Recursive initializer_list processing
template<typename T>
void print_nested_list(std::initializer_list<std::initializer_list<T>> nested_list) {
    std::cout << "   Nested list:\n";
    size_t row = 0;
    for (const auto& inner_list : nested_list) {
        std::cout << "     Row " << row++ << ": ";
        for (const auto& elem : inner_list) {
            std::cout << elem << " ";
        }
        std::cout << std::endl;
    }
}

void demonstrate_advanced_use_cases() {
    std::cout << "\n=== ADVANCED USE CASES AND PATTERNS ===\n";

    std::cout << "\n1. Builder pattern:\n";
    ConfigBuilder config{
        {"host", "localhost"},
        {"port", "8080"},
        {"ssl", "true"},
        {"timeout", "30"}
    };
    config.display();

    std::cout << "\n2. Function object with factors:\n";
    Multiplier mult{2.0, 1.5, 0.8};
    double result = mult(10.0);
    std::cout << "   10.0 * 2.0 * 1.5 * 0.8 = " << result << std::endl;

    std::cout << "\n3. Nested initializer lists:\n";
    print_nested_list<int>({
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    });

    std::cout << "\n4. Initializer list with algorithms:\n";
    auto numbers = {5, 2, 8, 1, 9, 3};

    auto min_elem = std::min_element(numbers.begin(), numbers.end());
    auto max_elem = std::max_element(numbers.begin(), numbers.end());
    auto sum = std::accumulate(numbers.begin(), numbers.end(), 0);

    std::cout << "   Numbers: ";
    for (auto n : numbers) std::cout << n << " ";
    std::cout << std::endl;
    std::cout << "   Min: " << *min_elem << ", Max: " << *max_elem << ", Sum: " << sum << std::endl;
}

// ============================================================================
// 9. COMMON PITFALLS AND BEST PRACTICES
// ============================================================================

void demonstrate_pitfalls_and_best_practices() {
    std::cout << "\n=== COMMON PITFALLS AND BEST PRACTICES ===\n";

    std::cout << "\n1. Pitfall: Dangling references\n";
    std::cout << "   auto get_list() -> std::initializer_list<int> {\n";
    std::cout << "       return {1, 2, 3}; // DANGER: Returns reference to temporary!\n";
    std::cout << "   }\n";
    std::cout << "   // The returned initializer_list refers to destroyed temporaries\n";

    std::cout << "\n2. Pitfall: Constructor ambiguity\n";
    std::cout << "   std::vector<int> v1(10, 5);  // 10 elements, each value 5\n";
    std::cout << "   std::vector<int> v2{10, 5};  // 2 elements: 10 and 5\n";

    std::vector<int> v1(10, 5);
    std::vector<int> v2{10, 5};
    std::cout << "   v1 size: " << v1.size() << ", v2 size: " << v2.size() << std::endl;

    std::cout << "\n3. Best practice: Use when appropriate\n";
    std::cout << "   ✓ Small, compile-time known collections\n";
    std::cout << "   ✓ Constructor initialization\n";
    std::cout << "   ✓ Function parameters for convenience\n";
    std::cout << "   ✗ Large collections (prefer other containers)\n";
    std::cout << "   ✗ When you need to modify elements\n";
    std::cout << "   ✗ Runtime-determined collections\n";

    std::cout << "\n4. Best practice: Explicit constructors\n";
    std::cout << "   Consider making single-parameter constructors explicit\n";
    std::cout << "   to avoid unintended conversions.\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "STD::INITIALIZER_LIST COMPREHENSIVE EXAMPLES\n";
    std::cout << "============================================\n";

    // Demonstrate all aspects of std::initializer_list
    demonstrate_basic_initializer_list();
    demonstrate_constructor_overloading();
    demonstrate_function_parameters();
    demonstrate_stl_container_initialization();
    demonstrate_custom_container();
    demonstrate_template_deduction();
    performance_comparison();
    demonstrate_advanced_use_cases();
    demonstrate_pitfalls_and_best_practices();

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Key takeaways:\n";
    std::cout << "1. std::initializer_list enables uniform initialization syntax\n";
    std::cout << "2. Initializer list constructors are preferred in overload resolution\n";
    std::cout << "3. Elements are const and stored in static memory\n";
    std::cout << "4. Cannot be used for template argument deduction without explicit specialization\n";
    std::cout << "5. Excellent for small, compile-time known collections\n";
    std::cout << "6. Be aware of constructor ambiguity with () vs {} syntax\n";

    return 0;
}

/*
===========================================================================================
COMPILATION AND TESTING NOTES
===========================================================================================

Compile with:
g++ -std=c++17 -O2 initializer_list_examples.cpp -o initializer_list_demo

For C++20 features:
g++ -std=c++20 -O2 initializer_list_examples.cpp -o initializer_list_demo

Expected output will demonstrate:
- Basic std::initializer_list usage and properties
- Constructor overloading behavior
- Function parameter usage
- STL container initialization patterns
- Custom container implementation
- Template deduction issues and solutions
- Performance characteristics
- Advanced use cases and patterns
- Common pitfalls and best practices

Key learning points:
1. std::initializer_list is a lightweight view over const elements
2. Enables  C++ initialization syntax
3. Preferred in constructomodernr overload resolution
4. Has specific template deduction limitations
5. Essential for writing modern, expressive C++ code

===========================================================================================
*/
