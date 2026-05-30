import java.util.concurrent.*;
import java.util.concurrent.locks.*;
import java.util.concurrent.atomic.*;
import java.util.*;

/**
 * Java External Locking Mechanisms for Thread Synchronization
 * ============================================================
 * Covers: synchronized, ReentrantLock, ReadWriteLock, StampedLock,
 *         Semaphore, CountDownLatch, CyclicBarrier, Phaser,
 *         AtomicXxx (lock-free), volatile, wait/notify,
 *         BlockingQueues, ULL recommendations
 *
 * C++ equivalents: pthread_mutex, std::mutex, std::shared_mutex,
 *                  std::atomic, std::condition_variable, sem_t
 *
 * Build: javac Java_External_Locking_Mechanisms.java
 * Run:   java Java_External_Locking_Mechanisms
 */
public class Java_External_Locking_Mechanisms {

    // =========================================================================
    // SECTION 1: synchronized keyword — intrinsic lock (monitor)
    // C++ equivalent: std::mutex + std::lock_guard
    // =========================================================================
    static class SynchronizedDemo {

        // 1a. Synchronized method — locks on 'this'
        private int counter = 0;

        public synchronized void increment() {
            counter++;   // only one thread at a time
        }

        public synchronized int get() {
            return counter;
        }

        // 1b. Synchronized static method — locks on Class object
        private static int staticCounter = 0;

        public static synchronized void staticIncrement() {
            staticCounter++;
        }

        // 1c. Synchronized block — fine-grained, explicit lock object
        // Preferred over method-level for better granularity
        private final Object bidLock  = new Object();
        private final Object askLock  = new Object();
        private double bestBid = 0.0;
        private double bestAsk = 0.0;

        public void updateBid(double price) {
            synchronized (bidLock) {   // only locks bid, ask free to update
                bestBid = price;
            }
        }

        public void updateAsk(double price) {
            synchronized (askLock) {   // concurrent with bid updates
                bestAsk = price;
            }
        }

        // 1d. wait / notify — conditional waiting (classic producer-consumer)
        // C++ equivalent: std::condition_variable
        private final Queue<String> orderQueue = new LinkedList<>();
        private final Object queueLock = new Object();

        public void produceOrder(String order) throws InterruptedException {
            synchronized (queueLock) {
                orderQueue.add(order);
                queueLock.notifyAll();  // wake all waiting consumers
            }
        }

        public String consumeOrder() throws InterruptedException {
            synchronized (queueLock) {
                while (orderQueue.isEmpty()) {
                    queueLock.wait();   // releases lock, sleeps until notified
                }
                return orderQueue.poll();
            }
        }

        public void run() {
            // Test synchronized increment
            Thread[] threads = new Thread[10];
            for (int i = 0; i < 10; i++) {
                threads[i] = new Thread(this::increment);
                threads[i].start();
            }
            for (Thread t : threads) {
                try { t.join(); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            }
            System.out.println("  synchronized counter = " + get() + " (expected 10)");

            // Test split lock objects
            updateBid(100.50);
            updateAsk(100.55);
            System.out.println("  BBO: bid=" + bestBid + " ask=" + bestAsk);
        }
    }

    // =========================================================================
    // SECTION 2: ReentrantLock — explicit lock with try-lock, timeout, fairness
    // C++ equivalent: std::unique_lock<std::mutex>
    // =========================================================================
    static class ReentrantLockDemo {

        private final ReentrantLock lock = new ReentrantLock();
        private final ReentrantLock fairLock = new ReentrantLock(true); // FIFO ordering
        private final Condition notEmpty = lock.newCondition();         // like std::condition_variable
        private final Deque<Integer> buffer = new ArrayDeque<>();

        // Basic lock/unlock — ALWAYS use try/finally
        public void basicUsage() {
            lock.lock();
            try {
                buffer.add(42);
            } finally {
                lock.unlock();  // MUST release even if exception thrown
            }
        }

        // tryLock — non-blocking attempt (C++: try_lock())
        public boolean tryUpdate(int value) {
            if (lock.tryLock()) {            // returns immediately
                try {
                    buffer.add(value);
                    return true;
                } finally {
                    lock.unlock();
                }
            }
            return false;  // lock held by another thread — don't block
        }

