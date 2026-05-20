#include <iostream>
#include <vector>
#include <string>
#include <typeinfo>
#include <type_traits>
#include <memory>
#include <functional>

/*
 * Comprehensive examples of decltype, sizeof, noexcept, and typeid
 * Use cases and practical applications in modern C++
 */

// =============================================================================
// 1. DECLTYPE USE CASES AND EXAMPLES
// =============================================================================

namespace decltype_examples {

    // Basic decltype usage
    void basic_decltype() {
        std::cout << "\n=== DECLTYPE BASIC EXAMPLES ===\n";

        int x = 42;
        double y = 3.14;
        std::string str = "hello";

        // decltype deduces the type of the expression
        decltype(x) a = 10;        // int a = 10;
        decltype(y) b = 2.71;      // double b = 2.71;
        decltype(str) c = "world"; // std::string c = "world";

        std::cout << "decltype(x) a = " << a << " (type: int)\n";
        std::cout << "decltype(y) b = " << b << " (type: double)\n";
        std::cout << "decltype(str) c = " << c << " (type: std::string)\n";
    }

    // decltype with references and expressions
    void decltype_with_references() {
        std::cout << "\n=== DECLTYPE WITH REFERENCES ===\n";

        int x = 42;
        int& ref = x;
        const int& cref = x;

        decltype(x) a = 1;     // int (value)
        decltype(ref) b = x;   // int& (reference)
        decltype(cref) c = x;  // const int& (const reference)
        decltype((x)) d = x;   // int& (parentheses make it lvalue reference)

        std::cout << "decltype preserves reference types and cv-qualifiers\n";
        std::cout << "decltype(x): int\n";
        std::cout << "decltype(ref): int&\n";
        std::cout << "decltype(cref): const int&\n";
        std::cout << "decltype((x)): int& (parentheses matter!)\n";
    }

    // decltype in template functions
    template<typename T, typename U>
    auto add(T t, U u) -> decltype(t + u) {
        return t + u;
    }

    // C++14 style with auto return type
    template<typename T, typename U>
    auto multiply(T t, U u) {
        return t * u;  // decltype is implicit here
    }

    // decltype for perfect forwarding
    template<typename F, typename... Args>
    auto call_function(F&& f, Args&&... args) -> decltype(f(std::forward<Args>(args)...)) {
        return f(std::forward<Args>(args)...);
    }

    void decltype_in_templates() {
        std::cout << "\n=== DECLTYPE IN TEMPLATES ===\n";

        auto result1 = add(5, 3.14);  // double
        auto result2 = add(10, 20);   // int
        auto result3 = multiply(2.5, 4); // double

        std::cout << "add(5, 3.14) = " << result1 << " (type deduced by decltype)\n";
        std::cout << "add(10, 20) = " << result2 << "\n";
        std::cout << "multiply(2.5, 4) = " << result3 << "\n";
    }

    // decltype with member variables and functions
    class MyClass {
    public:
        int value = 42;
        double getValue() const { return value * 1.5; }
        static std::string staticMember;
    };

    std::string MyClass::staticMember = "static";

    void decltype_with_members() {
        std::cout << "\n=== DECLTYPE WITH CLASS MEMBERS ===\n";

        MyClass obj;

        decltype(obj.value) x = 100;                    // int
        decltype(obj.getValue()) y = 3.14;              // double
        decltype(MyClass::staticMember) z = "hello";    // std::string

        // Member function pointer
        decltype(&MyClass::getValue) func_ptr = &MyClass::getValue;

        std::cout << "decltype works with member variables and functions\n";
        std::cout << "Member function result: " << (obj.*func_ptr)() << "\n";
    }

    // decltype vs auto
    void decltype_vs_auto() {
        std::cout << "\n=== DECLTYPE VS AUTO ===\n";

        const int x = 42;
        const int& ref = x;

        auto a = ref;        // int (auto strips references and cv-qualifiers)
        decltype(ref) b = x; // const int& (decltype preserves everything)
        decltype(auto) c = ref; // const int& (C++14 feature)

        std::cout << "auto strips references/cv-qualifiers, decltype preserves them\n";
        std::cout << "decltype(auto) combines benefits of both\n";
    }
}

