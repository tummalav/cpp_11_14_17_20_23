#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <execution>
#include <immintrin.h>  // For Intel SIMD intrinsics
#include <xmmintrin.h>  // For SSE
#include <emmintrin.h>  // For SSE2
#include <pmmintrin.h>  // For SSE3
#include <smmintrin.h>  // For SSE4.1
#include <nmmintrin.h>  // For SSE4.2
#include <cstring>
#include <cmath>

/*
 * SIMD (Single Instruction, Multiple Data) Comprehensive Guide
 *
 * WHAT IS SIMD?
 * SIMD is a parallel computing technique where a single instruction operates
 * on multiple data elements simultaneously. Modern CPUs have dedicated SIMD
 * units that can process multiple values in one operation.
 *
 * SIMD INSTRUCTION SETS (Intel/AMD):
 * - MMX (1997): 64-bit registers, integer operations
 * - SSE (1999): 128-bit registers, 4 floats or 2 doubles
 * - SSE2 (2001): Added integer operations to SSE
 * - SSE3 (2004): Horizontal operations, complex number support
 * - SSSE3 (2006): Additional packed integer operations
 * - SSE4.1/4.2 (2006/2008): String processing, improved operations
 * - AVX (2011): 256-bit registers, 8 floats or 4 doubles
 * - AVX2 (2013): Added integer operations to AVX
 * - AVX-512 (2015): 512-bit registers, 16 floats or 8 doubles
 *
 * ARM SIMD:
 * - NEON: ARM's SIMD technology, 128-bit registers
 * - SVE: Scalable Vector Extension, variable-length vectors
 *
 * BENEFITS:
 * - Higher throughput for data-parallel operations
 * - Better performance per watt
 * - Reduced instruction count
 * - Hardware-level parallelism
 */

