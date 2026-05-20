/**
 * Java Garbage Collection Algorithms - Complete Guide
 *
 * Focus: Ultra-Low Latency Trading Systems
 *
 * Topics Covered:
 * 1. Serial GC
 * 2. Parallel GC (Throughput GC)
 * 3. CMS (Concurrent Mark Sweep) - Deprecated
 * 4. G1 GC (Garbage First) - Default since Java 9
 * 5. ZGC (Z Garbage Collector) - Ultra-Low Latency
 * 6. Shenandoah GC - Ultra-Low Latency
 * 7. Epsilon GC (No-Op GC)
 * 8. GC Comparison for Trading Systems
 * 9. GC Tuning for Ultra-Low Latency
 * 10. Best Practices for HFT
 *
 * Author: Java GC Complete Guide
 * Date: February 8, 2026
 */

public class Java_GC_Algorithms_Complete_Guide {

    //=============================================================================
    // 1. SERIAL GC
    //=============================================================================

    /**
     * Serial GC - Single-threaded garbage collector
     *
     * Use Case: Small heaps (<100MB), single-core systems
     * NOT for production trading systems!
     */
    static void demonstrateSerialGC() {
        System.out.println("╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  1. SERIAL GC                                              ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Overview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Single-threaded GC (uses only 1 CPU core)");
        System.out.println("• Stop-the-World (STW) for both Young and Old generation");
        System.out.println("• Simple and predictable");
        System.out.println("• Minimal memory overhead");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Young Generation: Copy algorithm (Eden + 2 Survivor spaces)");
        System.out.println("Old Generation:   Mark-Sweep-Compact algorithm");
        System.out.println("Both:             Complete STW pause");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UseSerialGC");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        Low (single-threaded)");
        System.out.println("Pause Time:        High (100ms - 1s+)");
        System.out.println("Latency:           Very High (predictable but long pauses)");
        System.out.println("Memory Overhead:   Very Low");
        System.out.println("CPU Usage:         Low (1 core)");

        System.out.println("\nBest For:");
        System.out.println("• Small applications (<100MB heap)");
        System.out.println("• Single-core systems");
        System.out.println("• Embedded systems");

