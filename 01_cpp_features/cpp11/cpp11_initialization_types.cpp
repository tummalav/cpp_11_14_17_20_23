// =============================
// C++11 INITIALIZATION TYPES
// =============================
/*
C++11 introduced several new initialization syntaxes and improved existing ones.
This file demonstrates all the different kinds of initializations supported in C++11.
*/

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <initializer_list>
#include <array>

// =============================
// 1. DIRECT INITIALIZATION
// =============================
void direct_initialization_examples() {
    std::cout << "=== DIRECT INITIALIZATION ===\n";

    // Basic types with parentheses
    int x(42);                          // Direct initialization
    double d(3.14159);                  // Direct initialization
    char c('A');                        // Direct initialization
    bool flag(true);                    // Direct initialization

    // Objects with constructors
    std::string str("Hello World");     // Direct initialization with constructor
    std::vector<int> vec(5, 10);        // 5 elements, each initialized to 10
    std::vector<std::string> names(3, "default"); // 3 strings, each "default"

    // Complex objects
    std::pair<int, std::string> p(42, "answer");

    std::cout << "int x(42): " << x << "\n";
    std::cout << "string str(\"Hello World\"): " << str << "\n";
    std::cout << "vector<int> vec(5, 10) size: " << vec.size() << "\n";
    std::cout << "pair<int, string> p: (" << p.first << ", " << p.second << ")\n\n";
}

// =============================
// 2. COPY INITIALIZATION
// =============================
void copy_initialization_examples() {
    std::cout << "=== COPY INITIALIZATION ===\n";

    // Basic types with assignment syntax
    int x = 42;                         // Copy initialization
    double d = 3.14159;                 // Copy initialization
    char c = 'A';                       // Copy initialization
    bool flag = true;                   // Copy initialization

    // Objects with assignment syntax
    std::string str = "Hello World";    // Copy initialization (may optimize to direct)
    std::string str2 = std::string("Copy"); // Explicit copy initialization

    // Copy initialization with braced-init-list (C++11)
    std::vector<int> vec = {1, 2, 3, 4, 5}; // Copy list initialization
    std::map<int, std::string> map = {{1, "one"}, {2, "two"}}; // Copy list initialization

    std::cout << "int x = 42: " << x << "\n";
    std::cout << "string str = \"Hello World\": " << str << "\n";
    std::cout << "vector<int> vec = {1,2,3,4,5} size: " << vec.size() << "\n";
    std::cout << "map size: " << map.size() << "\n\n";
}

// =============================
// 3. UNIFORM INITIALIZATION (BRACED INITIALIZATION)
// =============================
void uniform_initialization_examples() {
    std::cout << "=== UNIFORM INITIALIZATION (BRACED) ===\n";

    // Direct list initialization
    int x{42};                          // Direct braced initialization
    double d{3.14159};                  // Direct braced initialization
    char c{'A'};                        // Direct braced initialization

    // Objects with braces
    std::string str{"Hello Braces"};    // Direct braced initialization
    std::vector<int> vec{1, 2, 3, 4, 5}; // Direct list initialization
    std::map<int, std::string> map{{1, "one"}, {2, "two"}}; // Direct list initialization

    // Copy list initialization
    int y = {42};                       // Copy braced initialization
    std::string str2 = {"Copy Braces"}; // Copy braced initialization
    std::vector<int> vec2 = {10, 20, 30}; // Copy list initialization

    // Zero/value initialization with braces
    int zero{};                         // Value initialization (zero)
    std::string empty{};                // Value initialization (empty string)
    std::vector<int> empty_vec{};       // Value initialization (empty vector)

    // Prevents narrowing conversions
    // int narrow{3.14};                // Error: narrowing conversion
    int safe{3};                        // OK: no narrowing

    std::cout << "int x{42}: " << x << "\n";
    std::cout << "string str{\"Hello Braces\"}: " << str << "\n";
    std::cout << "vector<int> vec{1,2,3,4,5} size: " << vec.size() << "\n";
    std::cout << "int zero{}: " << zero << "\n";
    std::cout << "empty string length: " << empty.length() << "\n\n";
}

// =============================
// 4. DEFAULT INITIALIZATION
// =============================
class DefaultClass {
public:
    int value;                          // Default initialization (undefined for built-in types)
    std::string name;                   // Default initialization (empty string)

    DefaultClass() = default;           // Compiler-generated default constructor
};

