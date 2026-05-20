# HKEX OCG-C Order Entry Plugin

Ultra-low latency order entry plugin for the Hong Kong Stock Exchange (HKEX) using the OCG-C protocol v4.9.

## Files

- `hkex_ocg_order_handler.hpp` - Plugin header with complete OCG-C v4.9 protocol implementation
- `hkex_ocg_order_handler.cpp` - Ultra-low latency plugin implementation
- `hkex_ocg_example_application.cpp` - Comprehensive usage examples
- `hkex_ocg_performance_test.cpp` - Advanced performance benchmarking
- `HKEX_OCG_README.md` - Complete documentation
- `HKEX_OCG_ANALYSIS.md` - Technical deep-dive analysis

## Quick Build

```bash
# Example application
g++ -std=c++17 -O3 -Wall -Wextra hkex_ocg_order_handler.cpp hkex_ocg_example_application.cpp -o hkex_example -pthread

# Performance test
g++ -std=c++17 -O3 -Wall -Wextra hkex_ocg_order_handler.cpp hkex_ocg_performance_test.cpp -o hkex_perf_test -pthread
```

## Performance

- **Latency**: < 3μs round-trip  
- **Throughput**: > 100,000 orders/sec
- **Memory**: Zero hot-path allocation
- **Jitter**: < 500ns P99

## Quick Start

```cpp
#include "hkex_ocg_order_handler.hpp"

auto plugin = createHKEXOCGPlugin();
plugin->initialize(config_json);
plugin->login();
plugin->sendNewOrder(order);
```

## Supported Markets

- Main Board (M)
- GEM (G) 
- Structured Products (S)
- Debt Securities (D)
- ETFs (E)
- REITs (R)
- China Connect (C)

See `HKEX_OCG_README.md` for complete documentation.
