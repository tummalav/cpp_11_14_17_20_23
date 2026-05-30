import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;

/**
 * Java: synchronized vs synchronized collections vs lock/wait-free collections
 * =============================================================================
 * Covers:
 *  1.  synchronized keyword — intrinsic monitor lock
 *  2.  Legacy synchronized collections — Vector, Hashtable, Stack
 *  3.  Collections.synchronizedXxx wrappers
 *  4.  java.util.concurrent — ConcurrentHashMap, CopyOnWriteArrayList, etc.
 *  5.  Lock-free collections (CAS-based) — underlying mechanics
 *  6.  Wait-free guarantees — AtomicXxx, SPSC ring buffer
 *  7.  Blocking queues — underlying implementations
 *  8.  Disruptor pattern — true wait-free SPSC
 *  9.  ULL decision guide
 * 10.  Complete comparison table
 *
 * Build: javac Java_Synchronized_vs_Concurrent_vs_LockFree_Collections.java
 * Run:   java  Java_Synchronized_vs_Concurrent_vs_LockFree_Collections
 */
public class Java_Synchronized_vs_Concurrent_vs_LockFree_Collections {

    // =========================================================================
    // SECTION 1: synchronized keyword — HOW it works internally
    // =========================================================================
    static class SynchronizedInternals {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Every Java object has a "monitor" (object header = mark word).
         * Mark word stores: identity hash, GC age, lock state, thread pointer.
         *
         * Lock states (biased → thin → fat, escalates under contention):
         *
         *  BIASED LOCK (no contention):
         *   - Mark word stores thread ID
         *   - Thread re-acquires with zero CAS (just compare thread ID)
         *   - Cost: ~1 ns
         *   - Removed in Java 21 (bias locking deprecated Java 15, removed 21)
         *
         *  THIN LOCK / STACK LOCK (low contention):
         *   - Mark word replaced with pointer to stack-allocated BasicLock
         *   - Uses CAS to acquire
         *   - Cost: ~5-20 ns (no OS call)
         *
         *  FAT LOCK / INFLATED (high contention):
         *   - Mark word points to ObjectMonitor (heap object)
         *   - Uses OS mutex (pthread_mutex on Linux)
         *   - Waiting threads enter _WaitSet or _EntryList
         *   - Cost: 1-10 µs (OS context switch)
         *
         * JVM bytecode:
         *   monitorenter  → acquire lock
         *   monitorexit   → release lock
         */

        private int value = 0;
        private final Object lock = new Object();

        // Method-level: locks on 'this' (entire object as monitor)
        public synchronized void increment() { value++; }

        // Block-level: locks on explicit object (finer granularity)
        public void incrementBlock() {
            synchronized (lock) { value++; }
        }

        // Class-level: locks on Class object (shared across all instances)
        public static synchronized void staticMethod() { /* ... */ }

        public void run() throws InterruptedException {
            Thread[] t = new Thread[20];
            for (int i = 0; i < 20; i++) t[i] = new Thread(this::increment);
            for (Thread th : t) th.start();
            for (Thread th : t) th.join();
            System.out.println("  synchronized result = " + value + " (expected 20)");
            System.out.println("  Lock escalation: biased(removed Java21) → thin(CAS) → fat(OS mutex)");
        }
    }

    // =========================================================================
    // SECTION 2: Legacy synchronized collections — Vector, Hashtable, Stack
    // =========================================================================
    static class LegacySynchronizedCollections {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Vector:    extends AbstractList, every method is synchronized on 'this'
         *            → coarse-grained locking: only one thread can read OR write
         *            → iterator is NOT thread-safe (must externally synchronize)
         *
         * Hashtable: every method synchronized on 'this'
         *            → null keys/values NOT allowed (unlike HashMap)
         *            → single lock for entire table
         *
         * Stack:     extends Vector → same coarse locking
         *
         * WHY AVOID:
         *  - Full method-level synchronization = massive contention
         *  - Two consecutive synchronized calls not atomic:
         *      if (!table.containsKey(k)) table.put(k, v)  ← RACE CONDITION
         *  - Superseded by ConcurrentHashMap, CopyOnWriteArrayList
         */

        @SuppressWarnings("UseOfObsoleteCollectionType")
        public void run() throws InterruptedException {
            Vector<Integer> vector = new Vector<>();
            Hashtable<String, Integer> table = new Hashtable<>();
            Stack<String> stack = new Stack<>();

            // Vector — all methods synchronized on 'this'
            Thread[] writers = new Thread[10];
            for (int i = 0; i < 10; i++) {
                final int val = i;
                writers[i] = new Thread(() -> vector.add(val));
                writers[i].start();
            }
            for (Thread t : writers) t.join();
            System.out.println("  Vector size=" + vector.size() + " (expected 10) — coarse lock on 'this'");

            // Hashtable
            table.put("SPY", 450);
            table.put("QQQ", 380);
            System.out.println("  Hashtable: " + table + " — every method locks whole table");

            // Stack
            stack.push("order1"); stack.push("order2");
            System.out.println("  Stack top=" + stack.peek() + " — extends Vector, same coarse lock");

            System.out.println("  ⚠ Legacy: avoid in new code — use ConcurrentHashMap/CopyOnWriteArrayList");
        }
    }

