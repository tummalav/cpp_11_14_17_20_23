// =============================
// RANGE-BASED FOR LOOPS USE CASES (C++11+)
// =============================
/*
RANGE-BASED FOR LOOP RULES AND SYNTAX:

1. BASIC SYNTAX: for (declaration : range) statement
   - declaration: variable declaration (can use auto)
   - range: any expression that represents a sequence
   - statement: code to execute for each element

2. EQUIVALENT TRADITIONAL LOOP:
   {
       auto&& __range = range_expression;
       for (auto __begin = std::begin(__range), __end = std::end(__range);
            __begin != __end; ++__begin) {
           declaration = *__begin;
           statement
       }
   }

3. REQUIREMENTS FOR RANGE:
   - Container with begin()/end() methods, OR
   - C-style array, OR
   - Type with std::begin()/std::end() overloads

4. VALUE SEMANTICS:
   - for (auto item : container)        // Copy each element
   - for (auto& item : container)       // Reference to each element
   - for (const auto& item : container) // Const reference (read-only)
   - for (auto&& item : container)      // Universal reference (perfect forwarding)

5. C++17 STRUCTURED BINDINGS:
   - for (auto [key, value] : map)      // Decompose pairs/tuples
   - for (const auto& [k, v] : map)     // Const reference to decomposed elements

6. C++20 RANGES:
   - Enhanced with ranges library
   - for (auto item : std::views::filter(container, predicate))
*/

// =============================
// COMPREHENSIVE EXAMPLES
// =============================

#include <iostream>
#include <vector>
#include <array>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <tuple>
#include <utility>

// 1. BASIC RANGE-BASED FOR LOOP EXAMPLES
void basic_range_for_examples() {
    std::cout << "=== BASIC RANGE-BASED FOR LOOP EXAMPLES ===\n";

    // With std::vector
    std::vector<int> numbers = {1, 2, 3, 4, 5};

    std::cout << "Vector elements (copy semantics): ";
    for (auto num : numbers) {  // Copy each element
        num *= 2;  // Modifies copy, not original
        std::cout << num << " ";
    }
    std::cout << "\nOriginal vector: ";
    for (const auto& num : numbers) {  // Original unchanged
        std::cout << num << " ";
    }

    // With C-style array
    int arr[] = {10, 20, 30, 40, 50};
    std::cout << "\n\nC-style array: ";
    for (auto element : arr) {
        std::cout << element << " ";
    }

    // With std::string
    std::string text = "Hello";
    std::cout << "\n\nString characters: ";
    for (auto ch : text) {
        std::cout << ch << " ";
    }

    std::cout << "\n\n";
}

// 2. REFERENCE SEMANTICS (MODIFYING ELEMENTS)
void reference_semantics_examples() {
    std::cout << "=== REFERENCE SEMANTICS EXAMPLES ===\n";

    std::vector<std::string> words = {"hello", "world", "test"};

    std::cout << "Original words: ";
    for (const auto& word : words) {
        std::cout << word << " ";
    }

    // Modify elements using reference
    std::cout << "\n\nModifying with auto&: ";
    for (auto& word : words) {  // Reference allows modification
        word += "!";
        std::cout << word << " ";
    }

    // Verify modification
    std::cout << "\n\nAfter modification: ";
    for (const auto& word : words) {
        std::cout << word << " ";
    }

    // Working with numbers
    std::vector<int> values = {1, 2, 3, 4, 5};
    std::cout << "\n\nOriginal values: ";
    for (auto val : values) std::cout << val << " ";

    // Square all values
    for (auto& val : values) {
        val *= val;
    }

    std::cout << "\nSquared values: ";
    for (auto val : values) std::cout << val << " ";

    std::cout << "\n\n";
}

// 3. CONST CORRECTNESS AND READ-ONLY ACCESS
void const_correctness_examples() {
    std::cout << "=== CONST CORRECTNESS EXAMPLES ===\n";

    const std::vector<double> prices = {19.99, 29.99, 39.99, 49.99};

    // Reading from const container
    std::cout << "Prices (const container): ";
    for (const auto& price : prices) {  // Must use const auto& for const container
        std::cout << "$" << price << " ";
    }

    // Demonstrating different access patterns
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "Diana"};

    std::cout << "\n\nDifferent access patterns:\n";

    // Read-only access (preferred for large objects)
    std::cout << "Read-only (const auto&): ";
    for (const auto& name : names) {
        std::cout << name << " ";
        // name += "!";  // Error: cannot modify const reference
    }

    // Copy access (safe but potentially expensive)
    std::cout << "\nCopy access (auto): ";
    for (auto name : names) {
        name += "!";  // Modifies copy, original unchanged
        std::cout << name << " ";
    }

    // Reference access (allows modification)
    std::cout << "\nReference access (auto&): ";
    std::vector<std::string> temp_names = names;  // Work with copy
    for (auto& name : temp_names) {
        name += "?";  // Modifies original
        std::cout << name << " ";
    }

    std::cout << "\n\n";
}