// =============================================================================
// 2. SIZEOF USE CASES AND EXAMPLES
// =============================================================================

namespace sizeof_examples {

    void basic_sizeof() {
        std::cout << "\n=== SIZEOF BASIC EXAMPLES ===\n";

        // Basic types
        std::cout << "sizeof(char): " << sizeof(char) << " byte\n";
        std::cout << "sizeof(int): " << sizeof(int) << " bytes\n";
        std::cout << "sizeof(long): " << sizeof(long) << " bytes\n";
        std::cout << "sizeof(double): " << sizeof(double) << " bytes\n";
        std::cout << "sizeof(void*): " << sizeof(void*) << " bytes\n";

        // Arrays
        int arr[10];
        std::cout << "sizeof(int[10]): " << sizeof(arr) << " bytes\n";
        std::cout << "sizeof(int) * 10 = " << sizeof(int) * 10 << " bytes\n";
    }

    struct MyStruct {
        char c;      // 1 byte
        int i;       // 4 bytes
        double d;    // 8 bytes
        // Padding may be added for alignment
    };

    class MyClass {
        int a, b, c;
        virtual void func() {} // vtable pointer added
    };

    void sizeof_with_structures() {
        std::cout << "\n=== SIZEOF WITH STRUCTURES/CLASSES ===\n";

        std::cout << "sizeof(MyStruct): " << sizeof(MyStruct) << " bytes\n";
        std::cout << "sizeof(MyClass): " << sizeof(MyClass) << " bytes (includes vtable)\n";

        // Array of structures
        MyStruct arr[5];
        std::cout << "sizeof(MyStruct[5]): " << sizeof(arr) << " bytes\n";

        // Empty class/struct
        struct Empty {};
        std::cout << "sizeof(Empty): " << sizeof(Empty) << " byte (never 0)\n";
    }

    template<typename T>
    void print_container_info(const T& container) {
        std::cout << "Container size: " << container.size() << " elements\n";
        std::cout << "sizeof(container): " << sizeof(container) << " bytes\n";
        std::cout << "sizeof(element): " << sizeof(typename T::value_type) << " bytes\n";
        std::cout << "Memory used by elements: " <<
                     container.size() * sizeof(typename T::value_type) << " bytes\n";
    }

    void sizeof_with_containers() {
        std::cout << "\n=== SIZEOF WITH STL CONTAINERS ===\n";

        std::vector<int> vec = {1, 2, 3, 4, 5};
        std::string str = "Hello, World!";

        std::cout << "Vector info:\n";
        print_container_info(vec);

        std::cout << "\nString info:\n";
        print_container_info(str);

        std::cout << "\nNote: sizeof() returns compile-time size, not runtime capacity!\n";
    }

    // sizeof in template metaprogramming
    template<typename T>
    constexpr bool is_small_type() {
        return sizeof(T) <= sizeof(void*);
    }

    template<typename T>
    void optimize_based_on_size() {
        if constexpr (is_small_type<T>()) {
            std::cout << "Type " << typeid(T).name() << " is small, pass by value\n";
        } else {
            std::cout << "Type " << typeid(T).name() << " is large, pass by reference\n";
        }
    }

    void sizeof_in_templates() {
        std::cout << "\n=== SIZEOF IN TEMPLATE METAPROGRAMMING ===\n";

        optimize_based_on_size<int>();
        optimize_based_on_size<std::string>();
        optimize_based_on_size<std::vector<int>>();
    }

    // sizeof... for parameter packs (C++11)
    template<typename... Args>
    void count_arguments(Args... args) {
        std::cout << "Number of arguments: " << sizeof...(args) << "\n";
        std::cout << "Number of types: " << sizeof...(Args) << "\n";
    }

