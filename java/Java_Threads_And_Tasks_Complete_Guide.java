/**
 * JAVA THREADS AND TASKS - COMPLETE GUIDE
 *
 * Purpose: Comprehensive guide to creating and managing threads in Java
 * From basic Thread class to modern CompletableFuture and Virtual Threads (Java 21+)
 *
 * Contents:
 * 1. Basic Thread Creation (extends Thread, implements Runnable)
 * 2. ExecutorService Framework
 * 3. Callable and Future
 * 4. ThreadPoolExecutor Configuration
 * 5. ScheduledExecutorService
 * 6. ForkJoinPool and Parallel Streams
 * 7. CompletableFuture (Async Programming)
 * 8. Virtual Threads (Java 19+, Production in Java 21)
 * 9. Thread Groups and Thread Local
 * 10. Real-World Trading System Examples
 */

package com.trading.threads;

import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.stream.*;

// ============================================================================
// PART 1: BASIC THREAD CREATION
// ============================================================================

/**
 * Method 1: Extending Thread class
 */
class ThreadByExtending extends Thread {
    private final String taskName;

    public ThreadByExtending(String taskName) {
        this.taskName = taskName;
    }

    @Override
    public void run() {
        System.out.println("[" + taskName + "] Running on thread: " +
                         Thread.currentThread().getName());

        // Simulate work
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("[" + taskName + "] Completed");
    }

    public static void demonstrateExtendingThread() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("METHOD 1: EXTENDING THREAD CLASS");
        System.out.println("=".repeat(80) + "\n");

        Thread t1 = new ThreadByExtending("Task-1");
        Thread t2 = new ThreadByExtending("Task-2");
        Thread t3 = new ThreadByExtending("Task-3");

        t1.start();
        t2.start();
        t3.start();

        try {
            t1.join();
            t2.join();
            t3.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ All threads completed");
        System.out.println("✗ Limitation: Cannot extend another class (single inheritance)");
    }
}

/**
 * Method 2: Implementing Runnable interface (Preferred)
 */
class TaskByRunnable implements Runnable {
    private final String taskName;

    public TaskByRunnable(String taskName) {
        this.taskName = taskName;
    }

    @Override
    public void run() {
        System.out.println("[" + taskName + "] Running on thread: " +
                         Thread.currentThread().getName());

        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("[" + taskName + "] Completed");
    }

    public static void demonstrateRunnableInterface() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("METHOD 2: IMPLEMENTING RUNNABLE INTERFACE");
        System.out.println("=".repeat(80) + "\n");

        Thread t1 = new Thread(new TaskByRunnable("Task-1"));
        Thread t2 = new Thread(new TaskByRunnable("Task-2"));
        Thread t3 = new Thread(new TaskByRunnable("Task-3"));

        t1.start();
        t2.start();
        t3.start();

        try {
            t1.join();
            t2.join();
            t3.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ All tasks completed");
        System.out.println("✓ Better: Can extend other classes, composition over inheritance");
    }
}

/**
 * Method 3: Using Lambda Expressions (Java 8+)
 */
class ThreadWithLambda {

    public static void demonstrateLambdaThreads() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("METHOD 3: LAMBDA EXPRESSIONS (JAVA 8+)");
        System.out.println("=".repeat(80) + "\n");

        // Lambda for Runnable
        Thread t1 = new Thread(() -> {
            System.out.println("[Lambda-1] Running on: " + Thread.currentThread().getName());
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        });

        Thread t2 = new Thread(() -> {
            System.out.println("[Lambda-2] Running on: " + Thread.currentThread().getName());
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        });

        t1.start();
        t2.start();

        try {
            t1.join();
            t2.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ Concise syntax with lambdas");
        System.out.println("✓ Cleaner code for simple tasks");
    }
}

// ============================================================================
// PART 2: EXECUTOR SERVICE FRAMEWORK
// ============================================================================

/**
 * ExecutorService - Thread Pool Management
 */
class ExecutorServiceExamples {

    /**
     * Fixed Thread Pool - Fixed number of threads
     */
    public static void demonstrateFixedThreadPool() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("EXECUTOR SERVICE: FIXED THREAD POOL");
        System.out.println("=".repeat(80) + "\n");

        // Create pool with 3 threads
        ExecutorService executor = Executors.newFixedThreadPool(3);

        System.out.println("Submitting 10 tasks to pool of 3 threads...\n");

