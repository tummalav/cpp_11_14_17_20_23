/**
 * Java Memory Management - Complete Guide
 *
 * Focus: Stack, Heap, Native Memory, Off-Heap Memory, Memory Pools
 * Ultra-Low Latency Trading Systems
 *
 * Topics Covered:
 * 1. Java Memory Model Overview
 * 2. Stack Memory (Thread Stacks)
 * 3. Heap Memory (Young Gen, Old Gen, Metaspace)
 * 4. Native Memory (Off-Heap)
 * 5. Direct ByteBuffers (Off-Heap)
 * 6. Memory Mapped Files
 * 7. Memory Pools and JMX
 * 8. Memory Leaks and Detection
 * 9. Memory Tuning for Trading Systems
 * 10. Best Practices for Ultra-Low Latency
 *
 * Author: Java Memory Management Guide
 * Date: February 8, 2026
 */

import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.lang.management.*;
import java.util.*;

public class Java_Memory_Management_Complete_Guide {

    //=============================================================================
    // 1. JAVA MEMORY MODEL OVERVIEW
    //=============================================================================

    /**
     * Java Memory Architecture
     *
     * JVM Memory Structure:
     * ┌─────────────────────────────────────────────────────────┐
     * │                    JVM PROCESS                          │
     * ├─────────────────────────────────────────────────────────┤
     * │  ┌──────────────────────────────────────────────────┐  │
     * │  │              HEAP MEMORY                         │  │
     * │  │  (Managed by GC, -Xms/-Xmx)                      │  │
     * │  │  ┌────────────────┐  ┌──────────────────────┐   │  │
     * │  │  │  Young Gen     │  │     Old Gen          │   │  │
     * │  │  │  (Eden + S0/S1)│  │  (Tenured)           │   │  │
     * │  │  └────────────────┘  └──────────────────────┘   │  │
     * │  └──────────────────────────────────────────────────┘  │
     * │  ┌──────────────────────────────────────────────────┐  │
     * │  │           METASPACE (Non-Heap)                   │  │
     * │  │  (Class metadata, -XX:MetaspaceSize)             │  │
     * │  └──────────────────────────────────────────────────┘  │
     * │  ┌──────────────────────────────────────────────────┐  │
     * │  │         THREAD STACKS (Per-Thread)               │  │
     * │  │  Thread 1: [─────────] (-Xss1m)                  │  │
     * │  │  Thread 2: [─────────]                           │  │
     * │  │  Thread N: [─────────]                           │  │
     * │  └──────────────────────────────────────────────────┘  │
     * │  ┌──────────────────────────────────────────────────┐  │
     * │  │         NATIVE MEMORY (Off-Heap)                 │  │
     * │  │  - Direct ByteBuffers                            │  │
     * │  │  - Memory Mapped Files                           │  │
     * │  │  - JNI allocations                               │  │
     * │  │  - Thread stacks                                 │  │
     * │  │  - Code cache                                    │  │
     * │  └──────────────────────────────────────────────────┘  │
     * └─────────────────────────────────────────────────────────┘
     */
    static void demonstrateMemoryOverview() {
        System.out.println("╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  1. JAVA MEMORY MODEL OVERVIEW                            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("JVM Memory Areas:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. HEAP MEMORY (GC-managed)");
        System.out.println("   • Young Generation (Eden + Survivor spaces)");
        System.out.println("   • Old Generation (Tenured)");
        System.out.println("   • Controlled by: -Xms (initial) -Xmx (maximum)");
        System.out.println();
        System.out.println("2. NON-HEAP MEMORY");
        System.out.println("   • Metaspace (class metadata, replaces PermGen)");
        System.out.println("   • Code Cache (JIT compiled code)");
        System.out.println("   • Controlled by: -XX:MetaspaceSize, -XX:MaxMetaspaceSize");
        System.out.println();
        System.out.println("3. THREAD STACKS (per thread)");
        System.out.println("   • Local variables, method calls, partial results");
        System.out.println("   • Controlled by: -Xss (stack size per thread)");
        System.out.println();
        System.out.println("4. NATIVE MEMORY (Off-Heap)");
        System.out.println("   • Direct ByteBuffers");
        System.out.println("   • Memory Mapped Files");
        System.out.println("   • JNI allocations");
        System.out.println("   • Internal JVM structures");

        // Get memory information
        Runtime runtime = Runtime.getRuntime();
        MemoryMXBean memoryMXBean = ManagementFactory.getMemoryMXBean();

        System.out.println("\nCurrent Memory Usage:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.printf("Heap Memory:     %,d MB / %,d MB (max: %,d MB)\n",
            runtime.totalMemory() / 1024 / 1024,
            (runtime.totalMemory() - runtime.freeMemory()) / 1024 / 1024,
            runtime.maxMemory() / 1024 / 1024);

        MemoryUsage heapUsage = memoryMXBean.getHeapMemoryUsage();
        System.out.printf("Heap Used:       %,d MB\n", heapUsage.getUsed() / 1024 / 1024);
        System.out.printf("Heap Committed:  %,d MB\n", heapUsage.getCommitted() / 1024 / 1024);
        System.out.printf("Heap Max:        %,d MB\n", heapUsage.getMax() / 1024 / 1024);

        MemoryUsage nonHeapUsage = memoryMXBean.getNonHeapMemoryUsage();
        System.out.printf("Non-Heap Used:   %,d MB\n", nonHeapUsage.getUsed() / 1024 / 1024);
        System.out.printf("Non-Heap Committed: %,d MB\n", nonHeapUsage.getCommitted() / 1024 / 1024);
    }

    //=============================================================================
    // 2. STACK MEMORY (THREAD STACKS)
    //=============================================================================

    /**
     * Stack Memory - Per-Thread Memory
     *
     * Each thread has its own stack containing:
     * - Local variables
     * - Method parameters
     * - Return addresses
     * - Partial results
     */
    static void demonstrateStackMemory() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  2. STACK MEMORY (Thread Stacks)                           ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Stack Memory Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Per-thread allocation (each thread has its own stack)");
        System.out.println("• LIFO (Last In, First Out) structure");
        System.out.println("• Stores: local variables, method calls, return addresses");
        System.out.println("• Fast allocation/deallocation (push/pop)");
        System.out.println("• Fixed size per thread (default ~1MB)");
        System.out.println("• NOT managed by GC");
        System.out.println("• Automatic cleanup when method returns");

        System.out.println("\nWhat Goes on Stack:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Primitive local variables (int, long, double, etc.)");
        System.out.println("✅ Object references (pointer to heap object)");
        System.out.println("✅ Method parameters");
        System.out.println("✅ Return addresses");
        System.out.println("❌ Objects themselves (always on heap!)");

        System.out.println("\nStack Frame Structure:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("method3()  ┌─────────────────┐  ← Stack Top");
        System.out.println("           │ Local vars      │");
        System.out.println("           │ Return address  │");
        System.out.println("           ├─────────────────┤");
        System.out.println("method2()  │ Local vars      │");
        System.out.println("           │ Return address  │");
        System.out.println("           ├─────────────────┤");
        System.out.println("method1()  │ Local vars      │");
        System.out.println("           │ Return address  │");
        System.out.println("           ├─────────────────┤");
        System.out.println("main()     │ Local vars      │");
        System.out.println("           └─────────────────┘  ← Stack Bottom");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-Xss1m              # Set stack size to 1MB per thread");
        System.out.println("-Xss512k            # Set stack size to 512KB");
        System.out.println("-Xss2m              # Set stack size to 2MB");

        System.out.println("\nStack Memory Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        demonstrateStackAllocation();

        System.out.println("\nStack Overflow:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Caused by: Too deep recursion or too many method calls");
        System.out.println("Error: java.lang.StackOverflowError");
        System.out.println("Solution: Increase -Xss or fix recursive algorithm");

        System.out.println("\nFor Trading Systems:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Default 1MB usually sufficient");
        System.out.println("• Deep call stacks → increase -Xss");
        System.out.println("• Many threads → consider stack size (100 threads = 100MB)");
    }

    static void demonstrateStackAllocation() {
        // Primitives on stack
        int orderId = 12345;              // Stack
        double price = 100.50;            // Stack
        long timestamp = System.currentTimeMillis();  // Stack

        // Object reference on stack, object on heap
        String symbol = "AAPL";           // Reference on stack, "AAPL" on heap
        Order order = new Order();        // Reference on stack, object on heap

        System.out.println("Stack variables created:");
        System.out.println("  orderId (int):     " + orderId + " [on stack]");
        System.out.println("  price (double):    " + price + " [on stack]");
        System.out.println("  timestamp (long):  " + timestamp + " [on stack]");
        System.out.println("  symbol (ref):      " + symbol + " [reference on stack, object on heap]");
        System.out.println("  order (ref):       " + order + " [reference on stack, object on heap]");
    }

    static class Order {
        int id;
        double price;
    }

    //=============================================================================
    // 3. HEAP MEMORY (YOUNG GEN, OLD GEN, METASPACE)
    //=============================================================================

    /**
     * Heap Memory - GC-managed memory for objects
     *
     * Structure:
     * ┌──────────────────────────────────────────────────┐
     * │              HEAP MEMORY                         │
     * ├──────────────────────┬───────────────────────────┤
     * │   Young Generation   │    Old Generation         │
     * │  ┌──────┬────┬────┐  │  (Tenured)                │
     * │  │ Eden │ S0 │ S1 │  │  Long-lived objects       │
     * │  └──────┴────┴────┘  │                           │
     * │  New objects          │                           │
     * └──────────────────────┴───────────────────────────┘
     */
    static void demonstrateHeapMemory() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  3. HEAP MEMORY (Young Gen, Old Gen, Metaspace)            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Heap Memory Structure:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. YOUNG GENERATION (~1/3 of heap)");
        System.out.println("   • Eden Space: Where new objects are allocated");
        System.out.println("   • Survivor Space 0 (S0): Surviving objects from Eden");
        System.out.println("   • Survivor Space 1 (S1): Surviving objects from S0");
        System.out.println("   • Minor GC: Collects Young Generation");
        System.out.println();
        System.out.println("2. OLD GENERATION (Tenured) (~2/3 of heap)");
        System.out.println("   • Long-lived objects (survived multiple GCs)");
        System.out.println("   • Promoted from Young Gen after aging");
        System.out.println("   • Major GC: Collects Old Generation");
        System.out.println();
        System.out.println("3. METASPACE (Non-Heap, replaces PermGen)");
        System.out.println("   • Class metadata");
        System.out.println("   • Method metadata");
        System.out.println("   • Constant pools");
        System.out.println("   • Uses native memory (auto-grows)");

        System.out.println("\nObject Lifecycle:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Object created → Eden Space");
        System.out.println("2. Eden full → Minor GC → survivors to S0");
        System.out.println("3. Next Minor GC → S0 → S1 (age++)");
        System.out.println("4. Age threshold reached → Promoted to Old Gen");
        System.out.println("5. Old Gen full → Major GC (expensive!)");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Heap sizing");
        System.out.println("-Xms8g                  # Initial heap size");
        System.out.println("-Xmx8g                  # Maximum heap size (set same as -Xms!)");
        System.out.println();
        System.out.println("# Young Gen sizing");
        System.out.println("-XX:NewSize=2g          # Initial Young Gen size");
        System.out.println("-XX:MaxNewSize=2g       # Maximum Young Gen size");
        System.out.println("-XX:NewRatio=2          # Old/Young ratio (Old = 2 * Young)");
        System.out.println();
        System.out.println("# Survivor ratio");
        System.out.println("-XX:SurvivorRatio=8     # Eden/Survivor ratio (Eden = 8 * Survivor)");
        System.out.println();
        System.out.println("# Metaspace sizing");
        System.out.println("-XX:MetaspaceSize=256m  # Initial metaspace size");
        System.out.println("-XX:MaxMetaspaceSize=512m  # Maximum metaspace size");

        // Get detailed heap info
        List<MemoryPoolMXBean> memoryPools = ManagementFactory.getMemoryPoolMXBeans();
        System.out.println("\nMemory Pools:");
        System.out.println("───────────────────────────────────────────────────────────");
        for (MemoryPoolMXBean pool : memoryPools) {
            MemoryUsage usage = pool.getUsage();
            System.out.printf("%-30s %,10d MB / %,10d MB\n",
                pool.getName() + ":",
                usage.getUsed() / 1024 / 1024,
                usage.getMax() > 0 ? usage.getMax() / 1024 / 1024 : -1);
        }

        System.out.println("\nWhat Goes on Heap:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ All objects (created with 'new')");
        System.out.println("✅ Instance variables");
        System.out.println("✅ Arrays");
        System.out.println("✅ Strings");
        System.out.println("❌ Primitives (unless wrapped in objects)");
        System.out.println("❌ Local variables (on stack)");

        System.out.println("\nFor Trading Systems:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Set -Xms = -Xmx (prevent resizing pauses)");
        System.out.println("• Size heap for working set + headroom");
        System.out.println("• Young Gen: 1/3 to 1/2 of total heap");
        System.out.println("• Monitor heap usage with JMX");
        System.out.println("• Use ZGC for large heaps (>16GB)");
    }

    //=============================================================================
    // 4. NATIVE MEMORY (OFF-HEAP)
    //=============================================================================

    /**
     * Native Memory - Memory outside JVM heap
     *
     * NOT managed by GC!
     */
    static void demonstrateNativeMemory() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  4. NATIVE MEMORY (Off-Heap)                               ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Native Memory Areas:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Thread Stacks");
        System.out.println("   • -Xss per thread");
        System.out.println("   • 100 threads × 1MB = 100MB");
        System.out.println();
        System.out.println("2. Direct ByteBuffers");
        System.out.println("   • ByteBuffer.allocateDirect()");
        System.out.println("   • Zero-copy I/O");
        System.out.println("   • Limited by -XX:MaxDirectMemorySize");
        System.out.println();
        System.out.println("3. Memory Mapped Files");
        System.out.println("   • FileChannel.map()");
        System.out.println("   • Fast file I/O");
        System.out.println("   • OS-managed memory");
        System.out.println();
        System.out.println("4. Code Cache");
        System.out.println("   • JIT compiled code");
        System.out.println("   • -XX:ReservedCodeCacheSize");
        System.out.println();
        System.out.println("5. Metaspace");
        System.out.println("   • Class metadata");
        System.out.println("   • -XX:MaxMetaspaceSize");
        System.out.println();
        System.out.println("6. JNI Allocations");
        System.out.println("   • Native libraries");
        System.out.println("   • C/C++ allocations via JNI");
        System.out.println();
        System.out.println("7. Internal JVM Structures");
        System.out.println("   • GC structures");
        System.out.println("   • Symbol tables");
        System.out.println("   • etc.");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:MaxDirectMemorySize=4g     # Max direct buffer memory");
        System.out.println("-XX:ReservedCodeCacheSize=256m # Code cache size");
        System.out.println("-XX:MaxMetaspaceSize=512m      # Max metaspace");

        System.out.println("\nNative Memory Tracking:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Enable native memory tracking");
        System.out.println("-XX:NativeMemoryTracking=summary  # or 'detail'");
        System.out.println();
        System.out.println("# Query native memory");
        System.out.println("jcmd <pid> VM.native_memory summary");
        System.out.println("jcmd <pid> VM.native_memory detail");

        System.out.println("\nAdvantages of Off-Heap:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ NOT affected by GC (no GC pauses)");
        System.out.println("✅ Can exceed heap size limits");
        System.out.println("✅ Zero-copy I/O operations");
        System.out.println("✅ Shared between processes (memory mapped files)");
        System.out.println("✅ Predictable latency");

        System.out.println("\nDisadvantages of Off-Heap:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("❌ Manual memory management required");
        System.out.println("❌ Memory leaks if not freed properly");
        System.out.println("❌ No automatic cleanup");
        System.out.println("❌ More complex to use");

        System.out.println("\nFor Trading Systems:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Use for: Market data buffers, IPC, ring buffers");
        System.out.println("✅ Benefit: Zero GC impact on critical path");
        System.out.println("⚠️  Caution: Must manage lifecycle carefully");
    }

    //=============================================================================
    // 5. DIRECT BYTEBUFFERS (OFF-HEAP)
    //=============================================================================

    /**
     * Direct ByteBuffers - Off-heap buffers for I/O
     */
    static void demonstrateDirectByteBuffers() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  5. DIRECT BYTEBUFFERS (Off-Heap)                          ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Heap ByteBuffer vs Direct ByteBuffer:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌─────────────────────┬──────────────────┬──────────────────┐");
        System.out.println("│ Feature             │ Heap ByteBuffer  │ Direct ByteBuffer│");
        System.out.println("├─────────────────────┼──────────────────┼──────────────────┤");
        System.out.println("│ Allocation          │ byte[] on heap   │ Native memory    │");
        System.out.println("│ GC impact           │ ✅ GC managed    │ ❌ No GC impact  │");
        System.out.println("│ Creation speed      │ Fast             │ Slow             │");
        System.out.println("│ I/O performance     │ Slower (copy)    │ Faster (no copy) │");
        System.out.println("│ Memory limit        │ Heap size        │ MaxDirectMemory  │");
        System.out.println("│ Cleanup             │ Automatic        │ Manual (cleaner) │");
        System.out.println("│ Best for            │ Short-lived      │ Long-lived I/O   │");
        System.out.println("└─────────────────────┴──────────────────┴──────────────────┘");

        System.out.println("\nCode Examples:");
        System.out.println("───────────────────────────────────────────────────────────");

        // Heap ByteBuffer
        System.out.println("// Heap ByteBuffer (on heap, GC-managed)");
        ByteBuffer heapBuffer = ByteBuffer.allocate(1024);
        System.out.println("ByteBuffer heapBuffer = ByteBuffer.allocate(1024);");
        System.out.println("  isDirect: " + heapBuffer.isDirect());  // false
        System.out.println("  capacity: " + heapBuffer.capacity());

        // Direct ByteBuffer
        System.out.println("\n// Direct ByteBuffer (off-heap, NOT GC-managed)");
        ByteBuffer directBuffer = ByteBuffer.allocateDirect(1024);
        System.out.println("ByteBuffer directBuffer = ByteBuffer.allocateDirect(1024);");
        System.out.println("  isDirect: " + directBuffer.isDirect());  // true
        System.out.println("  capacity: " + directBuffer.capacity());

        System.out.println("\nUsage Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Write market data to direct buffer");
        System.out.println("ByteBuffer marketData = ByteBuffer.allocateDirect(4096);");
        System.out.println("marketData.putLong(timestamp);");
        System.out.println("marketData.putInt(symbolId);");
        System.out.println("marketData.putDouble(bidPrice);");
        System.out.println("marketData.putDouble(askPrice);");
        System.out.println("marketData.putInt(bidSize);");
        System.out.println("marketData.putInt(askSize);");
        System.out.println("marketData.flip();  // Prepare for reading");

        // Actual usage
        ByteBuffer marketDataBuffer = ByteBuffer.allocateDirect(48);
        long timestamp = System.currentTimeMillis();
        marketDataBuffer.putLong(timestamp);      // 8 bytes
        marketDataBuffer.putInt(12345);           // 4 bytes (symbol ID)
        marketDataBuffer.putDouble(100.50);       // 8 bytes (bid)
        marketDataBuffer.putDouble(100.55);       // 8 bytes (ask)
        marketDataBuffer.putInt(1000);            // 4 bytes (bid size)
        marketDataBuffer.putInt(500);             // 4 bytes (ask size)
        marketDataBuffer.flip();

        System.out.println("\nBuffer written with market data:");
        System.out.printf("  Position: %d, Limit: %d, Capacity: %d\n",
            marketDataBuffer.position(), marketDataBuffer.limit(), marketDataBuffer.capacity());

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:MaxDirectMemorySize=4g     # Limit direct buffer memory");
        System.out.println();
        System.out.println("# If not set, defaults to: -Xmx minus survivor space");

        System.out.println("\nWhen to Use Direct ByteBuffers:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ High-throughput network I/O");
        System.out.println("✅ File I/O operations");
        System.out.println("✅ Long-lived buffers");
        System.out.println("✅ Zero-copy operations");
        System.out.println("❌ Short-lived buffers (creation overhead)");
        System.out.println("❌ Small buffers (<1KB)");

        System.out.println("\nFor Trading Systems:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Market data buffers (zero GC impact)");
        System.out.println("✅ Network I/O (FIX protocol, binary protocols)");
        System.out.println("✅ Ring buffers for order flow");
        System.out.println("✅ Pre-allocated pools of direct buffers");
    }