        // tryLock with timeout — avoid deadlock
        public boolean tryUpdateWithTimeout(int value) throws InterruptedException {
            if (lock.tryLock(100, TimeUnit.MICROSECONDS)) {  // 100µs timeout
                try {
                    buffer.add(value);
                    return true;
                } finally {
                    lock.unlock();
                }
            }
            return false;
        }

        // Condition variable — producer side
        public void produce(int value) throws InterruptedException {
            lock.lock();
            try {
                buffer.add(value);
                notEmpty.signal();   // wake one waiting consumer
            } finally {
                lock.unlock();
            }
        }

        // Condition variable — consumer side
        public int consume() throws InterruptedException {
            lock.lock();
            try {
                while (buffer.isEmpty()) {
                    notEmpty.await();  // releases lock, waits for signal
                }
                return buffer.poll();
            } finally {
                lock.unlock();
            }
        }

        // Reentrancy — same thread can acquire multiple times
        public void reentrantExample() {
            lock.lock();       // hold count = 1
            try {
                lock.lock();   // same thread — hold count = 2 (does NOT deadlock)
                try {
                    buffer.add(99);
                } finally {
                    lock.unlock(); // hold count = 1
                }
            } finally {
                lock.unlock();     // hold count = 0 — released
            }
        }

        public void run() {
            basicUsage();
            System.out.println("  ReentrantLock: basic add done, buffer=" + buffer);
            boolean got = tryUpdate(100);
            System.out.println("  tryLock result: " + got + ", buffer=" + buffer);
            reentrantExample();
            System.out.println("  reentrant add done, buffer=" + buffer);
        }
    }

    // =========================================================================
    // SECTION 3: ReadWriteLock — multiple readers OR one writer
    // C++ equivalent: std::shared_mutex (C++17)
    // USE CASE: iNAV table (many readers, rare writer)
    // =========================================================================
    static class ReadWriteLockDemo {

        private final ReentrantReadWriteLock rwLock = new ReentrantReadWriteLock();
        private final Lock readLock  = rwLock.readLock();
        private final Lock writeLock = rwLock.writeLock();

        // Simulates iNAV table: many strategy threads read, one feed handler writes
        private final Map<Integer, Double> iNavTable = new HashMap<>();

        // WRITE — exclusive, blocks all readers
        public void updateINAV(int symbolId, double value) {
            writeLock.lock();
            try {
                iNavTable.put(symbolId, value);
            } finally {
                writeLock.unlock();
            }
        }

        // READ — shared, multiple threads read simultaneously
        public double getINAV(int symbolId) {
            readLock.lock();
            try {
                return iNavTable.getOrDefault(symbolId, 0.0);
            } finally {
                readLock.unlock();
            }
        }

        public void run() throws InterruptedException {
            // Seed data
            updateINAV(1, 100.50);
            updateINAV(2, 205.30);

            // 5 readers concurrently — all proceed without blocking each other
            Thread[] readers = new Thread[5];
            for (int i = 0; i < 5; i++) {
                final int id = i % 2 + 1;
                readers[i] = new Thread(() ->
                    System.out.println("    Reader thread iNAV[" + id + "]=" + getINAV(id)));
                readers[i].start();
            }
            for (Thread t : readers) t.join();
            System.out.println("  ReadWriteLock: 5 concurrent reads completed");
        }
    }

    // =========================================================================
    // SECTION 4: StampedLock — optimistic reads (C++20 has no direct equivalent)
    // FASTEST for read-heavy workloads — no CAS on read if no write in progress
    // USE CASE: price cache, position snapshot, reference data
    // =========================================================================
    static class StampedLockDemo {

        private final StampedLock sl = new StampedLock();
        private double bidPrice = 0.0;
        private double askPrice = 0.0;

        // OPTIMISTIC READ — no lock taken, just reads a stamp
        // If stamp unchanged after read → no writer was active → data is valid
        // Cost: almost zero (no CAS, no lock)
        public double[] optimisticRead() {
            long stamp = sl.tryOptimisticRead();       // get stamp (not a lock)
            double b = bidPrice;                        // read speculatively
            double a = askPrice;
            if (!sl.validate(stamp)) {                  // writer was active? retry
                stamp = sl.readLock();                  // fall back to real read lock
                try {
                    b = bidPrice;
                    a = askPrice;
                } finally {
                    sl.unlockRead(stamp);
                }
            }
            return new double[]{b, a};
        }