namespace simd_examples {

// Utility functions for timing
auto get_current_time() {
    return std::chrono::high_resolution_clock::now();
}

template<typename TimePoint>
double get_duration_ms(TimePoint start, TimePoint end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

template<typename Func>
auto measure_performance(const std::string& name, Func&& func) {
    auto start = get_current_time();
    auto result = func();
    auto end = get_current_time();
    std::cout << name << ": " << get_duration_ms(start, end) << " ms\n";
    return result;
}

// ===========================================================================================
// 1. BASIC SIMD CONCEPTS AND MANUAL VECTORIZATION
// ===========================================================================================

class BasicSIMDExamples {
public:
    // Example 1: Vector Addition - Scalar vs SIMD
    static void vector_addition_comparison() {
        std::cout << "\n=== Vector Addition: Scalar vs SIMD ===\n";

        const size_t size = 1'000'000;
        std::vector<float> a(size), b(size), result_scalar(size), result_simd(size);

        // Initialize with random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-100.0f, 100.0f);

        std::generate(a.begin(), a.end(), [&]() { return dis(gen); });
        std::generate(b.begin(), b.end(), [&]() { return dis(gen); });

        // Scalar version
        measure_performance("Scalar addition", [&]() {
            for (size_t i = 0; i < size; ++i) {
                result_scalar[i] = a[i] + b[i];
            }
            return result_scalar[0];
        });

        // SIMD version using AVX (8 floats at once)
        measure_performance("SIMD addition (AVX)", [&]() {
            size_t simd_size = size - (size % 8); // Process in chunks of 8

            for (size_t i = 0; i < simd_size; i += 8) {
                __m256 va = _mm256_loadu_ps(&a[i]);
                __m256 vb = _mm256_loadu_ps(&b[i]);
                __m256 vresult = _mm256_add_ps(va, vb);
                _mm256_storeu_ps(&result_simd[i], vresult);
            }

            // Handle remaining elements
            for (size_t i = simd_size; i < size; ++i) {
                result_simd[i] = a[i] + b[i];
            }

            return result_simd[0];
        });

        // Verify results
        bool correct = std::equal(result_scalar.begin(), result_scalar.end(),
                                 result_simd.begin(),
                                 [](float a, float b) { return std::abs(a - b) < 0.001f; });
        std::cout << "Results match: " << (correct ? "YES" : "NO") << "\n";
    }

    // Example 2: Dot Product - Scalar vs SIMD
    static void dot_product_comparison() {
        std::cout << "\n=== Dot Product: Scalar vs SIMD ===\n";

        const size_t size = 1'000'000;
        std::vector<float> a(size), b(size);

        // Initialize vectors
        std::iota(a.begin(), a.end(), 1.0f);
        std::iota(b.begin(), b.end(), 2.0f);

        // Scalar version
        float scalar_result = measure_performance("Scalar dot product", [&]() {
            float sum = 0.0f;
            for (size_t i = 0; i < size; ++i) {
                sum += a[i] * b[i];
            }
            return sum;
        });

        // SIMD version using AVX
        float simd_result = measure_performance("SIMD dot product (AVX)", [&]() {
            __m256 sum_vec = _mm256_setzero_ps();
            size_t simd_size = size - (size % 8);

            for (size_t i = 0; i < simd_size; i += 8) {
                __m256 va = _mm256_loadu_ps(&a[i]);
                __m256 vb = _mm256_loadu_ps(&b[i]);
                __m256 prod = _mm256_mul_ps(va, vb);
                sum_vec = _mm256_add_ps(sum_vec, prod);
            }

            // Horizontal sum of the vector
            float result[8];
            _mm256_storeu_ps(result, sum_vec);
            float sum = result[0] + result[1] + result[2] + result[3] +
                       result[4] + result[5] + result[6] + result[7];

            // Handle remaining elements
            for (size_t i = simd_size; i < size; ++i) {
                sum += a[i] * b[i];
            }

            return sum;
        });

        std::cout << "Scalar result: " << scalar_result << "\n";
        std::cout << "SIMD result: " << simd_result << "\n";
        std::cout << "Difference: " << std::abs(scalar_result - simd_result) << "\n";
    }

    // Example 3: Matrix-Vector Multiplication
    static void matrix_vector_multiply() {
        std::cout << "\n=== Matrix-Vector Multiplication: SIMD Optimization ===\n";

        const size_t rows = 1000, cols = 1000;
        std::vector<std::vector<float>> matrix(rows, std::vector<float>(cols));
        std::vector<float> vector(cols), result_scalar(rows), result_simd(rows);

        // Initialize with random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (auto& row : matrix) {
            std::generate(row.begin(), row.end(), [&]() { return dis(gen); });
        }
        std::generate(vector.begin(), vector.end(), [&]() { return dis(gen); });

        // Scalar version
        measure_performance("Scalar matrix-vector multiply", [&]() {
            for (size_t i = 0; i < rows; ++i) {
                float sum = 0.0f;
                for (size_t j = 0; j < cols; ++j) {
                    sum += matrix[i][j] * vector[j];
                }
                result_scalar[i] = sum;
            }
            return result_scalar[0];
        });

        // SIMD version
        measure_performance("SIMD matrix-vector multiply", [&]() {
            for (size_t i = 0; i < rows; ++i) {
                __m256 sum_vec = _mm256_setzero_ps();
                size_t simd_cols = cols - (cols % 8);

                for (size_t j = 0; j < simd_cols; j += 8) {
                    __m256 m_vec = _mm256_loadu_ps(&matrix[i][j]);
                    __m256 v_vec = _mm256_loadu_ps(&vector[j]);
                    __m256 prod = _mm256_mul_ps(m_vec, v_vec);
                    sum_vec = _mm256_add_ps(sum_vec, prod);
                }

                // Horizontal sum
                float partial_sums[8];
                _mm256_storeu_ps(partial_sums, sum_vec);
                float sum = partial_sums[0] + partial_sums[1] + partial_sums[2] + partial_sums[3] +
                           partial_sums[4] + partial_sums[5] + partial_sums[6] + partial_sums[7];

                // Handle remaining elements
                for (size_t j = simd_cols; j < cols; ++j) {
                    sum += matrix[i][j] * vector[j];
                }

                result_simd[i] = sum;
            }
            return result_simd[0];
        });

        // Verify results
        bool correct = std::equal(result_scalar.begin(), result_scalar.end(),
                                 result_simd.begin(),
                                 [](float a, float b) { return std::abs(a - b) < 0.01f; });
        std::cout << "Results match: " << (correct ? "YES" : "NO") << "\n";
    }
};

// ===========================================================================================
// 2. COMPILER AUTO-VECTORIZATION
// ===========================================================================================

class AutoVectorizationExamples {
public:
    // Example 1: Simple loop that compilers can vectorize
    static void simple_auto_vectorization() {
        std::cout << "\n=== Compiler Auto-Vectorization Examples ===\n";

        const size_t size = 1'000'000;
        std::vector<float> a(size), b(size), c(size);

        // Initialize data
        std::iota(a.begin(), a.end(), 1.0f);
        std::iota(b.begin(), b.end(), 2.0f);

        // Simple loop that can be auto-vectorized
        // Compile with -O3 -march=native for best auto-vectorization
        auto start = get_current_time();

        for (size_t i = 0; i < size; ++i) {
            c[i] = a[i] + b[i] * 2.0f - 1.0f; // Linear combination
        }

        auto end = get_current_time();
        std::cout << "Auto-vectorized loop: " << get_duration_ms(start, end) << " ms\n";

        // More complex operation that benefits from vectorization
        start = get_current_time();

        for (size_t i = 0; i < size; ++i) {
            c[i] = std::sqrt(a[i] * a[i] + b[i] * b[i]); // Euclidean norm
        }

        end = get_current_time();
        std::cout << "Auto-vectorized sqrt operation: " << get_duration_ms(start, end) << " ms\n";

        std::cout << "Note: Use compiler flags -O3 -march=native -ffast-math for best results\n";
    }

    // Example 2: Reduction operations
    static void reduction_operations() {
        std::cout << "\n=== SIMD Reduction Operations ===\n";

        const size_t size = 10'000'000;
        std::vector<float> data(size);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-10.0f, 10.0f);
        std::generate(data.begin(), data.end(), [&]() { return dis(gen); });

