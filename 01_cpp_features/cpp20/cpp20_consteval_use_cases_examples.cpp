/*
 * C++20 consteval Use Cases and Examples
 *
 * consteval specifies that a function is an immediate function - it must be
 * evaluated at compile-time. Unlike constexpr, consteval functions CANNOT
 * be evaluated at runtime.
 *
 * Key Benefits:
 * 1. Guaranteed compile-time evaluation
 * 2. No runtime overhead - results are compile-time constants
 * 3. Stronger compile-time computation guarantees than constexpr
 * 4. Perfect for configuration validation, compile-time calculations
 * 5. Enables complex compile-time metaprogramming
 * 6. Improved performance for computations that can be done at compile-time
 */

#include <iostream>
#include <string>
#include <string_view>
#include <array>
#include <cmath>
#include <type_traits>
#include <chrono>
#include <algorithm>
#include <numeric>

// ============================================================================
// 1. BASIC CONSTEVAL FUNCTIONS
// ============================================================================

// consteval function - MUST be evaluated at compile-time
consteval int square(int x) {
    return x * x;
}

// constexpr function - CAN be evaluated at compile-time or runtime
constexpr int square_constexpr(int x) {
    return x * x;
}

// Regular function - runtime only
int square_runtime(int x) {
    return x * x;
}

void demonstrate_basic_consteval() {
    std::cout << "\n=== Basic consteval Functions ===\n";

    // consteval - compile-time only
    constexpr int compile_time_result = square(5);  // OK: compile-time constant
    std::cout << "square(5) = " << compile_time_result << " (computed at compile-time)\n";

    // This would cause compilation error:
    // int runtime_value = 5;
    // int result = square(runtime_value);  // ERROR: cannot call consteval with runtime value

    // constexpr - can be used both ways
    constexpr int constexpr_compile = square_constexpr(6);  // Compile-time
    int runtime_var = 7;
    int constexpr_runtime = square_constexpr(runtime_var);   // Runtime

    std::cout << "square_constexpr(6) = " << constexpr_compile << " (compile-time)\n";
    std::cout << "square_constexpr(runtime_var) = " << constexpr_runtime << " (runtime)\n";

    // Regular function - runtime only
    int regular_result = square_runtime(8);
    std::cout << "square_runtime(8) = " << regular_result << " (runtime)\n";
}

// ============================================================================
// 2. COMPILE-TIME VALIDATION
// ============================================================================

// Validate configuration at compile-time
struct Config {
    int max_connections;
    int timeout_seconds;
    double cache_size_mb;
};

consteval bool validate_config(const Config& config) {
    if (config.max_connections <= 0 || config.max_connections > 10000) {
        return false;
    }
    if (config.timeout_seconds <= 0 || config.timeout_seconds > 3600) {
        return false;
    }
    if (config.cache_size_mb <= 0 || config.cache_size_mb > 1024.0) {
        return false;
    }
    return true;
}

consteval Config create_validated_config(int max_conn, int timeout, double cache_mb) {
    Config config{max_conn, timeout, cache_mb};

    // This will cause compilation error if validation fails
    static_assert(validate_config(config), "Configuration validation failed!");

    return config;
}

// Network port validation
consteval bool is_valid_port(int port) {
    return port > 0 && port <= 65535;
}

consteval int validated_port(int port) {
    static_assert(is_valid_port(port), "Invalid port number!");
    return port;
}

