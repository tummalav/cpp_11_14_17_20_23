/**
 * ================================================================================================
 * LMAX DISRUPTOR - QUEUE VARIANTS (SPSC, MPSC, SPMC, MPMC)
 * ================================================================================================
 *
 * LMAX Disruptor is a high-performance inter-thread messaging library that achieves low latency
 * and high throughput by using a ring buffer with smart memory barriers instead of locks.
 *
 * KEY ADVANTAGES:
 * - Mechanical sympathy: designed with CPU cache lines in mind
 * - Lock-free: uses CAS (Compare-And-Swap) operations
 * - Pre-allocated memory: no GC pressure
 * - Cache-line padding: prevents false sharing
 * - Batching: processes multiple events in one go
 *
 * LATENCY NUMBERS:
 * - SPSC: ~10-50 nanoseconds (fastest)
 * - MPSC: ~50-100 nanoseconds
 * - SPMC: ~50-150 nanoseconds
 * - MPMC: ~100-200 nanoseconds
 *
 * ================================================================================================
 */

import com.lmax.disruptor.*;
import com.lmax.disruptor.dsl.Disruptor;
import com.lmax.disruptor.dsl.ProducerType;
import com.lmax.disruptor.util.DaemonThreadFactory;

import java.nio.ByteBuffer;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicLong;

// ================================================================================================
// 1. SPSC (Single Producer Single Consumer)
// ================================================================================================
/**
 * SPSC - Single Producer Single Consumer
 *
 * PURPOSE:
 * - Fastest variant of Disruptor
 * - One thread writes, one thread reads
 * - No contention, no CAS operations needed
 * - Optimal for point-to-point communication
 *
 * USE CASES FOR TRADING:
 * 1. Market Data Handler → Strategy Engine
 * 2. Strategy Engine → Order Gateway
 * 3. Order Gateway → Exchange Connectivity
 * 4. Risk Check → Order Router
 * 5. Price Feed → Single Strategy
 *
 * LATENCY: 10-50 nanoseconds per message
 */
class MarketDataEvent {
    private long sequence;
    private String symbol;
    private double bidPrice;
    private double askPrice;
    private long bidSize;
    private long askSize;
    private long timestamp;
    private byte exchange;

    // Getters and setters
    public void set(String symbol, double bidPrice, double askPrice,
                   long bidSize, long askSize, long timestamp, byte exchange) {
        this.symbol = symbol;
        this.bidPrice = bidPrice;
        this.askPrice = askPrice;
        this.bidSize = bidSize;
        this.askSize = askSize;
        this.timestamp = timestamp;
        this.exchange = exchange;
    }

    public String getSymbol() { return symbol; }
    public double getBidPrice() { return bidPrice; }
    public double getAskPrice() { return askPrice; }
    public long getBidSize() { return bidSize; }
    public long getAskSize() { return askSize; }
    public long getTimestamp() { return timestamp; }
    public byte getExchange() { return exchange; }
}

class SPSC_MarketDataToStrategy {

    private static final int BUFFER_SIZE = 1024 * 64; // Must be power of 2

    public static void main(String[] args) throws Exception {

        // Event Factory - pre-allocates events
        EventFactory<MarketDataEvent> factory = MarketDataEvent::new;

        // Thread Factory
        ThreadFactory threadFactory = DaemonThreadFactory.INSTANCE;

        // WaitStrategy - YieldingWaitStrategy for low latency
        WaitStrategy waitStrategy = new YieldingWaitStrategy();

        // Create Disruptor with SINGLE producer type (SPSC)
        Disruptor<MarketDataEvent> disruptor = new Disruptor<>(
            factory,
            BUFFER_SIZE,
            threadFactory,
            ProducerType.SINGLE,  // SINGLE PRODUCER
            waitStrategy
        );

        // Event Handler (Consumer)
        EventHandler<MarketDataEvent> strategyHandler = (event, sequence, endOfBatch) -> {
            // Strategy processing - ultra-low latency
            double midPrice = (event.getBidPrice() + event.getAskPrice()) / 2.0;
            double spread = event.getAskPrice() - event.getBidPrice();

            // Trading logic here
            if (spread < 0.01) {
                // Generate order
            }

            // Measure latency
            long latency = System.nanoTime() - event.getTimestamp();
            // Typical: 10-50 nanoseconds
        };

        disruptor.handleEventsWith(strategyHandler);

        // Start the Disruptor
        RingBuffer<MarketDataEvent> ringBuffer = disruptor.start();

        // Producer (Market Data Feed Handler)
        for (int i = 0; i < 1000000; i++) {
            long sequence = ringBuffer.next();
            try {
                MarketDataEvent event = ringBuffer.get(sequence);
                event.set("AAPL", 150.25, 150.26, 100, 200, System.nanoTime(), (byte)1);
            } finally {
                ringBuffer.publish(sequence);
            }
        }

        Thread.sleep(1000);
        disruptor.shutdown();
    }
}

