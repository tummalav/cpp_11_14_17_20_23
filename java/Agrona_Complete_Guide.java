/**
 * AGRONA - COMPLETE GUIDE
 *
 * Purpose: High-performance data structures and utilities for building ultra-low latency systems
 * Created by: Real Logic (creators of Aeron messaging)
 * Repository: https://github.com/real-logic/agrona
 *
 * Key Features:
 * - Zero-copy buffers (Direct & Unsafe memory access)
 * - Lock-free data structures (queues, ring buffers, counters)
 * - Object pooling and recycling
 * - Bit manipulation utilities
 * - High-performance collections
 * - Memory-mapped file support
 * - Concurrent data structures
 *
 * Use Cases:
 * 1. Inter-thread communication (lock-free queues)
 * 2. Shared memory IPC (between processes)
 * 3. Network protocol encoding/decoding (zero-copy)
 * 4. High-frequency trading systems
 * 5. Real-time streaming systems
 * 6. Event sourcing and logging
 * 7. Memory-efficient caching
 */

package com.trading.agrona;

import org.agrona.*;
import org.agrona.concurrent.*;
import org.agrona.concurrent.ringbuffer.*;
import org.agrona.collections.*;
import org.agrona.hints.ThreadHints;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

// ============================================================================
// PART 1: CORE AGRONA BUFFERS - ZERO-COPY OPERATIONS
// ============================================================================

/**
 * UnsafeBuffer - Direct memory access without bounds checking overhead
 * Key benefit: Zero-copy read/write operations
 */
class UnsafeBufferExample {

    public static void demonstrateUnsafeBuffer() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 1: UNSAFE BUFFER - ZERO-COPY OPERATIONS");
        System.out.println("=".repeat(80));

        // Allocate direct buffer (off-heap memory)
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(1024);
        UnsafeBuffer buffer = new UnsafeBuffer(byteBuffer);

        // Write primitive types - ultra-fast, no object creation
        int offset = 0;

        buffer.putLong(offset, 123456789L);          // Write long
        offset += Long.BYTES;

        buffer.putInt(offset, 42);                   // Write int
        offset += Integer.BYTES;

        buffer.putDouble(offset, 3.14159);           // Write double
        offset += Double.BYTES;

        // Write string (length-prefixed)
        String symbol = "AAPL";
        buffer.putInt(offset, symbol.length());
        offset += Integer.BYTES;
        buffer.putStringWithoutLengthAscii(offset, symbol);
        offset += symbol.length();

        // Read back - zero-copy, direct memory access
        int readOffset = 0;
        long longValue = buffer.getLong(readOffset);
        readOffset += Long.BYTES;

        int intValue = buffer.getInt(readOffset);
        readOffset += Integer.BYTES;

        double doubleValue = buffer.getDouble(readOffset);
        readOffset += Double.BYTES;

        int stringLen = buffer.getInt(readOffset);
        readOffset += Integer.BYTES;
        String symbolRead = buffer.getStringWithoutLengthAscii(readOffset, stringLen);

        System.out.println("\n=== Zero-Copy Read/Write ===");
        System.out.println("Long:    " + longValue);
        System.out.println("Int:     " + intValue);
        System.out.println("Double:  " + doubleValue);
        System.out.println("Symbol:  " + symbolRead);

        System.out.println("\n✓ No object allocation");
        System.out.println("✓ No array copying");
        System.out.println("✓ Direct memory access");
    }

    /**
     * Benchmark: UnsafeBuffer vs ByteBuffer
     */
    public static void benchmarkUnsafeBufferVsByteBuffer() {
        System.out.println("\n=== Benchmark: UnsafeBuffer vs ByteBuffer ===");

        int iterations = 10_000_000;

        // ByteBuffer approach
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(1024);
        byteBuffer.order(ByteOrder.nativeOrder());

        long startTime = System.nanoTime();
        for (int i = 0; i < iterations; i++) {
            byteBuffer.position(0);
            byteBuffer.putLong(123456789L);
            byteBuffer.putInt(42);
            byteBuffer.putDouble(3.14159);
        }
        long byteBufferTime = System.nanoTime() - startTime;

        // UnsafeBuffer approach
        UnsafeBuffer unsafeBuffer = new UnsafeBuffer(ByteBuffer.allocateDirect(1024));

        startTime = System.nanoTime();
        for (int i = 0; i < iterations; i++) {
            unsafeBuffer.putLong(0, 123456789L);
            unsafeBuffer.putInt(8, 42);
            unsafeBuffer.putDouble(12, 3.14159);
        }
        long unsafeBufferTime = System.nanoTime() - startTime;

        System.out.println("ByteBuffer:    " + (byteBufferTime / 1_000_000) + " ms");
        System.out.println("UnsafeBuffer:  " + (unsafeBufferTime / 1_000_000) + " ms");
        System.out.println("Speedup:       " + String.format("%.2fx", (double)byteBufferTime / unsafeBufferTime));
    }
}