    // =========================================================================
    // SECTION 3: Collections.synchronizedXxx wrappers
    // =========================================================================
    static class SynchronizedWrappers {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Collections.synchronizedList(list) returns SynchronizedList:
         *   - Wraps original list
         *   - Every method delegates to wrapped list inside synchronized(mutex) block
         *   - mutex = the wrapper object itself (or custom mutex passed to constructor)
         *
         * SAME PROBLEM as Vector:
         *   - Compound operations not atomic:
         *       list.get(list.size() - 1)   ← two separate synchronized calls = race
         *   - Iteration MUST be manually synchronized:
         *       synchronized(syncList) { for (E e : syncList) { ... } }
         *   - No read concurrency: readers block each other
         *
         * Wrappers available:
         *   synchronizedList(List)
         *   synchronizedSet(Set)
         *   synchronizedSortedSet(SortedSet)
         *   synchronizedMap(Map)
         *   synchronizedSortedMap(SortedMap)
         *   synchronizedNavigableMap(NavigableMap)
         */

        public void run() throws InterruptedException {
            List<String> rawList = new ArrayList<>();
            List<String> syncList = Collections.synchronizedList(rawList);

            Map<String, Double> rawMap = new HashMap<>();
            Map<String, Double> syncMap = Collections.synchronizedMap(rawMap);

            // Thread-safe individual operations
            Thread[] writers = new Thread[5];
            for (int i = 0; i < 5; i++) {
                final String order = "ORD-" + i;
                writers[i] = new Thread(() -> syncList.add(order));
                writers[i].start();
            }
            for (Thread t : writers) t.join();
            System.out.println("  synchronizedList size=" + syncList.size() + " (expected 5)");

            // MUST externally sync for iteration
            synchronized (syncList) {
                System.out.print("  Iteration (manually synced): ");
                for (String s : syncList) System.out.print(s + " ");
                System.out.println();
            }

            syncMap.put("SPY", 450.0);
            syncMap.put("QQQ", 380.0);
            System.out.println("  synchronizedMap: " + syncMap);
            System.out.println("  ⚠ Wrapper: readers block each other — prefer ConcurrentHashMap");
        }
    }

    // =========================================================================
    // SECTION 4: ConcurrentHashMap — segment/stripe locking + CAS
    // =========================================================================
    static class ConcurrentHashMapInternals {
        /*
         * UNDERLYING IMPLEMENTATION (Java 8+):
         * ----------------------------------------
         * Internal structure: Node<K,V>[] table  (array of linked list / tree heads)
         *
         * Java 7: 16 segments, each a ReentrantLock — one lock per segment
         * Java 8+: per-bucket CAS + synchronized on bin head only
         *
         * READ path:
         *   - No lock at all for most reads
         *   - table array is volatile → visibility guaranteed
         *   - Node.val and Node.next are volatile
         *   - Reads truly concurrent — zero blocking
         *
         * WRITE path:
         *   1. Hash key → bucket index
         *   2. If bucket empty: CAS(null → new Node) — lock-free insert
         *   3. If bucket non-empty: synchronized(head_node) — only locks that bin
         *   4. If bin length > 8: treeify to TreeBin (red-black tree), uses ReadWriteLock
         *
         * RESIZE path:
         *   - Multiple threads cooperate on transfer
         *   - ForwardingNode placed in bins being moved
         *   - Other threads help transfer (lock-free cooperation)
         *
         * computeIfAbsent / merge:
         *   - Atomic compound operations (check-then-act without race)
         *
         * size():
         *   - Uses CounterCell[] (like LongAdder — striped counter, no single CAS)
         *   - sumCount() aggregates all cells
         */

        public void run() throws InterruptedException {
            ConcurrentHashMap<String, AtomicLong> positions = new ConcurrentHashMap<>();

            // Pre-populate
            positions.put("SPY", new AtomicLong(0));
            positions.put("QQQ", new AtomicLong(0));

            // 50 threads updating different keys — no contention between keys
            Thread[] threads = new Thread[50];
            for (int i = 0; i < 50; i++) {
                final String sym = (i % 2 == 0) ? "SPY" : "QQQ";
                threads[i] = new Thread(() ->
                    positions.computeIfAbsent(sym, k -> new AtomicLong(0))
                             .incrementAndGet()
                );
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            System.out.println("  ConcurrentHashMap positions: " + positions);
            System.out.println("  Internal: per-bucket CAS (empty) or synchronized(bin_head) — readers never block");

            // Atomic compound ops
            ConcurrentHashMap<String, Long> pnl = new ConcurrentHashMap<>();
            pnl.merge("DESK-A", 1000L, Long::sum);   // atomic update-or-insert
            pnl.merge("DESK-A", 500L, Long::sum);
            pnl.merge("DESK-B", 2000L, Long::sum);
            System.out.println("  merge() (atomic): " + pnl);

            // compute — atomic read-modify-write
            pnl.compute("DESK-A", (k, v) -> v == null ? 100L : v * 2);
            System.out.println("  compute() (atomic): DESK-A=" + pnl.get("DESK-A"));
        }
    }

