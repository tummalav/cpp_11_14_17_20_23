/**
 * ============================================================================
 * C++ pthread_setaffinity_np → Java Equivalent
 * Thread Pinning + Scheduling Priority — Complete Side-by-Side Guide
 * ============================================================================
 *
 * C++ (what you know)          Java Equivalent
 * ─────────────────────────    ──────────────────────────────────────────────
 * pthread_setaffinity_np()  →  AffinityLock (OpenHFT) OR JNA sched_setaffinity
 * sched_setscheduler()      →  Thread.setPriority() + JNA sched_setscheduler
 * SCHED_FIFO / SCHED_RR     →  JNA direct call (no pure Java equivalent)
 * sched_param.sched_priority →  Thread.setPriority(1–10) maps to OS priority
 * pthread_attr_setschedparam →  Thread.setPriority() before thread.start()
 * cpu_set_t + CPU_SET()      →  long bitmask (Affinity.setAffinity(mask))
 * pthread_self() / gettid()  →  Thread.currentThread().threadId() (JVM ID)
 *                               ProcessHandle.current().pid() (OS PID)
 *
 * KEY INSIGHT:
 *   Java does NOT expose sched_setaffinity or sched_setscheduler natively.
 *   You MUST go through JNA → native C → Linux syscall.
 *   OpenHFT Java-Thread-Affinity wraps this cleanly for you.
 *
 * Build:
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/javac \
 *       Java_vs_CPP_Thread_Pinning_Scheduling.java
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/java \
 *       Java_vs_CPP_Thread_Pinning_Scheduling
 * ============================================================================
 */

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class Java_vs_CPP_Thread_Pinning_Scheduling {

    // =========================================================================
    // PART 1: SIDE-BY-SIDE COMPARISON — C++ vs Java
    // =========================================================================

    static void printSideBySide() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         C++ pthread_setaffinity_np  ←→  Java Equivalent
        ══════════════════════════════════════════════════════════════════════

        ┌─────────────────────────────────────┬─────────────────────────────────────────┐
        │  C++ (Linux / POSIX)                │  Java Equivalent                        │
        ├─────────────────────────────────────┼─────────────────────────────────────────┤
        │                                     │                                         │
        │  // Pin to core 3                   │  // Pin to core 3 (OpenHFT)             │
        │  cpu_set_t cpuset;                  │  import net.openhft.affinity.*;         │
        │  CPU_ZERO(&cpuset);                 │                                         │
        │  CPU_SET(3, &cpuset);               │  try (AffinityLock al =                 │
        │                                     │       AffinityLock.acquireLock(3)) {    │
        │  pthread_setaffinity_np(            │    // thread pinned to core 3           │
        │    pthread_self(),                  │    doWork();                            │
        │    sizeof(cpu_set_t),               │  } // auto-release on exit             │
        │    &cpuset);                        │                                         │
        ├─────────────────────────────────────┼─────────────────────────────────────────┤
        │                                     │                                         │
        │  // Pin to any free isolated core   │  // Pin to any free isolated core       │
        │  // (manual: find via              │  try (AffinityLock al =                 │
        │  //  /sys/devices/system/cpu/...)   │       AffinityLock.acquireCore()) {     │
        │                                     │    System.out.println(al.cpuId());      │
        │  cpu_set_t cpuset;                  │    doWork();                            │
        │  CPU_ZERO(&cpuset);                 │  }                                      │
        │  CPU_SET(isolated_core, &cpuset);   │                                         │
        │  pthread_setaffinity_np(...);       │                                         │
        ├─────────────────────────────────────┼─────────────────────────────────────────┤
        │                                     │                                         │
        │  // Get current affinity            │  // Get current affinity                │
        │  cpu_set_t cpuset;                  │  long mask = Affinity.getAffinity();    │
        │  pthread_getaffinity_np(            │  // e.g. 0x8 = binary 1000 = core 3    │
        │    pthread_self(),                  │                                         │
        │    sizeof(cpu_set_t),               │  // Or via JNA:                         │
        │    &cpuset);                        │  long[] mask = new long[16];            │
        │                                     │  CLib.sched_getaffinity(0, 128, mask);  │
        ├─────────────────────────────────────┼─────────────────────────────────────────┤
        │                                     │                                         │
        │  // SCHED_FIFO priority 90          │  // Thread priority (JVM, not RT)       │
        │  struct sched_param sp;             │  Thread t = new Thread(task);           │
        │  sp.sched_priority = 90;            │  t.setPriority(Thread.MAX_PRIORITY);    │
        │                                     │  // MAX_PRIORITY = 10                   │
        │  sched_setscheduler(                │  // maps to OS nice value               │
        │    0,                               │                                         │
        │    SCHED_FIFO,                      │  // For SCHED_FIFO you MUST use JNA:    │
        │    &sp);                            │  // (see Section 4 below)               │
        ├─────────────────────────────────────┼─────────────────────────────────────────┤
        │                                     │                                         │
        │  // SCHED_RR (round-robin RT)       │  // No pure Java equivalent             │
        │  sched_setscheduler(0,              │  // Must use JNA → sched_setscheduler   │
        │    SCHED_RR, &sp);                  │  // (see Section 4)                     │
        ├─────────────────────────────────────┼─────────────────────────────────────────┤
        │                                     │                                         │
        │  // Set thread name (Linux)         │  // Set thread name                     │
        │  pthread_setname_np(                │  Thread t = new Thread(task,            │
        │    pthread_self(),                  │             "feed-handler-core3");       │
        │    "feed-handler-core3");           │  // or:                                 │
        │                                     │  Thread.currentThread()                 │
        │                                     │    .setName("feed-handler-core3");      │
        └─────────────────────────────────────┴─────────────────────────────────────────┘
        """);
    }

    // =========================================================================
    // PART 2: METHOD 1 — OpenHFT AffinityLock (RECOMMENDED)
    // =========================================================================
    // Maven: net.openhft:Java-Thread-Affinity:3.23.3
    //
    // This is the EXACT Java equivalent of pthread_setaffinity_np.
    // Internally: Java → JNA → sched_setaffinity(2) → Linux kernel
    // =========================================================================

    static void openHFTExamples() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         METHOD 1: OpenHFT AffinityLock — Direct Equivalent of
                   pthread_setaffinity_np
        ══════════════════════════════════════════════════════════════════════

        Maven dependency:
          <dependency>
            <groupId>net.openhft</groupId>
            <artifactId>Java-Thread-Affinity</artifactId>
            <version>3.23.3</version>
          </dependency>

        ─── Pin to specific core (= pthread_setaffinity_np to core N) ─────────

          // C++ equivalent:
          // cpu_set_t s; CPU_ZERO(&s); CPU_SET(3, &s);
          // pthread_setaffinity_np(pthread_self(), sizeof(s), &s);

          try (AffinityLock al = AffinityLock.acquireLock(3)) {
              System.out.println("Pinned to: " + al.cpuId()); // prints 3
              feedHandler.run();                               // never leaves core 3
          } // releases pin when block exits

        ─── Pin to any isolated core (auto-select) ─────────────────────────

          try (AffinityLock al = AffinityLock.acquireCore()) {
              int core = al.cpuId();
              System.out.println("Got core: " + core);
              orderBook.run();
          }

        ─── Pin producer + consumer to HyperThread siblings (shared L1/L2) ──

          // C++ equivalent:
          // Find sibling from /sys/devices/system/cpu/cpu3/topology/thread_siblings_list
          // Then pin two pthreads to those two cores

          Thread producer = new Thread(() -> {
              try (AffinityLock producerLock = AffinityLock.acquireCore()) {
                  Thread consumer = new Thread(() -> {
                      // Pin to HT sibling of producer's core
                      try (AffinityLock consumerLock =
                               producerLock.acquireLock(AffinityStrategies.SAME_CORE)) {
                          consumeLoop();
                      }
                  });
                  consumer.start();
                  produceLoop();
              }
          });
          producer.start();

        ─── Pin to different physical cores (no HT sharing) ──────────────

          // Useful for: two threads that compete for L1 bandwidth

          Thread t1 = new Thread(() -> {
              try (AffinityLock lock1 = AffinityLock.acquireCore()) {
                  Thread t2 = new Thread(() -> {
                      try (AffinityLock lock2 =
                               lock1.acquireLock(AffinityStrategies.DIFFERENT_CORE)) {
                          riskThread();
                      }
                  });
                  t2.start();
                  strategyThread();
              }
          });

        ─── Set affinity mask directly (= cpu_set_t bitmask) ────────────

          // C++ equivalent:
          // cpu_set_t s; CPU_ZERO(&s); CPU_SET(2,&s); CPU_SET(3,&s);
          // pthread_setaffinity_np(pthread_self(), sizeof(s), &s);

          // Java: pin to cores 2 and 3
          // mask = 0b1100 = 0xC = cores 2 and 3
          Affinity.setAffinity(0xCL);

          // Pin to core 0 only:  mask = 0x1
          // Pin to core 3 only:  mask = 0x8
          // Pin to cores 0-7:    mask = 0xFF
          // Pin to even cores:   mask = 0x55 (0101 0101)

        ─── Get current affinity (= pthread_getaffinity_np) ─────────────

          long mask = Affinity.getAffinity();
          System.out.printf("Affinity: 0x%X%n", mask);
          // 0x8 = binary 1000 = core 3 only

        ─── AffinityStrategies (no C++ equivalent — Java-only convenience) ─

          SAME_CORE       → HyperThread sibling (shared L1/L2)
          SAME_SOCKET     → same CPU socket (shared L3)
          DIFFERENT_CORE  → separate physical core
          DIFFERENT_SOCKET → separate CPU socket (different NUMA node)
          ANY             → no preference
        """);
    }

    // =========================================================================
    // PART 3: METHOD 2 — JNA Direct sched_setaffinity (No Library Needed)
    // =========================================================================
    // Exactly mirrors the C++ syscall: sched_setaffinity(pid, size, &mask)
    // pid=0 means "current thread" (same as pthread_self() for affinity)
    // =========================================================================

    static void jnaDirectSyscall() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         METHOD 2: JNA Direct sched_setaffinity Syscall
                   (Exact mirror of C++ sched_setaffinity)
        ══════════════════════════════════════════════════════════════════════

        Maven:
          <dependency>
            <groupId>net.java.dev.jna</groupId>
            <artifactId>jna</artifactId>
            <version>5.14.0</version>
          </dependency>

        // ── Define JNA interface ────────────────────────────────────────────

        import com.sun.jna.Library;
        import com.sun.jna.Native;

        interface CLib extends Library {
            CLib INSTANCE = Native.load("c", CLib.class);

            // sched_setaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
            // C equivalent: extern int sched_setaffinity(pid_t, size_t, cpu_set_t*);
            int sched_setaffinity(int pid, int cpusetsize, long[] mask);
            int sched_getaffinity(int pid, int cpusetsize, long[] mask);
        }

        // ── Pin current thread to core 3 ────────────────────────────────────

        // C++ equivalent:
        // cpu_set_t cpuset;
        // CPU_ZERO(&cpuset);
        // CPU_SET(3, &cpuset);
        // sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

        // Java equivalent:
        static void pinToCore(int core) {
            long[] mask = new long[16];          // cpu_set_t = 128 bytes = 16 longs
            mask[core / 64] |= (1L << (core % 64)); // same as CPU_SET(core, &cpuset)

            int ret = CLib.INSTANCE.sched_setaffinity(
                0,      // 0 = current thread (same as pthread_self() for affinity)
                128,    // sizeof(cpu_set_t) = 128 bytes
                mask
            );
            if (ret != 0) throw new RuntimeException("sched_setaffinity failed: " + ret);
        }

        // ── Build bitmask (= CPU_SET / CPU_ZERO in C++) ─────────────────────

        // C++: CPU_ZERO(&cpuset); CPU_SET(2,&cpuset); CPU_SET(3,&cpuset);
        // Java:
        static long[] makeMask(int... cores) {
            long[] mask = new long[16];  // CPU_ZERO equivalent
            for (int core : cores) {
                mask[core / 64] |= (1L << (core % 64));  // CPU_SET equivalent
            }
            return mask;
        }

        // Usage:
        long[] mask = makeMask(2, 3, 4, 5);  // pin to cores 2,3,4,5
        CLib.INSTANCE.sched_setaffinity(0, 128, mask);

        // ── Get affinity (= pthread_getaffinity_np) ─────────────────────────

        static long[] getAffinity() {
            long[] mask = new long[16];
            CLib.INSTANCE.sched_getaffinity(0, 128, mask);
            return mask;
        }

        // ── IMPORTANT: Java Thread ID vs Linux TID ───────────────────────────

        // C++: pthread_self() gives the pthread handle
        //      gettid()       gives the Linux TID (= kernel thread ID)
        //
        // Java:
        //   Thread.currentThread().threadId()  → JVM thread ID (NOT kernel TID)
        //   ProcessHandle.current().pid()       → JVM process PID
        //
        // To get Linux TID from Java:
        //   int tid = CLib.INSTANCE.gettid();   // via JNA → gettid() syscall
        //
        // sched_setaffinity(0, ...) with pid=0 always means "this thread"
        // so you DON'T need the TID for self-pinning.
        //
        // For pinning OTHER threads, you need their Linux TID:
        //   Read from: /proc/<PID>/task/<TID>/status
        """);
    }

    // =========================================================================
    // PART 4: METHOD 3 — Thread Priority + RT Scheduling via JNA
    // =========================================================================
    // Java Thread.setPriority() is NOT the same as sched_setscheduler SCHED_FIFO
    // For real SCHED_FIFO/SCHED_RR you must use JNA
    // =========================================================================

    static void threadPriorityAndRTScheduling() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         METHOD 3: Thread Priority + Real-Time Scheduling
                   sched_setscheduler ↔ Java setPriority + JNA
        ══════════════════════════════════════════════════════════════════════

        ─── Java Thread.setPriority() ──────────────────────────────────────

        // C++ equivalent (rough): nice(-10)  (lower nice = higher priority)
        // Java: 1 (MIN) to 10 (MAX), default 5 (NORM)
        // Maps to OS nice values: 5→0, 10→-5, 1→+4 (approximate, JVM-dependent)

        Thread t = new Thread(() -> { /* work */ });
        t.setPriority(Thread.MAX_PRIORITY);    // 10 = highest JVM priority
        t.setPriority(Thread.MIN_PRIORITY);    // 1  = lowest
        t.setPriority(Thread.NORM_PRIORITY);   // 5  = default
        t.start();

        // Can also set on running thread:
        Thread.currentThread().setPriority(Thread.MAX_PRIORITY);

        // ⚠️  LIMITATION: Thread.setPriority() is JVM-controlled.
        //     It does NOT give you SCHED_FIFO or SCHED_RR.
        //     It's just a hint to the JVM scheduler.
        //     The OS can still preempt the thread.


        ─── Real-Time Scheduling: SCHED_FIFO via JNA ───────────────────────

        // C++ equivalent:
        // struct sched_param sp = { .sched_priority = 90 };
        // sched_setscheduler(0, SCHED_FIFO, &sp);

        // Java via JNA:

        import com.sun.jna.Library;
        import com.sun.jna.Native;
        import com.sun.jna.Structure;

        interface CLib extends Library {
            CLib INSTANCE = Native.load("c", CLib.class);

            // int sched_setscheduler(pid_t pid, int policy, sched_param *param)
            int sched_setscheduler(int pid, int policy, int[] param);

            // int sched_setparam(pid_t pid, const struct sched_param *param)
            int sched_setparam(int pid, int[] param);

            // int sched_getscheduler(pid_t pid)
            int sched_getscheduler(int pid);
        }

        // Scheduling policy constants (from <sched.h>):
        static final int SCHED_OTHER = 0;  // default Linux scheduler (CFS)
        static final int SCHED_FIFO  = 1;  // real-time: no preemption, runs until blocks
        static final int SCHED_RR    = 2;  // real-time: round-robin with time quantum
        static final int SCHED_BATCH = 3;  // batch: lower priority, no interactive
        static final int SCHED_IDLE  = 5;  // lowest priority, idle only

        // Set SCHED_FIFO priority 90 on current thread:
        static void setSchedFIFO(int priority) {
            int[] param = new int[1];    // struct sched_param { int sched_priority; }
            param[0] = priority;         // 1–99 for SCHED_FIFO/SCHED_RR
            int ret = CLib.INSTANCE.sched_setscheduler(
                0,           // 0 = current thread
                SCHED_FIFO,
                param
            );
            if (ret != 0) throw new RuntimeException(
                "sched_setscheduler failed (need root or CAP_SYS_NICE): " + ret);
        }

        // Usage in trading thread:
        Thread feedThread = new Thread(() -> {
            setSchedFIFO(90);           // same as: sched_setscheduler(0, SCHED_FIFO, {90})
            // Now this thread is SCHED_FIFO, priority 90
            // Will NOT be preempted by any lower-priority thread
            feedHandler.processLoop();
        });


        ─── Priority ranges ────────────────────────────────────────────────

        Policy          Valid Range   Meaning
        ─────────────── ──────────── ────────────────────────────────────────
        SCHED_OTHER     n/a (nice)    Default Linux CFS scheduler
        SCHED_FIFO      1–99          RT: no time slice, runs until blocks/yields
        SCHED_RR        1–99          RT: time-slice quantum (typically 100ms)
        SCHED_BATCH     n/a           Batch workload, won't interrupt interactive
        SCHED_IDLE      n/a           Absolute lowest, only runs when nothing else

        // Check max priority for a policy:
        sched_get_priority_max(SCHED_FIFO)   // returns 99
        sched_get_priority_min(SCHED_FIFO)   // returns 1

        // In Java via JNA:
        interface CLib extends Library {
            int sched_get_priority_max(int policy);
            int sched_get_priority_min(int policy);
        }


        ─── Recommended priorities for trading threads ───────────────────

        Thread              Policy       Priority   Rationale
        ─────────────────── ──────────── ─────────  ─────────────────────────
        NIC/kernel IRQ      SCHED_FIFO   99         Kernel sets this
        Feed handler        SCHED_FIFO   90         First to process MD
        Order book update   SCHED_FIFO   85         Book must reflect MD fast
        Strategy / signal   SCHED_FIFO   80         Decides quotes/orders
        Order sender        SCHED_FIFO   75         Sends to exchange
        Risk monitor        SCHED_FIFO   70         Checks pre-trade risk
        GC threads          SCHED_OTHER  default    Off isolated cores
        Logger              SCHED_BATCH  n/a        Never interrupts trading


        ─── Requirements on RHEL ───────────────────────────────────────────

        # SCHED_FIFO requires root OR CAP_SYS_NICE capability:
        sudo setcap cap_sys_nice+ep /usr/bin/java

        # Or run as root (not recommended in prod):
        sudo java -jar trading-app.jar

        # Or configure /etc/security/limits.conf:
        echo "tradinguser  -  rtprio  99" | sudo tee -a /etc/security/limits.conf
        echo "tradinguser  -  nice   -20" | sudo tee -a /etc/security/limits.conf
        """);
    }

    // =========================================================================
    // PART 5: COMPLETE WORKING EXAMPLE — ULL Trading Thread Setup
    // =========================================================================
    // This is the Java equivalent of the full C++ pthread setup you'd write
    // in a production trading system.
    // =========================================================================

    // Simulate workload interfaces
    interface FeedHandler    { void processLoop(); }
    interface OrderBook      { void updateLoop();  }
    interface Strategy       { void decisionLoop();}
    interface OrderSender    { void sendLoop();    }

    // ── ULL Thread Factory ──────────────────────────────────────────────────
    /**
     * Java equivalent of:
     *
     * // C++
     * void pin_thread(pthread_t tid, int core, int priority) {
     *     cpu_set_t cpuset;
     *     CPU_ZERO(&cpuset);
     *     CPU_SET(core, &cpuset);
     *     pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
     *
     *     struct sched_param sp = { .sched_priority = priority };
     *     pthread_setschedparam(tid, SCHED_FIFO, &sp);
     * }
     */
    static class ULLThreadFactory {

        /**
         * Creates a thread pinned to a specific core with a given name.
         * Uses AffinityLock inside the thread (requires OpenHFT on classpath).
         *
         * C++ equivalent:
         *   pthread_t tid;
         *   pthread_create(&tid, attr, thread_fn, arg);
         *   pin_thread(tid, coreId, priority);
         */
        static Thread createPinnedThread(String name, int coreId,
                                          int javaPriority, Runnable task) {
            return new Thread(() -> {
                // Set JVM priority first (before pinning)
                Thread.currentThread().setPriority(javaPriority);
                Thread.currentThread().setName(name);

                System.out.printf("[%s] Starting on JVM thread %d%n",
                    name, Thread.currentThread().threadId());

                /*
                 * === WITH OpenHFT (recommended) ===
                 *
                 * try (AffinityLock al = AffinityLock.acquireLock(coreId)) {
                 *     System.out.printf("[%s] Pinned to core %d%n", name, al.cpuId());
                 *     task.run();
                 * }
                 *
                 * === WITHOUT OpenHFT (JNA only) ===
                 *
                 * long[] mask = new long[16];
                 * mask[coreId / 64] |= (1L << (coreId % 64));
                 * CLib.INSTANCE.sched_setaffinity(0, 128, mask);
                 * System.out.printf("[%s] Pinned to core %d (via JNA)%n", name, coreId);
                 * task.run();
                 */

                // Simulation (no native lib required for demo):
                System.out.printf("[%s] Would pin to core %d, priority %d%n",
                    name, coreId, javaPriority);
                task.run();

            }, name);
        }

        /**
         * Full trading pipeline thread setup.
         *
         * C++ equivalent:
         *   std::thread feed_thread(feed_handler_fn);
         *   std::thread book_thread(order_book_fn);
         *   std::thread strat_thread(strategy_fn);
         *   std::thread send_thread(order_sender_fn);
         *   pin_thread(feed_thread, core=4, prio=90);
         *   pin_thread(book_thread, core=5, prio=85);
         *   pin_thread(strat_thread, core=6, prio=80);
         *   pin_thread(send_thread, core=7, prio=75);
         */
        static void launchTradingPipeline() throws InterruptedException {
            AtomicBoolean running = new AtomicBoolean(true);

            // Core assignments (match isolcpus=4,5,6,7 from GRUB)
            Thread feedThread = createPinnedThread(
                "feed-handler",
                4,                              // core 4
                Thread.MAX_PRIORITY,            // highest JVM priority = 10
                () -> { /* feedHandler.processLoop() */
                    System.out.println("[feed-handler] Processing market data...");
                }
            );

            Thread bookThread = createPinnedThread(
                "order-book",
                5,                              // core 5
                Thread.MAX_PRIORITY,
                () -> { /* orderBook.updateLoop() */
                    System.out.println("[order-book] Updating book...");
                }
            );

            Thread stratThread = createPinnedThread(
                "strategy",
                6,                              // core 6
                Thread.MAX_PRIORITY - 1,        // priority 9
                () -> { /* strategy.decisionLoop() */
                    System.out.println("[strategy] Making decisions...");
                }
            );

            Thread senderThread = createPinnedThread(
                "order-sender",
                7,                              // core 7
                Thread.MAX_PRIORITY - 1,        // priority 9
                () -> { /* orderSender.sendLoop() */
                    System.out.println("[order-sender] Sending orders...");
                }
            );

            // Set daemon=false so JVM waits for these (they're critical threads)
            feedThread.setDaemon(false);
            bookThread.setDaemon(false);
            stratThread.setDaemon(false);
            senderThread.setDaemon(false);

            // Start all threads
            feedThread.start();
            bookThread.start();
            stratThread.start();
            senderThread.start();

            // Join (wait for completion — in production these run forever)
            feedThread.join();
            bookThread.join();
            stratThread.join();
            senderThread.join();
        }
    }

    // =========================================================================
    // PART 6: THREAD IDENTITY — pthread_self() / gettid() in Java
    // =========================================================================

    static void threadIdentityDemo() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         Thread Identity: pthread_self() / gettid() → Java Equivalent
        ══════════════════════════════════════════════════════════════════════

        C++                              Java
        ──────────────────────────────── ──────────────────────────────────────
        pthread_self()                → Thread.currentThread()
        pthread_self() (as handle)    → Thread.currentThread().threadId()
        gettid() (Linux TID)          → JNA: CLib.INSTANCE.gettid()
        getpid()                      → ProcessHandle.current().pid()
        pthread_getname_np(...)       → Thread.currentThread().getName()
        pthread_setname_np(...)       → Thread.currentThread().setName("name")

        KEY DIFFERENCE:
          Java threadId() ≠ Linux TID (gettid())
          Java threadId() is JVM-internal, not the kernel thread ID.

          For sched_setaffinity on OTHER threads, you need the Linux TID.
          You can get it by: reading /proc/<PID>/task/ (lists all TIDs)
          or via JNA: interface CLib { int gettid(); }

        // Get Linux TID via JNA:
        interface CLib extends Library {
            CLib INSTANCE = Native.load("c", CLib.class);
            int gettid();
        }
        int linuxTid = CLib.INSTANCE.gettid();

        // Read Linux TID from /proc (no JNA needed, read-only):
        static int getLinuxTID() throws Exception {
            // /proc/self/task lists all thread TIDs for current process
            Path taskDir = Paths.get("/proc/self/task");
            // Each subdir name = TID of a thread
            // Hard to map to specific Java thread this way
            // → Use JNA gettid() inside the target thread instead
            return -1; // placeholder
        }

        // PRACTICAL: In sched_setaffinity(pid=0, ...), pid=0 always means
        // "current thread". So you DON'T need gettid() for self-pinning.
        // Just call sched_setaffinity from inside the thread you want to pin.
        """);
    }

    // =========================================================================
    // PART 7: COMPLETE REFERENCE CARD
    // =========================================================================

    static void printReferenceCard() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         COMPLETE REFERENCE CARD: C++ pthreads → Java
        ══════════════════════════════════════════════════════════════════════

        C++ Call                           Java Equivalent                    Lib
        ────────────────────────────────── ──────────────────────────────── ─────────────────────
        pthread_setaffinity_np(t,sz,&mask) AffinityLock.acquireLock(coreId) OpenHFT
        pthread_setaffinity_np(t,sz,&mask) Affinity.setAffinity(mask)       OpenHFT
        pthread_setaffinity_np (self-pin)  sched_setaffinity(0, 128, mask)  JNA
        pthread_getaffinity_np(t,sz,&mask) Affinity.getAffinity()           OpenHFT
        pthread_getaffinity_np (self)      sched_getaffinity(0, 128, mask)  JNA
        sched_setscheduler(0,SCHED_FIFO,p) sched_setscheduler(0, 1, param)  JNA
        sched_setscheduler(0,SCHED_RR, p)  sched_setscheduler(0, 2, param)  JNA
        sched_setparam(0, &param)          sched_setparam(0, param)         JNA
        pthread_setschedparam(t, pol, &p)  sched_setscheduler(0, pol, p)    JNA
        pthread_self()                     Thread.currentThread()           Pure Java
        gettid()                           CLib.INSTANCE.gettid()           JNA
        pthread_setname_np(t,"name")       t.setName("name")                Pure Java
        pthread_getname_np(t, buf, sz)     t.getName()                      Pure Java
        nice(value)                        Thread.setPriority(1–10)         Pure Java
        CPU_ZERO(&cpuset)                  new long[16]                     Pure Java
        CPU_SET(n, &cpuset)                mask[n/64] |= (1L << (n%64))     Pure Java
        CPU_CLR(n, &cpuset)                mask[n/64] &= ~(1L << (n%64))    Pure Java
        CPU_ISSET(n, &cpuset)              (mask[n/64] & (1L<<(n%64))) != 0 Pure Java
        pthread_create()                   new Thread(task)                 Pure Java
        pthread_join()                     thread.join()                    Pure Java
        pthread_detach()                   thread.setDaemon(true)           Pure Java

        ──────────────────────────────────────────────────────────────────────
        DECISION GUIDE: Which approach to use?
        ──────────────────────────────────────────────────────────────────────

        Situation                              Use
        ────────────────────────────────────── ──────────────────────────────
        Simple core pinning, production code   OpenHFT AffinityLock
        No external library allowed            JNA + sched_setaffinity
        SCHED_FIFO / SCHED_RR needed           JNA + sched_setscheduler
        Only rough priority (not RT)           Thread.setPriority()
        Pin whole JVM process                  taskset -c N java -jar app.jar
        Service-level pinning (systemd)        CPUAffinity= in unit file
        Pin + NUMA bind together               numactl + taskset + AffinityLock
        Remove cores from OS scheduler         isolcpus= in GRUB (kernel param)

        ──────────────────────────────────────────────────────────────────────
        MINIMUM CODE TO PIN A JAVA THREAD (copy-paste ready):
        ──────────────────────────────────────────────────────────────────────

        // With OpenHFT (recommended):
        Thread hotThread = new Thread(() -> {
            try (AffinityLock al = AffinityLock.acquireLock(3)) {  // core 3
                while (running) { processMessages(); }
            }
        }, "feed-handler");
        hotThread.setPriority(Thread.MAX_PRIORITY);
        hotThread.start();

        // Without any library (JNA only):
        Thread hotThread = new Thread(() -> {
            long[] mask = new long[16];
            mask[0] = 1L << 3;           // core 3
            CLib.INSTANCE.sched_setaffinity(0, 128, mask);
            while (running) { processMessages(); }
        }, "feed-handler");
        hotThread.setPriority(Thread.MAX_PRIORITY);
        hotThread.start();
        """);
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws InterruptedException {
        System.out.println("=".repeat(70));
        System.out.println("  C++ pthread_setaffinity_np → Java Thread Pinning Guide");
        System.out.println("=".repeat(70));

        printSideBySide();
        openHFTExamples();
        jnaDirectSyscall();
        threadPriorityAndRTScheduling();

        System.out.println("\n══════════════════════════════════════════════════════════════════════");
        System.out.println(" PART 5: Running ULL Trading Pipeline Demo");
        System.out.println("══════════════════════════════════════════════════════════════════════");
        ULLThreadFactory.launchTradingPipeline();

        threadIdentityDemo();
        printReferenceCard();

        System.out.println("\n--- Runtime Info ---");
        System.out.println("JVM PID          : " + ProcessHandle.current().pid());
        System.out.println("Available cores  : " + Runtime.getRuntime().availableProcessors());
        System.out.println("Java version     : " + System.getProperty("java.version"));
        System.out.println("Current thread ID: " + Thread.currentThread().threadId());
        System.out.println("Current priority : " + Thread.currentThread().getPriority()
                           + "  (MAX=" + Thread.MAX_PRIORITY + ")");

        System.out.println("\n✅ Complete.");
    }
}

