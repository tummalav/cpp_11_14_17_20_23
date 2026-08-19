# Ultra Low Latency Order Book
## Files
| File | Description |
|------|-------------|
| `ull_orderbook.cpp` | Main order book implementation + benchmark |
| `itch_top10_aggregated_book.hpp` | C++20 ITCH order-by-order to aggregated top-10 book with hidden deeper levels |
| `itch_top10_aggregated_book_demo.cpp` | Standalone demo covering level promotion/demotion beyond visible top 10 |
| `JAVA_VS_CPP_ORDERBOOK.md` | Java vs C++ performance comparison |
## Latency Targets
| Operation | Target |
|-----------|--------|
| add_limit (no fill) | 50–100 ns |
| cancel | 20–50 ns |
| add_limit (1 fill) | 100–200 ns |
| SPSC push/pop | 10–20 ns |
## Build
```bash
g++ -std=c++17 -O3 -march=native -pthread ull_orderbook.cpp -o ull_orderbook
g++ -std=c++20 -O3 -march=native -pthread itch_top10_aggregated_book_demo.cpp -o itch_top10_aggregated_book_demo
```

## ITCH top-10 aggregated book
- **Use case**: order-by-order feeds like NASDAQ ITCH where clients consume only the best 10 levels.
- **Internal representation**: full active-price-level maintenance per side, so when L10 is removed the hidden L11 is promoted automatically, and when a better level arrives the old visible tail drops back into hidden depth.
- **Hot-path structures**:
  - `orders_`: flat open-addressing order map (`order_ref -> {side, price, remaining_qty, level_slot}`).
  - `levels_`: cache-line-sized price-level nodes with aggregate quantity, order count, and intrusive `prev/next` links.
  - `level_lookup_`: flat open-addressing price lookup for `side + price -> level_slot`.
  - `published_`: double-buffered, lock-free published top-10 snapshot for strategies/clients.
- **Cache behavior**:
  - updates touch one `OrderSlot` and one `LevelNode` in contiguous arrays;
  - published view uses **structure-of-arrays** (`prices[]`, `quantities[]`, `order_counts[]`) for consumer-side cache locality and easy compiler autovectorization;
  - `alignas(64)` is used on hot data to reduce false sharing.
- **Concurrency model**: single writer for feed reconstruction, many readers via a seqlock-style double-buffer snapshot. No mutexes on the hot path.
## See Also
- `../risk_management/ull_risk_manager.cpp`
- `../position_management/ull_position_tracker.cpp`
- `../exchange_handlers/fix_protocol/fix_engine.cpp`
- `../../02_ultra_low_latency/core/cpu_affinity_numa.cpp`
