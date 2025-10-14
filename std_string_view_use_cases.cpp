// std_string_view_use_cases.cpp
// Comprehensive demonstration of std::string_view use cases, benefits, and comparisons
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <algorithm>

// =============================================================================
// BASIC USE CASES AND BENEFITS
// =============================================================================

// Function accepting string_view - flexible input types
void process_text(std::string_view text) {
    std::cout << "Processing: '" << text << "' (length: " << text.length() << ")\n";
}

// Function accepting std::string - requires string object
void process_string(const std::string& text) {
    std::cout << "Processing string: '" << text << "' (length: " << text.length() << ")\n";
}

// =============================================================================
// ADVANCED USE CASES
// =============================================================================

// Tokenization without allocations
std::vector<std::string_view> tokenize(std::string_view text, char delimiter) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = 0;

    while ((end = text.find(delimiter, start)) != std::string_view::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(text.substr(start));
    return tokens;
}

// Efficient string parsing
class ConfigParser {
public:
    void parse_line(std::string_view line) {
        // Trim whitespace
        line = trim(line);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') return;

        // Find key=value separator
        auto pos = line.find('=');
        if (pos != std::string_view::npos) {
            auto key = trim(line.substr(0, pos));
            auto value = trim(line.substr(pos + 1));

            std::cout << "Config: '" << key << "' = '" << value << "'\n";
        }
    }

private:
    std::string_view trim(std::string_view str) {
        auto start = str.find_first_not_of(" \t");
        if (start == std::string_view::npos) return {};

        auto end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }
};

// URL path processing
class URLProcessor {
public:
    struct URLParts {
        std::string_view scheme;
        std::string_view host;
        std::string_view path;
        std::string_view query;
    };

    URLParts parse_url(std::string_view url) {
        URLParts parts;

        // Find scheme
        auto scheme_end = url.find("://");
        if (scheme_end != std::string_view::npos) {
            parts.scheme = url.substr(0, scheme_end);
            url = url.substr(scheme_end + 3);
        }

        // Find host/path separator
        auto path_start = url.find('/');
        if (path_start != std::string_view::npos) {
            parts.host = url.substr(0, path_start);
            url = url.substr(path_start);

            // Find query separator
            auto query_start = url.find('?');
            if (query_start != std::string_view::npos) {
                parts.path = url.substr(0, query_start);
                parts.query = url.substr(query_start + 1);
            } else {
                parts.path = url;
            }
        } else {
            parts.host = url;
        }

        return parts;
    }
};

// =============================================================================
// PERFORMANCE COMPARISON
// =============================================================================

// Performance test helper
template<typename Func>
double measure_time(Func&& func, int iterations = 10000) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0; // Return milliseconds
}

void performance_comparison() {
    std::cout << "\n=== PERFORMANCE COMPARISON ===\n";

    std::string large_text = "This is a large string that we will use for performance testing. ";
    for (int i = 0; i < 10; ++i) {
        large_text += large_text; // Exponentially grow the string
    }

    std::cout << "String size: " << large_text.size() << " characters\n";

    // Test 1: Function parameter passing
    auto test_string_param = [&]() {
        process_string(large_text);
    };

    auto test_string_view_param = [&]() {
        std::string_view sv = large_text;
        process_text(sv);
    };

    double string_time = measure_time(test_string_param);
    double string_view_time = measure_time(test_string_view_param);

    std::cout << "Parameter passing:\n";
    std::cout << "  std::string:      " << string_time << " ms\n";
    std::cout << "  std::string_view: " << string_view_time << " ms\n";
    std::cout << "  Speedup: " << (string_time / string_view_time) << "x\n\n";

    // Test 2: Substring operations
    auto test_string_substr = [&]() {
        std::string sub = large_text.substr(100, 500); // Creates new string
        volatile auto len = sub.length(); // Prevent optimization
    };

    auto test_string_view_substr = [&]() {
        std::string_view sv = large_text;
        std::string_view sub = sv.substr(100, 500); // Just adjusts view
        volatile auto len = sub.length(); // Prevent optimization
    };

    double substr_string_time = measure_time(test_string_substr);
    double substr_view_time = measure_time(test_string_view_substr);

    std::cout << "Substring operations:\n";
    std::cout << "  std::string substr:      " << substr_string_time << " ms\n";
    std::cout << "  std::string_view substr: " << substr_view_time << " ms\n";
    std::cout << "  Speedup: " << (substr_string_time / substr_view_time) << "x\n\n";
}

