#!/bin/bash
# =============================================================================
# SMART DIRECTORY REORGANIZATION SCRIPT
# =============================================================================
# New Structure:
#   01_cpp_features/         - C++11/14/17/20/23 language feature files
#      ├── cpp11/            - C++11 specific features
#      ├── cpp17/            - C++17 specific features
#      ├── cpp20/            - C++20 specific features
#      └── general/          - Cross-version: templates, SFINAE, design patterns
#
#   02_ultra_low_latency/    - ULL, lockfree, SIMD, networking, containers
#      ├── core/             - ULL trading pipeline, crossing engine, guidelines
#      ├── lockfree/         - Lock-free queues, ring buffers, ABA problem
#      ├── networking/       - Solarflare, RDMA, zero-copy, SIMD
#      └── containers/       - Abseil, Folly, STL comparisons
#
#   03_trading_apps/         - Live trading applications
#      ├── market_making/    - MM strategies, ETF, FX pricing, backtesting
#      ├── orderbook/        - Order book implementations
#      ├── feed_handlers/    - Tick/FX feed handlers
#      └── exchange_handlers/- HKEX OCG/OMD, ASX OUCH, NASDAQ ITCH
#
#   build_scripts/           - All build & benchmark scripts
# =============================================================================

set -e  # Exit on any error
BASE="/Users/tummalavenkatasateesh/github_repos/cpp_11_14_17_20_23"
cd "$BASE"

echo ""
echo "============================================================"
echo "  STARTING SMART REORGANIZATION"
echo "============================================================"

# =============================================================================
# STEP 1: CREATE NEW DIRECTORY STRUCTURE
# =============================================================================
echo ""
echo "[STEP 1] Creating directory structure..."

mkdir -p 01_cpp_features/cpp11
mkdir -p 01_cpp_features/cpp17
mkdir -p 01_cpp_features/cpp20
mkdir -p 01_cpp_features/general

mkdir -p 02_ultra_low_latency/core
mkdir -p 02_ultra_low_latency/lockfree
mkdir -p 02_ultra_low_latency/networking
mkdir -p 02_ultra_low_latency/containers

mkdir -p 03_trading_apps/market_making
mkdir -p 03_trading_apps/orderbook
mkdir -p 03_trading_apps/feed_handlers
mkdir -p 03_trading_apps/exchange_handlers/hkex_ocg
mkdir -p 03_trading_apps/exchange_handlers/hkex_omd
mkdir -p 03_trading_apps/exchange_handlers/asx_ouch
mkdir -p 03_trading_apps/exchange_handlers/nasdaq_itch

mkdir -p build_scripts

echo "  [OK] Directory structure created"

# =============================================================================
# STEP 2: C++ FEATURES  →  01_cpp_features/
# =============================================================================
echo ""
echo "[STEP 2] Moving C++ feature files..."

# ── C++11 Core Features ──────────────────────────────────────────────────────
echo "  [cpp11]"
mv -v cpp11_initialization_types.cpp          01_cpp_features/cpp11/   2>/dev/null || true
mv -v lvalue_rvalue_references.cpp            01_cpp_features/cpp11/   2>/dev/null || true
mv -v nullptr_vs_null_vs_zero.cpp             01_cpp_features/cpp11/   2>/dev/null || true
mv -v lambda_expressions.cpp                  01_cpp_features/cpp11/   2>/dev/null || true
mv -v range_based_for_loops.cpp               01_cpp_features/cpp11/   2>/dev/null || true
mv -v scoped_enums.cpp                        01_cpp_features/cpp11/   2>/dev/null || true
mv -v initializer_list_examples.cpp           01_cpp_features/cpp11/   2>/dev/null || true
mv -v shared_ptr_use_cases_examples.cpp       01_cpp_features/cpp11/   2>/dev/null || true
mv -v unique_ptr_use_cases_examples.cpp       01_cpp_features/cpp11/   2>/dev/null || true
mv -v weak_ptr_use_cases_examples.cpp         01_cpp_features/cpp11/   2>/dev/null || true
mv -v auto_use_cases.cpp                      01_cpp_features/cpp11/   2>/dev/null || true
mv -v auto_vs_decltype.cpp                    01_cpp_features/cpp11/   2>/dev/null || true
mv -v decltype_use_cases.cpp                  01_cpp_features/cpp11/   2>/dev/null || true
mv -v decltype_sizeof_noexcept_typeid_examples.cpp 01_cpp_features/cpp11/ 2>/dev/null || true
mv -v thread_async_packaged_task_examples.cpp 01_cpp_features/cpp11/   2>/dev/null || true