// ================================================================================================
// 2. MPSC (Multi Producer Single Consumer)
// ================================================================================================
/**
 * MPSC - Multi Producer Single Consumer
 *
 * PURPOSE:
 * - Multiple threads can write, one thread reads
 * - Uses CAS for sequence allocation among producers
 * - Single consumer, no contention on read side
 *
 * USE CASES FOR TRADING:
 * 1. Multiple Market Data Feeds → Consolidated Feed Handler
 *    - CME, ICE, NASDAQ feeds → Single normalizer
 * 2. Multiple Strategies → Single Order Router
 *    - Strategy1, Strategy2, Strategy3 → Order Gateway
 * 3. Multiple Risk Checks → Single Risk Aggregator
 * 4. Multiple Exchanges → Single Position Manager
 * 5. Multiple Order Acknowledgments → Single Order Manager
 *
 * LATENCY: 50-100 nanoseconds per message
 */
class OrderEvent {
    private long orderId;
    private String symbol;
    private char side; // 'B' or 'S'
    private double price;
    private long quantity;
    private long timestamp;
    private int strategyId;

    public void set(long orderId, String symbol, char side, double price,
                   long quantity, long timestamp, int strategyId) {
        this.orderId = orderId;
        this.symbol = symbol;
        this.side = side;
        this.price = price;
        this.quantity = quantity;
        this.timestamp = timestamp;
        this.strategyId = strategyId;
    }

    public long getOrderId() { return orderId; }
    public String getSymbol() { return symbol; }
    public char getSide() { return side; }
    public double getPrice() { return price; }
    public long getQuantity() { return quantity; }
    public long getTimestamp() { return timestamp; }
    public int getStrategyId() { return strategyId; }
}

class MPSC_MultiStrategyToOrderRouter {

    private static final int BUFFER_SIZE = 1024 * 64;

