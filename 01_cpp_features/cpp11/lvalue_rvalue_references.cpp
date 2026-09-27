#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <utility>

// ================================
// LVALUE AND RVALUE REFERENCES
// ================================

class MyClass {
private:
    std::string data;
    size_t size;

public:
    // Default constructor
    MyClass() : data("default"), size(7) {
        std::cout << "Default constructor called\n";
    }

    // Parameterized constructor
    MyClass(const std::string& str) : data(str), size(str.length()) {
        std::cout << "Parameterized constructor called with: " << str << "\n";
    }

    // Copy constructor (lvalue reference)
    MyClass(const MyClass& other) : data(other.data), size(other.size) {
        std::cout << "Copy constructor called\n";
    }

    // Move constructor (rvalue reference)
    MyClass(MyClass&& other) noexcept : data(std::move(other.data)), size(other.size) {
        std::cout << "Move constructor called\n";
        other.size = 0;
    }

    // Copy assignment operator (lvalue reference)
    MyClass& operator=(const MyClass& other) {
        if (this != &other) {
            data = other.data;
            size = other.size;
            std::cout << "Copy assignment called\n";
        }
        return *this;
    }

    // Move assignment operator (rvalue reference)
    MyClass& operator=(MyClass&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            size = other.size;
            other.size = 0;
            std::cout << "Move assignment called\n";
        }
        return *this;
    }

    // Destructor
    ~MyClass() {
        std::cout << "Destructor called for: " << data << "\n";
    }

    // Getter methods
    const std::string& getData() const { return data; }
    size_t getSize() const { return size; }

    // Method that demonstrates lvalue reference parameter
    void processLValue(const std::string& str) {
        std::cout << "Processing lvalue: " << str << "\n";
        data += " + " + str;
    }

    // Method that demonstrates rvalue reference parameter
    void processRValue(std::string&& str) {
        std::cout << "Processing rvalue: " << str << "\n";
        data += " + " + std::move(str);
        // str is now in a valid but unspecified state
    }
};

// ================================
// UTILITY FUNCTIONS
// ================================

// Function returning by value (creates rvalue)
MyClass createObject(const std::string& str) {
    return MyClass(str);
}

// Function demonstrating perfect forwarding
template<typename T>
void forwardToConstructor(T&& arg) {
    std::cout << "Perfect forwarding: ";
    MyClass obj(std::forward<T>(arg));
}

// Function overloads for lvalue and rvalue references
void processValue(const std::string& str) {
    std::cout << "Lvalue version: " << str << "\n";
}

void processValue(std::string&& str) {
    std::cout << "Rvalue version: " << str << "\n";
    // Can modify str here since it's an rvalue
    str += " (modified)";
    std::cout << "Modified rvalue: " << str << "\n";
}

// ================================
// DEMONSTRATION FUNCTIONS
// ================================

void demonstrateLValueReferences() {
    std::cout << "\n=== LVALUE REFERENCES ===\n";

    // Lvalues are objects with identifiable memory locations
    std::string str1 = "Hello";
    std::string str2 = "World";

    // Lvalue references bind to lvalues
    std::string& ref1 = str1;  // OK: lvalue reference to lvalue
    const std::string& ref2 = str2;  // OK: const lvalue reference to lvalue

    std::cout << "Original str1: " << str1 << "\n";
    std::cout << "Reference ref1: " << ref1 << "\n";

    ref1 += " Modified";
    std::cout << "After modifying through reference: " << str1 << "\n";

    // Const lvalue references can bind to temporaries (rvalues)
    const std::string& ref3 = std::string("Temporary");  // OK: extends lifetime
    std::cout << "Const ref to temporary: " << ref3 << "\n";
}

void demonstrateRValueReferences() {
    std::cout << "\n=== RVALUE REFERENCES ===\n";

    // Rvalue references bind to rvalues (temporaries, literals, etc.)
    std::string&& rref1 = std::string("Temporary String");  // OK: rvalue ref to rvalue
    std::string&& rref2 = "Another Temp";  // OK: rvalue ref to string literal

    std::cout << "Rvalue reference 1: " << rref1 << "\n";
    std::cout << "Rvalue reference 2: " << rref2 << "\n";

    // Can modify through rvalue reference
    rref1 += " Modified";
    std::cout << "Modified rvalue ref: " << rref1 << "\n";

    // Moving from lvalue using std::move
    std::string str = "Original String";
    std::string&& rref3 = std::move(str);  // std::move casts lvalue to rvalue
    std::cout << "Moved string: " << rref3 << "\n";
    std::cout << "Original after move: '" << str << "' (unspecified state)\n";
}

void demonstrateMoveSemantics() {
    std::cout << "\n=== MOVE SEMANTICS ===\n";

    // Creating objects
    MyClass obj1("Object1");
    MyClass obj2 = createObject("TempObject");  // Move constructor called

    // Copy vs Move assignment
    std::cout << "\n--- Copy Assignment ---\n";
    MyClass obj3;
    obj3 = obj1;  // Copy assignment (obj1 is lvalue)

    std::cout << "\n--- Move Assignment ---\n";
    MyClass obj4;
    obj4 = createObject("AnotherTemp");  // Move assignment (temporary is rvalue)

    std::cout << "\n--- Explicit Move ---\n";
    MyClass obj5;
    obj5 = std::move(obj1);  // Move assignment (std::move makes obj1 rvalue)

    std::cout << "obj1 after move: " << obj1.getData() << "\n";
}