        // Sum reduction - scalar
        float scalar_sum = measure_performance("Scalar sum", [&]() {
            float sum = 0.0f;
            for (float value : data) {
                sum += value;
            }
            return sum;
        });

        // Sum reduction - manual SIMD
        float simd_sum = measure_performance("Manual SIMD sum", [&]() {
            __m256 sum_vec = _mm256_setzero_ps();
            size_t simd_size = size - (size % 8);

            for (size_t i = 0; i < simd_size; i += 8) {
                __m256 data_vec = _mm256_loadu_ps(&data[i]);
                sum_vec = _mm256_add_ps(sum_vec, data_vec);
            }

            // Horizontal sum using hadd
            __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
            __m128 sum_low = _mm256_castps256_ps128(sum_vec);
            __m128 sum128 = _mm_add_ps(sum_high, sum_low);

            sum128 = _mm_hadd_ps(sum128, sum128);
            sum128 = _mm_hadd_ps(sum128, sum128);

            float sum = _mm_cvtss_f32(sum128);

            // Handle remaining elements
            for (size_t i = simd_size; i < size; ++i) {
                sum += data[i];
            }

            return sum;
        });

        // Using C++17 parallel algorithms (which use SIMD internally)
        float parallel_sum = measure_performance("C++17 parallel reduce", [&]() {
            return std::reduce(std::execution::par_unseq, data.begin(), data.end(), 0.0f);
        });

        std::cout << "Scalar sum: " << scalar_sum << "\n";
        std::cout << "SIMD sum: " << simd_sum << "\n";
        std::cout << "Parallel sum: " << parallel_sum << "\n";

        float max_diff = std::max({std::abs(scalar_sum - simd_sum),
                                  std::abs(scalar_sum - parallel_sum),
                                  std::abs(simd_sum - parallel_sum)});
        std::cout << "Maximum difference: " << max_diff << "\n";
    }
};

// ===========================================================================================
// 3. ADVANCED SIMD TECHNIQUES
// ===========================================================================================

class AdvancedSIMDTechniques {
public:
    // Example 1: Conditional operations and masking
    static void conditional_operations() {
        std::cout << "\n=== SIMD Conditional Operations and Masking ===\n";

        const size_t size = 1'000'000;
        std::vector<float> input(size), output_scalar(size), output_simd(size);

        // Initialize with random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-10.0f, 10.0f);
        std::generate(input.begin(), input.end(), [&]() { return dis(gen); });

        // Operation: if x > 0 then sqrt(x) else 0
        measure_performance("Scalar conditional", [&]() {
            for (size_t i = 0; i < size; ++i) {
                output_scalar[i] = input[i] > 0.0f ? std::sqrt(input[i]) : 0.0f;
            }
            return output_scalar[0];
        });

        // SIMD version with masking
        measure_performance("SIMD conditional", [&]() {
            const __m256 zero = _mm256_setzero_ps();
            size_t simd_size = size - (size % 8);

            for (size_t i = 0; i < simd_size; i += 8) {
                __m256 x = _mm256_loadu_ps(&input[i]);
                __m256 mask = _mm256_cmp_ps(x, zero, _CMP_GT_OQ); // x > 0
                __m256 sqrt_x = _mm256_sqrt_ps(x);
                __m256 result = _mm256_and_ps(mask, sqrt_x); // Apply mask
                _mm256_storeu_ps(&output_simd[i], result);
            }

            // Handle remaining elements
            for (size_t i = simd_size; i < size; ++i) {
                output_simd[i] = input[i] > 0.0f ? std::sqrt(input[i]) : 0.0f;
            }

            return output_simd[0];
        });

        // Verify results
        bool correct = std::equal(output_scalar.begin(), output_scalar.end(),
                                 output_simd.begin(),
                                 [](float a, float b) { return std::abs(a - b) < 0.001f; });
        std::cout << "Results match: " << (correct ? "YES" : "NO") << "\n";
    }

    // Example 2: Complex number operations
    static void complex_number_operations() {
        std::cout << "\n=== SIMD Complex Number Operations ===\n";

        struct Complex {
            float real, imag;
            Complex(float r = 0, float i = 0) : real(r), imag(i) {}
        };

        const size_t size = 500'000; // Half the size since we pack 2 complex numbers per AVX register
        std::vector<Complex> a(size), b(size), result_scalar(size), result_simd(size);

        // Initialize with random complex numbers
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-5.0f, 5.0f);

        for (size_t i = 0; i < size; ++i) {
            a[i] = Complex(dis(gen), dis(gen));
            b[i] = Complex(dis(gen), dis(gen));
        }

        // Scalar complex multiplication: (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
        measure_performance("Scalar complex multiply", [&]() {
            for (size_t i = 0; i < size; ++i) {
                float real = a[i].real * b[i].real - a[i].imag * b[i].imag;
                float imag = a[i].real * b[i].imag + a[i].imag * b[i].real;
                result_scalar[i] = Complex(real, imag);
            }
            return result_scalar[0].real;
        });

