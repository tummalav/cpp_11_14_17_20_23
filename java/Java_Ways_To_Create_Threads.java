import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;
import java.util.*;
import java.util.function.*;

/**
 * Different Ways to Create Java Threads — Complete Guide
 * =======================================================
 * Covers all 10 ways with internals, use cases, ULL guidance:
 *
 *  1.  extends Thread
 *  2.  implements Runnable
 *  3.  implements Callable + FutureTask
 *  4.  Lambda Runnable / Callable
 *  5.  ExecutorService thread pools (Fixed, Cached, Single, Work-Stealing)
 *  6.  ScheduledExecutorService
 *  7.  CompletableFuture
 *  8.  ForkJoinPool (recursive decomposition)
 *  9.  Virtual Threads (Java 21+ — Project Loom)
 * 10.  ThreadFactory (custom thread creation)
 * 11.  ULL: custom pinned thread factory
 * 12.  Comparison table
 *
 * Build: javac Java_Ways_To_Create_Threads.java
 * Run:   java  Java_Ways_To_Create_Threads
 */
public class Java_Ways_To_Create_Threads {

    // =========================================================================
    // WAY 1: extends Thread
    // Oldest approach — tight coupling, can't extend another class
    // =========================================================================
    static class Way1_ExtendsThread {
        /*
         * INTERNALS:
         *   Thread class wraps a native OS thread (pthread on Linux)
         *   start() calls native start0() → JVM creates OS thread → calls run()
         *   Thread lifecycle: NEW → RUNNABLE → (BLOCKED/WAITING/TIMED_WAITING) → TERMINATED
         *
         * PROBLEM:
         *   Java is single-inheritance → extending Thread prevents extending other class
         *   Logic and thread mechanics are tightly coupled
         *   Can't reuse the task in thread pools or executors
         *
         * USE WHEN:
         *   Quick prototypes / learning
         *   Need to override Thread methods (setName, setPriority, setDaemon)
         */

        static class FeedHandlerThread extends Thread {
            private final String feedName;
            private final AtomicBoolean running = new AtomicBoolean(true);
            private long ticksProcessed = 0;

            FeedHandlerThread(String feedName) {
                this.feedName = feedName;
                setName("FeedHandler-" + feedName);   // thread name (visible in jstack)
                setDaemon(true);                       // dies with main thread
                setPriority(Thread.MAX_PRIORITY);      // hint to OS scheduler
            }

            @Override
            public void run() {
                while (running.get()) {
                    ticksProcessed++;
                    if (ticksProcessed >= 5) break;    // simulate 5 ticks
                }
            }

            public void shutdown() { running.set(false); }
        }

        public void run() throws InterruptedException {
            FeedHandlerThread t = new FeedHandlerThread("NYSE");
            t.start();              // NEW → RUNNABLE
            t.join();               // wait for completion
            System.out.println("  Way 1 (extends Thread): processed ticks=" + t.ticksProcessed
                + " thread=" + t.getName());
        }
    }

    // =========================================================================
    // WAY 2: implements Runnable
    // Preferred over extends Thread — separates task from execution mechanism
    // =========================================================================
    static class Way2_ImplementsRunnable {
        /*
         * INTERNALS:
         *   Runnable is a @FunctionalInterface with single method: void run()
         *   Thread(Runnable target) stores target as field 'target'
         *   Thread.run() calls: if (target != null) target.run();
         *
         * ADVANTAGES over extends Thread:
         *   - Class can extend something else
         *   - Same Runnable can be submitted to any ExecutorService
         *   - Clean separation: what to do (Runnable) vs how to run (Thread/Executor)
         *   - Runnable instances are reusable
         *
         * USE WHEN:
         *   Most general cases, thread pools, task queues
         */

        static class OrderBookUpdater implements Runnable {
            private final String symbol;
            private final AtomicLong updateCount = new AtomicLong(0);

            OrderBookUpdater(String symbol) { this.symbol = symbol; }

            @Override
            public void run() {
                for (int i = 0; i < 10; i++) {
                    updateCount.incrementAndGet();
                }
            }
        }

        public void run() throws InterruptedException {
            OrderBookUpdater task = new OrderBookUpdater("SPY");

            // Same Runnable → different Thread instances
            Thread t1 = new Thread(task, "BookUpdater-1");
            Thread t2 = new Thread(task, "BookUpdater-2");
            t1.start(); t2.start();
            t1.join(); t2.join();

            System.out.println("  Way 2 (implements Runnable): updates=" + task.updateCount.get()
                + " (2 threads × 10 = expected 20)");
        }
    }