// =============================================================================
// LIFETIME SAFETY DEMONSTRATION
// =============================================================================

void demonstrate_lifetime_issues() {
    std::cout << "\n=== LIFETIME SAFETY DEMONSTRATION ===\n";

    std::string_view dangerous_view;

    {
        std::string temp_string = "This string will be destroyed";
        dangerous_view = temp_string; // Dangerous! temp_string will be destroyed
        std::cout << "Inside scope: " << dangerous_view << "\n";
    } // temp_string is destroyed here

    // Using dangerous_view here would be undefined behavior!
    // std::cout << "Outside scope: " << dangerous_view << "\n"; // DON'T DO THIS!

    std::cout << "WARNING: dangerous_view is now invalid (pointing to destroyed string)\n";

    // Safe approach: copy to std::string when needed beyond original lifetime
    std::string safe_copy;
    {
        std::string temp_string = "This string will be copied";
        std::string_view view = temp_string;
        safe_copy = std::string(view); // Explicit copy
    }
    std::cout << "Safe copy: " << safe_copy << "\n";
}

// =============================================================================
// CONTAINER USAGE EXAMPLES
// =============================================================================

void demonstrate_container_usage() {
    std::cout << "\n=== CONTAINER USAGE ===\n";

    // Using string_view as map key (be careful with lifetime!)
    std::unordered_map<std::string_view, int> word_count;

    std::string text = "hello world hello";
    auto tokens = tokenize(text, ' ');

    for (const auto& token : tokens) {
        word_count[token]++; // Safe because 'text' outlives the map
    }

    std::cout << "Word count:\n";
    for (const auto& [word, count] : word_count) {
        std::cout << "  '" << word << "': " << count << "\n";
    }
}

// =============================================================================
// INTEROPERABILITY EXAMPLES
// =============================================================================

void demonstrate_interoperability() {
    std::cout << "\n=== INTEROPERABILITY ===\n";

    // 1. From std::string to string_view
    std::string str = "Hello, World!";
    std::string_view sv1 = str; // Implicit conversion
    std::cout << "From string: " << sv1 << "\n";

    // 2. From string literal to string_view
    std::string_view sv2 = "Direct from literal";
    std::cout << "From literal: " << sv2 << "\n";

    // 3. From char array to string_view
    const char arr[] = "From char array";
    std::string_view sv3 = arr;
    std::cout << "From char array: " << sv3 << "\n";

    // 4. From string_view to std::string (explicit)
    std::string new_str = std::string(sv1);
    std::cout << "Back to string: " << new_str << "\n";

    // 5. C-style API compatibility issue
    // sv1.data() might not be null-terminated!
    // For C APIs, convert to std::string first:
    std::string for_c_api = std::string(sv1);
    printf("C-style print: %s\n", for_c_api.c_str());
}

// =============================================================================
// COMPARISON WITH STD::SPAN
// =============================================================================

