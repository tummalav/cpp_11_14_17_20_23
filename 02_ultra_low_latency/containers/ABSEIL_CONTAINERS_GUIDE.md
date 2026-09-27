# Abseil Containers - Quick Reference Guide

Google's Abseil C++ Library - High-Performance Containers for Ultra-Low Latency Systems

---

## 📦 Installation

### macOS
```bash
brew install abseil
```

### RHEL/Linux
```bash
git clone https://github.com/abseil/abseil-cpp.git
cd abseil-cpp && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc) && sudo make install
```

---

## 🔨 Compilation

```bash
g++ -std=c++17 -O3 -march=native -DNDEBUG \
    your_code.cpp \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    -labsl_base -labsl_hash -labsl_raw_hash_set -labsl_strings \
    -lpthread -o your_app
```

---

## 📊 Container Overview

### Hash Containers (Swiss Tables)

#### `absl::flat_hash_map<K, V>`
- **Latency:** 15-60ns lookup (P99: 120ns)
- **Storage:** Inline (values in table)
- **Pointers:** Invalidated on rehash
- **Best for:** Fast lookups, general purpose
- **Use case:** Symbol → Price mapping

```cpp
#include "absl/container/flat_hash_map.h"

absl::flat_hash_map<uint64_t, double> prices;
prices.reserve(10000);  // Pre-allocate!
prices[symbol_id] = 100.50;
auto price = prices[symbol_id];  // 15-60ns
```

#### `absl::flat_hash_set<T>`
- **Latency:** 15-60ns lookup
- **Best for:** Unique elements, membership tests
- **Use case:** Active order IDs

```cpp
#include "absl/container/flat_hash_set.h"

absl::flat_hash_set<uint64_t> active_order_ids;
active_order_ids.reserve(50000);
active_order_ids.insert(order_id);
bool exists = active_order_ids.contains(order_id);  // C++20
```

#### `absl::node_hash_map<K, V>`
- **Latency:** 20-80ns lookup (P99: 150ns)
- **Storage:** Node-based (like std::unordered_map)
- **Pointers:** **Stable** (never invalidated)
- **Best for:** When pointer/iterator stability needed
- **Use case:** Storing iterators to orders

```cpp
#include "absl/container/node_hash_map.h"

absl::node_hash_map<uint64_t, Order> orders;
orders.reserve(50000);
orders.emplace(id, order);

// Safe to store iterator - never invalidated
auto it = orders.find(id);
order_iterators[id] = it;  // Safe!
```

#### `absl::node_hash_set<T>`
- **Latency:** 20-80ns lookup
- **Pointers:** Stable
- **Best for:** Sets with stable pointers

---

### Ordered Containers (B-trees)

#### `absl::btree_map<K, V>`
- **Latency:** 30-120ns lookup (P99: 250ns)
- **Structure:** B-tree (cache-friendly)
- **Ordering:** Maintains sorted order
- **Best for:** Sorted data, range queries
- **Use case:** Order book price levels

```cpp
#include "absl/container/btree_map.h"

// Price levels in order book
absl::btree_map<double, OrderQueue> bid_levels;

bid_levels[100.50] = order_queue;

// Fast best bid/ask
auto best_bid_price = bid_levels.rbegin()->first;  // Highest
auto best_ask_price = ask_levels.begin()->first;   // Lowest

// Range queries
auto it_lower = bid_levels.lower_bound(100.0);
auto it_upper = bid_levels.upper_bound(101.0);
```

#### `absl::btree_set<T>`
- **Latency:** 30-120ns lookup
- **Best for:** Sorted unique elements
- **Use case:** Sorted price points

```cpp
#include "absl/container/btree_set.h"

absl::btree_set<double> price_points;
price_points.insert(100.50);
```

---

### Sequential Containers

#### `absl::InlinedVector<T, N>`
- **Latency:** 35-90ns (for N elements)
- **Storage:** First N elements inline (stack), rest on heap
- **Heap:** **ZERO** for ≤N elements
- **Best for:** Frequently created small vectors
- **Use case:** Orders at a price level (typically <8)

```cpp
#include "absl/container/inlined_vector.h"

// Most price levels have <8 orders
absl::InlinedVector<Order, 8> orders_at_price;

// Fast for small sizes - NO heap allocation!
orders_at_price.push_back(order1);
orders_at_price.push_back(order2);
// ... up to 8 orders: ZERO heap
```

#### `absl::FixedArray<T>`
- **Latency:** 40-100ns
- **Storage:** Small sizes on stack, large on heap
- **Size:** Fixed at construction, cannot grow
- **Best for:** Runtime-sized arrays that don't change
- **Use case:** Snapshot buffers

```cpp
#include "absl/container/fixed_array.h"

// Runtime size, but doesn't change
size_t snapshot_size = get_current_orders();
absl::FixedArray<Order> snapshot(snapshot_size);

for (size_t i = 0; i < snapshot_size; ++i) {
    snapshot[i] = get_order(i);
}
```

---

## 🎯 Performance Comparison

| Container | Lookup Latency | vs STL | Best Use Case |
|-----------|----------------|--------|---------------|
| **flat_hash_map** | 15-60ns | 2-3x faster than std::unordered_map | Fast lookups |
| **node_hash_map** | 20-80ns | Similar to std::unordered_map | Stable pointers |
| **btree_map** | 30-120ns | 2-3x faster than std::map | Sorted data |
| **InlinedVector<T,N>** | 35-90ns | Faster for small sizes | Small vectors |
| **FixedArray** | 40-100ns | Similar to std::vector | Fixed runtime size |

---

## 💡 Best Practices for HFT

### ✅ DO

1. **Always reserve() hash containers**
   ```cpp
   prices.reserve(100000);  // Avoid rehashing!
   ```