    // =========================================================================
    // WAY 3: implements Callable<V> + FutureTask<V>
    // Returns a result and can throw checked exceptions
    // =========================================================================
    static class Way3_CallableAndFutureTask {
        /*
         * INTERNALS:
         *   Callable<V>: @FunctionalInterface, V call() throws Exception
         *   FutureTask<V>: implements both Runnable and Future<V>
         *     - Wraps Callable
         *     - run() calls callable.call(), stores result in 'outcome' field
         *     - get() blocks until result available (LockSupport.park internally)
         *     - State machine: NEW → COMPLETING → NORMAL / EXCEPTIONAL / CANCELLED
         *
         * vs Runnable:
         *   Runnable.run()  → void, no checked exceptions
         *   Callable.call() → returns V, throws Exception
         *
         * USE WHEN:
         *   Need result from thread
         *   Need to propagate exceptions from thread
         *   iNAV calculation, pricing computation, risk calc
         */

        static class INAVCalculator implements Callable<Double> {
            private final String etfSymbol;
            private final double[] constituentPrices;
            private final double[] weights;

            INAVCalculator(String etfSymbol, double[] prices, double[] weights) {
                this.etfSymbol = etfSymbol;
                this.constituentPrices = prices;
                this.weights = weights;
            }

            @Override
            public Double call() throws Exception {
                double inav = 0.0;
                for (int i = 0; i < constituentPrices.length; i++) {
                    inav += constituentPrices[i] * weights[i];
                }
                return inav;
            }
        }

        public void run() throws Exception {
            double[] prices  = {150.0, 380.0, 175.0, 290.0};
            double[] weights = {0.30,  0.25,  0.25,  0.20};

            INAVCalculator calc = new INAVCalculator("QQQ", prices, weights);
            FutureTask<Double> future = new FutureTask<>(calc);

            Thread t = new Thread(future, "INAVCalc-QQQ");
            t.start();

            Double inav = future.get();   // blocks until result ready
            System.out.printf("  Way 3 (Callable+FutureTask): QQQ iNAV=%.4f%n", inav);

            // FutureTask state checks
            System.out.println("  isDone=" + future.isDone() + " isCancelled=" + future.isCancelled());
        }
    }

    // =========================================================================
    // WAY 4: Lambda Runnable / Callable
    // Syntactic sugar — same as Runnable/Callable but concise
    // =========================================================================
    static class Way4_LambdaThreads {
        /*
         * INTERNALS:
         *   Lambda compiles to invokedynamic → JVM creates anonymous Runnable instance
         *   No actual anonymous class generated at compile time (unlike inner class)
         *   LambdaMetafactory creates implementation at runtime (first call only)
         *   Subsequent calls reuse the same functional interface instance if stateless
         *
         * USE WHEN:
         *   Inline tasks, callbacks, one-liners
         *   Most modern Java code uses this style
         */

        public void run() throws Exception {
            AtomicLong fillCount = new AtomicLong(0);
            AtomicLong rejectCount = new AtomicLong(0);

            // Lambda Runnable
            Thread fillProcessor = new Thread(() -> {
                for (int i = 0; i < 100; i++) fillCount.incrementAndGet();
            }, "FillProcessor");

            // Lambda Runnable — inline
            Thread rejectProcessor = new Thread(
                () -> { for (int i = 0; i < 50; i++) rejectCount.incrementAndGet(); },
                "RejectProcessor"
            );

            fillProcessor.start();
            rejectProcessor.start();
            fillProcessor.join();
            rejectProcessor.join();

            System.out.println("  Way 4 (Lambda): fills=" + fillCount.get()
                + " rejects=" + rejectCount.get());

            // Lambda Callable with FutureTask
            FutureTask<Long> positionCalc = new FutureTask<>(() -> {
                long pos = 0;
                for (int i = 0; i < 1000; i++) pos += i;
                return pos;
            });
            new Thread(positionCalc, "PositionCalc").start();
            System.out.println("  Lambda Callable result=" + positionCalc.get());
        }
    }

