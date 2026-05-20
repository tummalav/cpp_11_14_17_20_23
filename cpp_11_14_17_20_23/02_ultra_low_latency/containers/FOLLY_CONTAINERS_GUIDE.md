# Folly Containers - Quick Reference Guide

Facebook's Folly C++ Library - High-Performance Containers for Ultra-Low Latency Systems

---

## 📦 Installation

### macOS
```bash
brew install folly
```

### RHEL/Linux
```bash
# Install dependencies
sudo yum install -y double-conversion-devel gflags-devel \
    glog-devel libevent-devel openssl-devel fmt-devel

# Build from source
git clone https://github.com/facebook/folly.git
cd folly && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc) && sudo make install
```

---

## 🔨 Compilation

```bash
g++ -std=c++17 -O3 -march=native -DNDEBUG \
    -DGLOG_NO_ABBREVIATED_SEVERITIES -DGFLAGS_NAMESPACE=google \
    your_code.cpp \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
    -lboost_context -lpthread -o your_app
```

---

## 📊 Container Overview

### Sequential Containers

#### `folly::fbvector<T>`
- **Latency:** 90-180ns (similar to std::vector)
- **Storage:** Dynamic heap allocation
- **Growth:** 1.5x (vs std::vector 2x)
- **Best for:** Drop-in std::vector replacement
- **Use case:** General dynamic arrays

```cpp
#include <folly/FBVector.h>

folly::fbvector<Order> orders;
orders.reserve(10000);  // Pre-allocate
orders.push_back(order);

// Works exactly like std::vector
for (const auto& order : orders) {
    process(order);
}
```

**Key Features:**
- ✅ Better growth strategy (less memory waste)
- ✅ Optimized for relocatable types
- ✅ Drop-in std::vector replacement
- ✅ Production-proven at Facebook scale

---

#### `folly::small_vector<T, N>`
- **Latency:** 30-100ns (depends on N)
- **Storage:** First N elements inline (stack), rest on heap
- **Heap:** **ZERO** for ≤N elements
- **Best for:** Frequently created small vectors
- **Use case:** Temporary buffers, orders at price level

```cpp
#include <folly/small_vector.h>

// Most price levels have <8 orders
folly::small_vector<Order, 8> orders_at_price;

// Fast for small sizes - NO heap allocation!
orders_at_price.push_back(order1);
orders_at_price.push_back(order2);
// ... up to 8 orders: ZERO heap

// Automatically spills to heap if > 8
for (int i = 0; i < 100; ++i) {
    orders_at_price.push_back(order);  // Uses heap after 8
}
```

**Choosing N:**
```cpp
small_vector<Order, 8>   // 30-80ns   - Typical price level
small_vector<Order, 16>  // 35-90ns   - Small batches
small_vector<Order, 32>  // 40-100ns  - Medium batches
small_vector<Order, 64>  // 50-120ns  - Large batches
```

**Best Practices:**
- ✅ Profile your workload to find typical sizes
- ✅ Choose N to cover 95%+ of cases
- ✅ Don't make N too large (wastes stack space)

---

### Lock-Free Queues

#### `folly::ProducerConsumerQueue<T>` (SPSC)
- **Latency:** 80-250ns (P99: 600ns)
- **Producers:** Single
- **Consumers:** Single
- **Heap:** **ZERO** (fixed capacity)
- **Capacity:** Must be power of 2
- **Best for:** Single feed → Single processor
- **Use case:** Market data pipeline

```cpp
#include <folly/ProducerConsumerQueue.h>

// Must be power of 2
folly::ProducerConsumerQueue<MarketData> queue(4096);

// Producer thread
MarketData md = get_market_data();
while (!queue.write(md)) {
    _mm_pause();  // Busy-wait for low latency
}

// Consumer thread
MarketData md;
if (queue.read(md)) {
    process(md);
}
```

**Key Features:**
- ✅ Lock-free, wait-free operations
- ✅ 80-250ns latency (excellent SPSC)
- ✅ ZERO heap allocation
- ✅ Cache-line aligned
- ✅ Production-proven at Facebook

**Performance:**
- **Throughput:** ~10M ops/sec/core
- **Latency:** 80-250ns (P50), 600ns (P99)
- **Best in class** for SPSC

---

#### `folly::MPMCQueue<T>` (MPMC)
- **Latency:** 300-1200ns (P99: 3μs with contention)
- **Producers:** Multiple
- **Consumers:** Multiple
- **Heap:** **ZERO** (fixed capacity)
- **Capacity:** Must be power of 2
- **Best for:** Multiple producers/consumers
- **Use case:** Multi-strategy order execution

```cpp
#include <folly/MPMCQueue.h>

// Must be power of 2
folly::MPMCQueue<Order> queue(4096);

// Multiple producer threads
void strategy_thread() {
    Order order = create_order();
    while (!queue.write(order)) {
        _mm_pause();
    }
}

// Multiple consumer threads
void gateway_thread() {
    Order order;
    if (queue.read(order)) {
        send_to_exchange(order);
    }
}
```