void demonstratePerfectForwarding() {
    std::cout << "\n=== PERFECT FORWARDING ===\n";

    std::string str = "Lvalue String";

    std::cout << "Forwarding lvalue:\n";
    forwardToConstructor(str);

    std::cout << "\nForwarding rvalue:\n";
    forwardToConstructor(std::string("Rvalue String"));
}

void demonstrateFunctionOverloads() {
    std::cout << "\n=== FUNCTION OVERLOADS ===\n";

    std::string str = "Lvalue";

    processValue(str);  // Calls lvalue version
    processValue(std::string("Rvalue"));  // Calls rvalue version
    processValue("String Literal");  // Calls rvalue version
}

void demonstrateContainerOptimizations() {
    std::cout << "\n=== CONTAINER OPTIMIZATIONS ===\n";

    std::vector<MyClass> vec;

    std::cout << "--- push_back with lvalue ---\n";
    MyClass obj("Lvalue Object");
    vec.push_back(obj);  // Copy constructor called

    std::cout << "\n--- push_back with rvalue ---\n";
    vec.push_back(MyClass("Rvalue Object"));  // Move constructor called

    std::cout << "\n--- emplace_back ---\n";
    vec.emplace_back("Emplaced Object");  // Constructed in-place

    std::cout << "\nVector size: " << vec.size() << "\n";
}

void demonstrateReferenceCollapsing() {
    std::cout << "\n=== REFERENCE COLLAPSING ===\n";

    // Reference collapsing rules:
    // T& & → T&
    // T& && → T&
    // T&& & → T&
    // T&& && → T&&

    std::string str = "Test";

    // These demonstrate the rules in template contexts
    auto&& ref1 = str;        // T&& & → T& (str is lvalue)
    auto&& ref2 = std::move(str);  // T&& && → T&& (std::move returns rvalue)

    std::cout << "ref1 type deduced as lvalue reference\n";
    std::cout << "ref2 type deduced as rvalue reference\n";
}

void demonstrateUseCases() {
    std::cout << "\n=== PRACTICAL USE CASES ===\n";

    // 1. Resource management with move semantics
    std::cout << "1. Resource Management:\n";
    std::vector<std::unique_ptr<int>> ptrs;
    ptrs.push_back(std::make_unique<int>(42));  // Move semantics for unique_ptr

    // 2. String concatenation optimization
    std::cout << "\n2. String Optimization:\n";
    std::string result = std::string("Hello") + " " + "World";  // Efficient with move
    std::cout << "Result: " << result << "\n";

    // 3. Return value optimization
    std::cout << "\n3. Return Value Optimization:\n";
    auto obj = createObject("RVO Example");

    // 4. Perfect forwarding in wrapper functions
    std::cout << "\n4. Wrapper Function:\n";
    auto makeObject = [](auto&& arg) {
        return std::make_unique<MyClass>(std::forward<decltype(arg)>(arg));
    };

    auto ptr1 = makeObject(std::string("Forwarded"));
    std::string str = "Another";
    auto ptr2 = makeObject(str);
}

// ================================
// MAIN FUNCTION
// ================================

int main() {
    std::cout << "C++ LVALUE AND RVALUE REFERENCES DEMONSTRATION\n";
    std::cout << "================================================\n";

    demonstrateLValueReferences();
    demonstrateRValueReferences();
    demonstrateMoveSemantics();
    demonstratePerfectForwarding();
    demonstrateFunctionOverloads();
    demonstrateContainerOptimizations();
    demonstrateReferenceCollapsing();
    demonstrateUseCases();

    std::cout << "\n=== END OF DEMONSTRATION ===\n";
    return 0;
}

/*
KEY CONCEPTS SUMMARY:

1. LVALUE REFERENCES (&):
   - Bind to objects with identifiable memory locations
   - Allow modification of the original object
   - Cannot bind to temporaries (except const lvalue references)
   - Used for: aliasing, function parameters, avoiding copies

2. RVALUE REFERENCES (&&):
   - Bind to temporaries and objects marked with std::move
   - Enable move semantics and perfect forwarding
   - Allow "stealing" resources from temporaries
   - Used for: move constructors/assignment, perfect forwarding

3. MOVE SEMANTICS:
   - Transfer ownership instead of copying
   - Significantly improves performance for expensive-to-copy objects
   - Automatic for temporaries, explicit with std::move for lvalues

4. PERFECT FORWARDING:
   - Preserve the value category (lvalue/rvalue) when forwarding
   - Use std::forward<T> in template functions
   - Enables efficient wrapper functions

5. REFERENCE COLLAPSING:
   - T& & → T&, T& && → T&, T&& & → T&, T&& && → T&&
   - Makes perfect forwarding possible in templates

6. PRACTICAL BENEFITS:
   - Reduced unnecessary copying
   - Better performance for containers
   - Efficient resource management
   - Cleaner API design
*/
