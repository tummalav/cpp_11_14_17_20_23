/**
 * ============================================================================
 * Java Heap Analysis Techniques on RHEL — Complete Guide
 * ============================================================================
 *
 * WHEN YOU NEED HEAP ANALYSIS:
 *   - OutOfMemoryError (OOM) — heap exhausted
 *   - GC pause too long — too many live objects
 *   - Memory leak — objects accumulating over time
 *   - High GC frequency — allocation rate too high
 *   - Unexpected footprint — heap larger than expected
 *
 * TOOLS COVERED:
 *   1.  JVM Flags              — built-in heap monitoring at launch
 *   2.  jcmd                   — Swiss army knife (RHEL: ships with JDK)
 *   3.  jmap                   — heap dump + histogram
 *   4.  jstat                  — live GC/heap statistics
 *   5.  jinfo                  — JVM flags of running process
 *   6.  Programmatic Heap API  — MXBean inside your Java code
 *   7.  GC Log Analysis        — decode GC log files
 *   8.  Eclipse MAT            — deep heap dump analysis
 *   9.  async-profiler         — allocation profiling, no safepoint bias
 *   10. jfr / JDK Flight Recorder — continuous production profiling
 *   11. OOM Heap Dump on Crash  — auto-dump on OOM
 *   12. cgroups / container     — heap in containerised RHEL
 *   13. Trading system patterns — what to look for in ULL systems
 *
 * Build:
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/javac \
 *       Java_Heap_Analysis_RHEL.java
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/java \
 *       -Xmx256m Java_Heap_Analysis_RHEL
 * ============================================================================
 */

import java.lang.management.*;
import java.lang.ref.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.io.*;
import java.nio.file.*;
import java.time.*;
import java.time.format.*;

public class Java_Heap_Analysis_RHEL {

    // =========================================================================
    // SECTION 1: JVM LAUNCH FLAGS — Enable Heap Monitoring From the Start
    // =========================================================================
    static void printJVMLaunchFlags() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         SECTION 1: JVM Launch Flags for Heap Analysis on RHEL
        ══════════════════════════════════════════════════════════════════════

        ─── Basic Heap Sizing ────────────────────────────────────────────────
        java \\
            -Xms4g                    # initial heap size (set = Xmx to avoid resize)
            -Xmx4g                    # max heap size
            -Xss512k                  # thread stack size (default 512k-1m)
            -XX:MetaspaceSize=256m    # initial metaspace (class metadata)
            -XX:MaxMetaspaceSize=512m # cap metaspace (prevent creep)
            -jar app.jar

        ─── GC Selection (RHEL 8/9, Java 17+) ──────────────────────────────
        # ZGC  — sub-millisecond pauses, best for ULL trading
        java -XX:+UseZGC -XX:+ZGenerational -Xmx8g -jar app.jar

        # Shenandoah — concurrent GC, similar to ZGC
        java -XX:+UseShenandoahGC -Xmx8g -jar app.jar

        # G1GC — default, good for large heaps
        java -XX:+UseG1GC -XX:MaxGCPauseMillis=50 -Xmx8g -jar app.jar

        # ParallelGC — throughput (not low-latency)
        java -XX:+UseParallelGC -Xmx8g -jar app.jar

        ─── GC Logging (RHEL: write to /var/log/trading/) ───────────────────
        java \\
            -Xlog:gc*:file=/var/log/trading/gc.log:time,uptime,pid:filecount=10,filesize=50m \\
            -Xlog:gc+heap=debug \\
            -Xlog:safepoint \\
            -jar app.jar

        # What each flag logs:
        # gc*            → all GC events (pause, concurrent phase, region)
        # gc+heap=debug  → heap before/after each GC (used/committed/max)
        # safepoint      → STW safepoint enters (high = GC pressure)
        # file=...       → rolling log files (filecount=10, filesize=50m each)

        ─── Heap Dump on OOM (CRITICAL — always enable in production) ───────
        java \\
            -XX:+HeapDumpOnOutOfMemoryError \\
            -XX:HeapDumpPath=/var/dumps/heap_$(date +%Y%m%d_%H%M%S).hprof \\
            -XX:+ExitOnOutOfMemoryError \\
            -jar app.jar

        # HeapDumpOnOutOfMemoryError → write .hprof file when OOM occurs
        # HeapDumpPath               → where to write it (needs write permission)
        # ExitOnOutOfMemoryError     → exit JVM immediately (clean restart)

