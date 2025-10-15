//
// Created by Krapa Haritha on 14/10/25.
/*
===========================================================================================
MODERN BRANCH PREDICTION FEATURES AND OPTIMIZATION TECHNIQUES
===========================================================================================

Branch prediction is a critical CPU feature that attempts to guess which way a conditional
jump will go before it's resolved. Modern processors have sophisticated branch predictors
that can significantly impact performance, especially in high-frequency trading systems.

This file covers:
1. Branch prediction fundamentals
2. Modern CPU branch prediction features
3. Branch prediction friendly coding patterns
4. Performance measurement and profiling
5. Compiler hints and optimization techniques
6. Real-world optimization examples
7. Architecture-specific considerations (Intel, AMD, ARM)
8. Advanced techniques for HFT systems

Key Performance Impact:
- Correct prediction: ~1 cycle penalty
- Misprediction: 10-20+ cycle penalty (modern CPUs)
- Critical for tight loops and conditional logic

===========================================================================================
*/

#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <immintrin.h>
#include <cassert>

// ============================================================================
// 1. BRANCH PREDICTION FUNDAMENTALS
// ============================================================================

namespace branch_fundamentals {

// Simple example showing branch prediction impact
void demonstrate_branch_prediction_basics() {
    std::cout << "\n=== BRANCH PREDICTION FUNDAMENTALS ===\n";

    const size_t size = 100000;
    std::vector<int> data(size);

    // Initialize with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }

    // Test 1: Random data (poor branch prediction)
    auto start = std::chrono::high_resolution_clock::now();
    long long sum1 = 0;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] >= 128) {
            sum1 += data[i];
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto random_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test 2: Sorted data (excellent branch prediction)
    std::sort(data.begin(), data.end());

    start = std::chrono::high_resolution_clock::now();
    long long sum2 = 0;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] >= 128) {
            sum2 += data[i];
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto sorted_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Random data time: " << random_time.count() << " μs (sum: " << sum1 << ")\n";
    std::cout << "Sorted data time: " << sorted_time.count() << " μs (sum: " << sum2 << ")\n";
    std::cout << "Speedup ratio: " << static_cast<double>(random_time.count()) / sorted_time.count() << "x\n";
    std::cout << "This demonstrates the massive impact of branch prediction!\n";
}

} // namespace branch_fundamentals

// ============================================================================
// 2. MODERN CPU BRANCH PREDICTION FEATURES
// ============================================================================

namespace modern_features {

/*
Modern CPUs have sophisticated branch prediction mechanisms:

1. Two-Level Adaptive Predictor
   - Global History Buffer (GHB)
   - Pattern History Table (PHT)
   - Branch Target Buffer (BTB)

2. Perceptron-based Predictors
   - Neural network-like prediction
   - Better for complex patterns

3. TAGE (TAgged GEometric) Predictors
   - Multiple predictor tables
   - Different history lengths
   - State-of-the-art accuracy

4. Return Address Stack (RAS)
   - Specialized for function returns
   - Typically 16-32 entries deep

5. Indirect Branch Predictor
   - For function pointers, virtual calls
   - Critical for OOP performance
*/

// Example showing different branch patterns
class BranchPatternDemo {
public:
    // Pattern 1: Highly predictable (always taken)
    static long long always_taken_pattern(const std::vector<int>& data) {
        long long sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            if (true) {  // Always taken
                sum += data[i];
            }
        }
        return sum;
    }

    // Pattern 2: Highly predictable (never taken)
    static long long never_taken_pattern(const std::vector<int>& data) {
        long long sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            if (false) {  // Never taken
                sum -= data[i];
            } else {
                sum += data[i];
            }
        }
        return sum;
    }

    // Pattern 3: Regular pattern (50% taken, but predictable)
    static long long alternating_pattern(const std::vector<int>& data) {
        long long sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            if (i % 2 == 0) {  // Alternating pattern
                sum += data[i];
            } else {
                sum -= data[i];
            }
        }
        return sum;
    }

    // Pattern 4: Complex but learnable pattern
    static long long complex_pattern(const std::vector<int>& data) {
        long long sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            // Pattern: taken every 3rd iteration
            if ((i % 3) == 0) {
                sum += data[i] * 2;
            } else {
                sum += data[i];
            }
        }
        return sum;
    }

    // Pattern 5: Random (worst case for branch prediction)
    static long long random_pattern(const std::vector<int>& data, std::mt19937& gen) {
        long long sum = 0;
        std::uniform_int_distribution<> dis(0, 1);

        for (size_t i = 0; i < data.size(); ++i) {
            if (dis(gen)) {  // Random 50/50
                sum += data[i];
            } else {
                sum -= data[i];
            }
        }
        return sum;
    }
};

