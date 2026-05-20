/*
 * C++20 Modules Use Cases and Examples
 *
 * Modules provide better compilation performance, better encapsulation,
 * and eliminate issues with header guards and ODR violations.
 *
 * Key Benefits:
 * 1. Faster compilation (no repeated parsing of headers)
 * 2. Better encapsulation (only export what you want)
 * 3. No macro pollution
 * 4. Stronger ODR enforcement
 * 5. Better dependency management
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <numeric>

// ============================================================================
// 1. BASIC MODULE INTERFACE UNIT (.cppm or .ixx file)
// ============================================================================

/*
// File: math_utils.cppm
export module math_utils;

export namespace math {
    int add(int a, int b) {
        return a + b;
    }

    int multiply(int a, int b) {
        return a * b;
    }

    // Private function - not exported
    int internal_helper(int x) {
        return x * 2;
    }

    export int square(int x) {
        return multiply(x, x);
    }
}
*/

// ============================================================================
// 2. MODULE IMPLEMENTATION UNIT (.cpp file)
// ============================================================================

/*
// File: math_utils_impl.cpp
module math_utils;

#include <cmath>

namespace math {
    // Implementation of exported functions can be in separate file
    export double sqrt_custom(double x) {
        return std::sqrt(x);
    }

    // Private implementation
    double internal_calculation(double a, double b) {
        return a * b + internal_helper(static_cast<int>(a));
    }
}
*/

// ============================================================================
// 3. IMPORTING AND USING MODULES
// ============================================================================

void demonstrate_basic_module_usage() {
    std::cout << "\n=== Basic Module Usage ===\n";

    /*
    // In a consuming file:
    import math_utils;

    int main() {
        int result = math::add(5, 3);
        int squared = math::square(4);

        // This would cause compilation error - not exported:
        // int helper = math::internal_helper(5); // ERROR

        return 0;
    }
    */

    std::cout << "Module usage would be: math::add(5, 3) = 8\n";
    std::cout << "Module usage would be: math::square(4) = 16\n";
}

// ============================================================================
// 4. ADVANCED MODULE WITH CLASSES AND TEMPLATES
// ============================================================================

/*
// File: data_structures.cppm
export module data_structures;

import <vector>;
import <memory>;
import <algorithm>;

export template<typename T>
class Container {
private:
    std::vector<T> data;

public:
    void add(const T& item) {
        data.push_back(item);
    }

    void add(T&& item) {
        data.push_back(std::move(item));
    }

    template<typename Predicate>
    auto find_if(Predicate pred) -> decltype(auto) {
        return std::find_if(data.begin(), data.end(), pred);
    }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }
};

export class ResourceManager {
private:
    std::vector<std::unique_ptr<int>> resources;

public:
    void add_resource(int value) {
        resources.push_back(std::make_unique<int>(value));
    }

    size_t resource_count() const {
        return resources.size();
    }

    int total_value() const {
        int sum = 0;
        for (const auto& res : resources) {
            sum += *res;
        }
        return sum;
    }
};
*/

void demonstrate_advanced_module_usage() {
    std::cout << "\n=== Advanced Module Usage (Simulated) ===\n";

    // Simulating the Container class usage
    std::vector<int> container;
    container.push_back(1);
    container.push_back(2);
    container.push_back(3);

    std::cout << "Container size: " << container.size() << "\n";

    auto it = std::find_if(container.begin(), container.end(),
                          [](int x) { return x > 2; });
    if (it != container.end()) {
        std::cout << "Found element > 2: " << *it << "\n";
    }

    // Simulating ResourceManager
    std::vector<std::unique_ptr<int>> resources;
    resources.push_back(std::make_unique<int>(10));
    resources.push_back(std::make_unique<int>(20));
    resources.push_back(std::make_unique<int>(30));

    int total = std::accumulate(resources.begin(), resources.end(), 0,
                               [](int sum, const auto& ptr) {
                                   return sum + *ptr;
                               });
    std::cout << "Total resource value: " << total << "\n";
}

// ============================================================================
// 5. MODULE PARTITIONS
// ============================================================================