        // WRITE LOCK
        public void updateBBO(double bid, double ask) {
            long stamp = sl.writeLock();
            try {
                bidPrice = bid;
                askPrice = ask;
            } finally {
                sl.unlockWrite(stamp);
            }
        }

        // UPGRADE: read → write (avoids releasing read lock first)
        public void conditionalUpdate(double newBid) {
            long stamp = sl.readLock();
            try {
                if (newBid > bidPrice) {
                    long writeStamp = sl.tryConvertToWriteLock(stamp);
                    if (writeStamp != 0L) {
                        stamp = writeStamp;
                        bidPrice = newBid;
                    } else {
                        // upgrade failed — release read, take write
                        sl.unlockRead(stamp);
                        stamp = sl.writeLock();
                        bidPrice = newBid;
                    }
                }
            } finally {
                sl.unlock(stamp);
            }
        }

        public void run() {
            updateBBO(100.50, 100.55);
            double[] bbo = optimisticRead();
            System.out.println("  StampedLock optimistic read: bid=" + bbo[0] + " ask=" + bbo[1]);
            conditionalUpdate(100.52);
            System.out.println("  StampedLock after conditional update: bid=" + bidPrice);
        }
    }

    // =========================================================================
    // SECTION 5: Semaphore — control concurrency count
    // C++ equivalent: std::counting_semaphore (C++20), sem_t (POSIX)
    // USE CASE: limit concurrent exchange connections, thread pool throttle
    // =========================================================================
    static class SemaphoreDemo {

        // Allow max 3 concurrent exchange connections
        private final Semaphore connectionPool = new Semaphore(3);
        private final Semaphore binaryGate     = new Semaphore(1);  // mutex-like

        public void sendOrder(String orderId) throws InterruptedException {
            connectionPool.acquire();  // blocks if 3 already in use
            try {
                // simulate sending to exchange
                System.out.println("    Sending order " + orderId
                    + " [permits left: " + connectionPool.availablePermits() + "]");
                Thread.sleep(10);
            } finally {
                connectionPool.release(); // return permit
            }
        }

        // Non-blocking try
        public boolean trySendOrder(String orderId) {
            if (connectionPool.tryAcquire()) {
                try {
                    System.out.println("    tryAcquire success for " + orderId);
                    return true;
                } finally {
                    connectionPool.release();
                }
            }
            System.out.println("    Connection pool full, dropped: " + orderId);
            return false;
        }

        public void run() throws InterruptedException {
            Thread[] threads = new Thread[6];
            for (int i = 0; i < 6; i++) {
                final String id = "ORD-" + i;
                threads[i] = new Thread(() -> {
                    try { sendOrder(id); }
                    catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                });
                threads[i].start();
            }
            for (Thread t : threads) t.join();
            System.out.println("  Semaphore: all 6 orders sent via max-3 connection pool");
        }
    }

    // =========================================================================
    // SECTION 6: CountDownLatch — wait for N events to complete (one-shot)
    // C++ equivalent: std::latch (C++20)
    // USE CASE: wait for all feed handlers to connect before starting strategy
    // =========================================================================
    static class CountDownLatchDemo {

        public void run() throws InterruptedException {
            int NUM_FEEDS = 5;
            CountDownLatch readyLatch = new CountDownLatch(NUM_FEEDS);
            CountDownLatch startLatch = new CountDownLatch(1);  // gun to start

            // 5 feed handler threads — each connects then waits for start signal
            for (int i = 0; i < NUM_FEEDS; i++) {
                final int feedId = i;
                new Thread(() -> {
                    try {
                        Thread.sleep(10 * feedId);  // simulate connect time
                        System.out.println("    Feed " + feedId + " connected");
                        readyLatch.countDown();     // signal ready
                        startLatch.await();         // wait for start gun
                        System.out.println("    Feed " + feedId + " processing market data");
                    } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                }).start();
            }

            readyLatch.await();              // wait until ALL feeds connected
            System.out.println("  All feeds connected — firing start gun");
            startLatch.countDown();          // release all feed threads simultaneously
            Thread.sleep(50);
            System.out.println("  CountDownLatch: synchronized start complete");
        }
    }