        ─── Full ULL Trading JVM Command (RHEL production) ──────────────────
        numactl --cpunodebind=0 --membind=0 \\
        taskset -c 4,5,6,7 \\
        java \\
            -server \\
            -Xms8g -Xmx8g \\
            -XX:+UseZGC -XX:+ZGenerational \\
            -XX:ConcGCThreads=2 \\
            -XX:+AlwaysPreTouch \\
            -XX:+UseLargePages \\
            -XX:+DisableExplicitGC \\
            -XX:+HeapDumpOnOutOfMemoryError \\
            -XX:HeapDumpPath=/var/dumps/ \\
            -XX:+ExitOnOutOfMemoryError \\
            -Xlog:gc*:file=/var/log/trading/gc.log:time,uptime:filecount=10,filesize=50m \\
            -Xlog:safepoint \\
            -jar /opt/trading/app.jar
        """);
    }

    // =========================================================================
    // SECTION 2: jcmd — Swiss Army Knife (Best Tool on RHEL)
    // =========================================================================
    static void printJcmdCommands() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         SECTION 2: jcmd — Best All-Round Tool on RHEL
        ══════════════════════════════════════════════════════════════════════

        # Find your JVM PID:
        jcmd                           # lists all JVM processes
        jcmd | grep trading            # filter by app name
        pgrep -f trading-app.jar       # or use pgrep

        # List all available commands for a PID:
        jcmd <PID> help

        ─── Heap Info ────────────────────────────────────────────────────────
        # Heap summary (quick snapshot):
        jcmd <PID> GC.heap_info

        # Sample output:
        # ZHeap  used 1024M, capacity 2048M, max capacity 8192M
        # Metaspace  used 64M, committed 65M, reserved 1088M

        # Full heap histogram (top object types by count & size):
        jcmd <PID> GC.class_histogram | head -50

        # Sample output:
        #  num     #instances         #bytes  class name
        # ─────────────────────────────────────────────────
        #    1:       5000000      120000000  [B  (byte arrays)
        #    2:       1000000       32000000  java.lang.String
        #    3:        500000       24000000  com.trading.Order
        #    4:        200000        9600000  java.util.HashMap$Node

        ─── Heap Dump ────────────────────────────────────────────────────────
        # Take heap dump (live objects only — smaller, faster):
        jcmd <PID> GC.heap_dump /var/dumps/heap_live.hprof

        # Include all objects (including unreachable):
        jcmd <PID> GC.heap_dump -all /var/dumps/heap_all.hprof

        # Note: heap dump triggers a Stop-The-World pause!
        # On 8GB heap: ~30-60 seconds pause. Do NOT do this on production
        # unless you have a hot standby taking traffic.

        ─── GC Control ───────────────────────────────────────────────────────
        # Force a GC (for testing/baseline — NOT on production hot path):
        jcmd <PID> GC.run

        # Show GC settings:
        jcmd <PID> GC.run_finalization

        ─── Thread Info ──────────────────────────────────────────────────────
        # Thread dump (all threads + stack traces):
        jcmd <PID> Thread.print
        jcmd <PID> Thread.print -l   # with locks

        # Count threads by state:
        jcmd <PID> Thread.print | grep "java.lang.Thread.State" | sort | uniq -c

        ─── JVM Info ─────────────────────────────────────────────────────────
        # All JVM flags (set and defaults):
        jcmd <PID> VM.flags

        # System properties:
        jcmd <PID> VM.system_properties

        # Command line that started the JVM:
        jcmd <PID> VM.command_line

        # JVM uptime:
        jcmd <PID> VM.uptime

        # Native memory summary (requires -XX:NativeMemoryTracking=summary):
        jcmd <PID> VM.native_memory summary

        # Sample NMT output:
        # Total: reserved=10GB, committed=8GB
        # Java Heap: reserved=8192MB, committed=8192MB
        # Class:     reserved=1088MB, committed=65MB
        # Thread:    reserved=2048MB, committed=32MB  (4096 threads × 512k)
        # Code:      reserved=256MB,  committed=64MB   (JIT compiled code)

        ─── JFR (Flight Recorder) via jcmd ──────────────────────────────────
        # Start a 60-second JFR recording:
        jcmd <PID> JFR.start name=heap_analysis duration=60s filename=/tmp/recording.jfr

        # Check recording status:
        jcmd <PID> JFR.check

        # Stop and dump:
        jcmd <PID> JFR.stop name=heap_analysis filename=/tmp/recording.jfr

        # Open in JDK Mission Control (JMC):
        jmc /tmp/recording.jfr
        """);
    }

    // =========================================================================
    // SECTION 3: jmap — Heap Dump and Histogram
    // =========================================================================
    static void printJmapCommands() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         SECTION 3: jmap — Heap Dump and Object Histogram
        ══════════════════════════════════════════════════════════════════════

        NOTE: On RHEL, jmap may require ptrace permission:
          echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
          OR run jmap as same user as JVM process

        ─── Heap Histogram ───────────────────────────────────────────────────
        # Quick histogram (does NOT trigger full GC):
        jmap -histo <PID> | head -30

        # Live objects only (triggers GC first — heavier):
        jmap -histo:live <PID> | head -30

        # Output format:
        # num     #instances   #bytes   class name
        #   1:      5000000  120000000  [B          ← byte arrays
        #   2:       500000   24000000  com.trading.Order
        #   3:       200000   12800000  java.util.HashMap$Node
        #   Total:  6200000  168800000

        # Common class name abbreviations:
        # [B  = byte[]       [C  = char[]      [I  = int[]
        # [J  = long[]       [D  = double[]    [Z  = boolean[]
        # [L  = Object[]     [[B = byte[][]    (2D arrays)

        ─── Heap Dump ────────────────────────────────────────────────────────
        # Dump only live objects (smaller file, faster analysis):
        jmap -dump:live,format=b,file=/var/dumps/heap.hprof <PID>

        # Dump all objects including garbage:
        jmap -dump:format=b,file=/var/dumps/heap_all.hprof <PID>

        # GZip compress during dump (saves disk):
        jmap -dump:live,format=b,file=/var/dumps/heap.hprof.gz <PID>
        # Note: not all JVM versions support gz directly, use: gzip heap.hprof

        # File size estimate:
        # Heap dump ≈ 50-70% of heap used size
        # 8GB used heap → ~5GB .hprof file

        ─── Heap configuration ───────────────────────────────────────────────
        # Show heap configuration:
        jmap -heap <PID>

        # Sample output (ZGC):
        # Heap Configuration:
        #   MinHeapFreeRatio = 40
        #   MaxHeapFreeRatio = 70
        #   MaxHeapSize      = 8589934592 (8.0GB)
        #   NewSize          = 1363144 (1.3MB)  ← ZGC: generational
        #   OldSize          = 5452592 (5.2MB)
        #
        # Heap Usage:
        #   ZGC Heap:
        #    capacity = 2147483648 (2.0GB)   ← current committed
        #    used     = 1073741824 (1.0GB)   ← current used

        ─── Remote heap dump via SSH ─────────────────────────────────────────
        # If you're on a different machine:
        ssh prod-server "jmap -dump:live,format=b,file=/tmp/heap.hprof $(pgrep -f trading-app)"
        scp prod-server:/tmp/heap.hprof ./analysis/

        # Or stream directly:
        ssh prod-server "jmap -dump:live,format=b,file=/dev/stdout \\$(pgrep -f trading-app)" \\
            > ./analysis/heap.hprof
        """);
    }

    // =========================================================================
    // SECTION 4: jstat — Live GC Statistics (Best for Continuous Monitoring)
    // =========================================================================
    static void printJstatCommands() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         SECTION 4: jstat — Live GC Statistics Every N Milliseconds
        ══════════════════════════════════════════════════════════════════════

        # Format: jstat -<option> <PID> <interval_ms> <count>

        ─── GC Summary (most useful) ─────────────────────────────────────────
        # Print GC stats every 1 second, 100 times:
        jstat -gcutil <PID> 1000 100

        # Output columns:
        # S0   S1   E    O    M    CCS  YGC  YGCT  FGC  FGCT  CGC  CGCT  GCT
        # 0.0  0.0  45.2 72.1 95.3 90.1  523  2.341   2  0.800   0  0.000  3.141
        #
        # S0/S1  = Survivor 0/1 utilisation % (not relevant for ZGC/G1)
        # E      = Eden utilisation %
        # O      = Old gen utilisation %
        # M      = Metaspace utilisation %
        # YGC    = Young GC count (total since start)
        # YGCT   = Young GC cumulative time (seconds)
        # FGC    = Full GC count (should be ZERO in production)
        # FGCT   = Full GC time
        # GCT    = Total GC time

        # Alert levels:
        # O > 85%              → old gen pressure, OOM risk
        # FGC > 0              → Full GC happened (bad for latency)
        # GCT growing fast     → GC overhead too high
        # YGC rate > 10/sec    → allocation rate too high

        ─── Heap Capacity ───────────────────────────────────────────────────
        jstat -gccapacity <PID> 1000

        # Columns: NGCMN NGCMX NGC S0C S1C EC OGCMN OGCMX OGC OC MCMN MCMX MC
        # (min/max/current capacity of each generation in KB)

        ─── Allocation Rate (critical for ULL) ──────────────────────────────
        jstat -gcnew <PID> 1000

        # Watch the 'EU' (Eden Used) column — if it grows rapidly:
        # allocation rate = (EU_now - EU_prev) / interval

        # Script to compute allocation rate:
        prev=0
        while true; do
            eu=$(jstat -gcnew <PID> 1 1 | tail -1 | awk '{print $5}')
            rate=$((eu - prev))
            echo "$(date +%T) Eden used: ${eu}KB, delta: ${rate}KB/sec"
            prev=$eu
            sleep 1
        done

        ─── Class Loading ───────────────────────────────────────────────────
        jstat -class <PID> 1000
        # Columns: Loaded Bytes Unloaded Bytes Time
        # If Loaded keeps growing → classloader leak

        ─── Compiler (JIT) ──────────────────────────────────────────────────
        jstat -compiler <PID>
        # Compiled Failed Invalid Time FailedType FailedMethod
        # Shows JIT compilation activity

        ─── Watch all GC events in real-time ────────────────────────────────
        # Using -verbose:gc flag on running JVM:
        jcmd <PID> VM.flags | grep PrintGC

        # Or tail the GC log:
        tail -f /var/log/trading/gc.log | grep -E "Pause|GC|ms"
        """);
    }

    // =========================================================================
    // SECTION 5: Programmatic Heap Analysis — MXBean in Your Code
    // =========================================================================
    static class ProgrammaticHeapAnalysis {

        // ── 1. MemoryMXBean — basic heap info ───────────────────────────────
        static void memoryMXBeanDemo() {
            System.out.println("\n=== Programmatic: MemoryMXBean ===");

            MemoryMXBean memBean = ManagementFactory.getMemoryMXBean();

            MemoryUsage heap    = memBean.getHeapMemoryUsage();
            MemoryUsage nonHeap = memBean.getNonHeapMemoryUsage();

            System.out.printf("  Heap:     init=%,dMB  used=%,dMB  committed=%,dMB  max=%,dMB%n",
                heap.getInit()      / (1024*1024),
                heap.getUsed()      / (1024*1024),
                heap.getCommitted() / (1024*1024),
                heap.getMax()       / (1024*1024));

            System.out.printf("  NonHeap:  used=%,dMB  committed=%,dMB%n",
                nonHeap.getUsed()      / (1024*1024),
                nonHeap.getCommitted() / (1024*1024));

            double heapUsedPct = 100.0 * heap.getUsed() / heap.getMax();
            System.out.printf("  Heap utilisation: %.1f%%%n", heapUsedPct);

            if (heapUsedPct > 80)
                System.out.println("  ⚠️  WARNING: Heap > 80% — GC pressure high!");
        }

        // ── 2. MemoryPoolMXBean — per-generation breakdown ──────────────────
        static void memoryPoolDemo() {
            System.out.println("\n=== Programmatic: MemoryPoolMXBean (per generation) ===");

            List<MemoryPoolMXBean> pools = ManagementFactory.getMemoryPoolMXBeans();
            for (MemoryPoolMXBean pool : pools) {
                MemoryUsage usage = pool.getUsage();
                if (usage.getMax() > 0) {
                    double pct = 100.0 * usage.getUsed() / usage.getMax();
                    System.out.printf("  %-35s used=%,7dMB  max=%,7dMB  (%.1f%%)%n",
                        pool.getName(),
                        usage.getUsed()      / (1024*1024),
                        usage.getMax()       / (1024*1024),
                        pct);
                } else {
                    System.out.printf("  %-35s used=%,7dMB  (no max set)%n",
                        pool.getName(),
                        usage.getUsed() / (1024*1024));
                }
            }
        }

        // ── 3. GarbageCollectorMXBean — GC stats ────────────────────────────
        static void gcMXBeanDemo() {
            System.out.println("\n=== Programmatic: GarbageCollectorMXBean ===");

            List<GarbageCollectorMXBean> gcBeans =
                ManagementFactory.getGarbageCollectorMXBeans();

            for (GarbageCollectorMXBean gc : gcBeans) {
                System.out.printf("  GC: %-25s  count=%,5d  time=%,dms%n",
                    gc.getName(),
                    gc.getCollectionCount(),
                    gc.getCollectionTime());
            }
        }

        // ── 4. MemoryUsage Notification — alert when heap is high ───────────
        static void setupHeapThresholdAlert() {
            System.out.println("\n=== Programmatic: Heap Threshold Alert ===");

            List<MemoryPoolMXBean> pools = ManagementFactory.getMemoryPoolMXBeans();
            for (MemoryPoolMXBean pool : pools) {
                if (pool.getType() == MemoryType.HEAP && pool.isUsageThresholdSupported()) {
                    long threshold = (long) (pool.getUsage().getMax() * 0.80); // 80% alert
                    pool.setUsageThreshold(threshold);
                    System.out.printf("  Threshold set on '%s': %,dMB (80%% of max)%n",
                        pool.getName(), threshold / (1024*1024));
                    // In production: attach NotificationListener to get callbacks
                }
            }

            /* Production pattern — attach listener:
             *
             * MemoryMXBean memBean = ManagementFactory.getMemoryMXBean();
             * NotificationEmitter emitter = (NotificationEmitter) memBean;
             * emitter.addNotificationListener((notification, handback) -> {
             *     if (notification.getType().equals(
             *             MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED)) {
             *         MemoryNotificationInfo info = MemoryNotificationInfo
             *             .from((CompositeData) notification.getUserData());
             *         System.err.printf("HEAP ALERT: %s used=%,dMB%n",
             *             info.getPoolName(),
             *             info.getUsage().getUsed() / (1024 * 1024));
             *         // → send alert to monitoring, trigger graceful degradation
             *     }
             * }, null, null);
             */
        }

        // ── 5. RuntimeMXBean — JVM uptime, args ────────────────────────────
        static void runtimeInfo() {
            System.out.println("\n=== Programmatic: RuntimeMXBean ===");

            RuntimeMXBean rt = ManagementFactory.getRuntimeMXBean();
            System.out.printf("  JVM PID    : %d%n",
                ProcessHandle.current().pid());
            System.out.printf("  JVM Name   : %s%n", rt.getVmName());
            System.out.printf("  JVM Vendor : %s%n", rt.getVmVendor());
            System.out.printf("  JVM Version: %s%n", rt.getVmVersion());
            System.out.printf("  Uptime     : %,d ms%n", rt.getUptime());
            System.out.printf("  Start time : %s%n",
                Instant.ofEpochMilli(rt.getStartTime())
                    .atZone(java.time.ZoneId.systemDefault())
                    .format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")));
        }

        // ── 6. HeapMonitor thread — continuous monitoring ───────────────────
        static class HeapMonitor implements Runnable {
            private final long intervalMs;
            private final double alertThresholdPct;
            private volatile boolean running = true;

            HeapMonitor(long intervalMs, double alertThresholdPct) {
                this.intervalMs         = intervalMs;
                this.alertThresholdPct  = alertThresholdPct;
            }

            @Override
            public void run() {
                MemoryMXBean bean = ManagementFactory.getMemoryMXBean();
                System.out.println("\n=== HeapMonitor (3 samples, 100ms interval) ===");

                for (int i = 0; i < 3 && running; i++) {
                    MemoryUsage heap = bean.getHeapMemoryUsage();
                    long   usedMB   = heap.getUsed()      / (1024*1024);
                    long   maxMB    = heap.getMax()        / (1024*1024);
                    double usedPct  = 100.0 * heap.getUsed() / Math.max(heap.getMax(), 1);

                    String status = usedPct > alertThresholdPct ? "⚠️  HIGH" : "✅ OK  ";
                    System.out.printf("  [%s] %s heap=%,dMB/%,dMB (%.1f%%)%n",
                        LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm:ss.SSS")),
                        status, usedMB, maxMB, usedPct);

                    try { Thread.sleep(intervalMs); } catch (InterruptedException e) { break; }
                }
            }

            void stop() { running = false; }
        }
    }

    // =========================================================================
    // SECTION 6: GC LOG ANALYSIS — Reading the GC Log File on RHEL
    // =========================================================================
    static void printGCLogAnalysis() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 6: GC Log Analysis on RHEL
        ══════════════════════════════════════════════════════════════════════

        ─── GC Log Location ─────────────────────────────────────────────────
        # Configured via: -Xlog:gc*:file=/var/log/trading/gc.log:time,uptime

        ─── Real-time monitoring ────────────────────────────────────────────
        # Watch for pauses:
        tail -f /var/log/trading/gc.log | grep -E "Pause|ms\\]"

        # Count GC events per minute:
        grep "GC(" /var/log/trading/gc.log | awk '{print $1}' | cut -c1-17 | uniq -c

        # Find longest pauses:
        grep "Pause" /var/log/trading/gc.log | \\
            awk '{print $NF, $0}' | sort -rn | head -20

        ─── ZGC Log Interpretation ──────────────────────────────────────────
        # ZGC log example:
        [2.345s][info][gc] GC(0) Garbage Collection (Warmup)
        [2.346s][info][gc,phases] GC(0) Pause Mark Start     0.012ms   ← STW
        [2.347s][info][gc,phases] GC(0) Concurrent Mark     12.345ms   ← concurrent
        [2.359s][info][gc,phases] GC(0) Pause Mark End       0.008ms   ← STW
        [2.360s][info][gc,phases] GC(0) Concurrent Process   1.234ms   ← concurrent
        [2.361s][info][gc,phases] GC(0) Pause Relocate Start 0.009ms   ← STW
        [2.362s][info][gc,phases] GC(0) Concurrent Relocate  5.678ms   ← concurrent
        [2.368s][info][gc,heap ] GC(0) Heap before: 2048M, after: 1024M (freed 1GB)

        # Key metrics:
        # Pause phases = STW (Stop-The-World) — these cause latency spikes
        # ZGC target: all pauses < 1ms
        # Concurrent phases run alongside your application

        ─── G1GC Log Interpretation ─────────────────────────────────────────
        [1.234s][info][gc] GC(5) Pause Young (Normal) (G1 Evacuation Pause)
                          1.234s                                           ← timestamp
                               5                                           ← GC ID
                                 Pause Young (Normal)                      ← type
        [1.234s][info][gc] GC(5) 512M->256M(8192M) 45.678ms              ← before→after(max) time

        # Danger signs in G1GC:
        # "Pause Full"         → Full GC! Heap is fragmented or OOM
        # "to-space exhausted" → Survivor space full, objects promoted to old
        # "(G1 Humongous)"     → Object > 50% region size (memory fragmentation)

        ─── Tools for GC Log Analysis ───────────────────────────────────────
        # GCEasy (web): https://gceasy.io
        #   Upload gc.log → instant analysis, pause histogram, throughput

        # GCViewer (open source):
        #   java -jar gcviewer.jar /var/log/trading/gc.log

        # grep-based quick check on RHEL:
        echo "=== GC Pause Summary ==="
        grep -oP "\\d+\\.\\d+ms" /var/log/trading/gc.log | \\
            awk -F"ms" '{sum+=$1; if($1>max)max=$1; cnt++} \\
                 END {printf "count=%d avg=%.2fms max=%.2fms total=%.2fs\\n",
                      cnt, sum/cnt, max, sum/1000}'

        ─── Key Metrics to Alert On ─────────────────────────────────────────
        Metric                   Warning      Critical     Action
        ──────────────────────── ──────────── ──────────── ───────────────────────
        GC pause time (ZGC)      > 5ms        > 20ms       Increase heap / tune ZGC
        GC pause time (G1)       > 100ms      > 500ms      Switch to ZGC
        Full GC count            > 0          > 1          Fix memory leak / resize
        Heap utilisation         > 80%        > 90%        Increase Xmx or fix leak
        Metaspace growth         steady       growing      Classloader leak
        GC overhead (% time)     > 5%         > 10%        Too much allocation
        Allocation rate          > 1GB/s      > 5GB/s      Use object pools
        """);
    }

    // =========================================================================
    // SECTION 7: Eclipse MAT — Deep Heap Dump Analysis
    // =========================================================================
    static void printMATGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 7: Eclipse MAT (Memory Analyzer Tool) — Heap Dump Analysis
        ══════════════════════════════════════════════════════════════════════

        Download: https://www.eclipse.org/mat/downloads.php
        RHEL install:
          wget https://eclipse.dev/mat/releases/latest/mat.linux.x86_64.zip
          unzip mat.linux.x86_64.zip -d /opt/mat
          /opt/mat/MemoryAnalyzer   # GUI
          /opt/mat/ParseHeapDump.sh # headless (on server)

        ─── Headless Analysis on RHEL Server (no GUI needed) ────────────────
        # Parse heap dump and generate leak report:
        /opt/mat/ParseHeapDump.sh /var/dumps/heap.hprof \\
            org.eclipse.mat.api:suspects \\
            org.eclipse.mat.api:overview \\
            org.eclipse.mat.api:top_components

        # Output: heap.html + heap_Leak_Suspects.zip in same dir
        # Copy to local machine and open in browser:
        scp prod-server:/var/dumps/heap_Leak_Suspects.zip .

        ─── MAT GUI Key Features ─────────────────────────────────────────────

        1. Leak Suspects Report (File → Run Expert System Test → Leak Suspects)
           → Automatically finds likely memory leaks
           → Shows object retention paths
           → Groups by class, size, GC root

        2. Histogram View
           → Sort by "Retained Heap" (total memory held by object + references)
           → Retained Heap > Shallow Heap = object holds other objects
           → Find: which class holds the most memory?

        3. Dominator Tree
           → Shows which objects are "dominating" memory
           → Root → subtree = everything that would be freed if root freed
           → Find: single large object holding GB of data

        4. OQL (Object Query Language) — SQL for heap objects
           # Find all Orders with qty > 10000:
           SELECT * FROM com.trading.Order o WHERE o.qty > 10000

           # Count instances per class:
           SELECT COUNT(*) FROM java.util.HashMap$Node

           # Find all strings longer than 1000 chars:
           SELECT s FROM java.lang.String s WHERE s.value.length > 1000

        5. Retained Set
           → Select an object → right-click → "Show Retained Set"
           → Shows everything that would be freed if this object freed

        ─── Common Memory Leak Patterns to Look For ─────────────────────────

        Pattern               MAT Finding                 Fix
        ─────────────────────────────────────────────────────────────────────
        HashMap never cleared  Large HashMap retained      Clear or bound map size
        Static List growing    Large static collection     WeakReference or TTL eviction
        Listener not removed   EventBus/listener list      removeListener() on shutdown
        Thread-local leak      ThreadLocal in thread pool  ThreadLocal.remove() after use
        ClassLoader leak       Many Class objects loaded   Check custom classloaders
        String interning       Large String[]              Avoid String.intern() in loop
        Order objects piling   Order class dominates       Use object pool
        Byte[] from network    Large [B arrays             Off-heap buffers (ByteBuffer)
        """);
    }

    // =========================================================================
    // SECTION 8: async-profiler — Allocation Profiling (No Safepoint Bias)
    // =========================================================================
    static void printAsyncProfilerGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 8: async-profiler — Find What's Allocating on RHEL
        ══════════════════════════════════════════════════════════════════════

        Download:
          wget https://github.com/async-profiler/async-profiler/releases/\\
               latest/download/async-profiler-3.0-linux-x64.tar.gz
          tar xzf async-profiler-3.0-linux-x64.tar.gz -C /opt/

        Why async-profiler > jstack/jvisualvm:
          - Samples at OS level (POSIX signals), not safepoints
          - No safepoint bias (jstack only samples at safepoints)
          - Allocation profiling via TLAB events (sub-microsecond overhead)
          - Flame graphs for visual call tree

        ─── CPU Profiling ────────────────────────────────────────────────────
        # Profile for 30 seconds, generate flame graph:
        /opt/async-profiler/bin/asprof -d 30 -f /tmp/cpu_flame.html <PID>

        # Open flame graph:
        scp prod-server:/tmp/cpu_flame.html .
        open cpu_flame.html   # macOS
        xdg-open cpu_flame.html  # Linux

        # What to look for in flame graph:
        # Wide bars = function spends a lot of CPU time
        # Look for: GC threads (ZGC/G1 names), serialization, locking

        ─── Allocation Profiling (most useful for heap analysis) ─────────────
        # Profile allocations for 30 seconds:
        /opt/async-profiler/bin/asprof -e alloc -d 30 \\
            -f /tmp/alloc_flame.html <PID>

        # Shows: which call stacks are allocating the most memory
        # Wide bars = high allocation rate from that code path
        # Look for: hot path allocating String, Order, byte[]

        # Text output (top allocation sites):
        /opt/async-profiler/bin/asprof -e alloc -d 30 \\
            -o collapsed <PID> | \\
            awk -F';' '{print $NF, $0}' | sort -rn | head -20

        ─── Wall-clock profiling (find blocked/sleeping threads) ────────────
        /opt/async-profiler/bin/asprof -e wall -d 30 \\
            -f /tmp/wall_flame.html <PID>

        ─── JFR-format output (open in JMC) ─────────────────────────────────
        /opt/async-profiler/bin/asprof -d 30 -o jfr \\
            -f /tmp/recording.jfr <PID>

        ─── Attach to running JVM without stopping it ────────────────────────
        # Start profiling:
        /opt/async-profiler/bin/asprof start <PID>

        # Stop and dump:
        /opt/async-profiler/bin/asprof stop -f /tmp/profile.html <PID>

        ─── RHEL: Allow perf events (may need) ──────────────────────────────
        echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid
        echo 0 | sudo tee /proc/sys/kernel/kptr_restrict
        """);
    }

    // =========================================================================
    // SECTION 9: MEMORY LEAK DETECTION — Patterns and Scripts
    // =========================================================================
    static void printLeakDetection() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 9: Memory Leak Detection — Scripts and Patterns
        ══════════════════════════════════════════════════════════════════════

        ─── Script: Detect memory growth over time ──────────────────────────
        #!/bin/bash
        # leak_detector.sh — run on RHEL, monitor heap growth
        PID=$(pgrep -f trading-app.jar)
        echo "Monitoring PID $PID"
        echo "timestamp,used_mb,committed_mb,gc_count"

        for i in $(seq 1 60); do
            HEAP=$(jcmd $PID GC.heap_info 2>/dev/null | grep -oP 'used \\K[0-9]+')
            GC=$(jstat -gcutil $PID 1 1 2>/dev/null | tail -1 | awk '{print $7}')
            echo "$(date +%H:%M:%S),${HEAP:-0},${GC:-0}"
            sleep 60
        done

        # If HEAP grows every minute with no plateau → memory leak


        ─── Script: Top 20 object types ─────────────────────────────────────
        #!/bin/bash
        # top_objects.sh — snapshot every 10 minutes
        PID=$(pgrep -f trading-app.jar)
        OUTFILE="/tmp/histogram_$(date +%H%M%S).txt"

        jcmd $PID GC.class_histogram | head -25 > $OUTFILE
        echo "Histogram saved to $OUTFILE"

        # Run twice, 10 minutes apart, then diff:
        diff /tmp/histogram_HHMM01.txt /tmp/histogram_HHMM02.txt | grep "^[<>]"
        # Growing classes appear in the diff


        ─── Script: Watch allocation rate ───────────────────────────────────
        #!/bin/bash
        # alloc_rate.sh — show MB/s allocation rate
        PID=$(pgrep -f trading-app.jar)
        PREV_YGC=0; PREV_YGCT=0

        while true; do
            DATA=$(jstat -gcnew $PID 1 1 2>/dev/null | tail -1)
            EDEN_USED=$(echo $DATA | awk '{print $5}')
            echo "$(date +%T) Eden used: ${EDEN_USED}KB"
            sleep 1
        done


        ─── Weak References for Cache (prevent leak) ────────────────────────
        // BAD: static cache that grows forever = memory leak
        static Map<String, Order> cache = new HashMap<>();

        // GOOD: WeakHashMap — entries GC'd when key has no other references
        static Map<String, Order> cache = new WeakHashMap<>();

        // BETTER for ULL: bounded cache with explicit eviction
        static Map<Integer, Order> cache = Collections.synchronizedMap(
            new LinkedHashMap<>(1000, 0.75f, true) {  // LRU
                @Override protected boolean removeEldestEntry(Map.Entry e) {
                    return size() > 1000;  // evict when > 1000 entries
                }
            }
        );

        // BEST for trading: pre-allocated fixed array (zero GC pressure)
        static Order[] orderCache = new Order[MAX_ORDERS];  // int ID → Order


        ─── Detect static collection leaks ──────────────────────────────────
        // Pattern: static List/Map that grows on every trade → never cleared
        // Detection: histogram shows growing count of List entries or Map.Entry

        // In MAT: look for large Retained Heap on static fields
        // OQL: SELECT l FROM java.util.ArrayList l WHERE l.size > 10000
        """);
    }

    // =========================================================================
    // SECTION 10: ULL TRADING — Heap Analysis Checklist
    // =========================================================================
    static void printULLChecklist() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 10: ULL Trading — Heap Analysis Checklist
        ══════════════════════════════════════════════════════════════════════

        ─── Pre-deployment checks ────────────────────────────────────────────
        □ -Xms = -Xmx (prevent heap resize during trading)
        □ -XX:+AlwaysPreTouch (pre-fault pages before first trade)
        □ -XX:+HeapDumpOnOutOfMemoryError (auto-dump on crash)
        □ ZGC or Shenandoah (sub-ms pauses)
        □ GC log enabled with rolling files
        □ Heap sized at 2-3× peak live set (leave room for GC headroom)

        ─── During warm-up phase (before first trade) ───────────────────────
        □ jstat -gcutil <PID> 1000 — watch allocation rate stabilise
        □ Histogram baseline: jcmd <PID> GC.class_histogram > baseline.txt
        □ YGC rate should drop to near-zero after warm-up
        □ Old gen (O%) should stabilise (not growing)

        ─── Hot path allocation checks ───────────────────────────────────────
        □ async-profiler -e alloc: hot path should show ZERO allocation
          Acceptable: pre-allocated objects from pool
          Bad: new Order(), new String(), new byte[], new ArrayList()
        □ jstat Eden (E%) should be flat/stable during trading hours
        □ YGC count should not grow during trading (no young gen GC)

        ─── Periodic monitoring (every 5 minutes) ───────────────────────────
        □ jstat -gcutil <PID> — check O%, FGC count, GCT
        □ Heap utilisation < 60% (ZGC triggers at configurable threshold)
        □ No Full GC (FGC = 0 always)
        □ GC pause < 1ms (ZGC), < 100ms (G1GC)

        ─── After incident (latency spike) ──────────────────────────────────
        □ Check GC log for pause times at incident timestamp
        □ jcmd GC.class_histogram — did object count spike?
        □ async-profiler flame graph from 30s window around incident
        □ Check safepoint log: -Xlog:safepoint — long safepoints?

        ─── Memory sizing formula ────────────────────────────────────────────
        Xmx = (peak live set) × 3     for ZGC (ZGC needs headroom to relocate)
        Xmx = (peak live set) × 2     for G1GC

        # Measure peak live set:
        jmap -histo:live <PID>   # triggers GC, then reports live bytes
        # → Total bytes = your live set size

        # Example:
        # Live set = 2GB → ZGC Xmx = 6GB
        # Leave 2GB headroom for concurrent relocation + allocation during GC


        ─── Common trading heap issues and fixes ─────────────────────────────

        Issue                          Root Cause                  Fix
        ─────────────────────────────  ──────────────────────────  ──────────────────────
        OOM on EOD                     Order history not cleared   Bound history buffer
        Long GC pauses mid-session     G1 Full GC triggered        Switch to ZGC
        High allocation rate           new Order() on hot path     Object pool
        Metaspace grows steadily       Custom classloader leak     Fix classloader lifecycle
        String[] dominates heap        String parsing on hot path  Pre-parse at startup
        byte[] dominates heap          Large network buffers       off-heap ByteBuffer
        HashMap$Node dominates         Order map not evicted       Bounded map + eviction
        Thread stack OOM               Too many threads            Reduce thread count
        """);
    }

    // =========================================================================
    // SECTION 11: Native Memory Tracking (NMT) — Off-Heap Monitoring
    // =========================================================================
    static void printNMTGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 11: Native Memory Tracking (NMT) — Track Off-Heap Memory
        ══════════════════════════════════════════════════════════════════════

        # Enable NMT at JVM startup (small overhead):
        java -XX:NativeMemoryTracking=summary -jar app.jar
        java -XX:NativeMemoryTracking=detail  -jar app.jar  # more detail

        # Check native memory summary:
        jcmd <PID> VM.native_memory summary

        # Sample output:
        # Native Memory Tracking:
        # Total: reserved=12288MB, committed=8448MB
        #
        # Java Heap (reserved=8192MB, committed=8192MB)
        #   (mmap: reserved=8192MB, committed=8192MB)
        #
        # Class (reserved=1088MB, committed=68MB)
        #   (classes #12345)
        #   (  instance classes #11000, array classes #1345)
        #
        # Thread (reserved=2048MB, committed=32MB)
        #   (thread #64)
        #   (stack: reserved=2048MB, committed=32MB)
        #
        # Code (reserved=256MB, committed=80MB)
        #   (mmap: reserved=256MB, committed=80MB)
        #
        # GC (reserved=512MB, committed=512MB)   ← ZGC metadata
        #
        # Internal (reserved=64MB, committed=64MB)
        #
        # Symbol (reserved=30MB, committed=30MB)  ← interned strings
        #
        # Native Memory Tracking (reserved=10MB, committed=10MB)
        #
        # Shared class space (reserved=512MB, committed=12MB)
        #
        # Arena Chunk (reserved=2MB, committed=2MB)
        #
        # Tracing (reserved=64MB, committed=64MB)

        # Baseline + diff (find what grew over time):
        jcmd <PID> VM.native_memory baseline
        # ... wait some time ...
        jcmd <PID> VM.native_memory summary.diff

        # Key areas to watch:
        # Thread growing  → threads being created, not terminated
        # Code growing    → excessive JIT compilation (hot-deploy leaks)
        # Symbol growing  → String.intern() abuse
        # Internal growing → JNI/native library allocations
        """);
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws InterruptedException {
        System.out.println("=".repeat(70));
        System.out.println("  Java Heap Analysis Techniques on RHEL — Complete Guide");
        System.out.println("=".repeat(70));

        // Print all command-line guides
        printJVMLaunchFlags();
        printJcmdCommands();
        printJmapCommands();
        printJstatCommands();

        // Run live programmatic demos
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 5: Programmatic Heap Analysis — Live Demo
        ══════════════════════════════════════════════════════════════════════""");

        ProgrammaticHeapAnalysis.runtimeInfo();
        ProgrammaticHeapAnalysis.memoryMXBeanDemo();
        ProgrammaticHeapAnalysis.memoryPoolDemo();
        ProgrammaticHeapAnalysis.gcMXBeanDemo();
        ProgrammaticHeapAnalysis.setupHeapThresholdAlert();

        // Start HeapMonitor thread
        ProgrammaticHeapAnalysis.HeapMonitor monitor =
            new ProgrammaticHeapAnalysis.HeapMonitor(100, 80.0);
        Thread monThread = new Thread(monitor, "heap-monitor");
        monThread.setDaemon(true);
        monThread.start();
        monThread.join();

        // Rest of guide
        printGCLogAnalysis();
        printMATGuide();
        printAsyncProfilerGuide();
        printLeakDetection();
        printULLChecklist();
        printNMTGuide();

        System.out.println("\n" + "=".repeat(70));
        System.out.printf("Java: %s | Heap: %,dMB used / %,dMB max%n",
            System.getProperty("java.version"),
            (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory()) / (1024*1024),
            Runtime.getRuntime().maxMemory() / (1024*1024));
        System.out.println("✅ Complete.");
    }
}