    // =========================================================================
    // WAY 5: ExecutorService — thread pools
    // Production standard — don't create raw threads in real code
    // =========================================================================
    static class Way5_ExecutorService {
        /*
         * INTERNALS — ThreadPoolExecutor:
         *   Core fields:
         *     int corePoolSize     — threads always kept alive
         *     int maximumPoolSize  — max threads under load
         *     long keepAliveTime   — idle non-core thread TTL
         *     BlockingQueue<Runnable> workQueue — task buffer
         *     ThreadFactory threadFactory
         *     RejectedExecutionHandler — what to do when queue full + max threads reached
         *
         *   Task submission flow:
         *     1. If running threads < corePoolSize → create new thread
         *     2. Else if queue not full → enqueue
         *     3. Else if running threads < maximumPoolSize → create new thread
         *     4. Else → RejectedExecutionHandler (Abort/Discard/DiscardOldest/CallerRuns)
         *
         *   Worker thread loop:
         *     while (task = queue.take() != null) { task.run(); }
         *     (with keepAlive timeout for non-core threads)
         *
         * POOL TYPES:
         *
         *   newFixedThreadPool(n):
         *     corePoolSize = maxPoolSize = n
         *     queue = LinkedBlockingQueue (unbounded!)
         *     → predictable thread count, memory risk if queue grows unbounded
         *
         *   newCachedThreadPool():
         *     corePoolSize = 0, maxPoolSize = Integer.MAX_VALUE
         *     queue = SynchronousQueue (zero buffer → immediate handoff)
         *     keepAlive = 60s
         *     → creates thread per task, recycles idle threads
         *     → good for many short-lived tasks
         *
         *   newSingleThreadExecutor():
         *     corePoolSize = maxPoolSize = 1
         *     queue = LinkedBlockingQueue
         *     → serializes all tasks, guaranteed ordering
         *
         *   newWorkStealingPool(parallelism):
         *     Backed by ForkJoinPool
         *     Each thread has own deque, steals from others when idle
         *     → best for CPU-intensive parallel tasks
         */

        public void run() throws Exception {
            // Fixed thread pool — 4 threads, bounded
            ExecutorService fixed = Executors.newFixedThreadPool(4);
            AtomicLong fixedCount = new AtomicLong(0);
            List<Future<?>> futures = new ArrayList<>();

            for (int i = 0; i < 20; i++) {
                futures.add(fixed.submit(() -> fixedCount.incrementAndGet()));
            }
            for (Future<?> f : futures) f.get();  // wait all
            fixed.shutdown();
            System.out.println("  Fixed(4): tasks=" + fixedCount.get() + " (expected 20)");

            // Cached thread pool — grows as needed
            ExecutorService cached = Executors.newCachedThreadPool();
            AtomicLong cachedCount = new AtomicLong(0);
            futures.clear();
            for (int i = 0; i < 10; i++) {
                futures.add(cached.submit(() -> cachedCount.incrementAndGet()));
            }
            for (Future<?> f : futures) f.get();
            cached.shutdown();
            System.out.println("  Cached: tasks=" + cachedCount.get() + " (expected 10)");

            // Single thread — serialized execution
            ExecutorService single = Executors.newSingleThreadExecutor();
            Queue<Integer> ordered = new ConcurrentLinkedQueue<>();
            List<Future<?>> singleFutures = new ArrayList<>();
            for (int i = 0; i < 5; i++) {
                final int val = i;
                singleFutures.add(single.submit(() -> ordered.add(val)));
            }
            for (Future<?> f : singleFutures) f.get();
            single.shutdown();
            System.out.println("  Single: ordered=" + ordered + " (guaranteed serial order)");

            // Custom ThreadPoolExecutor — production pattern (bounded queue + rejection)
            ThreadPoolExecutor custom = new ThreadPoolExecutor(
                2, 8,                                         // core=2, max=8
                30, TimeUnit.SECONDS,                         // idle TTL
                new ArrayBlockingQueue<>(100),                // bounded queue
                new ThreadFactory() {
                    int idx = 0;
                    public Thread newThread(Runnable r) {
                        Thread t = new Thread(r, "CustomPool-" + idx++);
                        t.setDaemon(true);
                        return t;
                    }
                },
                new ThreadPoolExecutor.CallerRunsPolicy()     // back-pressure: caller runs task
            );
            AtomicLong customCount = new AtomicLong(0);
            futures.clear();
            for (int i = 0; i < 10; i++) {
                futures.add(custom.submit(() -> customCount.incrementAndGet()));
            }
            for (Future<?> f : futures) f.get();
            custom.shutdown();
            System.out.println("  Custom TPE (2-8 threads, bounded queue): tasks=" + customCount.get());
        }
    }

    // =========================================================================
    // WAY 6: ScheduledExecutorService — timer/cron-style thread scheduling
    // =========================================================================
    static class Way6_ScheduledExecutor {
        /*
         * INTERNALS — ScheduledThreadPoolExecutor:
         *   Extends ThreadPoolExecutor
         *   workQueue = DelayedWorkQueue (binary heap ordered by trigger time)
         *   Worker thread calls queue.take() → blocks until soonest task is due
         *   Uses System.nanoTime() internally (not wall clock)
         *
         * scheduleAtFixedRate(task, initialDelay, period):
         *   Next run = last START time + period
         *   If task takes longer than period → runs immediately after (no overlap)
         *
         * scheduleWithFixedDelay(task, initialDelay, delay):
         *   Next run = last END time + delay
         *   Guaranteed gap between runs
         *
         * USE CASE in trading:
         *   iNAV refresh every 100ms
         *   Heartbeat every 30s
         *   Position snapshot every 1s
         *   Order expiry checks
         */