void default_initialization_examples() {
    std::cout << "=== DEFAULT INITIALIZATION ===\n";

    // Built-in types (undefined behavior - don't use in practice)
    // int x;                           // Default initialization (undefined value)

    // Objects with default constructors
    std::string str;                    // Default initialization (empty string)
    std::vector<int> vec;               // Default initialization (empty vector)
    DefaultClass obj;                   // Default initialization

    std::cout << "default string length: " << str.length() << "\n";
    std::cout << "default vector size: " << vec.size() << "\n";
    std::cout << "default object string: \"" << obj.name << "\"\n\n";
}

// =============================
// 5. VALUE INITIALIZATION
// =============================
void value_initialization_examples() {
    std::cout << "=== VALUE INITIALIZATION ===\n";

    // Value initialization with braces
    int x{};                            // Value initialization (zero)
    double d{};                         // Value initialization (0.0)
    char c{};                           // Value initialization ('\0')
    bool flag{};                        // Value initialization (false)

    // Pointer value initialization
    int* ptr{};                         // Value initialization (nullptr)

    // Object value initialization
    std::string str{};                  // Value initialization (empty string)
    std::vector<int> vec{};             // Value initialization (empty vector)

    // Array value initialization
    int arr[5]{};                       // All elements value-initialized to 0

    // Dynamic allocation with value initialization
    int* dynamic_int = new int{};       // Value-initialized to 0
    std::string* dynamic_str = new std::string{}; // Value-initialized to empty

    std::cout << "int x{}: " << x << "\n";
    std::cout << "double d{}: " << d << "\n";
    std::cout << "char c{}: " << static_cast<int>(c) << " (as int)\n";
    std::cout << "bool flag{}: " << flag << "\n";
    std::cout << "ptr{}: " << (ptr ? "not null" : "null") << "\n";
    std::cout << "dynamic int: " << *dynamic_int << "\n";

    delete dynamic_int;
    delete dynamic_str;
    std::cout << "\n";
}

// =============================
// 6. AGGREGATE INITIALIZATION
// =============================
struct Point {
    int x, y;
};

struct Line {
    Point start, end;
};

struct Person {
    std::string name;
    int age;
    double height;
};

void aggregate_initialization_examples() {
    std::cout << "=== AGGREGATE INITIALIZATION ===\n";

    // Simple aggregate initialization
    Point p1{10, 20};                   // Aggregate initialization
    Point p2 = {30, 40};                // Aggregate initialization (copy form)

    // Nested aggregate initialization
    Line line{{0, 0}, {10, 10}};        // Nested aggregate initialization
    Line line2 = {{5, 5}, {15, 15}};    // Copy form

    // Mixed type aggregate
    Person person{"Alice", 25, 5.6};    // Mixed type aggregate
    Person person2 = {"Bob", 30, 6.0};  // Copy form

    // Array aggregate initialization
    int arr1[]{1, 2, 3, 4, 5};          // Array aggregate (size deduced)
    int arr2[5]{1, 2};                  // Partially initialized (rest are 0)
    int arr3[3] = {7, 8, 9};            // Traditional array initialization

    // Multi-dimensional arrays
    int matrix[][3] = {{1, 2, 3}, {4, 5, 6}}; // 2D array aggregate

    std::cout << "Point p1: (" << p1.x << ", " << p1.y << ")\n";
    std::cout << "Line: (" << line.start.x << "," << line.start.y << ") to ("
              << line.end.x << "," << line.end.y << ")\n";
    std::cout << "Person: " << person.name << ", " << person.age << " years, "
              << person.height << "ft\n";
    std::cout << "Array arr2[5]{1,2}: ";
    for (int i = 0; i < 5; ++i) std::cout << arr2[i] << " ";
    std::cout << "\n\n";
}

// =============================
// 7. MEMBER INITIALIZER LISTS (C++11 NSDMI)
// =============================
class MemberInitClass {
    int x = 5;                          // Default member initializer (C++11)
    std::string name{"default"};        // Braced default member initializer
    std::vector<int> data{1, 2, 3};     // Container default initialization
    double* ptr = nullptr;              // Pointer default initialization

public:
    MemberInitClass() = default;        // Uses default member initializers

    MemberInitClass(int val) : x(val) { // Constructor overrides default for x
    }

    MemberInitClass(int val, const std::string& n) : x(val), name(n) {
        // Constructor overrides defaults for x and name
    }

    void print() const {
        std::cout << "x: " << x << ", name: " << name << ", data size: " << data.size() << "\n";
    }
};