    void sizeof_parameter_pack() {
        std::cout << "\n=== SIZEOF WITH PARAMETER PACKS ===\n";

        count_arguments(1, 2.5, "hello", 'c');
        count_arguments();
        count_arguments(42);
    }
}

// =============================================================================
// 3. NOEXCEPT USE CASES AND EXAMPLES
// =============================================================================

namespace noexcept_examples {

    // Basic noexcept usage
    int safe_function() noexcept {
        return 42;  // Guaranteed not to throw
    }

    int might_throw() {
        if (rand() % 2) {
            throw std::runtime_error("Random error");
        }
        return 100;
    }

    // Conditional noexcept
    template<typename T>
    void swap_values(T& a, T& b) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                         std::is_nothrow_move_assignable_v<T>) {
        T temp = std::move(a);
        a = std::move(b);
        b = std::move(temp);
    }

    void basic_noexcept() {
        std::cout << "\n=== NOEXCEPT BASIC EXAMPLES ===\n";

        std::cout << "safe_function() is noexcept: " << std::boolalpha <<
                     noexcept(safe_function()) << "\n";
        std::cout << "might_throw() is noexcept: " <<
                     noexcept(might_throw()) << "\n";

        int x = 10, y = 20;
        std::cout << "swap_values(int) is noexcept: " <<
                     noexcept(swap_values(x, y)) << "\n";

        std::string s1 = "hello", s2 = "world";
        std::cout << "swap_values(string) is noexcept: " <<
                     noexcept(swap_values(s1, s2)) << "\n";
    }

    // noexcept with move semantics
    class MovableClass {
        std::vector<int> data;
    public:
        MovableClass(size_t size) : data(size) {}

        // Move constructor
        MovableClass(MovableClass&& other) noexcept : data(std::move(other.data)) {}

        // Move assignment
        MovableClass& operator=(MovableClass&& other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
            }
            return *this;
        }

        // Copy operations (not noexcept - might throw due to allocation)
        MovableClass(const MovableClass& other) : data(other.data) {}
        MovableClass& operator=(const MovableClass& other) {
            if (this != &other) {
                data = other.data;
            }
            return *this;
        }
    };

    void noexcept_with_move_semantics() {
        std::cout << "\n=== NOEXCEPT WITH MOVE SEMANTICS ===\n";

        std::cout << "MovableClass move constructor is noexcept: " <<
                     std::is_nothrow_move_constructible_v<MovableClass> << "\n";
        std::cout << "MovableClass move assignment is noexcept: " <<
                     std::is_nothrow_move_assignable_v<MovableClass> << "\n";
        std::cout << "MovableClass copy constructor is noexcept: " <<
                     std::is_nothrow_copy_constructible_v<MovableClass> << "\n";

        // This enables optimizations in std::vector and other containers
        std::vector<MovableClass> vec;
        vec.emplace_back(1000);  // Will use move semantics efficiently
        std::cout << "std::vector can use move semantics efficiently\n";
    }

    // noexcept with destructors (implicitly noexcept)
    class ResourceManager {
        std::unique_ptr<int[]> resource;
    public:
        ResourceManager(size_t size) : resource(std::make_unique<int[]>(size)) {}

        // Destructor is implicitly noexcept
        ~ResourceManager() = default;

        // Explicitly noexcept destructor
        // ~ResourceManager() noexcept = default;
    };

    void noexcept_with_destructors() {
        std::cout << "\n=== NOEXCEPT WITH DESTRUCTORS ===\n";

        std::cout << "ResourceManager destructor is noexcept: " <<
                     std::is_nothrow_destructible_v<ResourceManager> << "\n";

        std::cout << "Destructors are implicitly noexcept in C++11 and later\n";
        std::cout << "This is crucial for exception safety\n";
    }

    // noexcept operator vs noexcept specifier
    template<typename T>
    void demonstrate_noexcept_operator() {
        std::cout << "\n=== NOEXCEPT OPERATOR EXAMPLES ===\n";

        T obj{};

        std::cout << "Type: " << typeid(T).name() << "\n";
        std::cout << "Default constructor noexcept: " <<
                     noexcept(T()) << "\n";
        std::cout << "Copy constructor noexcept: " <<
                     noexcept(T(obj)) << "\n";
        std::cout << "Move constructor noexcept: " <<
                     noexcept(T(std::move(obj))) << "\n";
        std::cout << "Destructor noexcept: " <<
                     noexcept(obj.~T()) << "\n";
    }

    void noexcept_operator_examples() {
        demonstrate_noexcept_operator<int>();
        demonstrate_noexcept_operator<std::string>();
        demonstrate_noexcept_operator<std::vector<int>>();
    }
}