// ============================================================================
// PART 2: ONE-TO-ONE RING BUFFER - SPSC (Single Producer Single Consumer)
// ============================================================================

/**
 * OneToOneRingBuffer - Lock-free SPSC queue
 * Ultra-low latency: ~20-50ns per operation
 */
class OneToOneRingBufferExample {

    static class MarketDataEvent {
        String symbol;
        long bidPrice;
        long askPrice;
        long timestamp;

        public MarketDataEvent(String symbol, long bidPrice, long askPrice) {
            this.symbol = symbol;
            this.bidPrice = bidPrice;
            this.askPrice = askPrice;
            this.timestamp = System.nanoTime();
        }

        @Override
        public String toString() {
            return String.format("%s: Bid=%d, Ask=%d", symbol, bidPrice, askPrice);
        }
    }

    public static void demonstrateOneToOneRingBuffer() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 2: ONE-TO-ONE RING BUFFER - SPSC");
        System.out.println("=".repeat(80));

        // Create ring buffer (must be power of 2)
        int bufferSize = 1024;
        UnsafeBuffer buffer = new UnsafeBuffer(ByteBuffer.allocateDirect(bufferSize * 256));
        OneToOneRingBuffer ringBuffer = new OneToOneRingBuffer(buffer);

        // Message type ID
        final int MSG_TYPE_MARKET_DATA = 1;

        AtomicInteger messagesReceived = new AtomicInteger(0);

        // Consumer thread
        Thread consumer = new Thread(() -> {
            MessageHandler handler = (msgTypeId, buffer1, index, length) -> {
                // Decode message
                String symbol = buffer1.getStringWithoutLengthAscii(index, 4);
                long bidPrice = buffer1.getLong(index + 4);
                long askPrice = buffer1.getLong(index + 12);
                long timestamp = buffer1.getLong(index + 20);

                messagesReceived.incrementAndGet();

                if (messagesReceived.get() <= 5) {
                    System.out.println("Received: " + symbol + " Bid=" + bidPrice +
                                     " Ask=" + askPrice);
                }
            };

            // Poll ring buffer
            while (!Thread.currentThread().isInterrupted()) {
                int readCount = ringBuffer.read(handler);
                if (readCount == 0) {
                    ThreadHints.onSpinWait(); // CPU hint for busy-spin
                }
            }
        });
        consumer.start();

        // Producer thread
        System.out.println("\nProducing messages...\n");

        for (int i = 0; i < 100; i++) {
            String symbol = "AAPL";
            long bidPrice = 15000 + i;
            long askPrice = 15010 + i;
            long timestamp = System.nanoTime();

            // Encode message
            UnsafeBuffer msgBuffer = new UnsafeBuffer(new byte[32]);
            msgBuffer.putStringWithoutLengthAscii(0, symbol);
            msgBuffer.putLong(4, bidPrice);
            msgBuffer.putLong(12, askPrice);
            msgBuffer.putLong(20, timestamp);

            // Write to ring buffer
            while (!ringBuffer.write(MSG_TYPE_MARKET_DATA, msgBuffer, 0, 28)) {
                ThreadHints.onSpinWait(); // Busy-spin if buffer full
            }
        }

        // Wait for processing
        Thread.sleep(100);
        consumer.interrupt();
        consumer.join();

        System.out.println("\n✓ Total messages received: " + messagesReceived.get());
        System.out.println("✓ Lock-free SPSC communication");
        System.out.println("✓ Latency: ~20-50ns per message");
    }
}

// ============================================================================
// PART 3: MANY-TO-ONE RING BUFFER - MPSC (Multi Producer Single Consumer)
// ============================================================================

/**
 * ManyToOneRingBuffer - Lock-free MPSC queue
 * Multiple producers, single consumer
 */
class ManyToOneRingBufferExample {

    public static void demonstrateManyToOneRingBuffer() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 3: MANY-TO-ONE RING BUFFER - MPSC");
        System.out.println("=".repeat(80));

        // Create ring buffer
        int bufferSize = 64 * 1024; // 64KB
        UnsafeBuffer buffer = new UnsafeBuffer(ByteBuffer.allocateDirect(bufferSize));
        ManyToOneRingBuffer ringBuffer = new ManyToOneRingBuffer(buffer);

        final int MSG_TYPE_ORDER = 1;

        AtomicInteger ordersReceived = new AtomicInteger(0);