void member_initializer_examples() {
    std::cout << "=== MEMBER INITIALIZER LISTS (NSDMI) ===\n";

    MemberInitClass obj1;               // Uses all default member initializers
    MemberInitClass obj2(100);          // Overrides x, uses defaults for others
    MemberInitClass obj3(200, "custom"); // Overrides x and name

    std::cout << "obj1 (all defaults): ";
    obj1.print();
    std::cout << "obj2 (x=100): ";
    obj2.print();
    std::cout << "obj3 (x=200, name=custom): ";
    obj3.print();
    std::cout << "\n";
}

// =============================
// 8. CONSTRUCTOR INITIALIZATION
// =============================
class ConstructorInitClass {
    int x;
    std::string name;
    std::vector<int> data;

public:
    // Member initializer list
    ConstructorInitClass(int val, const std::string& n)
        : x(val), name(n), data{val, val*2, val*3} {
    }

    // Delegating constructor (C++11)
    ConstructorInitClass() : ConstructorInitClass(0, "default") {
    }

    // Another delegating constructor
    ConstructorInitClass(int val) : ConstructorInitClass(val, "unnamed") {
    }

    void print() const {
        std::cout << "x: " << x << ", name: " << name << ", data: ";
        for (int val : data) std::cout << val << " ";
        std::cout << "\n";
    }
};

// Base class for inheritance example
class BaseClass {
protected:
    int base_value;

public:
    BaseClass(int val) : base_value(val) {
        std::cout << "BaseClass constructor: " << val << "\n";
    }
};

// Derived class using inheriting constructors
class DerivedClass : public BaseClass {
    std::string derived_name;

public:
    using BaseClass::BaseClass;         // Inheriting constructors (C++11)

    DerivedClass(int val, const std::string& name)
        : BaseClass(val), derived_name(name) {
        std::cout << "DerivedClass constructor: " << name << "\n";
    }
};

void constructor_initialization_examples() {
    std::cout << "=== CONSTRUCTOR INITIALIZATION ===\n";

    ConstructorInitClass obj1;          // Delegating to main constructor
    ConstructorInitClass obj2(42);      // Delegating to main constructor
    ConstructorInitClass obj3(100, "test"); // Direct main constructor

    std::cout << "obj1 (default): ";
    obj1.print();
    std::cout << "obj2 (42): ";
    obj2.print();
    std::cout << "obj3 (100, test): ";
    obj3.print();

    std::cout << "\nInheriting constructors:\n";
    DerivedClass derived1(50);          // Uses inherited constructor
    DerivedClass derived2(75, "custom"); // Uses own constructor
    std::cout << "\n";
}

// =============================
// 9. STATIC INITIALIZATION
// =============================
class StaticInitClass {
public:
    static const int constant = 100;           // Static const member initialization
    static constexpr double pi = 3.14159;     // Static constexpr member initialization
    // C++17 allows inline static variables:
    // static inline std::vector<int> static_data{1, 2, 3};
};

// Global static initialization
static int global_var = 42;                   // Static initialization
static std::vector<int> global_data{1, 2, 3}; // Static initialization with braces

void static_initialization_examples() {
    std::cout << "=== STATIC INITIALIZATION ===\n";

    std::cout << "Static const: " << StaticInitClass::constant << "\n";
    std::cout << "Static constexpr: " << StaticInitClass::pi << "\n";
    std::cout << "Global static var: " << global_var << "\n";
    std::cout << "Global static vector size: " << global_data.size() << "\n\n";
}

// =============================
// 10. DYNAMIC INITIALIZATION
// =============================
int get_runtime_value() {
    static int counter = 0;
    return ++counter * 10;
}

// Dynamic initialization (happens at runtime)
static int dynamic_var = get_runtime_value();  // Dynamic initialization
static std::vector<int> dynamic_vec(get_runtime_value()); // Dynamic size

void dynamic_initialization_examples() {
    std::cout << "=== DYNAMIC INITIALIZATION ===\n";

    // Local dynamic initialization
    std::vector<int> local_vec(get_runtime_value()); // Size determined at runtime
    auto smart_ptr = std::make_unique<std::vector<int>>(get_runtime_value());

    std::cout << "Dynamic var: " << dynamic_var << "\n";
    std::cout << "Dynamic vector size: " << dynamic_vec.size() << "\n";
    std::cout << "Local dynamic vector size: " << local_vec.size() << "\n";
    std::cout << "Smart pointer vector size: " << smart_ptr->size() << "\n\n";
}

