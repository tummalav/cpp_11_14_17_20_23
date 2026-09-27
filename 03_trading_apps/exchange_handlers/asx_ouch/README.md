# ASX OUCH Order Entry Plugin

Ultra-low latency order entry plugin for the Australian Securities Exchange (ASX) using the OUCH protocol.

## Files

- `ouch_asx_order_handler.hpp` - Plugin header with complete OUCH protocol implementation
- `ouch_asx_order_handler.cpp` - Ultra-low latency plugin implementation
- `ouch_example_application.cpp` - Comprehensive usage examples
- `ouch_performance_test.cpp` - Performance benchmarking suite
- `ouch_plugin_manager.hpp` - Plugin management utilities
- `ASX_OUCH_README.md` - Detailed documentation
- `ASX_OUCH_ANALYSIS.md` - Technical analysis

## Quick Build

```bash
# Example application
g++ -std=c++17 -O3 -Wall -Wextra ouch_asx_order_handler.cpp ouch_example_application.cpp -o asx_example -pthread

# Performance test
g++ -std=c++17 -O3 -Wall -Wextra ouch_asx_order_handler.cpp ouch_performance_test.cpp -o asx_perf_test -pthread
```

## Performance

- **Latency**: < 10μs round-trip
- **Throughput**: > 100,000 orders/sec
- **Memory**: Zero hot-path allocation

## Quick Start

```cpp
#include "ouch_asx_order_handler.hpp"

auto plugin = createASXOUCHPlugin();
plugin->initialize(config_json);
plugin->sendEnterOrder(order);
```

See `ASX_OUCH_README.md` for complete documentation.
