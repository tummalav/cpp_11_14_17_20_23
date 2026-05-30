/**
 * ============================================================================
 * Java Native Memory Analysis on RHEL — Complete Guide
 * ============================================================================
 *
 * WHAT IS NATIVE MEMORY?
 *   Everything the JVM process uses OUTSIDE the Java heap.
 *   GC cannot see it. You (or the JVM internals) manage its lifetime.
 *
 * NATIVE MEMORY CONSUMERS IN A JVM PROCESS:
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │ Category            │ Typical Size │ Who Allocates                   │
 * ├─────────────────────┼──────────────┼─────────────────────────────────┤
 * │ Java Heap (GC)      │ Xmx          │ JVM GC subsystem                │
 * │ Metaspace           │ 64–256 MB    │ Class metadata, method data     │
 * │ Thread Stacks       │ 512k × N     │ Each Java/native thread         │
 * │ JIT Code Cache      │ 64–256 MB    │ JIT-compiled methods            │
 * │ Direct ByteBuffers  │ unbounded!   │ ByteBuffer.allocateDirect()     │
 * │ Unsafe.allocate     │ unbounded!   │ sun.misc.Unsafe.allocateMemory  │
 * │ MemorySegment       │ unbounded!   │ Java 22+ Foreign Memory API     │
 * │ mmap / MappedBuf    │ unbounded!   │ FileChannel.map()               │
 * │ JNI / native libs   │ unbounded!   │ Native code via JNI             │
 * │ GC metadata         │ 256–512 MB   │ ZGC/G1 internal bookkeeping     │
 * │ Symbol table        │ 10–50 MB     │ String.intern(), class names    │
 * │ Arena/Chunk         │ variable     │ JVM internal allocator          │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * TOOLS COVERED:
 *   1.  NMT  (JVM Native Memory Tracking) — built-in, lowest overhead
 *   2.  jcmd VM.native_memory             — query NMT from command line
 *   3.  /proc/<PID>/maps & smaps          — OS-level memory map
 *   4.  pmap                              — pretty-printed /proc/maps
 *   5.  Programmatic NMT via MXBean       — runtime queries in code
 *   6.  Direct ByteBuffer tracking        — track off-heap buffers
 *   7.  Unsafe memory tracking            — track raw allocations
 *   8.  jemalloc / malloc profiling       — native heap profiler
 *   9.  Valgrind / AddressSanitizer       — native leak detection
 *   10. async-profiler malloc mode        — allocation flame graphs
 *   11. ULL trading patterns              — common native memory issues
 *
 * Build:
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/javac \
 *       Java_Native_Memory_Analysis_RHEL.java
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/java \
 *       -XX:NativeMemoryTracking=summary \
 *       -Xmx256m Java_Native_Memory_Analysis_RHEL
 * ============================================================================
 */