2. **Use flat_hash_map for most cases**
   ```cpp
   absl::flat_hash_map<SymbolID, Price> last_prices;
   // Fastest hash map (15-60ns)
   ```

3. **Use node_hash_map when storing iterators**
   ```cpp
   absl::node_hash_map<OrderID, Order> orders;
   auto it = orders.find(id);
   saved_iterators[id] = it;  // Safe - never invalidates
   ```

4. **Use btree_map for sorted data**
   ```cpp
   absl::btree_map<Price, Volume> price_levels;
   // Fast best bid/ask: rbegin() / begin()
   ```

5. **Use InlinedVector for small frequent vectors**
   ```cpp
   absl::InlinedVector<Order, 8> orders;
   // ZERO heap for ≤8 orders
   ```

### ❌ DON'T

1. **Don't forget to reserve()**
   ```cpp
   // ❌ BAD - will rehash many times
   absl::flat_hash_map<int, int> map;
   for (int i = 0; i < 100000; ++i) map[i] = i;
   
   // ✅ GOOD
   absl::flat_hash_map<int, int> map;
   map.reserve(100000);
   for (int i = 0; i < 100000; ++i) map[i] = i;
   ```

2. **Don't use flat_hash_map if you need stable pointers**
   ```cpp
   // ❌ BAD - iterator invalidated on rehash
   absl::flat_hash_map<int, Order> orders;
   auto it = orders.find(id);
   orders[new_id] = order;  // May rehash, invalidates 'it'!
   
   // ✅ GOOD - use node_hash_map
   absl::node_hash_map<int, Order> orders;
   auto it = orders.find(id);
   orders[new_id] = order;  // 'it' still valid
   ```

3. **Don't use std::map when flat_hash_map works**
   ```cpp
   // ❌ BAD - 2-3x slower
   std::map<SymbolID, Price> prices;
   
   // ✅ GOOD - if order doesn't matter
   absl::flat_hash_map<SymbolID, Price> prices;
   
   // ✅ GOOD - if order matters
   absl::btree_map<SymbolID, Price> prices;
   ```

---

## 🔬 Practical Trading Examples

### Example 1: Order Book Price Levels
```cpp
using Price = double;
using OrderQueue = absl::InlinedVector<Order, 8>;

absl::btree_map<Price, OrderQueue> bid_levels;
absl::btree_map<Price, OrderQueue> ask_levels;

// Add order to price level
bid_levels[price].push_back(order);

// Get best bid/ask (30-120ns)
if (!bid_levels.empty()) {
    auto best_bid_price = bid_levels.rbegin()->first;
    auto& best_bid_orders = bid_levels.rbegin()->second;
}
```

### Example 2: Symbol → Last Price Cache
```cpp
absl::flat_hash_map<uint32_t, double> last_prices;
last_prices.reserve(10000);  // All symbols

// Update (15-60ns)
last_prices[symbol_id] = new_price;

// Lookup (15-60ns)
double price = last_prices[symbol_id];
```

### Example 3: Active Orders
```cpp
absl::node_hash_map<uint64_t, Order> active_orders;
active_orders.reserve(50000);

// Add order
active_orders.emplace(order_id, order);

// Modify order (pointer stability!)
auto it = active_orders.find(order_id);
if (it != active_orders.end()) {
    it->second.quantity = new_quantity;
}
```

### Example 4: Recent Trades Buffer
```cpp
constexpr size_t BUFFER_SIZE = 1000;
absl::FixedArray<Trade> recent_trades(BUFFER_SIZE);
size_t write_index = 0;

// Add trade (circular buffer)
recent_trades[write_index] = new_trade;
write_index = (write_index + 1) % BUFFER_SIZE;

// Calculate VWAP
double total_value = 0.0;
uint64_t total_volume = 0;
for (const auto& trade : recent_trades) {
    total_value += trade.price * trade.quantity;
    total_volume += trade.quantity;
}
double vwap = total_value / total_volume;
```

---

## 🚀 Swiss Tables Explained

**Why 2-3x faster than std::unordered_map?**

1. **Open Addressing**
   - No linked lists (better cache locality)
   - Less memory allocations

2. **SIMD Parallel Probing**
   - Checks 16 slots in parallel (SSE2)
   - Finds matches faster

3. **Control Bytes**
   - Metadata stored separately
   - Faster to scan than full keys

4. **Cache-Friendly Layout**
   - Data stored contiguously
   - Fewer cache misses

---

## 📚 Resources

- **Documentation:** https://abseil.io/docs/cpp/guides/container
- **Swiss Tables Paper:** https://abseil.io/about/design/swisstables
- **GitHub:** https://github.com/abseil/abseil-cpp
- **CppCon Talk:** "Designing Abseil" by Titus Winters

---

## 🎯 Quick Decision Tree

**Need fast lookups, no ordering?**
→ `absl::flat_hash_map` (15-60ns)

**Need fast lookups + stable pointers?**
→ `absl::node_hash_map` (20-80ns)

**Need sorted/ordered data?**
→ `absl::btree_map` (30-120ns)

**Small vectors created frequently?**
→ `absl::InlinedVector<T, N>` (35-90ns, ZERO heap for ≤N)

**Fixed runtime-sized array?**
→ `absl::FixedArray<T>` (40-100ns)

---

**File:** `abseil_containers_comprehensive.cpp`  
**Compile:** `g++ -std=c++17 -O3 -march=native -DNDEBUG abseil_containers_comprehensive.cpp -labsl_base -labsl_hash -labsl_raw_hash_set -labsl_strings -lpthread -o abseil_benchmark`  
**Run:** `./abseil_benchmark`

---

✅ **Ready for ultra-low latency trading!** 🚀