/*
// File: graphics.cppm (Primary module interface)
export module graphics;

export import :shapes;    // Import and re-export partition
export import :colors;    // Import and re-export partition
import :internal_utils;   // Import but don't re-export

export namespace graphics {
    void initialize_graphics_system() {
        // Use internal utilities
        setup_internal_systems();

        // Initialize shapes and colors subsystems
        shapes::initialize();
        colors::initialize();
    }
}

// File: graphics-shapes.cppm (Module partition)
export module graphics:shapes;

import <vector>;
import <memory>;

export namespace graphics::shapes {
    class Shape {
    public:
        virtual ~Shape() = default;
        virtual double area() const = 0;
        virtual void draw() const = 0;
    };

    class Rectangle : public Shape {
    private:
        double width, height;

    public:
        Rectangle(double w, double h) : width(w), height(h) {}

        double area() const override {
            return width * height;
        }

        void draw() const override {
            // Drawing implementation
        }
    };

    class Circle : public Shape {
    private:
        double radius;

    public:
        Circle(double r) : radius(r) {}

        double area() const override {
            return 3.14159 * radius * radius;
        }

        void draw() const override {
            // Drawing implementation
        }
    };

    void initialize() {
        // Initialize shapes subsystem
    }
}

// File: graphics-colors.cppm (Module partition)
export module graphics:colors;

export namespace graphics::colors {
    struct RGB {
        unsigned char r, g, b;

        RGB(unsigned char red, unsigned char green, unsigned char blue)
            : r(red), g(green), b(blue) {}
    };

    struct HSV {
        float h, s, v;

        HSV(float hue, float saturation, float value)
            : h(hue), s(saturation), v(value) {}
    };

    RGB hsv_to_rgb(const HSV& hsv) {
        // Conversion implementation
        return RGB(255, 255, 255);
    }

    HSV rgb_to_hsv(const RGB& rgb) {
        // Conversion implementation
        return HSV(0.0f, 0.0f, 1.0f);
    }

    void initialize() {
        // Initialize color subsystem
    }
}

// File: graphics-internal.cppm (Internal partition - not exported)
module graphics:internal_utils;

void setup_internal_systems() {
    // Internal setup that shouldn't be visible outside
}
*/

void demonstrate_module_partitions() {
    std::cout << "\n=== Module Partitions Usage (Simulated) ===\n";

    /*
    // Usage:
    import graphics;  // Gets shapes and colors, but not internal_utils

    auto rect = graphics::shapes::Rectangle(10.0, 5.0);
    auto circle = graphics::shapes::Circle(3.0);

    auto red = graphics::colors::RGB(255, 0, 0);
    auto hsv = graphics::colors::rgb_to_hsv(red);
    */

    std::cout << "Rectangle area would be: " << (10.0 * 5.0) << "\n";
    std::cout << "Circle area would be: " << (3.14159 * 3.0 * 3.0) << "\n";
    std::cout << "RGB to HSV conversion would be available\n";
}

// ============================================================================
// 6. HEADER UNITS (Legacy Header Integration)
// ============================================================================

/*
// You can import traditional headers as header units:
import <iostream>;  // Standard library header
import <vector>;    // Standard library header
import "legacy.h";  // Legacy header as header unit

// This provides:
// 1. Better compilation performance than #include
// 2. Isolation from macro pollution
// 3. Order independence
*/

void demonstrate_header_units() {
    std::cout << "\n=== Header Units Usage ===\n";
    std::cout << "Header units allow importing traditional headers\n";
    std::cout << "Examples: import <iostream>; import <vector>;\n";
    std::cout << "Benefits: Better performance, macro isolation\n";
}

// ============================================================================
// 7. GLOBAL MODULE FRAGMENT
// ============================================================================

/*
// File: platform_specific.cppm
module;  // Global module fragment

// Legacy includes that need preprocessing
#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
#endif

// Macro definitions
#define PLATFORM_SPECIFIC_MACRO 42

export module platform_specific;

export namespace platform {
    void do_platform_specific_work() {
        #ifdef _WIN32
            // Windows-specific code
        #elif defined(__linux__)
            // Linux-specific code
        #endif
    }

    constexpr int get_magic_number() {
        return PLATFORM_SPECIFIC_MACRO;
    }
}
*/