    //=============================================================================
    // 6. MEMORY MAPPED FILES
    //=============================================================================

    /**
     * Memory Mapped Files - Map file to memory
     */
    static void demonstrateMemoryMappedFiles() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  6. MEMORY MAPPED FILES                                    ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Memory Mapped Files:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Map file directly to memory");
        System.out.println("• OS manages paging (no explicit read/write)");
        System.out.println("• Shared memory between processes");
        System.out.println("• Very fast for large files");
        System.out.println("• Zero-copy I/O");

        System.out.println("\nCode Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Create memory mapped file");
        System.out.println("try (FileChannel channel = FileChannel.open(");
        System.out.println("        Paths.get(\"market_data.bin\"),");
        System.out.println("        StandardOpenOption.READ,");
        System.out.println("        StandardOpenOption.WRITE,");
        System.out.println("        StandardOpenOption.CREATE)) {");
        System.out.println("    ");
        System.out.println("    // Map 1MB of file to memory");
        System.out.println("    MappedByteBuffer buffer = channel.map(");
        System.out.println("        FileChannel.MapMode.READ_WRITE, 0, 1024 * 1024);");
        System.out.println("    ");
        System.out.println("    // Write data (appears in file automatically)");
        System.out.println("    buffer.putLong(System.currentTimeMillis());");
        System.out.println("    buffer.putDouble(100.50);  // Price");
        System.out.println("    ");
        System.out.println("    // Force to disk");
        System.out.println("    buffer.force();");
        System.out.println("}");