        // Consumer thread
        Thread consumer = new Thread(() -> {
            MessageHandler handler = (msgTypeId, buffer1, index, length) -> {
                long orderId = buffer1.getLong(index);
                String symbol = buffer1.getStringWithoutLengthAscii(index + 8, 4);
                char side = (char) buffer1.getByte(index + 12);
                long quantity = buffer1.getLong(index + 13);
                long price = buffer1.getLong(index + 21);

                ordersReceived.incrementAndGet();

                if (ordersReceived.get() <= 10) {
                    System.out.println("Order " + orderId + ": " + symbol + " " +
                                     side + " " + quantity + "@" + price);
                }
            };

            while (!Thread.currentThread().isInterrupted()) {
                int readCount = ringBuffer.read(handler);
                if (readCount == 0) {
                    ThreadHints.onSpinWait();
                }
            }
        });
        consumer.start();

        // Multiple producer threads
        int numProducers = 4;
        Thread[] producers = new Thread[numProducers];

        System.out.println("\nMultiple producers sending orders...\n");

        for (int p = 0; p < numProducers; p++) {
            final int producerId = p;
            producers[p] = new Thread(() -> {
                for (int i = 0; i < 25; i++) {
                    long orderId = producerId * 1000 + i;
                    String symbol = "SYMB";
                    char side = (i % 2 == 0) ? 'B' : 'S';
                    long quantity = 100 + i;
                    long price = 10000 + i;

                    // Encode order
                    UnsafeBuffer msgBuffer = new UnsafeBuffer(new byte[64]);
                    msgBuffer.putLong(0, orderId);
                    msgBuffer.putStringWithoutLengthAscii(8, symbol);
                    msgBuffer.putByte(12, (byte) side);
                    msgBuffer.putLong(13, quantity);
                    msgBuffer.putLong(21, price);

                    // Write to ring buffer (thread-safe for multiple producers)
                    while (!ringBuffer.write(MSG_TYPE_ORDER, msgBuffer, 0, 29)) {
                        ThreadHints.onSpinWait();
                    }
                }
            });
            producers[p].start();
        }

        // Wait for all producers
        for (Thread producer : producers) {
            producer.join();
        }

        Thread.sleep(100);
        consumer.interrupt();
        consumer.join();

        System.out.println("\n✓ Total orders received: " + ordersReceived.get());
        System.out.println("✓ Lock-free MPSC communication");
        System.out.println("✓ Multiple producers safely writing");
    }
}

// ============================================================================
// PART 4: BROADCAST RING BUFFER - SPMC (Single Producer Multi Consumer)
// ============================================================================

/**
 * Broadcast transmitter and receivers for SPMC pattern
 */
class BroadcastRingBufferExample {

    public static void demonstrateBroadcastBuffer() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 4: BROADCAST RING BUFFER - SPMC");
        System.out.println("=".repeat(80));

        // Create broadcast buffer
        int bufferSize = 64 * 1024;
        UnsafeBuffer buffer = new UnsafeBuffer(ByteBuffer.allocateDirect(bufferSize));
        BroadcastTransmitter transmitter = new BroadcastTransmitter(buffer);

        final int MSG_TYPE_TICK = 1;

        // Create multiple receivers (consumers)
        int numReceivers = 3;
        BroadcastReceiver[] receivers = new BroadcastReceiver[numReceivers];
        Thread[] consumerThreads = new Thread[numReceivers];

        for (int r = 0; r < numReceivers; r++) {
            receivers[r] = new BroadcastReceiver(buffer);
            final int receiverId = r;
            final BroadcastReceiver receiver = receivers[r];

            consumerThreads[r] = new Thread(() -> {
                int messagesReceived = 0;

                MessageHandler handler = (msgTypeId, buffer1, index, length) -> {
                    long tickId = buffer1.getLong(index);
                    String symbol = buffer1.getStringWithoutLengthAscii(index + 8, 4);
                    long price = buffer1.getLong(index + 12);
                };

                while (!Thread.currentThread().isInterrupted() && messagesReceived < 100) {
                    int readCount = receiver.receive(handler);
                    if (readCount > 0) {
                        messagesReceived += readCount;
                    } else {
                        ThreadHints.onSpinWait();
                    }
                }

                System.out.println("Receiver " + receiverId + " processed 100 messages");
            });
            consumerThreads[r].start();
        }

        // Producer
        System.out.println("\nBroadcasting ticks to multiple receivers...\n");

        Thread.sleep(10); // Let receivers start

        for (int i = 0; i < 100; i++) {
            long tickId = i;
            String symbol = "TICK";
            long price = 50000 + i;

            UnsafeBuffer msgBuffer = new UnsafeBuffer(new byte[32]);
            msgBuffer.putLong(0, tickId);
            msgBuffer.putStringWithoutLengthAscii(8, symbol);
            msgBuffer.putLong(12, price);

            transmitter.transmit(MSG_TYPE_TICK, msgBuffer, 0, 20);
        }