        // SIMD complex multiplication (process 4 complex numbers at once)
        measure_performance("SIMD complex multiply", [&]() {
            size_t simd_size = size - (size % 4);

            for (size_t i = 0; i < simd_size; i += 4) {
                // Load 4 complex numbers (8 floats total)
                __m256 a_vals = _mm256_loadu_ps(reinterpret_cast<const float*>(&a[i]));
                __m256 b_vals = _mm256_loadu_ps(reinterpret_cast<const float*>(&b[i]));

                // Separate real and imaginary parts
                __m256 a_real = _mm256_shuffle_ps(a_vals, a_vals, _MM_SHUFFLE(2, 0, 2, 0));
                __m256 a_imag = _mm256_shuffle_ps(a_vals, a_vals, _MM_SHUFFLE(3, 1, 3, 1));
                __m256 b_real = _mm256_shuffle_ps(b_vals, b_vals, _MM_SHUFFLE(2, 0, 2, 0));
                __m256 b_imag = _mm256_shuffle_ps(b_vals, b_vals, _MM_SHUFFLE(3, 1, 3, 1));

                // Complex multiplication
                __m256 real_part = _mm256_sub_ps(_mm256_mul_ps(a_real, b_real),
                                               _mm256_mul_ps(a_imag, b_imag));
                __m256 imag_part = _mm256_add_ps(_mm256_mul_ps(a_real, b_imag),
                                               _mm256_mul_ps(a_imag, b_real));

                // Interleave results back
                __m256 result_low = _mm256_unpacklo_ps(real_part, imag_part);
                __m256 result_high = _mm256_unpackhi_ps(real_part, imag_part);

                _mm256_storeu_ps(reinterpret_cast<float*>(&result_simd[i]), result_low);
                _mm256_storeu_ps(reinterpret_cast<float*>(&result_simd[i + 2]), result_high);
            }

            // Handle remaining elements
            for (size_t i = simd_size; i < size; ++i) {
                float real = a[i].real * b[i].real - a[i].imag * b[i].imag;
                float imag = a[i].real * b[i].imag + a[i].imag * b[i].real;
                result_simd[i] = Complex(real, imag);
            }

            return result_simd[0].real;
        });

        // Verify results
        bool correct = true;
        for (size_t i = 0; i < size; ++i) {
            if (std::abs(result_scalar[i].real - result_simd[i].real) > 0.001f ||
                std::abs(result_scalar[i].imag - result_simd[i].imag) > 0.001f) {
                correct = false;
                break;
            }
        }
        std::cout << "Results match: " << (correct ? "YES" : "NO") << "\n";
    }

    // Example 3: Memory alignment and prefetching
    static void memory_alignment_example() {
        std::cout << "\n=== Memory Alignment and Prefetching for SIMD ===\n";

        const size_t size = 1'000'000;

        // Aligned vs unaligned memory access
        std::vector<float> unaligned_data(size + 7); // Extra space for alignment
        float* aligned_data = reinterpret_cast<float*>(
            (reinterpret_cast<uintptr_t>(unaligned_data.data()) + 31) & ~31);

        std::vector<float> result(size);

        // Initialize aligned data
        for (size_t i = 0; i < size; ++i) {
            aligned_data[i] = static_cast<float>(i + 1);
        }

        // Test with aligned loads (faster)
        measure_performance("SIMD with aligned loads", [&]() {
            for (size_t i = 0; i < size; i += 8) {
                __m256 data = _mm256_load_ps(&aligned_data[i]); // Aligned load
                __m256 doubled = _mm256_mul_ps(data, _mm256_set1_ps(2.0f));
                _mm256_storeu_ps(&result[i], doubled);
            }
            return result[0];
        });

        // Test with unaligned loads (slower)
        measure_performance("SIMD with unaligned loads", [&]() {
            for (size_t i = 0; i < size; i += 8) {
                __m256 data = _mm256_loadu_ps(&unaligned_data[i + 3]); // Unaligned load
                __m256 doubled = _mm256_mul_ps(data, _mm256_set1_ps(2.0f));
                _mm256_storeu_ps(&result[i], doubled);
            }
            return result[0];
        });

        std::cout << "Note: Aligned memory access can be 10-20% faster for SIMD operations\n";
    }
};

// ===========================================================================================
// 4. FINANCIAL COMPUTING WITH SIMD
// ===========================================================================================