        System.out.println("\nMapping Modes:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("READ_ONLY:   Read-only access");
        System.out.println("READ_WRITE:  Read and write access");
        System.out.println("PRIVATE:     Copy-on-write (changes not written to file)");

        System.out.println("\nAdvantages:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Very fast for large files (no read/write syscalls)");
        System.out.println("✅ OS manages paging (automatic)");
        System.out.println("✅ Shared memory IPC");
        System.out.println("✅ Zero-copy I/O");
        System.out.println("✅ Survives process restart");

        System.out.println("\nDisadvantages:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("❌ Platform-dependent behavior");
        System.out.println("❌ Hard to unmap (JVM limitation)");
        System.out.println("❌ Can cause OutOfMemoryError (address space)");
        System.out.println("❌ Windows file locking issues");

        System.out.println("\nUse Cases for Trading:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Historical market data storage");
        System.out.println("✅ Tick data persistence");
        System.out.println("✅ Order book snapshots");
        System.out.println("✅ IPC between trading components");
        System.out.println("✅ Low-latency logging (Chronicle Queue)");

        System.out.println("\nExample: Trading Data Structure:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Memory layout for tick data");
        System.out.println("struct Tick {");
        System.out.println("    long timestamp;    // 8 bytes");
        System.out.println("    int symbolId;      // 4 bytes");
        System.out.println("    double bidPrice;   // 8 bytes");
        System.out.println("    double askPrice;   // 8 bytes");
        System.out.println("    int bidSize;       // 4 bytes");
        System.out.println("    int askSize;       // 4 bytes");
        System.out.println("};  // Total: 36 bytes per tick");
        System.out.println();
        System.out.println("// 1 million ticks = 36MB memory mapped file");

        System.out.println("\nBest Practices:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Use for large files (>10MB)");
        System.out.println("• Pre-allocate file size");
        System.out.println("• Use force() sparingly (performance)");
        System.out.println("• Consider page alignment (4KB boundaries)");
        System.out.println("• Monitor address space usage");
    }

    //=============================================================================
    // 7. MEMORY POOLS AND JMX
    //=============================================================================

    static void demonstrateMemoryPools() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  7. MEMORY POOLS AND JMX MONITORING                        ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Memory Pools:");
        System.out.println("───────────────────────────────────────────────────────────");

        List<MemoryPoolMXBean> pools = ManagementFactory.getMemoryPoolMXBeans();
        for (MemoryPoolMXBean pool : pools) {
            MemoryUsage usage = pool.getUsage();
            MemoryUsage peakUsage = pool.getPeakUsage();

            System.out.println("\n" + pool.getName() + ":");
            System.out.println("  Type: " + pool.getType());
            System.out.printf("  Used:      %,10d KB\n", usage.getUsed() / 1024);
            System.out.printf("  Committed: %,10d KB\n", usage.getCommitted() / 1024);
            System.out.printf("  Max:       %,10d KB\n", usage.getMax() > 0 ? usage.getMax() / 1024 : -1);
            System.out.printf("  Peak Used: %,10d KB\n", peakUsage.getUsed() / 1024);
        }

        System.out.println("\n\nGarbage Collectors:");
        System.out.println("───────────────────────────────────────────────────────────");
        List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
        for (GarbageCollectorMXBean gc : gcBeans) {
            System.out.println(gc.getName() + ":");
            System.out.println("  Collection Count: " + gc.getCollectionCount());
            System.out.println("  Collection Time:  " + gc.getCollectionTime() + " ms");
        }

        System.out.println("\n\nJMX Monitoring Code:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Get memory information");
        System.out.println("MemoryMXBean memoryBean = ManagementFactory.getMemoryMXBean();");
        System.out.println("MemoryUsage heapUsage = memoryBean.getHeapMemoryUsage();");
        System.out.println();
        System.out.println("System.out.println(\"Heap Used: \" + heapUsage.getUsed());");
        System.out.println("System.out.println(\"Heap Max:  \" + heapUsage.getMax());");

        System.out.println("\n\nMonitoring Tools:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• JConsole:  Built-in JMX monitoring GUI");
        System.out.println("• VisualVM:  Advanced profiling and monitoring");
        System.out.println("• JMC:       Java Mission Control (advanced)");
        System.out.println("• Custom:    JMX API for programmatic monitoring");
    }