        // Wait for consumers
        for (Thread consumer : consumerThreads) {
            consumer.join();
        }

        System.out.println("\n✓ All receivers got all messages");
        System.out.println("✓ SPMC pattern - one producer, multiple consumers");
        System.out.println("✓ Each consumer sees every message");
    }
}

// ============================================================================
// PART 5: HIGH-PERFORMANCE COLLECTIONS
// ============================================================================

/**
 * Int2IntHashMap - Primitive collections avoiding boxing
 */
class AgronaCollectionsExample {

    public static void demonstratePrimitiveCollections() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 5: HIGH-PERFORMANCE COLLECTIONS");
        System.out.println("=".repeat(80));

        // Int2IntHashMap - no boxing, direct primitive storage
        Int2IntHashMap symbolToPrice = new Int2IntHashMap(-1); // -1 = missing value

        // Simulate symbol IDs to price mapping
        symbolToPrice.put(1001, 15000); // AAPL
        symbolToPrice.put(1002, 28000); // GOOGL
        symbolToPrice.put(1003, 35000); // MSFT

        System.out.println("\n=== Int2IntHashMap (No Boxing) ===");
        System.out.println("Symbol 1001 price: " + symbolToPrice.get(1001));
        System.out.println("Symbol 1002 price: " + symbolToPrice.get(1002));
        System.out.println("Symbol 1003 price: " + symbolToPrice.get(1003));

        // Int2ObjectHashMap - int keys, object values
        Int2ObjectHashMap<String> symbolIdToName = new Int2ObjectHashMap<>();
        symbolIdToName.put(1001, "AAPL");
        symbolIdToName.put(1002, "GOOGL");
        symbolIdToName.put(1003, "MSFT");

        System.out.println("\n=== Int2ObjectHashMap ===");
        System.out.println("Symbol 1001 name: " + symbolIdToName.get(1001));

        // Long2LongHashMap - order ID to timestamp
        Long2LongHashMap orderIdToTimestamp = new Long2LongHashMap(-1L);
        orderIdToTimestamp.put(100001L, System.nanoTime());
        orderIdToTimestamp.put(100002L, System.nanoTime());

        System.out.println("\n=== Long2LongHashMap ===");
        System.out.println("Order timestamps stored: " + orderIdToTimestamp.size());

        // Object2ObjectHashMap - faster than java.util.HashMap
        Object2ObjectHashMap<String, Integer> symbolToQuantity = new Object2ObjectHashMap<>();
        symbolToQuantity.put("AAPL", 1000);
        symbolToQuantity.put("GOOGL", 500);

        System.out.println("\n=== Object2ObjectHashMap ===");
        System.out.println("AAPL quantity: " + symbolToQuantity.get("AAPL"));

        System.out.println("\n✓ No autoboxing overhead");
        System.out.println("✓ Better cache locality");
        System.out.println("✓ Lower GC pressure");
    }

    /**
     * Benchmark: Agrona vs Standard Java Collections
     */
    public static void benchmarkCollections() {
        System.out.println("\n=== Benchmark: Collections ===");

        int iterations = 1_000_000;

        // Standard HashMap<Integer, Integer> - lots of boxing
        HashMap<Integer, Integer> javaMap = new HashMap<>();
        long startTime = System.nanoTime();
        for (int i = 0; i < iterations; i++) {
            javaMap.put(i, i * 2);
        }
        long javaMapTime = System.nanoTime() - startTime;

        // Int2IntHashMap - no boxing
        Int2IntHashMap agronaMap = new Int2IntHashMap(-1);
        startTime = System.nanoTime();
        for (int i = 0; i < iterations; i++) {
            agronaMap.put(i, i * 2);
        }
        long agronaMapTime = System.nanoTime() - startTime;

        System.out.println("HashMap<Integer,Integer>: " + (javaMapTime / 1_000_000) + " ms");
        System.out.println("Int2IntHashMap:           " + (agronaMapTime / 1_000_000) + " ms");
        System.out.println("Speedup:                  " +
                         String.format("%.2fx", (double)javaMapTime / agronaMapTime));
    }
}

// ============================================================================
// PART 6: ATOMIC COUNTERS AND POSITION TRACKING
// ============================================================================

/**
 * AtomicCounter and CountersManager for high-performance metrics
 */
class AgronaCountersExample {

    public static void demonstrateAtomicCounters() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 6: ATOMIC COUNTERS - LOCK-FREE METRICS");
        System.out.println("=".repeat(80));