class FinancialSIMDExamples {
public:
    // Example 1: Black-Scholes option pricing with SIMD
    static void black_scholes_simd() {
        std::cout << "\n=== Black-Scholes Option Pricing with SIMD ===\n";

        const size_t num_options = 1'000'000;
        std::vector<float> S(num_options);    // Stock prices
        std::vector<float> K(num_options);    // Strike prices
        std::vector<float> T(num_options);    // Time to expiration
        std::vector<float> r(num_options);    // Risk-free rate
        std::vector<float> sigma(num_options); // Volatility
        std::vector<float> call_prices_scalar(num_options);
        std::vector<float> call_prices_simd(num_options);

        // Initialize with realistic option parameters
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> s_dis(80.0f, 120.0f);   // Stock price
        std::uniform_real_distribution<float> k_dis(90.0f, 110.0f);   // Strike price
        std::uniform_real_distribution<float> t_dis(0.1f, 1.0f);      // Time to expiration
        std::uniform_real_distribution<float> r_dis(0.01f, 0.05f);    // Interest rate
        std::uniform_real_distribution<float> vol_dis(0.15f, 0.35f);  // Volatility

        for (size_t i = 0; i < num_options; ++i) {
            S[i] = s_dis(gen);
            K[i] = k_dis(gen);
            T[i] = t_dis(gen);
            r[i] = r_dis(gen);
            sigma[i] = vol_dis(gen);
        }

        // Scalar Black-Scholes implementation
        auto norm_cdf = [](float x) -> float {
            return 0.5f * (1.0f + std::erf(x / std::sqrt(2.0f)));
        };

        measure_performance("Scalar Black-Scholes", [&]() {
            for (size_t i = 0; i < num_options; ++i) {
                float sqrt_T = std::sqrt(T[i]);
                float d1 = (std::log(S[i] / K[i]) + (r[i] + 0.5f * sigma[i] * sigma[i]) * T[i]) /
                          (sigma[i] * sqrt_T);
                float d2 = d1 - sigma[i] * sqrt_T;

                float N_d1 = norm_cdf(d1);
                float N_d2 = norm_cdf(d2);

                call_prices_scalar[i] = S[i] * N_d1 - K[i] * std::exp(-r[i] * T[i]) * N_d2;
            }
            return call_prices_scalar[0];
        });

        // SIMD Black-Scholes (simplified version focusing on the math)
        measure_performance("SIMD Black-Scholes", [&]() {
            size_t simd_size = num_options - (num_options % 8);

            for (size_t i = 0; i < simd_size; i += 8) {
                __m256 s_vec = _mm256_loadu_ps(&S[i]);
                __m256 k_vec = _mm256_loadu_ps(&K[i]);
                __m256 t_vec = _mm256_loadu_ps(&T[i]);
                __m256 r_vec = _mm256_loadu_ps(&r[i]);
                __m256 sigma_vec = _mm256_loadu_ps(&sigma[i]);

                // Calculate d1 and d2 using SIMD operations
                __m256 sqrt_t = _mm256_sqrt_ps(t_vec);
                __m256 log_s_k = _mm256_log_ps(_mm256_div_ps(s_vec, k_vec));
                __m256 sigma_squared = _mm256_mul_ps(sigma_vec, sigma_vec);
                __m256 half_sigma_squared = _mm256_mul_ps(_mm256_set1_ps(0.5f), sigma_squared);

                __m256 numerator = _mm256_add_ps(log_s_k,
                    _mm256_mul_ps(_mm256_add_ps(r_vec, half_sigma_squared), t_vec));
                __m256 denominator = _mm256_mul_ps(sigma_vec, sqrt_t);
                __m256 d1 = _mm256_div_ps(numerator, denominator);
                __m256 d2 = _mm256_sub_ps(d1, _mm256_mul_ps(sigma_vec, sqrt_t));

                // Note: For production code, you'd need SIMD versions of erf() and exp()
                // Here we fall back to scalar for demonstration
                float d1_vals[8], d2_vals[8], call_prices[8];
                _mm256_storeu_ps(d1_vals, d1);
                _mm256_storeu_ps(d2_vals, d2);

                for (int j = 0; j < 8; ++j) {
                    float N_d1 = norm_cdf(d1_vals[j]);
                    float N_d2 = norm_cdf(d2_vals[j]);
                    call_prices[j] = S[i + j] * N_d1 - K[i + j] * std::exp(-r[i + j] * T[i + j]) * N_d2;
                }

                _mm256_storeu_ps(&call_prices_simd[i], _mm256_loadu_ps(call_prices));
            }

            // Handle remaining elements
            for (size_t i = simd_size; i < num_options; ++i) {
                float sqrt_T = std::sqrt(T[i]);
                float d1 = (std::log(S[i] / K[i]) + (r[i] + 0.5f * sigma[i] * sigma[i]) * T[i]) /
                          (sigma[i] * sqrt_T);
                float d2 = d1 - sigma[i] * sqrt_T;

                float N_d1 = norm_cdf(d1);
                float N_d2 = norm_cdf(d2);

                call_prices_simd[i] = S[i] * N_d1 - K[i] * std::exp(-r[i] * T[i]) * N_d2;
            }

            return call_prices_simd[0];
        });

        // Verify results
        bool correct = std::equal(call_prices_scalar.begin(), call_prices_scalar.end(),
                                 call_prices_simd.begin(),
                                 [](float a, float b) { return std::abs(a - b) < 0.01f; });
        std::cout << "Results match: " << (correct ? "YES" : "NO") << "\n";
        std::cout << "Sample call price: $" << call_prices_scalar[0] << "\n";
    }