    //=============================================================================
    // 8. MEMORY LEAKS AND DETECTION
    //=============================================================================

    static void demonstrateMemoryLeaks() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  8. MEMORY LEAKS AND DETECTION                             ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Common Causes of Memory Leaks:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Static collections that grow indefinitely");
        System.out.println("2. Listeners not unregistered");
        System.out.println("3. ThreadLocal not cleaned up");
        System.out.println("4. Unclosed resources (connections, streams)");
        System.out.println("5. Cache without eviction policy");
        System.out.println("6. ClassLoader leaks");

        System.out.println("\nExample Memory Leak:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// ❌ BAD: Memory leak!");
        System.out.println("public class OrderCache {");
        System.out.println("    private static Map<String, Order> cache = new HashMap<>();");
        System.out.println("    ");
        System.out.println("    public void addOrder(Order order) {");
        System.out.println("        cache.put(order.id, order);  // Never removed!");
        System.out.println("    }");
        System.out.println("}");
        System.out.println();
        System.out.println("// ✅ GOOD: Use eviction policy");
        System.out.println("private static Map<String, Order> cache = ");
        System.out.println("    Collections.synchronizedMap(new LinkedHashMap<String, Order>(");
        System.out.println("        1000, 0.75f, true) {");
        System.out.println("            protected boolean removeEldestEntry(Map.Entry eldest) {");
        System.out.println("                return size() > 1000;  // Max 1000 entries");
        System.out.println("            }");
        System.out.println("        });");