// 4. CONTAINER-SPECIFIC EXAMPLES
void container_specific_examples() {
    std::cout << "=== CONTAINER-SPECIFIC EXAMPLES ===\n";

    // std::array
    std::array<int, 4> arr = {1, 4, 9, 16};
    std::cout << "std::array: ";
    for (const auto& elem : arr) {
        std::cout << elem << " ";
    }

    // std::list
    std::list<char> char_list = {'a', 'b', 'c', 'd'};
    std::cout << "\n\nstd::list: ";
    for (const auto& ch : char_list) {
        std::cout << ch << " ";
    }

    // std::set
    std::set<int> unique_numbers = {5, 2, 8, 2, 1, 8};  // Duplicates removed, sorted
    std::cout << "\n\nstd::set (sorted, unique): ";
    for (const auto& num : unique_numbers) {
        std::cout << num << " ";
    }

    // std::map
    std::map<std::string, int> ages = {
        {"Alice", 25}, {"Bob", 30}, {"Charlie", 35}
    };
    std::cout << "\n\nstd::map (key-value pairs):\n";
    for (const auto& pair : ages) {
        std::cout << pair.first << " is " << pair.second << " years old\n";
    }

    // std::unordered_map
    std::unordered_map<int, std::string> id_to_name = {
        {101, "John"}, {102, "Jane"}, {103, "Jim"}
    };
    std::cout << "\nstd::unordered_map:\n";
    for (const auto& entry : id_to_name) {
        std::cout << "ID " << entry.first << ": " << entry.second << "\n";
    }

    std::cout << "\n";
}

// 5. STRUCTURED BINDINGS (C++17+)
void structured_bindings_examples() {
    std::cout << "=== STRUCTURED BINDINGS EXAMPLES (C++17+) ===\n";

    // With std::map
    std::map<std::string, double> stock_prices = {
        {"AAPL", 150.25}, {"GOOGL", 2500.75}, {"MSFT", 300.50}
    };

    std::cout << "Stock prices with structured bindings:\n";
    for (const auto& [symbol, price] : stock_prices) {
        std::cout << symbol << ": $" << price << "\n";
    }

    // With std::pair vector
    std::vector<std::pair<int, std::string>> id_name_pairs = {
        {1, "Alice"}, {2, "Bob"}, {3, "Charlie"}
    };

    std::cout << "\nID-Name pairs:\n";
    for (const auto& [id, name] : id_name_pairs) {
        std::cout << "ID: " << id << ", Name: " << name << "\n";
    }

    // With std::tuple
    std::vector<std::tuple<std::string, int, double>> employees = {
        {"Alice", 25, 50000.0}, {"Bob", 30, 60000.0}, {"Charlie", 35, 70000.0}
    };

    std::cout << "\nEmployee records:\n";
    for (const auto& [name, age, salary] : employees) {
        std::cout << name << " (age " << age << "): $" << salary << "\n";
    }

    // Modifying with structured bindings
    std::map<int, std::string> mutable_map = {{1, "one"}, {2, "two"}, {3, "three"}};
    std::cout << "\nModifying map values:\n";
    for (auto& [key, value] : mutable_map) {
        value = "number_" + value;
        std::cout << key << ": " << value << "\n";
    }

    std::cout << "\n";
}

// 6. UNIVERSAL REFERENCES AND PERFECT FORWARDING
void universal_reference_examples() {
    std::cout << "=== UNIVERSAL REFERENCES EXAMPLES ===\n";

    // Universal reference with auto&&
    std::vector<std::string> strings = {"short", "medium_length", "very_long_string"};

    std::cout << "Using auto&& (universal reference):\n";
    for (auto&& str : strings) {
        std::cout << "String: " << str << " (length: " << str.length() << ")\n";
        // auto&& binds to the actual type (string& in this case)
    }

    // Demonstrating with different container types
    const std::vector<int> const_ints = {1, 2, 3};
    std::cout << "\nWith const container (auto&& -> const int&):\n";
    for (auto&& num : const_ints) {
        std::cout << num << " ";
        // num++;  // Error: cannot modify const reference
    }

    // With temporary container
    std::cout << "\n\nWith temporary container:\n";
    for (auto&& ch : std::string("temporary")) {
        std::cout << ch << " ";
    }

    std::cout << "\n\n";
}