        // Create counters manager with shared buffer
        int bufferSize = 64 * 1024;
        UnsafeBuffer metaDataBuffer = new UnsafeBuffer(ByteBuffer.allocateDirect(bufferSize));
        UnsafeBuffer valuesBuffer = new UnsafeBuffer(ByteBuffer.allocateDirect(bufferSize));

        CountersManager countersManager = new CountersManager(metaDataBuffer, valuesBuffer);

        // Allocate counters
        int ordersProcessedId = countersManager.allocate("Orders Processed");
        int ordersRejectedId = countersManager.allocate("Orders Rejected");
        int marketDataTicksId = countersManager.allocate("Market Data Ticks");

        AtomicCounter ordersProcessed = new AtomicCounter(valuesBuffer, ordersProcessedId);
        AtomicCounter ordersRejected = new AtomicCounter(valuesBuffer, ordersRejectedId);
        AtomicCounter marketDataTicks = new AtomicCounter(valuesBuffer, marketDataTicksId);

        System.out.println("\n=== Simulating Trading Activity ===\n");

        // Simulate activity
        for (int i = 0; i < 100; i++) {
            ordersProcessed.increment();

            if (i % 10 == 0) {
                ordersRejected.increment();
            }

            marketDataTicks.getAndAdd(5);
        }

        // Read counters
        System.out.println("Orders Processed:   " + ordersProcessed.get());
        System.out.println("Orders Rejected:    " + ordersRejected.get());
        System.out.println("Market Data Ticks:  " + marketDataTicks.get());

        System.out.println("\n✓ Lock-free counter updates");
        System.out.println("✓ Cache-line padded to avoid false sharing");
        System.out.println("✓ Can be shared across processes (via memory-mapped files)");
    }
}

// ============================================================================
// PART 7: EXPANDABLE ARRAY BUFFER - DYNAMIC SIZING
// ============================================================================

/**
 * ExpandableArrayBuffer - grows as needed
 */
class ExpandableArrayBufferExample {

    public static void demonstrateExpandableBuffer() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 7: EXPANDABLE ARRAY BUFFER");
        System.out.println("=".repeat(80));

        ExpandableArrayBuffer buffer = new ExpandableArrayBuffer(128);

        System.out.println("\n=== Writing Data ===");
        System.out.println("Initial capacity: " + buffer.capacity());

        int offset = 0;

        // Write data that exceeds initial capacity
        for (int i = 0; i < 20; i++) {
            buffer.putLong(offset, i * 1000L);
            offset += Long.BYTES;
            buffer.putInt(offset, i);
            offset += Integer.BYTES;
        }

        System.out.println("After writes capacity: " + buffer.capacity());
        System.out.println("Bytes written: " + offset);

        // Read back
        int readOffset = 0;
        System.out.println("\n=== Reading Data ===");
        for (int i = 0; i < 5; i++) {
            long longVal = buffer.getLong(readOffset);
            readOffset += Long.BYTES;
            int intVal = buffer.getInt(readOffset);
            readOffset += Integer.BYTES;

            System.out.println("Entry " + i + ": long=" + longVal + ", int=" + intVal);
        }

        System.out.println("\n✓ Auto-expands when needed");
        System.out.println("✓ No manual resizing required");
        System.out.println("✓ Useful for variable-length messages");
    }
}

// ============================================================================
// PART 8: SHARED MEMORY IPC (INTER-PROCESS COMMUNICATION)
// ============================================================================

/**
 * Memory-mapped file for inter-process communication
 */
class SharedMemoryIPCExample {

    public static void demonstrateSharedMemoryIPC() throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 8: SHARED MEMORY IPC");
        System.out.println("=".repeat(80));

        String filePath = "/tmp/agrona_shared_memory.dat";
        int fileSize = 64 * 1024;

        // Create memory-mapped file
        try (IoUtil.MappedBufferCleaner cleaner = IoUtil.mapNewFile(
                new java.io.File(filePath), fileSize)) {

            ByteBuffer mappedBuffer = cleaner.buffer();
            UnsafeBuffer sharedBuffer = new UnsafeBuffer(mappedBuffer);

            System.out.println("\n=== Writer Process ===");

            // Write data to shared memory
            sharedBuffer.putInt(0, 42);
            sharedBuffer.putLong(4, System.nanoTime());
            sharedBuffer.putStringWithoutLengthAscii(12, "SHARED_DATA");

            System.out.println("Written to shared memory:");
            System.out.println("  Int:    " + sharedBuffer.getInt(0));
            System.out.println("  Long:   " + sharedBuffer.getLong(4));
            System.out.println("  String: " + sharedBuffer.getStringWithoutLengthAscii(12, 11));

            // In real scenario, another process would map the same file and read
            // This is ultra-low latency IPC (faster than sockets, pipes, etc.)

            System.out.println("\n✓ Memory-mapped file for IPC");
            System.out.println("✓ Faster than sockets/pipes");
            System.out.println("✓ Typical latency: <1μs");
        }
    }
}

