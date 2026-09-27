#!/usr/bin/env python3
f = "/Users/tummalavenkatasateesh/github_repos/cpp_11_14_17_20_23/03_trading_apps/vwap_algo_engine.cpp"
content = open(f).read()

header = (
    "// =============================================================================\n"
    "// VWAP ALGO ENGINE -- Ultra-Low Latency Implementation\n"
    "// =============================================================================\n"
    "//\n"
    "// Architecture:\n"
    "//   VWAPAlgoEngine\n"
    "//     +-- VolumeCurveEngine   -- U-shaped historical + real-time blended curve\n"
    "//     +-- SchedulerEngine     -- per-bucket qty allocation, O(1) array index\n"
    "//     +-- ProgressTracker     -- lock-free atomic progress ratio\n"
    "//     +-- ChildOrderGenerator -- xorshift64 PRNG, RDTSC timestamps\n"
    "//     +-- SmartRouter         -- dark pool first, lit venue fallback\n"
    "//     +-- MarketImpactModel   -- SQRT / Almgren-Chriss model\n"
    "//     +-- PerformanceMonitor  -- atomic VWAP slippage + IS tracking\n"
    "//     +-- LatencyHistogram    -- RDTSC nano-histogram on hot path\n"
    "//\n"
    "// ULL Design (14 hot-path optimizations):\n"
    "//  #1  lock-free atomics       -- no std::mutex on hot path\n"
    "//  #2  std::array O(1)         -- no std::vector linear scans\n"
    "//  #3  rate-limited curve      -- rebuild every 50 ticks, not every tick\n"
    "//  #4  deferred schedule rebuild -- per bucket boundary, not per fill\n"
    "//  #5  SPSC async log queue    -- no std::cout on critical path\n"
    "//  #6  const char* tables      -- no std::string heap allocations\n"
    "//  #7  xorshift64 PRNG         -- 4x faster than std::mt19937\n"
    "//  #8  RDTSC timestamp         -- less than 5ns vs ~20ns for std::chrono\n"
    "//  #9  alignas(64)             -- cache-line isolated hot structs\n"
    "//  #10 double sentinel         -- no std::optional overhead\n"
    "//  #11 SPSC tick queue         -- lock-free producer/consumer decoupling\n"
    "//  #12 CPU affinity            -- pthread_setaffinity_np dedicated core\n"
    "//  #13 LIKELY/UNLIKELY         -- __builtin_expect branch hints\n"
    "//  #14 RDTSC histogram         -- nanosecond hot-path latency measurement\n"
    "//\n"
    "// Compile:\n"
    "//   g++ -std=c++17 -O3 -march=native -pthread \\\n"
    "//       vwap_algo_engine.cpp -o vwap_algo_engine\n"
    "// =============================================================================\n"
    "\n"
)

inc_idx = content.find("#include ")
new_content = header + content[inc_idx:]
open(f, "w").write(new_content)
print("Done. Lines:", new_content.count("\n"))
print("First 3 lines:")
for line in new_content.splitlines()[:3]:
    print(" ", line)

