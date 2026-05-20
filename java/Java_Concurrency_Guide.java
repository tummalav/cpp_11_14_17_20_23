/**
 * Java Concurrency Guide for C++ Developers
 *
 * This file demonstrates Java's concurrency features with trading examples,
 * showing equivalents and differences from C++ threading.
 */

import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;
import java.util.*;
import java.math.BigDecimal;
import java.time.LocalDateTime;

public class Java_Concurrency_Guide {

    // =============================================
    // 1. BASIC THREADING - Thread vs Runnable
    // =============================================

    // Method 1: Extend Thread class (less flexible)
    static class PriceUpdateThread extends Thread {
        private final String symbol;
        private volatile boolean running = true;

        public PriceUpdateThread(String symbol) {
            this.symbol = symbol;
        }

        @Override
        public void run() {
            while (running) {
                System.out.println("Updating price for " + symbol + " at " + LocalDateTime.now());
                try {
                    Thread.sleep(1000);  // Simulate price update interval
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        }

        public void stopUpdating() {
            running = false;
        }
    }

    // Method 2: Implement Runnable interface (preferred - allows multiple inheritance)
    static class OrderProcessor implements Runnable {
        private final BlockingQueue<String> orderQueue;
        private final String processorName;

        public OrderProcessor(BlockingQueue<String> orderQueue, String processorName) {
            this.orderQueue = orderQueue;
            this.processorName = processorName;
        }

        @Override
        public void run() {
            try {
                while (!Thread.currentThread().isInterrupted()) {
                    String order = orderQueue.take();  // Blocks until order available
                    System.out.println(processorName + " processing: " + order);
                    Thread.sleep(100);  // Simulate processing time
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                System.out.println(processorName + " shutting down");
            }
        }
    }

    // =============================================
    // 2. SYNCHRONIZATION - synchronized keyword
    // =============================================

    static class TradingAccount {
        private BigDecimal balance;
        private final Object balanceLock = new Object();  // Explicit lock object

        public TradingAccount(BigDecimal initialBalance) {
            this.balance = initialBalance;
        }

        // Synchronized method (equivalent to mutex lock in C++)
        public synchronized void deposit(BigDecimal amount) {
            balance = balance.add(amount);
            System.out.println("Deposited $" + amount + ", new balance: $" + balance);
        }

        // Synchronized block with explicit lock object
        public void withdraw(BigDecimal amount) {
            synchronized (balanceLock) {
                if (balance.compareTo(amount) >= 0) {
                    balance = balance.subtract(amount);
                    System.out.println("Withdrew $" + amount + ", new balance: $" + balance);
                } else {
                    System.out.println("Insufficient funds for withdrawal of $" + amount);
                }
            }
        }

        public synchronized BigDecimal getBalance() {
            return balance;
        }
    }

    // =============================================
    // 3. EXPLICIT LOCKS - java.util.concurrent.locks
    // =============================================

    static class AdvancedTradingAccount {
        private BigDecimal balance;
        private final ReadWriteLock rwLock = new ReentrantReadWriteLock();
        private final Lock readLock = rwLock.readLock();
        private final Lock writeLock = rwLock.writeLock();
        private final Condition sufficientFunds = writeLock.newCondition();

        public AdvancedTradingAccount(BigDecimal initialBalance) {
            this.balance = initialBalance;
        }

        // Read operation - multiple threads can read simultaneously
        public BigDecimal getBalance() {
            readLock.lock();
            try {
                return balance;
            } finally {
                readLock.unlock();
            }
        }

        // Write operation - exclusive access
        public void deposit(BigDecimal amount) {
            writeLock.lock();
            try {
                balance = balance.add(amount);
                sufficientFunds.signalAll();  // Wake up waiting threads
                System.out.println("Deposited $" + amount + ", new balance: $" + balance);
            } finally {
                writeLock.unlock();
            }
        }

        // Conditional wait example
        public boolean withdrawWithWait(BigDecimal amount, long timeoutMs) {
            writeLock.lock();
            try {
                while (balance.compareTo(amount) < 0) {
                    if (!sufficientFunds.await(timeoutMs, TimeUnit.MILLISECONDS)) {
                        return false;  // Timeout
                    }
                }
                balance = balance.subtract(amount);
                System.out.println("Withdrew $" + amount + ", new balance: $" + balance);
                return true;
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return false;
            } finally {
                writeLock.unlock();
            }
        }
    }

    // =============================================
    // 4. ATOMIC OPERATIONS - java.util.concurrent.atomic
    // =============================================

    static class AtomicCounters {
        private final AtomicLong orderCount = new AtomicLong(0);
        private final AtomicInteger activeConnections = new AtomicInteger(0);
        private final AtomicReference<BigDecimal> lastPrice = new AtomicReference<>(BigDecimal.ZERO);

        // Lock-free increment (equivalent to std::atomic in C++)
        public long incrementOrderCount() {
            return orderCount.incrementAndGet();
        }

        // Compare and swap operation
        public boolean updatePrice(BigDecimal expectedPrice, BigDecimal newPrice) {
            return lastPrice.compareAndSet(expectedPrice, newPrice);
        }

        // Atomic accumulate operation
        public void addConnection() {
            activeConnections.incrementAndGet();
        }

        public void removeConnection() {
            activeConnections.decrementAndGet();
        }

        public void printStats() {
            System.out.println("Order count: " + orderCount.get());
            System.out.println("Active connections: " + activeConnections.get());
            System.out.println("Last price: $" + lastPrice.get());
        }
    }

    // =============================================
    // 5. EXECUTOR FRAMEWORK - Thread Pools
    // =============================================

    static class TradingExecutorService {
        private final ExecutorService orderProcessingPool;
        private final ExecutorService priceUpdatePool;
        private final ScheduledExecutorService scheduledPool;

        public TradingExecutorService() {
            // Fixed thread pool for order processing
            orderProcessingPool = Executors.newFixedThreadPool(4);

            // Cached thread pool for price updates (creates threads as needed)
            priceUpdatePool = Executors.newCachedThreadPool();

            // Scheduled thread pool for periodic tasks
            scheduledPool = Executors.newScheduledThreadPool(2);
        }

        public void processOrder(String orderId, String symbol, BigDecimal price) {
            orderProcessingPool.submit(() -> {
                System.out.println("Processing order " + orderId + " for " + symbol + " at $" + price);
                try {
                    Thread.sleep(500);  // Simulate processing
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
                System.out.println("Completed order " + orderId);
            });
        }

        public Future<BigDecimal> fetchPrice(String symbol) {
            return priceUpdatePool.submit(() -> {
                // Simulate price fetch from market data feed
                Thread.sleep(100);
                return new BigDecimal("150.50").add(new BigDecimal(Math.random() * 10));
            });
        }

        public void startPeriodicTasks() {
            // Schedule at fixed rate (like cron job)
            scheduledPool.scheduleAtFixedRate(
                () -> System.out.println("Heartbeat: " + LocalDateTime.now()),
                0, 5, TimeUnit.SECONDS
            );

            // Schedule with fixed delay between executions
            scheduledPool.scheduleWithFixedDelay(
                () -> System.out.println("Cleanup task executed"),
                10, 30, TimeUnit.SECONDS
            );
        }

        public void shutdown() {
            orderProcessingPool.shutdown();
            priceUpdatePool.shutdown();
            scheduledPool.shutdown();
        }
    }

    // =============================================
    // 6. PRODUCER-CONSUMER PATTERN
    // =============================================

    static class MarketDataProcessor {
        private final BlockingQueue<String> marketDataQueue;
        private final List<Thread> consumerThreads;
        private volatile boolean running = true;

        public MarketDataProcessor(int consumerCount) {
            this.marketDataQueue = new ArrayBlockingQueue<>(10000);
            this.consumerThreads = new ArrayList<>();

            // Start consumer threads
            for (int i = 0; i < consumerCount; i++) {
                Thread consumer = new Thread(new MarketDataConsumer(i), "Consumer-" + i);
                consumerThreads.add(consumer);
                consumer.start();
            }
        }

        // Producer method
        public void publishMarketData(String data) {
            try {
                marketDataQueue.put(data);  // Blocks if queue is full
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        private class MarketDataConsumer implements Runnable {
            private final int consumerId;

            public MarketDataConsumer(int consumerId) {
                this.consumerId = consumerId;
            }

            @Override
            public void run() {
                while (running || !marketDataQueue.isEmpty()) {
                    try {
                        String data = marketDataQueue.poll(1, TimeUnit.SECONDS);
                        if (data != null) {
                            processMarketData(data);
                        }
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
                System.out.println("Consumer " + consumerId + " shutting down");
            }

            private void processMarketData(String data) {
                System.out.println("Consumer " + consumerId + " processing: " + data);
                try {
                    Thread.sleep(50);  // Simulate processing time
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        }

        public void shutdown() {
            running = false;
            consumerThreads.forEach(Thread::interrupt);
        }
    }

    // =============================================
    // 7. COMPLETABLE FUTURE - Asynchronous Programming
    // =============================================

    static class AsyncTradingService {
        private final ExecutorService executor = Executors.newFixedThreadPool(4);

        public CompletableFuture<BigDecimal> fetchPriceAsync(String symbol) {
            return CompletableFuture.supplyAsync(() -> {
                try {
                    Thread.sleep(100);  // Simulate network call
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                return new BigDecimal("150.00").add(new BigDecimal(Math.random() * 10));
            }, executor);
        }

        public CompletableFuture<String> submitOrderAsync(String symbol, BigDecimal price, int quantity) {
            return CompletableFuture.supplyAsync(() -> {
                try {
                    Thread.sleep(200);  // Simulate order submission
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                return "ORDER_" + System.currentTimeMillis();
            }, executor);
        }

        // Compose multiple async operations
        public CompletableFuture<String> tradeAsync(String symbol, int quantity) {
            return fetchPriceAsync(symbol)
                .thenCompose(price -> submitOrderAsync(symbol, price, quantity))
                .thenApply(orderId -> "Trade completed with order: " + orderId)
                .exceptionally(throwable -> "Trade failed: " + throwable.getMessage());
        }

        // Combine multiple futures
        public CompletableFuture<String> arbitrageCheck(String symbol1, String symbol2) {
            CompletableFuture<BigDecimal> price1 = fetchPriceAsync(symbol1);
            CompletableFuture<BigDecimal> price2 = fetchPriceAsync(symbol2);

            return price1.thenCombine(price2, (p1, p2) -> {
                BigDecimal diff = p1.subtract(p2).abs();
                if (diff.compareTo(new BigDecimal("1.00")) > 0) {
                    return "Arbitrage opportunity: " + symbol1 + "=$" + p1 + ", " + symbol2 + "=$" + p2;
                } else {
                    return "No arbitrage opportunity";
                }
            });
        }

        public void shutdown() {
            executor.shutdown();
        }
    }

    // =============================================
    // 8. VOLATILE KEYWORD - Memory Visibility
    // =============================================

    static class PricePublisher {
        private volatile BigDecimal currentPrice = BigDecimal.ZERO;  // Ensures visibility across threads
        private volatile boolean priceChanged = false;

        // Writer thread
        public void updatePrice(BigDecimal newPrice) {
            currentPrice = newPrice;
            priceChanged = true;  // Signal to other threads
            System.out.println("Price updated to: $" + newPrice);
        }

        // Reader thread
        public BigDecimal waitForPriceUpdate() {
            while (!priceChanged) {
                Thread.yield();  // Give other threads a chance
            }
            priceChanged = false;
            return currentPrice;
        }
    }

    // =============================================
    // DEMONSTRATION METHODS
    // =============================================

    public static void demonstrateBasicThreading() throws InterruptedException {
        System.out.println("=== Basic Threading Example ===");

        // Method 1: Thread extension
        PriceUpdateThread priceUpdater = new PriceUpdateThread("AAPL");
        priceUpdater.start();

        // Method 2: Runnable interface with ExecutorService
        BlockingQueue<String> orderQueue = new ArrayBlockingQueue<>(100);
        ExecutorService executor = Executors.newFixedThreadPool(2);

        executor.submit(new OrderProcessor(orderQueue, "Processor-1"));
        executor.submit(new OrderProcessor(orderQueue, "Processor-2"));

        // Add some orders
        orderQueue.put("BUY AAPL 100 @ 150.00");
        orderQueue.put("SELL GOOGL 50 @ 2800.00");
        orderQueue.put("BUY MSFT 200 @ 300.00");

        Thread.sleep(2000);

        priceUpdater.stopUpdating();
        executor.shutdownNow();
    }

    public static void demonstrateSynchronization() throws InterruptedException {
        System.out.println("\n=== Synchronization Example ===");

        TradingAccount account = new TradingAccount(new BigDecimal("10000.00"));

        // Multiple threads accessing the same account
        Thread depositor = new Thread(() -> {
            for (int i = 0; i < 5; i++) {
                account.deposit(new BigDecimal("100.00"));
                try { Thread.sleep(100); } catch (InterruptedException e) { break; }
            }
        });

        Thread withdrawer = new Thread(() -> {
            for (int i = 0; i < 3; i++) {
                account.withdraw(new BigDecimal("200.00"));
                try { Thread.sleep(150); } catch (InterruptedException e) { break; }
            }
        });

        depositor.start();
        withdrawer.start();

        depositor.join();
        withdrawer.join();

        System.out.println("Final balance: $" + account.getBalance());
    }

    public static void demonstrateExecutorFramework() {
        System.out.println("\n=== Executor Framework Example ===");

        TradingExecutorService tradingService = new TradingExecutorService();

        // Submit orders
        tradingService.processOrder("ORD001", "AAPL", new BigDecimal("150.50"));
        tradingService.processOrder("ORD002", "GOOGL", new BigDecimal("2800.00"));

        // Fetch prices asynchronously
        Future<BigDecimal> applePrice = tradingService.fetchPrice("AAPL");
        Future<BigDecimal> googlePrice = tradingService.fetchPrice("GOOGL");

        try {
            System.out.println("AAPL price: $" + applePrice.get(2, TimeUnit.SECONDS));
            System.out.println("GOOGL price: $" + googlePrice.get(2, TimeUnit.SECONDS));
        } catch (Exception e) {
            e.printStackTrace();
        }

        tradingService.startPeriodicTasks();

        // Let it run for a bit
        try { Thread.sleep(3000); } catch (InterruptedException e) {}

        tradingService.shutdown();
    }

    public static void demonstrateCompletableFuture() {
        System.out.println("\n=== CompletableFuture Example ===");

        AsyncTradingService asyncService = new AsyncTradingService();

        // Single async operation
        CompletableFuture<String> trade = asyncService.tradeAsync("AAPL", 100);
        trade.thenAccept(System.out::println);

        // Multiple async operations
        CompletableFuture<String> arbitrage = asyncService.arbitrageCheck("AAPL", "MSFT");
        arbitrage.thenAccept(System.out::println);

        // Wait for completion
        try {
            CompletableFuture.allOf(trade, arbitrage).get(5, TimeUnit.SECONDS);
        } catch (Exception e) {
            e.printStackTrace();
        }

        asyncService.shutdown();
    }

    public static void main(String[] args) throws InterruptedException {
        demonstrateBasicThreading();
        demonstrateSynchronization();
        demonstrateExecutorFramework();
        demonstrateCompletableFuture();

        System.out.println("\n=== Key Concurrency Differences from C++ ===");
        System.out.println("1. synchronized keyword provides built-in mutex functionality");
        System.out.println("2. volatile ensures memory visibility (like C++ volatile but different)");
        System.out.println("3. ExecutorService manages thread pools automatically");
        System.out.println("4. BlockingQueue provides thread-safe producer-consumer patterns");
        System.out.println("5. CompletableFuture enables functional async programming");
        System.out.println("6. Atomic classes provide lock-free programming primitives");
        System.out.println("7. ReadWriteLock allows multiple readers, single writer");
        System.out.println("8. Built-in interruption mechanism with InterruptedException");
    }
}

/*
=== Concurrency Best Practices for Trading Systems ===

1. Thread Safety:
   - Use ConcurrentHashMap for shared price/order data
   - Prefer atomic operations over synchronized blocks where possible
   - Use volatile for simple flags and status variables

2. Performance:
   - Use ThreadLocal for per-thread caching
   - Minimize lock scope and duration
   - Prefer lock-free data structures when possible
   - Use ForkJoinPool for CPU-intensive parallel tasks

3. Error Handling:
   - Always handle InterruptedException properly
   - Use CompletableFuture.exceptionally() for async error handling
   - Implement proper shutdown procedures for thread pools

4. Memory Management:
   - Be careful with thread-local storage (can cause memory leaks)
   - Shutdown ExecutorService properly to prevent resource leaks
   - Use weak references in caches to allow garbage collection

5. Monitoring:
   - Use JMX to monitor thread pool metrics
   - Log thread contention and deadlock detection
   - Monitor queue sizes in producer-consumer patterns

6. Architecture:
   - Separate I/O threads from CPU-intensive processing
   - Use different thread pools for different types of work
   - Consider LMAX Disruptor pattern for ultra-low latency
*/