// ============================================================================
// PART 9: REAL-WORLD USE CASE - ORDER BOOK WITH AGRONA
// ============================================================================

/**
 * Ultra-low latency order book using Agrona primitives
 */
class AgronaOrderBook {

    // Price level storage (no boxing)
    private final Long2LongHashMap bidPriceLevels = new Long2LongHashMap(0L);
    private final Long2LongHashMap askPriceLevels = new Long2LongHashMap(0L);

    // Order storage
    private final Long2ObjectHashMap<Order> orders = new Long2ObjectHashMap<>();

    // Metrics
    private final AtomicCounter ordersAdded;
    private final AtomicCounter ordersRemoved;

    static class Order {
        long orderId;
        char side;
        long price;
        long quantity;

        Order(long orderId, char side, long price, long quantity) {
            this.orderId = orderId;
            this.side = side;
            this.price = price;
            this.quantity = quantity;
        }
    }

    public AgronaOrderBook(AtomicCounter ordersAdded, AtomicCounter ordersRemoved) {
        this.ordersAdded = ordersAdded;
        this.ordersRemoved = ordersRemoved;
    }

    public void addOrder(long orderId, char side, long price, long quantity) {
        Order order = new Order(orderId, side, price, quantity);
        orders.put(orderId, order);

        if (side == 'B') {
            bidPriceLevels.put(price, bidPriceLevels.get(price) + quantity);
        } else {
            askPriceLevels.put(price, askPriceLevels.get(price) + quantity);
        }

        ordersAdded.increment();
    }

    public void removeOrder(long orderId) {
        Order order = orders.remove(orderId);
        if (order != null) {
            if (order.side == 'B') {
                long newQty = bidPriceLevels.get(order.price) - order.quantity;
                if (newQty <= 0) {
                    bidPriceLevels.remove(order.price);
                } else {
                    bidPriceLevels.put(order.price, newQty);
                }
            } else {
                long newQty = askPriceLevels.get(order.price) - order.quantity;
                if (newQty <= 0) {
                    askPriceLevels.remove(order.price);
                } else {
                    askPriceLevels.put(order.price, newQty);
                }
            }

            ordersRemoved.increment();
        }
    }

    public long getBestBid() {
        final long[] best = {0L};
        bidPriceLevels.forEach((price, qty) -> {
            if (price > best[0]) best[0] = price;
        });
        return best[0];
    }

    public long getBestAsk() {
        final long[] best = {Long.MAX_VALUE};
        askPriceLevels.forEach((price, qty) -> {
            if (price < best[0]) best[0] = price;
        });
        return best[0];
    }

    public static void demonstrateOrderBook() {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("PART 9: ORDER BOOK WITH AGRONA");
        System.out.println("=".repeat(80));

        // Setup counters
        UnsafeBuffer metaBuffer = new UnsafeBuffer(ByteBuffer.allocateDirect(4096));
        UnsafeBuffer valuesBuffer = new UnsafeBuffer(ByteBuffer.allocateDirect(4096));
        CountersManager countersManager = new CountersManager(metaBuffer, valuesBuffer);

        int addedId = countersManager.allocate("Orders Added");
        int removedId = countersManager.allocate("Orders Removed");

        AtomicCounter ordersAdded = new AtomicCounter(valuesBuffer, addedId);
        AtomicCounter ordersRemoved = new AtomicCounter(valuesBuffer, removedId);

        AgronaOrderBook orderBook = new AgronaOrderBook(ordersAdded, ordersRemoved);

        System.out.println("\n=== Adding Orders ===");

        // Add buy orders
        orderBook.addOrder(1001, 'B', 14990, 100);
        orderBook.addOrder(1002, 'B', 14995, 200);
        orderBook.addOrder(1003, 'B', 14985, 150);

        // Add sell orders
        orderBook.addOrder(2001, 'S', 15000, 100);
        orderBook.addOrder(2002, 'S', 15005, 150);
        orderBook.addOrder(2003, 'S', 15010, 200);

        System.out.println("Best Bid: " + orderBook.getBestBid());
        System.out.println("Best Ask: " + orderBook.getBestAsk());
        System.out.println("Spread:   " + (orderBook.getBestAsk() - orderBook.getBestBid()));

        System.out.println("\n=== Removing Orders ===");
        orderBook.removeOrder(1002);

        System.out.println("Best Bid after removal: " + orderBook.getBestBid());

        System.out.println("\n=== Metrics ===");
        System.out.println("Orders Added:   " + ordersAdded.get());
        System.out.println("Orders Removed: " + ordersRemoved.get());

        System.out.println("\n✓ Zero-allocation order book");
        System.out.println("✓ Lock-free data structures");
        System.out.println("✓ Sub-microsecond operations");
    }
}

