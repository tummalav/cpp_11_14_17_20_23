#include <iostream>
#include <cstddef>
#include <type_traits>
#include <chrono>
#include <vector>
#include <memory>
#include <immintrin.h>

/*
 * ALIGNAS VS #PRAGMA PACK COMPREHENSIVE COMPARISON
 *
 * This file demonstrates the differences between C++11 alignas and legacy #pragma pack
 * for memory layout control in ultra-low latency trading systems.
 *
 * Key Differences:
 * 1. alignas: Increases alignment (C++11 standard)
 * 2. #pragma pack: Decreases alignment (compiler-specific)
 * 3. Performance implications for cache lines and SIMD
 * 4. Portability and standards compliance
 */

// =============================================================================
// SECTION 1: BASIC ALIGNMENT CONCEPTS
// =============================================================================

// Default alignment behavior
struct DefaultStruct {
    char c;      // 1 byte
    int i;       // 4 bytes, typically aligned to 4-byte boundary
    double d;    // 8 bytes, typically aligned to 8-byte boundary
};

// Using alignas to increase alignment
struct alignas(64) CacheLineAligned {
    char c;
    int i;
    double d;
    // Compiler adds padding to reach 64-byte alignment
};

// Using alignas with specific members
struct MemberAligned {
    char c;
    alignas(16) int i;    // Force 16-byte alignment for this member
    double d;
};

// Using #pragma pack to decrease alignment
#pragma pack(push, 1)  // Pack to 1-byte boundaries
struct PackedStruct {
    char c;      // 1 byte
    int i;       // 4 bytes, no padding
    double d;    // 8 bytes, no padding
};              // Total: 13 bytes instead of default 16
#pragma pack(pop)

// =============================================================================
// SECTION 2: ULTRA-LOW LATENCY TRADING STRUCTURES
// =============================================================================

// Market data tick optimized for cache performance
struct alignas(64) OptimizedTick {
    double price;           // 8 bytes
    uint64_t volume;       // 8 bytes
    uint64_t timestamp;    // 8 bytes
    uint32_t sequence_id;  // 4 bytes
    uint32_t symbol_id;    // 4 bytes
    // Padding added automatically to reach 64 bytes
    char padding[64 - 32]; // Explicit padding for clarity
};

// Network packet structure using pragma pack for exact layout
#pragma pack(push, 1)
struct NetworkPacketHeader {
    uint16_t magic;        // 2 bytes
    uint16_t version;      // 2 bytes
    uint32_t length;       // 4 bytes
    uint64_t timestamp;    // 8 bytes
    uint32_t sequence;     // 4 bytes
    uint16_t checksum;     // 2 bytes
};                        // Exactly 22 bytes, no padding
#pragma pack(pop)

// SIMD-optimized price calculation structure
struct alignas(32) SIMDPriceData {
    double prices[4];      // 32 bytes, perfect for AVX operations
};

// Memory pool allocation structure
struct alignas(std::max_align_t) PoolAllocated {
    char data[1024];
    // Aligned to maximum fundamental alignment
};

// =============================================================================
// SECTION 3: PERFORMANCE COMPARISON EXAMPLES
// =============================================================================