import java.lang.management.*;
import java.lang.ref.*;
import java.lang.reflect.*;
import java.nio.*;
import java.nio.channels.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class Java_Native_Memory_Analysis_RHEL {

    // =========================================================================
    // SECTION 1: NMT — JVM Native Memory Tracking (Built-in)
    // =========================================================================
    static void printNMTSetupAndCommands() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         SECTION 1: NMT — JVM Native Memory Tracking (Built-in Tool)
        ══════════════════════════════════════════════════════════════════════

        ─── Enable NMT at JVM launch ────────────────────────────────────────
        # summary: low overhead (~5%), tracks categories
        java -XX:NativeMemoryTracking=summary -Xmx8g -jar app.jar

        # detail: moderate overhead (~10%), tracks individual allocations
        java -XX:NativeMemoryTracking=detail  -Xmx8g -jar app.jar

        # off: disabled (default)
        java -XX:NativeMemoryTracking=off     -Xmx8g -jar app.jar

        ─── Query NMT via jcmd ──────────────────────────────────────────────
        PID=$(pgrep -f trading-app.jar)

        # Summary view:
        jcmd $PID VM.native_memory summary

        # Detailed view (per-callsite breakdown):
        jcmd $PID VM.native_memory detail

        # Human-readable scale (KB/MB/GB):
        jcmd $PID VM.native_memory summary scale=MB

        ─── NMT Summary Output — Annotated ─────────────────────────────────
        # jcmd $PID VM.native_memory summary scale=MB

        Total: reserved=13312MB, committed=8960MB
        #        ^reserved = virtual address space mapped
        #                     ^committed = physical RAM actually used

        Java Heap (reserved=8192MB, committed=8192MB)
        #  = -Xmx. committed = AlwaysPreTouch touched all pages

        Class (reserved=1088MB, committed=72MB)
        #  = Metaspace: class bytecodes, method metadata, constant pools
        #  reserved >> committed: JVM pre-reserves VA space for metaspace growth

        Thread (reserved=2176MB, committed=34MB)
        #  = Thread stacks. reserved = maxStackSize × threadCount
        #  committed = pages actually written to (active stack frames)
        #  High reserved here → too many threads (each thread: 512k-1m reserved)

        Code (reserved=256MB, committed=80MB)
        #  = JIT compiled code cache (ReservedCodeCacheSize)
        #  If committed near reserved: increase -XX:ReservedCodeCacheSize=512m

        GC (reserved=512MB, committed=512MB)
        #  = ZGC/G1 internal data structures, card tables, remembered sets
        #  ZGC uses more than G1 for concurrent relocation metadata

        Compiler (reserved=8MB, committed=8MB)
        #  = JIT compiler working memory (temporary IR, etc.)

        Internal (reserved=20MB, committed=20MB)
        #  = JVM internal: symbol table, string pool, handles

        Symbol (reserved=28MB, committed=28MB)
        #  = Interned strings (String.intern()), class/method name symbols
        #  If growing: String.intern() abuse or classloader leak

        Native Memory Tracking (reserved=10MB, committed=10MB)
        #  = NMT's own bookkeeping overhead

        Shared class space (reserved=512MB, committed=12MB)
        #  = CDS (Class Data Sharing) archive

        Arena Chunk (reserved=2MB, committed=2MB)
        #  = JVM arena allocator scratch space

        ─── Baseline + Diff (find what grew) ────────────────────────────────
        # Take baseline:
        jcmd $PID VM.native_memory baseline

        # ... wait 10 minutes under load ...

        # Show what changed:
        jcmd $PID VM.native_memory summary.diff scale=MB

        # Sample diff output:
        # Thread (reserved=+256MB, committed=+4MB)
        #   ← 512 new threads created since baseline

        # Symbol (reserved=+8MB, committed=+8MB)
        #   ← String.intern() adding entries

        # Class (reserved=0, committed=+5MB)
        #   ← New classes loaded (hot-deploy, dynamic proxies)

        ─── NMT Detail — Find Specific Allocations ──────────────────────────
        # jcmd $PID VM.native_memory detail scale=MB

        # Shows per-callsite:
        # [0x00007f1234567890] Unsafe_AllocateMemory+0x28
        #   committed=128MB
        # [0x00007f9876543210] DirectByteBuffer.allocateDirect+0x40
        #   committed=512MB

        ─── NMT limitations ─────────────────────────────────────────────────
        # NMT does NOT track:
        #   - Native library (JNI) allocations (malloc from C code)
        #   - Memory-mapped files (shows in /proc/maps but not NMT)
        #   - Allocations from non-JVM threads (C threads spawned by JNI)
        # For those: use pmap, /proc/smaps, or jemalloc profiling
        """);
    }

    // =========================================================================
    // SECTION 2: /proc — OS-Level Native Memory View
    // =========================================================================
    static void printProcMapsGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 2: /proc/<PID> — OS Native Memory View on RHEL
        ══════════════════════════════════════════════════════════════════════

        # Get JVM PID:
        PID=$(pgrep -f trading-app.jar)

        ─── /proc/<PID>/status — Quick memory summary ────────────────────────
        cat /proc/$PID/status | grep -E "Vm|Threads"

        # Key fields:
        # VmPeak:   20480000 kB  ← peak virtual memory used (ever)
        # VmSize:   18432000 kB  ← current virtual memory (reserved)
        # VmLck:         0 kB   ← locked pages (mlock)
        # VmPin:         0 kB   ← pinned pages
        # VmHWM:    12288000 kB  ← peak RSS (high-water mark)
        # VmRSS:    10240000 kB  ← current RSS (actual physical RAM used) ← KEY
        # VmAnon:    9216000 kB  ← anonymous (heap, stacks)
        # VmFile:    1024000 kB  ← file-backed (mmap files, JAR files)
        # VmShr:      512000 kB  ← shared memory (shared libs)
        # VmData:    8192000 kB  ← data segment (heap)
        # VmStk:        8192 kB ← main thread stack
        # VmExe:         512 kB ← executable code
        # VmLib:      256000 kB ← shared library code
        # Threads:         128   ← active thread count
        #
        # VmRSS = actual physical RAM used by this process
        # VmSize = virtual address space (can be >> RAM, it's just reserved)
        # VmRSS >> Xmx → large native memory outside heap

        ─── pmap — Pretty-printed memory map ────────────────────────────────
        # Summary (aggregated by type):
        pmap -x $PID | tail -5

        # Detailed map with sizes:
        pmap -x $PID | sort -rn -k3 | head -30

        # Column meanings (pmap -x):
        # Address  Kbytes  RSS  Dirty  Mode  Mapping
        # 7f000000  8388608  8388608  0  rw--  [heap/anon]    ← Java heap
        # 7e000000   512000   80000  0  r-x-  libjvm.so      ← JVM code
        # 7d000000   131072   32768  0  rwx-  [anon]          ← JIT code cache
        # ...

        # RSS column = physical pages actually in RAM for each region
        # Large [anon] regions outside heap = Direct ByteBuffers or Unsafe

        # Filter for large anonymous regions (= off-heap allocations):
        pmap -x $PID | awk '$3 > 102400 && $6 ~ /anon/'
        # Shows regions > 100MB that are anonymous (no file backing)

        ─── /proc/<PID>/smaps — Detailed per-region stats ───────────────────
        # Full smaps (very verbose):
        cat /proc/$PID/smaps | head -100

        # Aggregate RSS by mapping type:
        awk '/^[0-9a-f]/{split($0,a," "); region=a[6]}
             /^Rss/{total[region]+=$2}
             END{for(r in total) print total[r], r}' /proc/$PID/smaps \\
             | sort -rn | head -20

        # Summarize total RSS:
        grep "^Rss:" /proc/$PID/smaps | awk '{sum+=$2} END{print sum/1024 "MB RSS total"}'

        # Find hugepage usage:
        grep "^AnonHugePages:" /proc/$PID/smaps | awk '{sum+=$2} END{print sum/1024 "MB HugePages"}'

        ─── /proc/<PID>/maps — Virtual address space layout ─────────────────
        cat /proc/$PID/maps

        # Format: addr-addr perms offset dev inode pathname
        # 7f1234000000-7f1334000000 rw-p 00000000 00:00 0           [anon:Java Heap]
        # 7f1334000000-7f1434000000 ---p 00000000 00:00 0
        # 7f0000000000-7f0010000000 rw-p 00000000 00:00 0           [anon]  ← Direct BB?
        # 7f5000000000-7f5001000000 rw-s 00000000 fd:01 12345678    /dev/shm/trading
        #                                                              ^ shared memory file

        # Count anonymous regions:
        grep " [rw]..p" /proc/$PID/maps | grep -v "java\\|jvm\\|\\.so\\|\\.jar" | wc -l

        ─── Track native memory growth over time ────────────────────────────
        #!/bin/bash
        # native_mem_watch.sh
        PID=$(pgrep -f trading-app.jar)
        echo "time,VmRSS_MB,VmSize_MB,Threads"
        while true; do
            RSS=$(grep VmRSS /proc/$PID/status | awk '{print $2/1024}')
            SZ=$(grep VmSize /proc/$PID/status | awk '{print $2/1024}')
            TH=$(grep Threads /proc/$PID/status | awk '{print $2}')
            echo "$(date +%H:%M:%S),${RSS%.*},${SZ%.*},$TH"
            sleep 30
        done

        # If VmRSS grows steadily → native memory leak
        # If Threads grows → thread leak
        """);
    }

    // =========================================================================
    // SECTION 3: Programmatic Native Memory Monitoring in Java
    // =========================================================================
    static class ProgrammaticNativeMonitor {

        // ── 1. Track total process RSS via /proc/self/status ────────────────
        static long getProcessRSSMB() {
            try {
                List<String> lines = Files.readAllLines(Path.of("/proc/self/status"));
                for (String line : lines) {
                    if (line.startsWith("VmRSS:")) {
                        // VmRSS:    1234567 kB
                        String[] parts = line.trim().split("\\s+");
                        return Long.parseLong(parts[1]) / 1024; // KB → MB
                    }
                }
            } catch (Exception e) {
                // Not on Linux (e.g., macOS in dev)
            }
            return -1;
        }

        static long getProcessVMSizeMB() {
            try {
                List<String> lines = Files.readAllLines(Path.of("/proc/self/status"));
                for (String line : lines) {
                    if (line.startsWith("VmSize:")) {
                        String[] parts = line.trim().split("\\s+");
                        return Long.parseLong(parts[1]) / 1024;
                    }
                }
            } catch (Exception e) {}
            return -1;
        }

        static int getThreadCount() {
            try {
                List<String> lines = Files.readAllLines(Path.of("/proc/self/status"));
                for (String line : lines) {
                    if (line.startsWith("Threads:")) {
                        return Integer.parseInt(line.trim().split("\\s+")[1]);
                    }
                }
            } catch (Exception e) {}
            return Thread.getAllStackTraces().size(); // fallback
        }

        // ── 2. Track Direct ByteBuffer usage ────────────────────────────────
        // DirectByteBuffers live off-heap — NMT tracks them under "Internal"
        // or you can track them yourself

        static final AtomicLong directBytesAllocated = new AtomicLong(0);
        static final AtomicLong directBytesFreed     = new AtomicLong(0);
        static final AtomicInteger directBufferCount = new AtomicInteger(0);

        static ByteBuffer allocateTrackedDirect(int capacity) {
            ByteBuffer buf = ByteBuffer.allocateDirect(capacity);
            directBytesAllocated.addAndGet(capacity);
            directBufferCount.incrementAndGet();
            return buf;
        }

        // Note: DirectByteBuffer freed when GC collects the Java object.
        // For explicit release use the cleaner:
        static void freeDirect(ByteBuffer buf) {
            if (!buf.isDirect()) return;
            try {
                // Java 9+: use Unsafe.invokeCleaner
                Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
                Field f = unsafeClass.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                Object unsafe = f.get(null);
                java.lang.reflect.Method cleaner =
                    unsafeClass.getMethod("invokeCleaner", ByteBuffer.class);
                cleaner.invoke(unsafe, buf);
                directBytesFreed.addAndGet(buf.capacity());
                directBufferCount.decrementAndGet();
            } catch (Exception e) {
                // Fallback: let GC clean it
            }
        }

        // ── 3. Track Unsafe allocations ─────────────────────────────────────
        static sun.misc.Unsafe UNSAFE;
        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
            } catch (Exception e) { throw new RuntimeException(e); }
        }

        static final AtomicLong unsafeBytesAllocated = new AtomicLong(0);
        static final Map<Long, Long> unsafeAllocations =
            new ConcurrentHashMap<>(); // addr → size

        static long allocateTrackedUnsafe(long bytes) {
            long addr = UNSAFE.allocateMemory(bytes);
            UNSAFE.setMemory(addr, bytes, (byte) 0);
            unsafeBytesAllocated.addAndGet(bytes);
            unsafeAllocations.put(addr, bytes);
            return addr;
        }

        static void freeTrackedUnsafe(long addr) {
            Long size = unsafeAllocations.remove(addr);
            if (size != null) {
                UNSAFE.freeMemory(addr);
                unsafeBytesAllocated.addAndGet(-size);
            }
        }

        // ── 4. NMT summary via MXBean + OS ──────────────────────────────────
        static void printNativeMemorySummary() {
            System.out.println("\n=== Programmatic Native Memory Summary ===");

            MemoryMXBean mxBean = ManagementFactory.getMemoryMXBean();
            MemoryUsage heap    = mxBean.getHeapMemoryUsage();
            MemoryUsage nonHeap = mxBean.getNonHeapMemoryUsage();

            long heapUsedMB    = heap.getUsed()      / (1024 * 1024);
            long heapMaxMB     = heap.getMax()        / (1024 * 1024);
            long nonHeapUsedMB = nonHeap.getUsed()    / (1024 * 1024);

            // OS-level (Linux only)
            long rssMB    = getProcessRSSMB();
            long vmSizeMB = getProcessVMSizeMB();
            int  threads  = getThreadCount();

            System.out.printf("  Java Heap used     : %,6d MB  (max %,d MB)%n",
                heapUsedMB, heapMaxMB);
            System.out.printf("  Non-Heap (metaspace): %,5d MB%n", nonHeapUsedMB);

            long nativeMB = (rssMB > 0) ? rssMB - heapUsedMB - nonHeapUsedMB : -1;
            if (rssMB > 0) {
                System.out.printf("  Process RSS (total) : %,6d MB%n", rssMB);
                System.out.printf("  VmSize (virtual)    : %,6d MB%n", vmSizeMB);
                System.out.printf("  Native overhead     : ~%,5d MB  (RSS - heap - metaspace)%n",
                    nativeMB);
                System.out.printf("  Thread count        : %d%n", threads);
            } else {
                System.out.println("  (Process RSS: /proc not available on this OS)");
            }

            // Direct ByteBuffer tracking
            System.out.printf("  Direct ByteBuffers  : %d buffers, %,d MB allocated%n",
                directBufferCount.get(),
                directBytesAllocated.get() / (1024 * 1024));

            // Unsafe tracking
            System.out.printf("  Unsafe allocations  : %d regions, %,d MB%n",
                unsafeAllocations.size(),
                unsafeBytesAllocated.get() / (1024 * 1024));

            // JVM memory pools (metaspace, code cache, etc.)
            System.out.println("\n  JVM Memory Pools (non-heap):");
            for (MemoryPoolMXBean pool : ManagementFactory.getMemoryPoolMXBeans()) {
                if (pool.getType() == MemoryType.NON_HEAP) {
                    MemoryUsage u = pool.getUsage();
                    System.out.printf("    %-40s used=%,5d MB%n",
                        pool.getName(), u.getUsed() / (1024 * 1024));
                }
            }
        }

        // ── 5. Native memory leak detector ──────────────────────────────────
        static class NativeMemoryLeakDetector {
            private final long baselineRSSMB;
            private final long baselineDirectMB;
            private final long baselineUnsafeMB;

            NativeMemoryLeakDetector() {
                this.baselineRSSMB    = getProcessRSSMB();
                this.baselineDirectMB = directBytesAllocated.get() / (1024*1024);
                this.baselineUnsafeMB = unsafeBytesAllocated.get() / (1024*1024);
                System.out.printf("  Baseline: RSS=%dMB Direct=%dMB Unsafe=%dMB%n",
                    baselineRSSMB, baselineDirectMB, baselineUnsafeMB);
            }

            void check(String label) {
                long currentRSS    = getProcessRSSMB();
                long currentDirect = directBytesAllocated.get() / (1024*1024);
                long currentUnsafe = unsafeBytesAllocated.get() / (1024*1024);

                long deltRSS    = currentRSS    - baselineRSSMB;
                long deltDirect = currentDirect - baselineDirectMB;
                long deltUnsafe = currentUnsafe - baselineUnsafeMB;

                String rssAlert    = deltRSS    > 50  ? "⚠️  LEAK?" : "✅";
                String directAlert = deltDirect > 10  ? "⚠️  LEAK?" : "✅";
                String unsafeAlert = deltUnsafe > 10  ? "⚠️  LEAK?" : "✅";

                System.out.printf("  [%s] RSS Δ=%+dMB%s  DirectBB Δ=%+dMB%s  Unsafe Δ=%+dMB%s%n",
                    label,
                    deltRSS,    rssAlert,
                    deltDirect, directAlert,
                    deltUnsafe, unsafeAlert);
            }
        }

        static void runDemo() throws InterruptedException {
            System.out.println("\n=== Demo: Native Memory Tracking ===");

            // Baseline
            NativeMemoryLeakDetector detector = new NativeMemoryLeakDetector();

            // Allocate Direct ByteBuffers
            List<ByteBuffer> buffers = new ArrayList<>();
            for (int i = 0; i < 5; i++) {
                buffers.add(allocateTrackedDirect(10 * 1024 * 1024)); // 10MB each
            }
            detector.check("After 5×10MB DirectBB");

            // Allocate Unsafe memory
            long addr1 = allocateTrackedUnsafe(50 * 1024 * 1024); // 50MB
            detector.check("After 50MB Unsafe");

            // Free Unsafe
            freeTrackedUnsafe(addr1);
            detector.check("After Unsafe free");

            // Free Direct ByteBuffers
            for (ByteBuffer buf : buffers) freeDirect(buf);
            buffers.clear();
            detector.check("After DirectBB free");

            // Simulate leak: allocate but don't free
            long leaked = allocateTrackedUnsafe(20 * 1024 * 1024); // 20MB LEAKED
            detector.check("After 20MB Unsafe LEAK (not freed)");
            // In production: always call freeTrackedUnsafe(leaked) in finally block
            freeTrackedUnsafe(leaked); // cleanup for demo
        }
    }

    // =========================================================================
    // SECTION 4: Direct ByteBuffer — Monitoring and Sizing
    // =========================================================================
    static void printDirectByteBufferMonitoring() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 4: Direct ByteBuffer — Native Memory Monitoring
        ══════════════════════════════════════════════════════════════════════

        ─── Limit total direct memory ────────────────────────────────────────
        # -XX:MaxDirectMemorySize limits total ByteBuffer.allocateDirect()
        java -XX:MaxDirectMemorySize=4g -Xmx8g -jar app.jar

        # Without this: DirectByteBuffers can consume ALL available RAM!
        # OOM from direct memory: "java.lang.OutOfMemoryError: Direct buffer memory"

        ─── Monitor via JMX / MXBean ────────────────────────────────────────
        # java.nio.BufferPool MXBean tracks direct and mapped buffers:

        import java.lang.management.*;
        import java.nio.*;

        List<BufferPoolMXBean> pools =
            ManagementFactory.getPlatformMXBeans(BufferPoolMXBean.class);

        for (BufferPoolMXBean pool : pools) {
            System.out.printf("Pool: %-10s  count=%,7d  capacity=%,7dMB  used=%,7dMB%n",
                pool.getName(),
                pool.getCount(),
                pool.getTotalCapacity() / (1024*1024),
                pool.getMemoryUsed()    / (1024*1024));
        }

        # Sample output:
        # Pool: direct      count=   1024  capacity=    512MB  used=    490MB
        # Pool: mapped       count=      3  capacity=     12MB  used=     12MB
        # Pool: mapped - 'non-volatile memory'  count=0  capacity=0MB  used=0MB

        ─── jcmd to check Direct ByteBuffer stats ───────────────────────────
        # Direct buffers show up under NMT "Internal" or as pmap anon regions

        # Better: use jcmd to query BufferPool MXBean
        jcmd $PID VM.native_memory detail | grep -A5 "Direct"

        # Or via JConsole: connect → Memory → Non-Heap → Direct buffers

        ─── Identify who's allocating Direct ByteBuffers ────────────────────
        # async-profiler: trace ByteBuffer.allocateDirect call sites
        /opt/async-profiler/bin/asprof -e alloc -d 30 \\
            --alloc-interval=1 -f /tmp/direct_alloc.html $PID

        # Look in flame graph for: java/nio/ByteBuffer.allocateDirect

        ─── Direct ByteBuffer lifecycle ────────────────────────────────────
        Allocation   → ByteBuffer.allocateDirect(N)
                       Native malloc called, Cleaner registered
        GC           → When DirectByteBuffer Java object is GC'd,
                       Cleaner.clean() called → native free()
                       WARNING: GC runs based on HEAP pressure, not native!
                       If heap has room but native is full → OOM native

        Explicit free → ((DirectBuffer) buf).cleaner().clean()  // Java 8
                        Unsafe.invokeCleaner(buf)                // Java 9+
                        Use this for large buffers!

        # TRAP: heap is small (low GC frequency) but allocating large
        # DirectByteBuffers → GC never runs → native OOM
        # Fix: -XX:MaxDirectMemorySize or explicit free after use

        ─── DirectByteBuffer naming for pmap ────────────────────────────────
        # In /proc/maps, DirectByteBuffers appear as [anon] regions
        # Size clue: if you see many [anon] regions of the same size,
        # they're likely from allocateDirect() called with same capacity

        # To correlate: look at the size in pmap vs your -XX:MaxDirectMemorySize
        pmap -x $PID | awk '$3 > 51200 && $6 ~ /anon/' | head -20
        # Shows anonymous regions > 50MB
        """);
    }

    // =========================================================================
    // SECTION 5: jemalloc — Native Heap Profiler for JNI/Native Code
    // =========================================================================
    static void printJemallocGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 5: jemalloc — Profile Native malloc() Allocations on RHEL
        ══════════════════════════════════════════════════════════════════════

        Use when: NMT says JVM uses 8GB but /proc RSS shows 12GB
                  → 4GB is from JNI native code (malloc/calloc, not JVM)

        ─── Install jemalloc on RHEL ────────────────────────────────────────
        sudo dnf install jemalloc jemalloc-devel     # RHEL 8/9

        # Or compile from source:
        wget https://github.com/jemalloc/jemalloc/releases/latest/download/jemalloc-5.3.0.tar.bz2
        tar xjf jemalloc-5.3.0.tar.bz2 && cd jemalloc-5.3.0
        ./configure --enable-prof && make && sudo make install

        ─── Run JVM with jemalloc profiling ─────────────────────────────────
        # LD_PRELOAD replaces glibc malloc with jemalloc
        LD_PRELOAD=/usr/lib64/libjemalloc.so \\
        MALLOC_CONF="prof:true,prof_leak:true,lg_prof_interval:30,lg_prof_sample:17" \\
        java -Xmx8g -jar app.jar

        # MALLOC_CONF options:
        # prof:true             → enable heap profiling
        # prof_leak:true        → dump on exit showing leaked allocations
        # lg_prof_interval:30   → dump every 2^30 = 1GB allocated
        # lg_prof_sample:17     → sample every 2^17 = 128KB allocated

        ─── Generate and read heap profile ──────────────────────────────────
        # Profile files appear in /tmp as heap.PID.N.prof
        ls /tmp/heap.*.prof

        # Convert to human-readable:
        jeprof --show_bytes --pdf \\
            /usr/bin/java /tmp/heap.12345.100.prof > /tmp/native_heap.pdf

        # Or text summary:
        jeprof --show_bytes --text \\
            /usr/bin/java /tmp/heap.12345.100.prof | head -30

        # Sample output:
        # Total: 4096.0 MB
        # 1024.0  25.0%  25.0%   1024.0  25.0%  JNI_CreateJavaVM
        #  512.0  12.5%  37.5%    512.0  12.5%  Java_io_FileInputStream_read
        #  256.0   6.3%  43.8%    256.0   6.3%  Java_java_util_zip_Inflater_inflate
        # → Inflater holding 256MB → not releasing compressed data!

        ─── Flame graph from jemalloc ───────────────────────────────────────
        jeprof --collapsed /usr/bin/java /tmp/heap.*.prof | \\
            /opt/FlameGraph/flamegraph.pl > /tmp/native_flame.svg

        ─── Live dump of running process ────────────────────────────────────
        # Trigger dump of currently running process:
        kill -USR1 $PID   # sends SIGUSR1 → jemalloc dumps current profile

        ─── tcmalloc (Google alternative) ───────────────────────────────────
        # Alternative: gperftools tcmalloc
        sudo dnf install gperftools-libs

        LD_PRELOAD=/usr/lib64/libprofiler.so \\
        HEAPPROFILE=/tmp/native_heap \\
        HEAP_PROFILE_ALLOCATION_INTERVAL=104857600 \\  # every 100MB
        java -Xmx8g -jar app.jar

        # Analyze:
        pprof --pdf /usr/bin/java /tmp/native_heap.0001.heap > /tmp/native.pdf
        """);
    }

    // =========================================================================
    // SECTION 6: async-profiler malloc Mode — Native Allocation Flame Graph
    // =========================================================================
    static void printAsyncProfilerNative() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 6: async-profiler — Native malloc Flame Graph on RHEL
        ══════════════════════════════════════════════════════════════════════

        # async-profiler can hook into malloc/free via LD_PRELOAD
        # Shows both Java and native allocations in one flame graph

        ─── Run with native allocation tracing ──────────────────────────────
        # Method 1: attach to running JVM
        /opt/async-profiler/bin/asprof -e nativealloc -d 30 \\
            -f /tmp/native_alloc.html $PID

        # Method 2: start with JVM (full lifecycle)
        java -agentpath:/opt/async-profiler/lib/libasyncProfiler.so=\\
             start,event=nativealloc,file=/tmp/native_alloc.html \\
             -Xmx8g -jar app.jar

        # What you see in the flame graph:
        # Wide C/C++ frames = native allocations from JNI code
        # Wide Java frames = Java-land allocations (via Unsafe/ByteBuffer)
        # malloc/calloc/realloc = raw native allocator calls

        ─── malloc profiling via LD_PRELOAD ─────────────────────────────────
        # async-profiler's LD_PRELOAD wrapper intercepts all malloc calls:
        LD_PRELOAD=/opt/async-profiler/lib/libasyncProfiler.so \\
        java -agentpath:/opt/async-profiler/lib/libasyncProfiler.so=\\
             start,event=nativealloc \\
             -Xmx8g -jar app.jar

        # Stop and dump:
        /opt/async-profiler/bin/asprof stop -f /tmp/native.jfr $PID

        ─── Reading native allocation flame graph ────────────────────────────
        # Frames to look for (indicate large native allocations):
        #   InflaterInputStream → decompress buffer
        #   DeflaterOutputStream → compress buffer
        #   ZipFile.open → ZIP file reading
        #   Unsafe.allocateMemory → explicit off-heap
        #   DirectByteBuffer.malloc → allocateDirect()
        #   NewJNILocalRef → JNI handle table growing
        #   PersistentHashMap → external library with native backing
        """);
    }

    // =========================================================================
    // SECTION 7: Thread Stack Native Memory
    // =========================================================================
    static void printThreadStackGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 7: Thread Stack Native Memory — Often Overlooked
        ══════════════════════════════════════════════════════════════════════

        # Each Java thread has a native stack (off-heap, not in Xmx)
        # Thread stack = reserved in virtual address space, committed on use

        ─── Stack size calculation ───────────────────────────────────────────
        Default stack sizes:
          Client JVM: -Xss512k  (512 KB per thread)
          Server JVM: -Xss1m    (1 MB per thread)
          Java 21+:   -Xss512k  (default)

        For 200 threads:
          200 × 512k = 100 MB committed (actual RAM used for active frames)
          200 × 512k = 100 MB reserved  (virtual address space)

        # NMT shows this:
        # Thread (reserved=102400KB, committed=6400KB)
        #         ^ 200 × 512k   ^ ~32KB committed per thread (shallow calls)

        ─── Finding thread count and stack usage ────────────────────────────
        # Count threads:
        jcmd $PID Thread.print | grep "^\\\"" | wc -l

        # Or from /proc:
        ls /proc/$PID/task | wc -l    # Linux kernel thread count
        grep "^Threads:" /proc/$PID/status

        # Stack sizes per thread (in /proc):
        for TID in /proc/$PID/task/*; do
            grep "VmStk" $TID/status 2>/dev/null
        done | awk '{sum+=$2} END{print "Total stack:", sum/1024, "MB"}'

        ─── Reduce stack memory for ULL (many pinned threads) ───────────────
        # If you have 50 pinned trading threads and 200 utility threads:
        # Separate their stack sizes:

        # Minimal stack for utility/IO threads (64k is enough for simple tasks):
        Thread t = new Thread(null, utilityTask, "util-1", 64 * 1024);

        # Deep-call threads (serialization, logging) need more:
        Thread t = new Thread(null, loggerTask, "logger", 256 * 1024);

        # Hot path threads: minimal stack = fewer pages to fault
        # JVM default 512k reserves 512k of VA space even if only 4k used
        # Explicit small stack: saves VA space on systems with 1000+ threads

        ─── Virtual Thread stack (Java 21+) ────────────────────────────────
        # Virtual threads (Project Loom) use heap memory for stacks,
        # not native memory! Their stacks are stored as Java arrays.

        // Create virtual thread (stack on heap, not native):
        Thread vt = Thread.ofVirtual().start(task);
        // 1 million virtual threads ≈ heap memory, not 1M × 512k native

        // For ULL trading: virtual threads are NOT recommended
        // They're for high-concurrency IO, not deterministic latency
        // Stick with platform (OS) threads + pinning for hot path
        """);
    }

    // =========================================================================
    // SECTION 8: Metaspace / Code Cache Native Memory
    // =========================================================================
    static void printMetaspaceCodeCache() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 8: Metaspace and Code Cache — Native Memory Management
        ══════════════════════════════════════════════════════════════════════

        ─── Metaspace ────────────────────────────────────────────────────────
        # Metaspace = class metadata, bytecodes, constant pools (off-heap)
        # Grows as new classes are loaded. Can grow without bound if:
        #   - Dynamic class generation (reflection proxies, Groovy, etc.)
        #   - Custom classloaders not unloaded
        #   - Hot-deploy without proper isolation

        # Configure:
        java -XX:MetaspaceSize=256m      # initial commitment (avoids early resize)
        java -XX:MaxMetaspaceSize=512m   # hard cap (prevents unbounded growth)

        # Monitor:
        jcmd $PID VM.native_memory summary scale=MB | grep -A3 "Class"

        # Sample output showing metaspace leak:
        # Class (reserved=1088MB, committed=450MB)  ← committed grew from 72MB!
        #   (classes #98765)                         ← 98K classes loaded

        # Normal class count: 5000-15000 at startup
        # Growing class count: dynamic proxy or hot-deploy leak

        # Diagnose:
        # 1. Histogram of classloaders:
        jcmd $PID GC.class_histogram | grep -i "classloader\\|proxy\\|$$"

        # 2. Count loaded classes:
        jstat -class $PID 1000 10
        # Loaded column should plateau after warmup

        # 3. Force unload of dead classes (diagnostic only):
        jcmd $PID GC.run

        ─── Code Cache ──────────────────────────────────────────────────────
        # Code cache = JIT-compiled native code (off-heap)
        # When full: JVM falls back to interpreted mode → 10-100x slower!

        # Default: -XX:ReservedCodeCacheSize=240m  (Java 17+)
        # For ULL: increase to ensure hot methods stay compiled:
        java -XX:ReservedCodeCacheSize=512m -jar app.jar

        # Monitor code cache:
        jcmd $PID VM.native_memory summary | grep -A3 "Code"
        # Code (reserved=512MB, committed=160MB)   ← using 160MB of 512MB

        # Alert if committed approaches reserved:
        # → Add -XX:ReservedCodeCacheSize=1g
        # → Or reduce: -XX:+UseCodeCacheFlushing (evict old compiled code)

        # Diagnostic: Is code cache full?
        jinfo -flag ReservedCodeCacheSize $PID
        jstat -compiler $PID     # CodeCache column

        # If "CodeCache is full" appears in logs:
        #   → JVM stops compiling new methods
        #   → Existing compiled methods stay compiled
        #   → New hot paths run interpreted (disaster for latency)
        # Fix: -XX:ReservedCodeCacheSize=1g
        """);
    }

    // =========================================================================
    // SECTION 9: BufferPoolMXBean — Track Direct & Mapped Buffers
    // =========================================================================
    static class BufferPoolMonitor {

        static void printBufferPools() {
            System.out.println("\n=== Buffer Pool MXBean (Direct + Mapped ByteBuffers) ===");

            List<BufferPoolMXBean> pools =
                ManagementFactory.getPlatformMXBeans(BufferPoolMXBean.class);

            for (BufferPoolMXBean pool : pools) {
                System.out.printf("  %-35s count=%,6d  capacity=%,8d KB  used=%,8d KB%n",
                    pool.getName(),
                    pool.getCount(),
                    pool.getTotalCapacity() / 1024,
                    pool.getMemoryUsed()    / 1024);
            }

            // Summary
            long totalDirectMB = pools.stream()
                .filter(p -> p.getName().equals("direct"))
                .mapToLong(BufferPoolMXBean::getMemoryUsed)
                .sum() / (1024 * 1024);
            long totalMappedMB = pools.stream()
                .filter(p -> p.getName().equals("mapped"))
                .mapToLong(BufferPoolMXBean::getMemoryUsed)
                .sum() / (1024 * 1024);

            System.out.printf("  → Total direct native: %d MB%n",  totalDirectMB);
            System.out.printf("  → Total mapped native: %d MB%n", totalMappedMB);
        }

        // ── Allocate and demonstrate tracking ────────────────────────────
        static void demo() {
            System.out.println("\n=== Demo: DirectByteBuffer creates native memory ===");

            printBufferPools();

            // Allocate 10 × 1MB direct buffers
            List<ByteBuffer> bufs = new ArrayList<>();
            for (int i = 0; i < 10; i++) {
                bufs.add(ByteBuffer.allocateDirect(1024 * 1024)); // 1MB each
            }

            System.out.println("\n  After allocating 10 × 1MB DirectByteBuffers:");
            printBufferPools();
            // 'direct' pool should show count=10, capacity≈10MB

            // Write data (commits physical pages)
            for (ByteBuffer b : bufs) {
                b.putLong(0, System.nanoTime()); // touch the buffer
            }

            // Let buffers go out of scope (GC will clean them up eventually)
            bufs.clear();
            System.out.println("\n  After clearing references (GC will free on next collection):");
            System.out.println("  Note: native memory NOT freed until GC runs!");
        }
    }

    // =========================================================================
    // SECTION 10: ULL Trading — Native Memory Checklist
    // =========================================================================
    static void printULLNativeChecklist() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 10: ULL Trading — Native Memory Checklist
        ══════════════════════════════════════════════════════════════════════

        ─── Launch flags for native memory safety ────────────────────────────
        java \\
            -XX:NativeMemoryTracking=summary       # monitor NMT
            -XX:MaxDirectMemorySize=4g             # cap DirectByteBuffer
            -XX:MetaspaceSize=256m                 # pre-commit metaspace
            -XX:MaxMetaspaceSize=512m              # cap metaspace
            -XX:ReservedCodeCacheSize=512m         # large JIT cache
            -Xss512k                               # thread stack size
            -jar app.jar

        ─── Pre-startup checks ───────────────────────────────────────────────
        □ -XX:NativeMemoryTracking=summary enabled
        □ -XX:MaxDirectMemorySize set (prevents unbounded growth)
        □ -XX:MaxMetaspaceSize set (prevents classloader leak OOM)
        □ /proc/<PID>/status RSS baseline taken
        □ BufferPoolMXBean baseline (direct=0MB at startup)

        ─── During warm-up ───────────────────────────────────────────────────
        □ jcmd $PID VM.native_memory baseline   (take after warm-up complete)
        □ Thread count stable (not growing after startup)
        □ Metaspace committed stable (class loading done)
        □ Code cache growing up to ~80% of max (JIT compiling hot paths)
        □ Direct ByteBuffers allocated, NOT growing after init

        ─── Hot path (trading hours) ─────────────────────────────────────────
        □ RSS stable (no growth = no native leak)
        □ jcmd VM.native_memory summary.diff clean (no growing categories)
        □ Thread count stable (no thread leak)
        □ Code cache committed < 80% of reserved
        □ Direct ByteBuffer count stable

        ─── Common native memory issues in trading systems ───────────────────
        Issue                           Root Cause                  Fix
        ──────────────────────────────  ──────────────────────────  ──────────────────────
        RSS grows 1GB/hour              DirectBB not freed          Explicit invokeCleaner()
        "Direct buffer memory" OOM      MaxDirectMemorySize not set -XX:MaxDirectMemorySize=4g
        "Metaspace OOM"                 Classloader leak            Fix lifecycle, set MaxMetaspace
        "CodeCache full" in logs        JIT cache overflow          -XX:ReservedCodeCacheSize=1g
        Thread count growing            Thread pool leak            Fix thread pool lifecycle
        RSS >> Xmx + Metaspace          JNI native code leaking     jemalloc profiling
        mmap regions growing            MappedByteBuffer not closed FileChannel.close() explicitly
        Shared memory growing           SHM not cleaned up          shm_unlink or /dev/shm cleanup

        ─── Monitoring script for RHEL production ───────────────────────────
        #!/bin/bash
        # native_monitor.sh — run every 5 minutes via cron
        PID=$(pgrep -f trading-app.jar)
        TS=$(date +%Y-%m-%dT%H:%M:%S)

        RSS=$(grep VmRSS /proc/$PID/status | awk '{print $2/1024}')
        THREADS=$(grep Threads /proc/$PID/status | awk '{print $2}')
        NMT=$(jcmd $PID VM.native_memory summary scale=MB 2>/dev/null | \\
              grep "Total:" | awk '{print $3}' | tr -d 'MB,')
        DIRECT=$(jcmd $PID VM.native_memory summary scale=MB 2>/dev/null | \\
                 grep "Internal" | awk '{print $3}' | tr -d 'MB,')

        echo "$TS,RSS=${RSS}MB,Threads=$THREADS,NMT=${NMT}MB,Direct=${DIRECT}MB" \\
            >> /var/log/trading/native_mem.csv

        # Alert if RSS > threshold:
        if [ "${RSS%.*}" -gt 20000 ]; then
            echo "ALERT: RSS=${RSS}MB exceeds 20GB" | mail -s "Native Memory Alert" ops@trading.com
        fi

        ─── RSS formula (expected values) ───────────────────────────────────
        Expected RSS = Heap + Metaspace + CodeCache + ThreadStacks + GCMeta + DirectBB

        Example (8GB heap, 200 threads):
          Heap:         8192 MB  (-Xmx8g, AlwaysPreTouch)
          Metaspace:     256 MB  (-XX:MaxMetaspaceSize=512m, typical usage)
          Code cache:    160 MB  (JIT compiled methods)
          Thread stacks:  100 MB  (200 × 512k)
          GC metadata:    512 MB  (ZGC internal)
          Direct buffers: 512 MB  (ByteBuffer.allocateDirect pools)
          JVM internals:   64 MB  (symbols, handles)
          ─────────────────────────────
          Expected RSS:  9796 MB ≈ 10 GB

        If actual RSS >> 10 GB → investigate with NMT diff + jemalloc profiling
        """);
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {
        System.out.println("=".repeat(70));
        System.out.println("  Java Native Memory Analysis on RHEL — Complete Guide");
        System.out.println("=".repeat(70));

        // Print all command-line guides
        printNMTSetupAndCommands();
        printProcMapsGuide();

        // Run live demos
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 3: Programmatic Native Memory Monitoring — Live Demo
        ══════════════════════════════════════════════════════════════════════""");

        ProgrammaticNativeMonitor.printNativeMemorySummary();
        ProgrammaticNativeMonitor.runDemo();

        // Buffer pool
        BufferPoolMonitor.demo();

        // Rest of guide
        printDirectByteBufferMonitoring();
        printJemallocGuide();
        printAsyncProfilerNative();
        printThreadStackGuide();
        printMetaspaceCodeCache();
        printULLNativeChecklist();

        // Final summary
        System.out.println("\n" + "=".repeat(70));
        System.out.println("  Quick Reference: Native Memory Analysis Commands");
        System.out.println("=".repeat(70));
        System.out.println("""
          ENABLE  : java -XX:NativeMemoryTracking=summary -jar app.jar
          SNAPSHOT: jcmd $PID VM.native_memory summary scale=MB
          BASELINE: jcmd $PID VM.native_memory baseline
          DIFF    : jcmd $PID VM.native_memory summary.diff scale=MB
          PROCSTAT: cat /proc/$PID/status | grep -E "VmRSS|VmSize|Threads"
          PMAP    : pmap -x $PID | sort -rn -k3 | head -20
          DIRECTBB: (see BufferPoolMXBean in code)
          NATIVELK: LD_PRELOAD=libjemalloc.so + MALLOC_CONF=prof:true
          THREADS : jcmd $PID Thread.print | grep "^\\\"" | wc -l
          METASP  : jstat -class $PID 1000   # watch Loaded column
          CODECACH: jcmd $PID VM.native_memory summary | grep Code
        """);

        System.out.printf("Java: %s | PID: %d | RSS: %sMB%n",
            System.getProperty("java.version"),
            ProcessHandle.current().pid(),
            ProgrammaticNativeMonitor.getProcessRSSMB() > 0
                ? String.valueOf(ProgrammaticNativeMonitor.getProcessRSSMB())
                : "N/A (not Linux)");
        System.out.println("✅ Complete.");
    }
}