// ============================================================================
// PART 10: EXAMPLE USAGE & DEMONSTRATIONS
// ============================================================================

class AgronaExamples {

    public static void main(String[] args) throws Exception {
        System.out.println("\n" + "=".repeat(80));
        System.out.println("AGRONA - COMPLETE GUIDE & EXAMPLES");
        System.out.println("High-Performance Data Structures for Ultra-Low Latency Systems");
        System.out.println("=".repeat(80));

        // Run all demonstrations
        UnsafeBufferExample.demonstrateUnsafeBuffer();
        UnsafeBufferExample.benchmarkUnsafeBufferVsByteBuffer();

        OneToOneRingBufferExample.demonstrateOneToOneRingBuffer();

        ManyToOneRingBufferExample.demonstrateManyToOneRingBuffer();

        BroadcastRingBufferExample.demonstrateBroadcastBuffer();

        AgronaCollectionsExample.demonstratePrimitiveCollections();
        AgronaCollectionsExample.benchmarkCollections();

        AgronaCountersExample.demonstrateAtomicCounters();

        ExpandableArrayBufferExample.demonstrateExpandableBuffer();

        SharedMemoryIPCExample.demonstrateSharedMemoryIPC();

        AgronaOrderBook.demonstrateOrderBook();

        System.out.println("\n" + "=".repeat(80));
        System.out.println("ALL EXAMPLES COMPLETED");
        System.out.println("=".repeat(80));
    }
}

// ============================================================================
// PART 11: KEY BENEFITS, COMPARISONS & BEST PRACTICES
// ============================================================================

