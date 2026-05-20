#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <cassert>
#include <optional>
#include <variant>

#include <unordered_map>
#include <thread>
#include <random>

/*
 * Comprehensive C++ Attributes Use Cases and Examples
 * Covers: [[maybe_unused]], [[nodiscard]], [[deprecated]], [[fallthrough]],
 *         [[likely]], [[unlikely]], [[no_unique_address]], [[noreturn]]
 */

// =============================================================================
// 1. [[maybe_unused]] - Suppresses unused variable/parameter warnings
// =============================================================================

namespace maybe_unused_examples {

    // Example 1: Debug builds vs Release builds
    void process_data(const std::vector<int>& data) {
        [[maybe_unused]] auto start_time = std::chrono::high_resolution_clock::now();

        // Process data...
        for (const auto& item : data) {
            // Some processing
            std::cout << item << " ";
        }
        std::cout << "\n";

        #ifdef DEBUG
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Processing took: " << duration.count() << "ms\n";
        #endif
        // In release builds, start_time is unused but no warning due to [[maybe_unused]]
    }

    // Example 2: Template parameters that might not be used
    template<typename T, typename U>
    class DataProcessor {
    public:
        void process([[maybe_unused]] const T& data) {
            // U might be used for specializations but not in general case
            [[maybe_unused]] constexpr bool is_same = std::is_same_v<T, U>;

            std::cout << "Processing data of type: " << typeid(T).name() << "\n";
            // is_same might be used in conditional compilation or assertions
        }
    };

    // Example 3: Lambda captures that might not be used
    void lambda_example() {
        int config_value = 42;
        [[maybe_unused]] bool debug_mode = true;

        auto processor = [config_value, debug_mode](const std::string& input) {
            std::cout << "Processing: " << input << " with config: " << config_value << "\n";
            // debug_mode might be used conditionally
            #ifdef VERBOSE_DEBUG
            if (debug_mode) {
                std::cout << "Debug mode is enabled\n";
            }
            #endif
        };

        processor("test data");
    }

    // Example 4: Function parameters in interface implementations
    class BaseHandler {
    public:
        virtual ~BaseHandler() = default;
        virtual void handle([[maybe_unused]] int error_code,
                          [[maybe_unused]] const std::string& message) = 0;
    };

    class SimpleHandler : public BaseHandler {
    public:
        void handle([[maybe_unused]] int error_code,
                   const std::string& message) override {
            // This implementation only uses message, not error_code
            std::cout << "Error: " << message << "\n";
        }
    };

    void demonstrate() {
        std::cout << "\n=== [[maybe_unused]] Examples ===\n";

        std::vector<int> data = {1, 2, 3, 4, 5};
        process_data(data);

        DataProcessor<int, double> processor;
        processor.process(42);

        lambda_example();

        SimpleHandler handler;
        handler.handle(404, "Not Found");
    }
}

// =============================================================================
// 2. [[nodiscard]] - Warns when return value is discarded
// =============================================================================

namespace nodiscard_examples {

    // Example 1: Error codes that must be checked
    enum class ErrorCode {
        SUCCESS,
        INVALID_INPUT,
        NETWORK_ERROR,
        TIMEOUT
    };

    [[nodiscard]] ErrorCode connect_to_server(const std::string& address) {
        if (address.empty()) {
            return ErrorCode::INVALID_INPUT;
        }
        // Simulate connection logic
        std::cout << "Connecting to: " << address << "\n";
        return ErrorCode::SUCCESS;
    }

    // Example 2: Resource allocation that must be checked
    [[nodiscard]] std::unique_ptr<int[]> allocate_buffer(size_t size) {
        if (size == 0) {
            return nullptr;
        }
        return std::make_unique<int[]>(size);
    }

    // Example 3: Pure functions where discarding result makes no sense
    [[nodiscard]] constexpr int square(int x) {
        return x * x;
    }

    [[nodiscard]] bool is_valid_email(const std::string& email) {
        return email.find('@') != std::string::npos;
    }

    // Example 4: Classes with [[nodiscard]] constructor
    class [[nodiscard]] ScopedLock {
    private:
        bool& locked_ref_;

    public:
        explicit ScopedLock(bool& mutex_locked) : locked_ref_(mutex_locked) {
            locked_ref_ = true;
            std::cout << "Lock acquired\n";
        }

        ~ScopedLock() {
            locked_ref_ = false;
            std::cout << "Lock released\n";
        }

        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;
    };