    public static void main(String[] args) throws Exception {

        EventFactory<OrderEvent> factory = OrderEvent::new;
        ThreadFactory threadFactory = DaemonThreadFactory.INSTANCE;
        WaitStrategy waitStrategy = new YieldingWaitStrategy();

        // Create Disruptor with MULTI producer type (MPSC)
        Disruptor<OrderEvent> disruptor = new Disruptor<>(
            factory,
            BUFFER_SIZE,
            threadFactory,
            ProducerType.MULTI,  // MULTIPLE PRODUCERS
            waitStrategy
        );

        // Single Consumer - Order Router
        EventHandler<OrderEvent> orderRouter = (event, sequence, endOfBatch) -> {
            // Route order to exchange
            long latency = System.nanoTime() - event.getTimestamp();

            // Risk checks
            if (event.getQuantity() > 10000) {
                // Reject
                return;
            }

            // Send to exchange
            // sendToExchange(event);

            // Typical latency: 50-100 nanoseconds
        };

        disruptor.handleEventsWith(orderRouter);
        RingBuffer<OrderEvent> ringBuffer = disruptor.start();

        // Multiple Producers (3 strategies)
        Thread strategy1 = new Thread(() -> {
            for (int i = 0; i < 100000; i++) {
                long sequence = ringBuffer.next();
                try {
                    OrderEvent event = ringBuffer.get(sequence);
                    event.set(i, "AAPL", 'B', 150.25, 100, System.nanoTime(), 1);
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        Thread strategy2 = new Thread(() -> {
            for (int i = 100000; i < 200000; i++) {
                long sequence = ringBuffer.next();
                try {
                    OrderEvent event = ringBuffer.get(sequence);
                    event.set(i, "MSFT", 'S', 320.50, 200, System.nanoTime(), 2);
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        Thread strategy3 = new Thread(() -> {
            for (int i = 200000; i < 300000; i++) {
                long sequence = ringBuffer.next();
                try {
                    OrderEvent event = ringBuffer.get(sequence);
                    event.set(i, "GOOGL", 'B', 2800.75, 50, System.nanoTime(), 3);
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        strategy1.start();
        strategy2.start();
        strategy3.start();

        strategy1.join();
        strategy2.join();
        strategy3.join();

        Thread.sleep(1000);
        disruptor.shutdown();
    }
}

// ================================================================================================
// 3. SPMC (Single Producer Multi Consumer)
// ================================================================================================
/**
 * SPMC - Single Producer Multi Consumer
 *
 * PURPOSE:
 * - One thread writes, multiple threads read
 * - Each consumer processes every event independently
 * - No CAS on producer side
 * - Consumers track their own sequence independently
 *
 * USE CASES FOR TRADING:
 * 1. Single Market Data Feed → Multiple Strategies
 *    - One normalized feed → Strategy1, Strategy2, Strategy3
 * 2. Single Order Fill → Multiple Handlers
 *    - Fill message → Position Manager, P&L Calculator, Risk System
 * 3. Single Reference Data → Multiple Services
 *    - Reference updates → All strategies need to see
 * 4. Single Tick Feed → Multiple Analytics
 *    - Raw tick → VWAP calc, TWAP calc, Volatility calc
 * 5. Broadcast Scenario
 *    - One source, many subscribers
 *
 * LATENCY: 50-150 nanoseconds per message per consumer
 */
class TickEvent {
    private String symbol;
    private double price;
    private long size;
    private long timestamp;
    private byte side; // 1=Buy, 2=Sell

    public void set(String symbol, double price, long size, long timestamp, byte side) {
        this.symbol = symbol;
        this.price = price;
        this.size = size;
        this.timestamp = timestamp;
        this.side = side;
    }

    public String getSymbol() { return symbol; }
    public double getPrice() { return price; }
    public long getSize() { return size; }
    public long getTimestamp() { return timestamp; }
    public byte getSide() { return side; }
}

class SPMC_SingleFeedToMultipleStrategies {

    private static final int BUFFER_SIZE = 1024 * 64;
    private static final AtomicLong strategy1Count = new AtomicLong(0);
    private static final AtomicLong strategy2Count = new AtomicLong(0);
    private static final AtomicLong strategy3Count = new AtomicLong(0);

    public static void main(String[] args) throws Exception {

        EventFactory<TickEvent> factory = TickEvent::new;
        ThreadFactory threadFactory = DaemonThreadFactory.INSTANCE;
        WaitStrategy waitStrategy = new YieldingWaitStrategy();

        // Create Disruptor with SINGLE producer (SPMC)
        Disruptor<TickEvent> disruptor = new Disruptor<>(
            factory,
            BUFFER_SIZE,
            threadFactory,
            ProducerType.SINGLE,  // SINGLE PRODUCER
            waitStrategy
        );

        // Multiple Consumers - Each gets every event
        EventHandler<TickEvent> strategy1Handler = (event, sequence, endOfBatch) -> {
            // Strategy 1: Mean Reversion
            if (event.getPrice() < 150.0) {
                // Generate buy signal
            }
            strategy1Count.incrementAndGet();
        };

        EventHandler<TickEvent> strategy2Handler = (event, sequence, endOfBatch) -> {
            // Strategy 2: Momentum
            if (event.getSize() > 1000) {
                // Generate signal based on size
            }
            strategy2Count.incrementAndGet();
        };

        EventHandler<TickEvent> strategy3Handler = (event, sequence, endOfBatch) -> {
            // Strategy 3: Market Making
            double spread = calculateSpread(event);
            strategy3Count.incrementAndGet();
        };

        // All handlers process independently
        disruptor.handleEventsWith(strategy1Handler, strategy2Handler, strategy3Handler);

        RingBuffer<TickEvent> ringBuffer = disruptor.start();

        // Single Producer - Market Data Feed
        Thread feedHandler = new Thread(() -> {
            for (int i = 0; i < 1000000; i++) {
                long sequence = ringBuffer.next();
                try {
                    TickEvent event = ringBuffer.get(sequence);
                    event.set("AAPL", 150.0 + (i % 100) * 0.01,
                             100 + (i % 1000), System.nanoTime(), (byte)(i % 2 + 1));
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        feedHandler.start();
        feedHandler.join();

        Thread.sleep(1000);

        System.out.println("Strategy 1 processed: " + strategy1Count.get());
        System.out.println("Strategy 2 processed: " + strategy2Count.get());
        System.out.println("Strategy 3 processed: " + strategy3Count.get());
        // All should be 1,000,000

        disruptor.shutdown();
    }

    private static double calculateSpread(TickEvent event) {
        return 0.01; // Simplified
    }
}

// ================================================================================================
// 4. MPMC (Multi Producer Multi Consumer)
// ================================================================================================
/**
 * MPMC - Multi Producer Multi Consumer
 *
 * PURPOSE:
 * - Multiple threads write, multiple threads read
 * - Uses CAS on both producer and consumer sides
 * - Most flexible but highest latency
 * - Each consumer still sees every event
 *
 * USE CASES FOR TRADING:
 * 1. Multiple Feeds → Multiple Strategies
 *    - CME, ICE, NASDAQ → Strategy1, Strategy2, Strategy3
 * 2. Multiple Order Sources → Multiple Risk Systems
 *    - Manual orders, Algo orders → Pre-trade risk, Post-trade risk
 * 3. Event Bus Pattern
 *    - Any component can publish, any can subscribe
 * 4. Multi-venue Trading
 *    - Multiple exchanges → Multiple strategies
 * 5. Complex System Integration
 *    - Multiple producers and consumers need full broadcast
 *
 * LATENCY: 100-200 nanoseconds per message
 */
class TradingEvent {
    private long eventId;
    private String eventType; // "ORDER", "FILL", "CANCEL", "MARKETDATA"
    private String symbol;
    private double price;
    private long quantity;
    private long timestamp;
    private int sourceId;

    public void set(long eventId, String eventType, String symbol, double price,
                   long quantity, long timestamp, int sourceId) {
        this.eventId = eventId;
        this.eventType = eventType;
        this.symbol = symbol;
        this.price = price;
        this.quantity = quantity;
        this.timestamp = timestamp;
        this.sourceId = sourceId;
    }

    public long getEventId() { return eventId; }
    public String getEventType() { return eventType; }
    public String getSymbol() { return symbol; }
    public double getPrice() { return price; }
    public long getQuantity() { return quantity; }
    public long getTimestamp() { return timestamp; }
    public int getSourceId() { return sourceId; }
}

class MPMC_EventBusForTradingSystem {

    private static final int BUFFER_SIZE = 1024 * 64;

    public static void main(String[] args) throws Exception {

        EventFactory<TradingEvent> factory = TradingEvent::new;
        ThreadFactory threadFactory = DaemonThreadFactory.INSTANCE;
        WaitStrategy waitStrategy = new YieldingWaitStrategy();

        // Create Disruptor with MULTI producer (MPMC)
        Disruptor<TradingEvent> disruptor = new Disruptor<>(
            factory,
            BUFFER_SIZE,
            threadFactory,
            ProducerType.MULTI,  // MULTIPLE PRODUCERS
            waitStrategy
        );

        // Multiple Consumers
        EventHandler<TradingEvent> positionManager = (event, sequence, endOfBatch) -> {
            if ("FILL".equals(event.getEventType())) {
                // Update positions
                updatePosition(event.getSymbol(), event.getQuantity());
            }
        };

        EventHandler<TradingEvent> riskManager = (event, sequence, endOfBatch) -> {
            if ("ORDER".equals(event.getEventType())) {
                // Pre-trade risk check
                checkRisk(event.getSymbol(), event.getQuantity());
            }
        };

        EventHandler<TradingEvent> pnlCalculator = (event, sequence, endOfBatch) -> {
            if ("FILL".equals(event.getEventType())) {
                // Calculate P&L
                calculatePnL(event.getPrice(), event.getQuantity());
            }
        };

        // All consumers process all events
        disruptor.handleEventsWith(positionManager, riskManager, pnlCalculator);

        RingBuffer<TradingEvent> ringBuffer = disruptor.start();

        // Multiple Producers
        Thread orderGateway = new Thread(() -> {
            for (int i = 0; i < 100000; i++) {
                long sequence = ringBuffer.next();
                try {
                    TradingEvent event = ringBuffer.get(sequence);
                    event.set(i, "ORDER", "AAPL", 150.25, 100, System.nanoTime(), 1);
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        Thread fillHandler = new Thread(() -> {
            for (int i = 100000; i < 200000; i++) {
                long sequence = ringBuffer.next();
                try {
                    TradingEvent event = ringBuffer.get(sequence);
                    event.set(i, "FILL", "MSFT", 320.50, 200, System.nanoTime(), 2);
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        Thread marketDataFeed = new Thread(() -> {
            for (int i = 200000; i < 300000; i++) {
                long sequence = ringBuffer.next();
                try {
                    TradingEvent event = ringBuffer.get(sequence);
                    event.set(i, "MARKETDATA", "GOOGL", 2800.75, 0, System.nanoTime(), 3);
                } finally {
                    ringBuffer.publish(sequence);
                }
            }
        });

        orderGateway.start();
        fillHandler.start();
        marketDataFeed.start();

        orderGateway.join();
        fillHandler.join();
        marketDataFeed.join();

        Thread.sleep(1000);
        disruptor.shutdown();
    }

    private static void updatePosition(String symbol, long quantity) {
        // Position update logic
    }

    private static void checkRisk(String symbol, long quantity) {
        // Risk check logic
    }

    private static void calculatePnL(double price, long quantity) {
        // P&L calculation logic
    }
}

// ================================================================================================
// WAIT STRATEGIES COMPARISON
// ================================================================================================
/**
 * WAIT STRATEGIES - Impact on Latency and CPU Usage
 *
 * 1. BusySpinWaitStrategy
 *    - LATENCY: Lowest (5-10ns)
 *    - CPU: 100% (busy spin)
 *    - USE CASE: When you need absolute lowest latency and can dedicate cores
 *    - TRADING: HFT market making, stat arb
 *
 * 2. YieldingWaitStrategy
 *    - LATENCY: Very Low (10-50ns)
 *    - CPU: High (yields to other threads)
 *    - USE CASE: Low latency with some CPU consideration
 *    - TRADING: Most trading systems, good balance
 *
 * 3. SleepingWaitStrategy
 *    - LATENCY: Medium (50-100ns)
 *    - CPU: Low (sleeps periodically)
 *    - USE CASE: When CPU usage matters more than latency
 *    - TRADING: Back office, reporting
 *
 * 4. BlockingWaitStrategy
 *    - LATENCY: Highest (100ns+)
 *    - CPU: Lowest (blocks using locks)
 *    - USE CASE: When latency is not critical
 *    - TRADING: Non-latency sensitive processing
 *
 * 5. TimeoutBlockingWaitStrategy
 *    - LATENCY: High (similar to blocking)
 *    - CPU: Low
 *    - USE CASE: When you need timeout capability
 *    - TRADING: Systems with SLA requirements
 */

class WaitStrategyComparison {

    public static void demonstrateWaitStrategies() {

        // 1. BusySpinWaitStrategy - Absolute lowest latency
        WaitStrategy busySpin = new BusySpinWaitStrategy();
        // Use when: HFT, market making, sub-microsecond requirements

        // 2. YieldingWaitStrategy - Best for most trading systems
        WaitStrategy yielding = new YieldingWaitStrategy();
        // Use when: Low latency trading, algo trading, most systems

        // 3. SleepingWaitStrategy - CPU friendly
        WaitStrategy sleeping = new SleepingWaitStrategy();
        // Use when: Reporting, analytics, back office

        // 4. BlockingWaitStrategy - Most CPU friendly
        WaitStrategy blocking = new BlockingWaitStrategy();
        // Use when: Non-critical paths, batch processing
    }
}

// ================================================================================================
// SEQUENCING PATTERNS
// ================================================================================================
/**
 * ADVANCED SEQUENCING PATTERNS
 *
 * 1. DIAMOND PATTERN
 *    - One producer → Multiple parallel consumers → Single aggregator
 *    - USE CASE: Market data → Multiple analytics → Consolidated decision
 *
 * 2. PIPELINE PATTERN
 *    - Producer → Consumer1 → Consumer2 → Consumer3
 *    - USE CASE: Order flow: Receive → Validate → Risk Check → Send
 *
 * 3. MULTICAST PATTERN
 *    - One producer → Multiple independent consumers
 *    - USE CASE: Market data → Multiple strategies
 *
 * 4. WORK POOL PATTERN
 *    - Multiple producers → Work pool of consumers (each event to one consumer)
 *    - USE CASE: Load balancing across consumer threads
 */

class SequencingPatterns {

    // Diamond Pattern Example
    public static void diamondPattern() throws Exception {

        EventFactory<MarketDataEvent> factory = MarketDataEvent::new;
        Disruptor<MarketDataEvent> disruptor = new Disruptor<>(
            factory, 1024, DaemonThreadFactory.INSTANCE,
            ProducerType.SINGLE, new YieldingWaitStrategy()
        );

        // Stage 1: Parallel processing
        EventHandler<MarketDataEvent> vwapCalculator = (event, seq, eob) -> {
            // Calculate VWAP
        };

        EventHandler<MarketDataEvent> volatilityCalculator = (event, seq, eob) -> {
            // Calculate volatility
        };

        // Stage 2: Aggregator (waits for both)
        EventHandler<MarketDataEvent> aggregator = (event, seq, eob) -> {
            // Combine results and make decision
        };

        // Diamond: vwapCalculator and volatilityCalculator run in parallel,
        // then aggregator waits for both
        disruptor.handleEventsWith(vwapCalculator, volatilityCalculator)
                 .then(aggregator);

        disruptor.start();
    }

    // Pipeline Pattern Example
    public static void pipelinePattern() throws Exception {

        EventFactory<OrderEvent> factory = OrderEvent::new;
        Disruptor<OrderEvent> disruptor = new Disruptor<>(
            factory, 1024, DaemonThreadFactory.INSTANCE,
            ProducerType.SINGLE, new YieldingWaitStrategy()
        );

        EventHandler<OrderEvent> validator = (event, seq, eob) -> {
            // Validate order
        };

        EventHandler<OrderEvent> riskCheck = (event, seq, eob) -> {
            // Check risk
        };

        EventHandler<OrderEvent> sender = (event, seq, eob) -> {
            // Send to exchange
        };

        // Pipeline: validator → riskCheck → sender (sequential)
        disruptor.handleEventsWith(validator)
                 .then(riskCheck)
                 .then(sender);

        disruptor.start();
    }
}

// ================================================================================================
// PERFORMANCE TUNING TIPS
// ================================================================================================
/**
 * PERFORMANCE OPTIMIZATION FOR ULTRA-LOW LATENCY
 *
 * 1. BUFFER SIZE
 *    - Must be power of 2 (enables bit masking instead of modulo)
 *    - Typical: 1024, 2048, 4096, 8192, 16384, 32768, 65536
 *    - Larger = more memory, better batching
 *    - Smaller = less memory, lower latency
 *    - Trading: 8192 or 16384 is common
 *
 * 2. CPU PINNING
 *    - Pin producer/consumer threads to dedicated CPU cores
 *    - Avoid context switches
 *    - Use taskset or Thread.setAffinity (with JNA)
 *
 * 3. NUMA AWARENESS
 *    - Keep producer/consumer on same NUMA node
 *    - Avoid cross-NUMA memory access
 *
 * 4. GC TUNING
 *    - Use ZGC or Shenandoah for low pause times
 *    - -XX:+UseZGC or -XX:+UseShenandoahGC
 *    - Pre-allocate ring buffer events (Disruptor does this)
 *
 * 5. CACHE LINE PADDING
 *    - Disruptor automatically pads to avoid false sharing
 *    - Keep event objects small (fits in cache line)
 *
 * 6. BATCH PROCESSING
 *    - Use endOfBatch flag to optimize
 *    - Flush only at end of batch
 *
 * 7. MEMORY BARRIERS
 *    - Disruptor uses memory barriers optimally
 *    - Avoid additional volatile reads/writes in handlers
 */

class PerformanceTuningExample {

    public static void optimalConfiguration() {

        // Optimal for ultra-low latency trading
        int bufferSize = 8192; // Power of 2

        EventFactory<MarketDataEvent> factory = MarketDataEvent::new;
        ThreadFactory threadFactory = DaemonThreadFactory.INSTANCE;

        // YieldingWaitStrategy - best balance for trading
        WaitStrategy waitStrategy = new YieldingWaitStrategy();

        Disruptor<MarketDataEvent> disruptor = new Disruptor<>(
            factory,
            bufferSize,
            threadFactory,
            ProducerType.SINGLE, // Use SINGLE if possible
            waitStrategy
        );

        // Handler with batch optimization
        EventHandler<MarketDataEvent> handler = (event, sequence, endOfBatch) -> {
            // Process event
            processMarketData(event);

            // Flush only at end of batch for better throughput
            if (endOfBatch) {
                flush();
            }
        };

        disruptor.handleEventsWith(handler);
        disruptor.start();
    }

    private static void processMarketData(MarketDataEvent event) {
        // Processing logic
    }

    private static void flush() {
        // Flush accumulated data
    }
}

// ================================================================================================
// LATENCY MEASUREMENTS
// ================================================================================================
/**
 * TYPICAL LATENCY NUMBERS FOR TRADING SYSTEMS
 *
 * COMPONENT                          LATENCY (50th percentile)   LATENCY (99th percentile)
 * -----------------------------------------------------------------------------------------
 * SPSC Disruptor                     10-30 ns                    50-100 ns
 * MPSC Disruptor                     50-80 ns                    100-200 ns
 * SPMC Disruptor                     50-100 ns                   150-300 ns
 * MPMC Disruptor                     100-150 ns                  200-400 ns
 *
 * ArrayBlockingQueue                 500-1000 ns                 2000-5000 ns
 * LinkedBlockingQueue                1000-2000 ns                3000-8000 ns
 * ConcurrentLinkedQueue              200-500 ns                  1000-2000 ns
 *
 * COMPARISON:
 * - Disruptor is 10-50x faster than Java blocking queues
 * - SPSC is fastest (no contention)
 * - Choose based on your producer/consumer pattern
 *
 * REAL-WORLD TRADING LATENCY BUDGET:
 * - Market Data Feed → Strategy: 10-50 ns (SPSC)
 * - Strategy → Risk Check: 50-100 ns (SPSC or MPSC)
 * - Risk Check → Order Router: 50-100 ns (SPSC)
 * - Order Router → Exchange Gateway: 100-200 ns (MPSC)
 * - Total in-process: 200-500 ns
 * - Network to exchange: 50-500 microseconds
 */

class LatencyMeasurement {

    public static void measureLatency() throws Exception {

        int iterations = 1000000;
        long[] latencies = new long[iterations];

        EventFactory<MarketDataEvent> factory = MarketDataEvent::new;
        Disruptor<MarketDataEvent> disruptor = new Disruptor<>(
            factory, 65536, DaemonThreadFactory.INSTANCE,
            ProducerType.SINGLE, new BusySpinWaitStrategy()
        );

        final AtomicLong counter = new AtomicLong(0);

        EventHandler<MarketDataEvent> handler = (event, sequence, endOfBatch) -> {
            long latency = System.nanoTime() - event.getTimestamp();
            int index = (int) counter.getAndIncrement();
            if (index < latencies.length) {
                latencies[index] = latency;
            }
        };

        disruptor.handleEventsWith(handler);
        RingBuffer<MarketDataEvent> ringBuffer = disruptor.start();

        // Warmup
        for (int i = 0; i < 100000; i++) {
            long seq = ringBuffer.next();
            try {
                MarketDataEvent event = ringBuffer.get(seq);
                event.set("WARM", 0, 0, 0, 0, System.nanoTime(), (byte)0);
            } finally {
                ringBuffer.publish(seq);
            }
        }

        Thread.sleep(1000);
        counter.set(0);

        // Actual measurement
        for (int i = 0; i < iterations; i++) {
            long seq = ringBuffer.next();
            try {
                MarketDataEvent event = ringBuffer.get(seq);
                event.set("TEST", 150.0, 150.01, 100, 200, System.nanoTime(), (byte)1);
            } finally {
                ringBuffer.publish(seq);
            }
        }

        Thread.sleep(2000);
        disruptor.shutdown();

        // Calculate percentiles
        java.util.Arrays.sort(latencies);
        System.out.println("50th percentile: " + latencies[iterations / 2] + " ns");
        System.out.println("99th percentile: " + latencies[(int)(iterations * 0.99)] + " ns");
        System.out.println("99.9th percentile: " + latencies[(int)(iterations * 0.999)] + " ns");
    }
}

// ================================================================================================
// SUMMARY - WHEN TO USE WHICH VARIANT
// ================================================================================================
/**
 * DECISION MATRIX FOR DISRUPTOR VARIANTS
 *
 * ┌─────────────────────────────────────────────────────────────────────────────────┐
 * │ SCENARIO                                    │ VARIANT │ LATENCY    │ USE CASE   │
 * ├─────────────────────────────────────────────────────────────────────────────────┤
 * │ Market Data Feed → Strategy                 │ SPSC    │ 10-50 ns   │ Best       │
 * │ Strategy → Order Gateway                    │ SPSC    │ 10-50 ns   │ Best       │
 * │ Multiple Strategies → Order Router          │ MPSC    │ 50-100 ns  │ Best       │
 * │ Multiple Feeds → Consolidated Handler       │ MPSC    │ 50-100 ns  │ Best       │
 * │ Single Feed → Multiple Strategies           │ SPMC    │ 50-150 ns  │ Best       │
 * │ Fill → Position/Risk/PnL (broadcast)        │ SPMC    │ 50-150 ns  │ Best       │
 * │ Multiple Feeds → Multiple Strategies        │ MPMC    │ 100-200 ns │ Flexible   │
 * │ Event Bus (any to any)                      │ MPMC    │ 100-200 ns │ Flexible   │
 * └─────────────────────────────────────────────────────────────────────────────────┘
 *
 * GENERAL RULES:
 * 1. Always prefer SPSC if possible (fastest)
 * 2. Use MPSC for multiple producers, single consumer
 * 3. Use SPMC for broadcast scenarios
 * 4. Use MPMC only when you need full flexibility
 * 5. Consider WorkProcessor for load balancing
 * 6. Use appropriate WaitStrategy (YieldingWaitStrategy for trading)
 * 7. Pin threads to CPU cores for best performance
 * 8. Monitor GC and tune JVM for low latency
 * 9. Keep events small (cache-line friendly)
 * 10. Use batching (endOfBatch) for throughput optimization
 *
 * CRITICAL FOR TRADING:
 * - Tick to trade latency budget: aim for < 1 microsecond in-process
 * - SPSC chain: Feed → Strategy → Risk → Gateway = 50-200 ns total
 * - Network latency to exchange: 50-500 microseconds
 * - Total tick to exchange: aim for < 1-2 microseconds
 */