# ── C++17 Features ───────────────────────────────────────────────────────────
echo "  [cpp17]"
mv -v cpp17_parallel_algorithms_examples.cpp  01_cpp_features/cpp17/   2>/dev/null || true
mv -v std_string_view_use_cases.cpp           01_cpp_features/cpp17/   2>/dev/null || true
mv -v cpp14_17_20_23_initialization_features.cpp 01_cpp_features/cpp17/ 2>/dev/null || true

# ── C++20 Features ───────────────────────────────────────────────────────────
echo "  [cpp20]"
mv -v cpp20_concepts_use_cases_examples.cpp   01_cpp_features/cpp20/   2>/dev/null || true
mv -v cpp20_consteval_use_cases_examples.cpp  01_cpp_features/cpp20/   2>/dev/null || true
mv -v cpp20_constinit_use_cases_examples.cpp  01_cpp_features/cpp20/   2>/dev/null || true
mv -v cpp20_coroutines_use_cases_examples.cpp 01_cpp_features/cpp20/   2>/dev/null || true
mv -v cpp20_designated_initializers_examples.cpp 01_cpp_features/cpp20/ 2>/dev/null || true
mv -v cpp20_modules_use_cases_examples.cpp    01_cpp_features/cpp20/   2>/dev/null || true
mv -v cpp20_ranges_use_cases_examples.cpp     01_cpp_features/cpp20/   2>/dev/null || true
mv -v cpp20_three_way_comparison_examples.cpp 01_cpp_features/cpp20/   2>/dev/null || true

# ── General / Cross-version features ─────────────────────────────────────────
echo "  [general]"
mv -v cpp_attributes_use_cases.cpp            01_cpp_features/general/ 2>/dev/null || true
mv -v cpp_standards_features_overview.cpp     01_cpp_features/general/ 2>/dev/null || true
mv -v function_template_type_deduction_examples.cpp 01_cpp_features/general/ 2>/dev/null || true
mv -v sfinae_use_cases_examples.cpp           01_cpp_features/general/ 2>/dev/null || true
mv -v static_polymorphism_techniques.cpp      01_cpp_features/general/ 2>/dev/null || true
mv -v constexpr_evolution_and_comparison.cpp  01_cpp_features/general/ 2>/dev/null || true
mv -v atomic_memory_orderings_use_cases_examples.cpp 01_cpp_features/general/ 2>/dev/null || true
mv -v alignas_vs_pragma_pack_comparison.cpp   01_cpp_features/general/ 2>/dev/null || true
mv -v branch_prediction_modern_features.cpp   01_cpp_features/general/ 2>/dev/null || true
mv -v concurrency_vs_parallelization_examples.cpp 01_cpp_features/general/ 2>/dev/null || true
mv -v design_patterns.cpp                     01_cpp_features/general/ 2>/dev/null || true
mv -v design_patterns_examples.cpp            01_cpp_features/general/ 2>/dev/null || true

echo "  [OK] C++ features moved"

# =============================================================================
# STEP 3: ULTRA LOW LATENCY  →  02_ultra_low_latency/
# =============================================================================
echo ""
echo "[STEP 3] Moving ultra low latency files..."

# ── ULL Core ─────────────────────────────────────────────────────────────────
echo "  [core]"
mv -v ultra_low_latency_crossing_engine.cpp       02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ultra_low_latency_design_guidelines.cpp     02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ultra_low_latency_trading_pipeline.cpp      02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ultra_low_latency_trading_pipeline_part2.cpp 02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ultra_low_latency_trading_system.cpp        02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ultra_low_latency_orderbook.cpp             02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ultra_low_latency_orderbook_improved.cpp    02_ultra_low_latency/core/ 2>/dev/null || true
mv -v latency_benchmarking_examples.cpp           02_ultra_low_latency/core/ 2>/dev/null || true
# ULL core docs
mv -v ULTRA_LOW_LATENCY_ANALYSIS.md               02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ULTRA_LOW_LATENCY_SUMMARY.md                02_ultra_low_latency/core/ 2>/dev/null || true
mv -v ULTRA_LOW_LATENCY_TRADING_ARCHITECTURE.md   02_ultra_low_latency/core/ 2>/dev/null || true
mv -v TRADING_PIPELINE_ARCHITECTURE.md            02_ultra_low_latency/core/ 2>/dev/null || true
mv -v LATENCY_BENCHMARKING_README.md              02_ultra_low_latency/core/ 2>/dev/null || true

