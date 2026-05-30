/**
 * ============================================================================
 * Java Thread Pinning & CPU Affinity on RHEL — Complete Guide
 * ============================================================================
 *
 * WHAT IS THREAD PINNING?
 *   Binding a Java thread to a specific CPU core so the OS scheduler
 *   never moves it to another core. Eliminates:
 *     - Cache migration overhead (L1/L2 invalidation on core switch)
 *     - NUMA cross-node memory access
 *     - Scheduling jitter (OS preemption at wrong moment)
 *
 * WHY IT MATTERS IN ULL TRADING:
 *   Core switch latency:   ~1–3 µs (L1 miss cascade)
 *   NUMA cross-node:       ~80–120 ns extra per memory access
 *   Scheduler jitter:      ~10–100 µs worst case
 *   Pinned thread benefit: deterministic sub-microsecond latency
 *
 * APPROACHES (ordered by latency impact):
 *   1. Java Affinity (OpenHFT)    — pure Java, uses JNA → native calls
 *   2. taskset (OS-level)         — wrap JVM process or per-thread via shell
 *   3. numactl (NUMA-aware)       — NUMA node + CPU pinning
 *   4. JNA direct syscall         — sched_setaffinity via JNA
 *   5. RHEL cgroups v2 cpuset     — OS-enforced at cgroup level
 *   6. RHEL tuned profiles        — latency-performance tuning
 *   7. isolcpus kernel parameter  — remove cores from OS scheduler entirely
 *
 * BUILD:
 *   # Compile (no external deps for sections 1–4)
 *   javac Java_Thread_Pinning_CPU_Affinity_RHEL.java
 *   java Java_Thread_Pinning_CPU_Affinity_RHEL
 *
 *   # With Java Affinity (OpenHFT):
 *   # Add to pom.xml:
 *   # <dependency>
 *   #   <groupId>net.openhft</groupId>
 *   #   <artifactId>Java-Thread-Affinity</artifactId>
 *   #   <version>3.23.3</version>
 *   # </dependency>
 *
 * RHEL VERSION: RHEL 8 / RHEL 9 (systemd, cgroups v2, tuned 2.x)
 * JDK VERSION:  Java 17+ (LTS), Java 21 (virtual threads aware)
 * ============================================================================
 */