    // =========================================================================
    // SECTION 5: CopyOnWriteArrayList / CopyOnWriteArraySet
    // =========================================================================
    static class CopyOnWriteInternals {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Internal field: volatile Object[] array
         *
         * READ path:
         *   - No lock at all — reads snapshot of current array
         *   - getArray() returns reference to volatile array
         *   - Readers never blocked, never stale (volatile ensures visibility)
         *   - Iterator snapshots array at creation — never throws ConcurrentModificationException
         *
         * WRITE path (add/set/remove):
         *   1. Acquire ReentrantLock (one global lock)
         *   2. Copy entire array: Arrays.copyOf(current, length + 1)
         *   3. Modify the new copy
         *   4. setArray(newArray) — volatile write — atomically publishes new array
         *   5. Release lock
         *
         * COST:
         *   - Read: O(1), zero locking
         *   - Write: O(n) — full array copy
         *   - Memory: 2x array in memory during write
         *
         * USE CASES:
         *   - Read-heavy, write-rare: listener lists, observer registrations
         *   - Config/reference data updates (rare writes, many readers)
         *   - Small collections where copy cost is acceptable
         *
         * AVOID:
         *   - High write frequency (each write copies entire array)
         *   - Large collections (memory + CPU cost of copy)
         */

        public void run() throws InterruptedException {
            CopyOnWriteArrayList<String> listeners = new CopyOnWriteArrayList<>();

            // Add subscribers (writes — rare)
            listeners.add("RiskEngine");
            listeners.add("PnLEngine");
            listeners.add("ComplianceEngine");

            // 20 reader threads — ZERO blocking, snapshot iteration
            Thread[] readers = new Thread[20];
            for (int i = 0; i < 20; i++) {
                readers[i] = new Thread(() -> {
                    int count = 0;
                    for (String s : listeners) count++;  // never throws CME
                    // count silently ignored to keep output clean
                });
                readers[i].start();
            }

            // Writer adds while readers iterate — safe (copy-on-write)
            listeners.add("AuditEngine");

            for (Thread t : readers) t.join();
            System.out.println("  CopyOnWriteArrayList: " + listeners);
            System.out.println("  Internal: volatile array ref, write=full copy+ReentrantLock, read=no lock");

            // CopyOnWriteArraySet — backed by CopyOnWriteArrayList
            CopyOnWriteArraySet<String> symbols = new CopyOnWriteArraySet<>();
            symbols.add("SPY"); symbols.add("QQQ"); symbols.add("SPY"); // dedup
            System.out.println("  CopyOnWriteArraySet (dedup): " + symbols);
        }
    }

    // =========================================================================
    // SECTION 6: ConcurrentLinkedQueue / ConcurrentLinkedDeque — lock-free
    // =========================================================================
    static class ConcurrentLinkedInternals {
        /*
         * UNDERLYING IMPLEMENTATION — Michael-Scott queue (CAS-based):
         * ---------------------------------------------------------------
         * Structure: singly-linked list with volatile head and tail pointers
         *
         * Node<E>:
         *   volatile E item
         *   volatile Node<E> next
         *
         * ENQUEUE (offer):
         *   1. Create new node
         *   2. CAS(tail.next, null, newNode)  — link new node
         *   3. CAS(tail, oldTail, newNode)    — advance tail (best-effort)
         *   Retry on CAS failure (another thread raced)
         *
         * DEQUEUE (poll):
         *   1. Read head.next
         *   2. CAS(head, oldHead, head.next)  — advance head
         *   3. Return item from old head.next
         *   Retry on CAS failure
         *
         * PROPERTIES:
         *   - Lock-free: system always makes progress (at least one thread succeeds CAS)
         *   - NOT wait-free: individual thread may retry many times under high contention
         *   - size() is O(n) — traverses entire list (not O(1)!)
         *   - Unbounded — no capacity limit
         *
         * ConcurrentLinkedDeque:
         *   - Doubly-linked, two CAS pointers (head + tail)
         *   - Lock-free push/pop from both ends
         */