        public void run() throws InterruptedException {
            ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(2);
            AtomicLong iNavRefreshCount = new AtomicLong(0);
            AtomicLong heartbeatCount  = new AtomicLong(0);

            // Fixed rate — iNAV refresh every 10ms
            ScheduledFuture<?> iNavTask = scheduler.scheduleAtFixedRate(
                () -> iNavRefreshCount.incrementAndGet(),
                0, 10, TimeUnit.MILLISECONDS
            );

            // Fixed delay — heartbeat every 15ms after previous completes
            ScheduledFuture<?> hbTask = scheduler.scheduleWithFixedDelay(
                () -> heartbeatCount.incrementAndGet(),
                0, 15, TimeUnit.MILLISECONDS
            );

            // One-shot delayed — cancel order after 100ms
            ScheduledFuture<String> cancelTask = scheduler.schedule(
                () -> "ORDER-123 cancelled",
                100, TimeUnit.MILLISECONDS
            );

            Thread.sleep(120);   // let tasks run
            iNavTask.cancel(false);
            hbTask.cancel(false);

            System.out.println("  Way 6 (Scheduled): iNAV refreshes≈" + iNavRefreshCount.get()
                + " heartbeats≈" + heartbeatCount.get());
            try {
                System.out.println("  One-shot cancel: " + cancelTask.get());
            } catch (Exception e) {
                System.out.println("  Cancel task exception: " + e.getMessage());
            }
            scheduler.shutdown();
        }
    }

    // =========================================================================
    // WAY 7: CompletableFuture — async pipeline / non-blocking composition
    // =========================================================================
    static class Way7_CompletableFuture {
        /*
         * INTERNALS:
         *   CompletableFuture<T> implements Future<T> and CompletionStage<T>
         *   Backed by ForkJoinPool.commonPool() unless custom Executor provided
         *   Completion chain stored as linked list of dependent CompletableFutures
         *   When stage completes → triggers dependent stages (push model, not poll)
         *
         *   Key operations:
         *     supplyAsync(Supplier)  → runs supplier, returns result async
         *     thenApply(Function)    → transform result (stays same thread or new)
         *     thenAccept(Consumer)   → consume result (no return)
         *     thenCompose(Function)  → flatMap (chain async ops)
         *     thenCombine(other, fn) → combine two futures
         *     exceptionally(fn)      → handle exception
         *     allOf(futures...)      → wait for ALL
         *     anyOf(futures...)      → first to complete wins
         *
         * USE CASE in trading:
         *   Parallel risk calculations across books
         *   Fan-out pricing: compute greeks for many options simultaneously
         *   Async order acknowledgement handling
         */

        public void run() throws Exception {
            // Step 1: async fetch price
            CompletableFuture<Double> priceFuture = CompletableFuture.supplyAsync(() -> {
                return 450.25;  // simulate SPY price fetch
            });

            // Step 2: chain — compute iNAV from price
            CompletableFuture<Double> iNavFuture = priceFuture.thenApply(price -> price * 0.9998);

            // Step 3: chain — check arb signal
            CompletableFuture<String> signalFuture = iNavFuture.thenApply(inav -> {
                double etfPrice = 450.10;
                return (etfPrice < inav - 0.05) ? "BUY_ETF" : (etfPrice > inav + 0.05) ? "SELL_ETF" : "FLAT";
            });

            System.out.println("  Way 7 (CompletableFuture) signal: " + signalFuture.get());

            // Parallel pricing of 4 options
            CompletableFuture<Double>[] pricings = new CompletableFuture[4];
            double[] strikes = {440, 445, 450, 455};
            for (int i = 0; i < 4; i++) {
                final double strike = strikes[i];
                pricings[i] = CompletableFuture.supplyAsync(() -> {
                    // Black-Scholes simplified
                    return Math.max(0, 450.0 - strike) + 2.5;  // intrinsic + time value
                });
            }

            // Wait for all and collect
            CompletableFuture<Void> allDone = CompletableFuture.allOf(pricings);
            allDone.get();
            System.out.print("  Parallel option prices: ");
            for (CompletableFuture<Double> p : pricings) System.out.printf("%.2f ", p.get());
            System.out.println();

            // Exception handling
            CompletableFuture<Double> withFallback = CompletableFuture
                .<Double>supplyAsync(() -> { throw new RuntimeException("Feed down"); })
                .exceptionally(ex -> { System.out.println("  Fallback: " + ex.getMessage()); return 0.0; });
            withFallback.get();
        }
    }