// =============================================================================
// 4. TYPEID USE CASES AND EXAMPLES
// =============================================================================

namespace typeid_examples {

    void basic_typeid() {
        std::cout << "\n=== TYPEID BASIC EXAMPLES ===\n";

        int x = 42;
        double y = 3.14;
        std::string str = "hello";

        std::cout << "typeid(x).name(): " << typeid(x).name() << "\n";
        std::cout << "typeid(y).name(): " << typeid(y).name() << "\n";
        std::cout << "typeid(str).name(): " << typeid(str).name() << "\n";
        std::cout << "typeid(int).name(): " << typeid(int).name() << "\n";

        // Type comparison
        std::cout << "typeid(x) == typeid(int): " <<
                     (typeid(x) == typeid(int)) << "\n";
        std::cout << "typeid(y) == typeid(double): " <<
                     (typeid(y) == typeid(double)) << "\n";
    }

    // typeid with polymorphism
    class Base {
    public:
        virtual ~Base() = default;
        virtual void print() const { std::cout << "Base"; }
    };

    class Derived : public Base {
    public:
        void print() const override { std::cout << "Derived"; }
    };

    class AnotherDerived : public Base {
    public:
        void print() const override { std::cout << "AnotherDerived"; }
    };

    void typeid_with_polymorphism() {
        std::cout << "\n=== TYPEID WITH POLYMORPHISM ===\n";

        Base base;
        Derived derived;
        AnotherDerived another;

        Base* ptr1 = &base;
        Base* ptr2 = &derived;
        Base* ptr3 = &another;

        std::cout << "Static type vs Dynamic type:\n";
        std::cout << "ptr1 static type: " << typeid(ptr1).name() << "\n";
        std::cout << "ptr1 dynamic type: " << typeid(*ptr1).name() << "\n";

        std::cout << "ptr2 static type: " << typeid(ptr2).name() << "\n";
        std::cout << "ptr2 dynamic type: " << typeid(*ptr2).name() << "\n";

        std::cout << "ptr3 static type: " << typeid(ptr3).name() << "\n";
        std::cout << "ptr3 dynamic type: " << typeid(*ptr3).name() << "\n";

        // Runtime type checking
        if (typeid(*ptr2) == typeid(Derived)) {
            std::cout << "ptr2 points to Derived object\n";
        }
    }

    // typeid in templates
    template<typename T>
    void print_type_info() {
        std::cout << "Template parameter T: " << typeid(T).name() << "\n";
        std::cout << "sizeof(T): " << sizeof(T) << " bytes\n";
        std::cout << "T is integral: " << std::is_integral_v<T> << "\n";
        std::cout << "T is floating point: " << std::is_floating_point_v<T> << "\n";
    }

    void typeid_in_templates() {
        std::cout << "\n=== TYPEID IN TEMPLATES ===\n";

        print_type_info<int>();
        std::cout << "\n";
        print_type_info<double>();
        std::cout << "\n";
        print_type_info<std::string>();
    }

    // Type-safe factory using typeid
    class ShapeFactory {
        std::unordered_map<std::type_index, std::function<std::unique_ptr<Base>()>> creators;