        public void run() throws InterruptedException {
            ConcurrentLinkedQueue<String> queue = new ConcurrentLinkedQueue<>();
            ConcurrentLinkedDeque<String> deque = new ConcurrentLinkedDeque<>();

            // MPMC — multiple producers, multiple consumers — no lock
            Thread[] producers = new Thread[5];
            Thread[] consumers = new Thread[5];
            AtomicInteger consumed = new AtomicInteger(0);

            for (int i = 0; i < 5; i++) {
                final int id = i;
                producers[i] = new Thread(() -> {
                    queue.offer("MSG-" + id);  // CAS-based, no lock
                });
                consumers[i] = new Thread(() -> {
                    String msg = queue.poll(); // CAS-based, no lock
                    if (msg != null) consumed.incrementAndGet();
                });
            }

            for (Thread t : producers) t.start();
            for (Thread t : producers) t.join();
            for (Thread t : consumers) t.start();
            for (Thread t : consumers) t.join();

            System.out.println("  ConcurrentLinkedQueue: consumed=" + consumed.get()
                + " remaining=" + queue.size() + " — Michael-Scott lock-free queue");

            // Deque operations
            deque.offerFirst("PRIORITY-ORDER");
            deque.offerLast("NORMAL-ORDER");
            System.out.println("  ConcurrentLinkedDeque head=" + deque.peekFirst()
                + " tail=" + deque.peekLast() + " — lock-free doubly-linked");
        }
    }

    // =========================================================================
    // SECTION 7: ConcurrentSkipListMap / ConcurrentSkipListSet — lock-free sorted
    // =========================================================================
    static class ConcurrentSkipListInternals {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Skip list: probabilistic sorted structure (alternative to balanced BST)
         *
         * Structure:
         *   Multiple levels of linked lists (level 0 = all nodes, higher = express lanes)
         *   Each node has volatile next[] array (one per level)
         *
         * PUT path:
         *   1. Find insertion point (traverse top levels, drop down)
         *   2. CAS(pred.next[0], null/succ, newNode) — link at bottom level
         *   3. CAS link at each upper level (probabilistic promotion)
         *   No global lock — fine-grained CAS per level
         *
         * GET path:
         *   - Lock-free traversal from top level
         *   - No CAS on read — purely volatile reads
         *
         * DELETE path:
         *   - Mark node with special marker node (logical delete)
         *   - Physically removed lazily
         *
         * COMPLEXITY: O(log n) expected for get/put/remove
         *
         * USE CASES:
         *   - Concurrent price levels in order book (sorted by price)
         *   - Priority queues needing sorted concurrent access
         *   - Range queries (headMap, tailMap, subMap — all lock-free)
         */

        public void run() throws InterruptedException {
            // Price level order book simulation
            ConcurrentSkipListMap<Double, Long> bids = new ConcurrentSkipListMap<>(Comparator.reverseOrder());
            ConcurrentSkipListMap<Double, Long> asks = new ConcurrentSkipListMap<>();

            Thread[] bidWriters = new Thread[5];
            Thread[] askWriters = new Thread[5];

            for (int i = 0; i < 5; i++) {
                final double price = 100.0 + i * 0.01;
                final long qty = (i + 1) * 100L;
                bidWriters[i] = new Thread(() -> bids.put(price, qty));
                askWriters[i] = new Thread(() -> asks.put(price + 0.10, qty));
                bidWriters[i].start();
                askWriters[i].start();
            }
            for (int i = 0; i < 5; i++) { bidWriters[i].join(); askWriters[i].join(); }

            System.out.println("  SkipListMap best bid=" + bids.firstKey()
                + " best ask=" + asks.firstKey() + " — lock-free sorted O(log n)");

            // Range query — lock-free
            NavigableMap<Double, Long> topBids = bids.headMap(100.03, true);
            System.out.println("  Bids >= 100.03: " + topBids.size() + " levels — lock-free range query");

            ConcurrentSkipListSet<String> symbols = new ConcurrentSkipListSet<>();
            symbols.add("SPY"); symbols.add("QQQ"); symbols.add("AAPL");
            System.out.println("  ConcurrentSkipListSet (sorted): " + symbols);
        }
    }