    // Example 5: Optional return values
    [[nodiscard]] std::optional<int> parse_int(const std::string& str) {
        try {
            return std::stoi(str);
        } catch (...) {
            return std::nullopt;
        }
    }

    void demonstrate() {
        std::cout << "\n=== [[nodiscard]] Examples ===\n";

        // Good: Checking return value
        auto result = connect_to_server("192.168.1.1");
        if (result != ErrorCode::SUCCESS) {
            std::cout << "Connection failed\n";
        }

        // Good: Using allocated buffer
        auto buffer = allocate_buffer(100);
        if (buffer) {
            std::cout << "Buffer allocated successfully\n";
        }

        // Good: Using calculation result
        int value = 5;
        int squared = square(value);
        std::cout << value << " squared is " << squared << "\n";

        // Good: Using validation result
        std::string email = "user@example.com";
        if (is_valid_email(email)) {
            std::cout << "Valid email: " << email << "\n";
        }

        // Good: Using scoped lock
        bool mutex_locked = false;
        {
            auto lock = ScopedLock(mutex_locked);
            std::cout << "Critical section\n";
        }

        // Good: Using optional result
        auto parsed = parse_int("123");
        if (parsed) {
            std::cout << "Parsed value: " << *parsed << "\n";
        }

        // BAD: These would generate warnings if uncommented
        // connect_to_server("192.168.1.1");  // Warning: discarding return value
        // square(5);                          // Warning: discarding return value
        // ScopedLock(mutex_locked);          // Warning: temporary object destroyed immediately
    }
}

// =============================================================================
// 3. [[deprecated]] - Marks entities as deprecated
// =============================================================================

namespace deprecated_examples {

    // Example 1: Deprecated function with replacement
    [[deprecated("Use process_data_v2() instead")]]
    void process_data(const std::vector<int>& data) {
        std::cout << "Old data processing (deprecated)\n";
        for (const auto& item : data) {
            std::cout << item << " ";
        }
        std::cout << "\n";
    }

    void process_data_v2(const std::vector<int>& data) {
        std::cout << "New data processing (recommended)\n";
        for (size_t i = 0; i < data.size(); ++i) {
            std::cout << "[" << i << "]=" << data[i] << " ";
        }
        std::cout << "\n";
    }

    // Example 2: Deprecated class
    class [[deprecated("Use ModernLogger instead")]] OldLogger {
    public:
        void log(const std::string& message) {
            std::cout << "OLD: " << message << "\n";
        }
    };

    class ModernLogger {
    public:
        void log(const std::string& message, const std::string& level = "INFO") {
            std::cout << "[" << level << "] " << message << "\n";
        }
    };

    // Example 3: Deprecated enum value
    enum class Status {
        PENDING,
        PROCESSING,
        COMPLETED,
        FAILED
    };

    // Example 4: Deprecated type alias
    using old_string_view [[deprecated("Use std::string_view instead")]] = const char*;
    using modern_string_view = std::string_view;

    // Example 5: Deprecated template specialization
    template<typename T>
    class Container {
    public:
        void add(const T& item) {
            std::cout << "Adding item to container\n";
            (void)item; // Suppress unused parameter warning
        }
    };

    template<>
    class [[deprecated("Use Container<std::string> instead")]] Container<const char*> {
    public:
        void add(const char* item) {
            std::cout << "Adding C-string (deprecated): " << item << "\n";
        }
    };

    void demonstrate() {
        std::cout << "\n=== [[deprecated]] Examples ===\n";

        std::vector<int> data = {1, 2, 3};

        // Using new recommended function
        process_data_v2(data);

        // Using deprecated function (would generate warning)
        // process_data(data);  // Warning: 'process_data' is deprecated

        // Using modern logger
        ModernLogger modern_logger;
        modern_logger.log("System started", "INFO");

        // Using deprecated logger (would generate warning)
        // OldLogger old_logger;  // Warning: 'OldLogger' is deprecated
        // old_logger.log("System started");

        // Using enum
        Status status = Status::COMPLETED;
        std::cout << "Status: " << static_cast<int>(status) << "\n";
    }
}

// =============================================================================
// 4. [[fallthrough]] - Indicates intentional fallthrough in switch
// =============================================================================

namespace fallthrough_examples {

    enum class HttpStatus {
        OK = 200,
        NOT_FOUND = 404,
        INTERNAL_ERROR = 500,
        BAD_GATEWAY = 502,
        SERVICE_UNAVAILABLE = 503
    };

