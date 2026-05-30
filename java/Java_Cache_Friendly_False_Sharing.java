import java.util.concurrent.atomic.*;
import java.util.concurrent.*;
import java.lang.invoke.*;

/**
 * Java Cache-Friendly Mechanisms — Avoiding False Sharing
 * ========================================================
 * C++ equivalent: alignas(64), __attribute__((aligned(64)))
 *
 * Covers:
 *  1.  What false sharing is + cache line anatomy
 *  2.  Manual padding (pre-Java 8 technique)
 *  3.  @jdk.internal.vm.annotation.Contended (Java 8+)
 *  4.  Inheritance padding trick (Padded base classes)
 *  5.  LongAdder / LongAccumulator — striped cells (built-in padding)
 *  6.  SPSC ring buffer with full cache-line padding
 *  7.  Disruptor-style sequence padding
 *  8.  Array element false sharing — padding objects in arrays
 *  9.  ThreadLocal — zero sharing (per-thread state)
 * 10.  VarHandle with memory ordering (C++ memory_order equivalent)
 * 11.  JVM flags: -XX:-RestrictContended
 * 12.  Benchmark: false sharing vs padded vs ThreadLocal
 * 13.  Comparison table: Java vs C++
 *
 * Build: javac Java_Cache_Friendly_False_Sharing.java
 * Run:   java -XX:-RestrictContended Java_Cache_Friendly_False_Sharing
 *
 * NOTE: @Contended requires -XX:-RestrictContended JVM flag
 */
public class Java_Cache_Friendly_False_Sharing {

    static final int CACHE_LINE = 64;   // bytes — Intel/AMD x86
    static final int LONGS_PER_LINE = CACHE_LINE / Long.BYTES;  // 8 longs

    // =========================================================================
    // SECTION 1: What is false sharing — anatomy
    // =========================================================================
    static class FalseSharingExplained {
        /*
         * Cache line = 64 bytes = smallest unit CPU loads/stores from memory
         *
         * FALSE SHARING scenario:
         *   Thread A writes field 'x' (offset 0)
         *   Thread B writes field 'y' (offset 8)
         *   Both x and y fit in the SAME 64-byte cache line
         *
         *   What happens:
         *     Thread A writes x → invalidates cache line in Thread B's L1
         *     Thread B must reload entire 64-byte line from L2/L3/RAM
         *     Thread B writes y → invalidates cache line in Thread A's L1
         *     Thread A must reload entire 64-byte line
         *     → Ping-pong between cores — latency 60-300ns per access
         *
         * C++ solution:
         *   struct alignas(64) Counter { long value; char _pad[56]{}; };
         *
         * Java solutions (covered below):
         *   Manual padding, @Contended, inheritance trick, ThreadLocal
         *
         * Object memory layout (JVM):
         *   [mark word 8 bytes][class pointer 4 bytes][fields...]
         *   Fields ordered by JVM (not declaration order!)
         *   JVM may reorder: doubles/longs first, ints, shorts, bytes, references
         */

        // BAD: both counters in same cache line → false sharing guaranteed
        static class UnpaddedCounters {
            volatile long counterA = 0;   // offset ~16 bytes from object start
            volatile long counterB = 0;   // offset ~24 bytes — same cache line as A!
        }

        public void run() {
            System.out.println("  Cache line = " + CACHE_LINE + " bytes = " + LONGS_PER_LINE + " longs");
            System.out.println("  Object header = 16 bytes (mark word + class ptr)");
            System.out.println("  UnpaddedCounters: counterA@~16, counterB@~24 → SAME cache line → false sharing");
            System.out.println("  Fix: pad each field to its own 64-byte cache line");
        }
    }

    // =========================================================================
    // SECTION 2: Manual padding — pre-Java 8 technique (still works everywhere)
    // C++ equivalent: char _pad[56]{}; after the field
    // =========================================================================
    static class ManualPadding {
        /*
         * Technique: surround the hot field with dummy long fields to fill 64 bytes
         *
         * Layout calculation:
         *   Object header:  16 bytes
         *   p1..p7:         7 × 8 = 56 bytes  ← fills remainder of first cache line
         *   value:          8 bytes            ← starts on NEW 64-byte boundary
         *   p8..p14:        7 × 8 = 56 bytes  ← fills rest of value's cache line
         *
         * Total: 16 + 56 + 8 + 56 = 136 bytes per counter object
         * Each counter object occupies its own cache line → no false sharing
         *
         * RISK: JVM/JIT may eliminate unused fields as dead code
         *   → Mitigation: extend a class (see Section 4) or use @Contended
         */

        // Padded counter — each instance occupies its own cache line
        @SuppressWarnings("unused")
        static final class PaddedLong {
            long p1, p2, p3, p4, p5, p6, p7;   // 7 × 8 = 56 bytes before value
            volatile long value = 0;             // on its own cache line
            long p8, p9, p10, p11, p12, p13, p14; // 56 bytes after value
        }