void demonstrate_compile_time_validation() {
    std::cout << "\n=== Compile-Time Validation ===\n";

    // Valid configuration - compiles successfully
    constexpr auto valid_config = create_validated_config(100, 30, 256.0);
    std::cout << "Valid config: " << valid_config.max_connections
              << " connections, " << valid_config.timeout_seconds
              << "s timeout, " << valid_config.cache_size_mb << "MB cache\n";

    // Invalid configuration would cause compilation error:
    // constexpr auto invalid_config = create_validated_config(-1, 30, 256.0);  // ERROR!

    // Port validation
    constexpr int web_port = validated_port(8080);  // OK
    constexpr int db_port = validated_port(5432);   // OK
    std::cout << "Web server port: " << web_port << "\n";
    std::cout << "Database port: " << db_port << "\n";

    // This would cause compilation error:
    // constexpr int invalid_port = validated_port(70000);  // ERROR: Invalid port!

    std::cout << "All configurations validated at compile-time!\n";
}

// ============================================================================
// 3. MATHEMATICAL COMPUTATIONS
// ============================================================================

// Factorial calculation at compile-time
consteval long long factorial(int n) {
    if (n < 0) {
        // This will cause compilation error for negative inputs
        throw "Factorial of negative number is undefined";
    }
    if (n == 0 || n == 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

// Prime number checking
consteval bool is_prime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;

    for (int i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// Generate nth prime number
consteval int nth_prime(int n) {
    if (n <= 0) throw "Invalid prime index";

    int count = 0;
    int candidate = 2;

    while (true) {
        if (is_prime(candidate)) {
            count++;
            if (count == n) {
                return candidate;
            }
        }
        candidate++;
    }
}

// Power calculation with integer exponent
consteval double power(double base, int exponent) {
    if (exponent == 0) return 1.0;
    if (exponent < 0) return 1.0 / power(base, -exponent);

    double result = 1.0;
    for (int i = 0; i < exponent; ++i) {
        result *= base;
    }
    return result;
}

void demonstrate_mathematical_computations() {
    std::cout << "\n=== Mathematical Computations ===\n";

    // Factorial calculations
    constexpr auto fact_5 = factorial(5);
    constexpr auto fact_10 = factorial(10);
    constexpr auto fact_15 = factorial(15);

    std::cout << "5! = " << fact_5 << "\n";
    std::cout << "10! = " << fact_10 << "\n";
    std::cout << "15! = " << fact_15 << "\n";

    // Prime number calculations
    constexpr bool is_17_prime = is_prime(17);
    constexpr bool is_18_prime = is_prime(18);
    constexpr int prime_10th = nth_prime(10);
    constexpr int prime_25th = nth_prime(25);

    std::cout << "17 is prime: " << std::boolalpha << is_17_prime << "\n";
    std::cout << "18 is prime: " << is_18_prime << "\n";
    std::cout << "10th prime: " << prime_10th << "\n";
    std::cout << "25th prime: " << prime_25th << "\n";

    // Power calculations
    constexpr double two_to_10 = power(2.0, 10);
    constexpr double pi_squared = power(3.14159, 2);
    constexpr double half_to_minus_3 = power(0.5, -3);

    std::cout << "2^10 = " << two_to_10 << "\n";
    std::cout << "π² ≈ " << pi_squared << "\n";
    std::cout << "0.5^(-3) = " << half_to_minus_3 << "\n";

    std::cout << "All calculations performed at compile-time!\n";
}

// ============================================================================
// 4. STRING PROCESSING AND HASHING
// ============================================================================

// Compile-time string length (alternative to std::string_view::size())
consteval std::size_t string_length(const char* str) {
    std::size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}

// Simple hash function for compile-time string hashing
consteval std::size_t hash_string(const char* str) {
    std::size_t hash = 5381;  // djb2 hash algorithm
    std::size_t len = string_length(str);

    for (std::size_t i = 0; i < len; ++i) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(str[i]);
    }

    return hash;
}

// String comparison at compile-time
consteval bool strings_equal(const char* str1, const char* str2) {
    std::size_t len1 = string_length(str1);
    std::size_t len2 = string_length(str2);

    if (len1 != len2) return false;

    for (std::size_t i = 0; i < len1; ++i) {
        if (str1[i] != str2[i]) return false;
    }

    return true;
}

// Convert string to uppercase at compile-time
consteval char to_upper_char(char c) {
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

template<std::size_t N>
consteval std::array<char, N> to_uppercase(const char (&str)[N]) {
    std::array<char, N> result{};
    for (std::size_t i = 0; i < N - 1; ++i) {  // N-1 to exclude null terminator
        result[i] = to_upper_char(str[i]);
    }
    result[N - 1] = '\0';  // Null terminator
    return result;
}

void demonstrate_string_processing() {
    std::cout << "\n=== String Processing and Hashing ===\n";

    // String length calculation
    constexpr std::size_t len1 = string_length("Hello, World!");
    constexpr std::size_t len2 = string_length("C++20 consteval");

    std::cout << "Length of 'Hello, World!': " << len1 << "\n";
    std::cout << "Length of 'C++20 consteval': " << len2 << "\n";

    // String hashing
    constexpr std::size_t hash1 = hash_string("AAPL");
    constexpr std::size_t hash2 = hash_string("GOOGL");
    constexpr std::size_t hash3 = hash_string("MSFT");

    std::cout << "Hash of 'AAPL': " << hash1 << "\n";
    std::cout << "Hash of 'GOOGL': " << hash2 << "\n";
    std::cout << "Hash of 'MSFT': " << hash3 << "\n";

    // String comparison
    constexpr bool equal1 = strings_equal("test", "test");
    constexpr bool equal2 = strings_equal("test", "TEST");

    std::cout << "'test' == 'test': " << std::boolalpha << equal1 << "\n";
    std::cout << "'test' == 'TEST': " << equal2 << "\n";

    // String case conversion
    constexpr auto upper1 = to_uppercase("hello world");
    constexpr auto upper2 = to_uppercase("trading system");

    std::cout << "Uppercase 'hello world': " << upper1.data() << "\n";
    std::cout << "Uppercase 'trading system': " << upper2.data() << "\n";

    std::cout << "All string operations performed at compile-time!\n";
}

// ============================================================================
// 5. FINANCIAL CALCULATIONS
// ============================================================================

// Compound interest calculation
consteval double compound_interest(double principal, double rate, int periods) {
    if (principal <= 0 || rate < 0 || periods <= 0) {
        throw "Invalid parameters for compound interest";
    }

    double result = principal;
    for (int i = 0; i < periods; ++i) {
        result *= (1.0 + rate);
    }
    return result;
}

// Black-Scholes option pricing (simplified)
consteval double black_scholes_call(double spot, double strike, double rate,
                                  double time, double volatility) {
    if (spot <= 0 || strike <= 0 || time <= 0 || volatility <= 0) {
        throw "Invalid Black-Scholes parameters";
    }

    // Simplified calculation (not the full Black-Scholes formula)
    double d1 = (spot - strike) / (volatility * time);
    double intrinsic = (spot > strike) ? spot - strike : 0.0;
    double time_value = volatility * time * 0.4;  // Simplified time value

    return intrinsic + time_value;
}

// Present value calculation
consteval double present_value(double future_value, double rate, int periods) {
    if (rate <= -1.0 || periods < 0) {
        throw "Invalid present value parameters";
    }

    double discount_factor = 1.0;
    for (int i = 0; i < periods; ++i) {
        discount_factor /= (1.0 + rate);
    }

    return future_value * discount_factor;
}

// Bond yield calculation (simplified)
consteval double bond_yield_approx(double price, double face_value,
                                 double coupon_rate, int years_to_maturity) {
    if (price <= 0 || face_value <= 0 || years_to_maturity <= 0) {
        throw "Invalid bond parameters";
    }

    double annual_coupon = face_value * coupon_rate;
    double capital_gain_loss = (face_value - price) / years_to_maturity;
    double average_price = (price + face_value) / 2.0;

    return (annual_coupon + capital_gain_loss) / average_price;
}

void demonstrate_financial_calculations() {
    std::cout << "\n=== Financial Calculations ===\n";

    // Compound interest scenarios
    constexpr double investment_1yr = compound_interest(10000.0, 0.05, 1);    // 5% for 1 year
    constexpr double investment_5yr = compound_interest(10000.0, 0.05, 5);    // 5% for 5 years
    constexpr double investment_10yr = compound_interest(10000.0, 0.07, 10);  // 7% for 10 years

    std::cout << "$10,000 at 5% for 1 year: $" << std::fixed << std::setprecision(2) << investment_1yr << "\n";
    std::cout << "$10,000 at 5% for 5 years: $" << investment_5yr << "\n";
    std::cout << "$10,000 at 7% for 10 years: $" << investment_10yr << "\n";

    // Option pricing
    constexpr double option_price_1 = black_scholes_call(100.0, 105.0, 0.05, 0.25, 0.20);
    constexpr double option_price_2 = black_scholes_call(150.0, 145.0, 0.03, 0.5, 0.25);

    std::cout << "Call option (S=100, K=105): $" << option_price_1 << "\n";
    std::cout << "Call option (S=150, K=145): $" << option_price_2 << "\n";

    // Present value calculations
    constexpr double pv_1 = present_value(1000.0, 0.05, 5);   // $1000 in 5 years at 5%
    constexpr double pv_2 = present_value(50000.0, 0.08, 10); // $50,000 in 10 years at 8%

    std::cout << "PV of $1,000 in 5 years at 5%: $" << pv_1 << "\n";
    std::cout << "PV of $50,000 in 10 years at 8%: $" << pv_2 << "\n";

    // Bond yield calculations
    constexpr double yield_1 = bond_yield_approx(950.0, 1000.0, 0.06, 5);  // 6% coupon, 5 years
    constexpr double yield_2 = bond_yield_approx(1050.0, 1000.0, 0.08, 3); // 8% coupon, 3 years

    std::cout << "Bond yield (price=$950, face=$1000, 6% coupon, 5yr): "
              << (yield_1 * 100) << "%\n";
    std::cout << "Bond yield (price=$1050, face=$1000, 8% coupon, 3yr): "
              << (yield_2 * 100) << "%\n";

    std::cout << "All financial calculations performed at compile-time!\n";
}

// ============================================================================
// 6. ARRAY AND CONTAINER INITIALIZATION
// ============================================================================

// Generate array of squares
template<std::size_t N>
consteval std::array<int, N> generate_squares() {
    std::array<int, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<int>((i + 1) * (i + 1));
    }
    return result;
}

// Generate array of prime numbers
template<std::size_t N>
consteval std::array<int, N> generate_primes() {
    std::array<int, N> result{};
    std::size_t count = 0;
    int candidate = 2;

    while (count < N) {
        if (is_prime(candidate)) {
            result[count] = candidate;
            count++;
        }
        candidate++;
    }

    return result;
}

// Generate Fibonacci sequence
template<std::size_t N>
consteval std::array<long long, N> generate_fibonacci() {
    std::array<long long, N> result{};
    if (N == 0) return result;

    if (N >= 1) result[0] = 0;
    if (N >= 2) result[1] = 1;

    for (std::size_t i = 2; i < N; ++i) {
        result[i] = result[i-1] + result[i-2];
    }

    return result;
}

// Lookup table for trigonometric values (simplified)
template<std::size_t N>
consteval std::array<double, N> generate_sin_table() {
    std::array<double, N> result{};
    constexpr double PI = 3.14159265358979323846;

    for (std::size_t i = 0; i < N; ++i) {
        double angle = (2.0 * PI * i) / N;

        // Taylor series approximation for sin(x)
        double x = angle;
        double sin_x = x;
        double term = x;

        for (int n = 1; n <= 10; ++n) {  // First 10 terms
            term *= -x * x / ((2 * n) * (2 * n + 1));
            sin_x += term;
        }

        result[i] = sin_x;
    }

    return result;
}

void demonstrate_array_initialization() {
    std::cout << "\n=== Array and Container Initialization ===\n";

    // Array of squares
    constexpr auto squares = generate_squares<10>();
    std::cout << "First 10 squares: ";
    for (const auto& square : squares) {
        std::cout << square << " ";
    }
    std::cout << "\n";

    // Array of prime numbers
    constexpr auto primes = generate_primes<15>();
    std::cout << "First 15 primes: ";
    for (const auto& prime : primes) {
        std::cout << prime << " ";
    }
    std::cout << "\n";

    // Fibonacci sequence
    constexpr auto fibonacci = generate_fibonacci<12>();
    std::cout << "First 12 Fibonacci numbers: ";
    for (const auto& fib : fibonacci) {
        std::cout << fib << " ";
    }
    std::cout << "\n";

    // Sine lookup table
    constexpr auto sin_table = generate_sin_table<8>();
    std::cout << "Sine values (8 points on unit circle):\n";
    for (std::size_t i = 0; i < sin_table.size(); ++i) {
        double angle_deg = 360.0 * i / sin_table.size();
        std::cout << "  sin(" << angle_deg << "°) ≈ "
                  << std::fixed << std::setprecision(4) << sin_table[i] << "\n";
    }

    std::cout << "All arrays generated at compile-time!\n";
}

// ============================================================================
// 7. COMPILE-TIME CONFIGURATION AND FEATURE FLAGS
// ============================================================================

enum class LogLevel { DEBUG, INFO, WARN, ERROR };
enum class Environment { DEVELOPMENT, STAGING, PRODUCTION };

struct SystemConfig {
    Environment env;
    LogLevel min_log_level;
    bool enable_debug_features;
    bool enable_profiling;
    int max_concurrent_users;
    double cache_size_mb;
};

consteval SystemConfig get_config_for_environment(Environment env) {
    switch (env) {
        case Environment::DEVELOPMENT:
            return SystemConfig{
                .env = Environment::DEVELOPMENT,
                .min_log_level = LogLevel::DEBUG,
                .enable_debug_features = true,
                .enable_profiling = true,
                .max_concurrent_users = 10,
                .cache_size_mb = 64.0
            };

        case Environment::STAGING:
            return SystemConfig{
                .env = Environment::STAGING,
                .min_log_level = LogLevel::INFO,
                .enable_debug_features = true,
                .enable_profiling = false,
                .max_concurrent_users = 100,
                .cache_size_mb = 256.0
            };

        case Environment::PRODUCTION:
            return SystemConfig{
                .env = Environment::PRODUCTION,
                .min_log_level = LogLevel::WARN,
                .enable_debug_features = false,
                .enable_profiling = false,
                .max_concurrent_users = 10000,
                .cache_size_mb = 1024.0
            };
    }

    throw "Invalid environment";
}

// Feature flag evaluation
consteval bool is_feature_enabled(const char* feature_name, Environment env) {
    // Simplified feature flag logic
    std::size_t feature_hash = hash_string(feature_name);

    // "new_ui" feature
    if (feature_hash == hash_string("new_ui")) {
        return env == Environment::DEVELOPMENT || env == Environment::STAGING;
    }

    // "advanced_analytics" feature
    if (feature_hash == hash_string("advanced_analytics")) {
        return env == Environment::PRODUCTION;
    }

    // "experimental_features" feature
    if (feature_hash == hash_string("experimental_features")) {
        return env == Environment::DEVELOPMENT;
    }

    return false;  // Unknown features are disabled by default
}

consteval const char* environment_name(Environment env) {
    switch (env) {
        case Environment::DEVELOPMENT: return "Development";
        case Environment::STAGING: return "Staging";
        case Environment::PRODUCTION: return "Production";
    }
    return "Unknown";
}

void demonstrate_configuration_feature_flags() {
    std::cout << "\n=== Compile-Time Configuration and Feature Flags ===\n";

    // Configuration for different environments
    constexpr auto dev_config = get_config_for_environment(Environment::DEVELOPMENT);
    constexpr auto staging_config = get_config_for_environment(Environment::STAGING);
    constexpr auto prod_config = get_config_for_environment(Environment::PRODUCTION);

    std::cout << "Development Config:\n";
    std::cout << "  Environment: " << environment_name(dev_config.env) << "\n";
    std::cout << "  Debug features: " << std::boolalpha << dev_config.enable_debug_features << "\n";
    std::cout << "  Max users: " << dev_config.max_concurrent_users << "\n";
    std::cout << "  Cache size: " << dev_config.cache_size_mb << "MB\n";

    std::cout << "\nProduction Config:\n";
    std::cout << "  Environment: " << environment_name(prod_config.env) << "\n";
    std::cout << "  Debug features: " << prod_config.enable_debug_features << "\n";
    std::cout << "  Max users: " << prod_config.max_concurrent_users << "\n";
    std::cout << "  Cache size: " << prod_config.cache_size_mb << "MB\n";

    // Feature flags evaluation
    constexpr bool new_ui_dev = is_feature_enabled("new_ui", Environment::DEVELOPMENT);
    constexpr bool new_ui_prod = is_feature_enabled("new_ui", Environment::PRODUCTION);
    constexpr bool analytics_prod = is_feature_enabled("advanced_analytics", Environment::PRODUCTION);
    constexpr bool experimental_dev = is_feature_enabled("experimental_features", Environment::DEVELOPMENT);

    std::cout << "\nFeature Flags:\n";
    std::cout << "  new_ui (Development): " << std::boolalpha << new_ui_dev << "\n";
    std::cout << "  new_ui (Production): " << new_ui_prod << "\n";
    std::cout << "  advanced_analytics (Production): " << analytics_prod << "\n";
    std::cout << "  experimental_features (Development): " << experimental_dev << "\n";

    std::cout << "All configurations determined at compile-time!\n";
}

// ============================================================================
// 8. CONSTEVAL VS CONSTEXPR COMPARISON
// ============================================================================

// constexpr function - can be called at runtime or compile-time
constexpr int constexpr_factorial(int n) {
    if (n <= 1) return 1;
    return n * constexpr_factorial(n - 1);
}

// consteval function - MUST be called at compile-time
consteval int consteval_factorial(int n) {
    if (n <= 1) return 1;
    return n * consteval_factorial(n - 1);
}

void demonstrate_consteval_vs_constexpr() {
    std::cout << "\n=== consteval vs constexpr Comparison ===\n";

    // constexpr can be used in both contexts
    constexpr int compile_time_constexpr = constexpr_factorial(6);  // Compile-time
    int runtime_value = 7;
    int runtime_constexpr = constexpr_factorial(runtime_value);     // Runtime

    std::cout << "constexpr_factorial(6) = " << compile_time_constexpr << " (compile-time)\n";
    std::cout << "constexpr_factorial(runtime_value) = " << runtime_constexpr << " (runtime)\n";

    // consteval can ONLY be used at compile-time
    constexpr int compile_time_consteval = consteval_factorial(8);  // Compile-time only
    std::cout << "consteval_factorial(8) = " << compile_time_consteval << " (compile-time only)\n";

    // This would cause compilation error:
    // int runtime_consteval = consteval_factorial(runtime_value);  // ERROR!

    std::cout << "\nKey Differences:\n";
    std::cout << "- constexpr: CAN be evaluated at runtime if needed\n";
    std::cout << "- consteval: MUST be evaluated at compile-time\n";
    std::cout << "- consteval provides stronger guarantees for compile-time evaluation\n";
    std::cout << "- consteval eliminates any possibility of runtime overhead\n";
}

// ============================================================================
// 9. ADVANCED CONSTEVAL PATTERNS
// ============================================================================

// Consteval lambda expressions
consteval auto create_multiplier(int factor) {
    return [factor](int x) consteval -> int {
        return x * factor;
    };
}

// Consteval with template parameters
template<int N>
consteval int template_power_of_2() {
    static_assert(N >= 0, "Exponent must be non-negative");
    return 1 << N;  // 2^N using bit shifting
}

// Consteval with concepts (if available)
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
consteval T consteval_abs(T value) {
    return value < T{0} ? -value : value;
}

// Recursive consteval template
template<int N>
consteval int sum_of_first_n() {
    if constexpr (N <= 0) {
        return 0;
    } else {
        return N + sum_of_first_n<N-1>();
    }
}

void demonstrate_advanced_patterns() {
    std::cout << "\n=== Advanced consteval Patterns ===\n";

    // Consteval lambda
    constexpr auto times_3 = create_multiplier(3);
    constexpr int result1 = times_3(7);
    std::cout << "Consteval lambda times_3(7) = " << result1 << "\n";

    // Template consteval
    constexpr int power_2_5 = template_power_of_2<5>();   // 2^5 = 32
    constexpr int power_2_10 = template_power_of_2<10>(); // 2^10 = 1024
    std::cout << "2^5 = " << power_2_5 << "\n";
    std::cout << "2^10 = " << power_2_10 << "\n";

    // Consteval with concepts
    constexpr int abs_neg = consteval_abs(-42);
    constexpr double abs_pos = consteval_abs(3.14);
    std::cout << "consteval_abs(-42) = " << abs_neg << "\n";
    std::cout << "consteval_abs(3.14) = " << abs_pos << "\n";

    // Recursive template consteval
    constexpr int sum_10 = sum_of_first_n<10>();  // 1+2+3+...+10 = 55
    constexpr int sum_20 = sum_of_first_n<20>();  // 1+2+3+...+20 = 210
    std::cout << "Sum of first 10 numbers: " << sum_10 << "\n";
    std::cout << "Sum of first 20 numbers: " << sum_20 << "\n";

    std::cout << "All advanced patterns evaluated at compile-time!\n";
}

// ============================================================================
// 10. PERFORMANCE AND OPTIMIZATION BENEFITS
// ============================================================================

// Large computation that benefits from compile-time evaluation
consteval double compute_expensive_constant() {
    double result = 0.0;

    // Simulate expensive computation
    for (int i = 1; i <= 1000; ++i) {
        result += 1.0 / (i * i);  // Approximates π²/6
    }

    return result;
}

// Table generation for performance-critical code
template<std::size_t TableSize>
consteval std::array<double, TableSize> generate_lookup_table() {
    std::array<double, TableSize> table{};

    for (std::size_t i = 0; i < TableSize; ++i) {
        double x = static_cast<double>(i) / TableSize;

        // Some expensive function - computed once at compile-time
        double value = 0.0;
        for (int n = 0; n < 50; ++n) {
            value += power(x, n) / factorial(n % 10);  // Simplified expensive calculation
        }

        table[i] = value;
    }

    return table;
}

// Configuration-based optimization
consteval bool should_enable_optimization(const char* optimization_name) {
    std::size_t hash = hash_string(optimization_name);

    // Different optimizations based on compile-time configuration
    if (hash == hash_string("fast_math")) return true;
    if (hash == hash_string("vectorization")) return true;
    if (hash == hash_string("loop_unrolling")) return false;  // Disabled for code size

    return false;
}

void demonstrate_performance_benefits() {
    std::cout << "\n=== Performance and Optimization Benefits ===\n";

    // Expensive constant computed once at compile-time
    constexpr double expensive_result = compute_expensive_constant();
    std::cout << "Expensive constant (computed at compile-time): "
              << std::fixed << std::setprecision(6) << expensive_result << "\n";
    std::cout << "This approximates π²/6 ≈ 1.644934\n";

    // Lookup table generated at compile-time
    constexpr auto lookup_table = generate_lookup_table<16>();
    std::cout << "\nLookup table (generated at compile-time):\n";
    for (std::size_t i = 0; i < lookup_table.size(); ++i) {
        if (i % 4 == 0) std::cout << "  ";
        std::cout << std::fixed << std::setprecision(2) << lookup_table[i] << " ";
        if (i % 4 == 3) std::cout << "\n";
    }

    // Optimization flags determined at compile-time
    constexpr bool fast_math = should_enable_optimization("fast_math");
    constexpr bool vectorization = should_enable_optimization("vectorization");
    constexpr bool loop_unroll = should_enable_optimization("loop_unrolling");

    std::cout << "\nOptimization flags (determined at compile-time):\n";
    std::cout << "  Fast math: " << std::boolalpha << fast_math << "\n";
    std::cout << "  Vectorization: " << vectorization << "\n";
    std::cout << "  Loop unrolling: " << loop_unroll << "\n";

    std::cout << "\nBenefits:\n";
    std::cout << "- Zero runtime computation overhead\n";
    std::cout << "- Results stored as compile-time constants\n";
    std::cout << "- Lookup tables pre-computed\n";
    std::cout << "- Configuration decisions made at compile-time\n";
    std::cout << "- Smaller binary size (no computation code)\n";
}

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "C++20 consteval Use Cases and Examples\n";
    std::cout << "======================================\n";

    demonstrate_basic_consteval();
    demonstrate_compile_time_validation();
    demonstrate_mathematical_computations();
    demonstrate_string_processing();
    demonstrate_financial_calculations();
    demonstrate_array_initialization();
    demonstrate_configuration_feature_flags();
    demonstrate_consteval_vs_constexpr();
    demonstrate_advanced_patterns();
    demonstrate_performance_benefits();

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. consteval functions MUST be evaluated at compile-time\n";
    std::cout << "2. Provides stronger guarantees than constexpr\n";
    std::cout << "3. Zero runtime overhead - results are compile-time constants\n";
    std::cout << "4. Perfect for configuration validation and setup\n";
    std::cout << "5. Excellent for mathematical computations and lookup tables\n";
    std::cout << "6. Enables compile-time string processing and hashing\n";
    std::cout << "7. Ideal for financial calculations and risk parameters\n";
    std::cout << "8. Supports advanced patterns: lambdas, templates, concepts\n";
    std::cout << "9. Significant performance benefits for expensive computations\n";
    std::cout << "10. Compile-time feature flags and environment configuration\n";
    std::cout << "11. Better optimization opportunities for compilers\n";
    std::cout << "12. Eliminates runtime errors for compile-time computable values\n";

    return 0;
}

/*
 * Compilation Requirements:
 * - C++20 compatible compiler
 * - GCC 10+, Clang 11+, or MSVC 2019+
 * - Use -std=c++20 flag
 *
 * Example compilation:
 * g++ -std=c++20 -Wall -Wextra -O2 cpp20_consteval_use_cases_examples.cpp -o consteval_demo
 *
 * Key Features of consteval:
 * 1. Immediate functions - must be evaluated at compile-time
 * 2. Cannot be called with runtime values
 * 3. Results are always compile-time constants
 * 4. Stronger than constexpr guarantees
 * 5. Zero runtime overhead
 * 6. Compile-time error if not evaluable at compile-time
 *
 * Common Use Cases:
 * - Configuration validation
 * - Mathematical constants and computations
 * - String processing and hashing
 * - Lookup table generation
 * - Feature flag evaluation
 * - Financial calculations
 * - Array initialization
 * - Performance optimization
 *
 * Benefits over constexpr:
 * - Guaranteed compile-time evaluation
 * - No possibility of runtime fallback
 * - Better optimization opportunities
 * - Clearer intent for compile-time-only functions
 * - Smaller binary size for complex computations
 */