    // =========================================================================
    // WAY 8: ForkJoinPool — recursive divide-and-conquer parallelism
    // =========================================================================
    static class Way8_ForkJoinPool {
        /*
         * INTERNALS:
         *   ForkJoinPool uses work-stealing deques (Deque per thread)
         *   Thread pushes sub-tasks to its own deque (LIFO for locality)
         *   Idle threads steal from OTHER threads' deques (FIFO end, different end)
         *   → Work stealing ensures CPU utilization without coordination overhead
         *
         *   ForkJoinTask variants:
         *     RecursiveTask<V>  — returns result (like Callable)
         *     RecursiveAction   — no result (like Runnable)
         *     CountedCompleter  — completion callbacks
         *
         *   fork() — submit sub-task to this thread's deque
         *   join() — wait for sub-task result (helps execute others while waiting)
         *   invoke() — submit + join
         *
         * USE CASE in trading:
         *   Parallel portfolio MTM (each book computed by one task)
         *   Greeks calculation across all options
         *   Batch risk aggregation
         */

        // Parallel sum of position PnL across books (divide and conquer)
        static class PnLAggregator extends RecursiveTask<Double> {
            private final double[] pnl;
            private final int from, to;
            private static final int THRESHOLD = 100;

            PnLAggregator(double[] pnl, int from, int to) {
                this.pnl = pnl; this.from = from; this.to = to;
            }

            @Override
            protected Double compute() {
                if (to - from <= THRESHOLD) {
                    // Base case: compute directly
                    double sum = 0;
                    for (int i = from; i < to; i++) sum += pnl[i];
                    return sum;
                }
                // Divide
                int mid = (from + to) / 2;
                PnLAggregator left  = new PnLAggregator(pnl, from, mid);
                PnLAggregator right = new PnLAggregator(pnl, mid, to);
                left.fork();                    // submit left to pool
                double rightResult = right.compute(); // compute right inline
                double leftResult  = left.join();     // wait for left
                return leftResult + rightResult;
            }
        }

        public void run() throws Exception {
            double[] bookPnl = new double[1000];
            double expected = 0;
            for (int i = 0; i < 1000; i++) {
                bookPnl[i] = i * 100.0;
                expected += bookPnl[i];
            }

            ForkJoinPool pool = new ForkJoinPool(4);  // 4 worker threads
            double result = pool.invoke(new PnLAggregator(bookPnl, 0, 1000));
            pool.shutdown();

            System.out.printf("  Way 8 (ForkJoin): PnL sum=%.0f expected=%.0f match=%b%n",
                result, expected, Math.abs(result - expected) < 0.001);

            // Parallel streams use ForkJoinPool.commonPool() internally
            double streamSum = Arrays.stream(bookPnl).parallel().sum();
            System.out.printf("  ForkJoin via parallel stream: %.0f%n", streamSum);
        }
    }

    // =========================================================================
    // WAY 9: Virtual Threads — Java 21+ (Project Loom)
    // Lightweight threads mounted on carrier OS threads
    // =========================================================================
    static class Way9_VirtualThreads {
        /*
         * INTERNALS (Java 21+):
         *   Virtual thread = lightweight JVM-managed thread (not 1:1 with OS thread)
         *   Mounted on carrier threads (OS threads from ForkJoinPool)
         *   Unmounted when blocking (I/O, sleep, lock) → carrier thread free to run others
         *
         *   Structure:
         *     Platform thread (Java 1-20): 1 Java thread = 1 OS thread (1-2MB stack)
         *     Virtual thread (Java 21+):   N Java threads = M OS threads (N >> M)
         *                                  Stack is heap-allocated, grows dynamically (~few KB)
         *
         *   Blocking behavior:
         *     Platform thread blocks → OS thread blocked → CPU idle
         *     Virtual thread blocks  → unmounted from carrier → carrier runs another virtual thread
         *     → Can run millions of virtual threads with small OS thread pool
         *
         *   NOT suitable for:
         *     CPU-bound tight loops (keeps carrier pinned — no benefit)
         *     synchronized blocks + blocking I/O inside (pins carrier — known limitation)
         *     ULL sub-microsecond latency (scheduling overhead)
         *
         *   BEST for:
         *     High-concurrency I/O (thousands of exchange connections)
         *     Back-office / middle-office services (confirmation, settlement)
         *     REST/gRPC servers (one virtual thread per request)
         *
         *   Creation APIs:
         *     Thread.ofVirtual().start(Runnable)
         *     Thread.startVirtualThread(Runnable)
         *     Executors.newVirtualThreadPerTaskExecutor()
         */