    // Example 2: Portfolio risk calculations with SIMD
    static void portfolio_risk_simd() {
        std::cout << "\n=== Portfolio Risk Calculations with SIMD ===\n";

        const size_t num_assets = 1000;
        const size_t num_scenarios = 100000;

        std::vector<std::vector<float>> returns(num_scenarios, std::vector<float>(num_assets));
        std::vector<float> weights(num_assets);
        std::vector<float> portfolio_returns_scalar(num_scenarios);
        std::vector<float> portfolio_returns_simd(num_scenarios);

        // Initialize with random returns and weights
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> return_dis(0.001f, 0.02f); // 0.1% mean, 2% std dev
        std::uniform_real_distribution<float> weight_dis(0.0f, 1.0f);

        // Generate random weights and normalize
        float weight_sum = 0.0f;
        for (size_t i = 0; i < num_assets; ++i) {
            weights[i] = weight_dis(gen);
            weight_sum += weights[i];
        }
        for (size_t i = 0; i < num_assets; ++i) {
            weights[i] /= weight_sum; // Normalize to sum to 1
        }

        // Generate random returns
        for (size_t i = 0; i < num_scenarios; ++i) {
            for (size_t j = 0; j < num_assets; ++j) {
                returns[i][j] = return_dis(gen);
            }
        }

        // Scalar portfolio return calculation
        measure_performance("Scalar portfolio returns", [&]() {
            for (size_t scenario = 0; scenario < num_scenarios; ++scenario) {
                float portfolio_return = 0.0f;
                for (size_t asset = 0; asset < num_assets; ++asset) {
                    portfolio_return += weights[asset] * returns[scenario][asset];
                }
                portfolio_returns_scalar[scenario] = portfolio_return;
            }
            return portfolio_returns_scalar[0];
        });

        // SIMD portfolio return calculation
        measure_performance("SIMD portfolio returns", [&]() {
            size_t simd_assets = num_assets - (num_assets % 8);

            for (size_t scenario = 0; scenario < num_scenarios; ++scenario) {
                __m256 portfolio_return_vec = _mm256_setzero_ps();

                for (size_t asset = 0; asset < simd_assets; asset += 8) {
                    __m256 weight_vec = _mm256_loadu_ps(&weights[asset]);
                    __m256 return_vec = _mm256_loadu_ps(&returns[scenario][asset]);
                    __m256 contrib = _mm256_mul_ps(weight_vec, return_vec);
                    portfolio_return_vec = _mm256_add_ps(portfolio_return_vec, contrib);
                }

                // Horizontal sum
                float contrib_vals[8];
                _mm256_storeu_ps(contrib_vals, portfolio_return_vec);
                float portfolio_return = contrib_vals[0] + contrib_vals[1] + contrib_vals[2] + contrib_vals[3] +
                                        contrib_vals[4] + contrib_vals[5] + contrib_vals[6] + contrib_vals[7];

                // Handle remaining assets
                for (size_t asset = simd_assets; asset < num_assets; ++asset) {
                    portfolio_return += weights[asset] * returns[scenario][asset];
                }

                portfolio_returns_simd[scenario] = portfolio_return;
            }
            return portfolio_returns_simd[0];
        });

        // Calculate VaR (Value at Risk) at 95% confidence level
        std::sort(portfolio_returns_scalar.begin(), portfolio_returns_scalar.end());
        std::sort(portfolio_returns_simd.begin(), portfolio_returns_simd.end());

        size_t var_index = static_cast<size_t>(0.05 * num_scenarios); // 5th percentile
        float var_scalar = -portfolio_returns_scalar[var_index];
        float var_simd = -portfolio_returns_simd[var_index];

        std::cout << "95% VaR (scalar): " << var_scalar * 100 << "%\n";
        std::cout << "95% VaR (SIMD): " << var_simd * 100 << "%\n";
        std::cout << "Difference: " << std::abs(var_scalar - var_simd) * 100 << "%\n";
    }
};

// ===========================================================================================
// 5. SIMD BEST PRACTICES AND GUIDELINES
// ===========================================================================================