void demonstrate_span_comparison() {
    std::cout << "\n=== std::string_view vs std::span ===\n";

    // string_view: specifically for character sequences
    std::string text = "Hello, World!";
    std::string_view sv = text;

    std::cout << "string_view operations:\n";
    std::cout << "  Original: " << sv << "\n";
    std::cout << "  Substring: " << sv.substr(7, 5) << "\n";
    std::cout << "  Find: " << sv.find("World") << "\n";

    // span: generic contiguous container view (C++20)
    // Note: std::span is not available in all C++17 implementations
    /*
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::span<int> span_view = numbers;

    std::cout << "span operations:\n";
    std::cout << "  Size: " << span_view.size() << "\n";
    std::cout << "  First element: " << span_view[0] << "\n";
    // No string-specific operations like find() or substr()
    */

    std::cout << "Key differences:\n";
    std::cout << "  - string_view: char sequences, string operations\n";
    std::cout << "  - span: any type, generic container operations\n";
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    std::cout << "=== std::string_view COMPREHENSIVE EXAMPLES ===\n";

    // Basic usage
    std::cout << "\n=== BASIC USAGE ===\n";

    // Various input types
    std::string str = "Hello from string";
    process_text(str);                    // std::string
    process_text("Hello from literal");   // string literal
    process_text(std::string_view("Hello from string_view"));

    const char* cstr = "Hello from C-string";
    process_text(cstr);                   // C-string

    // Substring without allocation
    std::string_view sv = "The quick brown fox jumps";
    auto sub = sv.substr(4, 5); // "quick"
    std::cout << "Substring: '" << sub << "'\n";

    // Tokenization example
    std::cout << "\n=== TOKENIZATION ===\n";
    std::string csv_line = "apple,banana,cherry,date";
    auto tokens = tokenize(csv_line, ',');
    std::cout << "Tokens: ";
    for (const auto& token : tokens) {
        std::cout << "'" << token << "' ";
    }
    std::cout << "\n";

    // Configuration parsing
    std::cout << "\n=== CONFIGURATION PARSING ===\n";
    ConfigParser parser;
    parser.parse_line("# This is a comment");
    parser.parse_line("database_host = localhost");
    parser.parse_line("  port = 5432  ");
    parser.parse_line("timeout=30");

    // URL processing
    std::cout << "\n=== URL PROCESSING ===\n";
    URLProcessor url_proc;
    auto parts = url_proc.parse_url("https://example.com/path/to/resource?param=value");
    std::cout << "URL Parts:\n";
    std::cout << "  Scheme: '" << parts.scheme << "'\n";
    std::cout << "  Host: '" << parts.host << "'\n";
    std::cout << "  Path: '" << parts.path << "'\n";
    std::cout << "  Query: '" << parts.query << "'\n";

    // Performance comparison
    performance_comparison();

    // Lifetime safety
    demonstrate_lifetime_issues();

    // Container usage
    demonstrate_container_usage();

    // Interoperability
    demonstrate_interoperability();

    // Comparison with span
    demonstrate_span_comparison();

    return 0;
}

/*
=============================================================================
COMPREHENSIVE COMPARISON: std::string_view vs std::string
=============================================================================

BENEFITS of std::string_view:
✓ Zero-cost abstraction: Just a pointer + size (16 bytes on 64-bit systems)
✓ No memory allocation or copying when created from existing strings
✓ Efficient substring operations (no allocation, just pointer arithmetic)
✓ Accepts multiple input types: std::string, literals, char arrays
✓ Perfect for read-only string processing and parsing
✓ Significant performance gains in algorithms that don't need ownership
✓ Reduces unnecessary string copies in function parameters
✓ Enables efficient string tokenization and parsing

LIMITATIONS of std::string_view:
✗ Non-owning: Does not manage memory, vulnerable to dangling references
✗ No null-termination guarantee: Cannot safely pass to C APIs
✗ Read-only: Cannot modify the underlying character data
✗ Lifetime dependency: Must ensure underlying data outlives the view
✗ No automatic memory management or RAII for the viewed data

WHEN TO USE std::string_view:
→ Function parameters that only need to read string data
→ Parsing and tokenization operations
→ Substring operations without creating new strings
→ APIs that should accept various string types flexibly
→ Performance-critical code that processes strings frequently
→ Template functions that work with string-like types

WHEN TO USE std::string:
→ When you need to own and manage the string data
→ When interfacing with C APIs that expect null-terminated strings
→ When you need to modify the string content
→ When storing strings in containers with uncertain lifetimes
→ When the string needs to outlive its creation context

PERFORMANCE CHARACTERISTICS:
- Construction: string_view is O(1), string may be O(n) for copying
- Substring: string_view is O(1), string is O(n) for copying
- Parameter passing: string_view is always cheap, string may involve copying
- Memory usage: string_view uses minimal memory, string owns its data

SAFETY CONSIDERATIONS:
- Always ensure the underlying string data outlives the string_view
- Be careful when storing string_view in containers or class members
- Convert to std::string when you need guaranteed ownership
- Never return string_view to local string objects from functions

BEST PRACTICES:
1. Use string_view for function parameters that only read strings
2. Convert to std::string when you need to store or modify the data
3. Be explicit about lifetime requirements in your APIs
4. Use std::string for data members unless you can guarantee lifetime
5. Prefer string_view for parsing and tokenization algorithms
6. Always validate that string_view data is still valid before use
=============================================================================
*/