        public void run() throws Exception {
            AtomicLong count = new AtomicLong(0);

            // Direct virtual thread creation
            Thread vt = Thread.ofVirtual()
                .name("VirtualFeedHandler")
                .start(() -> count.incrementAndGet());
            vt.join();
            System.out.println("  Way 9 (Virtual Thread): isVirtual=" + vt.isVirtual()
                + " count=" + count.get());

            // 10,000 virtual threads — would be impossible with platform threads
            int NUM = 10_000;
            AtomicLong total = new AtomicLong(0);
            Thread[] vThreads = new Thread[NUM];

            for (int i = 0; i < NUM; i++) {
                vThreads[i] = Thread.ofVirtual().start(() -> total.incrementAndGet());
            }
            for (Thread t : vThreads) t.join();
            System.out.println("  10,000 virtual threads: total=" + total.get()
                + " (impossible with platform threads due to 1-2MB stack each)");

            // Virtual thread executor — one virtual thread per task (ideal for I/O)
            try (ExecutorService vtExec = Executors.newVirtualThreadPerTaskExecutor()) {
                List<Future<Integer>> futures = new ArrayList<>();
                for (int i = 0; i < 100; i++) {
                    final int val = i;
                    futures.add(vtExec.submit(() -> {
                        Thread.sleep(1);   // simulate I/O — unmounts carrier thread!
                        return val;
                    }));
                }
                long sum = 0;
                for (Future<Integer> f : futures) sum += f.get();
                System.out.println("  VirtualThreadPerTask executor: sum=" + sum + " (expected 4950)");
            }

            // Thread.Builder API
            Thread.Builder.OfVirtual builder = Thread.ofVirtual().name("vt-", 0);
            Thread vt2 = builder.start(() -> System.out.println("  Named virtual thread: "
                + Thread.currentThread().getName()));
            vt2.join();
        }
    }

    // =========================================================================
    // WAY 10: ThreadFactory — custom thread creation strategy
    // =========================================================================
    static class Way10_ThreadFactory {
        /*
         * INTERNALS:
         *   ThreadFactory is a @FunctionalInterface: Thread newThread(Runnable r)
         *   Used by: ExecutorService, ForkJoinPool, ScheduledExecutorService
         *   Allows centralized control of: name, priority, daemon, group, stack size
         *
         *   Default thread factory (Executors.defaultThreadFactory()):
         *     Names: pool-N-thread-M
         *     Priority: Thread.NORM_PRIORITY (5)
         *     Daemon: false
         *     Group: same as creating thread's group
         *
         *   Custom factory use cases:
         *     ULL: set priority, core affinity, stack size
         *     Monitoring: inject MDC context (logging), metrics
         *     Naming: meaningful names for jstack/profiler
         */

        static class TradingThreadFactory implements ThreadFactory {
            private final String prefix;
            private final int priority;
            private final boolean daemon;
            private final AtomicInteger counter = new AtomicInteger(0);

            TradingThreadFactory(String prefix, int priority, boolean daemon) {
                this.prefix   = prefix;
                this.priority = priority;
                this.daemon   = daemon;
            }

            @Override
            public Thread newThread(Runnable r) {
                Thread t = new Thread(r, prefix + "-" + counter.incrementAndGet());
                t.setPriority(priority);
                t.setDaemon(daemon);
                t.setUncaughtExceptionHandler((thread, ex) ->
                    System.err.println("  UNCAUGHT in " + thread.getName() + ": " + ex.getMessage())
                );
                return t;
            }
        }

        public void run() throws Exception {
            TradingThreadFactory factory = new TradingThreadFactory(
                "HFT-Worker", Thread.MAX_PRIORITY, false
            );

            ExecutorService pool = new ThreadPoolExecutor(
                3, 3, 0L, TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<>(),
                factory
            );

            AtomicLong result = new AtomicLong(0);
            List<Future<?>> futures = new ArrayList<>();
            for (int i = 0; i < 9; i++) {
                futures.add(pool.submit(() -> result.incrementAndGet()));
            }
            for (Future<?> f : futures) f.get();
            pool.shutdown();

            System.out.println("  Way 10 (ThreadFactory): tasks=" + result.get()
                + " threads named HFT-Worker-1/2/3 at MAX_PRIORITY");
        }
    }

    // =========================================================================
    // WAY 11 (BONUS): ULL — Custom pinned thread with CPU affinity (Linux/RHEL)
    // =========================================================================
    static class Way11_ULLPinnedThread {
        /*
         * For ultra-low latency: combine ThreadFactory + CPU affinity
         *
         * On RHEL/Linux:
         *   1. OpenHFT AffinityLock.acquireCore(N) — pins to specific core
         *   2. sched_setscheduler(0, SCHED_FIFO, priority) via JNA — real-time scheduler
         *   3. Isolated cores via isolcpus= kernel param — OS never schedules there
         *
         * Pattern:
         *   - Pre-allocated thread at startup, pinned to isolated core
         *   - Busy-spin loop (no blocking calls) — thread never descheduled
         *   - SPSC queue feeds tasks in — wait-free
         *
         * Demo below simulates the pattern without actual affinity (macOS):
         */