class SIMDBestPractices {
public:
    static void demonstrate_best_practices() {
        std::cout << "\n=== SIMD Best Practices and Guidelines ===\n";

        std::cout << "\n1. MEMORY ALIGNMENT:\n";
        std::cout << "   • Use 32-byte alignment for AVX (256-bit)\n";
        std::cout << "   • Use 64-byte alignment for AVX-512 (512-bit)\n";
        std::cout << "   • Aligned loads/stores are faster than unaligned\n";

        std::cout << "\n2. DATA LAYOUT:\n";
        std::cout << "   • Structure of Arrays (SoA) is better than Array of Structures (AoS)\n";
        std::cout << "   • Avoid pointer chasing and indirect memory access\n";
        std::cout << "   • Keep data contiguous in memory\n";

        std::cout << "\n3. COMPILER OPTIMIZATION:\n";
        std::cout << "   • Use -O3 -march=native -ffast-math for best auto-vectorization\n";
        std::cout << "   • Enable specific instruction sets: -mavx2, -mavx512f\n";
        std::cout << "   • Use #pragma omp simd for OpenMP SIMD hints\n";

        std::cout << "\n4. ALGORITHM DESIGN:\n";
        std::cout << "   • Minimize conditional branches in SIMD loops\n";
        std::cout << "   • Use masking for conditional operations\n";
        std::cout << "   • Ensure sufficient work per SIMD operation\n";

        std::cout << "\n5. PERFORMANCE CONSIDERATIONS:\n";
        std::cout << "   • Profile to ensure SIMD actually improves performance\n";
        std::cout << "   • Consider memory bandwidth limitations\n";
        std::cout << "   • Account for setup/cleanup overhead\n";

        std::cout << "\n6. PORTABILITY:\n";
        std::cout << "   • Use runtime CPU feature detection\n";
        std::cout << "   • Provide scalar fallbacks\n";
        std::cout << "   • Consider cross-platform SIMD libraries (like xsimd)\n";

        demonstrate_soa_vs_aos();
        demonstrate_cpu_feature_detection();
    }

private:
    // Demonstrate Structure of Arrays vs Array of Structures
    static void demonstrate_soa_vs_aos() {
        std::cout << "\n--- SoA vs AoS Performance Comparison ---\n";

        struct Point3D_AoS {
            float x, y, z;
            Point3D_AoS(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
        };

        struct Point3D_SoA {
            std::vector<float> x, y, z;
            Point3D_SoA(size_t size) : x(size), y(size), z(size) {}
        };

        const size_t num_points = 1'000'000;

        // Array of Structures
        std::vector<Point3D_AoS> points_aos(num_points);
        for (size_t i = 0; i < num_points; ++i) {
            points_aos[i] = Point3D_AoS(i * 1.0f, i * 2.0f, i * 3.0f);
        }

        // Structure of Arrays
        Point3D_SoA points_soa(num_points);
        for (size_t i = 0; i < num_points; ++i) {
            points_soa.x[i] = i * 1.0f;
            points_soa.y[i] = i * 2.0f;
            points_soa.z[i] = i * 3.0f;
        }

        std::vector<float> distances_aos(num_points), distances_soa(num_points);

        // Calculate distance from origin - AoS version
        measure_performance("AoS distance calculation", [&]() {
            for (size_t i = 0; i < num_points; ++i) {
                float x = points_aos[i].x;
                float y = points_aos[i].y;
                float z = points_aos[i].z;
                distances_aos[i] = std::sqrt(x*x + y*y + z*z);
            }
            return distances_aos[0];
        });

        // Calculate distance from origin - SoA version (SIMD-friendly)
        measure_performance("SoA distance calculation", [&]() {
            size_t simd_size = num_points - (num_points % 8);

            for (size_t i = 0; i < simd_size; i += 8) {
                __m256 x_vec = _mm256_loadu_ps(&points_soa.x[i]);
                __m256 y_vec = _mm256_loadu_ps(&points_soa.y[i]);
                __m256 z_vec = _mm256_loadu_ps(&points_soa.z[i]);

                __m256 x_sq = _mm256_mul_ps(x_vec, x_vec);
                __m256 y_sq = _mm256_mul_ps(y_vec, y_vec);
                __m256 z_sq = _mm256_mul_ps(z_vec, z_vec);

                __m256 sum = _mm256_add_ps(x_sq, _mm256_add_ps(y_sq, z_sq));
                __m256 distance = _mm256_sqrt_ps(sum);

                _mm256_storeu_ps(&distances_soa[i], distance);
            }

            // Handle remaining elements
            for (size_t i = simd_size; i < num_points; ++i) {
                float x = points_soa.x[i];
                float y = points_soa.y[i];
                float z = points_soa.z[i];
                distances_soa[i] = std::sqrt(x*x + y*y + z*z);
            }

            return distances_soa[0];
        });

        std::cout << "SoA typically 2-4x faster due to SIMD vectorization\n";
    }

    // Demonstrate CPU feature detection for runtime SIMD selection
    static void demonstrate_cpu_feature_detection() {
        std::cout << "\n--- CPU Feature Detection ---\n";

        // Simple runtime feature detection (production code should use cpuid)
        bool has_sse = true;    // Assume all modern CPUs have SSE
        bool has_avx = true;    // Check with __builtin_cpu_supports("avx")
        bool has_avx2 = true;   // Check with __builtin_cpu_supports("avx2")
        bool has_avx512 = false; // Check with __builtin_cpu_supports("avx512f")

        std::cout << "CPU SIMD Support:\n";
        std::cout << "  SSE: " << (has_sse ? "YES" : "NO") << "\n";
        std::cout << "  AVX: " << (has_avx ? "YES" : "NO") << "\n";
        std::cout << "  AVX2: " << (has_avx2 ? "YES" : "NO") << "\n";
        std::cout << "  AVX-512: " << (has_avx512 ? "YES" : "NO") << "\n";

        std::cout << "\nRecommended approach: Use function pointers to select optimal implementation at runtime\n";
        std::cout << "Example:\n";
        std::cout << "  if (has_avx512) use_avx512_implementation();\n";
        std::cout << "  else if (has_avx2) use_avx2_implementation();\n";
        std::cout << "  else if (has_avx) use_avx_implementation();\n";
        std::cout << "  else use_scalar_implementation();\n";
    }
};

} // namespace simd_examples