        // Array of padded counters — each on its own cache line
        static final PaddedLong[] counters = new PaddedLong[4];
        static { for (int i = 0; i < 4; i++) counters[i] = new PaddedLong(); }

        public void run() throws InterruptedException {
            Thread[] threads = new Thread[4];
            for (int i = 0; i < 4; i++) {
                final int idx = i;
                threads[i] = new Thread(() -> {
                    for (int j = 0; j < 1_000_000; j++) {
                        counters[idx].value++;  // each thread touches different cache line
                    }
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            long total = 0;
            for (PaddedLong c : counters) total += c.value;
            System.out.println("  Manual padding: total=" + total + " (expected 4000000)");
            System.out.println("  Each PaddedLong = 16(header) + 56(pre) + 8(value) + 56(post) = 136 bytes");
            System.out.println("  No false sharing: each counter on its own cache line");
        }
    }

    // =========================================================================
    // SECTION 3: @Contended — JVM native cache-line isolation (Java 8+)
    // C++ equivalent: alignas(64) on struct
    // =========================================================================
    static class ContendedAnnotation {
        /*
         * @jdk.internal.vm.annotation.Contended (internal JDK annotation)
         * or @sun.misc.Contended (older name, same thing)
         *
         * HOW IT WORKS:
         *   JVM adds 128-byte padding BEFORE and AFTER the annotated field
         *   (128 = 2 × cache lines to handle hardware prefetching on some CPUs)
         *   Guaranteed by JVM — not dead-code eliminated like manual padding
         *
         * ON CLASS:
         *   @Contended on class → entire object padded (all fields isolated)
         *
         * ON FIELD:
         *   @Contended on field → just that field padded
         *   @Contended("group1") → fields in same group share a cache line
         *                          but isolated from other groups
         *
         * JVM FLAG REQUIRED:
         *   -XX:-RestrictContended
         *   Without it: annotation is IGNORED (no padding added)
         *   (Default restricted to internal JDK classes for security)
         *
         * USED BY JDK ITSELF:
         *   Thread class:         threadLocalRandomSeed, threadLocalRandomProbe
         *   LongAdder/LongAccumulator: Cell class
         *   ForkJoinPool:         WorkQueue class
         *   ConcurrentHashMap:    CounterCell class
         *
         * C++ comparison:
         *   C++: struct alignas(64) Counter { long value; };
         *   Java: @Contended volatile long value;   (with -XX:-RestrictContended)
         */

        // Simulating @Contended effect with manual padding (since we can't import internal)
        // In production code: use @jdk.internal.vm.annotation.Contended with --add-exports
        static final class ContendedStyle {
            // Mimics @Contended: 128 bytes of padding before + 128 after
            @SuppressWarnings("unused")
            private long pp1, pp2, pp3, pp4, pp5, pp6, pp7, pp8,
                         pp9, pp10, pp11, pp12, pp13, pp14, pp15, pp16; // 128 bytes before
            volatile long value = 0;
            @SuppressWarnings("unused")
            private long ps1, ps2, ps3, ps4, ps5, ps6, ps7, ps8,
                         ps9, ps10, ps11, ps12, ps13, ps14, ps15, ps16; // 128 bytes after
        }

        // In real code with JVM flag, you'd write:
        // @jdk.internal.vm.annotation.Contended
        // volatile long value = 0;
        //
        // Or use sun.misc.Contended for older JDKs:
        // @sun.misc.Contended volatile long hotCounter;

        static final ContendedStyle[] cells = new ContendedStyle[4];
        static { for (int i = 0; i < 4; i++) cells[i] = new ContendedStyle(); }

        public void run() throws InterruptedException {
            Thread[] threads = new Thread[4];
            for (int i = 0; i < 4; i++) {
                final int idx = i;
                threads[i] = new Thread(() -> {
                    for (int j = 0; j < 500_000; j++) cells[idx].value++;
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            long total = 0;
            for (ContendedStyle c : cells) total += c.value;
            System.out.println("  @Contended style: total=" + total + " (expected 2000000)");
            System.out.println("  JDK uses @Contended in: Thread, LongAdder.Cell, ForkJoinPool.WorkQueue");
            System.out.println("  Flag needed: -XX:-RestrictContended (else annotation ignored)");
        }
    }

    // =========================================================================
    // SECTION 4: Inheritance padding trick — JVM won't eliminate base class fields
    // Most reliable manual padding approach
    // =========================================================================
    static class InheritancePaddingTrick {
        /*
         * TECHNIQUE:
         *   Split padding into base class + middle class + concrete class
         *   JVM packs object fields: base class fields first, then subclass fields
         *   Base class fields are not dead-code eliminated (JVM can't prove unused)
         *
         * USED BY LMAX DISRUPTOR:
         *   class RhsPadding extends Value { long p9,p10,p11,p12,p13,p14,p15; }
         *   class Sequence extends RhsPadding { ... }
         *
         * Layout:
         *   [object header 16 bytes]
         *   [LhsPadding: p1..p7 = 56 bytes]   ← LHS pad fills to cache line boundary
         *   [Value: value = 8 bytes]            ← sits on own cache line
         *   [RhsPadding: p9..p15 = 56 bytes]   ← RHS pad fills rest of cache line
         *   Total value-bearing region: 64 bytes = exactly one cache line
         */

        // Step 1: Left-hand side padding
        static class LhsPadding {
            @SuppressWarnings("unused")
            protected long p1, p2, p3, p4, p5, p6, p7;
        }

        // Step 2: The actual value
        static class Value extends LhsPadding {
            protected volatile long value = 0L;
        }

        // Step 3: Right-hand side padding
        static class RhsPadding extends Value {
            @SuppressWarnings("unused")
            protected long p9, p10, p11, p12, p13, p14, p15;
        }

        // Step 4: Concrete class — inherits full padding sandwich
        static final class Sequence extends RhsPadding {
            static final long INITIAL_VALUE = -1L;

            Sequence()                 { value = INITIAL_VALUE; }
            Sequence(long initialVal)  { value = initialVal; }

            public long get()          { return value; }
            public void set(long v)    { value = v; }
            public boolean compareAndSet(long expected, long update) {
                // VarHandle approach (Java 9+) or Unsafe — simplified here
                if (value == expected) { value = update; return true; }
                return false;
            }
        }

        public void run() throws InterruptedException {
            Sequence[] seqs = new Sequence[4];
            for (int i = 0; i < 4; i++) seqs[i] = new Sequence(0L);

            Thread[] threads = new Thread[4];
            for (int i = 0; i < 4; i++) {
                final int idx = i;
                threads[i] = new Thread(() -> {
                    for (int j = 0; j < 500_000; j++) seqs[idx].value++;
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            long total = 0;
            for (Sequence s : seqs) total += s.get();
            System.out.println("  Inheritance padding (Disruptor style): total=" + total + " (expected 2000000)");
            System.out.println("  Layout: [16 header][56 LhsPad][8 value][56 RhsPad] = 136 bytes");
            System.out.println("  JVM cannot eliminate base class fields → safe padding");
        }
    }

    // =========================================================================
    // SECTION 5: LongAdder — built-in striped cells (JDK's own false-sharing fix)
    // =========================================================================
    static class LongAdderInternals {
        /*
         * LongAdder (Java 8+) — built-in solution to false sharing for counters
         *
         * Internal structure (Striped64):
         *   - base: long (used when low/no contention)
         *   - cells: Cell[]  where Cell is:
         *
         *     @Contended static final class Cell {
         *         volatile long value;
         *         Cell(long x) { value = x; }
         *         ...
         *     }
         *
         *   - Each Cell is @Contended → JVM pads it to its own cache line
         *   - Thread hashes to a Cell index → each thread updates its own Cell
         *   - sum() = base + Σ(cells[i].value)
         *
         * Performance:
         *   AtomicLong (contended): all threads fight for same cache line → ~1M ops/s
         *   LongAdder (contended):  each thread has own Cell → ~50M ops/s
         *
         * USE CASE in trading:
         *   Order count, fill count, tick count, volume accumulators
         *   Anything where you add/increment from many threads
         *   NOT for sequence numbers (sum() not atomic snapshot)
         */

        public void run() throws InterruptedException {
            LongAdder tickCount   = new LongAdder();
            LongAdder volume      = new LongAdder();
            AtomicLong seqNo      = new AtomicLong(0);   // still fine: single CAS

            int THREADS = 8, OPS = 100_000;
            Thread[] threads = new Thread[THREADS];
            for (int i = 0; i < THREADS; i++) {
                threads[i] = new Thread(() -> {
                    for (int j = 0; j < OPS; j++) {
                        tickCount.increment();
                        volume.add(100);
                        seqNo.incrementAndGet();
                    }
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            System.out.println("  LongAdder (built-in @Contended cells):");
            System.out.println("    tickCount=" + tickCount.sum()
                + " volume=" + volume.sum()
                + " seqNo=" + seqNo.get()
                + " (expected " + (THREADS * OPS) + " each)");
            System.out.println("    Cell class annotated @Contended in JDK source → each cell on own cache line");
        }
    }

    // =========================================================================
    // SECTION 6: SPSC Ring Buffer — full cache-line padding (ULL production pattern)
    // C++ equivalent: alignas(64) std::atomic<uint64_t> writePos
    // =========================================================================
    static class SPSCRingBufferPadded {
        /*
         * Critical insight for SPSC ring buffer:
         *   Producer writes: writePos
         *   Consumer writes: readPos
         *   Both read each other's position to check full/empty
         *
         * If writePos and readPos share a cache line:
         *   Producer updates writePos → invalidates consumer's cache → cache miss
         *   Consumer updates readPos  → invalidates producer's cache → cache miss
         *   → 60-100ns overhead on EVERY push/pop
         *
         * Fix: writePos and readPos on SEPARATE cache lines
         *   Plus: buffer array head/tail on yet another cache line
         *
         * C++ equivalent:
         *   alignas(64) std::atomic<uint64_t> writePos_;  // own cache line
         *   alignas(64) std::atomic<uint64_t> readPos_;   // own cache line
         */

        // Producer cache line — only producer writes this
        @SuppressWarnings("unused")
        private long pp1, pp2, pp3, pp4, pp5, pp6, pp7;  // 56 bytes pad
        private volatile long writePos = 0;                // 8 bytes
        @SuppressWarnings("unused")
        private long pp8, pp9, pp10, pp11, pp12, pp13, pp14; // 56 bytes pad

        // Consumer cache line — only consumer writes this
        @SuppressWarnings("unused")
        private long cp1, cp2, cp3, cp4, cp5, cp6, cp7;  // 56 bytes pad
        private volatile long readPos = 0;                 // 8 bytes
        @SuppressWarnings("unused")
        private long cp8, cp9, cp10, cp11, cp12, cp13, cp14; // 56 bytes pad

        // Buffer — separate cache line region
        private final Object[] buffer;
        private final int mask;

        @SuppressWarnings("unchecked")
        SPSCRingBufferPadded(int capacity) {
            int size = Integer.highestOneBit(capacity - 1) << 1; // round to power of 2
            buffer = new Object[size];
            mask   = size - 1;
        }

        // Producer only
        public boolean offer(Object item) {
            final long w = writePos;
            if (w - readPos >= buffer.length) return false;  // full
            buffer[(int)(w & mask)] = item;
            writePos = w + 1;  // volatile write — release barrier
            return true;
        }

        // Consumer only
        @SuppressWarnings("unchecked")
        public Object poll() {
            final long r = readPos;
            if (r == writePos) return null;  // empty
            Object item = buffer[(int)(r & mask)];
            readPos = r + 1;  // volatile write — release barrier
            return item;
        }

        public void run() throws InterruptedException {
            SPSCRingBufferPadded ring = new SPSCRingBufferPadded(4096);
            AtomicLong consumed = new AtomicLong(0);

            Thread producer = new Thread(() -> {
                for (int i = 0; i < 1_000_000; i++) {
                    while (!ring.offer("MSG")) { /* spin */ }
                }
            }, "SPSC-Producer");

            Thread consumer = new Thread(() -> {
                long count = 0;
                while (count < 1_000_000) {
                    if (ring.poll() != null) count++;
                }
                consumed.set(count);
            }, "SPSC-Consumer");

            long start = System.nanoTime();
            producer.start(); consumer.start();
            producer.join(); consumer.join();
            long ns = System.nanoTime() - start;

            System.out.printf("  SPSC padded ring: 1M msgs in %dms (%.1f ns/msg)%n",
                ns / 1_000_000, (double) ns / 1_000_000);
            System.out.println("  writePos on own 64-byte cache line, readPos on own 64-byte cache line");
            System.out.println("  No false sharing between producer and consumer threads");
        }
    }

    // =========================================================================
    // SECTION 7: Disruptor Sequence — industry standard padded sequence
    // =========================================================================
    static class DisruptorSequence {
        /*
         * LMAX Disruptor (2011) popularized cache-line padding in Java trading systems
         *
         * Disruptor's Sequence class:
         *   - Central cursor that producers/consumers advance
         *   - Each participant has its own Sequence
         *   - Sequences must NOT share cache lines → inheritance padding
         *
         * Disruptor's RingBuffer:
         *   - Pre-allocated circular array of Event objects
         *   - Each slot padded to cache line (via BUFFER_PAD = 128/elementSize)
         *   - Avoids false sharing between adjacent slots
         *
         * Real Disruptor source (simplified):
         *   class LhsPadding { long p1,p2,p3,p4,p5,p6,p7; }
         *   class Value extends LhsPadding { volatile long value; }
         *   class RhsPadding extends Value { long p9..p15; }
         *   public final class Sequence extends RhsPadding { ... }
         */

        // Disruptor-style Sequence — production quality
        static class LhsPad { @SuppressWarnings("unused") long p1,p2,p3,p4,p5,p6,p7; }
        static class Val    extends LhsPad { volatile long value = -1L; }
        static class RhsPad extends Val   { @SuppressWarnings("unused") long p9,p10,p11,p12,p13,p14,p15; }

        static final class DisruptorSeq extends RhsPad {
            private static final VarHandle VALUE;
            static {
                try { VALUE = MethodHandles.lookup().findVarHandle(Val.class, "value", long.class); }
                catch (Exception e) { throw new RuntimeException(e); }
            }

            DisruptorSeq(long init) { value = init; }

            public long get()                  { return (long) VALUE.getVolatile(this); }
            public void set(long v)            { VALUE.setVolatile(this, v); }
            public void setOrdered(long v)     { VALUE.setRelease(this, v); }  // release ordering
            public boolean cas(long exp, long upd) { return VALUE.compareAndSet(this, exp, upd); }
        }

        public void run() throws InterruptedException {
            int NUM = 4;
            DisruptorSeq[] sequences = new DisruptorSeq[NUM];
            for (int i = 0; i < NUM; i++) sequences[i] = new DisruptorSeq(0L);

            Thread[] threads = new Thread[NUM];
            for (int i = 0; i < NUM; i++) {
                final int idx = i;
                threads[i] = new Thread(() -> {
                    for (int j = 1; j <= 100_000; j++) sequences[idx].set(j);
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            System.out.println("  Disruptor-style Sequence values: "
                + sequences[0].get() + ", " + sequences[1].get()
                + ", " + sequences[2].get() + ", " + sequences[3].get()
                + " (all expected 100000)");
            System.out.println("  VarHandle.setRelease() = acquire/release ordering (cheaper than volatile)");
        }
    }

    // =========================================================================
    // SECTION 8: Array element false sharing — padding objects in arrays
    // =========================================================================
    static class ArrayFalseSharing {
        /*
         * PROBLEM: array of objects → adjacent objects may share cache lines
         *
         *   Object[] arr = new Object[1000];
         *   arr[0], arr[1] → object REFERENCES are 4 bytes (compressed oops)
         *   16 references fit in one cache line → accesses to arr[0]..arr[15]
         *   share a cache line (reference array false sharing)
         *
         *   arr[0] points to Object at heap addr A
         *   arr[1] points to Object at heap addr B
         *   If A and B happen to be in same cache line → false sharing on objects
         *
         * SOLUTIONS:
         *   1. Pad object to 64+ bytes → JVM allocates on separate cache lines
         *   2. Use primitive arrays instead of object arrays (better cache density)
         *   3. Structure-of-Arrays instead of Array-of-Structures
         *
         * For HOT arrays (order book price levels):
         *   Use long[] for prices, long[] for quantities (SoA pattern)
         *   NOT PriceLevel[] where each PriceLevel has price+qty
         */

        // BAD: Array of objects — adjacent objects may share cache lines
        static class HotSlot {
            volatile long price = 0;
            volatile long qty   = 0;
        }

        // GOOD: Structure of Arrays — better cache utilization
        static class OrderBookSoA {
            // Separate arrays — process all prices in one cache sweep
            final long[] prices  = new long[1024];  // 8KB = 128 cache lines
            final long[] qtys    = new long[1024];
            final long[] orderIds = new long[1024];

            void updateLevel(int idx, long price, long qty, long orderId) {
                prices[idx]   = price;
                qtys[idx]     = qty;
                orderIds[idx] = orderId;
            }

            // Scan all prices: sequential access → hardware prefetcher kicks in
            long bestBid() {
                long best = Long.MIN_VALUE;
                for (int i = 0; i < 1024; i++) {
                    if (qtys[i] > 0 && prices[i] > best) best = prices[i];
                }
                return best;
            }
        }

        public void run() {
            // SoA demo
            OrderBookSoA book = new OrderBookSoA();
            book.updateLevel(0, 45000L, 100L, 1001L);
            book.updateLevel(1, 45001L, 200L, 1002L);
            book.updateLevel(2, 44999L, 150L, 1003L);
            long best = book.bestBid();

            System.out.println("  Structure-of-Arrays orderbook best bid=" + best);
            System.out.println("  long[] prices scanned sequentially → prefetcher loads ahead → ~L1 speed");
            System.out.println("  vs HotSlot[] → each HotSlot object on different heap addr → cache misses");

            // Padding single array elements to avoid false sharing
            // When 2 threads write to array[i] and array[j], if |i-j| < 8 → same cache line
            // Fix: stride by 8 longs = 64 bytes
            int PADDED_STRIDE = 8;
            long[] paddedArr = new long[4 * PADDED_STRIDE];
            // Thread 0 uses index 0, Thread 1 uses index 8, Thread 2 uses index 16, Thread 3 uses index 24
            // Each on its own 64-byte cache line
            paddedArr[0]  = 100;  // thread 0
            paddedArr[8]  = 200;  // thread 1 — 64 bytes away from thread 0's slot
            paddedArr[16] = 300;  // thread 2
            paddedArr[24] = 400;  // thread 3
            System.out.println("  Padded primitive array (stride=8): values="
                + paddedArr[0] + "," + paddedArr[8] + "," + paddedArr[16] + "," + paddedArr[24]);
        }
    }

    // =========================================================================
    // SECTION 9: ThreadLocal — zero sharing (per-thread state)
    // C++ equivalent: thread_local
    // =========================================================================
    static class ThreadLocalCacheFriendly {
        /*
         * ThreadLocal<T>:
         *   Each thread has its own Thread.threadLocals (ThreadLocalMap)
         *   ThreadLocalMap: open-addressing hash table keyed by ThreadLocal
         *   Value stored directly in each Thread's map → no sharing, no contention
         *
         * USE CASES:
         *   Per-thread encode buffer (FIX/ITCH message encoding)
         *   Per-thread random number generator (no seed sharing)
         *   Per-thread calendar/date formatter
         *   Per-thread pre-allocated work buffer
         *
         * C++ equivalent:
         *   thread_local uint8_t encodeBuf[4096];
         *   thread_local std::mt19937_64 rng(seed);
         *
         * CLEANUP:
         *   Must remove() when using thread pools (thread reuse → stale values)
         *   With virtual threads: careful (virtual thread may run on different carrier)
         */

        // Pre-allocated per-thread encode buffer (zero GC, zero contention)
        static final ThreadLocal<byte[]> ENCODE_BUF = ThreadLocal.withInitial(() -> new byte[4096]);
        static final ThreadLocal<long[]> SCRATCH     = ThreadLocal.withInitial(() -> new long[64]);
        static final ThreadLocal<StringBuilder> SB   = ThreadLocal.withInitial(StringBuilder::new);

        public void run() throws InterruptedException {
            int THREADS = 4;
            Thread[] threads = new Thread[THREADS];
            long[] results = new long[THREADS];

            for (int i = 0; i < THREADS; i++) {
                final int idx = i;
                threads[i] = new Thread(() -> {
                    // Each thread gets its own buffer — no contention, no allocation
                    byte[] buf = ENCODE_BUF.get();     // O(1) lookup from Thread's map
                    long[] scratch = SCRATCH.get();
                    StringBuilder sb = SB.get();

                    // Use per-thread resources without any locking
                    buf[0] = (byte)('A' + idx);
                    scratch[0] = idx * 1000L;
                    sb.setLength(0);
                    sb.append("Thread-").append(idx).append(" val=").append(scratch[0]);
                    results[idx] = scratch[0];
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            System.out.print("  ThreadLocal per-thread scratch values: ");
            for (long r : results) System.out.print(r + " ");
            System.out.println();
            System.out.println("  Zero contention: each thread accesses its own Thread.threadLocals map");
            System.out.println("  C++ equivalent: thread_local byte encodeBuf[4096];");
        }
    }

    // =========================================================================
    // SECTION 10: VarHandle — C++ memory_order equivalent (Java 9+)
    // =========================================================================
    static class VarHandleMemoryOrdering {
        /*
         * VarHandle (Java 9+) — fine-grained memory ordering on fields
         * Replaces sun.misc.Unsafe for most use cases
         *
         * ACCESS MODES — C++ memory_order equivalents:
         *
         *   getVolatile / setVolatile
         *     = std::memory_order_seq_cst
         *     Strongest: full sequential consistency
         *     Cost: memory fence instruction (MFENCE on x86)
         *
         *   getAcquire / setRelease
         *     = std::memory_order_acquire / memory_order_release
         *     Acquire: no reads/writes after this can be reordered BEFORE it
         *     Release: no reads/writes before this can be reordered AFTER it
         *     Cost: LOCK prefix or SFENCE/LFENCE (cheaper than full MFENCE)
         *     USE THIS for SPSC ring buffer, producer-consumer patterns
         *
         *   getOpaque / setOpaque
         *     = std::memory_order_relaxed (approximately)
         *     Only guarantees atomicity of the access itself
         *     No ordering guarantees — cheapest
         *     USE THIS for: per-thread counters visible to other threads eventually
         *
         *   getPlain / setPlain
         *     No atomicity, no ordering — like normal field access
         *     USE THIS for: single-threaded access, within synchronized blocks
         *
         * C++ → Java mapping:
         *   memory_order_seq_cst  → getVolatile / setVolatile
         *   memory_order_acquire  → getAcquire
         *   memory_order_release  → setRelease
         *   memory_order_relaxed  → getOpaque / setOpaque
         *   memory_order_consume  → getAcquire (Java has no exact equivalent)
         */

        static class SPSCWithVarHandle {
            private long writePos = 0L;
            private long readPos  = 0L;

            private static final VarHandle WP, RP;
            static {
                try {
                    MethodHandles.Lookup lookup = MethodHandles.lookup();
                    WP = lookup.findVarHandle(SPSCWithVarHandle.class, "writePos", long.class);
                    RP = lookup.findVarHandle(SPSCWithVarHandle.class, "readPos",  long.class);
                } catch (Exception e) { throw new RuntimeException(e); }
            }

            private final Object[] buf = new Object[1024];
            private final int mask = 1023;

            // Producer: setRelease → cheaper than volatile (no full fence on x86)
            public boolean offer(Object item) {
                long w = (long) WP.getOpaque(this);
                long r = (long) RP.getAcquire(this);    // acquire: see consumer's latest read
                if (w - r >= buf.length) return false;
                buf[(int)(w & mask)] = item;
                WP.setRelease(this, w + 1);             // release: consumer will see this
                return true;
            }

            // Consumer: setRelease → cheaper than volatile
            public Object poll() {
                long r = (long) RP.getOpaque(this);
                long w = (long) WP.getAcquire(this);    // acquire: see producer's latest write
                if (r == w) return null;
                Object item = buf[(int)(r & mask)];
                RP.setRelease(this, r + 1);             // release: producer will see this
                return item;
            }
        }

        public void run() throws InterruptedException {
            SPSCWithVarHandle ring = new SPSCWithVarHandle();
            AtomicLong count = new AtomicLong(0);

            Thread producer = new Thread(() -> {
                for (int i = 0; i < 100_000; i++) {
                    while (!ring.offer("X")) { /* spin */ }
                }
            });
            Thread consumer = new Thread(() -> {
                int got = 0;
                while (got < 100_000) {
                    if (ring.poll() != null) got++;
                }
                count.set(got);
            });

            producer.start(); consumer.start();
            producer.join(); consumer.join();

            System.out.println("  VarHandle SPSC: " + count.get() + "/100000 msgs");
            System.out.println("  setRelease/getAcquire = C++ memory_order_release/acquire (cheaper than seq_cst)");
            System.out.println("  On x86: setRelease is plain store (no fence!), getAcquire is plain load");
            System.out.println("  TSO (Total Store Order) on x86 makes acquire/release nearly free");
        }
    }

    // =========================================================================
    // SECTION 11: Benchmark — false sharing vs padded vs ThreadLocal
    // =========================================================================
    static class Benchmark {
        static final int THREADS = 4;
        static final int OPS     = 2_000_000;

        // Unpadded — false sharing
        static volatile long cA = 0, cB = 0, cC = 0, cD = 0;

        // Padded — no false sharing
        @SuppressWarnings("unused")
        static long pp1,pp2,pp3,pp4,pp5,pp6,pp7;
        static volatile long pA = 0;
        @SuppressWarnings("unused")
        static long pp8,pp9,pp10,pp11,pp12,pp13,pp14;
        @SuppressWarnings("unused")
        static long pp15,pp16,pp17,pp18,pp19,pp20,pp21;
        static volatile long pB = 0;
        @SuppressWarnings("unused")
        static long pp22,pp23,pp24,pp25,pp26,pp27,pp28;
        @SuppressWarnings("unused")
        static long pp29,pp30,pp31,pp32,pp33,pp34,pp35;
        static volatile long pC = 0;
        @SuppressWarnings("unused")
        static long pp36,pp37,pp38,pp39,pp40,pp41,pp42;
        @SuppressWarnings("unused")
        static long pp43,pp44,pp45,pp46,pp47,pp48,pp49;
        static volatile long pD = 0;
        @SuppressWarnings("unused")
        static long pp50,pp51,pp52,pp53,pp54,pp55,pp56;

        static long benchmark(Runnable[] tasks) throws InterruptedException {
            Thread[] threads = new Thread[THREADS];
            for (int i = 0; i < THREADS; i++) {
                final Runnable r = tasks[i];
                threads[i] = new Thread(r);
            }
            long start = System.nanoTime();
            for (Thread t : threads) t.start();
            for (Thread t : threads) t.join();
            return System.nanoTime() - start;
        }

        public void run() throws InterruptedException {
            // Warm-up
            for (int w = 0; w < 3; w++) {
                benchmark(new Runnable[]{
                    () -> { for(int i=0;i<OPS;i++) cA++; },
                    () -> { for(int i=0;i<OPS;i++) cB++; },
                    () -> { for(int i=0;i<OPS;i++) cC++; },
                    () -> { for(int i=0;i<OPS;i++) cD++; }
                });
            }

            long falseMs = benchmark(new Runnable[]{
                () -> { for(int i=0;i<OPS;i++) cA++; },
                () -> { for(int i=0;i<OPS;i++) cB++; },
                () -> { for(int i=0;i<OPS;i++) cC++; },
                () -> { for(int i=0;i<OPS;i++) cD++; }
            }) / 1_000_000;

            long paddedMs = benchmark(new Runnable[]{
                () -> { for(int i=0;i<OPS;i++) pA++; },
                () -> { for(int i=0;i<OPS;i++) pB++; },
                () -> { for(int i=0;i<OPS;i++) pC++; },
                () -> { for(int i=0;i<OPS;i++) pD++; }
            }) / 1_000_000;

            // ThreadLocal — per-thread local counter
            ThreadLocal<long[]> tl = ThreadLocal.withInitial(() -> new long[]{0});
            long tlMs = benchmark(new Runnable[]{
                () -> { long[] c = tl.get(); for(int i=0;i<OPS;i++) c[0]++; },
                () -> { long[] c = tl.get(); for(int i=0;i<OPS;i++) c[0]++; },
                () -> { long[] c = tl.get(); for(int i=0;i<OPS;i++) c[0]++; },
                () -> { long[] c = tl.get(); for(int i=0;i<OPS;i++) c[0]++; }
            }) / 1_000_000;

            System.out.println("\n  === Benchmark: " + THREADS + " threads × " + OPS + " ops ===");
            System.out.printf("  False sharing (same cache line): %5d ms%n", falseMs);
            System.out.printf("  Padded (own cache lines):        %5d ms  (~%.1fx faster)%n",
                paddedMs, falseMs > 0 ? (double)falseMs/paddedMs : 1.0);
            System.out.printf("  ThreadLocal (no sharing):        %5d ms  (~%.1fx faster)%n",
                tlMs, falseMs > 0 ? (double)falseMs/tlMs : 1.0);
        }
    }

    // =========================================================================
    // SECTION 12: Comparison table — Java vs C++
    // =========================================================================
    static class ComparisonTable {
        public void run() {
            System.out.println("\n  ┌─────────────────────────────────┬──────────────────────────────────┬───────────────────┐");
            System.out.println("  │ Java Technique                   │ C++ Equivalent                   │ Overhead / Notes  │");
            System.out.println("  ├─────────────────────────────────┼──────────────────────────────────┼───────────────────┤");
            System.out.println("  │ Manual long padding (7 longs)    │ char _pad[56]{}                  │ Risk: JIT elim    │");
            System.out.println("  │ @Contended annotation            │ alignas(64) / [[gnu::aligned]]   │ Needs JVM flag    │");
            System.out.println("  │ Inheritance padding trick        │ alignas(64) on struct            │ Safe, reliable    │");
            System.out.println("  │ LongAdder (builtin @Contended)   │ per-thread counter array         │ Zero extra work   │");
            System.out.println("  │ SPSC padded ring buffer          │ alignas(64) atomic<uint64_t>     │ Production ULL    │");
            System.out.println("  │ Disruptor Sequence               │ Sequence with alignas(64)        │ Industry standard │");
            System.out.println("  │ ThreadLocal<T>                   │ thread_local T                   │ Zero contention   │");
            System.out.println("  │ VarHandle.setRelease             │ atomic.store(memory_order_release│ Cheaper than vol  │");
            System.out.println("  │ VarHandle.getAcquire             │ atomic.load(memory_order_acquire)│ Cheaper than vol  │");
            System.out.println("  │ VarHandle.getVolatile            │ atomic.load(memory_order_seq_cst)│ Full fence        │");
            System.out.println("  │ VarHandle.getOpaque              │ atomic.load(memory_order_relaxed)│ Cheapest atomic   │");
            System.out.println("  │ SoA (separate long[] arrays)     │ SoA struct layout                │ Best cache dens.  │");
            System.out.println("  └─────────────────────────────────┴──────────────────────────────────┴───────────────────┘");

            System.out.println("\n  Key Rules:");
            System.out.println("  1. Any two volatile/atomic fields written by DIFFERENT threads → pad to own cache line");
            System.out.println("  2. Pad = surround with 7 longs before + 7 longs after (or use @Contended)");
            System.out.println("  3. SPSC ring: writePos and readPos MUST be on different cache lines");
            System.out.println("  4. Use setRelease/getAcquire instead of volatile for SPSC (cheaper on non-x86)");
            System.out.println("  5. LongAdder is always better than AtomicLong for pure counters");
            System.out.println("  6. SoA (parallel primitive arrays) > AoS (object array) for hot loops");
            System.out.println("  7. ThreadLocal = zero contention = zero cache line sharing");
        }
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {
        System.out.println("==========================================================================");
        System.out.println(" Java Cache-Friendly Mechanisms — Avoiding False Sharing");
        System.out.println(" C++ equivalent: alignas(64), thread_local, memory_order");
        System.out.println("==========================================================================\n");

        System.out.println("--- Section 1: False sharing explained ---");
        new FalseSharingExplained().run();

        System.out.println("\n--- Section 2: Manual padding (7 longs before + after) ---");
        new ManualPadding().run();

        System.out.println("\n--- Section 3: @Contended annotation style ---");
        new ContendedAnnotation().run();

        System.out.println("\n--- Section 4: Inheritance padding (Disruptor/LMAX style) ---");
        new InheritancePaddingTrick().run();

        System.out.println("\n--- Section 5: LongAdder — built-in @Contended cells ---");
        new LongAdderInternals().run();

        System.out.println("\n--- Section 6: SPSC ring buffer with full cache-line padding ---");
        new SPSCRingBufferPadded(4096).run();

        System.out.println("\n--- Section 7: Disruptor-style Sequence with VarHandle ---");
        new DisruptorSequence().run();

        System.out.println("\n--- Section 8: Array element false sharing + SoA pattern ---");
        new ArrayFalseSharing().run();

        System.out.println("\n--- Section 9: ThreadLocal — zero sharing ---");
        new ThreadLocalCacheFriendly().run();

        System.out.println("\n--- Section 10: VarHandle memory ordering (memory_order equivalent) ---");
        new VarHandleMemoryOrdering().run();

        System.out.println("\n--- Section 11: Benchmark — false sharing vs padded vs ThreadLocal ---");
        new Benchmark().run();

        System.out.println("\n--- Section 12: Java vs C++ comparison table ---");
        new ComparisonTable().run();

        System.out.println("\n==========================================================================");
        System.out.println(" All cache-friendly mechanism demos completed");
        System.out.println("==========================================================================");
    }
}