**Key Features:**
- ✅ Lock-free with atomic operations
- ✅ Excellent contention handling
- ✅ ZERO heap allocation
- ✅ Cache-line aligned
- ✅ Scalable to many threads

**Performance:**
- **Throughput:** ~3-5M ops/sec (multi-threaded)
- **Latency:** 300-1200ns (depends on contention)
- **Scales well** with multiple producers/consumers

---

## 🎯 Performance Comparison

| Container | Latency | vs STL/Boost | Heap Allocation |
|-----------|---------|--------------|-----------------|
| **fbvector** | 90-180ns | Similar to std::vector | Single |
| **small_vector<T,8>** | 30-80ns | Similar to boost::small_vector | ZERO (≤8) |
| **small_vector<T,16>** | 35-90ns | Similar to boost::small_vector | ZERO (≤16) |
| **small_vector<T,32>** | 40-100ns | Similar to boost::small_vector | ZERO (≤32) |
| **ProducerConsumerQueue** | 80-250ns | vs boost::spsc: 50-200ns | ZERO |
| **MPMCQueue** | 300-1200ns | vs boost::queue: 200-800ns | ZERO |

---

## 💡 Best Practices for HFT

### ✅ DO

1. **Use ProducerConsumerQueue for SPSC**
   ```cpp
   folly::ProducerConsumerQueue<MarketData> feed_queue(4096);
   // 80-250ns latency - excellent for single producer/consumer
   ```

2. **Use small_vector for small frequent vectors**
   ```cpp
   // Orders at a price level (typically <8)
   folly::small_vector<Order, 8> orders;
   // ZERO heap allocation!
   ```

3. **Size queues as power of 2**
   ```cpp
   ProducerConsumerQueue<Order> queue(4096);  // ✅ Good
   // Not: queue(4000)  // ❌ Bad
   ```

4. **Use fbvector as std::vector replacement**
   ```cpp
   folly::fbvector<Order> all_orders;
   all_orders.reserve(100000);
   // Better growth strategy than std::vector
   ```

5. **Profile to choose small_vector size**
   ```cpp
   // Measure typical sizes
   // If 95% are ≤16, use small_vector<T, 16>
   ```

### ❌ DON'T

1. **Don't use MPMC when SPSC works**
   ```cpp
   // ❌ BAD - MPMC is 3-4x slower for SPSC workload
   folly::MPMCQueue<MarketData> queue(4096);
   
   // ✅ GOOD - Use SPSC when possible
   folly::ProducerConsumerQueue<MarketData> queue(4096);
   ```

2. **Don't forget power-of-2 capacity**
   ```cpp
   // ❌ BAD
   folly::MPMCQueue<Order> queue(5000);
   
   // ✅ GOOD
   folly::MPMCQueue<Order> queue(4096);  // 2^12
   ```

3. **Don't block on queue operations**
   ```cpp
   // ❌ BAD - adds latency
   while (!queue.write(order)) {
       std::this_thread::yield();
   }
   
   // ✅ GOOD - busy-wait for low latency
   while (!queue.write(order)) {
       _mm_pause();
   }
   ```

4. **Don't use wrong small_vector size**
   ```cpp
   // ❌ BAD - if typical size is 8, N=4 wastes opportunity
   folly::small_vector<Order, 4> orders;
   
   // ✅ GOOD - profile to find right N
   folly::small_vector<Order, 8> orders;
   ```

---

## 🔬 Practical Trading Examples

### Example 1: Market Data Pipeline
```cpp
// Exchange feed → Market data processor
folly::ProducerConsumerQueue<MarketData> md_queue(8192);

// Feed handler (producer)
void feed_handler_thread() {
    while (running) {
        MarketData md = receive_from_exchange();
        while (!md_queue.write(md)) {
            _mm_pause();
        }
    }
}

// Processor (consumer)
void processor_thread() {
    MarketData md;
    while (running) {
        if (md_queue.read(md)) {
            update_orderbook(md);
            // 80-250ns latency!
        }
    }
}
```

### Example 2: Multi-Strategy Order Execution
```cpp
// Multiple strategies → Order gateway
folly::MPMCQueue<Order> order_queue(4096);

// Trading strategy (producer)
void strategy_thread(int strategy_id) {
    while (running) {
        Order order = generate_order(strategy_id);
        while (!order_queue.write(order)) {
            _mm_pause();
        }
    }
}

// Order gateway (consumer)
void gateway_thread() {
    Order order;
    while (running) {
        if (order_queue.read(order)) {
            send_to_exchange(order);
        }
    }
}
```