    // =========================================================================
    // SECTION 7: CyclicBarrier — all threads wait at barrier, then proceed together
    // C++ equivalent: std::barrier (C++20)
    // USE CASE: end-of-day batch where all pricers must finish before reporting
    // =========================================================================
    static class CyclicBarrierDemo {

        public void run() throws InterruptedException {
            int NUM_PRICERS = 4;
            // Barrier action runs when all threads arrive
            CyclicBarrier barrier = new CyclicBarrier(NUM_PRICERS,
                () -> System.out.println("  [Barrier] All pricers done — generating report"));

            for (int i = 0; i < NUM_PRICERS; i++) {
                final int id = i;
                new Thread(() -> {
                    try {
                        Thread.sleep(20 * (id + 1));  // different pricing durations
                        System.out.println("    Pricer " + id + " finished");
                        barrier.await();  // wait for ALL pricers
                        System.out.println("    Pricer " + id + " proceeding after barrier");
                    } catch (Exception e) { Thread.currentThread().interrupt(); }
                }).start();
            }

            Thread.sleep(200);
            System.out.println("  CyclicBarrier: all pricers synchronized");
        }
    }

    // =========================================================================
    // SECTION 8: Phaser — flexible multi-phase barrier (replaces Latch + Barrier)
    // C++ has no direct equivalent
    // USE CASE: multi-phase market open (connect → snapshot → incremental → trade)
    // =========================================================================
    static class PhaserDemo {

        public void run() throws InterruptedException {
            Phaser phaser = new Phaser(1);  // 1 = main thread initially registered

            String[] phases = {"CONNECT", "SNAPSHOT", "INCREMENTAL", "TRADING"};

            for (int t = 0; t < 3; t++) {  // 3 feed handler threads
                final int tid = t;
                phaser.register();          // register each thread
                new Thread(() -> {
                    for (String phase : phases) {
                        System.out.println("    Feed-" + tid + " phase=" + phase);
                        try { Thread.sleep(10); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                        phaser.arriveAndAwaitAdvance();  // sync at each phase boundary
                    }
                    phaser.arriveAndDeregister();
                }).start();
            }

            // Main thread drives phase transitions
            for (String phase : phases) {
                phaser.arriveAndAwaitAdvance();
                System.out.println("  [Phaser] All feeds completed phase: " + phase);
            }
            phaser.arriveAndDeregister();
            System.out.println("  Phaser: 4-phase market open complete");
        }
    }

    // =========================================================================
    // SECTION 9: Atomic classes — lock-free synchronization
    // C++ equivalent: std::atomic<T>
    // FASTEST for single-variable updates — no OS lock, uses CPU CAS instruction
    // =========================================================================
    static class AtomicDemo {

        // AtomicLong — lock-free counter
        private final AtomicLong orderCount     = new AtomicLong(0);
        private final AtomicLong executedQty    = new AtomicLong(0);

        // AtomicBoolean — flag (kill switch, circuit breaker)
        private final AtomicBoolean killSwitch  = new AtomicBoolean(false);

        // AtomicReference — lock-free reference swap
        private final AtomicReference<String> activeConfig = new AtomicReference<>("v1");

        // AtomicLongArray — lock-free array (position per symbol)
        private final AtomicLongArray positions = new AtomicLongArray(1024);

        // AtomicIntegerFieldUpdater — lock-free update of a field (no AtomicXxx wrapper object)
        // Saves object allocation overhead for high-frequency per-order state
        static class Order {
            volatile int state = 0;  // 0=new, 1=sent, 2=acked, 3=filled
        }
        private static final AtomicIntegerFieldUpdater<Order> STATE_UPDATER =
            AtomicIntegerFieldUpdater.newUpdater(Order.class, "state");