void demonstrate_global_module_fragment() {
    std::cout << "\n=== Global Module Fragment Usage ===\n";
    std::cout << "Global module fragment allows legacy preprocessing\n";
    std::cout << "Useful for platform-specific includes and macros\n";
    std::cout << "Magic number would be: 42\n";
}

// ============================================================================
// 8. MODULE LINKAGE AND VISIBILITY
// ============================================================================

/*
// File: visibility_example.cppm
export module visibility_example;

// 1. Module linkage (visible only within this module)
static int module_static_var = 100;

namespace {
    int anonymous_namespace_var = 200;
}

// 2. Internal linkage within module
namespace internal {
    int internal_var = 300;

    void internal_function() {
        // Only visible within this module
    }
}

// 3. Exported entities (external linkage)
export namespace api {
    int public_var = 400;

    void public_function() {
        // Visible to importers
        internal_function();  // Can call internal functions
    }

    class PublicClass {
    private:
        int private_member = internal_var;  // Can access internal vars

    public:
        int get_value() const {
            return private_member + module_static_var;
        }
    };
}

// 4. Selective export
namespace hidden {
    void secret_function() {
        // Not exported - invisible to importers
    }

    export void exposed_function() {
        // Exported from within namespace
        secret_function();  // Can call non-exported functions
    }
}
*/

void demonstrate_module_linkage() {
    std::cout << "\n=== Module Linkage and Visibility ===\n";
    std::cout << "Module-static variables: Only visible within module\n";
    std::cout << "Exported functions: Visible to importers\n";
    std::cout << "Internal functions: Hidden from importers\n";
    std::cout << "Selective export: Can export specific items from namespaces\n";
}

// ============================================================================
// 9. TEMPLATE MODULES
// ============================================================================

/*
// File: algorithms.cppm
export module algorithms;

import <vector>;
import <algorithm>;
import <functional>;
import <type_traits>;

export template<typename Container, typename Predicate>
auto filter(const Container& container, Predicate pred) {
    using ValueType = typename Container::value_type;
    std::vector<ValueType> result;

    std::copy_if(container.begin(), container.end(),
                 std::back_inserter(result), pred);

    return result;
}

export template<typename Container, typename Transform>
auto map(const Container& container, Transform transform) {
    using InputType = typename Container::value_type;
    using OutputType = decltype(transform(*container.begin()));

    std::vector<OutputType> result;
    result.reserve(container.size());

    std::transform(container.begin(), container.end(),
                   std::back_inserter(result), transform);

    return result;
}

export template<typename Container, typename BinaryOp>
auto reduce(const Container& container, BinaryOp op) {
    static_assert(!container.empty(), "Cannot reduce empty container");

    auto it = container.begin();
    auto result = *it++;

    return std::accumulate(it, container.end(), result, op);
}

// Concept-based template (C++20)
export template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

export template<Numeric T>
class Statistics {
private:
    std::vector<T> data;

public:
    void add(T value) {
        data.push_back(value);
    }

    T mean() const {
        if (data.empty()) return T{};
        return reduce(data, std::plus<T>{}) / static_cast<T>(data.size());
    }

    T min() const {
        return *std::min_element(data.begin(), data.end());
    }

    T max() const {
        return *std::max_element(data.begin(), data.end());
    }
};
*/

void demonstrate_template_modules() {
    std::cout << "\n=== Template Modules Usage (Simulated) ===\n";

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Simulate filter operation
    std::vector<int> evens;
    std::copy_if(numbers.begin(), numbers.end(), std::back_inserter(evens),
                 [](int x) { return x % 2 == 0; });
    std::cout << "Filtered evens count: " << evens.size() << "\n";

    // Simulate map operation
    std::vector<int> squared;
    std::transform(numbers.begin(), numbers.end(), std::back_inserter(squared),
                   [](int x) { return x * x; });
    std::cout << "First squared value: " << squared[0] << "\n";

    // Simulate reduce operation
    int sum = std::accumulate(numbers.begin(), numbers.end(), 0);
    std::cout << "Sum: " << sum << "\n";

    // Simulate Statistics class
    double mean = static_cast<double>(sum) / numbers.size();
    std::cout << "Mean: " << mean << "\n";
}

// ============================================================================
// 10. MODULE COMPILATION MODEL
// ============================================================================