        System.out.println("\n❌ NOT for Trading Systems:");
        System.out.println("• Long STW pauses (100ms+)");
        System.out.println("• Not scalable");
        System.out.println("• Single-threaded (wastes multi-core)");
    }

    //=============================================================================
    // 2. PARALLEL GC (THROUGHPUT GC)
    //=============================================================================

    /**
     * Parallel GC - Multi-threaded garbage collector
     *
     * Use Case: High throughput applications
     * Better than Serial but still has long pauses
     */
    static void demonstrateParallelGC() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  2. PARALLEL GC (Throughput GC)                            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Overview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Multi-threaded GC (uses multiple CPU cores)");
        System.out.println("• Stop-the-World for both Young and Old generation");
        System.out.println("• High throughput focus");
        System.out.println("• Default GC in Java 8 and earlier");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Young Generation: Parallel copying (multiple threads)");
        System.out.println("Old Generation:   Parallel Mark-Sweep-Compact");
        System.out.println("Both:             STW pause (but faster than Serial)");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UseParallelGC                    # Enable Parallel GC");
        System.out.println("-XX:ParallelGCThreads=<n>             # Number of GC threads");
        System.out.println("-XX:MaxGCPauseMillis=<n>              # Target max pause time");
        System.out.println("-XX:GCTimeRatio=<n>                   # Throughput goal");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        High (multi-threaded)");
        System.out.println("Pause Time:        Medium-High (10ms - 500ms)");
        System.out.println("Latency:           High (long pauses)");
        System.out.println("Memory Overhead:   Low");
        System.out.println("CPU Usage:         High during GC");

        System.out.println("\nBest For:");
        System.out.println("• Batch processing");
        System.out.println("• Scientific computing");
        System.out.println("• Applications prioritizing throughput over latency");

        System.out.println("\n⚠️ Marginal for Trading Systems:");
        System.out.println("• Still has significant STW pauses (10-500ms)");
        System.out.println("• Not suitable for ultra-low latency (<10ms)");
        System.out.println("• Good for reporting/analytics (not order execution)");
    }

    //=============================================================================
    // 3. CMS (CONCURRENT MARK SWEEP) - DEPRECATED
    //=============================================================================

    /**
     * CMS GC - Low-latency garbage collector (DEPRECATED in Java 9, removed in Java 14)
     *
     * Use Case: Low-latency applications (legacy)
     * Replaced by G1 and ZGC
     */
    static void demonstrateCMS() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  3. CMS (Concurrent Mark Sweep) - DEPRECATED               ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("⚠️ STATUS: DEPRECATED in Java 9, REMOVED in Java 14");
        System.out.println("Use G1, ZGC, or Shenandoah instead!");

        System.out.println("\nOverview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Concurrent GC (runs alongside application threads)");
        System.out.println("• Low-latency focus");
        System.out.println("• Does NOT compact Old Generation → fragmentation");
        System.out.println("• Can cause \"concurrent mode failure\" → Full GC");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Young Generation: ParNew (parallel, STW)");
        System.out.println("Old Generation:   Concurrent Mark-Sweep (mostly concurrent)");
        System.out.println("Phases:");
        System.out.println("  1. Initial Mark (STW, short ~10-50ms)");
        System.out.println("  2. Concurrent Mark (concurrent, no pause)");
        System.out.println("  3. Concurrent Preclean (concurrent, no pause)");
        System.out.println("  4. Remark (STW, short ~10-100ms)");
        System.out.println("  5. Concurrent Sweep (concurrent, no pause)");
        System.out.println("  6. Concurrent Reset (concurrent, no pause)");

        System.out.println("\nJVM Flags (Legacy):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UseConcMarkSweepGC               # Enable CMS");
        System.out.println("-XX:+UseParNewGC                      # ParNew for Young Gen");
        System.out.println("-XX:CMSInitiatingOccupancyFraction=70 # Start CMS at 70% full");
        System.out.println("-XX:+UseCMSInitiatingOccupancyOnly    # Don't auto-adjust");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        Medium (competes with app threads)");
        System.out.println("Pause Time:        Low (10-100ms for remark)");
        System.out.println("Latency:           Medium-Low");
        System.out.println("Memory Overhead:   High (floating garbage, fragmentation)");
        System.out.println("CPU Usage:         Medium (concurrent marking)");

        System.out.println("\nProblems:");
        System.out.println("• Fragmentation (no compaction) → Full GC");
        System.out.println("• Concurrent mode failure → Full GC (multi-second pause!)");
        System.out.println("• High memory overhead");
        System.out.println("• Unpredictable Full GC pauses");

        System.out.println("\n❌ DO NOT USE:");
        System.out.println("• Removed in Java 14+");
        System.out.println("• Unpredictable Full GC can cause multi-second pauses");
        System.out.println("• Use G1, ZGC, or Shenandoah instead");
    }

    //=============================================================================
    // 4. G1 GC (GARBAGE FIRST) - DEFAULT SINCE JAVA 9
    //=============================================================================

    /**
     * G1 GC - Low-latency garbage collector with predictable pauses
     *
     * Use Case: General-purpose low-latency applications
     * Default GC since Java 9
     */
    static void demonstrateG1GC() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  4. G1 GC (Garbage First) - DEFAULT SINCE JAVA 9           ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Overview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Region-based heap layout (2048 regions)");
        System.out.println("• Predictable pause times (configurable target)");
        System.out.println("• Generational and incremental");
        System.out.println("• Compacts heap (avoids fragmentation)");
        System.out.println("• Default GC in Java 9+");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Heap Layout: Divided into ~2048 equal-sized regions (1-32MB)");
        System.out.println("             Each region can be Eden, Survivor, or Old");
        System.out.println();
        System.out.println("Young GC:    STW, parallel, evacuates young regions");
        System.out.println("             Pause: 10-200ms (target configurable)");
        System.out.println();
        System.out.println("Mixed GC:    STW, collects Young + some Old regions");
        System.out.println("             Prioritizes regions with most garbage (Garbage First!)");
        System.out.println();
        System.out.println("Concurrent Marking Cycle:");
        System.out.println("  1. Initial Mark (STW, piggybacks on Young GC)");
        System.out.println("  2. Root Region Scan (concurrent)");
        System.out.println("  3. Concurrent Mark (concurrent)");
        System.out.println("  4. Remark (STW, short)");
        System.out.println("  5. Cleanup (STW + concurrent)");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UseG1GC                          # Enable G1 (default in Java 9+)");
        System.out.println("-XX:MaxGCPauseMillis=200              # Target max pause (default 200ms)");
        System.out.println("-XX:G1HeapRegionSize=<n>              # Region size (1-32MB)");
        System.out.println("-XX:InitiatingHeapOccupancyPercent=45 # Start concurrent mark at 45%");
        System.out.println("-XX:G1ReservePercent=10               # Reserve 10% for to-space");
        System.out.println("-XX:ConcGCThreads=<n>                 # Concurrent marking threads");
        System.out.println("-XX:ParallelGCThreads=<n>             # Parallel GC threads");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        High (good balance)");
        System.out.println("Pause Time:        Low-Medium (10-200ms, predictable)");
        System.out.println("Latency:           Medium (better than Parallel/CMS)");
        System.out.println("Memory Overhead:   Medium (region metadata, card tables)");
        System.out.println("CPU Usage:         Medium (concurrent marking)");

        System.out.println("\nBest For:");
        System.out.println("• Large heaps (>4GB)");
        System.out.println("• Predictable pause times");
        System.out.println("• General-purpose low-latency applications");
        System.out.println("• Microservices");

        System.out.println("\n⚠️ For Trading Systems:");
        System.out.println("✅ Good for: Order management, risk systems, reporting");
        System.out.println("⚠️  Not ideal for: Ultra-low latency execution (<10ms)");
        System.out.println("   Pauses still 10-200ms (too high for HFT)");

        System.out.println("\nTuning Example for Trading:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("java -Xms8g -Xmx8g \\");
        System.out.println("     -XX:+UseG1GC \\");
        System.out.println("     -XX:MaxGCPauseMillis=50 \\");
        System.out.println("     -XX:G1HeapRegionSize=16m \\");
        System.out.println("     -XX:InitiatingHeapOccupancyPercent=30 \\");
        System.out.println("     -XX:G1ReservePercent=15 \\");
        System.out.println("     TradingApp");
    }

    //=============================================================================
    // 5. ZGC (Z GARBAGE COLLECTOR) - ULTRA-LOW LATENCY
    //=============================================================================

    /**
     * ZGC - Scalable low-latency garbage collector
     *
     * Use Case: Ultra-low latency applications, large heaps
     * Target: <10ms pauses, even with multi-TB heaps!
     */
    static void demonstrateZGC() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  5. ZGC (Z Garbage Collector) - ULTRA-LOW LATENCY ⭐       ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Overview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Ultra-low latency GC (sub-millisecond pauses!)");
        System.out.println("• Scalable (8MB to 16TB heaps)");
        System.out.println("• Region-based, colored pointers, load barriers");
        System.out.println("• Concurrent compaction (almost everything concurrent)");
        System.out.println("• Production-ready since Java 15 (experimental in Java 11)");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Key Innovation: Colored Pointers (64-bit pointers with metadata)");
        System.out.println("  • Bits 0-41:  Object address (4TB addressable)");
        System.out.println("  • Bits 42-45: Metadata (marked, remapped, etc.)");
        System.out.println("  • Load barriers: Check/fix pointer on every object load");
        System.out.println();
        System.out.println("Phases (all mostly concurrent!):");
        System.out.println("  1. Pause Mark Start (STW, <1ms)");
        System.out.println("  2. Concurrent Mark");
        System.out.println("  3. Pause Mark End (STW, <1ms)");
        System.out.println("  4. Concurrent Prepare for Relocation");
        System.out.println("  5. Pause Relocate Start (STW, <1ms)");
        System.out.println("  6. Concurrent Relocate");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UseZGC                           # Enable ZGC");
        System.out.println("-XX:ZCollectionInterval=<seconds>     # Force GC interval");
        System.out.println("-XX:ZAllocationSpikeTolerance=<n>     # Allocation spike tolerance");
        System.out.println("-XX:ConcGCThreads=<n>                 # Concurrent threads");
        System.out.println("-XX:ParallelGCThreads=<n>             # Parallel threads (STW phases)");
        System.out.println();
        System.out.println("# Java 15+ (production-ready):");
        System.out.println("-XX:+UnlockExperimentalVMOptions      # Not needed in Java 15+");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        High (minimal impact, ~5-15% overhead)");
        System.out.println("Pause Time:        Ultra-Low (<1ms, typically <0.5ms!) ⭐");
        System.out.println("Latency:           Ultra-Low (sub-millisecond pauses)");
        System.out.println("Memory Overhead:   Medium-High (colored pointers, load barriers)");
        System.out.println("CPU Usage:         Medium (concurrent work)");

        System.out.println("\nPause Time Guarantees:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Max pause: <10ms (typically <1ms)");
        System.out.println("• Independent of heap size (works with 16TB heap!)");
        System.out.println("• Independent of live-set size");
        System.out.println("• Concurrent compaction (no fragmentation)");

        System.out.println("\nBest For:");
        System.out.println("• Ultra-low latency applications (HFT, real-time systems)");
        System.out.println("• Large heaps (multi-GB to multi-TB)");
        System.out.println("• Applications requiring consistent sub-10ms pauses");

        System.out.println("\n✅ EXCELLENT for Trading Systems:");
        System.out.println("• Sub-millisecond GC pauses (<1ms typical)");
        System.out.println("• Predictable latency (no long pauses)");
        System.out.println("• Scales to large heaps");
        System.out.println("• Production-ready (Java 15+)");

        System.out.println("\nTuning Example for HFT:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("java -Xms16g -Xmx16g \\");
        System.out.println("     -XX:+UseZGC \\");
        System.out.println("     -XX:ConcGCThreads=4 \\");
        System.out.println("     -XX:ParallelGCThreads=2 \\");
        System.out.println("     -XX:ZAllocationSpikeTolerance=2 \\");
        System.out.println("     -XX:+AlwaysPreTouch \\");
        System.out.println("     HFTTradingEngine");

        System.out.println("\nReal-World Results:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• 95th percentile pause: 0.2-0.5ms");
        System.out.println("• 99th percentile pause: 0.5-1ms");
        System.out.println("• Max pause: <10ms (even with TB heaps)");
        System.out.println("• Throughput overhead: 5-15%");
    }

    //=============================================================================
    // 6. SHENANDOAH GC - ULTRA-LOW LATENCY
    //=============================================================================

    /**
     * Shenandoah GC - Ultra-low latency garbage collector
     *
     * Use Case: Ultra-low latency applications
     * Target: <10ms pauses, similar to ZGC
     */
    static void demonstrateShenandoahGC() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  6. SHENANDOAH GC - ULTRA-LOW LATENCY ⭐                   ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Overview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Ultra-low latency GC (sub-10ms pauses)");
        System.out.println("• Region-based, concurrent compaction");
        System.out.println("• Brooks pointers (forwarding pointers)");
        System.out.println("• Production-ready since Java 12");
        System.out.println("• Developed by Red Hat");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Key Innovation: Brooks Pointers (indirection pointers)");
        System.out.println("  • Each object has a forwarding pointer");
        System.out.println("  • Allows concurrent relocation");
        System.out.println("  • Read/write barriers on object access");
        System.out.println();
        System.out.println("Phases (mostly concurrent!):");
        System.out.println("  1. Init Mark (STW, <1ms)");
        System.out.println("  2. Concurrent Marking");
        System.out.println("  3. Final Mark (STW, <10ms)");
        System.out.println("  4. Concurrent Cleanup");
        System.out.println("  5. Concurrent Evacuation (unique to Shenandoah!)");
        System.out.println("  6. Init Update Refs (STW, <1ms)");
        System.out.println("  7. Concurrent Update References");
        System.out.println("  8. Final Update Refs (STW, <10ms)");
        System.out.println("  9. Concurrent Cleanup");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UseShenandoahGC                  # Enable Shenandoah");
        System.out.println("-XX:ShenandoahGCHeuristics=<mode>     # adaptive, static, compact, aggressive");
        System.out.println("-XX:ConcGCThreads=<n>                 # Concurrent threads");
        System.out.println("-XX:ParallelGCThreads=<n>             # Parallel threads");
        System.out.println();
        System.out.println("# Heuristics modes:");
        System.out.println("#   adaptive:    Default, balances latency and throughput");
        System.out.println("#   static:      Fixed GC cycle interval");
        System.out.println("#   compact:     Aggressive compaction");
        System.out.println("#   aggressive:  Most aggressive, lowest latency");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        High (similar to ZGC)");
        System.out.println("Pause Time:        Ultra-Low (<10ms, typically 1-5ms) ⭐");
        System.out.println("Latency:           Ultra-Low");
        System.out.println("Memory Overhead:   Medium (Brooks pointers, barriers)");
        System.out.println("CPU Usage:         Medium (concurrent work)");

        System.out.println("\nPause Time Guarantees:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Max pause: <10ms (usually 1-5ms)");
        System.out.println("• Independent of heap size");
        System.out.println("• Concurrent evacuation (unique feature!)");

        System.out.println("\nShenandoah vs ZGC:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌─────────────────────┬──────────────────┬──────────────────┐");
        System.out.println("│ Feature             │ ZGC              │ Shenandoah       │");
        System.out.println("├─────────────────────┼──────────────────┼──────────────────┤");
        System.out.println("│ Max Pause           │ <1ms typical     │ 1-10ms typical   │");
        System.out.println("│ Technique           │ Colored pointers │ Brooks pointers  │");
        System.out.println("│ Barriers            │ Load barriers    │ Read/write       │");
        System.out.println("│ Max Heap Size       │ 16TB             │ ~1TB practical   │");
        System.out.println("│ Throughput overhead │ 5-15%            │ 10-20%           │");
        System.out.println("│ Memory overhead     │ Medium           │ Medium-High      │");
        System.out.println("│ Platform support    │ x64, aarch64     │ x64, aarch64     │");
        System.out.println("└─────────────────────┴──────────────────┴──────────────────┘");

        System.out.println("\nBest For:");
        System.out.println("• Ultra-low latency applications");
        System.out.println("• Heaps up to ~1TB");
        System.out.println("• Applications requiring consistent pauses");
        System.out.println("• OpenJDK distributions (widely available)");

        System.out.println("\n✅ EXCELLENT for Trading Systems:");
        System.out.println("• Sub-10ms GC pauses (typically 1-5ms)");
        System.out.println("• Predictable latency");
        System.out.println("• Concurrent evacuation (minimal impact)");
        System.out.println("• Production-ready");

        System.out.println("\nTuning Example for HFT:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("java -Xms16g -Xmx16g \\");
        System.out.println("     -XX:+UseShenandoahGC \\");
        System.out.println("     -XX:ShenandoahGCHeuristics=aggressive \\");
        System.out.println("     -XX:ConcGCThreads=4 \\");
        System.out.println("     -XX:ParallelGCThreads=2 \\");
        System.out.println("     -XX:+AlwaysPreTouch \\");
        System.out.println("     HFTTradingEngine");

        System.out.println("\nReal-World Results:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• 95th percentile pause: 1-3ms");
        System.out.println("• 99th percentile pause: 3-5ms");
        System.out.println("• Max pause: <10ms");
        System.out.println("• Throughput overhead: 10-20%");
    }

    //=============================================================================
    // 7. EPSILON GC (NO-OP GC)
    //=============================================================================

    /**
     * Epsilon GC - No-operation garbage collector
     *
     * Use Case: Performance testing, ultra-short lived applications
     * Does NOT collect garbage at all!
     */
    static void demonstrateEpsilonGC() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  7. EPSILON GC (No-Op GC)                                  ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Overview:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("• Does NOT collect garbage!");
        System.out.println("• Allocates memory until heap is full, then exits");
        System.out.println("• Zero GC overhead (no pauses at all)");
        System.out.println("• Introduced in Java 11");

        System.out.println("\nHow It Works:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Allocates objects in heap");
        System.out.println("2. Never reclaims memory");
        System.out.println("3. When heap is full → OutOfMemoryError");
        System.out.println("4. Process exits");

        System.out.println("\nJVM Flags:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("-XX:+UnlockExperimentalVMOptions");
        System.out.println("-XX:+UseEpsilonGC");

        System.out.println("\nPerformance Characteristics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Throughput:        Maximum (no GC overhead)");
        System.out.println("Pause Time:        Zero (no GC pauses)");
        System.out.println("Latency:           Minimum (no GC)");
        System.out.println("Memory Overhead:   Zero (no GC structures)");
        System.out.println("CPU Usage:         Zero for GC");

        System.out.println("\nBest For:");
        System.out.println("• Performance testing (measure GC overhead)");
        System.out.println("• Ultra-short lived applications");
        System.out.println("• Applications with predictable/bounded memory");
        System.out.println("• GC sensitivity analysis");

        System.out.println("\n⚠️ For Trading Systems:");
        System.out.println("✅ Possible for: Ultra-short microbursts (<1 second)");
        System.out.println("                Market data snapshots");
        System.out.println("                Performance benchmarking");
        System.out.println("❌ Not suitable: Long-running processes");
        System.out.println("                 Continuous trading engines");

        System.out.println("\nUse Case Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Measure GC overhead of your application:");
        System.out.println("// 1. Run with Epsilon: no GC overhead, will OOM if runs too long");
        System.out.println("// 2. Run with ZGC: measure difference in throughput");
        System.out.println("// 3. Difference = GC overhead");
        System.out.println();
        System.out.println("java -Xms8g -Xmx8g \\");
        System.out.println("     -XX:+UnlockExperimentalVMOptions \\");
        System.out.println("     -XX:+UseEpsilonGC \\");
        System.out.println("     PerformanceTest");
    }

    //=============================================================================
    // 8. GC COMPARISON FOR TRADING SYSTEMS
    //=============================================================================

    static void compareGCForTrading() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  8. GC COMPARISON FOR TRADING SYSTEMS                      ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("Complete Comparison Table:");
        System.out.println("─────────────────────────────────────────────────────────────────────────");
        System.out.println("┌──────────────┬──────────┬──────────┬──────────┬──────────┬─────────────┐");
        System.out.println("│ GC           │ Max Pause│ Typical  │ Overhead │ Max Heap │ Trading Use │");
        System.out.println("├──────────────┼──────────┼──────────┼──────────┼──────────┼─────────────┤");
        System.out.println("│ Serial       │ 1s+      │ 100-500ms│ Very Low │ <100MB   │ ❌ Never    │");
        System.out.println("│ Parallel     │ 500ms    │ 10-200ms │ Low      │ <100GB   │ ⚠️ Analytics│");
        System.out.println("│ CMS          │ 5s+      │ 10-100ms │ High     │ <64GB    │ ❌ Removed  │");
        System.out.println("│ G1           │ 200ms    │ 10-100ms │ Medium   │ <100GB   │ ⚠️ Backoffice│");
        System.out.println("│ ZGC          │ <1ms ⭐  │ 0.2-0.5ms│ 5-15%    │ 16TB     │ ✅ HFT      │");
        System.out.println("│ Shenandoah   │ <10ms ⭐ │ 1-5ms    │ 10-20%   │ ~1TB     │ ✅ HFT      │");
        System.out.println("│ Epsilon      │ 0ms ⭐   │ 0ms      │ 0%       │ Any      │ ⚠️ Special  │");
        System.out.println("└──────────────┴──────────┴──────────┴──────────┴──────────┴─────────────┘");

        System.out.println("\nLatency Requirements vs GC Choice:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("┌─────────────────────────┬────────────────────────────────┐");
        System.out.println("│ Latency Requirement     │ Recommended GC                 │");
        System.out.println("├─────────────────────────┼────────────────────────────────┤");
        System.out.println("│ <1ms (Ultra HFT)        │ ZGC ⭐ (best choice)           │");
        System.out.println("│ <10ms (HFT)             │ ZGC ⭐ or Shenandoah ⭐        │");
        System.out.println("│ <100ms (Low-latency)    │ G1 (good balance)              │");
        System.out.println("│ <1s (Medium-latency)    │ G1 or Parallel                 │");
        System.out.println("│ Throughput priority     │ Parallel                       │");
        System.out.println("│ No GC (short-lived)     │ Epsilon (special cases)        │");
        System.out.println("└─────────────────────────┴────────────────────────────────┘");

        System.out.println("\nTrading System Components:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Component              Latency Req    Recommended GC");
        System.out.println("──────────────────────────────────────────────────────────");
        System.out.println("Order Execution        <1ms           ZGC ⭐");
        System.out.println("Market Making          <1ms           ZGC ⭐");
        System.out.println("Market Data Feed       <5ms           ZGC or Shenandoah ⭐");
        System.out.println("Order Gateway          <10ms          ZGC or Shenandoah");
        System.out.println("Risk Management        <50ms          G1");
        System.out.println("Order Management       <100ms         G1");
        System.out.println("Position Management    <500ms         G1");
        System.out.println("Reporting/Analytics    <5s            Parallel or G1");
        System.out.println("Batch Processing       No limit       Parallel");

        System.out.println("\nHeap Size Recommendations:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Application Type       Heap Size      GC Choice");
        System.out.println("──────────────────────────────────────────────────────────");
        System.out.println("Microservice           <4GB           G1");
        System.out.println("Small Trading App      4-8GB          G1 or ZGC");
        System.out.println("Medium Trading App     8-32GB         ZGC ⭐");
        System.out.println("Large Trading Platform 32-128GB       ZGC ⭐");
        System.out.println("Massive System         >128GB         ZGC ⭐ (up to 16TB)");

        System.out.println("\nDecision Tree:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("Need <1ms pauses?");
        System.out.println("  └─ YES → ZGC ⭐ (best choice for HFT)");
        System.out.println("  └─ NO  → Need <10ms pauses?");
        System.out.println("           └─ YES → ZGC ⭐ or Shenandoah ⭐");
        System.out.println("           └─ NO  → Need <100ms pauses?");
        System.out.println("                    └─ YES → G1 (good balance)");
        System.out.println("                    └─ NO  → Parallel (throughput focus)");
    }

    //=============================================================================
    // 9. GC TUNING FOR ULTRA-LOW LATENCY
    //=============================================================================

    static void demonstrateGCTuning() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  9. GC TUNING FOR ULTRA-LOW LATENCY                        ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("ZGC Tuning for HFT (Recommended ⭐):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Basic setup");
        System.out.println("java -Xms16g -Xmx16g \\              # Fixed heap (no resizing)");
        System.out.println("     -XX:+UseZGC \\                  # Enable ZGC");
        System.out.println("     -XX:+AlwaysPreTouch \\          # Pre-touch all memory pages");
        System.out.println("     -XX:+UseLargePages \\           # Use huge pages (2MB)");
        System.out.println("     -XX:+UnlockDiagnosticVMOptions \\");
        System.out.println("     -XX:-ZProactive \\              # Disable proactive GC");
        System.out.println("     -XX:ConcGCThreads=4 \\          # Concurrent GC threads");
        System.out.println("     -XX:ParallelGCThreads=2 \\      # Parallel GC threads (STW)");
        System.out.println("     TradingEngine");

        System.out.println("\nShenandoah Tuning for HFT:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("java -Xms16g -Xmx16g \\");
        System.out.println("     -XX:+UseShenandoahGC \\");
        System.out.println("     -XX:ShenandoahGCHeuristics=aggressive \\  # Most aggressive");
        System.out.println("     -XX:+AlwaysPreTouch \\");
        System.out.println("     -XX:+UseLargePages \\");
        System.out.println("     -XX:ConcGCThreads=4 \\");
        System.out.println("     -XX:ParallelGCThreads=2 \\");
        System.out.println("     TradingEngine");

        System.out.println("\nG1 Tuning for Medium-Latency:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("java -Xms8g -Xmx8g \\");
        System.out.println("     -XX:+UseG1GC \\");
        System.out.println("     -XX:MaxGCPauseMillis=50 \\      # Target 50ms max pause");
        System.out.println("     -XX:G1HeapRegionSize=16m \\     # Larger regions");
        System.out.println("     -XX:InitiatingHeapOccupancyPercent=30 \\");
        System.out.println("     -XX:G1ReservePercent=15 \\");
        System.out.println("     -XX:+AlwaysPreTouch \\");
        System.out.println("     OrderManagementSystem");

        System.out.println("\nCritical JVM Flags for Trading:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Memory");
        System.out.println("-Xms and -Xmx            # Set to SAME value (no resizing)");
        System.out.println("-XX:+AlwaysPreTouch      # Pre-allocate all memory");
        System.out.println("-XX:+UseLargePages       # Use huge pages (2MB on Linux)");
        System.out.println();
        System.out.println("# CPU affinity");
        System.out.println("-XX:+UseNUMA             # NUMA-aware allocation");
        System.out.println();
        System.out.println("# GC logging (for analysis)");
        System.out.println("-Xlog:gc*:file=gc.log:time,uptime,level,tags");
        System.out.println();
        System.out.println("# Disable biased locking (can cause pauses)");
        System.out.println("-XX:-UseBiasedLocking");

        System.out.println("\nOperating System Tuning:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Linux huge pages");
        System.out.println("echo 8192 > /proc/sys/vm/nr_hugepages  # 16GB of 2MB pages");
        System.out.println();
        System.out.println("# Disable transparent huge pages (can cause pauses)");
        System.out.println("echo never > /sys/kernel/mm/transparent_hugepage/enabled");
        System.out.println();
        System.out.println("# CPU isolation (dedicate cores to JVM)");
        System.out.println("isolcpus=2-7            # In kernel boot parameters");
        System.out.println("taskset -c 2-7 java ... # Pin JVM to isolated cores");
        System.out.println();
        System.out.println("# Disable swap");
        System.out.println("swapoff -a");

        System.out.println("\nMonitoring and Diagnostics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("# Enable GC logging");
        System.out.println("-Xlog:gc*=info:file=gc.log:time,level,tags");
        System.out.println();
        System.out.println("# JFR (Java Flight Recorder)");
        System.out.println("-XX:StartFlightRecording=duration=60s,filename=recording.jfr");
        System.out.println();
        System.out.println("# Print GC details");
        System.out.println("-XX:+PrintGCDetails");
        System.out.println("-XX:+PrintGCDateStamps");
    }

    //=============================================================================
    // 10. BEST PRACTICES FOR HFT
    //=============================================================================

    static void demonstrateBestPractices() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  10. BEST PRACTICES FOR HFT                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("✅ DO:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Use ZGC for ultra-low latency (<1ms pauses)");
        System.out.println("2. Set -Xms = -Xmx (prevent heap resizing)");
        System.out.println("3. Enable -XX:+AlwaysPreTouch (pre-allocate memory)");
        System.out.println("4. Use huge pages (-XX:+UseLargePages)");
        System.out.println("5. Disable biased locking (-XX:-UseBiasedLocking)");
        System.out.println("6. Size heap appropriately (enough for working set)");
        System.out.println("7. Use object pools for hot path objects");
        System.out.println("8. Minimize allocation rate in critical path");
        System.out.println("9. Monitor GC logs continuously");
        System.out.println("10. Test under realistic load");

        System.out.println("\n❌ DON'T:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("1. Don't use Serial or Parallel GC for HFT");
        System.out.println("2. Don't use CMS (removed in Java 14)");
        System.out.println("3. Don't let heap size auto-adjust");
        System.out.println("4. Don't allocate heavily in hot path");
        System.out.println("5. Don't use finalize() (unpredictable)");
        System.out.println("6. Don't use soft/weak references excessively");
        System.out.println("7. Don't run GC explicitly (System.gc())");
        System.out.println("8. Don't enable unnecessary JVM features");

        System.out.println("\nCode Best Practices:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// ✅ GOOD: Object pooling");
        System.out.println("private final ObjectPool<Order> orderPool = new ObjectPool<>();");
        System.out.println("Order order = orderPool.acquire();  // Reuse");
        System.out.println("// ... use order ...");
        System.out.println("orderPool.release(order);");
        System.out.println();
        System.out.println("// ❌ BAD: Allocation in hot path");
        System.out.println("for (int i = 0; i < 1000000; i++) {");
        System.out.println("    Order order = new Order();  // Creates 1M objects!");
        System.out.println("    process(order);");
        System.out.println("}");
        System.out.println();
        System.out.println("// ✅ GOOD: Pre-size collections");
        System.out.println("List<Order> orders = new ArrayList<>(10000);  // Known size");
        System.out.println();
        System.out.println("// ❌ BAD: Growing collections");
        System.out.println("List<Order> orders = new ArrayList<>();  // Starts at 10, grows");

        System.out.println("\nComplete HFT Setup Example:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("#!/bin/bash");
        System.out.println("# HFT Trading Engine Startup Script");
        System.out.println();
        System.out.println("# OS setup");
        System.out.println("echo never > /sys/kernel/mm/transparent_hugepage/enabled");
        System.out.println("swapoff -a");
        System.out.println();
        System.out.println("# Run trading engine");
        System.out.println("taskset -c 2-7 java \\");
        System.out.println("  -Xms16g -Xmx16g \\");
        System.out.println("  -XX:+UseZGC \\");
        System.out.println("  -XX:+AlwaysPreTouch \\");
        System.out.println("  -XX:+UseLargePages \\");
        System.out.println("  -XX:-UseBiasedLocking \\");
        System.out.println("  -XX:+UseNUMA \\");
        System.out.println("  -XX:ConcGCThreads=4 \\");
        System.out.println("  -XX:ParallelGCThreads=2 \\");
        System.out.println("  -XX:+UnlockDiagnosticVMOptions \\");
        System.out.println("  -XX:-ZProactive \\");
        System.out.println("  -Xlog:gc*:file=gc.log:time,level,tags \\");
        System.out.println("  -XX:StartFlightRecording=duration=60s,filename=hft.jfr \\");
        System.out.println("  -cp trading.jar \\");
        System.out.println("  com.trading.HFTEngine");

        System.out.println("\nExpected Results:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("With ZGC tuning:");
        System.out.println("• P50 latency: 0.1-0.3ms");
        System.out.println("• P95 latency: 0.2-0.5ms");
        System.out.println("• P99 latency: 0.5-1ms");
        System.out.println("• P99.9 latency: <10ms");
        System.out.println("• Max GC pause: <1ms");
        System.out.println("• Throughput overhead: 5-15%");
    }

    //=============================================================================
    // MAIN
    //=============================================================================

    public static void main(String[] args) {
        System.out.println("╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║                                                                ║");
        System.out.println("║          Java Garbage Collection Algorithms Guide              ║");
        System.out.println("║              Ultra-Low Latency Trading Systems                 ║");
        System.out.println("║                                                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        demonstrateSerialGC();
        demonstrateParallelGC();
        demonstrateCMS();
        demonstrateG1GC();
        demonstrateZGC();
        demonstrateShenandoahGC();
        demonstrateEpsilonGC();
        compareGCForTrading();
        demonstrateGCTuning();
        demonstrateBestPractices();

        System.out.println("\n╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║  SUMMARY                                                       ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        System.out.println("GC Evolution:");
        System.out.println("• Java 1-8:  Serial, Parallel, CMS (high pauses)");
        System.out.println("• Java 9+:   G1 (default, low-medium pauses)");
        System.out.println("• Java 11+:  ZGC, Shenandoah (ultra-low pauses) ⭐");

        System.out.println("\nFor Ultra-Low Latency Trading:");
        System.out.println("┌──────────────────────┬─────────────────────────────┐");
        System.out.println("│ Latency Requirement  │ Best GC Choice              │");
        System.out.println("├──────────────────────┼─────────────────────────────┤");
        System.out.println("│ <1ms (Ultra HFT)     │ ZGC ⭐ (0.2-0.5ms pauses)   │");
        System.out.println("│ <10ms (HFT)          │ ZGC ⭐ or Shenandoah ⭐     │");
        System.out.println("│ <100ms (Low-latency) │ G1 (good balance)           │");
        System.out.println("│ Throughput priority  │ Parallel                    │");
        System.out.println("└──────────────────────┴─────────────────────────────┘");

        System.out.println("\nKey Recommendations:");
        System.out.println("1. ⭐ ZGC for HFT: <1ms pauses, up to 16TB heap");
        System.out.println("2. ⭐ Shenandoah alternative: <10ms pauses, up to 1TB");
        System.out.println("3. Always: -Xms = -Xmx, +AlwaysPreTouch, +UseLargePages");
        System.out.println("4. Monitor: GC logs, JFR, latency percentiles");
        System.out.println("5. Optimize: Object pooling, minimize allocations");

        System.out.println("\n🚀 ZGC is the clear winner for ultra-low latency trading!");
    }
}