// =============================
// 11. INITIALIZER_LIST CONSTRUCTOR
// =============================
class InitListClass {
    std::vector<int> data;

public:
    // Constructor that takes initializer_list
    InitListClass(std::initializer_list<int> list) : data(list) {
    }

    // Regular constructor
    InitListClass(int size, int value) : data(size, value) {
    }

    void print() const {
        std::cout << "Data: ";
        for (int val : data) std::cout << val << " ";
        std::cout << "(size: " << data.size() << ")\n";
    }
};

void initializer_list_examples() {
    std::cout << "=== INITIALIZER_LIST CONSTRUCTOR ===\n";

    InitListClass obj1{1, 2, 3, 4, 5};          // Uses initializer_list constructor
    InitListClass obj2(5, 10);                  // Uses regular constructor
    InitListClass obj3 = {7, 8, 9};             // Uses initializer_list constructor

    std::cout << "obj1{1,2,3,4,5}: ";
    obj1.print();
    std::cout << "obj2(5, 10): ";
    obj2.print();
    std::cout << "obj3 = {7,8,9}: ";
    obj3.print();
    std::cout << "\n";
}

// =============================
// 12. AUTO TYPE DEDUCTION WITH INITIALIZATION
// =============================
void auto_initialization_examples() {
    std::cout << "=== AUTO WITH DIFFERENT INITIALIZATIONS ===\n";

    // Auto with different initialization forms
    auto a = 42;                        // Copy initialization -> int
    auto b(3.14);                       // Direct initialization -> double
    auto c{100};                        // Direct list initialization -> int (C++17+)
    auto d = {1, 2, 3};                 // Copy list initialization -> initializer_list<int>

    // Auto with complex types
    auto vec = std::vector<int>{1, 2, 3}; // Copy initialization
    auto map = std::map<int, std::string>{{1, "one"}}; // Copy list initialization
    auto ptr = std::make_unique<int>(42); // Copy initialization

    std::cout << "auto a = 42: " << a << " (int)\n";
    std::cout << "auto b(3.14): " << b << " (double)\n";
    std::cout << "auto c{100}: " << c << " (int)\n";
    std::cout << "auto d = {1,2,3}: initializer_list with " << d.size() << " elements\n";
    std::cout << "auto vector size: " << vec.size() << "\n";
    std::cout << "auto map size: " << map.size() << "\n";
    std::cout << "auto unique_ptr value: " << *ptr << "\n\n";
}

// =============================
// MAIN FUNCTION
// =============================
int main() {
    std::cout << "C++11 INITIALIZATION TYPES DEMONSTRATION\n";
    std::cout << "=========================================\n\n";

    direct_initialization_examples();
    copy_initialization_examples();
    uniform_initialization_examples();
    default_initialization_examples();
    value_initialization_examples();
    aggregate_initialization_examples();
    member_initializer_examples();
    constructor_initialization_examples();
    static_initialization_examples();
    dynamic_initialization_examples();
    initializer_list_examples();
    auto_initialization_examples();

    std::cout << "=== SUMMARY OF C++11 INITIALIZATION TYPES ===\n";
    std::cout << "1. Direct Initialization: Type name(args)\n";
    std::cout << "2. Copy Initialization: Type name = value\n";
    std::cout << "3. Uniform/Braced Initialization: Type name{args} - NEW in C++11\n";
    std::cout << "4. Default Initialization: Type name; (for objects with default constructor)\n";
    std::cout << "5. Value Initialization: Type name{}; - ENHANCED in C++11\n";
    std::cout << "6. Aggregate Initialization: struct{args} - ENHANCED in C++11\n";
    std::cout << "7. Member Initializer Lists: class member = value; - NEW in C++11\n";
    std::cout << "8. Constructor Initialization: Member initializer lists, delegating - ENHANCED in C++11\n";
    std::cout << "9. Static Initialization: static/global variables\n";
    std::cout << "10. Dynamic Initialization: Runtime-computed initialization\n";
    std::cout << "11. Initializer_list Constructor: Type{list} - NEW in C++11\n";
    std::cout << "12. Auto Type Deduction: auto name = value; - NEW in C++11\n";
    std::cout << "\nKey C++11 Features:\n";
    std::cout << "- Uniform initialization syntax with braces\n";
    std::cout << "- Prevents narrowing conversions\n";
    std::cout << "- Default member initializers (NSDMI)\n";
    std::cout << "- Delegating constructors\n";
    std::cout << "- Inheriting constructors\n";
    std::cout << "- initializer_list support\n";
    std::cout << "- auto type deduction\n";

    return 0;
}