    // Example 1: HTTP status code handling
    void handle_http_status(HttpStatus status) {
        switch (status) {
            case HttpStatus::OK:
                std::cout << "Request successful\n";
                break;

            case HttpStatus::NOT_FOUND:
                std::cout << "Resource not found\n";
                break;

            case HttpStatus::INTERNAL_ERROR:
                std::cout << "Server error occurred\n";
                [[fallthrough]];  // Intentionally fall through to handle all server errors

            case HttpStatus::BAD_GATEWAY:
            case HttpStatus::SERVICE_UNAVAILABLE:
                std::cout << "Server is experiencing issues\n";
                std::cout << "Please try again later\n";
                break;
        }
    }

    // Example 2: Command line argument parsing
    void parse_arguments(char option) {
        bool verbose = false;
        bool debug = false;

        switch (option) {
            case 'D':
                debug = true;
                std::cout << "Debug mode enabled\n";
                [[fallthrough]];  // Debug mode implies verbose

            case 'v':
                verbose = true;
                std::cout << "Verbose mode enabled\n";
                break;

            case 'q':
                std::cout << "Quiet mode enabled\n";
                break;

            case 'h':
            default:
                std::cout << "Usage: program [-D] [-v] [-q] [-h]\n";
                break;
        }

        (void)verbose; // Suppress unused variable warning
        (void)debug;   // Suppress unused variable warning
    }

    void demonstrate() {
        std::cout << "\n=== [[fallthrough]] Examples ===\n";

        std::cout << "HTTP Status Handling:\n";
        handle_http_status(HttpStatus::INTERNAL_ERROR);

        std::cout << "\nCommand Line Parsing:\n";
        parse_arguments('D');
    }
}

// =============================================================================
// 5. [[likely]] and [[unlikely]] - Branch prediction hints (C++20)
// =============================================================================

namespace likely_unlikely_examples {

    // Example 1: Error checking (using if constexpr for C++17 compatibility)
    [[nodiscard]] bool validate_input(const std::string& input) {
        if (input.empty()) {
            std::cout << "Error: Empty input\n";
            return false;
        }

        if (input.length() > 1000) {
            std::cout << "Error: Input too long\n";
            return false;
        }

        // Common case - input is valid
        return true;
    }

    // Example 2: Cache operations
    class Cache {
    private:
        std::unordered_map<std::string, std::string> data_;

    public:
        std::optional<std::string> get(const std::string& key) {
            auto it = data_.find(key);
            if (it != data_.end()) {
                // Cache hits are expected to be common
                std::cout << "Cache hit for: " << key << "\n";
                return it->second;
            } else {
                // Cache misses should be rare in a well-tuned system
                std::cout << "Cache miss for: " << key << "\n";
                return std::nullopt;
            }
        }

        void put(const std::string& key, const std::string& value) {
            data_[key] = value;
        }
    };

    void demonstrate() {
        std::cout << "\n=== Branch Prediction Examples (C++20 likely/unlikely) ===\n";

        // Input validation
        std::cout << "Input validation:\n";
        auto result1 = validate_input("valid input");
        auto result2 = validate_input("");
        (void)result1; (void)result2; // Suppress unused warnings

        // Cache operations
        std::cout << "\nCache operations:\n";
        Cache cache;
        cache.put("key1", "value1");
        cache.get("key1");  // Cache hit
        cache.get("key2");  // Cache miss

        std::cout << "Note: [[likely]]/[[unlikely]] attributes are C++20 features\n";
        std::cout << "They provide hints to the compiler for branch prediction optimization\n";
    }
}

// =============================================================================
// 6. [[no_unique_address]] - Optimizes empty base classes (C++20)
// =============================================================================

namespace no_unique_address_examples {

    // Example 1: Empty allocator optimization
    template<typename T>
    class EmptyAllocator {
    public:
        using value_type = T;

        T* allocate(size_t n) {
            return static_cast<T*>(std::malloc(n * sizeof(T)));
        }

        void deallocate(T* p, size_t) {
            std::free(p);
        }
    };

    // Example 2: Stateless function objects
    struct Add {
        template<typename T>
        constexpr T operator()(const T& a, const T& b) const {
            return a + b;
        }
    };

    template<typename T, typename BinaryOp = Add>
    class Calculator {
    private:
        T value_;
        BinaryOp operation_;  // Would use [[no_unique_address]] in C++20

    public:
        explicit Calculator(T initial_value) : value_(initial_value) {}

        void apply(const T& operand) {
            value_ = operation_(value_, operand);
        }