        System.out.println("\nDetection Tools:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Heap Dump Analysis");
        System.out.println("   jmap -dump:live,format=b,file=heap.bin <pid>");
        System.out.println("   Analyze with: Eclipse MAT, VisualVM, JProfiler");
        System.out.println();
        System.out.println("2. GC Logs");
        System.out.println("   -Xlog:gc*:file=gc.log:time,level,tags");
        System.out.println("   Watch for: Old Gen continuously growing");
        System.out.println();
        System.out.println("3. JFR (Java Flight Recorder)");
        System.out.println("   -XX:StartFlightRecording=duration=60s,filename=recording.jfr");
        System.out.println();
        System.out.println("4. Profilers");
        System.out.println("   • YourKit");
        System.out.println("   • JProfiler");
        System.out.println("   • Async Profiler");

        System.out.println("\nPrevention:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("✅ Use try-with-resources for AutoCloseable");
        System.out.println("✅ Implement cache eviction policies");
        System.out.println("✅ Clean up ThreadLocal variables");
        System.out.println("✅ Unregister listeners when done");
        System.out.println("✅ Use weak references for caches");
        System.out.println("✅ Monitor heap growth in production");
    }

    //=============================================================================
    // 9. MEMORY TUNING FOR TRADING SYSTEMS
    //=============================================================================