# ── Lock-Free ────────────────────────────────────────────────────────────────
echo "  [lockfree]"
mv -v lockfree_queue_variants_comprehensive_guide.cpp 02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v lockfree_ring_buffers_trading.cpp           02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v lockfree_shm_ring_buffers_ipc.cpp           02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v aba_problem_demo.cpp                        02_ultra_low_latency/lockfree/ 2>/dev/null || true
# Lockfree docs
mv -v LOCKFREE_QUEUES_COMPLETE_GUIDE.md           02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v LOCKFREE_QUEUES_QUICK_REFERENCE.txt         02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v LOCKFREE_QUEUES_SUMMARY.md                  02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v LOCKFREE_QUICK_REFERENCE.txt                02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v LOCKFREE_RING_BUFFERS_GUIDE.md              02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v LOCKFREE_SHM_IPC_GUIDE.md                  02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v ABA_PROBLEM_EXPLAINED.md                    02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v ABA_PROBLEM_README.md                       02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v ABA_PROBLEM_VISUAL_GUIDE.txt                02_ultra_low_latency/lockfree/ 2>/dev/null || true
# Lockfree binaries
mv -v aba_demo                                    02_ultra_low_latency/lockfree/ 2>/dev/null || true
mv -v lockfree_benchmark                          02_ultra_low_latency/lockfree/ 2>/dev/null || true

# ── Networking / Kernel Bypass ───────────────────────────────────────────────
echo "  [networking]"
mv -v solarflare_ef_vi_example.cpp                02_ultra_low_latency/networking/ 2>/dev/null || true
mv -v solarflare_tcpdirect_example.cpp            02_ultra_low_latency/networking/ 2>/dev/null || true
mv -v solarcapture_example.cpp                    02_ultra_low_latency/networking/ 2>/dev/null || true
mv -v zero_copy_mechanisms_ultra_low_latency.cpp  02_ultra_low_latency/networking/ 2>/dev/null || true
mv -v simd_vectorization_examples.cpp             02_ultra_low_latency/networking/ 2>/dev/null || true

# ── Containers (Abseil / Folly / STL) ────────────────────────────────────────
echo "  [containers]"
mv -v abseil_containers_comprehensive.cpp         02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v folly_containers_comprehensive.cpp          02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v stl_abseil_folly_containers_comparison.cpp  02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v ultra_low_latency_containers_comparison.cpp 02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v test_abseil_folly.cpp                       02_ultra_low_latency/containers/ 2>/dev/null || true
# Container docs & benchmarks
mv -v ABSEIL_CONTAINERS_GUIDE.md                  02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v CONTAINERS_BENCHMARK_README.md              02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v CONTAINERS_QUICK_REFERENCE.txt              02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v FOLLY_CONTAINERS_GUIDE.md                   02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v STL_ABSEIL_FOLLY_CONTAINERS_GUIDE.md        02_ultra_low_latency/containers/ 2>/dev/null || true
mv -v containers_comparison                       02_ultra_low_latency/containers/ 2>/dev/null || true

echo "  [OK] ULL files moved"

# =============================================================================
# STEP 4: TRADING APPLICATIONS  →  03_trading_apps/
# =============================================================================
echo ""
echo "[STEP 4] Moving trading application files..."

# ── Market Making ─────────────────────────────────────────────────────────────
echo "  [market_making]"
mv -v comprehensive_mm_strategies_pricing.cpp     03_trading_apps/market_making/ 2>/dev/null || true
mv -v etf_strategies_pricing_deep.cpp             03_trading_apps/market_making/ 2>/dev/null || true
mv -v market_making_backtesting_framework.cpp     03_trading_apps/market_making/ 2>/dev/null || true
mv -v mm_backtest_simple_test.cpp                 03_trading_apps/market_making/ 2>/dev/null || true
mv -v quote_aggregation_challenges.cpp            03_trading_apps/market_making/ 2>/dev/null || true
mv -v fx_pricestream.cpp                          03_trading_apps/market_making/ 2>/dev/null || true
mv -v fx_pricestream_enhanced.cpp                 03_trading_apps/market_making/ 2>/dev/null || true
# Market making docs
mv -v ETF_NEAR_TOUCH_FAR_TOUCH_PRICES.md          03_trading_apps/market_making/ 2>/dev/null || true
mv -v MARKET_MAKING_BACKTEST_README.md            03_trading_apps/market_making/ 2>/dev/null || true
# Binary
mv -v etf_strategies                              03_trading_apps/market_making/ 2>/dev/null || true

# ── Order Book ────────────────────────────────────────────────────────────────
echo "  [orderbook]"
mv -v orderbook_implementation.cpp                03_trading_apps/orderbook/ 2>/dev/null || true
mv -v JAVA_VS_CPP_ORDERBOOK.md                    03_trading_apps/orderbook/ 2>/dev/null || true

# ── Feed Handlers ─────────────────────────────────────────────────────────────
echo "  [feed_handlers]"
mv -v tick_feed_handler.cpp                       03_trading_apps/feed_handlers/ 2>/dev/null || true
mv -v fx_consolidated_feed_handler.cpp            03_trading_apps/feed_handlers/ 2>/dev/null || true

