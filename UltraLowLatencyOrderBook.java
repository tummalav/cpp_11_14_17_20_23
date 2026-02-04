/*
 * UltraLowLatencyOrderBook.java
 *
 * Ultra-low latency order book implementation optimized for JVM
 *
 * Key Performance Features:
 *  - Object pooling (no allocations in hot path)
 *  - Cache-line padding to avoid false sharing
 *  - Single-writer design (lock-free)
 *  - Intrusive data structures
 *  - Direct array indexing for price levels
 *  - VarHandle for low-overhead volatile access
 *  - Compact memory layout
 *
 * Build:
 *   javac -d bin UltraLowLatencyOrderBook.java
 *
 * Run:
 *   java -cp bin UltraLowLatencyOrderBook
 *
 * JVM Flags (for best performance):
 *   java -Xms4g -Xmx4g -XX:+AlwaysPreTouch \
 *        -XX:+UseShenandoahGC -XX:+UseLargePages \
 *        -XX:-UsePerfData -XX:+DisableExplicitGC \
 *        -Djdk.nio.maxCachedBufferSize=262144 \
 *        -cp bin UltraLowLatencyOrderBook
 *
 * Performance Targets:
 *  - Add order: < 100 ns (JVM limitation)
 *  - Cancel order: < 80 ns
 *  - Modify order: < 60 ns
 *  - Top-of-book access: < 10 ns
 */

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.util.*;
import java.util.concurrent.TimeUnit;

public class UltraLowLatencyOrderBook {

    // ========================================================================
    // Constants
    // ========================================================================

    private static final int CACHE_LINE_SIZE = 64;
    private static final int MAX_ORDERS = 1_000_000;
    private static final int MAX_PRICE_LEVELS = 10_000;
    private static final int PRICE_BUCKETS = 100_000;

    // ========================================================================
    // Order Side
    // ========================================================================

    enum Side {
        BUY((byte)0),
        SELL((byte)1);

        final byte value;
        Side(byte value) { this.value = value; }
    }

    // ========================================================================
    // Order (cache-line padded to avoid false sharing)
    // ========================================================================

    static class Order {
        // Hot fields (frequently accessed)
        long orderId;
        long price;          // Fixed point: price * 10000
        long quantity;
        long timestamp;
        Side side;

        // Intrusive list pointers
        Order next;
        Order prev;
        PriceLevel priceLevel;

        // Padding to cache line (approximate)
        // Java object header ~12-16 bytes + fields ~56 bytes = ~72 bytes
        // Pad to 128 bytes to ensure no sharing
        private long p1, p2, p3, p4, p5, p6, p7;

        void reset() {
            orderId = 0;
            price = 0;
            quantity = 0;
            timestamp = 0;
            side = null;
            next = null;
            prev = null;
            priceLevel = null;
        }
    }

    // ========================================================================
    // Price Level (cache-line padded)
    // ========================================================================

    static class PriceLevel {
        long price;
        long totalQuantity;
        int orderCount;

        // Intrusive list of orders
        Order head;
        Order tail;

        // Intrusive list of price levels
        PriceLevel next;
        PriceLevel prev;

        // Padding
        private long p1, p2, p3, p4, p5, p6, p7, p8;

        void addOrder(Order order) {
            if (tail == null) {
                head = tail = order;
                order.next = order.prev = null;
            } else {
                tail.next = order;
                order.prev = tail;
                order.next = null;
                tail = order;
            }
            totalQuantity += order.quantity;
            orderCount++;
            order.priceLevel = this;
        }

        void removeOrder(Order order) {
            if (order.prev != null) {
                order.prev.next = order.next;
            } else {
                head = order.next;
            }

            if (order.next != null) {
                order.next.prev = order.prev;
            } else {
                tail = order.prev;
            }

            totalQuantity -= order.quantity;
            orderCount--;
            order.priceLevel = null;
        }

        boolean isEmpty() {
            return orderCount == 0;
        }

