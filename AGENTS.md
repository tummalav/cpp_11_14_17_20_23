# AGENTS.md — AI Agent Guide for cpp_11_14_17_20_23

## Project Overview

This is a **dual-purpose C++ repository**:
1. **Learning reference** — annotated examples of C++11/14/17/20/23 language features (`01_cpp_features/`)
2. **Ultra-low latency (ULL) HFT system** — production-grade capital markets code (`02_ultra_low_latency/`, `03_trading_apps/`)

These two purposes coexist in the same repo but serve different audiences. Treat `01_cpp_features/` as standalone educational files; treat `02_ultra_low_latency/` and `03_trading_apps/` as a coherent ULL system.

---

## Directory Map

```
01_cpp_features/cpp{11,14,17,20,23}/   Standalone feature demos (each file self-contained)
01_cpp_features/general/               Templates, SFINAE, constexpr, design patterns
02_ultra_low_latency/
  core/          CPU affinity, NUMA, latency benchmarking, trading pipeline docs
  lockfree/      Ring buffers, ABA problem, lock-free IPC over shared memory
  networking/    Low-latency network I/O
  containers/    Cache-friendly containers
03_trading_apps/
  exchange_handlers/{asx_ouch,hkex_ocg,hkex_omd,nasdaq_itch,fix_protocol}/
  feed_handlers/     Market data ingestion
  orderbook/         ULL order book (price-level, lock-free)
  market_making/     MM strategy framework
  risk_management/   Pre-trade risk checks
  position_management/
sgx_hackerrank_problems/               Competitive programming / interview problems
```

---

## Build System

### CMake (ASX OUCH plugin — root `CMakeLists.txt`)
```bash
./build.sh release       # Release build → build/
./build.sh debug         # Debug build
./build.sh performance   # Extra LTO + loop unrolling
./build.sh clean         # Wipe build/
```
Outputs: `build/ouch_asx_plugin.so`, `build/ouch_example`, `build/ouch_performance_test`

### Individual files (feature demos & benchmarks)
Use `compile_cpp20.sh` — auto-detects best available standard (C++20 → C++17 → C++14):
```bash
./compile_cpp20.sh 01_cpp_features/cpp20/cpp20_concepts_use_cases_examples.cpp
```

`build_scripts/run_benchmarks.sh` runs from the repo root and compiles/runs `quick_latency_test.cpp` and `latency_benchmarking_examples.cpp` (plus the optional atomic benchmark when present).

Per-component benchmark build scripts live in `build_scripts/`:
```bash
build_scripts/build_lockfree_benchmark.sh   # Lock-free ring buffers
build_scripts/build_containers_benchmark.sh
build_scripts/build_shm_ipc_benchmark.sh
```
`build_scripts/build_abseil_benchmark.sh` and `build_scripts/build_folly_benchmark.sh` check/install their external deps before compiling `abseil_benchmark` / `folly_benchmark`.

These scripts expect to be run **from the component's source directory** (they use relative paths); the exchange-handler subdirectories (`03_trading_apps/exchange_handlers/{asx_ouch,hkex_ocg,hkex_omd,nasdaq_itch}/build.sh`) are also standalone and run from their own directories.

---

## Key Patterns & Conventions

### ULL Code Rules
- **Zero hot-path allocation**: no `new`/`malloc` in order submission or market data paths.
- **`alignas(64)`** on all structures that are cache-line sensitive.
- RDTSC-based latency measurement (see `02_ultra_low_latency/telemetry_corvil_rdtsc.cpp`).
- CPU pinning via `pthread_setaffinity_np` / `sched_setaffinity` (see `core/cpu_affinity_numa.cpp`).
- Lock-free structures use `std::atomic` with explicit `memory_order`; never use `memory_order_seq_cst` in hot paths.

### Exchange Handler Pattern
Exchange handlers now split into two concrete shapes:
1. **Order-entry plugins** such as `03_trading_apps/exchange_handlers/asx_ouch/ouch_asx_order_handler.hpp` and `03_trading_apps/exchange_handlers/hkex_ocg/hkex_ocg_order_handler.hpp` expose `initialize(...)`, session/login control, and order methods like `sendEnterOrder(...)`, `sendNewOrder(...)`, `sendCancelOrder(...)`, and `sendReplaceOrder(...)` plus `registerEventHandler(...)`.
2. **Market-data feed handlers** such as `03_trading_apps/exchange_handlers/hkex_omd/hkex_omd_feed_handler.hpp` and `03_trading_apps/exchange_handlers/nasdaq_itch/nasdaq_itch_feed_handler.hpp` expose `connect(...)`, `subscribe(...)`, order-book access, and callback interfaces (`IOMDEventHandler`, CRTP-style `ITCHEventHandler`).
3. Keep the header/implementation pair beside each protocol, and use the local `README.md` / `build.sh` in that protocol directory as the canonical usage/build entry point.

### C++ Standard Targets
- Feature demos: use the standard being demonstrated; files are standalone.
- ULL/trading code: **C++17 minimum**, C++20 preferred; CMake enforces `CMAKE_CXX_STANDARD 20`.
- Compiler flags for release: `-O3 -march=native -mtune=native -flto -ffast-math`.

### Naming
- ULL source files: `snake_case` with descriptive prefix (`ull_orderbook.cpp`, `lockfree_shm_ring_buffers_ipc.cpp`).
- Exchange handlers: `{exchange}_{protocol}_{role}.{hpp,cpp}` (e.g., `ouch_asx_order_handler.hpp`).
- Benchmark executables: `*_benchmark` or `*_performance_test`.

---

## Integration Points

| Component | Communicates Via |
|---|---|
| Exchange handlers | TCP sockets for order entry; multicast UDP/MoldUDP64 for market data |
| Feed handlers → Orderbook | Lock-free SPSC ring buffer or callback delivery (shared memory or in-process) |
| Orderbook → Strategy | Callback / lock-free queue, never blocking |
| Risk checks | Inline on order submission path, not async |

Config for the OUCH plugin is JSON: `config/ouch_config.json.in` → `build/ouch_config.json`.

---

## Key Reference Files

- `02_ultra_low_latency/core/ULTRA_LOW_LATENCY_ANALYSIS.md` — design rationale for ULL choices
- `02_ultra_low_latency/core/LATENCY_BENCHMARKING_README.md` — benchmark suite details and `run_benchmarks.sh`
- `02_ultra_low_latency/core/TRADING_PIPELINE_ARCHITECTURE.md` — end-to-end OMS/SOR/risk/execution pipeline
- `02_ultra_low_latency/lockfree/LOCKFREE_SHM_IPC_GUIDE.md` — IPC ring buffer architecture
- `03_trading_apps/exchange_handlers/asx_ouch/ASX_OUCH_ANALYSIS.md` — OUCH protocol deep-dive
- `03_trading_apps/orderbook/JAVA_VS_CPP_ORDERBOOK.md` — design trade-offs
- `03_trading_apps/exchange_handlers/EXCHANGE_PROTOCOLS_CONNECTIVITY.md` — cross-exchange overview