// 7. NESTED CONTAINERS AND COMPLEX TYPES
void nested_containers_examples() {
    std::cout << "=== NESTED CONTAINERS EXAMPLES ===\n";

    // Vector of vectors
    std::vector<std::vector<int>> matrix = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    std::cout << "2D Matrix:\n";
    for (const auto& row : matrix) {
        for (const auto& element : row) {
            std::cout << element << " ";
        }
        std::cout << "\n";
    }

    // Map of vectors
    std::map<std::string, std::vector<int>> categories = {
        {"even", {2, 4, 6, 8}},
        {"odd", {1, 3, 5, 7}},
        {"prime", {2, 3, 5, 7}}
    };

    std::cout << "\nCategories:\n";
    for (const auto& [category, numbers] : categories) {
        std::cout << category << ": ";
        for (const auto& num : numbers) {
            std::cout << num << " ";
        }
        std::cout << "\n";
    }

    // Vector of pairs
    std::vector<std::pair<std::string, std::vector<double>>> data = {
        {"temperatures", {20.5, 22.1, 19.8, 23.4}},
        {"prices", {10.99, 15.50, 8.75}}
    };

    std::cout << "\nComplex data structures:\n";
    for (const auto& [label, values] : data) {
        std::cout << label << ": ";
        for (const auto& val : values) {
            std::cout << val << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\n";
}

// 8. CUSTOM CLASSES WITH RANGE-BASED FOR
class NumberSequence {
private:
    std::vector<int> numbers;

public:
    NumberSequence(std::initializer_list<int> init) : numbers(init) {}

    // Iterator support for range-based for
    auto begin() { return numbers.begin(); }
    auto end() { return numbers.end(); }
    auto begin() const { return numbers.begin(); }
    auto end() const { return numbers.end(); }

    // Additional methods
    void add(int num) { numbers.push_back(num); }
    size_t size() const { return numbers.size(); }
};

void custom_class_examples() {
    std::cout << "=== CUSTOM CLASS EXAMPLES ===\n";

    NumberSequence seq{10, 20, 30, 40, 50};

    std::cout << "Custom NumberSequence: ";
    for (const auto& num : seq) {
        std::cout << num << " ";
    }

    // Modify through range-based for
    std::cout << "\n\nDoubling values: ";
    for (auto& num : seq) {
        num *= 2;
        std::cout << num << " ";
    }

    std::cout << "\n\n";
}

// 9. ALGORITHMS AND RANGE-BASED FOR
void algorithms_with_range_for() {
    std::cout << "=== ALGORITHMS WITH RANGE-BASED FOR ===\n";

    std::vector<int> data = {5, 2, 8, 1, 9, 3};

    std::cout << "Original data: ";
    for (const auto& val : data) std::cout << val << " ";

    // Find maximum using range-based for
    int max_val = *data.begin();
    for (const auto& val : data) {
        if (val > max_val) max_val = val;
    }
    std::cout << "\nMaximum value: " << max_val;

    // Calculate sum
    int sum = 0;
    for (const auto& val : data) {
        sum += val;
    }
    std::cout << "\nSum: " << sum;

    // Count elements greater than 5
    int count = 0;
    for (const auto& val : data) {
        if (val > 5) count++;
    }
    std::cout << "\nElements > 5: " << count;

    // Transform elements (square them)
    std::cout << "\n\nSquared values: ";
    for (auto& val : data) {
        val *= val;
        std::cout << val << " ";
    }

    std::cout << "\n\n";
}

// 10. PERFORMANCE CONSIDERATIONS
void performance_considerations() {
    std::cout << "=== PERFORMANCE CONSIDERATIONS ===\n";

    std::vector<std::string> large_strings = {
        "This is a very long string that demonstrates performance implications",
        "Another long string to show the difference between copy and reference",
        "Yet another string to complete our performance example"
    };

    std::cout << "Performance comparison:\n\n";

    // EXPENSIVE: Copy semantics for large objects
    std::cout << "1. Copy semantics (auto) - EXPENSIVE for large objects:\n";
    for (auto str : large_strings) {  // Copies each string
        std::cout << "Length: " << str.length() << "\n";
    }

    // EFFICIENT: Reference semantics for read-only access
    std::cout << "\n2. Reference semantics (const auto&) - EFFICIENT:\n";
    for (const auto& str : large_strings) {  // No copy, read-only
        std::cout << "Length: " << str.length() << "\n";
    }

    // EFFICIENT: Reference semantics for modification
    std::cout << "\n3. Reference semantics (auto&) - EFFICIENT for modification:\n";
    std::vector<std::string> modifiable_strings = large_strings;
    for (auto& str : modifiable_strings) {  // No copy, allows modification
        str += " [MODIFIED]";
        std::cout << "New length: " << str.length() << "\n";
    }

    // Guidelines
    std::cout << "\nPERFORMANCE GUIDELINES:\n";
    std::cout << "- Use 'const auto&' for read-only access to avoid copies\n";
    std::cout << "- Use 'auto&' when you need to modify elements\n";
    std::cout << "- Use 'auto' only for small types or when you need a copy\n";
    std::cout << "- Use 'auto&&' for perfect forwarding scenarios\n";

    std::cout << "\n";
}

// 11. COMMON PITFALLS AND SOLUTIONS
void common_pitfalls() {
    std::cout << "=== COMMON PITFALLS AND SOLUTIONS ===\n";

    // Pitfall 1: Dangling references
    std::cout << "1. PITFALL: Dangling references\n";

    // DON'T DO THIS:
    // auto& ref = some_function_returning_temporary();
    // for (const auto& item : ref) { ... }  // Undefined behavior!

    std::cout << "Solution: Ensure the range object lives long enough\n";

    // Pitfall 2: Modifying container while iterating
    std::cout << "\n2. PITFALL: Modifying container size during iteration\n";
    std::vector<int> numbers = {1, 2, 3, 4, 5};

    // DON'T DO THIS in general:
    // for (const auto& num : numbers) {
    //     if (num % 2 == 0) {
    //         numbers.push_back(num * 2);  // Dangerous!
    //     }
    // }

    std::cout << "Solution: Use traditional iterators or collect changes first\n";

    // Safe approach - collect indices to modify
    std::vector<int> to_add;
    for (const auto& num : numbers) {
        if (num % 2 == 0) {
            to_add.push_back(num * 2);
        }
    }
    for (const auto& num : to_add) {
        numbers.push_back(num);
    }

    std::cout << "Result after safe modification: ";
    for (const auto& num : numbers) std::cout << num << " ";

    // Pitfall 3: Wrong reference type
    std::cout << "\n\n3. PITFALL: Using auto instead of const auto& for large objects\n";
    std::cout << "Always prefer const auto& for read-only access to avoid unnecessary copies\n";

    // Pitfall 4: Lifetime issues with temporaries
    std::cout << "\n4. PITFALL: Lifetime issues\n";
    std::cout << "Ensure temporary objects live for the entire loop duration\n";

    std::cout << "\n";
}

int main() {
    basic_range_for_examples();
    reference_semantics_examples();
    const_correctness_examples();
    container_specific_examples();
    structured_bindings_examples();
    universal_reference_examples();
    nested_containers_examples();
    custom_class_examples();
    algorithms_with_range_for();
    performance_considerations();
    common_pitfalls();

    std::cout << "=== RANGE-BASED FOR LOOPS SUMMARY ===\n";
    std::cout << "SYNTAX VARIANTS:\n";
    std::cout << "- for (auto item : container)         // Copy semantics\n";
    std::cout << "- for (auto& item : container)        // Reference (modifiable)\n";
    std::cout << "- for (const auto& item : container)  // Const reference (read-only)\n";
    std::cout << "- for (auto&& item : container)       // Universal reference\n";
    std::cout << "- for (auto [a, b] : pairs)           // Structured bindings (C++17+)\n";
    std::cout << "\nBEST PRACTICES:\n";
    std::cout << "- Prefer const auto& for read-only access\n";
    std::cout << "- Use auto& when modifying elements\n";
    std::cout << "- Use structured bindings for pairs/tuples\n";
    std::cout << "- Be careful with container modifications during iteration\n";
    std::cout << "- Ensure range object lifetime exceeds loop duration\n";
    std::cout << "\nCOMPATIBLE TYPES:\n";
    std::cout << "- STL containers (vector, list, set, map, etc.)\n";
    std::cout << "- C-style arrays\n";
    std::cout << "- std::string\n";
    std::cout << "- Custom classes with begin()/end() methods\n";
    std::cout << "- Initializer lists\n";

    return 0;
}
