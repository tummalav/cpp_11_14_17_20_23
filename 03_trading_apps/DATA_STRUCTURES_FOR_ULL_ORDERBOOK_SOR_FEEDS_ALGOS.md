# Data Structures for Ultra-Low-Latency Order Books, SOR, Feed Handlers & Algos

## Executive Summary

Exchange crossing engines, Smart Order Routers (SOR), market data feed handlers, and
execution algos each stress different parts of the memory/compute hierarchy. Picking
the right data structure per hot-path role is more important than picking one "best"
library. This guide surveys **in-house (array/intrusive) implementations**, **Abseil**,
**Folly**, **moodycamel::ConcurrentQueue**, and a few other specialized libraries
(Boost.Intrusive, Boost.Lockfree, robin-hood-hashing, DPDK rte_ring), and maps each to
the four subsystems.

### Quick Recommendation

| Subsystem | Hot-path need | Recommended structure | Library / Origin |
|---|---|---|---|
| **Order book price levels** | Ordered price → FIFO queue, O(1) touch price | Flat array (price ladder) or intrusive doubly-linked list per level | **In-house** (array-indexed levels) |
| **Order book order lookup (cancel/replace by OrderID)** | O(1) hash lookup, dense, no allocation | Open-addressing flat hash map | **Abseil `flat_hash_map`** or in-house perfect-hash/slot-index |
| **Order pool (Order objects)** | Zero allocation on hot path | Fixed-size slab/free-list pool | **In-house `OrderPool`** (see `orderbook/ull_orderbook.cpp`) |
| **Network → matching engine ingress** | Single-producer/single-consumer, wait-free | Ring buffer (SPSC) | **In-house SPSC ring** or **DPDK rte_ring** (kernel-bypass) |
| **SOR venue book aggregation** | Merge N venue books, top-of-book scan | Small fixed-size arrays (SIMD-friendly) + intrusive min-heap for best price | **In-house** array/heap; **Boost.Intrusive** for heap nodes |
| **SOR routing table / venue metadata** | Rare writes, frequent reads, small N | Sorted vector / flat map | **Abseil `flat_hash_map`** or **Folly `sorted_vector_map`** |
| **Feed handler symbol table** | 1M+ symbols, O(1) lookup, cold-storage friendly | Perfect/minimal hash or dense hash map | **Abseil `flat_hash_map`**, **Folly `F14FastMap`** |
| **Feed handler packet queue (MPSC)** | Multiple NIC queues/cores → single decode thread | Lock-free MPSC queue | **moodycamel::ConcurrentQueue** or **moodycamel::ReaderWriterQueue** (SPSC variant) |
| **Feed handler sequence-gap/replay buffer** | Bounded ring, overwrite-oldest | Ring buffer with sequence stamps | **In-house** ring buffer |
| **Algo/strategy child-order tracking** | Frequent insert/update/erase by order id | Open-addressing hash map | **Abseil `flat_hash_map`** |
| **Algo time-series (VWAP/TWAP buckets, book snapshots)** | Append-mostly, iterate in order | Flat vector / ring buffer | **In-house** vector/ring, or **Folly `small_vector`** for SSO |
| **Cross-thread stats/telemetry publish** | Single writer, many readers, no locks | Seqlock / double-buffer | **In-house** (see `telemetry_corvil_rdtsc.cpp` pattern) |

---

## Table of Contents