        T get_value() const { return value_; }
    };

    // Example 3: Empty base class optimization
    class EmptyBase {
        // Empty class - normally would take 1 byte
    };

    struct OptimizedStruct {
        int data;
        EmptyBase empty_member;  // Would use [[no_unique_address]] in C++20
        double more_data;
    };

    void demonstrate() {
        std::cout << "\n=== [[no_unique_address]] Examples (C++20) ===\n";

        // Size comparison
        std::cout << "Size comparisons:\n";
        std::cout << "OptimizedStruct size: " << sizeof(OptimizedStruct) << " bytes\n";
        std::cout << "EmptyAllocator size: " << sizeof(EmptyAllocator<int>) << " bytes\n";

        // Calculator with stateless operation
        Calculator<int, Add> adder(10);
        adder.apply(5);

        std::cout << "Calculator result: " << adder.get_value() << "\n";

        std::cout << "Note: [[no_unique_address]] is a C++20 feature\n";
        std::cout << "It allows empty types to not take up storage space\n";
    }
}

// =============================================================================
// 7. [[noreturn]] - Indicates function never returns
// =============================================================================

namespace noreturn_examples {

    // Example 1: Error handling that terminates program
    [[noreturn]] void fatal_error(const std::string& message) {
        std::cerr << "FATAL ERROR: " << message << "\n";
        std::cerr << "Program will terminate\n";
        std::abort();
    }

    // Example 2: Exception throwing helper
    [[noreturn]] void throw_invalid_argument(const std::string& message) {
        throw std::invalid_argument(message);
    }

    // Example 3: Switch case that should never be reached
    enum class Color { RED, GREEN, BLUE };

    [[noreturn]] void unreachable_case() {
        std::cerr << "This code should never be reached!\n";
        std::abort();
    }

    void process_color(Color color) {
        switch (color) {
            case Color::RED:
                std::cout << "Processing red\n";
                break;
            case Color::GREEN:
                std::cout << "Processing green\n";
                break;
            case Color::BLUE:
                std::cout << "Processing blue\n";
                break;
            default:
                unreachable_case();  // Should never happen
        }
    }

    // Example 4: Function that always throws
    [[nodiscard]] int divide(int a, int b) {
        if (b == 0) {
            throw_invalid_argument("Division by zero");
        }
        return a / b;
    }

    void demonstrate() {
        std::cout << "\n=== [[noreturn]] Examples ===\n";

        // Process colors
        std::cout << "Color processing:\n";
        process_color(Color::RED);
        process_color(Color::GREEN);
        process_color(Color::BLUE);

        // Division example
        std::cout << "\nDivision examples:\n";
        try {
            int result = divide(10, 2);
            std::cout << "10 / 2 = " << result << "\n";

            // This will throw
            result = divide(10, 0);
            (void)result; // Suppress unused warning
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << "\n";
        }

        std::cout << "Note: Some [[noreturn]] examples are commented out to avoid program termination\n";
        std::cout << "[[noreturn]] functions like fatal_error() would not return control to caller\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL ATTRIBUTES
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "C++ ATTRIBUTES COMPREHENSIVE EXAMPLES\n";
    std::cout << "=============================================================================\n";

    try {
        maybe_unused_examples::demonstrate();
        nodiscard_examples::demonstrate();
        deprecated_examples::demonstrate();
        fallthrough_examples::demonstrate();
        likely_unlikely_examples::demonstrate();
        no_unique_address_examples::demonstrate();
        noreturn_examples::demonstrate();

    } catch (const std::exception& e) {
        std::cerr << "Exception caught in main: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n=============================================================================\n";
    std::cout << "SUMMARY OF C++ ATTRIBUTES:\n";
    std::cout << "=============================================================================\n";
    std::cout << "[[maybe_unused]]     - Suppresses unused variable/parameter warnings\n";
    std::cout << "[[nodiscard]]        - Warns when return value is discarded\n";
    std::cout << "[[deprecated]]       - Marks entities as deprecated with optional message\n";
    std::cout << "[[fallthrough]]      - Indicates intentional fallthrough in switch statements\n";
    std::cout << "[[likely]]           - Hints that branch is likely to be taken\n";
    std::cout << "[[unlikely]]         - Hints that branch is unlikely to be taken\n";
    std::cout << "[[no_unique_address]] - Optimizes storage for empty types\n";
    std::cout << "[[noreturn]]         - Indicates function never returns normally\n";
    std::cout << "=============================================================================\n";

    return 0;
}
