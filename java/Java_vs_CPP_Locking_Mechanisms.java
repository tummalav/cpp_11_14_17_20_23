/**
 * Java vs C++ Locking Mechanisms - Complete Comparison
 *
 * Focus: Concurrency primitives for ultra-low latency trading systems
 *
 * Topics Covered:
 * 1. Java Built-in Synchronization (synchronized keyword)
 * 2. Java ReentrantLock (explicit locks)
 * 3. Java ReadWriteLock
 * 4. Java StampedLock (optimistic locking)
 * 5. Java Atomic Variables
 * 6. Java volatile keyword
 * 7. C++ std::mutex and variants
 * 8. C++ std::atomic
 * 9. C++ Memory Ordering
 * 10. Performance Comparison for Trading Systems
 *
 * Author: Java vs C++ Locking Guide
 * Date: February 8, 2026
 */

import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import java.util.concurrent.atomic.*;
import java.util.*;

public class Java_vs_CPP_Locking_Mechanisms {

    //=============================================================================
    // 1. JAVA BUILT-IN SYNCHRONIZATION (synchronized)
    //=============================================================================

    /**
     * synchronized - Java's built-in locking mechanism
     *
     * - Implicit lock (monitor)
     * - Automatic acquire/release
     * - Reentrant by default
     * - Cannot timeout or try-lock
     */
    static void demonstrateSynchronized() {
        System.out.println("╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  1. JAVA BUILT-IN SYNCHRONIZATION (synchronized)          ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("synchronized Keyword:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Built into Java language");
        System.out.println("• Implicit lock (monitor lock)");
        System.out.println("• Automatic lock acquisition/release");
        System.out.println("• Reentrant (thread can re-acquire own lock)");
        System.out.println("• Works with wait()/notify()");

        System.out.println("\nThree Ways to Use synchronized:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Synchronized method (locks 'this'):");
        System.out.println("   public synchronized void method() { }");
        System.out.println();
        System.out.println("2. Synchronized static method (locks Class object):");
        System.out.println("   public static synchronized void method() { }");
        System.out.println();
        System.out.println("3. Synchronized block (locks specific object):");
        System.out.println("   synchronized (lockObject) { }");

        System.out.println("\nCode Examples:");
        System.out.println("───────────────────────────────────────────────────────────");

        // Example class with synchronized methods
        OrderBook orderBook = new OrderBook();

        System.out.println("// Synchronized method");
        System.out.println("class OrderBook {");
        System.out.println("    public synchronized void addOrder(Order order) {");
        System.out.println("        // Locks 'this' (the OrderBook instance)");
        System.out.println("        orders.add(order);");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\n// Synchronized block (finer-grained)");
        System.out.println("class OrderBook {");
        System.out.println("    private final Object lock = new Object();");
        System.out.println("    ");
        System.out.println("    public void addOrder(Order order) {");
        System.out.println("        // Do some work without lock");
        System.out.println("        validateOrder(order);");
        System.out.println("        ");
        System.out.println("        synchronized (lock) {");
        System.out.println("            // Only lock critical section");
        System.out.println("            orders.add(order);");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\nC++ Equivalent:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class OrderBook {");
        System.out.println("    std::mutex mutex_;");
        System.out.println("    ");
        System.out.println("    void addOrder(Order order) {");
        System.out.println("        std::lock_guard<std::mutex> lock(mutex_);");
        System.out.println("        orders.push_back(order);");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nAdvantages of synchronized:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Simple syntax");
        System.out.println("✅ Automatic lock release (even with exceptions)");
        System.out.println("✅ Built-in to language");
        System.out.println("✅ Reentrant by default");
        System.out.println("✅ Works with wait()/notify()");

        System.out.println("\nDisadvantages of synchronized:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("❌ Cannot timeout (blocks forever)");
        System.out.println("❌ Cannot tryLock() (non-blocking attempt)");
        System.out.println("❌ No fair queuing");
        System.out.println("❌ Cannot interrupt waiting thread (Java 5+)");
        System.out.println("❌ Less flexible than ReentrantLock");

        System.out.println("\nPerformance:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Uncontended: ~10-20ns (very fast, biased locking)");
        System.out.println("Contended:   ~100-500ns (thread parking/unparking)");
    }

    static class OrderBook {
        private final List<Order> orders = new ArrayList<>();
        private final Object lock = new Object();

        // Synchronized method
        public synchronized void addOrderSync(Order order) {
            orders.add(order);
        }

        // Synchronized block
        public void addOrderBlock(Order order) {
            synchronized (lock) {
                orders.add(order);
            }
        }
    }

    static class Order {
        long id;
        double price;
        int quantity;
    }

    //=============================================================================
    // 2. JAVA REENTRANTLOCK (EXPLICIT LOCKS)
    //=============================================================================

    /**
     * ReentrantLock - Explicit lock with advanced features
     *
     * - Explicit lock/unlock
     * - Supports timeout, tryLock
     * - Fair/unfair modes
     * - Interruptible
     */
    static void demonstrateReentrantLock() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  2. JAVA REENTRANTLOCK (Explicit Locks)                    ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("ReentrantLock:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Part of java.util.concurrent.locks");
        System.out.println("• Explicit lock()/unlock()");
        System.out.println("• Supports timeout");
        System.out.println("• Supports tryLock() (non-blocking)");
        System.out.println("• Fair/unfair modes");
        System.out.println("• Interruptible waiting");
        System.out.println("• Reentrant (like synchronized)");

        System.out.println("\nCode Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("import java.util.concurrent.locks.*;");
        System.out.println();
        System.out.println("class OrderBook {");
        System.out.println("    private final ReentrantLock lock = new ReentrantLock();");
        System.out.println("    ");
        System.out.println("    public void addOrder(Order order) {");
        System.out.println("        lock.lock();  // Acquire lock");
        System.out.println("        try {");
        System.out.println("            orders.add(order);");
        System.out.println("        } finally {");
        System.out.println("            lock.unlock();  // MUST unlock in finally!");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\nAdvanced Features:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// tryLock() - non-blocking attempt");
        System.out.println("if (lock.tryLock()) {");
        System.out.println("    try {");
        System.out.println("        // Got lock, do work");
        System.out.println("    } finally { lock.unlock(); }");
        System.out.println("} else {");
        System.out.println("    // Couldn't get lock, do something else");
        System.out.println("}");
        System.out.println();
        System.out.println("// tryLock with timeout");
        System.out.println("if (lock.tryLock(100, TimeUnit.MILLISECONDS)) {");
        System.out.println("    try {");
        System.out.println("        // Got lock within 100ms");
        System.out.println("    } finally { lock.unlock(); }");
        System.out.println("} else {");
        System.out.println("    // Timeout, couldn't get lock");
        System.out.println("}");
        System.out.println();
        System.out.println("// lockInterruptibly() - can be interrupted");
        System.out.println("try {");
        System.out.println("    lock.lockInterruptibly();");
        System.out.println("    try {");
        System.out.println("        // Do work");
        System.out.println("    } finally { lock.unlock(); }");
        System.out.println("} catch (InterruptedException e) {");
        System.out.println("    // Thread was interrupted while waiting");
        System.out.println("}");

        System.out.println("\nFair vs Unfair:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Unfair (default, faster)");
        System.out.println("ReentrantLock unfairLock = new ReentrantLock();");
        System.out.println("  • Threads can 'cut in line'");
        System.out.println("  • Better throughput");
        System.out.println("  • Can cause starvation");
        System.out.println();
        System.out.println("// Fair (slower, but prevents starvation)");
        System.out.println("ReentrantLock fairLock = new ReentrantLock(true);");
        System.out.println("  • FIFO order (longest waiting thread gets lock first)");
        System.out.println("  • Lower throughput (~2x slower)");
        System.out.println("  • Prevents starvation");

        System.out.println("\nC++ Equivalent:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class OrderBook {");
        System.out.println("    std::mutex mutex_;");
        System.out.println("    std::timed_mutex timed_mutex_;");
        System.out.println("    ");
        System.out.println("    // Basic locking");
        System.out.println("    void addOrder(Order order) {");
        System.out.println("        std::lock_guard<std::mutex> lock(mutex_);");
        System.out.println("        orders.push_back(order);");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    // Try lock");
        System.out.println("    bool tryAddOrder(Order order) {");
        System.out.println("        if (mutex_.try_lock()) {");
        System.out.println("            orders.push_back(order);");
        System.out.println("            mutex_.unlock();");
        System.out.println("            return true;");
        System.out.println("        }");
        System.out.println("        return false;");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    // Timed lock");
        System.out.println("    bool tryAddOrderTimed(Order order) {");
        System.out.println("        if (timed_mutex_.try_lock_for(100ms)) {");
        System.out.println("            orders.push_back(order);");
        System.out.println("            timed_mutex_.unlock();");
        System.out.println("            return true;");
        System.out.println("        }");
        System.out.println("        return false;");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nsynchronized vs ReentrantLock:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌────────────────────┬─────────────────┬──────────────────┐");
        System.out.println("│ Feature            │ synchronized    │ ReentrantLock    │");
        System.out.println("├────────────────────┼─────────────────┼──────────────────┤");
        System.out.println("│ Syntax             │ Simple          │ Explicit         │");
        System.out.println("│ Lock release       │ Automatic       │ Manual (finally) │");
        System.out.println("│ tryLock()          │ ❌ No           │ ✅ Yes           │");
        System.out.println("│ Timeout            │ ❌ No           │ ✅ Yes           │");
        System.out.println("│ Interruptible      │ ❌ No           │ ✅ Yes           │");
        System.out.println("│ Fair queuing       │ ❌ No           │ ✅ Yes (option)  │");
        System.out.println("│ Conditions         │ wait/notify     │ Multiple Cond.   │");
        System.out.println("│ Performance        │ Slightly faster │ Slightly slower  │");
        System.out.println("│ Use case           │ Simple locking  │ Advanced needs   │");
        System.out.println("└────────────────────┴─────────────────┴──────────────────┘");

        System.out.println("\nWhen to Use ReentrantLock:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Need timeout on lock acquisition");
        System.out.println("✅ Need tryLock() (non-blocking attempt)");
        System.out.println("✅ Need fair lock ordering");
        System.out.println("✅ Need multiple condition variables");
        System.out.println("✅ Need interruptible lock acquisition");
        System.out.println("❌ Otherwise, use synchronized (simpler)");
    }

    //=============================================================================
    // 3. JAVA READWRITELOCK
    //=============================================================================

    /**
     * ReadWriteLock - Multiple readers, single writer
     */
    static void demonstrateReadWriteLock() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  3. JAVA READWRITELOCK                                     ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("ReadWriteLock:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Two locks: read lock and write lock");
        System.out.println("• Multiple threads can hold read lock simultaneously");
        System.out.println("• Only one thread can hold write lock");
        System.out.println("• Write lock is exclusive (no readers allowed)");
        System.out.println("• Good for read-heavy workloads");

        System.out.println("\nCode Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class PriceCache {");
        System.out.println("    private final ReadWriteLock rwLock = new ReentrantReadWriteLock();");
        System.out.println("    private final Lock readLock = rwLock.readLock();");
        System.out.println("    private final Lock writeLock = rwLock.writeLock();");
        System.out.println("    private Map<String, Double> prices = new HashMap<>();");
        System.out.println("    ");
        System.out.println("    // Multiple threads can read simultaneously");
        System.out.println("    public double getPrice(String symbol) {");
        System.out.println("        readLock.lock();");
        System.out.println("        try {");
        System.out.println("            return prices.get(symbol);");
        System.out.println("        } finally {");
        System.out.println("            readLock.unlock();");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    // Only one thread can write");
        System.out.println("    public void updatePrice(String symbol, double price) {");
        System.out.println("        writeLock.lock();");
        System.out.println("        try {");
        System.out.println("            prices.put(symbol, price);");
        System.out.println("        } finally {");
        System.out.println("            writeLock.unlock();");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\nC++ Equivalent:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class PriceCache {");
        System.out.println("    std::shared_mutex mutex_;  // C++17");
        System.out.println("    std::unordered_map<std::string, double> prices_;");
        System.out.println("    ");
        System.out.println("    // Multiple readers");
        System.out.println("    double getPrice(const std::string& symbol) {");
        System.out.println("        std::shared_lock<std::shared_mutex> lock(mutex_);");
        System.out.println("        return prices_[symbol];");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    // Single writer");
        System.out.println("    void updatePrice(const std::string& symbol, double price) {");
        System.out.println("        std::unique_lock<std::shared_mutex> lock(mutex_);");
        System.out.println("        prices_[symbol] = price;");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nPerformance:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Read-heavy (90% reads): 3-5x better than exclusive lock");
        System.out.println("Write-heavy (50% writes): Similar to exclusive lock");
        System.out.println("Use when: Read/Write ratio > 3:1");
    }

    //=============================================================================
    // 4. JAVA STAMPEDLOCK (OPTIMISTIC LOCKING)
    //=============================================================================

    /**
     * StampedLock - Optimistic locking with read/write modes
     */
    static void demonstrateStampedLock() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  4. JAVA STAMPEDLOCK (Optimistic Locking)                  ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("StampedLock (Java 8+):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Optimistic read mode (no blocking!)");
        System.out.println("• Pessimistic read mode");
        System.out.println("• Write mode");
        System.out.println("• Lock upgrade/downgrade support");
        System.out.println("• NOT reentrant (careful!)");
        System.out.println("• Faster than ReadWriteLock for read-heavy workloads");

        System.out.println("\nCode Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Point {");
        System.out.println("    private final StampedLock lock = new StampedLock();");
        System.out.println("    private double x, y;");
        System.out.println("    ");
        System.out.println("    // Optimistic read (fastest!)");
        System.out.println("    public double distanceFromOrigin() {");
        System.out.println("        long stamp = lock.tryOptimisticRead();  // No blocking!");
        System.out.println("        double currentX = x;  // Read");
        System.out.println("        double currentY = y;  // Read");
        System.out.println("        ");
        System.out.println("        if (!lock.validate(stamp)) {  // Check if modified");
        System.out.println("            // Data was modified, get pessimistic lock");
        System.out.println("            stamp = lock.readLock();");
        System.out.println("            try {");
        System.out.println("                currentX = x;");
        System.out.println("                currentY = y;");
        System.out.println("            } finally {");
        System.out.println("                lock.unlockRead(stamp);");
        System.out.println("            }");
        System.out.println("        }");
        System.out.println("        return Math.sqrt(currentX * currentX + currentY * currentY);");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    // Write lock");
        System.out.println("    public void move(double deltaX, double deltaY) {");
        System.out.println("        long stamp = lock.writeLock();");
        System.out.println("        try {");
        System.out.println("            x += deltaX;");
        System.out.println("            y += deltaY;");
        System.out.println("        } finally {");
        System.out.println("            lock.unlockWrite(stamp);");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\nThree Modes:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Optimistic Read (tryOptimisticRead)");
        System.out.println("   • No blocking, just returns stamp");
        System.out.println("   • Check with validate() after reading");
        System.out.println("   • Fastest, but may need retry");
        System.out.println();
        System.out.println("2. Pessimistic Read (readLock)");
        System.out.println("   • Blocking read lock");
        System.out.println("   • Multiple readers allowed");
        System.out.println();
        System.out.println("3. Write Lock (writeLock)");
        System.out.println("   • Exclusive lock");
        System.out.println("   • Blocks all readers and writers");

        System.out.println("\nC++ Equivalent (No direct equivalent):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Closest: Manual optimistic locking with atomics");
        System.out.println("class Point {");
        System.out.println("    std::atomic<uint64_t> version_{0};");
        System.out.println("    double x_, y_;");
        System.out.println("    std::shared_mutex mutex_;");
        System.out.println("    ");
        System.out.println("    double distanceFromOrigin() {");
        System.out.println("        // Optimistic read");
        System.out.println("        uint64_t v1 = version_.load(std::memory_order_acquire);");
        System.out.println("        double x = x_, y = y_;");
        System.out.println("        uint64_t v2 = version_.load(std::memory_order_acquire);");
        System.out.println("        ");
        System.out.println("        if (v1 != v2) {  // Modified during read");
        System.out.println("            // Fall back to pessimistic read");
        System.out.println("            std::shared_lock lock(mutex_);");
        System.out.println("            x = x_; y = y_;");
        System.out.println("        }");
        System.out.println("        return std::sqrt(x * x + y * y);");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nPerformance:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Optimistic read: ~5-10ns (no locking!)");
        System.out.println("ReadWriteLock read: ~20-50ns");
        System.out.println("Best for: Very read-heavy workloads (>95% reads)");

        System.out.println("\n⚠️  Caution:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• NOT reentrant (deadlock risk!)");
        System.out.println("• More complex than ReadWriteLock");
        System.out.println("• Only use if profiling shows benefit");
    }

    //=============================================================================
    // 5. JAVA ATOMIC VARIABLES
    //=============================================================================

    /**
     * Atomic Variables - Lock-free thread-safe variables
     */
    static void demonstrateAtomicVariables() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  5. JAVA ATOMIC VARIABLES                                  ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Atomic Classes:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• AtomicInteger, AtomicLong");
        System.out.println("• AtomicBoolean");
        System.out.println("• AtomicReference<T>");
        System.out.println("• AtomicIntegerArray, AtomicLongArray, AtomicReferenceArray");
        System.out.println("• Lock-free (CAS-based)");
        System.out.println("• Thread-safe without locks");

        System.out.println("\nCode Examples:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Atomic counter");
        System.out.println("class OrderCounter {");
        System.out.println("    private AtomicLong counter = new AtomicLong(0);");
        System.out.println("    ");
        System.out.println("    public long getNextOrderId() {");
        System.out.println("        return counter.incrementAndGet();  // Atomic increment");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    public long getCurrentCount() {");
        System.out.println("        return counter.get();  // Atomic read");
        System.out.println("    }");
        System.out.println("}");
        System.out.println();
        System.out.println("// Atomic reference");
        System.out.println("class BestPrice {");
        System.out.println("    private AtomicReference<Price> bestBid = new AtomicReference<>();");
        System.out.println("    ");
        System.out.println("    public void updateBestBid(Price newPrice) {");
        System.out.println("        Price current;");
        System.out.println("        do {");
        System.out.println("            current = bestBid.get();");
        System.out.println("            if (current != null && current.value >= newPrice.value) {");
        System.out.println("                return;  // Not better, don't update");
        System.out.println("            }");
        System.out.println("        } while (!bestBid.compareAndSet(current, newPrice));");
        System.out.println("    }");
        System.out.println("}");

        // Actual example
        AtomicLong orderIdCounter = new AtomicLong(0);
        System.out.println("\nDemonstration:");
        System.out.println("AtomicLong counter = new AtomicLong(0);");
        System.out.println("counter.incrementAndGet(): " + orderIdCounter.incrementAndGet());
        System.out.println("counter.incrementAndGet(): " + orderIdCounter.incrementAndGet());
        System.out.println("counter.addAndGet(10):     " + orderIdCounter.addAndGet(10));
        System.out.println("counter.get():             " + orderIdCounter.get());

        System.out.println("\nCommon Operations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("get()                    // Read current value");
        System.out.println("set(newValue)            // Write new value");
        System.out.println("getAndSet(newValue)      // Atomic swap");
        System.out.println("compareAndSet(expect, update)  // CAS operation");
        System.out.println("incrementAndGet()        // Atomic ++i");
        System.out.println("getAndIncrement()        // Atomic i++");
        System.out.println("addAndGet(delta)         // Atomic add");
        System.out.println("updateAndGet(function)   // Atomic update (Java 8+)");

        System.out.println("\nC++ Equivalent:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("#include <atomic>");
        System.out.println();
        System.out.println("class OrderCounter {");
        System.out.println("    std::atomic<uint64_t> counter_{0};");
        System.out.println();
        System.out.println("public:");
        System.out.println("    uint64_t getNextOrderId() {");
        System.out.println("        return counter_.fetch_add(1, std::memory_order_relaxed) + 1;");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    uint64_t getCurrentCount() {");
        System.out.println("        return counter_.load(std::memory_order_relaxed);");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nJava vs C++ Atomic Operations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌──────────────────────────┬─────────────────┬──────────────────────┐");
        System.out.println("│ Operation                │ Java            │ C++                  │");
        System.out.println("├──────────────────────────┼─────────────────┼──────────────────────┤");
        System.out.println("│ Read                     │ get()           │ load()               │");
        System.out.println("│ Write                    │ set(val)        │ store(val)           │");
        System.out.println("│ CAS                      │ compareAndSet() │ compare_exchange     │");
        System.out.println("│ Add                      │ addAndGet()     │ fetch_add()          │");
        System.out.println("│ Increment                │ incrementAndGet│ fetch_add(1)         │");
        System.out.println("│ Memory ordering          │ Hidden          │ Explicit (relaxed...)│");
        System.out.println("└──────────────────────────┴─────────────────┴──────────────────────┘");

        System.out.println("\nPerformance:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Uncontended: ~5-10ns");
        System.out.println("Contended:   ~50-100ns (CAS retry loop)");
        System.out.println("Much faster than locks for simple operations");
    }

    //=============================================================================
    // 6. JAVA VOLATILE KEYWORD
    //=============================================================================

    /**
     * volatile - Visibility guarantee without locking
     */
    static void demonstrateVolatile() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  6. JAVA VOLATILE KEYWORD                                  ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("volatile Keyword:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Guarantees visibility across threads");
        System.out.println("• Prevents instruction reordering");
        System.out.println("• Does NOT provide atomicity for compound operations");
        System.out.println("• Reads/writes go directly to main memory (no caching)");
        System.out.println("• Lighter weight than synchronized");

        System.out.println("\nCode Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class MarketDataFeed {");
        System.out.println("    private volatile boolean running = true;  // Visible to all threads");
        System.out.println("    ");
        System.out.println("    public void run() {");
        System.out.println("        while (running) {  // Other thread can stop this");
        System.out.println("            processTick();");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    public void stop() {");
        System.out.println("        running = false;  // Immediately visible to reader thread");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\nWhat volatile Does:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Visibility: Changes visible to all threads immediately");
        System.out.println("✅ Ordering: Happens-before guarantee");
        System.out.println("✅ No caching: Reads from main memory");
        System.out.println("❌ NOT atomic for compound operations (i++)");
        System.out.println("❌ NOT a lock (no mutual exclusion)");

        System.out.println("\nWhen to Use volatile:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Boolean flags (running, shutdown, etc.)");
        System.out.println("✅ Simple state variables");
        System.out.println("✅ Reference updates (if no compound operations)");
        System.out.println("❌ Counters (use AtomicInteger instead)");
        System.out.println("❌ Compound operations (use locks or atomics)");

        System.out.println("\nC++ Equivalent:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class MarketDataFeed {");
        System.out.println("    std::atomic<bool> running_{true};  // C++ uses atomic");
        System.out.println();
        System.out.println("public:");
        System.out.println("    void run() {");
        System.out.println("        while (running_.load(std::memory_order_acquire)) {");
        System.out.println("            processTick();");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    void stop() {");
        System.out.println("        running_.store(false, std::memory_order_release);");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nJava volatile vs C++ atomic:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Java volatile: Visibility + ordering, but NOT atomic operations");
        System.out.println("• C++  atomic:   Visibility + ordering + atomic operations");
        System.out.println("• Java uses AtomicXxx for atomic operations");
        System.out.println("• C++ std::atomic covers both use cases");

        System.out.println("\nPerformance:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Read:  ~5-10ns (slightly slower than normal read)");
        System.out.println("Write: ~5-10ns (memory barrier overhead)");
        System.out.println("Much faster than synchronized or locks");
    }

    //=============================================================================
    // 7. C++ STD::MUTEX AND VARIANTS
    //=============================================================================

    static void demonstrateCppMutex() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  7. C++ STD::MUTEX AND VARIANTS                            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Mutex Types:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. std::mutex                - Basic mutual exclusion");
        System.out.println("2. std::recursive_mutex      - Reentrant (like Java synchronized)");
        System.out.println("3. std::timed_mutex          - With timeout support");
        System.out.println("4. std::shared_mutex (C++17) - Read/write lock");
        System.out.println("5. std::shared_timed_mutex   - Read/write with timeout");

        System.out.println("\n1. std::mutex (Basic):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class OrderBook {");
        System.out.println("    std::mutex mutex_;");
        System.out.println("    std::vector<Order> orders_;");
        System.out.println();
        System.out.println("public:");
        System.out.println("    void addOrder(Order order) {");
        System.out.println("        std::lock_guard<std::mutex> lock(mutex_);");
        System.out.println("        orders_.push_back(order);");
        System.out.println("    }  // Auto unlock");
        System.out.println("};");
        System.out.println();
        System.out.println("Java equivalent: synchronized");

        System.out.println("\n2. std::recursive_mutex (Reentrant):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class OrderBook {");
        System.out.println("    std::recursive_mutex mutex_;");
        System.out.println();
        System.out.println("public:");
        System.out.println("    void method1() {");
        System.out.println("        std::lock_guard<std::recursive_mutex> lock(mutex_);");
        System.out.println("        method2();  // ✅ OK, reentrant");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    void method2() {");
        System.out.println("        std::lock_guard<std::recursive_mutex> lock(mutex_);");
        System.out.println("        // Can acquire same lock again");
        System.out.println("    }");
        System.out.println("};");
        System.out.println();
        System.out.println("Java equivalent: synchronized (always reentrant)");

        System.out.println("\n3. std::timed_mutex (With Timeout):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class OrderBook {");
        System.out.println("    std::timed_mutex mutex_;");
        System.out.println();
        System.out.println("public:");
        System.out.println("    bool tryAddOrder(Order order) {");
        System.out.println("        if (mutex_.try_lock_for(100ms)) {");
        System.out.println("            orders_.push_back(order);");
        System.out.println("            mutex_.unlock();");
        System.out.println("            return true;");
        System.out.println("        }");
        System.out.println("        return false;  // Timeout");
        System.out.println("    }");
        System.out.println("};");
        System.out.println();
        System.out.println("Java equivalent: ReentrantLock.tryLock(timeout)");

        System.out.println("\n4. std::shared_mutex (Read/Write):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class PriceCache {");
        System.out.println("    std::shared_mutex mutex_;");
        System.out.println("    std::unordered_map<std::string, double> prices_;");
        System.out.println();
        System.out.println("public:");
        System.out.println("    // Shared (read) lock");
        System.out.println("    double getPrice(const std::string& symbol) {");
        System.out.println("        std::shared_lock<std::shared_mutex> lock(mutex_);");
        System.out.println("        return prices_[symbol];");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    // Exclusive (write) lock");
        System.out.println("    void updatePrice(const std::string& symbol, double price) {");
        System.out.println("        std::unique_lock<std::shared_mutex> lock(mutex_);");
        System.out.println("        prices_[symbol] = price;");
        System.out.println("    }");
        System.out.println("};");
        System.out.println();
        System.out.println("Java equivalent: ReadWriteLock");

        System.out.println("\nLock Guards (RAII):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("std::lock_guard<Mutex>    // Simple RAII lock");
        System.out.println("std::unique_lock<Mutex>   // Movable, can unlock early");
        System.out.println("std::shared_lock<Mutex>   // For shared_mutex (read lock)");
        System.out.println("std::scoped_lock<M...>    // Multiple mutexes (C++17)");

        System.out.println("\nJava vs C++ Mutex Comparison:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌────────────────────┬───────────────────┬─────────────────────┐");
        System.out.println("│ Feature            │ Java              │ C++                 │");
        System.out.println("├────────────────────┼───────────────────┼─────────────────────┤");
        System.out.println("│ Basic mutex        │ synchronized      │ std::mutex          │");
        System.out.println("│ Reentrant          │ synchronized      │ recursive_mutex     │");
        System.out.println("│ Explicit lock      │ ReentrantLock     │ std::mutex          │");
        System.out.println("│ Timeout            │ ReentrantLock     │ timed_mutex         │");
        System.out.println("│ Read/Write         │ ReadWriteLock     │ shared_mutex        │");
        System.out.println("│ RAII               │ ❌ No             │ ✅ lock_guard       │");
        System.out.println("│ Default reentrant  │ ✅ Yes            │ ❌ No (opt-in)      │");
        System.out.println("└────────────────────┴───────────────────┴─────────────────────┘");
    }

    //=============================================================================
    // 8. C++ STD::ATOMIC
    //=============================================================================

    static void demonstrateCppAtomic() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  8. C++ STD::ATOMIC                                        ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ std::atomic:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Lock-free atomic operations");
        System.out.println("• Explicit memory ordering control");
        System.out.println("• CAS operations");
        System.out.println("• Works with primitives and pointers");

        System.out.println("\nCode Examples:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("#include <atomic>");
        System.out.println();
        System.out.println("class OrderCounter {");
        System.out.println("    std::atomic<uint64_t> counter_{0};");
        System.out.println();
        System.out.println("public:");
        System.out.println("    uint64_t getNextOrderId() {");
        System.out.println("        // Atomic increment");
        System.out.println("        return counter_.fetch_add(1, std::memory_order_relaxed) + 1;");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    uint64_t getCurrentCount() {");
        System.out.println("        // Atomic load");
        System.out.println("        return counter_.load(std::memory_order_relaxed);");
        System.out.println("    }");
        System.out.println("};");

        System.out.println("\nAtomic Operations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("load(memory_order)         // Read");
        System.out.println("store(val, memory_order)   // Write");
        System.out.println("exchange(val, memory_order)  // Swap");
        System.out.println("compare_exchange_weak/strong  // CAS");
        System.out.println("fetch_add(delta, memory_order)  // Atomic add");
        System.out.println("fetch_sub(delta, memory_order)  // Atomic subtract");
        System.out.println("fetch_and/or/xor              // Atomic bitwise ops");

        System.out.println("\nJava vs C++ Atomic:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌──────────────────────┬─────────────────────┬──────────────────────┐");
        System.out.println("│ Operation            │ Java                │ C++                  │");
        System.out.println("├──────────────────────┼─────────────────────┼──────────────────────┤");
        System.out.println("│ Type                 │ AtomicInteger, etc. │ std::atomic<T>       │");
        System.out.println("│ Read                 │ get()               │ load(ordering)       │");
        System.out.println("│ Write                │ set(val)            │ store(val, ordering) │");
        System.out.println("│ CAS                  │ compareAndSet()     │ compare_exchange     │");
        System.out.println("│ Add                  │ addAndGet(delta)    │ fetch_add(delta, ..) │");
        System.out.println("│ Memory ordering      │ Hidden (seq_cst)    │ Explicit ⭐          │");
        System.out.println("│ Lock-free guarantee  │ Usually             │ is_lock_free()       │");
        System.out.println("└──────────────────────┴─────────────────────┴──────────────────────┘");
    }

    //=============================================================================
    // 9. C++ MEMORY ORDERING
    //=============================================================================

    static void demonstrateCppMemoryOrdering() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  9. C++ MEMORY ORDERING                                    ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Memory Orders:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("memory_order_relaxed   // No ordering guarantees (fastest)");
        System.out.println("memory_order_acquire   // Acquire barrier (for loads)");
        System.out.println("memory_order_release   // Release barrier (for stores)");
        System.out.println("memory_order_acq_rel   // Both acquire and release");
        System.out.println("memory_order_seq_cst   // Sequential consistency (default, slowest)");

        System.out.println("\nWhen to Use Each:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("relaxed:  Independent atomic operations (counters)");
        System.out.println("acquire:  Reading shared data (consume side)");
        System.out.println("release:  Writing shared data (produce side)");
        System.out.println("acq_rel:  Read-modify-write operations");
        System.out.println("seq_cst:  When in doubt, or need total order");

        System.out.println("\nCode Example (Producer-Consumer):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Producer");
        System.out.println("data.store(42, std::memory_order_relaxed);");
        System.out.println("ready.store(true, std::memory_order_release);  // Synchronizes");
        System.out.println();
        System.out.println("// Consumer");
        System.out.println("while (!ready.load(std::memory_order_acquire));  // Wait");
        System.out.println("int value = data.load(std::memory_order_relaxed);  // Guaranteed to see 42");

        System.out.println("\nJava Equivalent:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Java uses sequential consistency for all atomic operations");
        System.out.println("// (Similar to C++ memory_order_seq_cst)");
        System.out.println();
        System.out.println("// For relaxed semantics, use VarHandle (Java 9+)");
        System.out.println("VarHandle.getAcquire()   // Like memory_order_acquire");
        System.out.println("VarHandle.setRelease()   // Like memory_order_release");

        System.out.println("\nPerformance Impact:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("relaxed:  ~1-2ns  (no barriers) ⭐");
        System.out.println("acquire:  ~2-5ns  (one-way barrier)");
        System.out.println("release:  ~2-5ns  (one-way barrier)");
        System.out.println("seq_cst:  ~5-10ns (full barrier)");
        System.out.println();
        System.out.println("For HFT: Use relaxed when possible!");
    }

    //=============================================================================
    // 10. PERFORMANCE COMPARISON FOR TRADING SYSTEMS
    //=============================================================================

    static void demonstratePerformanceComparison() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  10. PERFORMANCE COMPARISON FOR TRADING SYSTEMS            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Latency Comparison (Uncontended):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌──────────────────────────────┬──────────┬──────────┐");
        System.out.println("│ Mechanism                    │ Java     │ C++      │");
        System.out.println("├──────────────────────────────┼──────────┼──────────┤");
        System.out.println("│ Atomic (relaxed)             │ N/A      │ 1-2ns ⭐  │");
        System.out.println("│ Atomic (seq_cst)             │ 5-10ns   │ 5-10ns   │");
        System.out.println("│ volatile read/write          │ 5-10ns   │ N/A      │");
        System.out.println("│ synchronized (biased)        │ 10-20ns  │ N/A      │");
        System.out.println("│ std::mutex (uncontended)     │ N/A      │ 15-25ns  │");
        System.out.println("│ ReentrantLock (unfair)       │ 20-40ns  │ N/A      │");
        System.out.println("│ synchronized (thin lock)     │ 30-50ns  │ N/A      │");
        System.out.println("│ ReentrantLock (fair)         │ 50-100ns │ N/A      │");
        System.out.println("└──────────────────────────────┴──────────┴──────────┘");

        System.out.println("\nLatency Comparison (Contended):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌──────────────────────────────┬──────────┬──────────┐");
        System.out.println("│ Mechanism                    │ Java     │ C++      │");
        System.out.println("├──────────────────────────────┼──────────┼──────────┤");
        System.out.println("│ Atomic CAS loop              │ 50-200ns │ 50-200ns │");
        System.out.println("│ synchronized                 │ 100-500ns│ N/A      │");
        System.out.println("│ std::mutex                   │ N/A      │ 100-500ns│");
        System.out.println("│ ReentrantLock                │ 150-600ns│ N/A      │");
        System.out.println("│ Park/Unpark (OS scheduler)   │ 1-10μs   │ 1-10μs   │");
        System.out.println("└──────────────────────────────┴──────────┴──────────┘");

        System.out.println("\nRecommendations by Latency Requirement:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌─────────────────────┬────────────────────────────────────┐");
        System.out.println("│ Latency Target      │ Recommended Mechanism              │");
        System.out.println("├─────────────────────┼────────────────────────────────────┤");
        System.out.println("│ <10ns (HFT)         │ C++ atomic (relaxed) ⭐            │");
        System.out.println("│ <50ns (HFT)         │ C++ atomic (seq_cst), Java atomic  │");
        System.out.println("│ <100ns              │ synchronized, std::mutex           │");
        System.out.println("│ <1μs                │ ReentrantLock, any mutex           │");
        System.out.println("│ >1μs                │ Any locking mechanism              │");
        System.out.println("└─────────────────────┴────────────────────────────────────┘");

        System.out.println("\nUse Case Recommendations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Counters/Sequence IDs:");
        System.out.println("  Java:  AtomicLong");
        System.out.println("  C++:   std::atomic<uint64_t> with memory_order_relaxed ⭐");
        System.out.println();
        System.out.println("Order Book Updates:");
        System.out.println("  Java:  synchronized or ReentrantLock");
        System.out.println("  C++:   std::mutex (or lock-free if expert)");
        System.out.println();
        System.out.println("Price Cache (read-heavy):");
        System.out.println("  Java:  ReadWriteLock or StampedLock");
        System.out.println("  C++:   std::shared_mutex ⭐");
        System.out.println();
        System.out.println("Ring Buffer (SPSC):");
        System.out.println("  Java:  volatile + AtomicLong for indices");
        System.out.println("  C++:   std::atomic with memory_order_acquire/release ⭐");
        System.out.println();
        System.out.println("Flags/State:");
        System.out.println("  Java:  volatile boolean");
        System.out.println("  C++:   std::atomic<bool> with memory_order_relaxed");

        System.out.println("\nKey Insights:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. C++ atomic with relaxed ordering: Fastest (1-2ns)");
        System.out.println("2. Java/C++ atomics (seq_cst): ~5-10ns");
        System.out.println("3. Locks (uncontended): ~20-50ns");
        System.out.println("4. Locks (contended): ~100-500ns");
        System.out.println("5. For HFT: Use C++ atomics with explicit memory ordering");
        System.out.println("6. For Java: AtomicXxx for counters, synchronized for complex");

        System.out.println("\nFinal Recommendations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Java Trading Systems:");
        System.out.println("  ✅ Use AtomicLong for order IDs");
        System.out.println("  ✅ Use synchronized for simple critical sections");
        System.out.println("  ✅ Use ReentrantLock when need tryLock/timeout");
        System.out.println("  ✅ Use ReadWriteLock for read-heavy caches");
        System.out.println("  ✅ Use volatile for flags");
        System.out.println();
        System.out.println("C++ Trading Systems (Ultra-low latency):");
        System.out.println("  ✅ Use std::atomic with memory_order_relaxed for counters ⭐");
        System.out.println("  ✅ Use std::atomic with acquire/release for synchronization");
        System.out.println("  ✅ Use std::mutex for complex critical sections");
        System.out.println("  ✅ Use std::shared_mutex for read-heavy data");
        System.out.println("  ✅ Lock-free ring buffers for IPC ⭐");

        System.out.println("\n🚀 C++ wins for ultra-low latency (<10ns) with explicit memory ordering!");
    }

    //=============================================================================
    // MAIN
    //=============================================================================

    public static void main(String[] args) {
        System.out.println("╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║                                                                ║");
        System.out.println("║     Java vs C++ Locking Mechanisms - Complete Comparison      ║");
        System.out.println("║          Ultra-Low Latency Trading Systems                     ║");
        System.out.println("║                                                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        demonstrateSynchronized();
        demonstrateReentrantLock();
        demonstrateReadWriteLock();
        demonstrateStampedLock();
        demonstrateAtomicVariables();
        demonstrateVolatile();
        demonstrateCppMutex();
        demonstrateCppAtomic();
        demonstrateCppMemoryOrdering();
        demonstratePerformanceComparison();

        System.out.println("\n╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║  SUMMARY                                                       ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        System.out.println("Java Locking Mechanisms:");
        System.out.println("• synchronized:   Simple, built-in, reentrant");
        System.out.println("• ReentrantLock:  Explicit, timeout, tryLock, fair/unfair");
        System.out.println("• ReadWriteLock:  Multiple readers, single writer");
        System.out.println("• StampedLock:    Optimistic locking (fastest for reads)");
        System.out.println("• Atomic*:        Lock-free, CAS-based");
        System.out.println("• volatile:       Visibility without locking");

        System.out.println("\nC++ Locking Mechanisms:");
        System.out.println("• std::mutex:           Basic mutex (not reentrant!)");
        System.out.println("• std::recursive_mutex: Reentrant mutex");
        System.out.println("• std::shared_mutex:    Read/write lock");
        System.out.println("• std::atomic<T>:       Lock-free with explicit memory ordering ⭐");
        System.out.println("• memory_order_*:       Fine-grained control (relaxed fastest!)");

        System.out.println("\nKey Differences:");
        System.out.println("1. Java: All locking is reentrant by default");
        System.out.println("2. C++:  Must use recursive_mutex for reentrancy");
        System.out.println("3. Java: Memory ordering is hidden (sequential consistency)");
        System.out.println("4. C++:  Explicit memory ordering (relaxed = 1-2ns!) ⭐");
        System.out.println("5. Java: RAII not needed (automatic unlock)");
        System.out.println("6. C++:  RAII with lock_guard, unique_lock");

        System.out.println("\nFor Ultra-Low Latency Trading:");
        System.out.println("C++ Wins: Atomic with memory_order_relaxed (1-2ns)");
        System.out.println("Java OK:  AtomicLong for most use cases (5-10ns)");
        System.out.println();
        System.out.println("Choose C++ for: <10ns latency requirements");
        System.out.println("Choose Java for: <100ns latency (easier development)");

        System.out.println("\n🚀 Know your latency budget, choose accordingly!");
    }
}