# ── Exchange Handlers (loose files in root) ───────────────────────────────────
echo "  [exchange_handlers - hkex_ocg]"
mv -v hkex_ocg_order_handler.cpp                  03_trading_apps/exchange_handlers/hkex_ocg/ 2>/dev/null || true
mv -v hkex_ocg_order_handler.hpp                  03_trading_apps/exchange_handlers/hkex_ocg/ 2>/dev/null || true
mv -v hkex_ocg_example_application.cpp            03_trading_apps/exchange_handlers/hkex_ocg/ 2>/dev/null || true
mv -v hkex_ocg_performance_test.cpp               03_trading_apps/exchange_handlers/hkex_ocg/ 2>/dev/null || true
mv -v HKEX_OCG_ANALYSIS.md                        03_trading_apps/exchange_handlers/hkex_ocg/ 2>/dev/null || true
mv -v HKEX_OCG_README.md                          03_trading_apps/exchange_handlers/hkex_ocg/ 2>/dev/null || true

echo "  [exchange_handlers - asx_ouch]"
mv -v ouch_asx_order_handler.cpp                  03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true
mv -v ouch_asx_order_handler.hpp                  03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true
mv -v ouch_example_application.cpp                03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true
mv -v ouch_performance_test.cpp                   03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true
mv -v ouch_plugin_manager.hpp                     03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true
mv -v ASX_OUCH_ANALYSIS.md                        03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true
mv -v ASX_OUCH_README.md                          03_trading_apps/exchange_handlers/asx_ouch/ 2>/dev/null || true

# ── Merge existing exchange_handlers/ folder content ─────────────────────────
echo "  [exchange_handlers - merging existing exchange_handlers/ folder]"
# HKEX OCG
cp -r exchange_handlers/hkex_ocg/.   03_trading_apps/exchange_handlers/hkex_ocg/  2>/dev/null || true
# HKEX OMD
cp -r exchange_handlers/hkex_omd/.   03_trading_apps/exchange_handlers/hkex_omd/  2>/dev/null || true
# ASX OUCH
cp -r exchange_handlers/asx_ouch/.   03_trading_apps/exchange_handlers/asx_ouch/  2>/dev/null || true
# NASDAQ ITCH
cp -r exchange_handlers/nasdaq_itch/. 03_trading_apps/exchange_handlers/nasdaq_itch/ 2>/dev/null || true
# Exchange handlers docs
mv -v EXCHANGE_PROTOCOLS_CONNECTIVITY.md  03_trading_apps/exchange_handlers/ 2>/dev/null || true

echo "  [OK] Trading app files moved"

# =============================================================================
# STEP 5: BUILD SCRIPTS  →  build_scripts/
# =============================================================================
echo ""
echo "[STEP 5] Moving build scripts..."

mv -v build_abseil_benchmark.sh    build_scripts/ 2>/dev/null || true
mv -v build_containers_benchmark.sh build_scripts/ 2>/dev/null || true
mv -v build_folly_benchmark.sh     build_scripts/ 2>/dev/null || true
mv -v build_lockfree_benchmark.sh  build_scripts/ 2>/dev/null || true
mv -v build_shm_ipc_benchmark.sh   build_scripts/ 2>/dev/null || true
mv -v run_benchmarks.sh            build_scripts/ 2>/dev/null || true
mv -v setup_market_making_framework.sh build_scripts/ 2>/dev/null || true

echo "  [OK] Build scripts moved"

# =============================================================================
# STEP 6: CLEANUP OLD exchange_handlers/ IF EMPTY AFTER MERGE
# =============================================================================
echo ""
echo "[STEP 6] Cleanup..."
# Remove old exchange_handlers directory after content was copied to new location
rm -rf exchange_handlers/ 2>/dev/null || true
echo "  [OK] Cleaned up old exchange_handlers/"

# =============================================================================
# STEP 7: PRINT FINAL STRUCTURE
# =============================================================================
echo ""
echo "============================================================"
echo "  REORGANIZATION COMPLETE! Final Structure:"
echo "============================================================"
echo ""
find . -not -path './.git/*' \
       -not -path './cmake-build-debug/*' \
       -not -path './config/*' \
       -not -name '*.o' \
       -not -name '*.d' \
  | sort \
  | grep -v "^\.$" \
  | sed 's|[^/]*/|  |g; s|  \([^/]*\)$|  └─ \1|'

echo ""
echo "============================================================"
echo "  Root level (kept):"
echo "    CMakeLists.txt   - build system"
echo "    build.sh         - main build script"
echo "    README.md        - project overview"
echo "    config/          - configuration files"
echo "============================================================"
echo ""
echo "  Done!"