    // =========================================================================
    // SECTION 8: Blocking Queues — underlying implementations
    // =========================================================================
    static class BlockingQueueInternals {
        /*
         * ArrayBlockingQueue:
         * -------------------
         *   - Circular array (Object[] items), fixed capacity
         *   - ONE ReentrantLock (both head and tail share it) — put and take contend
         *   - Two Conditions: notFull (producers wait), notEmpty (consumers wait)
         *   - put(): lock → if full, notFull.await() → insert → notEmpty.signal() → unlock
         *   - take(): lock → if empty, notEmpty.await() → remove → notFull.signal() → unlock
         *   - Bounded → good for back-pressure
         *
         * LinkedBlockingQueue:
         * --------------------
         *   - Singly-linked Node list
         *   - TWO locks: takeLock (ReentrantLock) + putLock (ReentrantLock)
         *   - put and take can proceed concurrently (different locks)
         *   - AtomicInteger count for coordination between the two locks
         *   - Higher throughput than ArrayBlockingQueue for MPMC
         *
         * PriorityBlockingQueue:
         * ----------------------
         *   - Binary min-heap (Object[] queue), unbounded (grows dynamically)
         *   - ONE ReentrantLock + notEmpty Condition
         *   - No notFull — unbounded, offer() never blocks
         *   - heapify on insert/remove — O(log n)
         *
         * LinkedTransferQueue:
         * --------------------
         *   - Dual-mode queue (Lea/Scherer/Scott algorithm)
         *   - Lock-free CAS internally (no ReentrantLock)
         *   - transfer(): if consumer waiting → direct handoff (zero latency)
         *   - offer(): enqueue; consumer picks up later
         *   - BEST throughput among blocking queues
         *
         * SynchronousQueue:
         * -----------------
         *   - Zero capacity — each put must meet a take
         *   - Fair mode: FIFO queue of waiting threads
         *   - Unfair mode: LIFO stack of waiting threads (lower latency)
         *   - Used by CachedThreadPool as handoff channel
         *
         * DelayQueue:
         * -----------
         *   - PriorityBlockingQueue of Delayed elements
         *   - take() blocks until element's getDelay() <= 0
         *   - USE CASE: scheduled order cancellations, TTL expiry
         */

        public void run() throws InterruptedException {
            // ArrayBlockingQueue — bounded, single lock
            ArrayBlockingQueue<String> abq = new ArrayBlockingQueue<>(5);
            abq.put("ORDER-1");
            abq.offer("ORDER-2", 1, TimeUnit.MILLISECONDS); // timeout
            System.out.println("  ArrayBlockingQueue: size=" + abq.size()
                + " — single ReentrantLock, notFull/notEmpty conditions");

            // LinkedBlockingQueue — dual lock (put + take concurrent)
            LinkedBlockingQueue<String> lbq = new LinkedBlockingQueue<>(1000);
            lbq.put("MSG-A"); lbq.put("MSG-B");
            System.out.println("  LinkedBlockingQueue: size=" + lbq.size()
                + " — dual ReentrantLock (putLock + takeLock)");

            // LinkedTransferQueue — lock-free CAS
            LinkedTransferQueue<String> ltq = new LinkedTransferQueue<>();
            ltq.offer("FAST-MSG");
            System.out.println("  LinkedTransferQueue: size=" + ltq.size()
                + " — lock-free CAS, direct handoff if consumer waiting");

            // PriorityBlockingQueue — heap + single lock
            PriorityBlockingQueue<Integer> pbq = new PriorityBlockingQueue<>();
            pbq.offer(30); pbq.offer(10); pbq.offer(20);
            System.out.println("  PriorityBlockingQueue: poll=" + pbq.poll()
                + " — binary heap, single lock, unbounded");

            // SynchronousQueue — zero buffer
            SynchronousQueue<String> sq = new SynchronousQueue<>();
            Thread producer = new Thread(() -> {
                try { sq.put("DIRECT-HANDOFF"); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            });
            Thread consumer = new Thread(() -> {
                try { System.out.println("  SynchronousQueue received: " + sq.take()
                    + " — zero buffer, direct thread handoff"); }
                catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            });
            producer.start(); consumer.start();
            producer.join(); consumer.join();
        }
    }

    // =========================================================================
    // SECTION 9: Wait-free vs Lock-free — precise definitions
    // =========================================================================
    static class WaitFreeVsLockFree {
        /*
         * BLOCKING:
         *   Thread A holds lock → Thread B waits indefinitely
         *   OS can preempt Thread A while holding lock → Thread B stuck
         *   Worst case: unbounded wait
         *   Examples: synchronized, ReentrantLock, BlockingQueue
         *
         * LOCK-FREE:
         *   No lock, but uses CAS loops (compare-and-swap)
         *   System progress guaranteed: at least ONE thread completes per step
         *   Individual thread may retry CAS loop many times (not wait-free)
         *   Worst case for individual thread: unbounded retries (starvation possible)
         *   Examples: ConcurrentLinkedQueue, ConcurrentHashMap (writes),
         *             AtomicLong.compareAndSet loop
         *
         * WAIT-FREE:
         *   EVERY thread completes in bounded steps (no starvation ever)
         *   Strongest progress guarantee
         *   Hardest to implement
         *   Examples: AtomicLong.getAndIncrement() (single CAS, no retry loop),
         *             AtomicLong.get() (single read)
         *             SPSC ring buffer (no CAS at all — just memory ordering)
         *
         * OBSTRUCTION-FREE (weakest):
         *   Thread makes progress only if running alone (no contention)
         *   Not used in practice for trading systems
         *
         * HIERARCHY (strongest → weakest guarantee):
         *   Wait-free > Lock-free > Obstruction-free > Blocking
         */