1. [Design Principles for ULL Systems](#design-principles-for-ull-systems)
2. [Order Book Data Structures](#order-book-data-structures)
3. [Smart Order Router (SOR) Data Structures](#smart-order-router-sor-data-structures)
4. [Market Data Feed Handler Data Structures](#market-data-feed-handler-data-structures)
5. [Algo / Strategy Data Structures](#algo--strategy-data-structures)
6. [Library Cheat Sheet: In-house vs Abseil vs Folly vs moodycamel](#library-cheat-sheet-in-house-vs-abseil-vs-folly-vs-moodycamel)
7. [Decision Matrix](#decision-matrix)
8. [References in This Repo](#references-in-this-repo)

---

## Design Principles for ULL Systems

These constraints drive every choice below:

1. **No allocation on the hot path.** Use pools/slabs/arenas sized at startup.
2. **Cache-line awareness.** `alignas(64)` hot structs; separate read-mostly and
   write-mostly fields onto different cache lines to avoid false sharing.
3. **Mechanical sympathy over asymptotic elegance.** A flat array scanned linearly
   often beats a tree/hash map when N is small (e.g., <64 price levels, <20 venues).
4. **Prefer intrusive containers.** Embed list/tree hooks directly in the object
   (Boost.Intrusive style) to avoid a second allocation and a pointer chase.
5. **Single-producer/single-consumer (SPSC) wherever the topology allows it** —
   it is provably wait-free and needs no CAS loop, unlike MPMC/MPSC queues.
6. **Branch-light code on the matching path** — array indexing (`price_to_level[]`)
   beats `if/else` chains or map lookups for price → level resolution.

---

## Order Book Data Structures

### 1. Price ladder (bid/ask levels)

**In-house (recommended for the matching engine itself):**
- Fixed-size array indexed by `(price - base_price) / tick_size`, one array for
  bids, one for asks (separate allocations → no false sharing between sides).
- Each `PriceLevel` holds aggregate qty + head/tail indices into an intrusive
  FIFO (time-priority) list of `Order*`, not a `std::list` or `std::deque`.
- This is exactly the pattern used in `orderbook/ull_orderbook.cpp`
  (`PriceLevel`, `alignas(64) Order`, intrusive `enqueue`/`dequeue`).
- **Why not `std::map<price, level>` or `abseil::btree_map`?** Direct array
  indexing is O(1) with no comparisons/rotations; a book only has tens to a
  few hundred active levels, so a dense array is both faster and simpler than
  a balanced tree. Reserve the ordered-map approach (`abseil::btree_map`) for
  **sparse** books (e.g., illiquid options chains with huge strike ranges) where
  a full-range array would waste memory.

### 2. Order lookup by Order ID (for cancel/replace)

- **In-house slot-index map**: if your OMS assigns sequential/dense internal
  order IDs, use a flat `std::vector<Order*>` indexed directly by ID modulo pool
  size — O(1), no hashing at all.
- **When IDs are sparse/exchange-assigned** (e.g., FIX `ClOrdID`, exchange order
  tokens): **Abseil `flat_hash_map<uint64_t, Order*>`** — open addressing, SIMD
  probing (SSE2/SSSE3 "Swiss Tables"), ~25-35ns lookup vs 50-70ns for
  `std::unordered_map`. **Folly `F14FastMap`** is a close/faster alternative
  (20-30ns) if you already depend on Folly.

### 3. Order object storage

- **In-house fixed-capacity pool** (`OrderPool` in `ull_orderbook.cpp`):
  pre-allocated array of `alignas(64) Order`, free-list of indices, huge-page
  friendly. Zero `malloc`/`free` on the hot path — mandatory for a matching
  engine's tail latency.

### 4. Network ingress queue (feed → matching engine)

- **In-house SPSC ring buffer** with acquire/release atomics on head/tail
  (see `SPSCQueue` in `ull_orderbook.cpp`, and the broader survey in
  `02_ultra_low_latency/lockfree/ringbuffer_all_variants_capital_markets.cpp`).
- If the topology is genuinely single-producer/single-consumer,
  **do not reach for a general MPMC queue** — SPSC is simpler, wait-free, and
  faster (no CAS retry loop).
- For kernel-bypass NIC ingress (Solarflare/Onload, DPDK), the vendor ring
  buffer (`ef_vi`, `rte_ring`) replaces this layer entirely; see
  `02_ultra_low_latency/networking/solarflare_ef_vi_example.cpp`.

### 5. Top-of-book / L1 snapshot for market data out

- Small fixed struct (best bid/ask price+qty), published via a **seqlock or
  double-buffer** (in-house) so a reader thread never blocks the writer
  (matching engine) thread — avoids a lock on the absolute hottest path.

---

## Smart Order Router (SOR) Data Structures

The SOR aggregates N venue order books, decides where to slice/route child
orders, and must react within microseconds to any venue's top-of-book change.

### 1. Aggregated top-of-book across venues

- **In-house fixed-size array** of `VenueQuote{price, qty, venue_id}`, one slot
  per venue (N is small — tens, not millions). Linear scan for best price is
  faster than a heap for N < ~32 due to SIMD-friendly cache-line scans.
- If N is larger (broker aggregating 100+ liquidity pools/dark pools), use an
  **intrusive min/max-heap** (Boost.Intrusive `heap` or hand-rolled binary heap
  over an array) keyed by price so best-price extraction is O(log N) instead
  of O(N).

### 2. Per-venue full depth book (if SOR does depth-aware slicing)

- Same pattern as the exchange order book above: array-indexed price ladder,
  one instance per venue. Reuse the in-house `PriceLevel`/pool design.

### 3. Venue routing table / static metadata (fees, latency, fill-rates, order-type support)

- Read-mostly, written only on config reload. **Folly `sorted_vector_map`** or
  a plain sorted `std::vector` + binary search — no need for hash map overhead
  when N (venues) is small and updates are rare. **Abseil `flat_hash_map`** is
  fine too if lookups are by venue-name string rather than a small enum/int.

### 4. Child-order → parent-order tracking (fan-out/fan-in)

- **Abseil `flat_hash_map<uint64_t child_id, ParentOrderCtx*>`** — high churn
  (created/erased per child order), needs fast insert/erase, not just lookup.

### 5. Order-routing decision queue (strategy thread → venue gateway threads)

- **moodycamel::ConcurrentQueue** (MPMC) when one router core fans out to
  multiple venue-gateway threads, or **moodycamel::ReaderWriterQueue** (SPSC)
  per-venue-gateway if you shard by venue (preferred — turns an MPMC problem
  into N independent SPSC problems, which is always faster).

---

## Market Data Feed Handler Data Structures

### 1. Symbol → book/state lookup

- Exchanges publish numeric instrument IDs (e.g., ITCH `stock_locate`,
  CME MDP3 `SecurityID`) that are often **dense small integers** — use a flat
  **array** indexed directly by ID (fastest possible, O(1), no hashing).
- When IDs are sparse or you must support multi-exchange symbol mapping,
  **Abseil `flat_hash_map`** or **Folly `F14FastMap`** for the string/ID → index
  translation layer (done once per session, not per message).

### 2. Packet/message queue: NIC → decode thread(s)

- **moodycamel::ReaderWriterQueue** if one NIC RX ring feeds one decode thread
  (SPSC) — lowest latency, no CAS.
- **moodycamel::ConcurrentQueue** if multiple NIC queues/cores feed a shared
  pool of decode threads (MPMC) — lock-free, blocks-based allocation amortizes
  allocation cost, supports bulk enqueue/dequeue which matters for
  high-message-rate bursts (options market data, e.g., HKEX OMD, CME MDP3).
- **In-house SPSC ring buffer** remains the best choice when the topology is
  strictly 1:1 and you want full control over cache-line layout (see
  `02_ultra_low_latency/lockfree/lockfree_shm_ring_buffers_ipc.cpp` for the
  shared-memory IPC variant used to hand off between processes).

### 3. Order-book reconstruction state (per-symbol L2/L3 book from ITCH/OUCH/MDP3)

- Same array-indexed price-ladder + intrusive order list as the exchange
  order book (Section: Order Book). See `orderbook/itch_top10_aggregated_book.hpp`
  for a concrete "top-10 aggregated book" example already in this repo.

### 4. Sequence-number gap detection / replay buffer

- **In-house ring buffer** storing the last K raw messages (or a bitmap of
  received sequence numbers) so a gap-fill/replay request can be served
  without hitting a slower recovery/snapshot channel.

### 5. Multicast/UDP reordering buffer (out-of-order packet arrival)

- Small fixed-size **sliding window array** indexed by `seq % window_size` —
  not a general priority queue; ULL feed handlers exploit the fact that
  reordering depth is bounded (a handful of packets) to use O(1) slotting
  instead of a heap.

---

## Algo / Strategy Data Structures

### 1. Child order tracking (TWAP/VWAP/POV/Participate algos)

- **Abseil `flat_hash_map<OrderID, ChildOrderState>`** for active child orders —
  matches the pattern already used in `execution_algos/algo_common.hpp`.
- Keep the struct small and `alignas(64)` if the algo runs on a shared core
  with the matching/feed threads, to avoid false sharing.

### 2. Volume/time bucket accumulators (VWAP curve, participation targets)

- **In-house flat array or ring buffer** of buckets (e.g., 390 one-minute
  buckets for a trading day) — indexed directly by bucket number, no map
  needed since bucket count is fixed and known at algo start.

### 3. Historical/rolling window calculations (moving average, realized vol, imbalance)

- **In-house fixed-capacity ring buffer** (`std::array` + head index) for O(1)
  push and O(1) window-sum via a running total — avoids `std::deque` allocation
  churn.
- **Folly `small_vector`** is useful where the window size is usually small
  but occasionally grows (e.g., an order's fill history) — it avoids heap
  allocation for the common small case (SSO for vectors).

### 4. Strategy → OMS/EMS order state machine

- **In-house pool + flat_hash_map by order id**, same pattern as the SOR's
  child-order tracking. Keep state transitions branch-predictable
  (`enum class OStatus` in `ull_orderbook.cpp` is a good template).

### 5. Cross-strategy shared market data cache (last trade, book snapshot, greeks)

- Read-mostly, high fan-out (many strategies read, one feed handler writes).
  **In-house double-buffer/seqlock per symbol**, or **Abseil `flat_hash_map`**
  from symbol → pointer into a snapshot arena if the number of symbols is
  large and dynamic.

### 6. Order book replay / backtesting containers

- Since backtests are not on the live hot path, this is one place where
  **Folly `sorted_vector_map`** (bulk-loaded once, read many times) or plain
  `std::vector` + `std::sort` is appropriate — optimize for iteration/memory,
  not update speed. See `market_making/market_making_backtesting_framework.cpp`.

---

## Library Cheat Sheet: In-house vs Abseil vs Folly vs moodycamel

| Library | Best for | Avoid for | Notes |
|---|---|---|---|
| **In-house (array/intrusive/pool)** | Matching-engine hot path: price ladders, order pools, SPSC rings, seqlocks | Anything needing dynamic/sparse keyspace with large N | Full control of memory layout, cache-line alignment, zero allocation; highest engineering cost |
| **Abseil `flat_hash_map`/`btree_map`** | Order-id lookup, symbol tables, routing tables, child-order tracking | Extremely hot single-digit-nanosecond loops where array indexing is possible | Swiss Tables, SIMD probing, ~2x faster than `std::unordered_map`, drop-in API compatible |
| **Folly `F14FastMap`/`sorted_vector_map`/`small_vector`** | Read-heavy reference data, backtesting containers, SSO-friendly small collections | Write-heavy hot structures needing predictable single-digit-ns latency | Fastest raw hash map benchmark numbers; heavier dependency footprint than Abseil |
| **moodycamel::ConcurrentQueue** | MPMC queues: multi-core feed handler fan-in, SOR fan-out to venue gateways | Strict SPSC topologies (use `ReaderWriterQueue` or in-house SPSC instead — faster) | Lock-free, supports bulk ops, widely used in HFT/market-data pipelines |
| **moodycamel::ReaderWriterQueue** | SPSC when you want a maintained, tested library instead of hand-rolled ring | Cases needing shared-memory (cross-process) IPC | Single header, very low overhead, good default for SPSC unless you need SHM |
| **Boost.Intrusive** | Embedding list/tree/heap hooks directly in Order/Venue objects | Simple, low-N collections where a flat array suffices | Avoids secondary allocation + pointer chase; pairs well with in-house pools |
| **Boost.Lockfree** | Quick prototyping of lock-free queues/stacks before writing bespoke SPSC | Final production matching-engine ingress (in-house SPSC usually still wins on tail latency) | Good reference implementation, less tuned than a bespoke ring |
| **DPDK `rte_ring` / Solarflare `ef_vi`** | Kernel-bypass NIC ingress replacing the OS network stack entirely | Anything above the NIC/driver layer | See `02_ultra_low_latency/networking/` for worked examples in this repo |

---

## Decision Matrix

| Question | If yes → | If no → |
|---|---|---|
| Is the key space dense & known at startup (e.g., exchange instrument IDs, sequential internal order IDs)? | Flat array, direct index | Hash map (Abseil/Folly) |
| Is N (levels/venues/symbols) small (<~64) and fits in a few cache lines? | Linear scan over array beats hash/tree | Hash map or B-tree for O(1)/O(log N) |
| Is the producer/consumer topology strictly 1:1? | SPSC (in-house or `moodycamel::ReaderWriterQueue`) | MPMC (`moodycamel::ConcurrentQueue`) |
| Does the collection need range queries (best-N prices, price ≥ X)? | Ordered structure: array price-ladder, or `abseil::btree_map` if sparse | Hash map is fine |
| Is this on the absolute hot path (matching engine tick-to-trade)? | In-house, zero-allocation, cache-line aligned | Library container (Abseil/Folly) acceptable |
| Is this read-mostly reference/config data? | `folly::sorted_vector_map` or plain sorted vector | Hash map for write-heavy data |
| Do you need cross-process (not just cross-thread) hand-off? | Shared-memory ring buffer (in-house SHM IPC) | In-process queue (moodycamel/in-house) |

---

## References in This Repo

- `03_trading_apps/orderbook/ull_orderbook.cpp` — in-house array price-ladder,
  intrusive FIFO, `OrderPool`, SPSC ingress queue, cache-line-aligned `Order`.
- `03_trading_apps/orderbook/itch_top10_aggregated_book.hpp` — feed-handler-side
  book reconstruction (top-10 aggregated depth) example.
- `03_trading_apps/feed_handlers/ull_feed_handler_infrastructure.hpp`,
  `ull_market_feed_handlers.hpp`, `ull_multicontainer_ipc.hpp` — feed handler
  queueing and multi-container IPC patterns.
- `03_trading_apps/execution_algos/algo_common.hpp` and `twap_algo.cpp`,
  `vwap_algo.cpp`, `participate_algo.cpp`, `moc_algo.cpp`, `moo_algo.cpp` — algo
  child-order tracking patterns.
- `02_ultra_low_latency/lockfree/ringbuffer_all_variants_capital_markets.cpp`,
  `lockfree_shm_ring_buffers_ipc.cpp` — SPSC/MPSC ring buffer variants,
  including shared-memory IPC rings.
- `02_ultra_low_latency/containers/STL_ABSEIL_FOLLY_CONTAINERS_GUIDE.md`,
  `ABSEIL_CONTAINERS_GUIDE.md`, `FOLLY_CONTAINERS_GUIDE.md` — detailed
  container-level benchmarks (hash maps, ordered maps) referenced throughout
  this document.
- `02_ultra_low_latency/networking/solarflare_ef_vi_example.cpp`,
  `solarflare_tcpdirect_example.cpp` — kernel-bypass NIC ingress alternatives
  to in-process SPSC queues.