        static class ULLThread extends Thread {
            private final SPSCRingBuffer<Runnable> taskQueue;
            private final AtomicBoolean running = new AtomicBoolean(true);
            private volatile long tasksProcessed = 0;
            private volatile long spinCount = 0;

            ULLThread(String name) {
                super(name);
                this.taskQueue = new SPSCRingBuffer<>(4096);
                setDaemon(true);
                setPriority(Thread.MAX_PRIORITY);
            }

            // Simple SPSC ring buffer (copied inline for self-containment)
            static class SPSCRingBuffer<T> {
                private final Object[] buf;
                private final int mask;
                private volatile long wp = 0, rp = 0;
                @SuppressWarnings("unchecked")
                SPSCRingBuffer(int cap) { buf = new Object[cap]; mask = cap - 1; }
                boolean offer(T t) {
                    if (wp - rp >= buf.length) return false;
                    buf[(int)(wp & mask)] = t; wp++; return true;
                }
                @SuppressWarnings("unchecked")
                T poll() {
                    if (rp == wp) return null;
                    T t = (T) buf[(int)(rp & mask)]; rp++; return t;
                }
            }

            public boolean submit(Runnable task) { return taskQueue.offer(task); }
            public void shutdown() { running.set(false); }

            @Override
            public void run() {
                // Busy-spin — never blocks, never yields to OS on hot path
                while (running.get()) {
                    Runnable task = taskQueue.poll();
                    if (task != null) {
                        task.run();
                        tasksProcessed++;
                    } else {
                        spinCount++;
                        if (spinCount > 1_000_000) break; // guard for demo
                    }
                }
            }
        }

        public void run() throws InterruptedException {
            ULLThread ullThread = new ULLThread("ULL-HotPath");
            ullThread.start();

            AtomicLong processed = new AtomicLong(0);
            for (int i = 0; i < 100; i++) {
                while (!ullThread.submit(() -> processed.incrementAndGet())) { /* spin */ }
            }

            Thread.sleep(20);  // let ULL thread drain
            ullThread.shutdown();
            ullThread.join();

            System.out.println("  Way 11 (ULL pinned thread): processed=" + ullThread.tasksProcessed
                + " — busy-spin, SPSC queue, MAX_PRIORITY");
            System.out.println("  On Linux: add AffinityLock.acquireCore(3) + sched_setscheduler FIFO");
        }
    }

    // =========================================================================
    // SECTION 12: Thread lifecycle and states
    // =========================================================================
    static class ThreadLifecycle {
        public void run() throws InterruptedException {
            System.out.println("\n  Thread States:");
            System.out.println("  NEW         → Thread created, not started yet");
            System.out.println("  RUNNABLE    → Running or ready to run (on CPU or in run queue)");
            System.out.println("  BLOCKED     → Waiting for monitor lock (synchronized block)");
            System.out.println("  WAITING     → wait(), join(), LockSupport.park() — no timeout");
            System.out.println("  TIMED_WAIT  → sleep(ms), wait(ms), park(ns) — with timeout");
            System.out.println("  TERMINATED  → run() returned or threw exception");

            Thread t = new Thread(() -> {
                try { Thread.sleep(5); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            });
            System.out.println("  Before start: " + t.getState());   // NEW
            t.start();
            Thread.sleep(1);
            System.out.println("  While running: " + t.getState());  // TIMED_WAITING (sleeping)
            t.join();
            System.out.println("  After join: " + t.getState());     // TERMINATED

            System.out.println("\n  Key Thread methods:");
            System.out.println("  start()           — NEW → RUNNABLE (cannot call twice)");
            System.out.println("  join()            — caller waits for this thread to die");
            System.out.println("  join(millis)      — caller waits at most millis ms");
            System.out.println("  interrupt()       — sets interrupt flag; wakes sleeping/waiting threads");
            System.out.println("  isInterrupted()   — checks flag (does NOT clear it)");
            System.out.println("  interrupted()     — checks AND clears flag (static)");
            System.out.println("  sleep(ms)         — RUNNABLE → TIMED_WAITING, releases no locks");
            System.out.println("  yield()           — hint to scheduler: willing to give up CPU");
            System.out.println("  setDaemon(true)   — JVM exits when only daemon threads remain");
        }
    }

