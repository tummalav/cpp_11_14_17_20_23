# Modern C++ and Low-Latency Trading Systems

Examples and reference implementations covering C++11 through C++23, ultra-low-latency engineering, and electronic trading systems. The repository combines standalone language-feature demonstrations with connected market-data, order-book, execution, and exchange-connectivity components.

## Repository Contents

| Directory | Description |
|---|---|
| `01_cpp_features/` | Standalone examples organized by C++ standard, plus templates, constexpr, concurrency, and design patterns |
| `02_ultra_low_latency/` | Latency measurement, CPU and NUMA techniques, lock-free data structures, IPC, networking, and cache-aware containers |
| `03_trading_apps/` | Exchange handlers, market-data feed handlers, order books, execution algorithms, smart order routing, market making, risk, and position management |
| `build_scripts/` | Build and benchmark scripts for selected components |
| `config/` | Configuration templates used by the exchange-handler examples |

The examples are intended as learning and engineering references. Build requirements and operational suitability vary by component; consult its local README or build instructions before use.

## Getting Started

### Build a C++ feature example

The helper script selects the best supported standard available on the system:

```bash
./compile_cpp20.sh 01_cpp_features/cpp20/cpp20_concepts_use_cases_examples.cpp
```

### Build the ASX OUCH example

The root CMake project builds the ASX OUCH plugin and related example programs:

```bash
./build.sh release
```

Other supported modes are `./build.sh debug`, `./build.sh performance`, and `./build.sh clean`. See `03_trading_apps/exchange_handlers/asx_ouch/README.md` for component-specific details.

### Build selected benchmarks

Benchmark scripts are available in `build_scripts/`, including:

```bash
build_scripts/build_lockfree_benchmark.sh
build_scripts/build_containers_benchmark.sh
build_scripts/build_shm_ipc_benchmark.sh
```

Follow each script's instructions for its working directory and optional dependencies.

## Trading-System Overview

The trading components illustrate a typical event path:

```text
Exchange market data
    -> feed handler and order book
    -> strategy and execution algorithm
    -> pre-trade risk checks
    -> smart order router and exchange gateway
    -> acknowledgments, fills, and position updates
```

Protocol-specific handlers, local build instructions, and design notes live beside their implementations under `03_trading_apps/`.

## Further Reading

- `02_ultra_low_latency/core/ULTRA_LOW_LATENCY_ANALYSIS.md` — low-latency design considerations
- `02_ultra_low_latency/core/LATENCY_BENCHMARKING_README.md` — latency measurement and benchmark guidance
- `02_ultra_low_latency/core/TRADING_PIPELINE_ARCHITECTURE.md` — end-to-end trading pipeline
- `02_ultra_low_latency/lockfree/LOCKFREE_SHM_IPC_GUIDE.md` — shared-memory ring-buffer IPC
- `03_trading_apps/exchange_handlers/EXCHANGE_PROTOCOLS_CONNECTIVITY.md` — exchange protocol overview
- `03_trading_apps/orderbook/JAVA_VS_CPP_ORDERBOOK.md` — order-book implementation trade-offs