void demonstrate_branch_patterns() {
    std::cout << "\n=== MODERN BRANCH PREDICTION PATTERNS ===\n";

    const size_t size = 1000000;
    std::vector<int> data(size, 1);  // Initialize with 1s for consistency

    std::random_device rd;
    std::mt19937 gen(rd());

    auto measure_time = [](auto func, auto... args) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = func(args...);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        return std::make_pair(result, duration.count());
    };

    auto [result1, time1] = measure_time(BranchPatternDemo::always_taken_pattern, std::cref(data));
    auto [result2, time2] = measure_time(BranchPatternDemo::never_taken_pattern, std::cref(data));
    auto [result3, time3] = measure_time(BranchPatternDemo::alternating_pattern, std::cref(data));
    auto [result4, time4] = measure_time(BranchPatternDemo::complex_pattern, std::cref(data));
    auto [result5, time5] = measure_time(BranchPatternDemo::random_pattern, std::cref(data), std::ref(gen));

    std::cout << "Always taken pattern:  " << time1 << " ns\n";
    std::cout << "Never taken pattern:   " << time2 << " ns\n";
    std::cout << "Alternating pattern:   " << time3 << " ns\n";
    std::cout << "Complex pattern:       " << time4 << " ns\n";
    std::cout << "Random pattern:        " << time5 << " ns\n";
    std::cout << "\nRandom vs Always ratio: " << static_cast<double>(time5) / time1 << "x slower\n";
}

} // namespace modern_features

// ============================================================================
// 3. COMPILER HINTS AND OPTIMIZATION TECHNIQUES
// ============================================================================

namespace compiler_hints {

// GCC/Clang branch prediction hints
#ifdef __GNUC__
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

// C++20 [[likely]] and [[unlikely]] attributes
#if __cplusplus >= 202002L
#define CPP20_LIKELY   [[likely]]
#define CPP20_UNLIKELY [[unlikely]]
#else
#define CPP20_LIKELY
#define CPP20_UNLIKELY
#endif

// Example: Error handling with branch hints
class ErrorHandlingExample {
public:
    enum class ErrorCode {
        SUCCESS = 0,
        INVALID_INPUT,
        NETWORK_ERROR,
        TIMEOUT
    };

    // Traditional approach (no hints)
    static ErrorCode process_data_traditional(int value) {
        if (value < 0) {
            return ErrorCode::INVALID_INPUT;
        }

        if (value > 1000000) {
            return ErrorCode::TIMEOUT;
        }

        // Normal processing
        return ErrorCode::SUCCESS;
    }

    // With compiler hints (GCC/Clang)
    static ErrorCode process_data_with_hints(int value) {
        if (UNLIKELY(value < 0)) {
            return ErrorCode::INVALID_INPUT;
        }

        if (UNLIKELY(value > 1000000)) {
            return ErrorCode::TIMEOUT;
        }

        // Normal processing (most likely path)
        return ErrorCode::SUCCESS;
    }