class AlignmentPerformanceTest {
private:
    static constexpr size_t ITERATIONS = 1000000;
    static constexpr size_t ARRAY_SIZE = 1000;

public:
    // Test cache line alignment performance
    void testCacheLineAlignment() {
        std::cout << "\n=== CACHE LINE ALIGNMENT PERFORMANCE TEST ===\n";

        // Aligned array
        std::vector<OptimizedTick> aligned_ticks(ARRAY_SIZE);

        // Unaligned array (using default alignment)
        std::vector<DefaultStruct> unaligned_data(ARRAY_SIZE);

        // Benchmark aligned access
        auto start = std::chrono::high_resolution_clock::now();
        volatile double sum = 0.0;

        for (size_t iter = 0; iter < ITERATIONS; ++iter) {
            for (size_t i = 0; i < ARRAY_SIZE; ++i) {
                sum += aligned_ticks[i].price;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto aligned_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "Aligned access time: " << aligned_time.count() << " ns\n";
        std::cout << "Aligned sum (prevent optimization): " << sum << "\n";
    }

    // Test SIMD alignment performance
    void testSIMDAlignment() {
        std::cout << "\n=== SIMD ALIGNMENT PERFORMANCE TEST ===\n";

        // Properly aligned for AVX
        alignas(32) double aligned_prices[8] = {100.1, 101.2, 102.3, 103.4,
                                               104.5, 105.6, 106.7, 107.8};

        // Misaligned data
        double unaligned_prices[9] = {0.0, 100.1, 101.2, 102.3, 103.4,
                                     104.5, 105.6, 106.7, 107.8};
        double* misaligned = &unaligned_prices[1]; // Force misalignment

        // Benchmark aligned SIMD operations
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < ITERATIONS; ++i) {
            __m256d vec1 = _mm256_load_pd(&aligned_prices[0]);    // Aligned load
            __m256d vec2 = _mm256_load_pd(&aligned_prices[4]);    // Aligned load
            __m256d result = _mm256_add_pd(vec1, vec2);
            _mm256_store_pd(&aligned_prices[0], result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto aligned_simd_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        // Benchmark misaligned SIMD operations
        start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < ITERATIONS; ++i) {
            __m256d vec1 = _mm256_loadu_pd(&misaligned[0]);       // Unaligned load
            __m256d vec2 = _mm256_loadu_pd(&misaligned[4]);       // Unaligned load
            __m256d result = _mm256_add_pd(vec1, vec2);
            _mm256_storeu_pd(&misaligned[0], result);
        }

        end = std::chrono::high_resolution_clock::now();
        auto misaligned_simd_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "Aligned SIMD time: " << aligned_simd_time.count() << " ns\n";
        std::cout << "Misaligned SIMD time: " << misaligned_simd_time.count() << " ns\n";
        std::cout << "Performance degradation: "
                  << ((double)misaligned_simd_time.count() / aligned_simd_time.count() - 1.0) * 100.0
                  << "%\n";
    }

    // Test memory bandwidth with different alignments
    void testMemoryBandwidth() {
        std::cout << "\n=== MEMORY BANDWIDTH TEST ===\n";

        const size_t buffer_size = 1024 * 1024; // 1MB

        // Cache-line aligned buffer
        std::unique_ptr<char[]> aligned_buffer(new(std::align_val_t{64}) char[buffer_size]);

        // Regular aligned buffer
        std::unique_ptr<char[]> regular_buffer(new char[buffer_size]);

        // Test aligned bandwidth
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t iter = 0; iter < 1000; ++iter) {
            for (size_t i = 0; i < buffer_size; i += 64) {
                _mm_prefetch(&aligned_buffer[i], _MM_HINT_T0);
                volatile char dummy = aligned_buffer[i];
                (void)dummy;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto aligned_bandwidth_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "Cache-line aligned bandwidth time: " << aligned_bandwidth_time.count() << " ns\n";
    }
};

// =============================================================================
// SECTION 4: PRACTICAL TRADING SYSTEM EXAMPLES
// =============================================================================

// Order book level using alignas for cache optimization
struct alignas(64) OrderBookLevel {
    double price;          // 8 bytes
    uint64_t quantity;     // 8 bytes
    uint32_t order_count;  // 4 bytes
    uint32_t flags;        // 4 bytes
    uint64_t timestamp;    // 8 bytes
    // Padding to 64 bytes for cache line optimization
    char padding[64 - 32];

    OrderBookLevel() noexcept : price(0), quantity(0), order_count(0), flags(0), timestamp(0) {}
};

// FIX message header using pragma pack for exact protocol layout
#pragma pack(push, 1)
struct FIXMessageHeader {
    char begin_string[8];  // "FIX.4.4"
    uint16_t body_length;  // Message body length
    char msg_type;         // Message type
    char sender_comp_id[12]; // Sender ID
    char target_comp_id[12]; // Target ID
    uint32_t msg_seq_num;  // Message sequence number
    char sending_time[21]; // Timestamp
};
#pragma pack(pop)

// Trading signal structure with member-specific alignment
struct TradingSignal {
    alignas(8) double signal_strength;     // 8-byte aligned for fast access
    alignas(4) uint32_t confidence_level;  // 4-byte aligned
    alignas(8) uint64_t generation_time;   // 8-byte aligned
    char strategy_id[16];                  // No special alignment needed
    alignas(16) double risk_metrics[4];    // 16-byte aligned for SIMD
};

// =============================================================================
// SECTION 5: COMPILER AND PLATFORM SPECIFIC CONSIDERATIONS
// =============================================================================

// Check alignment requirements at compile time
template<typename T>
void printAlignmentInfo(const char* type_name) {
    std::cout << type_name << ":\n";
    std::cout << "  Size: " << sizeof(T) << " bytes\n";
    std::cout << "  Alignment: " << alignof(T) << " bytes\n";
    std::cout << "  Is trivially copyable: " << std::is_trivially_copyable_v<T> << "\n";
    std::cout << "  Is standard layout: " << std::is_standard_layout_v<T> << "\n\n";
}

// Alignment checking utilities
class AlignmentChecker {
public:
    template<typename T>
    static bool isAligned(const T* ptr, size_t alignment) {
        return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
    }

    static void* alignedAlloc(size_t size, size_t alignment) {
        return std::aligned_alloc(alignment, size);
    }

    static void alignedFree(void* ptr) {
        std::free(ptr);
    }
};

// =============================================================================
// SECTION 6: BEST PRACTICES AND GUIDELINES
// =============================================================================

namespace BestPractices {

    /*
     * WHEN TO USE ALIGNAS:
     * 1. Cache line optimization (alignas(64))
     * 2. SIMD operations (alignas(16), alignas(32))
     * 3. Atomic operations on some platforms
     * 4. Memory pool alignment
     * 5. Hardware-specific optimizations
     */

    // Good: Cache-friendly structure
    struct alignas(64) CacheFriendlyData {
        std::atomic<uint64_t> hot_data;     // Frequently accessed
        char padding[56];                   // Keep in separate cache line
    };

    /*
     * WHEN TO USE #PRAGMA PACK:
     * 1. Network protocol compliance
     * 2. File format specifications
     * 3. Hardware register layouts
     * 4. Memory-constrained environments
     * 5. Interfacing with C libraries
     */

    // Good: Network protocol structure
    #pragma pack(push, 1)
    struct ProtocolMessage {
        uint16_t header;
        uint32_t length;
        uint64_t timestamp;
        // Exactly matches wire format
    };
    #pragma pack(pop)

    /*
     * PERFORMANCE CONSIDERATIONS:
     * 1. alignas generally improves performance
     * 2. #pragma pack often degrades performance
     * 3. Cache line alignment is crucial for hot data
     * 4. SIMD requires proper alignment
     * 5. False sharing prevention with alignas
     */

    // Example: Preventing false sharing
    struct alignas(64) ThreadLocalCounter {
        std::atomic<uint64_t> counter{0};
        // Each counter gets its own cache line
    };

    /*
     * PORTABILITY NOTES:
     * 1. alignas is C++11 standard (portable)
     * 2. #pragma pack is compiler-specific
     * 3. Use static_assert for alignment verification
     * 4. Consider std::max_align_t for maximum portability
     */

    // Portable alignment verification
    struct VerifiedAlignment {
        alignas(32) double data[4];
        static_assert(alignof(VerifiedAlignment) >= 32, "Insufficient alignment");
        static_assert(sizeof(VerifiedAlignment) >= 32, "Insufficient size");
    };
}

// =============================================================================
// SECTION 7: DEMONSTRATION AND TESTING
// =============================================================================

void demonstrateAlignmentDifferences() {
    std::cout << "=== ALIGNAS VS #PRAGMA PACK DEMONSTRATION ===\n\n";

    // Show size and alignment differences
    std::cout << "STRUCTURE SIZE AND ALIGNMENT COMPARISON:\n";
    std::cout << "========================================\n";

    printAlignmentInfo<DefaultStruct>("DefaultStruct");
    printAlignmentInfo<CacheLineAligned>("CacheLineAligned (alignas(64))");
    printAlignmentInfo<PackedStruct>("PackedStruct (#pragma pack(1))");
    printAlignmentInfo<OptimizedTick>("OptimizedTick (alignas(64))");
    printAlignmentInfo<NetworkPacketHeader>("NetworkPacketHeader (#pragma pack(1))");
    printAlignmentInfo<SIMDPriceData>("SIMDPriceData (alignas(32))");

    // Memory layout demonstration
    std::cout << "MEMORY LAYOUT ANALYSIS:\n";
    std::cout << "=======================\n";

    DefaultStruct default_obj;
    CacheLineAligned aligned_obj;
    PackedStruct packed_obj;

    std::cout << "DefaultStruct address: " << &default_obj << "\n";
    std::cout << "  c offset: " << offsetof(DefaultStruct, c) << "\n";
    std::cout << "  i offset: " << offsetof(DefaultStruct, i) << "\n";
    std::cout << "  d offset: " << offsetof(DefaultStruct, d) << "\n";

    std::cout << "PackedStruct address: " << &packed_obj << "\n";
    std::cout << "  c offset: " << offsetof(PackedStruct, c) << "\n";
    std::cout << "  i offset: " << offsetof(PackedStruct, i) << "\n";
    std::cout << "  d offset: " << offsetof(PackedStruct, d) << "\n";

    // Alignment verification
    std::cout << "\nALIGNMENT VERIFICATION:\n";
    std::cout << "======================\n";

    std::cout << "CacheLineAligned is 64-byte aligned: "
              << AlignmentChecker::isAligned(&aligned_obj, 64) << "\n";

    SIMDPriceData simd_data;
    std::cout << "SIMDPriceData is 32-byte aligned: "
              << AlignmentChecker::isAligned(&simd_data, 32) << "\n";

    // Performance testing
    AlignmentPerformanceTest perf_test;
    perf_test.testCacheLineAlignment();
    perf_test.testSIMDAlignment();
    perf_test.testMemoryBandwidth();

    std::cout << "\n=== KEY TAKEAWAYS ===\n";
    std::cout << "1. alignas INCREASES alignment for performance optimization\n";
    std::cout << "2. #pragma pack DECREASES alignment for space efficiency\n";
    std::cout << "3. Cache line alignment (64 bytes) crucial for hot data\n";
    std::cout << "4. SIMD operations require proper alignment (16/32 bytes)\n";
    std::cout << "5. Network protocols often need packed structures\n";
    std::cout << "6. alignas is C++11 standard, #pragma pack is compiler-specific\n";
    std::cout << "7. Performance vs memory trade-offs depend on use case\n";
    std::cout << "8. Use static_assert to verify alignment requirements\n";
}

// =============================================================================
// SECTION 8: REAL-WORLD TRADING SYSTEM EXAMPLE
// =============================================================================

class UltraLowLatencyOrderProcessor {
private:
    // Hot path data - cache line aligned for maximum performance
    struct alignas(64) HotPathData {
        std::atomic<uint64_t> sequence_number{0};
        std::atomic<double> last_price{0.0};
        std::atomic<uint64_t> total_volume{0};
        // Padding ensures this gets its own cache line
    };

    // Network message - packed for protocol compliance
    #pragma pack(push, 1)
    struct OrderMessage {
        uint16_t msg_type;      // 2 bytes
        uint32_t order_id;      // 4 bytes
        uint64_t price;         // 8 bytes (fixed point)
        uint32_t quantity;      // 4 bytes
        uint16_t flags;         // 2 bytes
        // Total: exactly 20 bytes for network efficiency
    };
    #pragma pack(pop)

    // SIMD-optimized price calculation buffer
    struct alignas(32) PriceCalculationBuffer {
        double bid_prices[4];   // 32 bytes for AVX
        double ask_prices[4];   // 32 bytes for AVX

        void calculateMidPrices(double* results) const noexcept {
            __m256d bids = _mm256_load_pd(bid_prices);
            __m256d asks = _mm256_load_pd(ask_prices);
            __m256d two = _mm256_set1_pd(2.0);
            __m256d mids = _mm256_div_pd(_mm256_add_pd(bids, asks), two);
            _mm256_store_pd(results, mids);
        }
    };

    HotPathData hot_data_;
    PriceCalculationBuffer price_buffer_;

public:
    // Process incoming order with minimal latency
    bool processOrder(const OrderMessage& order) noexcept {
        // Hot path processing with aligned data access
        auto seq = hot_data_.sequence_number.fetch_add(1, std::memory_order_relaxed);

        // Convert fixed-point price to double
        double price = static_cast<double>(order.price) / 10000.0;
        hot_data_.last_price.store(price, std::memory_order_relaxed);

        return true; // Success
    }

    // Get current market state
    struct MarketState {
        uint64_t sequence;
        double last_price;
        uint64_t total_volume;
    };

    MarketState getMarketState() const noexcept {
        return {
            hot_data_.sequence_number.load(std::memory_order_relaxed),
            hot_data_.last_price.load(std::memory_order_relaxed),
            hot_data_.total_volume.load(std::memory_order_relaxed)
        };
    }
};

int main() {
    demonstrateAlignmentDifferences();

    std::cout << "\n=== TRADING SYSTEM EXAMPLE ===\n";
    UltraLowLatencyOrderProcessor processor;

    // Demonstrate the trading system
    std::cout << "Processing sample orders...\n";

    // This would typically come from network in packed format
    #pragma pack(push, 1)
    struct {
        uint16_t msg_type = 1;
        uint32_t order_id = 12345;
        uint64_t price = 1001250;  // $100.125 in fixed point
        uint32_t quantity = 1000;
        uint16_t flags = 0;
    } sample_order;
    #pragma pack(pop)

    processor.processOrder(reinterpret_cast<const UltraLowLatencyOrderProcessor::OrderMessage&>(sample_order));

    auto state = processor.getMarketState();
    std::cout << "Market state - Sequence: " << state.sequence
              << ", Last price: $" << state.last_price
              << ", Volume: " << state.total_volume << "\n";

    return 0;
}

/*
 * SUMMARY: ALIGNAS VS #PRAGMA PACK
 *
 * ALIGNAS (C++11):
 * ================
 * Purpose: Increase alignment for performance optimization
 * Use cases:
 * - Cache line alignment (64 bytes)
 * - SIMD operations (16/32 bytes)
 * - Atomic operations
 * - Memory pool alignment
 * - False sharing prevention
 *
 * Benefits:
 * + Improves performance
 * + Standard C++11 feature
 * + Portable across compilers
 * + Type-safe
 * + Compile-time verification
 *
 * Trade-offs:
 * - Increases memory usage
 * - May waste space with padding
 *
 * #PRAGMA PACK:
 * =============
 * Purpose: Decrease alignment for space efficiency
 * Use cases:
 * - Network protocol compliance
 * - File format specifications
 * - Hardware register layouts
 * - Memory-constrained systems
 * - C library compatibility
 *
 * Benefits:
 * + Reduces memory usage
 * + Exact layout control
 * + Protocol compliance
 * + Space efficiency
 *
 * Trade-offs:
 * - Often degrades performance
 * - Compiler-specific
 * - May cause unaligned access penalties
 * - Can break on some architectures
 *
 * PERFORMANCE IMPACT:
 * ==================
 * - Cache line alignment: 2-10x performance improvement
 * - SIMD alignment: 5-50% performance improvement
 * - Packed structures: 10-50% performance degradation
 * - Memory bandwidth: Highly dependent on access patterns
 *
 * BEST PRACTICES:
 * ===============
 * 1. Use alignas for hot path data structures
 * 2. Use #pragma pack only when protocol compliance required
 * 3. Verify alignment with static_assert
 * 4. Profile performance impact of alignment choices
 * 5. Consider cache line boundaries for frequently accessed data
 * 6. Use proper alignment for atomic operations
 * 7. Prevent false sharing with cache line alignment
 * 8. Test on target hardware architectures
 */