### Example 3: Order Book Price Levels
```cpp
// Orders at each price level
using PriceLevel = folly::small_vector<Order, 8>;
std::map<double, PriceLevel> bid_levels;

// Add order to price level
void add_order(double price, Order order) {
    bid_levels[price].push_back(order);
    // ZERO heap if ≤8 orders at this price!
}

// Remove order from price level
void remove_order(double price, uint64_t order_id) {
    auto& level = bid_levels[price];
    level.erase(
        std::remove_if(level.begin(), level.end(),
            [order_id](const Order& o) { return o.order_id == order_id; }),
        level.end()
    );
}
```

### Example 4: Temporary Order Buffers
```cpp
// Process batch of orders
void process_order_batch() {
    // Typical batch: 10-20 orders
    folly::small_vector<Order, 32> batch;
    
    // Collect orders (ZERO heap for ≤32)
    for (int i = 0; i < get_batch_size(); ++i) {
        batch.push_back(get_next_order());
    }
    
    // Process batch
    for (const auto& order : batch) {
        validate_and_send(order);
    }
    
    // No heap allocation or deallocation!
}
```

---

## 📈 Queue Sizing Guide

### ProducerConsumerQueue / MPMCQueue

**Size Selection:**
```
1024   (1KB)   - Low throughput, low latency requirement
2048   (2KB)   - Medium throughput
4096   (4KB)   - High throughput (recommended)
8192   (8KB)   - Very high throughput
16384  (16KB)  - Extreme throughput
```

**Factors:**
- **Too small:** Frequent full conditions (latency spikes)
- **Too large:** Wasted memory, cache pressure
- **Sweet spot:** 2048-8192 for most HFT use cases

**Calculate required size:**
```
Required Size = Peak Throughput × Max Processing Time
Example: 1M msgs/sec × 5ms = 5000 messages
Choose: 8192 (next power of 2)
```

---

## 🚀 Performance Tips

### 1. Thread Pinning
```bash
# Pin producer to core 2, consumer to core 3
taskset -c 2 ./producer &
taskset -c 3 ./consumer &
```

### 2. Busy-Wait vs Blocking
```cpp
// ✅ Lowest latency (80-250ns)
while (!queue.write(item)) {
    _mm_pause();
}

// ⚠️ Higher latency but lower CPU
while (!queue.write(item)) {
    std::this_thread::yield();
}

// ❌ Highest latency
while (!queue.write(item)) {
    std::this_thread::sleep_for(1us);
}
```

### 3. Batch Processing
```cpp
// Process multiple items at once
folly::small_vector<Order, 32> batch;
while (queue.read(order)) {
    batch.push_back(order);
    if (batch.size() >= 32) break;
}
process_batch(batch);
```

### 4. Choose Right Container
```
Market Data Feed → Processor:     ProducerConsumerQueue (SPSC)
Multiple Strategies → Gateway:    MPMCQueue (MPMC)
Orders at Price Level:            small_vector<Order, 8>
General Dynamic Array:            fbvector<T>
```

---

## 🎓 Decision Tree

**Need thread-safe queue?**
- **YES:** 
  - Single producer/consumer? → `ProducerConsumerQueue` (80-250ns)
  - Multiple producers/consumers? → `MPMCQueue` (300-1200ns)
- **NO:**
  - Frequently created small vectors? → `small_vector<T, N>` (30-100ns)
  - Large dynamic array? → `fbvector<T>` (90-180ns)

**How to choose N for small_vector?**
1. Profile your workload
2. Find P95 size
3. Use that as N
4. Typical values: 8, 16, 32

---

## 📚 Resources

- **Documentation:** https://github.com/facebook/folly
- **Containers:** https://github.com/facebook/folly/tree/main/folly
- **Build Guide:** https://github.com/facebook/folly#building-folly
- **CppCon Talk:** "Practical Type Erasure" by Louis Dionne (covers Folly)

---

## 🔬 Benchmarking

```bash
# Compile
g++ -std=c++17 -O3 -march=native -DNDEBUG \
    -DGLOG_NO_ABBREVIATED_SEVERITIES -DGFLAGS_NAMESPACE=google \
    folly_containers_comprehensive.cpp \
    -I/opt/homebrew/include -L/opt/homebrew/lib \
    -lfolly -lglog -lgflags -lfmt -ldouble-conversion \
    -lpthread -o folly_benchmark

# Run
./folly_benchmark

# Profile with perf
perf stat -e cache-misses,cache-references ./folly_benchmark
```

---

## ⚡ Quick Reference

| Need | Use | Latency |
|------|-----|---------|
| SPSC queue | ProducerConsumerQueue | 80-250ns |
| MPMC queue | MPMCQueue | 300-1200ns |
| Small vectors (≤8) | small_vector<T, 8> | 30-80ns |
| Small vectors (≤16) | small_vector<T, 16> | 35-90ns |
| Dynamic array | fbvector<T> | 90-180ns |

---

**File:** `folly_containers_comprehensive.cpp`  
**Build:** `./build_folly_benchmark.sh`  
**Run:** `./folly_benchmark`

---

✅ **Ready for ultra-low latency trading with Facebook's Folly!** 🚀