        for (int i = 1; i <= 10; i++) {
            final int taskId = i;
            executor.submit(() -> {
                System.out.println("Task-" + taskId + " running on: " +
                                 Thread.currentThread().getName());
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
        }

        executor.shutdown();
        try {
            executor.awaitTermination(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ Fixed pool reuses threads efficiently");
        System.out.println("✓ Good for: Known workload, bounded concurrency");
    }

    /**
     * Cached Thread Pool - Creates threads as needed
     */
    public static void demonstrateCachedThreadPool() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("EXECUTOR SERVICE: CACHED THREAD POOL");
        System.out.println("=".repeat(80) + "\n");

        ExecutorService executor = Executors.newCachedThreadPool();

        System.out.println("Submitting tasks to cached thread pool...\n");

        for (int i = 1; i <= 5; i++) {
            final int taskId = i;
            executor.submit(() -> {
                System.out.println("Task-" + taskId + " on: " +
                                 Thread.currentThread().getName());
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
        }

        executor.shutdown();
        try {
            executor.awaitTermination(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ Creates threads on demand, reuses idle threads");
        System.out.println("✓ Good for: Short-lived async tasks");
        System.out.println("✗ Risk: Unbounded thread creation under load");
    }

    /**
     * Single Thread Executor - Sequential execution
     */
    public static void demonstrateSingleThreadExecutor() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("EXECUTOR SERVICE: SINGLE THREAD EXECUTOR");
        System.out.println("=".repeat(80) + "\n");

        ExecutorService executor = Executors.newSingleThreadExecutor();

        System.out.println("Submitting tasks to single thread executor...\n");

        for (int i = 1; i <= 5; i++) {
            final int taskId = i;
            executor.submit(() -> {
                System.out.println("Task-" + taskId + " on: " +
                                 Thread.currentThread().getName());
            });
        }

        executor.shutdown();
        try {
            executor.awaitTermination(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ Guarantees sequential execution order");
        System.out.println("✓ Good for: Event queue, task sequencing");
    }

    /**
     * Work Stealing Pool (ForkJoinPool) - Java 8+
     */
    public static void demonstrateWorkStealingPool() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("EXECUTOR SERVICE: WORK STEALING POOL");
        System.out.println("=".repeat(80) + "\n");

        // Creates pool with parallelism = number of CPU cores
        ExecutorService executor = Executors.newWorkStealingPool();

        System.out.println("Available processors: " + Runtime.getRuntime().availableProcessors());
        System.out.println("Submitting tasks to work-stealing pool...\n");

        List<Future<?>> futures = new ArrayList<>();

        for (int i = 1; i <= 10; i++) {
            final int taskId = i;
            futures.add(executor.submit(() -> {
                System.out.println("Task-" + taskId + " on: " +
                                 Thread.currentThread().getName());
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }));
        }

        // Wait for all tasks
        futures.forEach(f -> {
            try {
                f.get();
            } catch (Exception e) {
                e.printStackTrace();
            }
        });

        executor.shutdown();

        System.out.println("\n✓ Work-stealing: Idle threads steal tasks from busy threads");
        System.out.println("✓ Good for: CPU-intensive parallel tasks");
    }
}

// ============================================================================
// PART 3: CALLABLE AND FUTURE - TASKS WITH RETURN VALUES
// ============================================================================

/**
 * Callable - Like Runnable but can return a result and throw exceptions
 */
class CallableAndFutureExamples {

    static class PriceCalculator implements Callable<Double> {
        private final String symbol;

        public PriceCalculator(String symbol) {
            this.symbol = symbol;
        }

        @Override
        public Double call() throws Exception {
            System.out.println("Calculating price for " + symbol +
                             " on thread: " + Thread.currentThread().getName());

            // Simulate calculation
            Thread.sleep(100);

            // Return calculated price
            return Math.random() * 1000;
        }
    }

    public static void demonstrateCallableAndFuture() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("CALLABLE AND FUTURE - TASKS WITH RETURN VALUES");
        System.out.println("=".repeat(80) + "\n");

        ExecutorService executor = Executors.newFixedThreadPool(3);

        // Submit Callable tasks
        Future<Double> applePrice = executor.submit(new PriceCalculator("AAPL"));
        Future<Double> googlePrice = executor.submit(new PriceCalculator("GOOGL"));
        Future<Double> msftPrice = executor.submit(new PriceCalculator("MSFT"));

        try {
            // Get results (blocking calls)
            System.out.println("AAPL Price: $" + String.format("%.2f", applePrice.get()));
            System.out.println("GOOGL Price: $" + String.format("%.2f", googlePrice.get()));
            System.out.println("MSFT Price: $" + String.format("%.2f", msftPrice.get()));

        } catch (InterruptedException | ExecutionException e) {
            e.printStackTrace();
        }

        executor.shutdown();

        System.out.println("\n✓ Callable can return values");
        System.out.println("✓ Future.get() blocks until result is ready");
        System.out.println("✓ Can throw checked exceptions");
    }

    /**
     * invokeAll - Execute multiple tasks and wait for all
     */
    public static void demonstrateInvokeAll() throws InterruptedException {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("INVOKE ALL - BATCH EXECUTION");
        System.out.println("=".repeat(80) + "\n");

        ExecutorService executor = Executors.newFixedThreadPool(3);

        List<Callable<Double>> tasks = Arrays.asList(
            new PriceCalculator("AAPL"),
            new PriceCalculator("GOOGL"),
            new PriceCalculator("MSFT"),
            new PriceCalculator("AMZN"),
            new PriceCalculator("TSLA")
        );

        // Execute all tasks and wait
        List<Future<Double>> results = executor.invokeAll(tasks);

        System.out.println("\nAll calculations completed:");
        for (int i = 0; i < results.size(); i++) {
            try {
                System.out.println("Task " + i + " result: $" +
                                 String.format("%.2f", results.get(i).get()));
            } catch (ExecutionException e) {
                e.printStackTrace();
            }
        }

        executor.shutdown();

        System.out.println("\n✓ invokeAll waits for ALL tasks to complete");
    }

    /**
     * invokeAny - Returns result of first completed task
     */
    public static void demonstrateInvokeAny() throws InterruptedException, ExecutionException {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("INVOKE ANY - FIRST RESULT WINS");
        System.out.println("=".repeat(80) + "\n");

        ExecutorService executor = Executors.newFixedThreadPool(3);

        List<Callable<String>> tasks = Arrays.asList(
            () -> {
                Thread.sleep(200);
                return "Result from Task 1";
            },
            () -> {
                Thread.sleep(100);
                return "Result from Task 2 (fastest)";
            },
            () -> {
                Thread.sleep(300);
                return "Result from Task 3";
            }
        );

        // Returns result from fastest task
        String result = executor.invokeAny(tasks);
        System.out.println("First result: " + result);

        executor.shutdown();

        System.out.println("\n✓ invokeAny returns first successful result");
        System.out.println("✓ Good for: Redundant computation, fastest wins");
    }
}

// ============================================================================
// PART 4: THREAD POOL EXECUTOR - FINE-GRAINED CONTROL
// ============================================================================

class ThreadPoolExecutorExamples {

    public static void demonstrateCustomThreadPool() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("THREAD POOL EXECUTOR - CUSTOM CONFIGURATION");
        System.out.println("=".repeat(80) + "\n");

        // Custom thread pool with fine-grained control
        ThreadPoolExecutor executor = new ThreadPoolExecutor(
            2,                              // corePoolSize
            4,                              // maximumPoolSize
            60L,                            // keepAliveTime
            TimeUnit.SECONDS,               // time unit
            new LinkedBlockingQueue<>(10),  // work queue
            new CustomThreadFactory("Worker"),
            new ThreadPoolExecutor.CallerRunsPolicy()  // rejection policy
        );

        System.out.println("Thread Pool Configuration:");
        System.out.println("  Core Pool Size: " + executor.getCorePoolSize());
        System.out.println("  Max Pool Size: " + executor.getMaximumPoolSize());
        System.out.println("  Queue Capacity: 10");
        System.out.println();

        // Submit tasks
        for (int i = 1; i <= 15; i++) {
            final int taskId = i;
            executor.submit(() -> {
                System.out.println("Task-" + taskId + " executing on: " +
                                 Thread.currentThread().getName());
                try {
                    Thread.sleep(200);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
        }

        // Monitor pool
        System.out.println("\nPool Stats:");
        System.out.println("  Active Threads: " + executor.getActiveCount());
        System.out.println("  Pool Size: " + executor.getPoolSize());
        System.out.println("  Queue Size: " + executor.getQueue().size());

        executor.shutdown();
        try {
            executor.awaitTermination(10, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\n✓ Fine-grained control over thread pool behavior");
        System.out.println("✓ Custom queue, rejection policy, thread factory");
    }

    /**
     * Custom Thread Factory
     */
    static class CustomThreadFactory implements ThreadFactory {
        private final AtomicInteger threadNumber = new AtomicInteger(1);
        private final String prefix;

        public CustomThreadFactory(String prefix) {
            this.prefix = prefix;
        }

        @Override
        public Thread newThread(Runnable r) {
            Thread thread = new Thread(r, prefix + "-" + threadNumber.getAndIncrement());
            thread.setDaemon(false);
            thread.setPriority(Thread.NORM_PRIORITY);
            return thread;
        }
    }

    /**
     * Different Rejection Policies
     */
    public static void demonstrateRejectionPolicies() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("REJECTION POLICIES");
        System.out.println("=".repeat(80) + "\n");

        System.out.println("Available Rejection Policies:");
        System.out.println("1. AbortPolicy (default) - Throws RejectedExecutionException");
        System.out.println("2. CallerRunsPolicy - Executes task in caller's thread");
        System.out.println("3. DiscardPolicy - Silently discards rejected task");
        System.out.println("4. DiscardOldestPolicy - Discards oldest unhandled task");

        // Example: CallerRunsPolicy
        ThreadPoolExecutor executor = new ThreadPoolExecutor(
            1, 1, 0L, TimeUnit.SECONDS,
            new LinkedBlockingQueue<>(2),
            new ThreadPoolExecutor.CallerRunsPolicy()
        );

        for (int i = 1; i <= 5; i++) {
            final int taskId = i;
            executor.submit(() -> {
                System.out.println("Task-" + taskId + " on: " +
                                 Thread.currentThread().getName());
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
        }

        executor.shutdown();
        try {
            executor.awaitTermination(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}

// ============================================================================
// PART 5: SCHEDULED EXECUTOR SERVICE - DELAYED AND PERIODIC TASKS
// ============================================================================

class ScheduledExecutorExamples {

    public static void demonstrateScheduledTasks() throws InterruptedException {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("SCHEDULED EXECUTOR SERVICE");
        System.out.println("=".repeat(80) + "\n");

        ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(2);

        // 1. Schedule one-time delayed task
        System.out.println("Scheduling delayed task (2 seconds)...");
        scheduler.schedule(() -> {
            System.out.println("[Delayed Task] Executed after delay");
        }, 2, TimeUnit.SECONDS);

        // 2. Schedule periodic task at fixed rate
        System.out.println("Scheduling periodic task (fixed rate, every 1 second)...");
        ScheduledFuture<?> periodicTask = scheduler.scheduleAtFixedRate(() -> {
            System.out.println("[Periodic Task] Tick at " + System.currentTimeMillis());
        }, 0, 1, TimeUnit.SECONDS);

        // Let it run for a while
        Thread.sleep(5000);

        // Cancel periodic task
        periodicTask.cancel(false);

        // 3. Schedule with fixed delay
        scheduler.scheduleWithFixedDelay(() -> {
            System.out.println("[Fixed Delay Task] Executing...");
            try {
                Thread.sleep(500); // Task takes 500ms
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }, 0, 1, TimeUnit.SECONDS);

        Thread.sleep(3000);

        scheduler.shutdown();

        System.out.println("\n✓ schedule() - One-time delayed execution");
        System.out.println("✓ scheduleAtFixedRate() - Fixed period between starts");
        System.out.println("✓ scheduleWithFixedDelay() - Fixed delay between end and next start");
    }
}

// ============================================================================
// PART 6: FORK-JOIN POOL - DIVIDE AND CONQUER
// ============================================================================

class ForkJoinPoolExamples {

    /**
     * RecursiveTask - Returns a result
     */
    static class SumTask extends RecursiveTask<Long> {
        private final long[] array;
        private final int start;
        private final int end;
        private static final int THRESHOLD = 1000;

        public SumTask(long[] array, int start, int end) {
            this.array = array;
            this.start = start;
            this.end = end;
        }

        @Override
        protected Long compute() {
            if (end - start <= THRESHOLD) {
                // Base case: compute directly
                long sum = 0;
                for (int i = start; i < end; i++) {
                    sum += array[i];
                }
                return sum;
            } else {
                // Recursive case: split task
                int mid = (start + end) / 2;
                SumTask leftTask = new SumTask(array, start, mid);
                SumTask rightTask = new SumTask(array, mid, end);

                // Fork left task
                leftTask.fork();

                // Compute right task in current thread
                long rightResult = rightTask.compute();

                // Join left task
                long leftResult = leftTask.join();

                return leftResult + rightResult;
            }
        }
    }

    public static void demonstrateForkJoinPool() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("FORK-JOIN POOL - DIVIDE AND CONQUER");
        System.out.println("=".repeat(80) + "\n");

        // Create array
        long[] array = new long[10000];
        for (int i = 0; i < array.length; i++) {
            array[i] = i + 1;
        }

        ForkJoinPool pool = new ForkJoinPool();

        SumTask task = new SumTask(array, 0, array.length);
        long result = pool.invoke(task);

        System.out.println("Sum of 1 to 10000: " + result);
        System.out.println("Expected: " + (10000L * 10001L / 2));

        pool.shutdown();

        System.out.println("\n✓ Fork-Join framework for recursive parallelism");
        System.out.println("✓ Work-stealing algorithm for load balancing");
        System.out.println("✓ Good for: Divide-and-conquer algorithms");
    }

    /**
     * Parallel Streams use ForkJoinPool.commonPool()
     */
    public static void demonstrateParallelStreams() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PARALLEL STREAMS - BUILT ON FORK-JOIN");
        System.out.println("=".repeat(80) + "\n");

        List<Integer> numbers = IntStream.rangeClosed(1, 100)
                                        .boxed()
                                        .collect(Collectors.toList());

        // Sequential stream
        long startTime = System.nanoTime();
        long sum1 = numbers.stream()
                          .mapToLong(i -> i * i)
                          .sum();
        long sequentialTime = System.nanoTime() - startTime;

        // Parallel stream
        startTime = System.nanoTime();
        long sum2 = numbers.parallelStream()
                          .mapToLong(i -> i * i)
                          .sum();
        long parallelTime = System.nanoTime() - startTime;

        System.out.println("Sum of squares (1-100): " + sum1);
        System.out.println("Sequential time: " + (sequentialTime / 1000) + " μs");
        System.out.println("Parallel time: " + (parallelTime / 1000) + " μs");

        System.out.println("\n✓ Parallel streams use ForkJoinPool.commonPool()");
        System.out.println("✓ Easy parallelization of stream operations");
    }
}

// ============================================================================
// PART 7: COMPLETABLE FUTURE - ASYNC PROGRAMMING
// ============================================================================

class CompletableFutureExamples {

    public static void demonstrateBasicCompletableFuture() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("COMPLETABLE FUTURE - ASYNC PROGRAMMING");
        System.out.println("=".repeat(80) + "\n");

        // Simple async computation
        CompletableFuture<String> future = CompletableFuture.supplyAsync(() -> {
            System.out.println("Fetching data on: " + Thread.currentThread().getName());
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            return "Market Data";
        });

        // Attach callback
        future.thenAccept(result -> {
            System.out.println("Received: " + result + " on " +
                             Thread.currentThread().getName());
        });

        System.out.println("Main thread continues...");

        // Wait for completion
        future.get();

        System.out.println("\n✓ Non-blocking async computation");
        System.out.println("✓ Composable and chainable");
    }

    public static void demonstrateChaining() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("COMPLETABLE FUTURE - CHAINING");
        System.out.println("=".repeat(80) + "\n");

        CompletableFuture<Double> future = CompletableFuture
            .supplyAsync(() -> {
                System.out.println("Step 1: Fetch price");
                return 100.0;
            })
            .thenApply(price -> {
                System.out.println("Step 2: Apply discount");
                return price * 0.9;
            })
            .thenApply(discountedPrice -> {
                System.out.println("Step 3: Add tax");
                return discountedPrice * 1.1;
            });

        Double finalPrice = future.get();
        System.out.println("\nFinal price: $" + String.format("%.2f", finalPrice));

        System.out.println("\n✓ Chain transformations with thenApply()");
    }

    public static void demonstrateCombining() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("COMPLETABLE FUTURE - COMBINING FUTURES");
        System.out.println("=".repeat(80) + "\n");

        CompletableFuture<Double> priceNYSE = CompletableFuture.supplyAsync(() -> {
            System.out.println("Fetching NYSE price...");
            sleep(100);
            return 100.0;
        });

        CompletableFuture<Double> priceNASDAQ = CompletableFuture.supplyAsync(() -> {
            System.out.println("Fetching NASDAQ price...");
            sleep(150);
            return 100.5;
        });

        // Combine two futures
        CompletableFuture<Double> bestPrice = priceNYSE.thenCombine(priceNASDAQ,
            (nyse, nasdaq) -> {
                System.out.println("Comparing prices...");
                return Math.min(nyse, nasdaq);
            });

        System.out.println("Best price: $" + String.format("%.2f", bestPrice.get()));

        System.out.println("\n✓ Combine multiple async operations");
    }

    public static void demonstrateAllOf() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("COMPLETABLE FUTURE - ALL OF");
        System.out.println("=".repeat(80) + "\n");

        CompletableFuture<String> future1 = CompletableFuture.supplyAsync(() -> {
            sleep(100);
            return "AAPL";
        });

        CompletableFuture<String> future2 = CompletableFuture.supplyAsync(() -> {
            sleep(150);
            return "GOOGL";
        });

        CompletableFuture<String> future3 = CompletableFuture.supplyAsync(() -> {
            sleep(80);
            return "MSFT";
        });

        // Wait for all to complete
        CompletableFuture<Void> allFutures = CompletableFuture.allOf(
            future1, future2, future3
        );

        allFutures.get();

        System.out.println("All completed:");
        System.out.println("  " + future1.get());
        System.out.println("  " + future2.get());
        System.out.println("  " + future3.get());

        System.out.println("\n✓ Wait for multiple futures to complete");
    }

    public static void demonstrateAnyOf() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("COMPLETABLE FUTURE - ANY OF");
        System.out.println("=".repeat(80) + "\n");

        CompletableFuture<String> venue1 = CompletableFuture.supplyAsync(() -> {
            sleep(200);
            return "NYSE: $100.00";
        });

        CompletableFuture<String> venue2 = CompletableFuture.supplyAsync(() -> {
            sleep(100);
            return "NASDAQ: $100.50";
        });

        CompletableFuture<String> venue3 = CompletableFuture.supplyAsync(() -> {
            sleep(150);
            return "ARCA: $100.25";
        });

        // Get first completed result
        CompletableFuture<Object> firstCompleted = CompletableFuture.anyOf(
            venue1, venue2, venue3
        );

        System.out.println("First result: " + firstCompleted.get());

        System.out.println("\n✓ Get first completed future (fastest wins)");
    }

    public static void demonstrateExceptionHandling() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("COMPLETABLE FUTURE - EXCEPTION HANDLING");
        System.out.println("=".repeat(80) + "\n");

        CompletableFuture<String> future = CompletableFuture
            .supplyAsync(() -> {
                if (Math.random() > 0.5) {
                    throw new RuntimeException("Random error!");
                }
                return "Success";
            })
            .exceptionally(ex -> {
                System.out.println("Caught exception: " + ex.getMessage());
                return "Fallback value";
            })
            .thenApply(result -> {
                System.out.println("Processing: " + result);
                return result.toUpperCase();
            });

        System.out.println("Result: " + future.get());

        System.out.println("\n✓ Handle exceptions with exceptionally()");
        System.out.println("✓ Chain continues with fallback value");
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}

// ============================================================================
// PART 8: VIRTUAL THREADS (JAVA 19+, PRODUCTION IN JAVA 21)
// ============================================================================

class VirtualThreadExamples {

    public static void demonstrateVirtualThreads() throws InterruptedException {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("VIRTUAL THREADS (JAVA 21+)");
        System.out.println("=".repeat(80) + "\n");

        // Check Java version
        String javaVersion = System.getProperty("java.version");
        System.out.println("Java Version: " + javaVersion);

        // Note: This code requires Java 21+
        // Uncomment when running on Java 21+

        /*
        // Create virtual thread
        Thread virtualThread = Thread.ofVirtual().start(() -> {
            System.out.println("Running in virtual thread: " +
                             Thread.currentThread());
        });

        virtualThread.join();

        // Create many virtual threads (millions possible!)
        try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
            for (int i = 0; i < 10000; i++) {
                final int taskId = i;
                executor.submit(() -> {
                    try {
                        Thread.sleep(1000);
                        if (taskId < 5) {
                            System.out.println("Virtual thread task " + taskId);
                        }
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                });
            }
        }
        */

        System.out.println("\n✓ Virtual threads are lightweight (millions possible)");
        System.out.println("✓ Managed by JVM, not OS");
        System.out.println("✓ Perfect for high-concurrency I/O workloads");
        System.out.println("✓ Much cheaper than platform threads");
        System.out.println("\nNote: Requires Java 21+ to run");
    }

    public static void compareVirtualVsPlatformThreads() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("VIRTUAL vs PLATFORM THREADS COMPARISON");
        System.out.println("=".repeat(80) + "\n");

        System.out.println("PLATFORM THREADS (Traditional):");
        System.out.println("  - OS-level threads");
        System.out.println("  - ~1MB stack memory per thread");
        System.out.println("  - Limited to thousands of threads");
        System.out.println("  - Context switch overhead");
        System.out.println("  - Good for: CPU-intensive tasks");

        System.out.println("\nVIRTUAL THREADS (Java 21+):");
        System.out.println("  - JVM-managed threads");
        System.out.println("  - Very small memory footprint");
        System.out.println("  - Can create millions");
        System.out.println("  - Mounted on platform threads");
        System.out.println("  - Good for: I/O-intensive, high-concurrency");
    }
}

// ============================================================================
// PART 9: THREAD GROUPS AND THREAD LOCAL
// ============================================================================

class ThreadGroupAndThreadLocalExamples {

    public static void demonstrateThreadGroups() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("THREAD GROUPS");
        System.out.println("=".repeat(80) + "\n");

        ThreadGroup group = new ThreadGroup("WorkerGroup");

        Thread t1 = new Thread(group, () -> {
            System.out.println("Thread 1 in group: " +
                             Thread.currentThread().getThreadGroup().getName());
        });

        Thread t2 = new Thread(group, () -> {
            System.out.println("Thread 2 in group: " +
                             Thread.currentThread().getThreadGroup().getName());
        });

        t1.start();
        t2.start();

        try {
            t1.join();
            t2.join();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        System.out.println("\nActive threads in group: " + group.activeCount());

        System.out.println("\n✓ Group related threads together");
        System.out.println("✗ Mostly legacy, prefer ExecutorService");
    }

    /**
     * ThreadLocal - Thread-specific data
     */
    static class ThreadLocalExample {
        private static ThreadLocal<String> threadLocal = ThreadLocal.withInitial(() -> "Default");

        public static void demonstrateThreadLocal() throws InterruptedException {
            System.out.println("\n" + "=".repeat(80));
            System.out.println("THREAD LOCAL - THREAD-SPECIFIC DATA");
            System.out.println("=".repeat(80) + "\n");

            Thread t1 = new Thread(() -> {
                threadLocal.set("Thread-1 Data");
                System.out.println("Thread 1 reads: " + threadLocal.get());
            });

            Thread t2 = new Thread(() -> {
                threadLocal.set("Thread-2 Data");
                System.out.println("Thread 2 reads: " + threadLocal.get());
            });

            t1.start();
            t2.start();

            t1.join();
            t2.join();

            System.out.println("Main thread reads: " + threadLocal.get());

            System.out.println("\n✓ Each thread has its own copy of the variable");
            System.out.println("✓ Good for: User sessions, DB connections per thread");
            System.out.println("✗ Risk: Memory leaks with thread pools (use remove())");
        }
    }

    /**
     * InheritableThreadLocal - Propagate to child threads
     */
    static class InheritableThreadLocalExample {
        private static InheritableThreadLocal<String> inherited =
            new InheritableThreadLocal<>();

        public static void demonstrateInheritableThreadLocal() throws InterruptedException {
            System.out.println("\n" + "=".repeat(80));
            System.out.println("INHERITABLE THREAD LOCAL");
            System.out.println("=".repeat(80) + "\n");

            inherited.set("Parent Value");
            System.out.println("Parent set: " + inherited.get());

            Thread child = new Thread(() -> {
                System.out.println("Child inherited: " + inherited.get());
                inherited.set("Child Value");
                System.out.println("Child modified: " + inherited.get());
            });

            child.start();
            child.join();

            System.out.println("Parent still has: " + inherited.get());

            System.out.println("\n✓ Child threads inherit parent's ThreadLocal values");
        }
    }
}

// ============================================================================
// PART 10: REAL-WORLD TRADING SYSTEM EXAMPLES
// ============================================================================

class TradingSystemExamples {

    /**
     * Market Data Handler - Multiple symbols in parallel
     */
    static class MarketDataHandler {
        private final ExecutorService executor;

        public MarketDataHandler() {
            this.executor = Executors.newFixedThreadPool(
                Runtime.getRuntime().availableProcessors()
            );
        }

        public void processMarketData(List<String> symbols) {
            System.out.println("\n=== Processing Market Data for " +
                             symbols.size() + " symbols ===\n");

            List<CompletableFuture<Void>> futures = symbols.stream()
                .map(symbol -> CompletableFuture.runAsync(() -> {
                    processSymbol(symbol);
                }, executor))
                .collect(Collectors.toList());

            // Wait for all to complete
            CompletableFuture.allOf(futures.toArray(new CompletableFuture[0])).join();

            System.out.println("\n✓ All symbols processed");
        }

        private void processSymbol(String symbol) {
            System.out.println("Processing " + symbol + " on " +
                             Thread.currentThread().getName());
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        public void shutdown() {
            executor.shutdown();
        }
    }

    /**
     * Order Processing Pipeline
     */
    static class OrderPipeline {
        private final ExecutorService validationExecutor;
        private final ExecutorService riskExecutor;
        private final ExecutorService executionExecutor;

        public OrderPipeline() {
            this.validationExecutor = Executors.newFixedThreadPool(2);
            this.riskExecutor = Executors.newFixedThreadPool(2);
            this.executionExecutor = Executors.newFixedThreadPool(2);
        }

        public CompletableFuture<String> processOrder(String orderId) {
            return CompletableFuture
                .supplyAsync(() -> validateOrder(orderId), validationExecutor)
                .thenApplyAsync(this::checkRisk, riskExecutor)
                .thenApplyAsync(this::executeOrder, executionExecutor);
        }

        private String validateOrder(String orderId) {
            System.out.println("[Validation] " + orderId);
            sleep(50);
            return orderId;
        }

        private String checkRisk(String orderId) {
            System.out.println("[Risk Check] " + orderId);
            sleep(50);
            return orderId;
        }

        private String executeOrder(String orderId) {
            System.out.println("[Execution] " + orderId);
            sleep(50);
            return "EXECUTED-" + orderId;
        }

        private void sleep(long millis) {
            try {
                Thread.sleep(millis);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        public void shutdown() {
            validationExecutor.shutdown();
            riskExecutor.shutdown();
            executionExecutor.shutdown();
        }
    }

    /**
     * Price Aggregation from Multiple Venues
     */
    static class PriceAggregator {

        public Map<String, Double> getBestPrices(String symbol) {
            CompletableFuture<Double> nysePrice = CompletableFuture.supplyAsync(() -> {
                System.out.println("Fetching " + symbol + " from NYSE");
                sleep(100);
                return 100.0 + Math.random();
            });

            CompletableFuture<Double> nasdaqPrice = CompletableFuture.supplyAsync(() -> {
                System.out.println("Fetching " + symbol + " from NASDAQ");
                sleep(150);
                return 100.0 + Math.random();
            });

            CompletableFuture<Double> arcaPrice = CompletableFuture.supplyAsync(() -> {
                System.out.println("Fetching " + symbol + " from ARCA");
                sleep(80);
                return 100.0 + Math.random();
            });

            // Wait for all
            CompletableFuture.allOf(nysePrice, nasdaqPrice, arcaPrice).join();

            Map<String, Double> prices = new HashMap<>();
            try {
                prices.put("NYSE", nysePrice.get());
                prices.put("NASDAQ", nasdaqPrice.get());
                prices.put("ARCA", arcaPrice.get());
            } catch (Exception e) {
                e.printStackTrace();
            }

            return prices;
        }

        private void sleep(long millis) {
            try {
                Thread.sleep(millis);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    public static void demonstrateTradingExamples() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("REAL-WORLD TRADING SYSTEM EXAMPLES");
        System.out.println("=".repeat(80));

        // Example 1: Market Data Handler
        MarketDataHandler mdHandler = new MarketDataHandler();
        mdHandler.processMarketData(Arrays.asList("AAPL", "GOOGL", "MSFT", "AMZN", "TSLA"));
        mdHandler.shutdown();

        // Example 2: Order Pipeline
        System.out.println("\n=== Order Processing Pipeline ===\n");
        OrderPipeline pipeline = new OrderPipeline();

        List<CompletableFuture<String>> orderFutures = Arrays.asList(
            pipeline.processOrder("ORD-001"),
            pipeline.processOrder("ORD-002"),
            pipeline.processOrder("ORD-003")
        );

        orderFutures.forEach(f -> {
            try {
                System.out.println("Result: " + f.get());
            } catch (Exception e) {
                e.printStackTrace();
            }
        });

        pipeline.shutdown();

        // Example 3: Price Aggregation
        System.out.println("\n=== Price Aggregation ===\n");
        PriceAggregator aggregator = new PriceAggregator();
        Map<String, Double> prices = aggregator.getBestPrices("AAPL");

        System.out.println("\nAggregated Prices:");
        prices.forEach((venue, price) ->
            System.out.println("  " + venue + ": $" + String.format("%.2f", price))
        );

        double bestPrice = prices.values().stream()
            .mapToDouble(Double::doubleValue)
            .min()
            .orElse(0.0);

        System.out.println("\nBest Price: $" + String.format("%.2f", bestPrice));
    }
}

// ============================================================================
// MAIN EXAMPLES RUNNER
// ============================================================================

public class Java_Threads_And_Tasks_Complete_Guide {

    public static void main(String[] args) throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("JAVA THREADS AND TASKS - COMPLETE GUIDE");
        System.out.println("=".repeat(80));

        // Part 1: Basic Thread Creation
        ThreadByExtending.demonstrateExtendingThread();
        TaskByRunnable.demonstrateRunnableInterface();
        ThreadWithLambda.demonstrateLambdaThreads();

        // Part 2: ExecutorService
        ExecutorServiceExamples.demonstrateFixedThreadPool();
        ExecutorServiceExamples.demonstrateCachedThreadPool();
        ExecutorServiceExamples.demonstrateSingleThreadExecutor();
        ExecutorServiceExamples.demonstrateWorkStealingPool();

        // Part 3: Callable and Future
        CallableAndFutureExamples.demonstrateCallableAndFuture();
        CallableAndFutureExamples.demonstrateInvokeAll();
        CallableAndFutureExamples.demonstrateInvokeAny();

        // Part 4: ThreadPoolExecutor
        ThreadPoolExecutorExamples.demonstrateCustomThreadPool();
        ThreadPoolExecutorExamples.demonstrateRejectionPolicies();

        // Part 5: Scheduled Executor
        ScheduledExecutorExamples.demonstrateScheduledTasks();

        // Part 6: Fork-Join Pool
        ForkJoinPoolExamples.demonstrateForkJoinPool();
        ForkJoinPoolExamples.demonstrateParallelStreams();

        // Part 7: CompletableFuture
        CompletableFutureExamples.demonstrateBasicCompletableFuture();
        CompletableFutureExamples.demonstrateChaining();
        CompletableFutureExamples.demonstrateCombining();
        CompletableFutureExamples.demonstrateAllOf();
        CompletableFutureExamples.demonstrateAnyOf();
        CompletableFutureExamples.demonstrateExceptionHandling();

        // Part 8: Virtual Threads
        VirtualThreadExamples.demonstrateVirtualThreads();
        VirtualThreadExamples.compareVirtualVsPlatformThreads();

        // Part 9: Thread Groups and ThreadLocal
        ThreadGroupAndThreadLocalExamples.demonstrateThreadGroups();
        ThreadGroupAndThreadLocalExamples.ThreadLocalExample.demonstrateThreadLocal();
        ThreadGroupAndThreadLocalExamples.InheritableThreadLocalExample.demonstrateInheritableThreadLocal();

        // Part 10: Trading System Examples
        TradingSystemExamples.demonstrateTradingExamples();

        System.out.println("\n" + "=".repeat(80));
        System.out.println("ALL EXAMPLES COMPLETED");
        System.out.println("=".repeat(80));

        // Summary
        printSummary();
    }

    private static void printSummary() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("SUMMARY - WHEN TO USE EACH APPROACH");
        System.out.println("=".repeat(80));

        System.out.println("\n1. Basic Thread (extends Thread / implements Runnable):");
        System.out.println("   Use: Simple, one-off tasks");
        System.out.println("   Avoid: Production systems (no pooling, expensive)");

        System.out.println("\n2. ExecutorService (Fixed/Cached Thread Pool):");
        System.out.println("   Use: Most production scenarios, bounded concurrency");
        System.out.println("   Good: Thread reuse, task queue management");

        System.out.println("\n3. Callable/Future:");
        System.out.println("   Use: Tasks that return results");
        System.out.println("   Good: Get results from async computations");

        System.out.println("\n4. ScheduledExecutorService:");
        System.out.println("   Use: Delayed or periodic tasks");
        System.out.println("   Good: Market snapshots, heartbeats, cleanup jobs");

        System.out.println("\n5. ForkJoinPool:");
        System.out.println("   Use: Recursive divide-and-conquer algorithms");
        System.out.println("   Good: Parallel processing of large datasets");

        System.out.println("\n6. CompletableFuture:");
        System.out.println("   Use: Complex async workflows, composition");
        System.out.println("   Good: Non-blocking pipelines, combining multiple async ops");

        System.out.println("\n7. Virtual Threads (Java 21+):");
        System.out.println("   Use: High-concurrency I/O workloads");
        System.out.println("   Good: Millions of concurrent connections, blocking I/O");

        System.out.println("\n8. ThreadLocal:");
        System.out.println("   Use: Thread-specific data (DB connections, user context)");
        System.out.println("   Caution: Memory leaks with thread pools");

        System.out.println("\n" + "=".repeat(80));
        System.out.println("BEST PRACTICES:");
        System.out.println("=".repeat(80));

        System.out.println("\n✓ Prefer ExecutorService over manual Thread creation");
        System.out.println("✓ Use CompletableFuture for async composition");
        System.out.println("✓ Size thread pools appropriately:");
        System.out.println("    - CPU-bound: # of cores");
        System.out.println("    - I/O-bound: # of cores * (1 + wait/compute ratio)");
        System.out.println("✓ Always shutdown executors (use try-with-resources)");
        System.out.println("✓ Handle InterruptedException properly");
        System.out.println("✓ Use virtual threads for I/O-heavy workloads (Java 21+)");
        System.out.println("✓ Monitor thread pools (active threads, queue size)");
        System.out.println("✓ Consider rejection policies for bounded queues");

        System.out.println("\n" + "=".repeat(80));
    }
}