    // =========================================================================
    // SECTION 13: Comparison table
    // =========================================================================
    static class ComparisonTable {
        public void run() {
            System.out.println("\n  ┌──────────────────────────────────┬────────────────────┬───────────────┬──────────────────────────────┐");
            System.out.println("  │ Method                            │ Returns result?    │ Manages pool? │ Best use case                │");
            System.out.println("  ├──────────────────────────────────┼────────────────────┼───────────────┼──────────────────────────────┤");
            System.out.println("  │ extends Thread                    │ No                 │ No            │ Quick prototypes, legacy     │");
            System.out.println("  │ implements Runnable               │ No                 │ No            │ Tasks submitted to pools     │");
            System.out.println("  │ Callable + FutureTask             │ Yes (Future.get)   │ No            │ Compute result in background │");
            System.out.println("  │ Lambda Runnable/Callable          │ Via FutureTask     │ No            │ Modern inline tasks          │");
            System.out.println("  │ FixedThreadPool                   │ Yes (Future)       │ Yes (fixed N) │ Predictable parallelism      │");
            System.out.println("  │ CachedThreadPool                  │ Yes (Future)       │ Yes (dynamic) │ Many short-lived tasks       │");
            System.out.println("  │ SingleThreadExecutor              │ Yes (Future)       │ Yes (1 thread)│ Serialized task processing   │");
            System.out.println("  │ ScheduledExecutorService          │ Yes (Future)       │ Yes           │ Periodic / delayed tasks     │");
            System.out.println("  │ CompletableFuture                 │ Yes (CF.get)       │ Via FJPool    │ Async pipelines, fan-out     │");
            System.out.println("  │ ForkJoinPool / RecursiveTask      │ Yes                │ Yes (WS)      │ Divide-and-conquer, parallel │");
            System.out.println("  │ Virtual Thread (Java 21+)         │ Yes (Future)       │ Via FJPool    │ High-concurrency I/O         │");
            System.out.println("  │ ThreadFactory                     │ Depends on pool    │ Via pool      │ Custom naming/priority/affin │");
            System.out.println("  │ ULL busy-spin + SPSC              │ Via atomic         │ Manual        │ Sub-microsecond hot path     │");
            System.out.println("  └──────────────────────────────────┴────────────────────┴───────────────┴──────────────────────────────┘");

            System.out.println("\n  ULL Thread Rules:");
            System.out.println("  ✅ Pre-create threads at startup  — no Thread() on hot path");
            System.out.println("  ✅ Busy-spin on hot path          — avoid park/unpark latency");
            System.out.println("  ✅ SPSC queue for task dispatch    — wait-free, no lock");
            System.out.println("  ✅ Isolated cores (isolcpus=)      — OS never preempts");
            System.out.println("  ✅ SCHED_FIFO priority             — real-time class, no time-slicing");
            System.out.println("  ✅ MAX_PRIORITY + setDaemon(false) — not killed with main");
            System.out.println("  ❌ Never create Thread() in loop  — GC + OS overhead");
            System.out.println("  ❌ Never use ExecutorService on    — pool overhead, queue overhead");
            System.out.println("     sub-microsecond hot path");
            System.out.println("  ❌ Virtual threads for HFT        — scheduling latency too high");
        }
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {
        System.out.println("==========================================================================");
        System.out.println(" Different Ways to Create Java Threads — Complete Guide");
        System.out.println("==========================================================================\n");

        System.out.println("--- Way 1: extends Thread ---");
        new Way1_ExtendsThread().run();

        System.out.println("\n--- Way 2: implements Runnable ---");
        new Way2_ImplementsRunnable().run();

        System.out.println("\n--- Way 3: Callable + FutureTask ---");
        new Way3_CallableAndFutureTask().run();

        System.out.println("\n--- Way 4: Lambda Runnable / Callable ---");
        new Way4_LambdaThreads().run();

        System.out.println("\n--- Way 5: ExecutorService thread pools ---");
        new Way5_ExecutorService().run();

        System.out.println("\n--- Way 6: ScheduledExecutorService ---");
        new Way6_ScheduledExecutor().run();

        System.out.println("\n--- Way 7: CompletableFuture ---");
        new Way7_CompletableFuture().run();

        System.out.println("\n--- Way 8: ForkJoinPool + RecursiveTask ---");
        new Way8_ForkJoinPool().run();

        System.out.println("\n--- Way 9: Virtual Threads (Java 21+) ---");
        new Way9_VirtualThreads().run();

        System.out.println("\n--- Way 10: ThreadFactory ---");
        new Way10_ThreadFactory().run();

        System.out.println("\n--- Way 11: ULL busy-spin pinned thread ---");
        new Way11_ULLPinnedThread().run();

        System.out.println("\n--- Thread Lifecycle & States ---");
        new ThreadLifecycle().run();

        System.out.println("\n--- Comparison Table ---");
        new ComparisonTable().run();

        System.out.println("\n==========================================================================");
        System.out.println(" All ways to create threads demonstrated successfully");
        System.out.println("==========================================================================");
    }
}