    static void demonstrateMemoryTuning() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  9. MEMORY TUNING FOR TRADING SYSTEMS                      ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Complete JVM Configuration for HFT:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("#!/bin/bash");
        System.out.println("# Ultra-low latency trading system");
        System.out.println();
        System.out.println("java \\");
        System.out.println("  # Heap sizing (fixed size!)");
        System.out.println("  -Xms16g -Xmx16g \\");
        System.out.println("  ");
        System.out.println("  # GC (ZGC for ultra-low latency)");
        System.out.println("  -XX:+UseZGC \\");
        System.out.println("  -XX:+AlwaysPreTouch \\");
        System.out.println("  ");
        System.out.println("  # Off-heap memory");
        System.out.println("  -XX:MaxDirectMemorySize=4g \\");
        System.out.println("  ");
        System.out.println("  # Thread stacks");
        System.out.println("  -Xss1m \\");
        System.out.println("  ");
        System.out.println("  # Metaspace");
        System.out.println("  -XX:MetaspaceSize=256m \\");
        System.out.println("  -XX:MaxMetaspaceSize=512m \\");
        System.out.println("  ");
        System.out.println("  # Code cache");
        System.out.println("  -XX:ReservedCodeCacheSize=256m \\");
        System.out.println("  ");
        System.out.println("  # Large pages");
        System.out.println("  -XX:+UseLargePages \\");
        System.out.println("  ");
        System.out.println("  # NUMA aware");
        System.out.println("  -XX:+UseNUMA \\");
        System.out.println("  ");
        System.out.println("  # Disable biased locking (can cause pauses)");
        System.out.println("  -XX:-UseBiasedLocking \\");
        System.out.println("  ");
        System.out.println("  # Native memory tracking");
        System.out.println("  -XX:NativeMemoryTracking=summary \\");
        System.out.println("  ");
        System.out.println("  # GC logging");
        System.out.println("  -Xlog:gc*:file=gc.log:time,level,tags \\");
        System.out.println("  ");
        System.out.println("  HFTTradingEngine");