// ===========================================================================================
// MAIN FUNCTION - COMPREHENSIVE SIMD DEMONSTRATION
// ===========================================================================================

int main() {
    std::cout << "=================================================================\n";
    std::cout << "           SIMD (Single Instruction, Multiple Data)\n";
    std::cout << "                 Comprehensive Examples\n";
    std::cout << "=================================================================\n";

    std::cout << "\nSYSTEM INFORMATION:\n";
    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "Compile with: g++ -O3 -march=native -mavx2 -mfma\n";
    std::cout << "For best performance!\n";

    // Run all SIMD examples
    simd_examples::BasicSIMDExamples::vector_addition_comparison();
    simd_examples::BasicSIMDExamples::dot_product_comparison();
    simd_examples::BasicSIMDExamples::matrix_vector_multiply();

    simd_examples::AutoVectorizationExamples::simple_auto_vectorization();
    simd_examples::AutoVectorizationExamples::reduction_operations();

    simd_examples::AdvancedSIMDTechniques::conditional_operations();
    simd_examples::AdvancedSIMDTechniques::complex_number_operations();
    simd_examples::AdvancedSIMDTechniques::memory_alignment_example();

    simd_examples::FinancialSIMDExamples::black_scholes_simd();
    simd_examples::FinancialSIMDExamples::portfolio_risk_simd();

    simd_examples::SIMDBestPractices::demonstrate_best_practices();

    std::cout << "\n=================================================================\n";
    std::cout << "                           SUMMARY\n";
    std::cout << "=================================================================\n";

    std::cout << "\n🎯 KEY SIMD CONCEPTS:\n";
    std::cout << "1. SIMD processes multiple data elements with single instruction\n";
    std::cout << "2. Modern CPUs support 128-bit (SSE), 256-bit (AVX), 512-bit (AVX-512)\n";
    std::cout << "3. Typical speedup: 2-8x for suitable algorithms\n";
    std::cout << "4. Best for: math operations, data processing, signal processing\n";

    std::cout << "\n⚡ PERFORMANCE TIPS:\n";
    std::cout << "• Use aligned memory access when possible\n";
    std::cout << "• Structure of Arrays (SoA) > Array of Structures (AoS)\n";
    std::cout << "• Minimize branches and conditional operations\n";
    std::cout << "• Let compiler auto-vectorize simple loops (-O3 -march=native)\n";
    std::cout << "• Use manual SIMD for critical performance paths\n";

    std::cout << "\n🛠️ TOOLS AND TECHNIQUES:\n";
    std::cout << "• Compiler intrinsics: _mm256_add_ps(), _mm256_mul_ps(), etc.\n";
    std::cout << "• C++17 parallel algorithms with par_unseq policy\n";
    std::cout << "• Runtime CPU feature detection for optimal code selection\n";
    std::cout << "• Cross-platform libraries: xsimd, Vc, highway\n";

    std::cout << "\n📊 WHEN TO USE SIMD:\n";
    std::cout << "✅ Large datasets with regular access patterns\n";
    std::cout << "✅ Mathematical computations (linear algebra, statistics)\n";
    std::cout << "✅ Signal/image processing, compression\n";
    std::cout << "✅ Financial calculations (option pricing, risk)\n";
    std::cout << "❌ Small datasets (overhead dominates)\n";
    std::cout << "❌ Highly irregular/branchy algorithms\n";
    std::cout << "❌ Memory-bound operations (bandwidth limited)\n";

    return 0;
}

/*
 * COMPILATION INSTRUCTIONS:
 *
 * For optimal SIMD performance:
 * g++ -std=c++17 -O3 -march=native -mavx2 -mfma -ffast-math simd_vectorization_examples.cpp -o simd_demo
 *
 * For specific CPU targets:
 * g++ -std=c++17 -O3 -mavx2 -msse4.2 simd_vectorization_examples.cpp -o simd_demo
 *
 * With OpenMP SIMD support:
 * g++ -std=c++17 -O3 -march=native -fopenmp-simd simd_vectorization_examples.cpp -o simd_demo
 *
 * For debugging SIMD code:
 * g++ -std=c++17 -O1 -g -march=native -mavx2 simd_vectorization_examples.cpp -o simd_demo
 *
 * Note:
 * - Use -march=native for best performance on your specific CPU
 * - Add -ffast-math for aggressive floating-point optimizations (may affect precision)
 * - Intel compiler (icc) often provides better auto-vectorization than GCC/Clang
 */