void demonstrate_compilation_model() {
    std::cout << "\n=== Module Compilation Model ===\n";
    std::cout << "1. Module Interface Units (.cppm/.ixx) compiled first\n";
    std::cout << "2. Binary Module Interface (BMI) files generated\n";
    std::cout << "3. Module Implementation Units compiled using BMI\n";
    std::cout << "4. Consumers import BMI, not source code\n";
    std::cout << "5. Parallel compilation of independent modules possible\n";
    std::cout << "6. Faster incremental builds\n";
}

// ============================================================================
// 11. MIGRATION STRATEGIES
// ============================================================================

void demonstrate_migration_strategies() {
    std::cout << "\n=== Migration from Headers to Modules ===\n";
    std::cout << "1. Start with leaf dependencies (no dependencies)\n";
    std::cout << "2. Convert headers to module interface units\n";
    std::cout << "3. Use header units for legacy code integration\n";
    std::cout << "4. Gradually convert consumers to import statements\n";
    std::cout << "5. Remove redundant header includes\n";
    std::cout << "6. Take advantage of better encapsulation\n";
}

// ============================================================================
// 12. BEST PRACTICES AND GUIDELINES
// ============================================================================

void demonstrate_best_practices() {
    std::cout << "\n=== Module Best Practices ===\n";
    std::cout << "1. Keep module interfaces minimal and stable\n";
    std::cout << "2. Use partitions for large modules\n";
    std::cout << "3. Avoid exporting implementation details\n";
    std::cout << "4. Use meaningful module names (avoid conflicts)\n";
    std::cout << "5. Document module dependencies clearly\n";
    std::cout << "6. Consider ABI stability when designing interfaces\n";
    std::cout << "7. Use concepts for template constraints\n";
    std::cout << "8. Prefer composition over large monolithic modules\n";
}

// ============================================================================
// 13. PERFORMANCE COMPARISONS
// ============================================================================

void demonstrate_performance_benefits() {
    std::cout << "\n=== Module Performance Benefits ===\n";
    std::cout << "Traditional Headers:\n";
    std::cout << "- Each #include processes entire header\n";
    std::cout << "- Repeated parsing of same headers\n";
    std::cout << "- Macro processing overhead\n";
    std::cout << "- ODR violations possible\n";

    std::cout << "\nModules:\n";
    std::cout << "- Binary module interface (BMI) - pre-compiled\n";
    std::cout << "- No repeated parsing\n";
    std::cout << "- No macro pollution\n";
    std::cout << "- Strong ODR enforcement\n";
    std::cout << "- Typically 2-10x faster compilation\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 Modules Use Cases and Examples\n";
    std::cout << "====================================\n";

    demonstrate_basic_module_usage();
    demonstrate_advanced_module_usage();
    demonstrate_module_partitions();
    demonstrate_header_units();
    demonstrate_global_module_fragment();
    demonstrate_module_linkage();
    demonstrate_template_modules();
    demonstrate_compilation_model();
    demonstrate_migration_strategies();
    demonstrate_best_practices();
    demonstrate_performance_benefits();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Modules provide better encapsulation than headers\n";
    std::cout << "2. Significant compilation performance improvements\n";
    std::cout << "3. Eliminates macro pollution and ODR issues\n";
    std::cout << "4. Better dependency management\n";
    std::cout << "5. Gradual migration path from headers\n";
    std::cout << "6. Template and concept support\n";
    std::cout << "7. Module partitions for organization\n";
    std::cout << "8. Compatible with existing code via header units\n";

    return 0;
}

/*
 * Compilation Commands (GCC 11+ or Clang 12+ with modules support):
 *
 * For GCC:
 * g++ -std=c++20 -fmodules-ts module_file.cppm -c
 * g++ -std=c++20 -fmodules-ts main.cpp module_file.o -o program
 *
 * For Clang:
 * clang++ -std=c++20 -stdlib=libc++ --precompile module_file.cppm -o module_file.pcm
 * clang++ -std=c++20 -stdlib=libc++ -fmodule-file=module_file.pcm main.cpp -o program
 *
 * For MSVC:
 * cl /std:c++20 /experimental:module module_file.ixx /c
 * cl /std:c++20 /experimental:module main.cpp module_file.obj
 *
 * Note: Module support varies by compiler and version.
 * Some examples are conceptual as full module support is still evolving.
 */