        // Wait-free: single atomic op, guaranteed to complete in bounded steps
        private final AtomicLong seqNo = new AtomicLong(0);

        public long nextSeq()   { return seqNo.incrementAndGet(); }  // wait-free
        public long readSeq()   { return seqNo.get(); }              // wait-free

        // Lock-free: CAS loop — progress guaranteed for system, not individual thread
        public long addIfPositive(AtomicLong value, long delta) {
            long current, updated;
            do {
                current = value.get();
                if (current <= 0) return current;   // abort condition
                updated = current + delta;
            } while (!value.compareAndSet(current, updated));  // retry on CAS fail
            return updated;
        }

        public void run() throws InterruptedException {
            // Wait-free counter — 1000 threads, each gets unique seq
            AtomicLong counter = new AtomicLong(0);
            Thread[] threads = new Thread[1000];
            long[] results = new long[1000];
            for (int i = 0; i < 1000; i++) {
                final int idx = i;
                threads[i] = new Thread(() -> results[idx] = counter.incrementAndGet());
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            // All 1000 values must be unique (1..1000)
            Set<Long> unique = new HashSet<>();
            for (long r : results) unique.add(r);
            System.out.println("  Wait-free AtomicLong: 1000 threads, " + unique.size()
                + " unique values (expected 1000)");
            System.out.println("  Progress: wait-free > lock-free > obstruction-free > blocking");
        }
    }

    // =========================================================================
    // SECTION 10: SPSC Ring Buffer — TRUE wait-free, zero CAS
    // C++ equivalent: SPSC queue with memory_order_acquire/release
    // =========================================================================
    static class SPSCRingBuffer<T> {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Fixed-size circular array of slots
         * Producer owns writePos (only it writes this)
         * Consumer owns readPos  (only it reads this)
         *
         * NO CAS — just volatile reads and array writes
         * Memory ordering: volatile write (release) + volatile read (acquire)
         *
         * This is WAIT-FREE:
         *   Producer: single array write + volatile store → completes in bounded steps
         *   Consumer: single array read + volatile store → completes in bounded steps
         *
         * Cache line padding (see padding below) prevents false sharing
         * between producer's writePos and consumer's readPos
         *
         * C++ equivalent:
         *   std::atomic<uint64_t> writePos(memory_order_release)
         *   std::atomic<uint64_t> readPos (memory_order_acquire)
         */

        private static final int CACHE_LINE = 64;

        private final Object[] buffer;
        private final int      mask;

        // Pad each counter to its own cache line to prevent false sharing
        @SuppressWarnings("unused")
        private long p1,p2,p3,p4,p5,p6,p7;           // pad before writePos
        private volatile long writePos = 0;
        @SuppressWarnings("unused")
        private long p8,p9,p10,p11,p12,p13,p14;       // pad after writePos

        @SuppressWarnings("unused")
        private long p15,p16,p17,p18,p19,p20,p21;     // pad before readPos
        private volatile long readPos = 0;
        @SuppressWarnings("unused")
        private long p22,p23,p24,p25,p26,p27,p28;     // pad after readPos

        @SuppressWarnings("unchecked")
        public SPSCRingBuffer(int capacity) {
            // Capacity must be power of 2 for cheap modulo
            int size = Integer.highestOneBit(capacity - 1) << 1;
            buffer = new Object[size];
            mask   = size - 1;
        }

        // Producer — single writer only, wait-free
        public boolean offer(T item) {
            final long w = writePos;
            final long r = readPos;              // acquire — sees consumer's latest read
            if (w - r >= buffer.length) return false; // full
            buffer[(int)(w & mask)] = item;      // write data
            writePos = w + 1;                    // release — visible to consumer
            return true;
        }

        // Consumer — single reader only, wait-free
        @SuppressWarnings("unchecked")
        public T poll() {
            final long r = readPos;
            final long w = writePos;             // acquire — sees producer's latest write
            if (r == w) return null;             // empty
            T item = (T) buffer[(int)(r & mask)];
            readPos = r + 1;                     // release — visible to producer
            return item;
        }

        public int size() {
            return (int)(writePos - readPos);
        }

        public void run() throws InterruptedException {
            SPSCRingBuffer<String> ring = new SPSCRingBuffer<>(1024);

            // Producer thread
            Thread producer = new Thread(() -> {
                for (int i = 0; i < 100; i++) {
                    while (!ring.offer("MSG-" + i)) { /* spin */ }
                }
            });

            // Consumer thread
            AtomicInteger count = new AtomicInteger(0);
            Thread consumer = new Thread(() -> {
                int received = 0;
                while (received < 100) {
                    String msg = ring.poll();
                    if (msg != null) received++;
                }
                count.set(received);
            });

            producer.start(); consumer.start();
            producer.join(); consumer.join();
            System.out.println("  SPSC Ring Buffer: " + count.get()
                + "/100 messages — wait-free, zero CAS, volatile only");
        }
    }