        System.out.println("\n\nMemory Sizing Guidelines:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Component              Recommended Size");
        System.out.println("──────────────────────────────────────────────────────────");
        System.out.println("Heap (-Xmx)            Working set + 30% headroom");
        System.out.println("Direct Memory          2-4GB for I/O buffers");
        System.out.println("Thread Stack (-Xss)    512KB - 1MB per thread");
        System.out.println("Metaspace              256MB - 512MB");
        System.out.println("Code Cache             128MB - 256MB");

        System.out.println("\n\nTotal Memory Calculation:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Total JVM Memory = ");
        System.out.println("    Heap (-Xmx)");
        System.out.println("  + Direct Memory (-XX:MaxDirectMemorySize)");
        System.out.println("  + Metaspace (-XX:MaxMetaspaceSize)");
        System.out.println("  + Code Cache (-XX:ReservedCodeCacheSize)");
        System.out.println("  + Thread Stacks (num_threads × -Xss)");
        System.out.println("  + Internal JVM overhead (~500MB)");
        System.out.println();
        System.out.println("Example:");
        System.out.println("  16GB heap");
        System.out.println("+ 4GB direct memory");
        System.out.println("+ 512MB metaspace");
        System.out.println("+ 256MB code cache");
        System.out.println("+ 100MB thread stacks (100 threads × 1MB)");
        System.out.println("+ 500MB JVM overhead");
        System.out.println("─────────────────");
        System.out.println("= ~21.4GB total");
        System.out.println();
        System.out.println("→ Provision 24GB RAM minimum");

        System.out.println("\n\nOS-Level Tuning:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Huge pages (Linux)");
        System.out.println("echo 10240 > /proc/sys/vm/nr_hugepages  # 20GB of 2MB pages");
        System.out.println();
        System.out.println("# Disable THP");
        System.out.println("echo never > /sys/kernel/mm/transparent_hugepage/enabled");
        System.out.println();
        System.out.println("# Disable swap");
        System.out.println("swapoff -a");
        System.out.println();
        System.out.println("# Increase max map count (for memory mapped files)");
        System.out.println("sysctl -w vm.max_map_count=262144");