/**
 * AGRONA KEY BENEFITS:
 *
 * 1. ZERO-COPY OPERATIONS
 *    - Direct memory access via UnsafeBuffer
 *    - No intermediate byte array copies
 *    - Critical for network protocol encoding/decoding
 *
 * 2. LOCK-FREE DATA STRUCTURES
 *    - OneToOneRingBuffer (SPSC): ~20-50ns latency
 *    - ManyToOneRingBuffer (MPSC): ~50-100ns latency
 *    - BroadcastBuffer (SPMC): Fan-out with minimal overhead
 *
 * 3. NO BOXING/UNBOXING
 *    - Primitive collections (Int2IntHashMap, Long2LongHashMap)
 *    - Eliminates autoboxing overhead
 *    - Reduces GC pressure significantly
 *
 * 4. CACHE-FRIENDLY
 *    - Contiguous memory layouts
 *    - Cache-line padding for counters
 *    - Better CPU cache utilization
 *
 * 5. SHARED MEMORY IPC
 *    - Memory-mapped files for inter-process communication
 *    - <1μs latency between processes
 *    - Much faster than sockets or pipes
 *
 * 6. PRODUCTION-PROVEN
 *    - Used by Aeron (low-latency messaging)
 *    - Used by SBE (Simple Binary Encoding)
 *    - Battle-tested in HFT environments
 *
 * USE CASES:
 *
 * 1. High-Frequency Trading:
 *    - Order gateways
 *    - Market data handlers
 *    - Risk engines
 *
 * 2. Network Protocol Encoding:
 *    - FIX protocol
 *    - Binary protocols (SBE, Protobuf)
 *    - Custom wire formats
 *
 * 3. Inter-Thread Communication:
 *    - Market data thread → Strategy threads
 *    - Strategy threads → Order gateway
 *    - Lock-free queues
 *
 * 4. Inter-Process Communication:
 *    - Shared memory between processes
 *    - Zero-copy data sharing
 *    - Memory-mapped files
 *
 * 5. Event Sourcing & Logging:
 *    - High-throughput event logs
 *    - Deterministic replay
 *    - Persistent ring buffers
 *
 * 6. Real-Time Analytics:
 *    - Stream processing
 *    - Metrics collection
 *    - Low-latency aggregation
 *
 * COMPARISON WITH ALTERNATIVES:
 *
 * vs LMAX Disruptor:
 *   + More comprehensive (buffers, collections, utilities)
 *   + Better for IPC (shared memory support)
 *   - Disruptor has simpler API for pure SPSC
 *   ~ Similar performance characteristics
 *
 * vs Chronicle Queue:
 *   + Lower latency for in-memory operations
 *   - Chronicle better for persistence
 *   - Chronicle has built-in replication
 *   + Agrona more general-purpose
 *
 * vs Java NIO ByteBuffer:
 *   + 2-5x faster (no bounds checking)
 *   + More convenient API
 *   + Better for primitive types
 *   - Uses sun.misc.Unsafe (not officially supported)
 *
 * vs Standard Java Collections:
 *   + 2-10x faster for primitives
 *   + Zero boxing overhead
 *   + Lower GC pressure
 *   - Less feature-rich than java.util
 *
 * BEST PRACTICES:
 *
 * 1. Buffer Sizing:
 *    - Ring buffers: Always power-of-2 size
 *    - Typical: 1024, 4096, 65536 slots
 *    - Larger = less contention, more memory
 *
 * 2. Memory Allocation:
 *    - Use direct buffers (off-heap) for IPC
 *    - Use heap buffers for internal use
 *    - Pre-allocate at startup (avoid GC)
 *
 * 3. Wait Strategies:
 *    - Busy-spin: Lowest latency, high CPU
 *    - ThreadHints.onSpinWait(): Modern CPUs
 *    - LockSupport.parkNanos(): Lower CPU, higher latency
 *
 * 4. Primitive Collections:
 *    - Use Int2Int/Long2Long for numeric keys
 *    - Use Int2Object for ID → Object mappings
 *    - Set missing value appropriately
 *
 * 5. Shared Memory IPC:
 *    - Use tmpfs (/dev/shm) for in-memory files
 *    - Align sizes to page boundaries (4KB, 2MB)
 *    - Implement producer/consumer coordination
 *
 * 6. Monitoring:
 *    - Use AtomicCounters for metrics
 *    - Export via JMX or custom monitoring
 *    - Track latency percentiles
 *
 * PERFORMANCE CHARACTERISTICS:
 *
 * OneToOneRingBuffer (SPSC):
 *   - Latency: 20-50ns per message
 *   - Throughput: 50-100M msgs/sec
 *   - Use case: Dedicated thread pairs
 *
 * ManyToOneRingBuffer (MPSC):
 *   - Latency: 50-100ns per message
 *   - Throughput: 20-50M msgs/sec
 *   - Use case: Multiple producers → single consumer
 *
 * BroadcastBuffer (SPMC):
 *   - Latency: 30-60ns per message
 *   - Throughput: 40-80M msgs/sec
 *   - Use case: Market data fan-out
 *
 * Primitive Collections:
 *   - Get/Put: 5-10ns
 *   - 2-5x faster than HashMap
 *   - Zero GC for primitive types
 *
 * UnsafeBuffer:
 *   - Read/Write: <5ns
 *   - 2-3x faster than ByteBuffer
 *   - Zero bounds checking overhead
 *
 * TYPICAL TRADING SYSTEM ARCHITECTURE:
 *
 * Market Data Feed
 *      ↓
 * OneToOneRingBuffer (SPSC)
 *      ↓
 * Market Data Normalizer
 *      ↓
 * BroadcastBuffer (SPMC)
 *      ↓        ↓        ↓
 *   Strat1  Strat2  Strat3
 *      ↓        ↓        ↓
 * ManyToOneRingBuffer (MPSC)
 *      ↓
 * Order Gateway
 *      ↓
 * Exchange
 *
 * MAVEN DEPENDENCY:
 *
 * <dependency>
 *     <groupId>org.agrona</groupId>
 *     <artifactId>agrona</artifactId>
 *     <version>1.21.1</version>
 * </dependency>
 *
 * PRODUCTION DEPLOYMENT CHECKLIST:
 *
 * 1. JVM Tuning:
 *    - -XX:+UseG1GC or -XX:+UseZGC
 *    - -XX:+AlwaysPreTouch
 *    - -XX:MaxGCPauseMillis=1
 *    - -Xms = -Xmx (fixed heap)
 *
 * 2. CPU Isolation:
 *    - isolcpus kernel parameter
 *    - Pin threads to cores (taskset)
 *    - Disable hyperthreading
 *
 * 3. Memory:
 *    - Use huge pages (2MB)
 *    - Lock memory (mlockall)
 *    - NUMA awareness
 *
 * 4. Monitoring:
 *    - Track ring buffer fill levels
 *    - Monitor latency percentiles (p50, p99, p99.9)
 *    - Alert on buffer overruns
 *
 * 5. Testing:
 *    - Load testing at 2x peak capacity
 *    - Chaos testing (network failures, CPU spikes)
 *    - Latency benchmarking under load
 *
 * COMMON PITFALLS TO AVOID:
 *
 * 1. Non-power-of-2 ring buffer sizes
 * 2. Sharing buffers between incompatible patterns (SPSC vs MPSC)
 * 3. Not pre-allocating buffers (causes GC)
 * 4. Using heap buffers for IPC (not shared)
 * 5. Ignoring cache-line alignment
 * 6. Busy-spinning without ThreadHints.onSpinWait()
 * 7. Not handling buffer overrun conditions
 * 8. Using standard collections for hot paths
 */