    // =========================================================================
    // SECTION 11: LongAdder / LongAccumulator — striped counter (better than AtomicLong)
    // =========================================================================
    static class LongAdderInternals {
        /*
         * UNDERLYING IMPLEMENTATION:
         * ---------------------------
         * Problem with AtomicLong under high contention:
         *   Many threads CAS the same memory location → CAS failures → retry loops
         *   → effectively serialized under high load
         *
         * LongAdder solution (Lea's striped counter):
         *   - base: long (used when no contention)
         *   - cells: Cell[] (array of padded AtomicLong)
         *   - Thread hashes to a Cell index
         *   - Each thread updates its own Cell (minimal contention)
         *   - sum(): base + sum(all cells)
         *
         * Cell is @Contended (padded to cache line) to prevent false sharing
         *
         * Result: scales linearly with thread count
         *   AtomicLong: 100 threads → ~1M ops/sec (all fighting one CAS)
         *   LongAdder:  100 threads → ~50M ops/sec (each thread has own cell)
         *
         * Tradeoff: sum() is NOT atomic snapshot (can be stale during updates)
         *   → perfect for counters (order count, fill count, tick count)
         *   → NOT suitable for sequence numbers (needs exact ordering)
         */

        public void run() throws InterruptedException {
            LongAdder tickCount   = new LongAdder();
            LongAdder fillCount   = new LongAdder();
            LongAccumulator maxQty = new LongAccumulator(Long::max, 0L);

            Thread[] threads = new Thread[50];
            for (int i = 0; i < 50; i++) {
                final long qty = i + 1;
                threads[i] = new Thread(() -> {
                    tickCount.increment();
                    fillCount.add(qty);
                    maxQty.accumulate(qty);
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();

            System.out.println("  LongAdder tickCount=" + tickCount.sum()
                + " fillCount=" + fillCount.sum()
                + " — striped cells, no CAS contention");
            System.out.println("  LongAccumulator maxQty=" + maxQty.get()
                + " — accumulate with arbitrary BinaryOperator");
            System.out.println("  LongAdder vs AtomicLong: same semantics, ~50x faster under high thread count");
        }
    }

    // =========================================================================
    // SECTION 12: Complete comparison table
    // =========================================================================
    static class ComparisonTable {
        public void run() {
            System.out.println();
            System.out.println("  ┌─────────────────────────────────┬──────────────────┬──────────────┬──────────┬───────────────────────────────┐");
            System.out.println("  │ Collection                       │ Read Locking     │ Write Locking│ Progress │ Use Case                      │");
            System.out.println("  ├─────────────────────────────────┼──────────────────┼──────────────┼──────────┼───────────────────────────────┤");
            System.out.println("  │ Vector / Hashtable               │ Full method lock │ Full lock    │ Blocking │ Legacy — avoid                │");
            System.out.println("  │ Collections.synchronizedXxx      │ Full method lock │ Full lock    │ Blocking │ Legacy wrapper — avoid        │");
            System.out.println("  │ ConcurrentHashMap                │ None (volatile)  │ Per-bin CAS  │ Lock-free│ General concurrent map        │");
            System.out.println("  │ CopyOnWriteArrayList             │ None (snapshot)  │ Full copy+lock│ Blocking│ Rare writes, many readers     │");
            System.out.println("  │ ConcurrentLinkedQueue            │ None (CAS)       │ CAS loop     │ Lock-free│ MPMC queue, unbounded         │");
            System.out.println("  │ ConcurrentLinkedDeque            │ None (CAS)       │ CAS loop     │ Lock-free│ Work stealing deque           │");
            System.out.println("  │ ConcurrentSkipListMap            │ None (volatile)  │ CAS per level│ Lock-free│ Sorted concurrent map         │");
            System.out.println("  │ ArrayBlockingQueue               │ Single lock      │ Single lock  │ Blocking │ Bounded MPMC, back-pressure   │");
            System.out.println("  │ LinkedBlockingQueue              │ takeLock         │ putLock      │ Blocking │ MPMC, put/take concurrent     │");
            System.out.println("  │ LinkedTransferQueue              │ CAS (lock-free)  │ CAS loop     │ Lock-free│ Fastest blocking queue        │");
            System.out.println("  │ PriorityBlockingQueue            │ Single lock      │ Single lock  │ Blocking │ Priority-ordered queue        │");
            System.out.println("  │ SynchronousQueue                 │ No buffer        │ No buffer    │ Blocking │ Thread handoff, thread pools  │");
            System.out.println("  │ AtomicLong.getAndIncrement       │ None             │ Single CAS   │ Wait-free│ Sequence numbers, counters    │");
            System.out.println("  │ LongAdder                        │ None             │ Striped cells│ Lock-free│ High-contention counters      │");
            System.out.println("  │ SPSC Ring Buffer (custom)        │ Volatile read    │ Volatile write│ Wait-free│ ULL single-producer-consumer  │");
            System.out.println("  │ LMAX Disruptor                   │ Volatile cursor  │ CAS claim    │ Wait-free│ ULL MPMC ring buffer          │");
            System.out.println("  └─────────────────────────────────┴──────────────────┴──────────────┴──────────┴───────────────────────────────┘");
        }
    }

    // =========================================================================
    // SECTION 13: ULL Decision Guide
    // =========================================================================
    static class ULLDecisionGuide {
        public void run() {
            System.out.println("\n  === ULL Collection Decision Guide ===\n");
            System.out.println("  ❌ NEVER on hot path:");
            System.out.println("     Vector / Hashtable           — full method lock, legacy");
            System.out.println("     Collections.synchronizedXxx  — coarse lock, readers block each other");
            System.out.println("     CopyOnWriteArrayList (write) — full copy on every write");
            System.out.println("     ArrayBlockingQueue.put/take  — OS mutex involved");
            System.out.println("     LinkedBlockingQueue.put/take — OS mutex, even with dual lock");

            System.out.println("\n  ✅ ACCEPTABLE on hot path (low contention):");
            System.out.println("     ConcurrentHashMap.get()      — no lock, volatile reads");
            System.out.println("     ConcurrentLinkedQueue        — CAS, no OS lock");
            System.out.println("     ConcurrentSkipListMap        — CAS, sorted order");
            System.out.println("     AtomicLong.incrementAndGet   — single CAS, wait-free");
            System.out.println("     LongAdder.increment()        — striped, no contention per thread");

            System.out.println("\n  ✅✅ BEST for ULL hot path:");
            System.out.println("     SPSC Ring Buffer             — wait-free, zero CAS, volatile only");
            System.out.println("     LMAX Disruptor               — wait-free MPMC, cache-line padded");
            System.out.println("     int[] / long[] direct array  — no locking at all (partition by thread)");
            System.out.println("     ThreadLocal<T>               — zero contention, per-thread state");

            System.out.println("\n  MEMORY LAYOUT RULES for collections:");
            System.out.println("     Pad hot fields to cache line (64 bytes) → prevent false sharing");
            System.out.println("     Power-of-2 capacity → cheap modulo (& mask instead of %)");
            System.out.println("     Pre-allocate all slots → no GC on hot path");
            System.out.println("     Separate producer/consumer state → different cache lines");
        }
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {
        System.out.println("==========================================================================");
        System.out.println(" Java: synchronized vs Concurrent vs Lock/Wait-Free Collections");
        System.out.println("==========================================================================\n");

        System.out.println("--- Section 1: synchronized internals (biased→thin→fat lock) ---");
        new SynchronizedInternals().run();

        System.out.println("\n--- Section 2: Legacy synchronized collections (Vector, Hashtable) ---");
        new LegacySynchronizedCollections().run();

        System.out.println("\n--- Section 3: Collections.synchronizedXxx wrappers ---");
        new SynchronizedWrappers().run();

        System.out.println("\n--- Section 4: ConcurrentHashMap (per-bucket CAS) ---");
        new ConcurrentHashMapInternals().run();

        System.out.println("\n--- Section 5: CopyOnWriteArrayList (copy-on-write) ---");
        new CopyOnWriteInternals().run();

        System.out.println("\n--- Section 6: ConcurrentLinkedQueue (Michael-Scott lock-free) ---");
        new ConcurrentLinkedInternals().run();

        System.out.println("\n--- Section 7: ConcurrentSkipListMap (lock-free sorted) ---");
        new ConcurrentSkipListInternals().run();

        System.out.println("\n--- Section 8: Blocking Queues (underlying implementations) ---");
        new BlockingQueueInternals().run();

        System.out.println("\n--- Section 9: Wait-free vs Lock-free (precise definitions) ---");
        new WaitFreeVsLockFree().run();

        System.out.println("\n--- Section 10: SPSC Ring Buffer (wait-free, zero CAS) ---");
        new SPSCRingBuffer<String>(1024).run();

        System.out.println("\n--- Section 11: LongAdder / LongAccumulator (striped counter) ---");
        new LongAdderInternals().run();

        System.out.println("\n--- Section 12: Complete Comparison Table ---");
        new ComparisonTable().run();

        System.out.println("\n--- Section 13: ULL Decision Guide ---");
        new ULLDecisionGuide().run();

        System.out.println("\n==========================================================================");
        System.out.println(" All sections completed successfully");
        System.out.println("==========================================================================");
    }
}