        // Compare-and-Swap (CAS) — core primitive
        // C++ equivalent: atomic.compare_exchange_strong
        public boolean tryAcknowledge(Order order) {
            return STATE_UPDATER.compareAndSet(order, 1, 2);  // sent→acked only
        }

        // getAndAdd — fetch-and-add (C++: fetch_add)
        public long recordFill(long qty) {
            return executedQty.getAndAdd(qty);
        }

        // updateAndGet — apply function atomically
        public long updatePosition(int symbolId, long delta) {
            return positions.getAndAdd(symbolId, delta);
        }

        public void run() throws InterruptedException {
            // 100 threads increment counter
            Thread[] threads = new Thread[100];
            for (int i = 0; i < 100; i++) {
                threads[i] = new Thread(() -> orderCount.incrementAndGet());
                threads[i].start();
            }
            for (Thread t : threads) t.join();
            System.out.println("  AtomicLong counter = " + orderCount.get() + " (expected 100)");

            // CAS on order state
            Order order = new Order();
            order.state = 1;  // sent
            boolean acked = tryAcknowledge(order);
            System.out.println("  CAS sent→acked: " + acked + ", state=" + order.state);

            // Kill switch
            killSwitch.compareAndSet(false, true);
            System.out.println("  KillSwitch fired: " + killSwitch.get());

            // Config hot-swap
            activeConfig.compareAndSet("v1", "v2");
            System.out.println("  Config swapped to: " + activeConfig.get());
        }
    }

    // =========================================================================
    // SECTION 10: volatile — visibility guarantee (no mutual exclusion)
    // C++ equivalent: std::atomic<T> with memory_order_relaxed writes + reads
    // USE CASE: single-writer flags, sequence numbers visible across threads
    // =========================================================================
    static class VolatileDemo {

        // volatile guarantees: writes visible immediately to all threads
        // Does NOT guarantee atomicity for compound operations (i++ is NOT safe)
        private volatile boolean running = true;
        private volatile long    seqNo   = 0L;
        private volatile int     phase   = 0;

        // Safe: single writer, multiple readers
        public void stop()          { running = false; }
        public boolean isRunning()  { return running; }
        public void nextSeq()       { seqNo++; }     // UNSAFE if multiple writers!
        public long  getSeq()       { return seqNo; }

        // Double-checked locking with volatile — safe singleton pattern
        private static volatile VolatileDemo instance = null;
        public static VolatileDemo getInstance() {
            if (instance == null) {
                synchronized (VolatileDemo.class) {
                    if (instance == null) {
                        instance = new VolatileDemo();
                    }
                }
            }
            return instance;
        }

        public void run() throws InterruptedException {
            Thread writer = new Thread(() -> {
                for (int i = 0; i < 5; i++) {
                    seqNo = i * 100;
                    try { Thread.sleep(5); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                }
                running = false;
            });

            Thread reader = new Thread(() -> {
                while (running) {
                    // Always sees latest seqNo due to volatile
                }
                System.out.println("  volatile: reader saw stop, final seqNo=" + seqNo);
            });

            reader.start();
            writer.start();
            writer.join(); reader.join();
        }
    }

    // =========================================================================
    // SECTION 11: BlockingQueue — thread-safe producer-consumer queues
    // C++ equivalent: concurrent queue with condition variable
    // =========================================================================
    static class BlockingQueueDemo {

        // ArrayBlockingQueue — bounded, uses single lock
        // LinkedBlockingQueue — bounded/unbounded, separate head/tail locks
        // PriorityBlockingQueue — priority-ordered, unbounded
        // LinkedTransferQueue — zero-copy if consumer waiting (fastest for handoff)
        // SynchronousQueue — zero buffer, direct handoff only

        private final ArrayBlockingQueue<String>    bounded   = new ArrayBlockingQueue<>(1000);
        private final LinkedBlockingQueue<String>   unbounded = new LinkedBlockingQueue<>();
        private final PriorityBlockingQueue<String> priority  = new PriorityBlockingQueue<>();