import java.lang.management.ManagementFactory;
import java.nio.file.*;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class Java_Thread_Pinning_CPU_Affinity_RHEL {

    // =========================================================================
    // SECTION 1: OPENH FT JAVA AFFINITY — Recommended for Trading Systems
    // =========================================================================
    /**
     * OpenHFT Java-Thread-Affinity is the INDUSTRY STANDARD for Java CPU pinning.
     * Used by: LMAX Exchange, HFT firms, low-latency banks.
     *
     * How it works:
     *   Java → JNA → sched_setaffinity(2) syscall → Linux kernel
     *   Result: thread locked to CPU core, never migrated
     *
     * Maven dependency:
     *   net.openhft:Java-Thread-Affinity:3.23.3
     */
    static class OpenHFTAffinityDemo {

        /*
         * --- CODE (requires net.openhft:Java-Thread-Affinity on classpath) ---
         *
         * import net.openhft.affinity.AffinityLock;
         * import net.openhft.affinity.AffinityStrategies;
         * import net.openhft.affinity.Affinity;
         *
         * // METHOD 1: Pin current thread to any available isolated core
         * void pinToAnyCore() throws InterruptedException {
         *     try (AffinityLock al = AffinityLock.acquireCore()) {
         *         System.out.println("Pinned to CPU: " + al.cpuId());
         *         // do latency-sensitive work here
         *         Thread.sleep(1000);
         *     } // auto-released on close
         * }
         *
         * // METHOD 2: Pin to a SPECIFIC CPU core
         * void pinToCore(int coreId) {
         *     try (AffinityLock al = AffinityLock.acquireLock(coreId)) {
         *         System.out.println("Pinned to CPU: " + al.cpuId());
         *         runHotLoop();
         *     }
         * }
         *
         * // METHOD 3: Pin producer + consumer on SAME physical core (HT siblings)
         * //           Shares L1/L2 cache → very fast data handoff
         * void pinProducerConsumerSameCore() {
         *     Thread producer = new Thread(() -> {
         *         try (AffinityLock al = AffinityLock.acquireCore()) {
         *             // Pin consumer to sibling HT core of this core
         *             Thread consumer = new Thread(() -> {
         *                 try (AffinityLock al2 = al.acquireLock(
         *                         AffinityStrategies.SAME_CORE)) {
         *                     consumeLoop();
         *                 }
         *             });
         *             consumer.start();
         *             produceLoop();
         *         }
         *     });
         *     producer.start();
         * }
         *
         * // METHOD 4: Pin threads on DIFFERENT cores (avoid HT sharing)
         * void pinOnDifferentPhysicalCores() {
         *     // First thread: any core
         *     Thread t1 = new Thread(() -> {
         *         try (AffinityLock al = AffinityLock.acquireCore()) {
         *             // Second thread: different physical core (not HT sibling)
         *             Thread t2 = new Thread(() -> {
         *                 try (AffinityLock al2 = al.acquireLock(
         *                         AffinityStrategies.DIFFERENT_CORE)) {
         *                     riskyLoop(); // no HT interference
         *                 }
         *             });
         *             t2.start();
         *             safeLoop();
         *         }
         *     });
         *     t1.start();
         * }
         *
         * // METHOD 5: Read current CPU affinity mask
         * void checkCurrentAffinity() {
         *     long affinityMask = Affinity.getAffinity();
         *     System.out.printf("Affinity mask: 0x%X%n", affinityMask);
         *     // e.g. 0x4 = binary 100 = CPU core 2 only
         * }
         *
         * // METHOD 6: Set affinity mask directly (bitmask)
         * void setAffinityMask() {
         *     // Pin to cores 2 and 3: mask = 0b1100 = 0xC
         *     Affinity.setAffinity(0xCL);
         * }
         */

        static void printInstructions() {
            System.out.println("""
                === OpenHFT Java Affinity ===

                Maven:
                  <dependency>
                    <groupId>net.openhft</groupId>
                    <artifactId>Java-Thread-Affinity</artifactId>
                    <version>3.23.3</version>
                  </dependency>

                Key Classes:
                  AffinityLock.acquireCore()           → pin to any free core
                  AffinityLock.acquireLock(cpuId)      → pin to specific core
                  AffinityLock.acquireLock(strategy)   → SAME_CORE/DIFF_CORE
                  Affinity.getAffinity()               → get current mask
                  Affinity.setAffinity(mask)           → set mask directly

                AffinityStrategies:
                  SAME_CORE      → HT sibling (shared L1/L2, fast queue handoff)
                  DIFFERENT_CORE → separate physical core (no HT interference)
                  SAME_SOCKET    → same NUMA node (shared L3)
                  DIFFERENT_SOCKET → separate NUMA nodes

                Try-with-resources: AffinityLock is AutoCloseable
                  → always use try() {} to release lock on thread exit
                """);
        }
    }

    // =========================================================================
    // SECTION 2: JNA DIRECT SYSCALL — sched_setaffinity without library
    // =========================================================================
    /**
     * If you cannot add OpenHFT dependency, use JNA directly.
     * sched_setaffinity(2) is the Linux syscall that pins a thread to CPUs.
     *
     * Maven:
     *   <dependency>
     *     <groupId>net.java.dev.jna</groupId>
     *     <artifactId>jna</artifactId>
     *     <version>5.14.0</version>
     *   </dependency>
     */
    static class JNAAffinityDemo {

        /*
         * import com.sun.jna.Library;
         * import com.sun.jna.Native;
         * import com.sun.jna.Platform;
         * import com.sun.jna.ptr.LongByReference;
         *
         * interface CLibrary extends Library {
         *     CLibrary INSTANCE = Native.load("c", CLibrary.class);
         *
         *     // sched_setaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
         *     int sched_setaffinity(int pid, int cpusetsize, long[] mask);
         *     int sched_getaffinity(int pid, int cpusetsize, long[] mask);
         *
         *     // gettid() — get Linux thread ID (not Java thread ID)
         *     int gettid();
         * }
         *
         * // Pin current Java thread to CPU core 3
         * static void pinToCore(int core) {
         *     long[] mask = new long[16];   // cpu_set_t is 128 bytes = 16 longs
         *     mask[core / 64] |= (1L << (core % 64));
         *
         *     // pid=0 means current thread
         *     int ret = CLibrary.INSTANCE.sched_setaffinity(0, 128, mask);
         *     if (ret != 0) {
         *         throw new RuntimeException("sched_setaffinity failed: " + ret);
         *     }
         *     System.out.println("Thread " + Thread.currentThread().getName()
         *                        + " pinned to core " + core);
         * }
         *
         * // Get current affinity mask
         * static long[] getAffinity() {
         *     long[] mask = new long[16];
         *     CLibrary.INSTANCE.sched_getaffinity(0, 128, mask);
         *     return mask;
         * }
         *
         * // Pin all cores in a range (e.g., cores 2–5)
         * static void pinToCoreRange(int fromCore, int toCore) {
         *     long[] mask = new long[16];
         *     for (int c = fromCore; c <= toCore; c++) {
         *         mask[c / 64] |= (1L << (c % 64));
         *     }
         *     CLibrary.INSTANCE.sched_setaffinity(0, 128, mask);
         * }
         */

        static void printInstructions() {
            System.out.println("""
                === JNA Direct sched_setaffinity ===

                Key: sched_setaffinity(pid=0, size=128, mask)
                  pid=0  → applies to current thread
                  mask   → bitmask: bit N set = core N allowed

                Core 0 only:  mask[0] = 0x1L   (binary: ...0001)
                Core 3 only:  mask[0] = 0x8L   (binary: ...1000)
                Cores 2-5:    mask[0] = 0x3CL  (binary: 111100)

                Important: Java thread ID ≠ Linux TID
                  Use gettid() via JNA to get Linux TID for per-thread pinning
                  Thread.currentThread().getId() returns JVM ID, not kernel TID
                """);
        }
    }

    // =========================================================================
    // SECTION 3: PROCESS-LEVEL PINNING — taskset & numactl on RHEL
    // =========================================================================
    /**
     * Simplest approach: pin the entire JVM process to specific cores.
     * All JVM threads (including GC threads!) share the pinned cores.
     * For ULL: pin JVM to isolated cores, then use AffinityLock within Java.
     */
    static class OSLevelPinning {

        static void printRHELCommands() {
            System.out.println("""
                ============================================================
                RHEL: OS-Level CPU Pinning Commands
                ============================================================

                --- 1. taskset — pin process/thread to CPUs ---

                # Pin JVM to core 2 only
                taskset -c 2 java -jar trading-app.jar

                # Pin JVM to cores 2,3,4,5
                taskset -c 2-5 java -jar trading-app.jar

                # Pin JVM to cores 2,3 and 6,7 (non-contiguous)
                taskset -c 2,3,6,7 java -jar trading-app.jar

                # Pin a RUNNING process (PID 12345) to core 3
                taskset -cp 3 12345

                # Check affinity of a running process
                taskset -p 12345

                # Pin a specific thread (TID, not PID) to core 4
                # Get TID: cat /proc/<PID>/task/<TID>/status | grep Pid
                taskset -cp 4 <TID>


                --- 2. numactl — NUMA-aware pinning ---

                # Pin JVM to NUMA node 0 (all CPUs on node 0)
                numactl --cpunodebind=0 --membind=0 java -jar trading-app.jar

                # Pin to specific CPUs + memory on node 0
                numactl --physcpubind=2,3,4,5 --membind=0 java -jar trading-app.jar

                # Check NUMA topology
                numactl --hardware
                numactl --show

                # Example output:
                # node 0 cpus: 0 1 2 3 4 5 6 7
                # node 1 cpus: 8 9 10 11 12 13 14 15
                # node distances: 0→0: 10, 0→1: 21

                # BEST PRACTICE for ULL:
                numactl --cpunodebind=0 --membind=0 \\
                    taskset -c 2,3,4,5 \\
                    java -XX:+UseZGC -XX:+ZGenerational \\
                         -Xms4g -Xmx4g \\
                         -jar trading-app.jar


                --- 3. Check current CPU topology on RHEL ---

                lscpu                          # CPU/socket/core/thread counts
                lscpu -p                       # machine-parseable format
                cat /proc/cpuinfo              # detailed per-core info
                lstopo                         # visual NUMA topology (hwloc)
                cat /sys/devices/system/cpu/cpu*/topology/core_id

                # Find which cores are on which NUMA node:
                cat /sys/devices/system/node/node0/cpulist   # e.g. 0-7
                cat /sys/devices/system/node/node1/cpulist   # e.g. 8-15

                # Find HyperThread siblings (logical → physical mapping):
                cat /sys/devices/system/cpu/cpu2/topology/thread_siblings_list
                # e.g. "2,10" means cpu2 and cpu10 share same physical core
                """);
        }
    }

    // =========================================================================
    // SECTION 4: isolcpus — REMOVE CORES FROM OS SCHEDULER (Most Powerful)
    // =========================================================================
    /**
     * isolcpus removes CPUs from the Linux scheduler entirely.
     * Only your explicitly pinned threads run on those cores.
     * No OS tasks, no interrupts, no kernel threads.
     * This is what HFT firms do on production servers.
     */
    static class IsolCPUsSetup {

        static void printSetupSteps() {
            System.out.println("""
                ============================================================
                RHEL: isolcpus Kernel Parameter — Remove Cores from Scheduler
                ============================================================

                WARNING: Requires root + reboot. Production servers only.

                --- Step 1: Edit GRUB on RHEL 8/9 ---

                # Edit /etc/default/grub:
                sudo vi /etc/default/grub

                # Find GRUB_CMDLINE_LINUX and add:
                # isolcpus=2,3,4,5 nohz_full=2,3,4,5 rcu_nocbs=2,3,4,5

                # Example before:
                GRUB_CMDLINE_LINUX="crashkernel=auto rd.lvm.lv=rhel/root quiet"

                # Example after (isolate cores 2–5):
                GRUB_CMDLINE_LINUX="crashkernel=auto rd.lvm.lv=rhel/root quiet \\
                    isolcpus=2,3,4,5 \\
                    nohz_full=2,3,4,5 \\
                    rcu_nocbs=2,3,4,5 \\
                    irqaffinity=0,1"

                --- Step 2: Rebuild GRUB ---

                # RHEL 8/9 with BIOS:
                sudo grub2-mkconfig -o /boot/grub2/grub.cfg

                # RHEL 8/9 with UEFI:
                sudo grub2-mkconfig -o /boot/efi/EFI/redhat/grub.cfg

                --- Step 3: Reboot ---
                sudo reboot

                --- Step 4: Verify isolated cores ---
                cat /sys/devices/system/cpu/isolated
                # Expected output: 2-5

                cat /proc/cmdline | grep isolcpus
                # Should show: isolcpus=2,3,4,5

                --- Step 5: Run JVM on isolated cores ---

                # Now taskset to those isolated cores:
                taskset -c 2,3,4,5 java -jar trading-app.jar

                # Or in Java with OpenHFT — it auto-detects isolated CPUs:
                # AffinityLock.acquireCore() will prefer isolated cores


                --- What each parameter does ---

                isolcpus=2,3,4,5
                  → Removes cores 2-5 from general scheduler
                  → No OS tasks, kernel threads, or other processes land here
                  → Your JVM threads are the ONLY thing running on these cores

                nohz_full=2,3,4,5
                  → Disables timer tick on those cores (no 1ms interrupt)
                  → Eliminates scheduler jitter from timer interrupts
                  → Critical for sub-microsecond determinism

                rcu_nocbs=2,3,4,5
                  → Offloads RCU callbacks off isolated cores
                  → RCU = Read-Copy-Update (kernel locking mechanism)
                  → Without this: periodic RCU callbacks interrupt your thread

                irqaffinity=0,1
                  → Forces ALL hardware interrupts to cores 0 and 1
                  → NIC interrupts, disk interrupts, etc. stay off your cores
                  → Use with DPDK/Solarflare: interrupts handled by NIC itself
                """);
        }
    }

    // =========================================================================
    // SECTION 5: RHEL cgroups v2 cpuset — Container/Service Level Pinning
    // =========================================================================
    /**
     * cgroups v2 cpuset assigns CPU cores to a cgroup.
     * Any process in that cgroup is restricted to those CPUs.
     * Used with: systemd services, containers (Podman/Docker on RHEL).
     */
    static class CgroupsCpusetSetup {

        static void printSetupSteps() {
            System.out.println("""
                ============================================================
                RHEL cgroups v2 cpuset — Service-Level CPU Pinning
                ============================================================

                --- Method 1: systemd service with CPUAffinity ---

                # /etc/systemd/system/trading-app.service
                [Unit]
                Description=ULL Trading Application
                After=network.target

                [Service]
                Type=simple
                ExecStart=/usr/bin/java \\
                    -XX:+UseZGC -XX:+ZGenerational \\
                    -Xms8g -Xmx8g \\
                    -XX:ConcGCThreads=2 \\
                    -jar /opt/trading/app.jar

                # Pin this service to cores 2,3,4,5
                CPUAffinity=2 3 4 5

                # NUMA memory policy
                NUMAPolicy=bind
                NUMAMask=0

                # Prevent swapping
                MemorySwapMax=0

                # Real-time scheduling (optional, needs CAP_SYS_NICE)
                # CPUSchedulingPolicy=fifo
                # CPUSchedulingPriority=90

                [Install]
                WantedBy=multi-user.target

                # Apply:
                sudo systemctl daemon-reload
                sudo systemctl start trading-app
                sudo systemctl enable trading-app

                # Verify CPU affinity:
                systemctl show trading-app | grep CPU
                ps -eo pid,psr,comm | grep java   # PSR = current CPU


                --- Method 2: Manual cgroups v2 cpuset ---

                # Create cgroup for trading app
                sudo mkdir /sys/fs/cgroup/trading

                # Assign cores 2-5 to this cgroup
                echo "2-5" | sudo tee /sys/fs/cgroup/trading/cpuset.cpus

                # Pin memory to NUMA node 0
                echo "0" | sudo tee /sys/fs/cgroup/trading/cpuset.mems

                # Move JVM process (PID 12345) into cgroup
                echo 12345 | sudo tee /sys/fs/cgroup/trading/cgroup.procs

                # Verify
                cat /sys/fs/cgroup/trading/cpuset.cpus   # 2-5
                cat /proc/12345/status | grep Cpus_allowed


                --- Method 3: Podman container CPU pinning on RHEL ---

                # Pin container to cores 2-5
                podman run --cpuset-cpus="2-5" \\
                           --cpuset-mems="0" \\
                           trading-image:latest
                """);
        }
    }

    // =========================================================================
    // SECTION 6: RHEL tuned — Latency Performance Profiles
    // =========================================================================
    /**
     * tuned is RHEL's system tuning daemon.
     * The latency-performance and realtime profiles configure the OS
     * for minimum latency automatically.
     */
    static class TunedProfiles {

        static void printSetupSteps() {
            System.out.println("""
                ============================================================
                RHEL tuned — Latency Profiles
                ============================================================

                # Install tuned (usually pre-installed on RHEL)
                sudo dnf install tuned tuned-profiles-realtime

                # List available profiles
                tuned-adm list

                # RECOMMENDED for ULL trading:
                sudo tuned-adm profile latency-performance

                # For absolute minimum latency (real-time):
                sudo tuned-adm profile realtime

                # Check active profile
                tuned-adm active

                # What latency-performance does:
                #   - Sets CPU governor to 'performance' (no freq scaling)
                #   - Disables CPU C-states (no deep sleep transitions)
                #   - Sets /proc/sys/kernel/sched_min_granularity_ns
                #   - Disables transparent hugepages (reduces GC pauses)
                #   - Sets network IRQ affinity

                # What realtime adds:
                #   - isolcpus (configured via /etc/tuned/realtime-variables.conf)
                #   - nohz_full
                #   - rcu_nocbs
                #   - PREEMPT_RT patches (if kernel supports it)

                # Configure realtime isolated cores:
                sudo vi /etc/tuned/realtime-variables.conf
                # Set: isolated_cores=2-5

                sudo tuned-adm profile realtime
                sudo reboot


                # Disable CPU frequency scaling manually (if not using tuned):
                # Set all cores to performance governor
                for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
                    echo performance | sudo tee $cpu
                done

                # Verify:
                cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
                # Expected: performance


                # Disable C-states for lowest latency:
                # C0 = running, C1 = halt, C3+ = deep sleep (causes wakeup latency)
                sudo cpupower idle-set -D 0   # disable all idle states deeper than C0

                # Or per-core:
                echo 1 | sudo tee /sys/devices/system/cpu/cpu2/cpuidle/state3/disable
                """);
        }
    }

    // =========================================================================
    // SECTION 7: JVM GC THREAD PINNING — Keep GC off hot cores
    // =========================================================================
    /**
     * GC threads compete with your trading threads for CPU time.
     * Strategy: pin GC threads to "house-keeping" cores, trading to isolated cores.
     * ZGC with concurrent GC runs mostly off the hot path anyway.
     */
    static class GCThreadPinning {

        static void printJVMFlags() {
            System.out.println("""
                ============================================================
                JVM Flags: Separate GC Threads from Trading Threads
                ============================================================

                # Full JVM launch command for ULL trading on RHEL:
                # Assuming: 16-core server, cores 0-1 = OS, cores 2-5 = trading,
                #            cores 6-7 = GC/housekeeping

                numactl --cpunodebind=0 --membind=0 \\
                taskset -c 2,3,4,5,6,7 \\
                java \\
                    # Heap: fixed size = no resize pauses
                    -Xms8g -Xmx8g \\

                    # ZGC: sub-millisecond GC pauses (RHEL 8+, Java 17+)
                    -XX:+UseZGC \\
                    -XX:+ZGenerational \\

                    # Limit GC threads (they go to cores 6-7 via taskset)
                    -XX:ConcGCThreads=2 \\
                    -XX:ParallelGCThreads=2 \\

                    # Disable GC heuristics that cause stop-the-world
                    -XX:+AlwaysPreTouch \\         # pre-fault all pages at startup
                    -XX:+DisableExplicitGC \\      # ignore System.gc() calls

                    # JIT compiler: compile at startup, no JIT on hot path
                    -XX:+TieredCompilation \\
                    -XX:ReservedCodeCacheSize=256m \\

                    # Huge pages (reduces TLB misses)
                    -XX:+UseLargePages \\
                    -XX:LargePageSizeInBytes=2m \\

                    # Disable NUMA auto-balancing (we pin manually)
                    -XX:+UseNUMA \\

                    # Thread stack size (smaller = more threads, less memory)
                    -Xss512k \\

                    # Log GC for monitoring (file, not stdout)
                    -Xlog:gc*:file=/var/log/trading/gc.log:time,uptime:filecount=5,filesize=20m \\

                    -jar /opt/trading/app.jar


                --- ZGC on RHEL: setup huge pages ---

                # Check current huge pages
                cat /proc/meminfo | grep HugePage

                # Allocate 4096 huge pages (2MB each = 8GB total)
                echo 4096 | sudo tee /proc/sys/vm/nr_hugepages

                # Make persistent (survives reboot):
                echo "vm.nr_hugepages=4096" | sudo tee /etc/sysctl.d/99-hugepages.conf
                sudo sysctl -p /etc/sysctl.d/99-hugepages.conf

                # Verify allocation:
                grep HugePages /proc/meminfo
                # HugePages_Total: 4096
                # HugePages_Free:  4096
                # Hugepagesize:    2048 kB
                """);
        }
    }

    // =========================================================================
    // SECTION 8: COMPLETE PRODUCTION SETUP — Step by Step for RHEL
    // =========================================================================
    static class ProductionSetup {

        static void printCompleteSetup() {
            System.out.println("""
                ============================================================
                COMPLETE RHEL PRODUCTION SETUP — ULL Java Trading App
                ============================================================

                SERVER: 2-socket, 16 cores each (32 total), RHEL 9
                GOAL:   Cores 0-3 = OS/infra, Cores 4-7 = trading (isolated)


                STEP 1: Check NUMA topology
                ─────────────────────────────
                numactl --hardware
                lscpu | grep -E "Socket|Core|Thread|NUMA"
                cat /sys/devices/system/node/node0/cpulist


                STEP 2: Configure isolcpus + nohz_full
                ─────────────────────────────────────────
                sudo vi /etc/default/grub

                # Add to GRUB_CMDLINE_LINUX:
                # isolcpus=4,5,6,7 nohz_full=4,5,6,7 rcu_nocbs=4,5,6,7 irqaffinity=0-3

                sudo grub2-mkconfig -o /boot/grub2/grub.cfg
                sudo reboot

                # Verify after reboot:
                cat /sys/devices/system/cpu/isolated   # should show: 4-7


                STEP 3: Set tuned profile
                ──────────────────────────
                sudo tuned-adm profile latency-performance

                # Disable C-states:
                sudo cpupower idle-set -D 0

                # Set performance governor:
                for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
                    echo performance | sudo tee $cpu
                done


                STEP 4: Configure huge pages
                ─────────────────────────────
                echo 4096 | sudo tee /proc/sys/vm/nr_hugepages
                echo "vm.nr_hugepages=4096" | sudo tee /etc/sysctl.d/99-hugepages.conf
                sudo sysctl -p /etc/sysctl.d/99-hugepages.conf


                STEP 5: Move all IRQs to cores 0-3
                ────────────────────────────────────
                # Move all IRQ handling to non-isolated cores
                sudo systemctl start irqbalance
                sudo vi /etc/sysconfig/irqbalance
                # Add: IRQBALANCE_BANNED_CPUS="00f0"   # hex bitmask: 0xF0 = cores 4-7
                sudo systemctl restart irqbalance


                STEP 6: Deploy Java app with pinning
                ─────────────────────────────────────
                # /etc/systemd/system/trading-app.service
                [Service]
                CPUAffinity=4 5 6 7
                NUMAPolicy=bind
                NUMAMask=0
                MemorySwapMax=0
                ExecStart=java \\
                    -Xms8g -Xmx8g \\
                    -XX:+UseZGC -XX:+ZGenerational \\
                    -XX:+AlwaysPreTouch \\
                    -XX:+UseLargePages \\
                    -XX:ConcGCThreads=2 \\
                    -jar /opt/trading/app.jar


                STEP 7: Inside Java — pin hot threads to specific cores
                ────────────────────────────────────────────────────────
                // Add to pom.xml: net.openhft:Java-Thread-Affinity:3.23.3

                // Feed handler thread: pin to core 4
                Thread feedThread = new Thread(() -> {
                    try (AffinityLock al = AffinityLock.acquireLock(4)) {
                        feedHandler.run();
                    }
                }, "feed-handler");

                // Order book thread: pin to core 5
                Thread bookThread = new Thread(() -> {
                    try (AffinityLock al = AffinityLock.acquireLock(5)) {
                        orderBook.processUpdates();
                    }
                }, "order-book");

                // Strategy thread: pin to core 6
                Thread strategyThread = new Thread(() -> {
                    try (AffinityLock al = AffinityLock.acquireLock(6)) {
                        strategy.run();
                    }
                }, "strategy");

                // Order sender: pin to core 7
                Thread senderThread = new Thread(() -> {
                    try (AffinityLock al = AffinityLock.acquireLock(7)) {
                        orderSender.run();
                    }
                }, "order-sender");


                STEP 8: Verify pinning is working
                ──────────────────────────────────
                # Get JVM PID
                PID=$(pgrep -f trading-app.jar)

                # Show all threads and which CPU they're on:
                ps -eLo pid,tid,psr,comm | grep $PID
                # PSR column = current CPU core

                # Check thread-level affinity (TID from above):
                taskset -p <TID>
                # Affinity mask should be 0x10 for core 4 (bit 4 = 0x10)

                # Monitor for CPU migration (should be 0 for pinned threads):
                perf stat -e migrations -p $PID sleep 10

                # Check NUMA hits vs misses:
                numastat -p $PID
                """);
        }
    }

    // =========================================================================
    // SECTION 9: MONITORING & VERIFICATION
    // =========================================================================
    static class MonitoringTools {

        static void printMonitoringCommands() {
            System.out.println("""
                ============================================================
                Monitoring Thread Pinning on RHEL
                ============================================================

                --- Verify a thread is pinned ---

                # Show all Java threads + CPU they're currently on:
                ps -eLo pid,tid,psr,comm | grep java
                # PSR = Processor (current CPU). Should not change for pinned thread.

                # Get Linux TID from Java thread name:
                jstack <PID> | grep -A1 "feed-handler"
                # Look for: nid=0x<HEX>   ← this is TID in hex
                # Convert: printf "%d\\n" 0x<HEX>

                # Check affinity mask of a specific TID:
                taskset -p <TID>
                # Output: pid <TID>'s current affinity mask: 10
                # 0x10 = binary 10000 = core 4


                --- Detect CPU migration (should be 0 if pinned) ---

                # Using perf:
                sudo perf stat -e cpu-migrations -p <PID> sleep 5
                # cpu-migrations: 0   ← good, thread is pinned
                # cpu-migrations: 150 ← bad, thread is migrating

                # Using /proc:
                grep voluntary_ctxt_switches /proc/<PID>/task/<TID>/status
                grep nonvoluntary_ctxt_switches /proc/<PID>/task/<TID>/status


                --- Measure scheduling jitter ---

                # cyclictest: measures timer latency (RHEL rt-tests package)
                sudo dnf install rt-tests

                # Run on isolated core 4, 10000 samples:
                sudo cyclictest -c 4 -l 10000 -p 90 -t 1
                # Target: max latency < 20µs on isolated cores
                # Without isolation: max latency can be > 1ms


                --- NUMA access monitoring ---

                numastat -p <PID>
                # numa_hit     → accesses from local NUMA node (GOOD)
                # numa_miss    → accesses from remote NUMA node (BAD)
                # Should be: numa_miss = 0 if running with --membind=0


                --- CPU frequency verification ---

                # Confirm cores are at max frequency (not throttled):
                cpupower -c 4,5,6,7 frequency-info | grep "current CPU"
                # Expected: current CPU frequency: 3.80 GHz (max)

                watch -n1 "cat /sys/devices/system/cpu/cpu4/cpufreq/scaling_cur_freq"


                --- Interrupt verification (IRQs off hot cores) ---

                # Show which CPUs handle each IRQ:
                cat /proc/interrupts | head -20

                # Confirm no IRQs on cores 4-7:
                # Columns 4-7 should all be 0 after irqaffinity setup
                """);
        }
    }

    // =========================================================================
    // SECTION 10: QUICK REFERENCE TABLE
    // =========================================================================
    static void printQuickReference() {
        System.out.println("""
            ============================================================
            QUICK REFERENCE: Java Thread Pinning on RHEL
            ============================================================

            ┌──────────────────────────┬───────────────────────────┬────────────┐
            │ Method                   │ Scope                     │ Latency    │
            │                          │                           │ Impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ isolcpus + nohz_full     │ Kernel removes core from  │ Highest    │
            │ (GRUB param)             │ scheduler entirely        │ impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ AffinityLock (OpenHFT)   │ Per Java thread, uses     │ High       │
            │                          │ sched_setaffinity         │ impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ taskset -c               │ Whole JVM process or      │ Medium     │
            │                          │ per Linux TID             │ impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ numactl --cpunodebind    │ NUMA node + CPU binding   │ Medium     │
            │                          │ reduces memory latency    │ impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ systemd CPUAffinity=     │ Service-level, all threads│ Medium     │
            │                          │ of JVM on those cores     │ impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ cgroups v2 cpuset        │ cgroup-level, container   │ Medium     │
            │                          │ or service boundary       │ impact     │
            ├──────────────────────────┼───────────────────────────┼────────────┤
            │ tuned latency-performance│ Whole system tuning       │ Low-Medium │
            │                          │ (freq, C-states, IRQ)     │ impact     │
            └──────────────────────────┴───────────────────────────┴────────────┘

            BEST PRACTICE STACK (use all together):
              1. isolcpus=4,5,6,7 nohz_full=4,5,6,7 (GRUB)
              2. tuned-adm profile latency-performance
              3. numactl --cpunodebind=0 --membind=0
              4. taskset -c 4,5,6,7 java [JVM flags] -jar app.jar
              5. AffinityLock.acquireLock(N) for each hot Java thread

            VERIFY:
              ps -eLo pid,tid,psr,comm | grep java   → check PSR column (current CPU)
              perf stat -e cpu-migrations -p <PID>   → should be 0
              cyclictest -c 4 -l 10000               → should be < 20µs
              numastat -p <PID>                      → numa_miss should be 0
            """);
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) {
        System.out.println("=".repeat(70));
        System.out.println("  Java Thread Pinning & CPU Affinity on RHEL — Complete Guide");
        System.out.println("=".repeat(70));

        OpenHFTAffinityDemo.printInstructions();
        JNAAffinityDemo.printInstructions();
        OSLevelPinning.printRHELCommands();
        IsolCPUsSetup.printSetupSteps();
        CgroupsCpusetSetup.printSetupSteps();
        TunedProfiles.printSetupSteps();
        GCThreadPinning.printJVMFlags();
        ProductionSetup.printCompleteSetup();
        MonitoringTools.printMonitoringCommands();
        printQuickReference();

        System.out.println("\n✅ Guide complete.");

        // Runtime info about current JVM
        System.out.println("\n--- Current JVM Runtime Info ---");
        System.out.println("Available processors: "
            + Runtime.getRuntime().availableProcessors());
        System.out.println("JVM PID: "
            + ProcessHandle.current().pid());
        System.out.println("Java version: "
            + System.getProperty("java.version"));
        System.out.println("OS: "
            + System.getProperty("os.name") + " "
            + System.getProperty("os.arch"));

        // Show all current thread names
        System.out.println("\n--- Active JVM Threads (pin these in production) ---");
        Thread.getAllStackTraces().keySet().stream()
            .sorted(Comparator.comparing(Thread::getName))
            .forEach(t -> System.out.printf("  Thread: %-30s  ID: %d  Daemon: %b%n",
                t.getName(), t.threadId(), t.isDaemon()));
    }
}