    // C++20 approach
    static ErrorCode process_data_cpp20(int value) {
        if (value < 0) CPP20_UNLIKELY {
            return ErrorCode::INVALID_INPUT;
        }

        if (value > 1000000) CPP20_UNLIKELY {
            return ErrorCode::TIMEOUT;
        }

        // Normal processing
        return ErrorCode::SUCCESS;
    }
};

// Profile-Guided Optimization (PGO) demonstration
class PGOExample {
public:
    // Function that benefits from PGO
    static int classify_value(int value) {
        // In real usage, these branches have different frequencies
        if (value < 10) {        // 5% of cases
            return 1;
        } else if (value < 100) { // 15% of cases
            return 2;
        } else if (value < 1000) { // 70% of cases (hot path)
            return 3;
        } else {                 // 10% of cases
            return 4;
        }
    }
};

void demonstrate_compiler_hints() {
    std::cout << "\n=== COMPILER HINTS AND OPTIMIZATION ===\n";

    const size_t iterations = 10000000;
    std::vector<int> test_data;

    // Generate test data where errors are rare (realistic scenario)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000);

    for (size_t i = 0; i < iterations; ++i) {
        int value = dis(gen);
        // Make errors rare (5% chance)
        if (i % 20 == 0) {
            value = -1;  // Error case
        }
        test_data.push_back(value);
    }

    // Benchmark traditional approach
    auto start = std::chrono::high_resolution_clock::now();
    size_t error_count1 = 0;
    for (int value : test_data) {
        if (ErrorHandlingExample::process_data_traditional(value) != ErrorHandlingExample::ErrorCode::SUCCESS) {
            error_count1++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto traditional_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Benchmark with hints
    start = std::chrono::high_resolution_clock::now();
    size_t error_count2 = 0;
    for (int value : test_data) {
        if (ErrorHandlingExample::process_data_with_hints(value) != ErrorHandlingExample::ErrorCode::SUCCESS) {
            error_count2++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto hints_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Traditional approach: " << traditional_time.count() << " μs (errors: " << error_count1 << ")\n";
    std::cout << "With branch hints:    " << hints_time.count() << " μs (errors: " << error_count2 << ")\n";
    std::cout << "Improvement: " << static_cast<double>(traditional_time.count()) / hints_time.count() << "x\n";

    std::cout << "\nCompiler optimization notes:\n";
    std::cout << "1. Use __builtin_expect for GCC/Clang\n";
    std::cout << "2. Use [[likely]]/[[unlikely]] for C++20\n";
    std::cout << "3. Profile-Guided Optimization (PGO) is most effective\n";
    std::cout << "4. Modern CPUs learn patterns automatically\n";
}

} // namespace compiler_hints

// ============================================================================
// 4. BRANCHLESS PROGRAMMING TECHNIQUES
// ============================================================================

namespace branchless_techniques {

// Technique 1: Conditional moves
class ConditionalMoves {
public:
    // Branchy version
    static int max_branchy(int a, int b) {
        if (a > b) {
            return a;
        } else {
            return b;
        }
    }

    // Branchless version using conditional move
    static int max_branchless(int a, int b) {
        return a > b ? a : b;  // Compiler often generates CMOV
    }

    // Explicit branchless using bit manipulation
    static int max_bitwise(int a, int b) {
        int diff = a - b;
        int sign = (diff >> 31) & 1;  // 1 if a < b, 0 if a >= b
        return a - sign * diff;
    }
};

// Technique 2: Lookup tables
class LookupTables {
public:
    // Branchy classification
    static char classify_branchy(int value) {
        if (value < 0) return 'N';
        if (value == 0) return 'Z';
        if (value < 10) return 'S';
        if (value < 100) return 'M';
        return 'L';
    }

    // Branchless using lookup table
    static char classify_lut(int value) {
        static const char lut[] = {
            'N', 'N', 'N', 'N',  // < 0 (approximation)
            'Z',                 // == 0
            'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S',  // 1-9
            'M', 'M', 'M', 'M', 'M', 'M', 'M', 'M', 'M', 'M'  // 10-99 (partial)
        };

        if (value < 0) return 'N';
        if (value >= 100) return 'L';
        return lut[std::min(value, 19)];
    }
};

// Technique 3: SIMD for parallel branchless operations
class SIMDBranchless {
public:
    // Vectorized conditional operation
    static void conditional_add_simd(const float* a, const float* b, float* result,
                                   const float* condition, size_t size) {
        for (size_t i = 0; i < size; i += 8) {
            __m256 va = _mm256_load_ps(&a[i]);
            __m256 vb = _mm256_load_ps(&b[i]);
            __m256 vcond = _mm256_load_ps(&condition[i]);
            __m256 vzero = _mm256_setzero_ps();

            // Branchless: if condition > 0, add a + b, else just a
            __m256 mask = _mm256_cmp_ps(vcond, vzero, _CMP_GT_OQ);
            __m256 sum = _mm256_add_ps(va, vb);
            __m256 vresult = _mm256_blendv_ps(va, sum, mask);

            _mm256_store_ps(&result[i], vresult);
        }
    }
};

// Technique 4: Predication for complex conditions
class PredicationTechniques {
public:
    // Complex branchy function
    static int complex_branchy(int x, int y, int z) {
        int result = 0;
        if (x > 0) {
            result += x * 2;
            if (y > x) {
                result += y;
                if (z > y) {
                    result += z * 3;
                }
            }
        }
        return result;
    }

    // Branchless equivalent using predication
    static int complex_branchless(int x, int y, int z) {
        int result = 0;
        int x_pos = (x > 0);
        int y_gt_x = (y > x);
        int z_gt_y = (z > y);

        result += x_pos * (x * 2);
        result += x_pos * y_gt_x * y;
        result += x_pos * y_gt_x * z_gt_y * (z * 3);

        return result;
    }
};

void demonstrate_branchless_techniques() {
    std::cout << "\n=== BRANCHLESS PROGRAMMING TECHNIQUES ===\n";

    const size_t size = 1000000;
    std::vector<int> data1(size), data2(size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(-100, 100);

    for (size_t i = 0; i < size; ++i) {
        data1[i] = dis(gen);
        data2[i] = dis(gen);
    }

    // Benchmark conditional moves
    auto start = std::chrono::high_resolution_clock::now();
    long long sum1 = 0;
    for (size_t i = 0; i < size; ++i) {
        sum1 += ConditionalMoves::max_branchy(data1[i], data2[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto branchy_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    long long sum2 = 0;
    for (size_t i = 0; i < size; ++i) {
        sum2 += ConditionalMoves::max_branchless(data1[i], data2[i]);
    }
    end = std::chrono::high_resolution_clock::now();
    auto branchless_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Branchy max function:    " << branchy_time.count() << " μs (sum: " << sum1 << ")\n";
    std::cout << "Branchless max function: " << branchless_time.count() << " μs (sum: " << sum2 << ")\n";
    std::cout << "Speedup: " << static_cast<double>(branchy_time.count()) / branchless_time.count() << "x\n";

    // Benchmark complex conditions
    start = std::chrono::high_resolution_clock::now();
    long long sum3 = 0;
    for (size_t i = 0; i < size - 2; ++i) {
        sum3 += PredicationTechniques::complex_branchy(data1[i], data1[i+1], data1[i+2]);
    }
    end = std::chrono::high_resolution_clock::now();
    auto complex_branchy_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    long long sum4 = 0;
    for (size_t i = 0; i < size - 2; ++i) {
        sum4 += PredicationTechniques::complex_branchless(data1[i], data1[i+1], data1[i+2]);
    }
    end = std::chrono::high_resolution_clock::now();
    auto complex_branchless_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "\nComplex branchy function:    " << complex_branchy_time.count() << " μs (sum: " << sum3 << ")\n";
    std::cout << "Complex branchless function: " << complex_branchless_time.count() << " μs (sum: " << sum4 << ")\n";
    std::cout << "Speedup: " << static_cast<double>(complex_branchy_time.count()) / complex_branchless_time.count() << "x\n";
}

} // namespace branchless_techniques

// ============================================================================
// 5. REAL-WORLD OPTIMIZATION EXAMPLES
// ============================================================================

namespace real_world_examples {

// Example 1: FX Trading Price Processing
class FXPriceProcessor {
public:
    struct Price {
        double bid;
        double ask;
        uint64_t timestamp;
        bool is_valid;
    };

    // Branchy version with multiple validations
    static bool validate_price_branchy(const Price& price) {
        if (!price.is_valid) {
            return false;
        }

        if (price.bid <= 0.0 || price.ask <= 0.0) {
            return false;
        }

        if (price.ask <= price.bid) {
            return false;
        }

        double spread = price.ask - price.bid;
        if (spread > price.bid * 0.1) {  // 10% spread limit
            return false;
        }

        return true;
    }

    // Optimized version with early exit optimization
    static bool validate_price_optimized(const Price& price) {
        // Most likely to fail first - put it first
        if (UNLIKELY(!price.is_valid)) {
            return false;
        }

        // Combine conditions to reduce branches
        if (UNLIKELY(price.bid <= 0.0 || price.ask <= price.bid)) {
            return false;
        }

        // Less common check last
        double spread = price.ask - price.bid;
        if (UNLIKELY(spread > price.bid * 0.1)) {
            return false;
        }

        return true;
    }

    // Branchless version for hot paths
    static bool validate_price_branchless(const Price& price) {
        bool valid = price.is_valid;
        valid &= (price.bid > 0.0);
        valid &= (price.ask > price.bid);
        valid &= ((price.ask - price.bid) <= (price.bid * 0.1));

        return valid;
    }
};

// Example 2: Market Data Classification
class MarketDataClassifier {
public:
    enum class MessageType {
        TRADE = 0,
        QUOTE = 1,
        HEARTBEAT = 2,
        STATUS = 3,
        UNKNOWN = 4
    };

    // Branchy classification
    static MessageType classify_branchy(char message_type) {
        if (message_type == 'T') {
            return MessageType::TRADE;
        } else if (message_type == 'Q') {
            return MessageType::QUOTE;
        } else if (message_type == 'H') {
            return MessageType::HEARTBEAT;
        } else if (message_type == 'S') {
            return MessageType::STATUS;
        } else {
            return MessageType::UNKNOWN;
        }
    }

    // Lookup table version (branchless)
    static MessageType classify_lut(char message_type) {
        static const MessageType lookup[256] = {
            // Initialize all to UNKNOWN
            [0 ... 255] = MessageType::UNKNOWN,
            ['T'] = MessageType::TRADE,
            ['Q'] = MessageType::QUOTE,
            ['H'] = MessageType::HEARTBEAT,
            ['S'] = MessageType::STATUS
        };

        return lookup[static_cast<unsigned char>(message_type)];
    }
};

// Example 3: Risk Management with Multiple Conditions
class RiskManager {
public:
    struct Position {
        double size;
        double entry_price;
        double current_price;
        double max_loss_limit;
        bool is_long;
    };

    // Complex risk calculation with many branches
    static bool should_close_position_branchy(const Position& pos) {
        if (pos.size == 0.0) {
            return false;
        }

        double pnl;
        if (pos.is_long) {
            pnl = (pos.current_price - pos.entry_price) * pos.size;
        } else {
            pnl = (pos.entry_price - pos.current_price) * pos.size;
        }

        if (pnl < pos.max_loss_limit) {
            return true;
        }

        // Additional risk checks
        double price_move_pct = std::abs(pos.current_price - pos.entry_price) / pos.entry_price;
        if (price_move_pct > 0.05) {  // 5% move
            return true;
        }

        return false;
    }

    // Optimized version with reduced branches
    static bool should_close_position_optimized(const Position& pos) {
        if (UNLIKELY(pos.size == 0.0)) {
            return false;
        }

        // Branchless PnL calculation
        double price_diff = pos.current_price - pos.entry_price;
        double pnl = pos.is_long ? price_diff * pos.size : -price_diff * pos.size;

        // Combined conditions
        bool loss_limit_hit = (pnl < pos.max_loss_limit);
        bool large_move = (std::abs(price_diff) / pos.entry_price > 0.05);

        return loss_limit_hit || large_move;
    }
};

void demonstrate_real_world_examples() {
    std::cout << "\n=== REAL-WORLD OPTIMIZATION EXAMPLES ===\n";

    const size_t size = 1000000;
    std::vector<FXPriceProcessor::Price> prices;
    std::vector<char> message_types;
    std::vector<RiskManager::Position> positions;

    // Generate test data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_dis(1.0, 2.0);
    std::uniform_int_distribution<> type_dis(0, 4);
    std::uniform_int_distribution<> bool_dis(0, 1);

    for (size_t i = 0; i < size; ++i) {
        // Price data
        double bid = price_dis(gen);
        prices.push_back({
            bid,
            bid + 0.0001 + (i % 100) * 0.00001,  // Small spread variation
            static_cast<uint64_t>(i),
            i % 20 != 0  // 95% valid
        });

        // Message types
        const char types[] = {'T', 'Q', 'H', 'S', 'X'};
        message_types.push_back(types[type_dis(gen)]);

        // Positions
        double entry = price_dis(gen);
        positions.push_back({
            100.0,
            entry,
            entry * (0.95 + 0.1 * static_cast<double>(i % 100) / 100.0),
            -500.0,
            bool_dis(gen) == 1
        });
    }

    // Benchmark price validation
    auto start = std::chrono::high_resolution_clock::now();
    size_t valid_count1 = 0;
    for (const auto& price : prices) {
        if (FXPriceProcessor::validate_price_branchy(price)) {
            valid_count1++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto branchy_price_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    size_t valid_count2 = 0;
    for (const auto& price : prices) {
        if (FXPriceProcessor::validate_price_optimized(price)) {
            valid_count2++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto optimized_price_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Price validation (branchy):    " << branchy_price_time.count() << " μs (valid: " << valid_count1 << ")\n";
    std::cout << "Price validation (optimized):  " << optimized_price_time.count() << " μs (valid: " << valid_count2 << ")\n";
    std::cout << "Speedup: " << static_cast<double>(branchy_price_time.count()) / optimized_price_time.count() << "x\n";

    // Benchmark message classification
    start = std::chrono::high_resolution_clock::now();
    size_t trade_count1 = 0;
    for (char type : message_types) {
        if (MarketDataClassifier::classify_branchy(type) == MarketDataClassifier::MessageType::TRADE) {
            trade_count1++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto branchy_msg_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    size_t trade_count2 = 0;
    for (char type : message_types) {
        if (MarketDataClassifier::classify_lut(type) == MarketDataClassifier::MessageType::TRADE) {
            trade_count2++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto lut_msg_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "\nMessage classification (branchy): " << branchy_msg_time.count() << " μs (trades: " << trade_count1 << ")\n";
    std::cout << "Message classification (LUT):     " << lut_msg_time.count() << " μs (trades: " << trade_count2 << ")\n";
    std::cout << "Speedup: " << static_cast<double>(branchy_msg_time.count()) / lut_msg_time.count() << "x\n";
}

} // namespace real_world_examples

// ============================================================================
// 6. PERFORMANCE PROFILING AND MEASUREMENT
// ============================================================================

namespace performance_profiling {

// Hardware performance counter simulation (simplified)
class BranchProfiler {
public:
    struct BranchStats {
        uint64_t total_branches = 0;
        uint64_t mispredicted_branches = 0;

        double misprediction_rate() const {
            return total_branches > 0 ?
                static_cast<double>(mispredicted_branches) / total_branches * 100.0 : 0.0;
        }
    };

    // Simulate branch prediction accuracy
    static BranchStats profile_function(std::function<void()> func,
                                      const std::string& pattern_type) {
        BranchStats stats;

        // Simplified simulation based on pattern type
        if (pattern_type == "random") {
            stats.total_branches = 1000000;
            stats.mispredicted_branches = 500000;  // 50% misprediction
        } else if (pattern_type == "predictable") {
            stats.total_branches = 1000000;
            stats.mispredicted_branches = 10000;   // 1% misprediction
        } else if (pattern_type == "alternating") {
            stats.total_branches = 1000000;
            stats.mispredicted_branches = 50000;   // 5% misprediction
        }

        // Run the function
        func();

        return stats;
    }
};

void demonstrate_performance_profiling() {
    std::cout << "\n=== PERFORMANCE PROFILING TECHNIQUES ===\n";

    std::cout << "Branch prediction profiling methods:\n";
    std::cout << "1. Hardware Performance Counters (perf on Linux)\n";
    std::cout << "2. Intel VTune Profiler\n";
    std::cout << "3. CPU vendor-specific tools\n";
    std::cout << "4. Compiler optimization reports\n";
    std::cout << "\nExample perf commands:\n";
    std::cout << "  perf stat -e branches,branch-misses ./your_program\n";
    std::cout << "  perf record -e branch-misses ./your_program\n";
    std::cout << "  perf report\n";

    std::cout << "\nSimulated branch prediction analysis:\n";

    auto random_func = []() {
        // Simulate random branching
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1);
        volatile int sum = 0;
        for (int i = 0; i < 10000; ++i) {
            if (dis(gen)) {
                sum += i;
            }
        }
    };

    auto predictable_func = []() {
        // Simulate predictable branching
        volatile int sum = 0;
        for (int i = 0; i < 10000; ++i) {
            if (i % 10 == 0) {  // Predictable pattern
                sum += i;
            }
        }
    };

    auto stats1 = BranchProfiler::profile_function(random_func, "random");
    auto stats2 = BranchProfiler::profile_function(predictable_func, "predictable");

    std::cout << "Random pattern - Misprediction rate: " << stats1.misprediction_rate() << "%\n";
    std::cout << "Predictable pattern - Misprediction rate: " << stats2.misprediction_rate() << "%\n";
}

} // namespace performance_profiling

// ============================================================================
// 7. ARCHITECTURE-SPECIFIC CONSIDERATIONS
// ============================================================================

namespace architecture_specific {

void demonstrate_architecture_considerations() {
    std::cout << "\n=== ARCHITECTURE-SPECIFIC CONSIDERATIONS ===\n";

    std::cout << "Intel x86-64 Branch Prediction:\n";
    std::cout << "- Two-level adaptive predictor\n";
    std::cout << "- 4K entry BTB (Branch Target Buffer)\n";
    std::cout << "- 16-32 entry RAS (Return Address Stack)\n";
    std::cout << "- ~95% accuracy for typical workloads\n";
    std::cout << "- 15-20 cycle misprediction penalty\n";

    std::cout << "\nAMD x86-64 Branch Prediction:\n";
    std::cout << "- Perceptron-based predictor (Zen architecture)\n";
    std::cout << "- Enhanced BTB with multiple levels\n";
    std::cout << "- Improved indirect branch prediction\n";
    std::cout << "- Similar performance characteristics to Intel\n";

    std::cout << "\nARM AArch64 Branch Prediction:\n";
    std::cout << "- Implementation-specific designs\n";
    std::cout << "- Cortex-A series: Advanced predictors\n";
    std::cout << "- Lower misprediction penalties (10-15 cycles)\n";
    std::cout << "- Energy-efficient prediction\n";

    std::cout << "\nOptimization Guidelines by Architecture:\n";
    std::cout << "1. x86-64: Focus on reducing branch count\n";
    std::cout << "2. ARM: Balance performance vs power\n";
    std::cout << "3. All: Profile with actual hardware\n";
    std::cout << "4. All: Consider micro-architecture differences\n";
}

} // namespace architecture_specific

// ============================================================================
// 8. ADVANCED TECHNIQUES FOR HFT SYSTEMS
// ============================================================================

namespace hft_techniques {

// Technique 1: Loop unrolling to reduce branch overhead
class LoopOptimizations {
public:
    // Standard loop with branch
    static double calculate_moving_average_standard(const double* prices, size_t count) {
        double sum = 0.0;
        for (size_t i = 0; i < count; ++i) {
            sum += prices[i];
        }
        return sum / count;
    }

    // Unrolled loop (compiler usually does this automatically)
    static double calculate_moving_average_unrolled(const double* prices, size_t count) {
        double sum = 0.0;
        size_t i = 0;

        // Process 4 elements at a time
        for (; i + 3 < count; i += 4) {
            sum += prices[i] + prices[i+1] + prices[i+2] + prices[i+3];
        }

        // Handle remaining elements
        for (; i < count; ++i) {
            sum += prices[i];
        }

        return sum / count;
    }
};

// Technique 2: Avoiding function pointers in hot paths
class FunctionPointerOptimization {
public:
    using ProcessorFunc = double(*)(double);

    static double identity(double x) { return x; }
    static double square(double x) { return x * x; }
    static double sqrt_func(double x) { return std::sqrt(x); }

    // Function pointer version (indirect branch)
    static double process_with_function_pointer(const double* data, size_t size, ProcessorFunc func) {
        double result = 0.0;
        for (size_t i = 0; i < size; ++i) {
            result += func(data[i]);  // Indirect branch
        }
        return result;
    }

    // Template version (direct calls)
    template<typename Func>
    static double process_with_template(const double* data, size_t size, Func func) {
        double result = 0.0;
        for (size_t i = 0; i < size; ++i) {
            result += func(data[i]);  // Direct call, inlined
        }
        return result;
    }
};

// Technique 3: SIMD with reduced branching
class SIMDOptimizations {
public:
    // Vectorized price filtering
    static void filter_prices_simd(const float* input, float* output,
                                 float threshold, size_t size) {
        const __m256 vthreshold = _mm256_set1_ps(threshold);

        for (size_t i = 0; i < size; i += 8) {
            __m256 vinput = _mm256_load_ps(&input[i]);
            __m256 mask = _mm256_cmp_ps(vinput, vthreshold, _CMP_GT_OQ);
            __m256 result = _mm256_and_ps(vinput, mask);
            _mm256_store_ps(&output[i], result);
        }
    }
};

void demonstrate_hft_techniques() {
    std::cout << "\n=== ADVANCED HFT OPTIMIZATION TECHNIQUES ===\n";

    const size_t size = 1000000;
    std::vector<double> prices(size);

    // Initialize with realistic price data
    double base_price = 1.05;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> price_noise(0.0, 0.0001);

    for (size_t i = 0; i < size; ++i) {
        prices[i] = base_price + price_noise(gen);
    }

    // Benchmark loop unrolling
    auto start = std::chrono::high_resolution_clock::now();
    double avg1 = LoopOptimizations::calculate_moving_average_standard(prices.data(), size);
    auto end = std::chrono::high_resolution_clock::now();
    auto standard_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    double avg2 = LoopOptimizations::calculate_moving_average_unrolled(prices.data(), size);
    end = std::chrono::high_resolution_clock::now();
    auto unrolled_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << "Standard loop:   " << standard_time.count() << " ns (avg: " << avg1 << ")\n";
    std::cout << "Unrolled loop:   " << unrolled_time.count() << " ns (avg: " << avg2 << ")\n";
    std::cout << "Unrolling speedup: " << static_cast<double>(standard_time.count()) / unrolled_time.count() << "x\n";

    // Benchmark function pointers vs templates
    start = std::chrono::high_resolution_clock::now();
    double result1 = FunctionPointerOptimization::process_with_function_pointer(
        prices.data(), size, FunctionPointerOptimization::identity);
    end = std::chrono::high_resolution_clock::now();
    auto func_ptr_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    double result2 = FunctionPointerOptimization::process_with_template(
        prices.data(), size, [](double x) { return x; });
    end = std::chrono::high_resolution_clock::now();
    auto template_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << "\nFunction pointer: " << func_ptr_time.count() << " ns (result: " << result1 << ")\n";
    std::cout << "Template inline:  " << template_time.count() << " ns (result: " << result2 << ")\n";
    std::cout << "Template speedup: " << static_cast<double>(func_ptr_time.count()) / template_time.count() << "x\n";

    std::cout << "\nKey HFT optimization principles:\n";
    std::cout << "1. Minimize indirect branches (virtual calls, function pointers)\n";
    std::cout << "2. Use templates for compile-time polymorphism\n";
    std::cout << "3. Profile on target hardware\n";
    std::cout << "4. Consider CPU pipeline characteristics\n";
    std::cout << "5. Use SIMD for data-parallel operations\n";
}

} // namespace hft_techniques

// ============================================================================
// MAIN DEMONSTRATION FUNCTION
// ============================================================================

int main() {
    std::cout << "MODERN BRANCH PREDICTION FEATURES AND OPTIMIZATION\n";
    std::cout << "==================================================\n";

    // Demonstrate all branch prediction concepts
    branch_fundamentals::demonstrate_branch_prediction_basics();
    modern_features::demonstrate_branch_patterns();
    compiler_hints::demonstrate_compiler_hints();
    branchless_techniques::demonstrate_branchless_techniques();
    real_world_examples::demonstrate_real_world_examples();
    performance_profiling::demonstrate_performance_profiling();
    architecture_specific::demonstrate_architecture_considerations();
    hft_techniques::demonstrate_hft_techniques();

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Key branch prediction optimization strategies:\n";
    std::cout << "1. Understand your CPU's branch predictor capabilities\n";
    std::cout << "2. Profile branch misprediction rates in real workloads\n";
    std::cout << "3. Use compiler hints (__builtin_expect, [[likely]])\n";
    std::cout << "4. Consider branchless algorithms for hot paths\n";
    std::cout << "5. Optimize for common cases (error handling)\n";
    std::cout << "6. Use lookup tables for complex classifications\n";
    std::cout << "7. Leverage SIMD for parallel conditional operations\n";
    std::cout << "8. Minimize indirect branches in performance-critical code\n";
    std::cout << "9. Test optimizations on target hardware\n";
    std::cout << "10. Consider Profile-Guided Optimization (PGO)\n";

    return 0;
}

/*
===========================================================================================
COMPILATION AND TESTING NOTES
===========================================================================================

Compile with optimizations:
g++ -std=c++17 -O3 -march=native -mavx2 branch_prediction_examples.cpp -o branch_demo

For profiling branch mispredictions (Linux):
perf stat -e branches,branch-misses ./branch_demo

For detailed analysis:
perf record -e branch-misses -g ./branch_demo
perf report

Expected results:
- Sorted data should be 2-5x faster than random data
- Branchless techniques show improvement with unpredictable data
- Compiler hints provide modest improvements (5-15%)
- SIMD operations eliminate branch overhead entirely
- Function pointers show 2-3x overhead vs templates in tight loops

Key takeaways:
1. Branch prediction has massive performance impact (10-20 cycle penalty)
2. Modern CPUs are very good at learning patterns
3. Random/unpredictable branches are the worst case
4. Branchless programming is valuable for hot paths
5. Profile-guided optimization is most effective
6. Architecture-specific tuning may be necessary for extreme performance

===========================================================================================
*/