        public void run() throws InterruptedException {
            // Producer
            Thread producer = new Thread(() -> {
                String[] orders = {"SELL-AAPL", "BUY-MSFT", "BUY-GOOG", "SELL-SPY"};
                for (String o : orders) {
                    try {
                        bounded.put(o);       // blocks if full
                        System.out.println("    Produced: " + o);
                    } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                }
            });

            // Consumer
            Thread consumer = new Thread(() -> {
                for (int i = 0; i < 4; i++) {
                    try {
                        String o = bounded.poll(100, TimeUnit.MILLISECONDS); // timeout
                        System.out.println("    Consumed: " + o);
                    } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                }
            });

            producer.start(); consumer.start();
            producer.join(); consumer.join();

            // offer (non-blocking) and poll (non-blocking)
            bounded.offer("FAST-ORDER");           // returns false if full
            String taken = bounded.poll();          // returns null if empty
            System.out.println("  BlockingQueue: offer/poll result = " + taken);
        }
    }

    // =========================================================================
    // SECTION 12: LockSupport — low-level thread park/unpark
    // C++ equivalent: futex (Linux), SleepConditionVariableCS (Windows)
    // USE CASE: custom lock implementations, async framework internals
    // =========================================================================
    static class LockSupportDemo {

        public void run() throws InterruptedException {
            Thread worker = new Thread(() -> {
                System.out.println("    Worker: parking...");
                LockSupport.park();                              // block until unparked
                System.out.println("    Worker: unparked and running");
                LockSupport.parkNanos(1_000_000L);               // park for 1ms max
                System.out.println("    Worker: timed park done");
            });

            worker.start();
            Thread.sleep(20);
            System.out.println("  Main: unparking worker");
            LockSupport.unpark(worker);                          // wake the worker
            worker.join();
            System.out.println("  LockSupport: park/unpark complete");
        }
    }

    // =========================================================================
    // SECTION 13: ULL — What to NEVER use vs ALWAYS use on hot path
    // =========================================================================
    static class ULLLockingGuide {

        // ❌ NEVER on hot path — causes OS scheduler involvement
        @SuppressWarnings("unused")
        static void neverOnHotPath() throws InterruptedException {
            ReentrantLock lock = new ReentrantLock();
            // lock.lock()     → may cause context switch (OS mutex)
            // .wait()         → always context switch
            // .notify()       → may cause context switch
            // Semaphore       → OS involvement
            // CountDownLatch  → use only at startup/shutdown
            // BlockingQueue   → has locks inside
        }

        // ✅ ULL hot path alternatives — no OS involvement
        static void ullHotPath() {
            // 1. AtomicLong / AtomicReference — single CAS, no OS
            AtomicLong seq = new AtomicLong(0);
            seq.incrementAndGet();

            // 2. volatile flag — single memory barrier, no CAS
            // (only safe for single writer)

            // 3. SPSC ring buffer — no lock at all (Disruptor pattern)
            // Disruptor: producer writes to ring, consumer reads via sequence cursor

            // 4. StampedLock optimistic read — no CAS if no writer active
            StampedLock sl = new StampedLock();
            long stamp = sl.tryOptimisticRead();
            sl.validate(stamp);

            // 5. ThreadLocal — per-thread state, zero contention
            ThreadLocal<byte[]> encodeBuf = ThreadLocal.withInitial(() -> new byte[4096]);
            byte[] buf = encodeBuf.get();
        }

        public void run() {
            System.out.println("\n  === ULL Locking Decision Guide ===");
            System.out.println("  ❌ AVOID on hot path:");
            System.out.println("     synchronized method/block  → OS mutex, context switch");
            System.out.println("     ReentrantLock.lock()        → OS mutex if contended");
            System.out.println("     BlockingQueue.put/take      → locks inside");
            System.out.println("     wait() / notify()           → always context switch");
            System.out.println("     Semaphore.acquire()         → OS semaphore");

            System.out.println("\n  ✅ USE on hot path:");
            System.out.println("     AtomicLong/AtomicReference  → single CAS instruction");
            System.out.println("     volatile (single writer)    → memory barrier only");
            System.out.println("     StampedLock optimistic read → no lock if no writer");
            System.out.println("     SPSC ring buffer            → zero locking (Disruptor)");
            System.out.println("     ThreadLocal                 → zero contention");
            System.out.println("     LockSupport.park/unpark     → futex, cheaper than mutex");
        }
    }