        System.out.println("\n\nMonitoring:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Native memory tracking");
        System.out.println("jcmd <pid> VM.native_memory summary");
        System.out.println();
        System.out.println("# Check GC overhead");
        System.out.println("jstat -gcutil <pid> 1000  # Every 1 second");
        System.out.println();
        System.out.println("# Heap histogram");
        System.out.println("jcmd <pid> GC.class_histogram");
    }

    //=============================================================================
    // 10. BEST PRACTICES FOR ULTRA-LOW LATENCY
    //=============================================================================

    static void demonstrateBestPractices() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  10. BEST PRACTICES FOR ULTRA-LOW LATENCY                  ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("✅ DO:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Set -Xms = -Xmx (prevent heap resizing)");
        System.out.println("2. Use -XX:+AlwaysPreTouch (pre-allocate memory)");
        System.out.println("3. Enable huge pages (-XX:+UseLargePages)");
        System.out.println("4. Use ZGC for <1ms GC pauses");
        System.out.println("5. Use Direct ByteBuffers for I/O");
        System.out.println("6. Object pooling for hot path objects");
        System.out.println("7. Pre-size collections (ArrayList, HashMap)");
        System.out.println("8. Use primitive arrays instead of object arrays");
        System.out.println("9. Monitor native memory usage");
        System.out.println("10. Use off-heap for GC-free critical data");

        System.out.println("\n❌ DON'T:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Don't allocate in hot path");
        System.out.println("2. Don't use finalize() (unpredictable)");
        System.out.println("3. Don't rely on System.gc()");
        System.out.println("4. Don't use excessive ThreadLocal");
        System.out.println("5. Don't create temporary objects in loops");
        System.out.println("6. Don't use String concatenation in loops");
        System.out.println("7. Don't forget to clean up off-heap memory");
        System.out.println("8. Don't ignore GC logs");

        System.out.println("\nObject Pooling Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// ✅ GOOD: Object pool (zero allocation in hot path)");
        System.out.println("public class OrderPool {");
        System.out.println("    private final Queue<Order> pool = new ConcurrentLinkedQueue<>();");
        System.out.println("    ");
        System.out.println("    public OrderPool(int size) {");
        System.out.println("        for (int i = 0; i < size; i++) {");
        System.out.println("            pool.offer(new Order());");
        System.out.println("        }");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    public Order acquire() {");
        System.out.println("        Order order = pool.poll();");
        System.out.println("        return order != null ? order : new Order();");
        System.out.println("    }");
        System.out.println("    ");
        System.out.println("    public void release(Order order) {");
        System.out.println("        order.reset();  // Clear state");
        System.out.println("        pool.offer(order);");
        System.out.println("    }");
        System.out.println("}");

        System.out.println("\nOff-Heap Data Structure Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Market data in direct buffer (GC-free!)");
        System.out.println("ByteBuffer marketData = ByteBuffer.allocateDirect(1024 * 1024);");
        System.out.println();
        System.out.println("// Write tick data");
        System.out.println("void writeTick(ByteBuffer buf, int offset, Tick tick) {");
        System.out.println("    buf.putLong(offset, tick.timestamp);");
        System.out.println("    buf.putInt(offset + 8, tick.symbolId);");
        System.out.println("    buf.putDouble(offset + 12, tick.bidPrice);");
        System.out.println("    buf.putDouble(offset + 20, tick.askPrice);");
        System.out.println("    buf.putInt(offset + 28, tick.bidSize);");
        System.out.println("    buf.putInt(offset + 32, tick.askSize);");
        System.out.println("}");

        System.out.println("\nMemory Usage Summary:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌──────────────────────┬──────────┬──────────────────────┐");
        System.out.println("│ Memory Type          │ GC?      │ Best For             │");
        System.out.println("├──────────────────────┼──────────┼──────────────────────┤");
        System.out.println("│ Stack                │ No       │ Local variables      │");
        System.out.println("│ Heap (Young)         │ Yes      │ Short-lived objects  │");
        System.out.println("│ Heap (Old)           │ Yes      │ Long-lived objects   │");
        System.out.println("│ Direct ByteBuffer    │ No       │ I/O buffers          │");
        System.out.println("│ Memory Mapped Files  │ No       │ Large files, IPC     │");
        System.out.println("│ Metaspace            │ No       │ Class metadata       │");
        System.out.println("└──────────────────────┴──────────┴──────────────────────┘");

        System.out.println("\nFinal Recommendations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("For <1ms latency (HFT execution):");
        System.out.println("  • ZGC with 16GB heap");
        System.out.println("  • 4GB direct memory for I/O");
        System.out.println("  • Object pooling for orders");
        System.out.println("  • Off-heap ring buffers");
        System.out.println("  • Memory mapped files for persistence");
        System.out.println();
        System.out.println("For <10ms latency (Order Management):");
        System.out.println("  • G1 GC with 8GB heap");
        System.out.println("  • 2GB direct memory");
        System.out.println("  • Standard Java objects OK");
        System.out.println();
        System.out.println("For Analytics/Reporting:");
        System.out.println("  • Parallel GC or G1");
        System.out.println("  • Larger heap (32GB+)");
        System.out.println("  • Optimize for throughput");
    }

    //=============================================================================
    // MAIN
    //=============================================================================

    public static void main(String[] args) {
        System.out.println("╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║                                                                ║");
        System.out.println("║         Java Memory Management - Complete Guide                ║");
        System.out.println("║          Ultra-Low Latency Trading Systems                     ║");
        System.out.println("║                                                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        demonstrateMemoryOverview();
        demonstrateStackMemory();
        demonstrateHeapMemory();
        demonstrateNativeMemory();
        demonstrateDirectByteBuffers();
        demonstrateMemoryMappedFiles();
        demonstrateMemoryPools();
        demonstrateMemoryLeaks();
        demonstrateMemoryTuning();
        demonstrateBestPractices();

        System.out.println("\n╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║  SUMMARY                                                       ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        System.out.println("Java Memory Areas:");
        System.out.println("┌──────────────────┬──────────┬──────────────────────────────┐");
        System.out.println("│ Memory Area      │ GC?      │ Use Case                     │");
        System.out.println("├──────────────────┼──────────┼──────────────────────────────┤");
        System.out.println("│ Stack            │ No       │ Method calls, local vars     │");
        System.out.println("│ Heap (Young)     │ Yes      │ New objects                  │");
        System.out.println("│ Heap (Old)       │ Yes      │ Long-lived objects           │");
        System.out.println("│ Metaspace        │ No       │ Class metadata               │");
        System.out.println("│ Direct Buffers   │ No       │ I/O, zero-copy               │");
        System.out.println("│ Memory Mapped    │ No       │ Files, IPC                   │");
        System.out.println("│ Code Cache       │ No       │ JIT compiled code            │");
        System.out.println("└──────────────────┴──────────┴──────────────────────────────┘");

        System.out.println("\nKey Takeaways:");
        System.out.println("1. Stack: Fast, automatic, per-thread");
        System.out.println("2. Heap: GC-managed, most objects live here");
        System.out.println("3. Off-Heap: No GC impact, manual management");
        System.out.println("4. Direct ByteBuffers: Best for I/O");
        System.out.println("5. Memory Mapped Files: Best for large files/IPC");
        System.out.println();
        System.out.println("For Ultra-Low Latency Trading:");
        System.out.println("✅ Use ZGC (sub-millisecond GC pauses)");
        System.out.println("✅ Use Direct ByteBuffers for I/O");
        System.out.println("✅ Use Object Pooling (zero allocation)");
        System.out.println("✅ Use Off-Heap for critical data");
        System.out.println("✅ Pre-allocate everything (-XX:+AlwaysPreTouch)");
        System.out.println();
        System.out.println("🚀 Proper memory management = predictable latency!");
    }
}