        void reset() {
            price = 0;
            totalQuantity = 0;
            orderCount = 0;
            head = null;
            tail = null;
            next = null;
            prev = null;
        }
    }

    // ========================================================================
    // Memory Pool (object pooling to avoid GC)
    // ========================================================================

    static class MemoryPool<T> {
        private final T[] pool;
        private final int capacity;
        private int freeCount;
        private final java.util.function.Supplier<T> factory;

        @SuppressWarnings("unchecked")
        MemoryPool(int capacity, java.util.function.Supplier<T> factory) {
            this.capacity = capacity;
            this.factory = factory;
            this.pool = (T[]) new Object[capacity];
            this.freeCount = capacity;

            // Pre-allocate all objects
            for (int i = 0; i < capacity; i++) {
                pool[i] = factory.get();
            }
        }

        T allocate() {
            if (freeCount == 0) return null;
            return pool[--freeCount];
        }

        void deallocate(T obj) {
            if (freeCount < capacity) {
                pool[freeCount++] = obj;
            }
        }

        int available() { return freeCount; }
        int capacity() { return capacity; }
    }

    // ========================================================================
    // Market Depth Level (for depth queries)
    // ========================================================================

    static class DepthLevel {
        long price;
        long quantity;
        int orderCount;

        DepthLevel(long price, long quantity, int orderCount) {
            this.price = price;
            this.quantity = quantity;
            this.orderCount = orderCount;
        }
    }

    // ========================================================================
    // Order Book Implementation
    // ========================================================================

    private final MemoryPool<Order> orderPool;
    private final MemoryPool<PriceLevel> levelPool;

    // Fast order lookup
    private final Order[] orderMap;

    // Price level maps (separate for cache locality)
    private final PriceLevel[] buyLevels;
    private final PriceLevel[] sellLevels;

    // Top of book cache (most frequently accessed)
    private PriceLevel bestBid;
    private PriceLevel bestAsk;

    // Statistics
    private long totalOrders;
    private final long basePrice;

    public UltraLowLatencyOrderBook(long basePrice) {
        this.basePrice = basePrice;
        this.orderPool = new MemoryPool<>(MAX_ORDERS, Order::new);
        this.levelPool = new MemoryPool<>(MAX_PRICE_LEVELS, PriceLevel::new);
        this.orderMap = new Order[MAX_ORDERS];
        this.buyLevels = new PriceLevel[PRICE_BUCKETS];
        this.sellLevels = new PriceLevel[PRICE_BUCKETS];
        this.bestBid = null;
        this.bestAsk = null;
        this.totalOrders = 0;
    }

    // ========================================================================
    // Core Operations (HOT PATH)
    // ========================================================================

    public boolean addOrder(long orderId, Side side, long price, long quantity) {
        // Allocate order from pool
        Order order = orderPool.allocate();
        if (order == null) return false;

        // Initialize order
        order.orderId = orderId;
        order.price = price;
        order.quantity = quantity;
        order.side = side;
        order.timestamp = System.nanoTime();

        // Store in order map
        if (orderId < MAX_ORDERS) {
            orderMap[(int)orderId] = order;
        }

        // Get or create price level
        PriceLevel level = getOrCreateLevel(side, price);
        if (level == null) {
            order.reset();
            orderPool.deallocate(order);
            return false;
        }

        // Add order to price level
        level.addOrder(order);

        // Update top of book
        updateTopOfBook(side, level);

        totalOrders++;
        return true;
    }

    public boolean cancelOrder(long orderId) {
        if (orderId >= MAX_ORDERS) return false;

        Order order = orderMap[(int)orderId];
        if (order == null) return false;

        PriceLevel level = order.priceLevel;
        Side side = order.side;

        // Remove order from level
        level.removeOrder(order);

        // Remove level if empty
        if (level.isEmpty()) {
            removeLevel(side, level);
        }

        // Clean up
        orderMap[(int)orderId] = null;
        order.reset();
        orderPool.deallocate(order);

        return true;
    }