    public:
        template<typename T>
        void register_creator() {
            static_assert(std::is_base_of_v<Base, T>, "T must derive from Base");
            creators[std::type_index(typeid(T))] = []() {
                return std::make_unique<T>();
            };
        }

        template<typename T>
        std::unique_ptr<Base> create() {
            auto it = creators.find(std::type_index(typeid(T)));
            if (it != creators.end()) {
                return it->second();
            }
            return nullptr;
        }
    };

    void typeid_factory_pattern() {
        std::cout << "\n=== TYPEID IN FACTORY PATTERN ===\n";

        ShapeFactory factory;
        factory.register_creator<Derived>();
        factory.register_creator<AnotherDerived>();

        auto obj1 = factory.create<Derived>();
        auto obj2 = factory.create<AnotherDerived>();

        if (obj1) {
            std::cout << "Created object of type: " << typeid(*obj1).name() << "\n";
            obj1->print();
            std::cout << "\n";
        }

        if (obj2) {
            std::cout << "Created object of type: " << typeid(*obj2).name() << "\n";
            obj2->print();
            std::cout << "\n";
        }
    }

    // typeid with references and cv-qualifiers
    void typeid_with_qualifiers() {
        std::cout << "\n=== TYPEID WITH CV-QUALIFIERS ===\n";

        int x = 42;
        const int cx = 42;
        volatile int vx = 42;
        const volatile int cvx = 42;

        int& ref = x;
        const int& cref = cx;

        std::cout << "typeid strips cv-qualifiers and references:\n";
        std::cout << "typeid(int): " << typeid(int).name() << "\n";
        std::cout << "typeid(const int): " << typeid(const int).name() << "\n";
        std::cout << "typeid(volatile int): " << typeid(volatile int).name() << "\n";
        std::cout << "typeid(const volatile int): " << typeid(const volatile int).name() << "\n";
        std::cout << "typeid(int&): " << typeid(int&).name() << "\n";
        std::cout << "typeid(const int&): " << typeid(const int&).name() << "\n";

        std::cout << "All are equal: " <<
                     (typeid(int) == typeid(const int) &&
                      typeid(int) == typeid(volatile int) &&
                      typeid(int) == typeid(int&)) << "\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL EXAMPLES
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE EXAMPLES: decltype, sizeof, noexcept, typeid\n";
    std::cout << "=============================================================================\n";

    // DECLTYPE examples
    decltype_examples::basic_decltype();
    decltype_examples::decltype_with_references();
    decltype_examples::decltype_in_templates();
    decltype_examples::decltype_with_members();
    decltype_examples::decltype_vs_auto();

    // SIZEOF examples
    sizeof_examples::basic_sizeof();
    sizeof_examples::sizeof_with_structures();
    sizeof_examples::sizeof_with_containers();
    sizeof_examples::sizeof_in_templates();
    sizeof_examples::sizeof_parameter_pack();

    // NOEXCEPT examples
    noexcept_examples::basic_noexcept();
    noexcept_examples::noexcept_with_move_semantics();
    noexcept_examples::noexcept_with_destructors();
    noexcept_examples::noexcept_operator_examples();

    // TYPEID examples
    typeid_examples::basic_typeid();
    typeid_examples::typeid_with_polymorphism();
    typeid_examples::typeid_in_templates();
    typeid_examples::typeid_factory_pattern();
    typeid_examples::typeid_with_qualifiers();

    std::cout << "\n=============================================================================\n";
    std::cout << "KEY TAKEAWAYS:\n";
    std::cout << "1. decltype: Deduces types from expressions, preserves references/cv-qualifiers\n";
    std::cout << "2. sizeof: Compile-time size calculation, useful for optimization and metaprogramming\n";
    std::cout << "3. noexcept: Exception safety specification, enables optimizations\n";
    std::cout << "4. typeid: Runtime type identification, useful for polymorphism and type checking\n";
    std::cout << "=============================================================================\n";

    return 0;
}