    // =========================================================================
    // SECTION 14: Full comparison table — Java vs C++ locking
    // =========================================================================
    static class ComparisonTable {
        public void run() {
            System.out.println("\n  ╔══════════════════════════════╦══════════════════════════════╦═════════════════════╗");
            System.out.println("  ║ Java                         ║ C++                          ║ Cost                ║");
            System.out.println("  ╠══════════════════════════════╬══════════════════════════════╬═════════════════════╣");
            System.out.println("  ║ synchronized (intrinsic)     ║ std::mutex + lock_guard      ║ High (OS mutex)     ║");
            System.out.println("  ║ ReentrantLock                ║ std::unique_lock             ║ High (OS mutex)     ║");
            System.out.println("  ║ ReadWriteLock                ║ std::shared_mutex            ║ Medium-High         ║");
            System.out.println("  ║ StampedLock optimistic       ║ No direct equivalent         ║ Very Low (no lock)  ║");
            System.out.println("  ║ StampedLock read/write       ║ std::shared_mutex            ║ Medium              ║");
            System.out.println("  ║ AtomicLong.CAS               ║ std::atomic CAS              ║ Low (CPU instr)     ║");
            System.out.println("  ║ volatile read/write          ║ std::atomic relaxed          ║ Very Low (barrier)  ║");
            System.out.println("  ║ Semaphore                    ║ std::counting_semaphore      ║ High (OS)           ║");
            System.out.println("  ║ CountDownLatch               ║ std::latch (C++20)           ║ Medium (OS)         ║");
            System.out.println("  ║ CyclicBarrier                ║ std::barrier (C++20)         ║ Medium (OS)         ║");
            System.out.println("  ║ Phaser                       ║ No direct equivalent         ║ Medium (OS)         ║");
            System.out.println("  ║ BlockingQueue                ║ locked_queue / cond_var      ║ High (OS)           ║");
            System.out.println("  ║ LockSupport.park/unpark      ║ futex                        ║ Low-Medium (futex)  ║");
            System.out.println("  ║ LMAX Disruptor SPSC          ║ SPSC ring + memory_order     ║ Lowest (no lock)    ║");
            System.out.println("  ╚══════════════════════════════╩══════════════════════════════╩═════════════════════╝");
        }
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {

        System.out.println("=============================================================");
        System.out.println(" Java External Locking Mechanisms — Complete Guide");
        System.out.println("=============================================================\n");

        System.out.println("--- Section 1: synchronized (intrinsic lock) ---");
        new SynchronizedDemo().run();

        System.out.println("\n--- Section 2: ReentrantLock ---");
        new ReentrantLockDemo().run();

        System.out.println("\n--- Section 3: ReadWriteLock (shared_mutex equivalent) ---");
        new ReadWriteLockDemo().run();

        System.out.println("\n--- Section 4: StampedLock (optimistic reads) ---");
        new StampedLockDemo().run();

        System.out.println("\n--- Section 5: Semaphore (connection pool) ---");
        new SemaphoreDemo().run();

        System.out.println("\n--- Section 6: CountDownLatch (startup sync) ---");
        new CountDownLatchDemo().run();

        System.out.println("\n--- Section 7: CyclicBarrier (phase sync) ---");
        new CyclicBarrierDemo().run();

        System.out.println("\n--- Section 8: Phaser (multi-phase sync) ---");
        new PhaserDemo().run();

        System.out.println("\n--- Section 9: Atomic classes (lock-free) ---");
        new AtomicDemo().run();

        System.out.println("\n--- Section 10: volatile (visibility only) ---");
        new VolatileDemo().run();

        System.out.println("\n--- Section 11: BlockingQueue ---");
        new BlockingQueueDemo().run();

        System.out.println("\n--- Section 12: LockSupport (low-level) ---");
        new LockSupportDemo().run();

        System.out.println("\n--- Section 13: ULL Hot Path Guide ---");
        new ULLLockingGuide().run();

        System.out.println("\n--- Section 14: Java vs C++ Comparison Table ---");
        new ComparisonTable().run();

        System.out.println("\n=============================================================");
        System.out.println(" All locking mechanism demos completed successfully");
        System.out.println("=============================================================");
    }
}