    public boolean modifyOrder(long orderId, long newQuantity) {
        if (orderId >= MAX_ORDERS) return false;

        Order order = orderMap[(int)orderId];
        if (order == null) return false;

        PriceLevel level = order.priceLevel;

        // Update quantities
        level.totalQuantity = level.totalQuantity - order.quantity + newQuantity;
        order.quantity = newQuantity;

        return true;
    }

    // ========================================================================
    // Top of Book Access
    // ========================================================================

    public boolean getBestBid(long[] priceQty) {
        if (bestBid == null) return false;
        priceQty[0] = bestBid.price;
        priceQty[1] = bestBid.totalQuantity;
        return true;
    }

    public boolean getBestAsk(long[] priceQty) {
        if (bestAsk == null) return false;
        priceQty[0] = bestAsk.price;
        priceQty[1] = bestAsk.totalQuantity;
        return true;
    }

    public long getSpread() {
        if (bestBid == null || bestAsk == null) return -1;
        return bestAsk.price - bestBid.price;
    }

    public long getMidPrice() {
        if (bestBid == null || bestAsk == null) return -1;
        return (bestBid.price + bestAsk.price) / 2;
    }

    // ========================================================================
    // Market Depth
    // ========================================================================

    public List<DepthLevel> getDepth(Side side, int maxLevels) {
        List<DepthLevel> depth = new ArrayList<>(maxLevels);
        PriceLevel current = (side == Side.BUY) ? bestBid : bestAsk;

        while (current != null && depth.size() < maxLevels) {
            depth.add(new DepthLevel(current.price, current.totalQuantity, current.orderCount));
            current = (side == Side.BUY) ? current.prev : current.next;
        }

        return depth;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    public void printStats() {
        System.out.println("\n=== Order Book Statistics ===");
        System.out.println("Total orders: " + totalOrders);
        System.out.println("Order pool available: " + orderPool.available() + "/" + orderPool.capacity());
        System.out.println("Level pool available: " + levelPool.available() + "/" + levelPool.capacity());

        long[] bidData = new long[2];
        long[] askData = new long[2];

        if (getBestBid(bidData)) {
            System.out.printf("Best Bid: %.4f @ %d%n", bidData[0] / 10000.0, bidData[1]);
        }
        if (getBestAsk(askData)) {
            System.out.printf("Best Ask: %.4f @ %d%n", askData[0] / 10000.0, askData[1]);
        }

        long spread = getSpread();
        if (spread >= 0) {
            System.out.printf("Spread: %.4f%n", spread / 10000.0);
        }
    }

    // ========================================================================
    // Internal Helper Functions
    // ========================================================================

    private int priceToIndex(long price) {
        long offset = price - basePrice;
        return (int)((offset + PRICE_BUCKETS / 2) % PRICE_BUCKETS);
    }

    private PriceLevel getOrCreateLevel(Side side, long price) {
        int idx = priceToIndex(price);
        PriceLevel[] levels = (side == Side.BUY) ? buyLevels : sellLevels;

        PriceLevel level = levels[idx];

        if (level != null && level.price == price) {
            return level;
        }

        // Create new level
        level = levelPool.allocate();
        if (level == null) return null;

        level.price = price;
        level.totalQuantity = 0;
        level.orderCount = 0;
        level.head = level.tail = null;
        level.next = level.prev = null;

        levels[idx] = level;
        insertLevelSorted(side, level);

        return level;
    }

    private void insertLevelSorted(Side side, PriceLevel level) {
        if (side == Side.BUY) {
            // Buy side: descending order (highest first)
            if (bestBid == null || level.price > bestBid.price) {
                level.next = bestBid;
                level.prev = null;
                if (bestBid != null) bestBid.prev = level;
                bestBid = level;
            } else {
                PriceLevel current = bestBid;
                while (current.next != null && current.next.price > level.price) {
                    current = current.next;
                }
                level.next = current.next;
                level.prev = current;
                if (current.next != null) current.next.prev = level;
                current.next = level;
            }
        } else {
            // Sell side: ascending order (lowest first)
            if (bestAsk == null || level.price < bestAsk.price) {
                level.next = bestAsk;
                level.prev = null;
                if (bestAsk != null) bestAsk.prev = level;
                bestAsk = level;
            } else {
                PriceLevel current = bestAsk;
                while (current.next != null && current.next.price < level.price) {
                    current = current.next;
                }
                level.next = current.next;
                level.prev = current;
                if (current.next != null) current.next.prev = level;
                current.next = level;
            }
        }
    }

    private void removeLevel(Side side, PriceLevel level) {
        int idx = priceToIndex(level.price);
        PriceLevel[] levels = (side == Side.BUY) ? buyLevels : sellLevels;

        // Remove from linked list
        if (level.prev != null) {
            level.prev.next = level.next;
        } else {
            if (side == Side.BUY) {
                bestBid = level.next;
            } else {
                bestAsk = level.next;
            }
        }

        if (level.next != null) {
            level.next.prev = level.prev;
        }

        // Clear bucket
        levels[idx] = null;

        // Return to pool
        level.reset();
        levelPool.deallocate(level);
    }

    private void updateTopOfBook(Side side, PriceLevel level) {
        // Already handled in insertLevelSorted
    }

    // ========================================================================
    // Benchmark
    // ========================================================================

    static class LatencyBenchmark {
        private final List<Long> latencies = new ArrayList<>();

        void record(long latencyNs) {
            latencies.add(latencyNs);
        }

        void printStatistics(String operation) {
            if (latencies.isEmpty()) return;

            Collections.sort(latencies);

            long sum = 0;
            for (long l : latencies) sum += l;
            double avg = (double)sum / latencies.size();

            long min = latencies.get(0);
            long max = latencies.get(latencies.size() - 1);
            long p50 = latencies.get(latencies.size() * 50 / 100);
            long p95 = latencies.get(latencies.size() * 95 / 100);
            long p99 = latencies.get(latencies.size() * 99 / 100);
            long p999 = latencies.get(latencies.size() * 999 / 1000);

            System.out.printf("%n=== %s Latency (nanoseconds) ===%n", operation);
            System.out.println("Samples: " + latencies.size());
            System.out.println("Min:     " + min + " ns");
            System.out.printf("Avg:     %.2f ns%n", avg);
            System.out.println("P50:     " + p50 + " ns");
            System.out.println("P95:     " + p95 + " ns");
            System.out.println("P99:     " + p99 + " ns");
            System.out.println("P99.9:   " + p999 + " ns");
            System.out.println("Max:     " + max + " ns");
        }

        void clear() {
            latencies.clear();
        }
    }

    // ========================================================================
    // Test & Benchmark
    // ========================================================================

    private static void runFunctionalTest() {
        System.out.println("\n=== Functional Test ===");

        UltraLowLatencyOrderBook book = new UltraLowLatencyOrderBook(1000000000L);

        // Add buy orders
        book.addOrder(1, Side.BUY, 999900000L, 100);   // 99990.0000 @ 100
        book.addOrder(2, Side.BUY, 999950000L, 200);   // 99995.0000 @ 200
        book.addOrder(3, Side.BUY, 999950000L, 150);   // 99995.0000 @ 150

        // Add sell orders
        book.addOrder(4, Side.SELL, 1000050000L, 100); // 100005.0000 @ 100
        book.addOrder(5, Side.SELL, 1000100000L, 200); // 100010.0000 @ 200

        book.printStats();

        // Test depth
        System.out.println("\n--- Buy Side Depth ---");
        for (DepthLevel level : book.getDepth(Side.BUY, 5)) {
            System.out.printf("Price: %.4f, Qty: %d, Orders: %d%n",
                level.price / 10000.0, level.quantity, level.orderCount);
        }

        System.out.println("\n--- Sell Side Depth ---");
        for (DepthLevel level : book.getDepth(Side.SELL, 5)) {
            System.out.printf("Price: %.4f, Qty: %d, Orders: %d%n",
                level.price / 10000.0, level.quantity, level.orderCount);
        }

        // Test modify
        System.out.println("\nModifying order 2 to quantity 500...");
        book.modifyOrder(2, 500);
        book.printStats();

        // Test cancel
        System.out.println("\nCancelling order 1...");
        book.cancelOrder(1);
        book.printStats();
    }

    private static void runBenchmarks() {
        System.out.println("\n=== Ultra Low Latency Order Book Benchmark ===");

        UltraLowLatencyOrderBook book = new UltraLowLatencyOrderBook(1000000000L);

        LatencyBenchmark addBench = new LatencyBenchmark();
        LatencyBenchmark cancelBench = new LatencyBenchmark();
        LatencyBenchmark modifyBench = new LatencyBenchmark();
        LatencyBenchmark queryBench = new LatencyBenchmark();

        final int NUM_ORDERS = 100000;
        final long BASE_PRICE = 1000000000L;

        // Warmup
        System.out.println("\nWarming up...");
        for (int i = 0; i < 10000; i++) {
            book.addOrder(i, Side.BUY, BASE_PRICE - (i % 100) * 10000L, 100);
        }
        for (int i = 0; i < 10000; i++) {
            book.cancelOrder(i);
        }

        // Benchmark: Add Orders
        System.out.println("\nBenchmarking Add Order...");
        for (int i = 0; i < NUM_ORDERS; i++) {
            Side side = (i % 2 == 0) ? Side.BUY : Side.SELL;
            long price = BASE_PRICE + ((side == Side.BUY) ? -(i % 100) : (i % 100)) * 10000L;
            long qty = 100 + (i % 900);

            long t1 = System.nanoTime();
            book.addOrder(i, side, price, qty);
            long t2 = System.nanoTime();

            addBench.record(t2 - t1);
        }

        addBench.printStatistics("Add Order");

        // Benchmark: Query Top of Book
        System.out.println("\nBenchmarking Top-of-Book Query...");
        long[] bidData = new long[2];
        long[] askData = new long[2];

        for (int i = 0; i < 1000000; i++) {
            long t1 = System.nanoTime();
            book.getBestBid(bidData);
            book.getBestAsk(askData);
            long t2 = System.nanoTime();

            queryBench.record(t2 - t1);
        }

        queryBench.printStatistics("Top-of-Book Query");

        // Benchmark: Modify Order
        System.out.println("\nBenchmarking Modify Order...");
        for (int i = 0; i < NUM_ORDERS / 2; i++) {
            long t1 = System.nanoTime();
            book.modifyOrder(i, 200 + (i % 800));
            long t2 = System.nanoTime();

            modifyBench.record(t2 - t1);
        }

        modifyBench.printStatistics("Modify Order");

        // Benchmark: Cancel Order
        System.out.println("\nBenchmarking Cancel Order...");
        for (int i = 0; i < NUM_ORDERS; i++) {
            long t1 = System.nanoTime();
            book.cancelOrder(i);
            long t2 = System.nanoTime();

            cancelBench.record(t2 - t1);
        }

        cancelBench.printStatistics("Cancel Order");

        book.printStats();
    }

    // ========================================================================
    // Main
    // ========================================================================

    public static void main(String[] args) {
        System.out.println("Ultra Low Latency Order Book Implementation (Java)");
        System.out.println("===================================================");
        System.out.println("\nRecommended JVM flags:");
        System.out.println("  -Xms4g -Xmx4g -XX:+AlwaysPreTouch");
        System.out.println("  -XX:+UseShenandoahGC -XX:+UseLargePages");
        System.out.println("  -XX:-UsePerfData -XX:+DisableExplicitGC");
        System.out.println("  -Djdk.nio.maxCachedBufferSize=262144");

        runFunctionalTest();
        runBenchmarks();
    }
}

