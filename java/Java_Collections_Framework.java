/**
 * Java Collections Framework - Complete Guide for C++ Developers
 *
 * This file demonstrates all major Java collections with their C++ equivalents
 * and use cases in capital markets trading applications.
 */

import java.util.*;
import java.util.concurrent.*;
import java.time.LocalDateTime;
import java.math.BigDecimal;

public class Java_Collections_Framework {

    // Trading-related classes for examples
    static class Order {
        String symbol;
        String orderId;
        BigDecimal price;
        int quantity;
        LocalDateTime timestamp;

        public Order(String symbol, String orderId, BigDecimal price, int quantity) {
            this.symbol = symbol;
            this.orderId = orderId;
            this.price = price;
            this.quantity = quantity;
            this.timestamp = LocalDateTime.now();
        }

        @Override
        public String toString() {
            return String.format("Order{%s, %s, $%.2f, qty:%d}",
                symbol, orderId, price, quantity);
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) return true;
            if (obj == null || getClass() != obj.getClass()) return false;
            Order order = (Order) obj;
            return Objects.equals(orderId, order.orderId);
        }

        @Override
        public int hashCode() {
            return Objects.hash(orderId);
        }
    }

    static class Trade {
        String symbol;
        BigDecimal price;
        int volume;
        LocalDateTime timestamp;

        public Trade(String symbol, BigDecimal price, int volume) {
            this.symbol = symbol;
            this.price = price;
            this.volume = volume;
            this.timestamp = LocalDateTime.now();
        }

        @Override
        public String toString() {
            return String.format("Trade{%s, $%.2f, vol:%d}", symbol, price, volume);
        }
    }

    public static void demonstrateCollections() {

        // =============================================
        // 1. LIST INTERFACE - Dynamic Arrays
        // =============================================

        // ArrayList - C++ equivalent: std::vector
        // Resizable array, good for random access
        System.out.println("=== ArrayList (like std::vector) ===");
        List<Order> orderBook = new ArrayList<>();
        orderBook.add(new Order("AAPL", "ORD001", new BigDecimal("150.50"), 100));
        orderBook.add(new Order("GOOGL", "ORD002", new BigDecimal("2800.00"), 50));
        orderBook.add(new Order("MSFT", "ORD003", new BigDecimal("300.25"), 200));

        // Access by index (O(1) like std::vector)
        Order firstOrder = orderBook.get(0);
        System.out.println("First order: " + firstOrder);

        // Iteration (multiple ways)
        System.out.println("All orders:");
        for (Order order : orderBook) {  // Enhanced for loop
            System.out.println("  " + order);
        }

        // Lambda with streams (Java 8+)
        orderBook.stream()
                 .filter(order -> order.price.compareTo(new BigDecimal("200")) > 0)
                 .forEach(order -> System.out.println("  High value: " + order));

        // LinkedList - C++ equivalent: std::list
        // Doubly linked list, good for insertions/deletions
        System.out.println("\n=== LinkedList (like std::list) ===");
        LinkedList<Trade> tradeHistory = new LinkedList<>();
        tradeHistory.addFirst(new Trade("AAPL", new BigDecimal("150.75"), 1000));
        tradeHistory.addLast(new Trade("GOOGL", new BigDecimal("2805.50"), 500));

        // Deque operations (double-ended queue)
        Trade latestTrade = tradeHistory.peekLast();
        System.out.println("Latest trade: " + latestTrade);

        // =============================================
        // 2. SET INTERFACE - Unique Elements
        // =============================================

        // HashSet - C++ equivalent: std::unordered_set
        // Hash table implementation, O(1) average for basic operations
        System.out.println("\n=== HashSet (like std::unordered_set) ===");
        Set<String> uniqueSymbols = new HashSet<>();
        uniqueSymbols.add("AAPL");
        uniqueSymbols.add("GOOGL");
        uniqueSymbols.add("AAPL");  // Duplicate - won't be added

        System.out.println("Unique symbols: " + uniqueSymbols);  // Only shows AAPL once

        // TreeSet - C++ equivalent: std::set
        // Sorted set using Red-Black tree, O(log n) operations
        System.out.println("\n=== TreeSet (like std::set) ===");
        Set<String> sortedSymbols = new TreeSet<>();
        sortedSymbols.add("MSFT");
        sortedSymbols.add("AAPL");
        sortedSymbols.add("GOOGL");

        System.out.println("Sorted symbols: " + sortedSymbols);  // Alphabetically sorted

        // LinkedHashSet - Maintains insertion order
        System.out.println("\n=== LinkedHashSet (insertion order) ===");
        Set<String> orderedSymbols = new LinkedHashSet<>();
        orderedSymbols.add("MSFT");
        orderedSymbols.add("AAPL");
        orderedSymbols.add("GOOGL");

        System.out.println("Insertion-ordered symbols: " + orderedSymbols);

        // =============================================
        // 3. MAP INTERFACE - Key-Value Pairs
        // =============================================

        // HashMap - C++ equivalent: std::unordered_map
        // Hash table, O(1) average for get/put operations
        System.out.println("\n=== HashMap (like std::unordered_map) ===");
        Map<String, BigDecimal> lastPrices = new HashMap<>();
        lastPrices.put("AAPL", new BigDecimal("150.50"));
        lastPrices.put("GOOGL", new BigDecimal("2800.00"));
        lastPrices.put("MSFT", new BigDecimal("300.25"));

        BigDecimal aaplPrice = lastPrices.get("AAPL");
        System.out.println("AAPL last price: $" + aaplPrice);

        // Iterate over map
        System.out.println("All last prices:");
        for (Map.Entry<String, BigDecimal> entry : lastPrices.entrySet()) {
            System.out.println("  " + entry.getKey() + ": $" + entry.getValue());
        }

        // Lambda iteration
        lastPrices.forEach((symbol, price) ->
            System.out.println("  " + symbol + " -> $" + price));

        // TreeMap - C++ equivalent: std::map
        // Sorted map using Red-Black tree
        System.out.println("\n=== TreeMap (like std::map) ===");
        Map<String, Integer> symbolVolumes = new TreeMap<>();
        symbolVolumes.put("MSFT", 50000);
        symbolVolumes.put("AAPL", 100000);
        symbolVolumes.put("GOOGL", 25000);

        System.out.println("Volumes (sorted by symbol): " + symbolVolumes);

        // LinkedHashMap - Maintains insertion or access order
        System.out.println("\n=== LinkedHashMap (insertion order) ===");
        Map<String, String> marketStatus = new LinkedHashMap<>();
        marketStatus.put("Pre-Market", "OPEN");
        marketStatus.put("Regular", "OPEN");
        marketStatus.put("After-Hours", "CLOSED");

        System.out.println("Market status: " + marketStatus);

        // =============================================
        // 4. QUEUE INTERFACE - FIFO Collections
        // =============================================

        // ArrayDeque - C++ equivalent: std::deque
        // Double-ended queue, can be used as stack or queue
        System.out.println("\n=== ArrayDeque (like std::deque) ===");
        Deque<Order> pendingOrders = new ArrayDeque<>();

        // Queue operations (FIFO)
        pendingOrders.offer(new Order("AAPL", "ORD004", new BigDecimal("151.00"), 100));
        pendingOrders.offer(new Order("GOOGL", "ORD005", new BigDecimal("2810.00"), 50));

        Order nextOrder = pendingOrders.poll();  // Remove and return head
        System.out.println("Processing order: " + nextOrder);

        // Stack operations (LIFO)
        pendingOrders.push(new Order("MSFT", "ORD006", new BigDecimal("301.00"), 75));
        Order topOrder = pendingOrders.pop();  // Remove and return top
        System.out.println("Latest order: " + topOrder);

        // PriorityQueue - C++ equivalent: std::priority_queue
        // Heap-based priority queue
        System.out.println("\n=== PriorityQueue (like std::priority_queue) ===");
        PriorityQueue<Order> priorityOrders = new PriorityQueue<>(
            (o1, o2) -> o2.price.compareTo(o1.price)  // Higher price = higher priority
        );

        priorityOrders.offer(new Order("AAPL", "ORD007", new BigDecimal("150.00"), 100));
        priorityOrders.offer(new Order("AAPL", "ORD008", new BigDecimal("152.00"), 50));
        priorityOrders.offer(new Order("AAPL", "ORD009", new BigDecimal("151.00"), 75));

        System.out.println("Orders by priority (highest price first):");
        while (!priorityOrders.isEmpty()) {
            System.out.println("  " + priorityOrders.poll());
        }

        // =============================================
        // 5. CONCURRENT COLLECTIONS - Thread-Safe
        // =============================================

        System.out.println("\n=== Concurrent Collections ===");

        // ConcurrentHashMap - Thread-safe HashMap
        ConcurrentHashMap<String, BigDecimal> realTimePrices = new ConcurrentHashMap<>();
        realTimePrices.put("AAPL", new BigDecimal("150.50"));

        // Atomic operations
        realTimePrices.compute("AAPL", (key, oldPrice) ->
            oldPrice.add(new BigDecimal("0.25")));  // Atomic price update

        System.out.println("Updated AAPL price: $" + realTimePrices.get("AAPL"));

        // CopyOnWriteArrayList - Thread-safe list for read-heavy scenarios
        CopyOnWriteArrayList<String> watchList = new CopyOnWriteArrayList<>();
        watchList.add("AAPL");
        watchList.add("GOOGL");

        // BlockingQueue - Producer-consumer pattern
        BlockingQueue<Trade> tradeQueue = new ArrayBlockingQueue<>(1000);

        // Producer thread would do:
        // tradeQueue.put(new Trade("AAPL", new BigDecimal("150.75"), 100));

        // Consumer thread would do:
        // Trade trade = tradeQueue.take();  // Blocks if queue is empty

        // =============================================
        // 6. UTILITY COLLECTIONS
        // =============================================

        System.out.println("\n=== Utility Collections ===");

        // Collections utility class methods
        List<String> symbols = Arrays.asList("MSFT", "AAPL", "GOOGL");

        // Sort
        Collections.sort(symbols);
        System.out.println("Sorted symbols: " + symbols);

        // Reverse
        Collections.reverse(symbols);
        System.out.println("Reversed symbols: " + symbols);

        // Binary search (list must be sorted)
        Collections.sort(symbols);
        int index = Collections.binarySearch(symbols, "GOOGL");
        System.out.println("GOOGL found at index: " + index);

        // Immutable collections (Java 9+)
        List<String> immutableList = List.of("AAPL", "GOOGL", "MSFT");
        Set<String> immutableSet = Set.of("AAPL", "GOOGL", "MSFT");
        Map<String, String> immutableMap = Map.of(
            "AAPL", "Apple",
            "GOOGL", "Google",
            "MSFT", "Microsoft"
        );

        System.out.println("Immutable list: " + immutableList);
        // immutableList.add("TSLA");  // Would throw UnsupportedOperationException

        // =============================================
        // 7. PERFORMANCE CHARACTERISTICS SUMMARY
        // =============================================

        System.out.println("\n=== Performance Summary ===");
        System.out.println("ArrayList:        Random access O(1), Insert/Delete O(n)");
        System.out.println("LinkedList:       Insert/Delete O(1), Random access O(n)");
        System.out.println("HashSet:          Add/Remove/Contains O(1) average");
        System.out.println("TreeSet:          Add/Remove/Contains O(log n)");
        System.out.println("HashMap:          Get/Put O(1) average");
        System.out.println("TreeMap:          Get/Put O(log n)");
        System.out.println("ArrayDeque:       Add/Remove at ends O(1)");
        System.out.println("PriorityQueue:    Add O(log n), Remove O(log n)");
    }

    // =============================================
    // 8. REAL-WORLD TRADING EXAMPLE
    // =============================================

    static class OrderBook {
        private final TreeMap<BigDecimal, List<Order>> buyOrders;  // Price -> Orders
        private final TreeMap<BigDecimal, List<Order>> sellOrders;
        private final Map<String, Order> orderIndex;  // OrderId -> Order

        public OrderBook() {
            this.buyOrders = new TreeMap<>(Collections.reverseOrder());  // Highest price first
            this.sellOrders = new TreeMap<>();  // Lowest price first
            this.orderIndex = new ConcurrentHashMap<>();
        }

        public void addBuyOrder(Order order) {
            buyOrders.computeIfAbsent(order.price, k -> new ArrayList<>()).add(order);
            orderIndex.put(order.orderId, order);
        }

        public void addSellOrder(Order order) {
            sellOrders.computeIfAbsent(order.price, k -> new ArrayList<>()).add(order);
            orderIndex.put(order.orderId, order);
        }

        public BigDecimal getBestBid() {
            return buyOrders.isEmpty() ? null : buyOrders.firstKey();
        }

        public BigDecimal getBestAsk() {
            return sellOrders.isEmpty() ? null : sellOrders.firstKey();
        }

        public void printOrderBook() {
            System.out.println("\n=== Order Book ===");
            System.out.println("Best Bid: $" + getBestBid());
            System.out.println("Best Ask: $" + getBestAsk());

            System.out.println("Buy Orders:");
            buyOrders.entrySet().stream()
                     .limit(3)  // Top 3 levels
                     .forEach(entry ->
                         System.out.println("  $" + entry.getKey() + " -> " +
                                          entry.getValue().size() + " orders"));

            System.out.println("Sell Orders:");
            sellOrders.entrySet().stream()
                      .limit(3)  // Top 3 levels
                      .forEach(entry ->
                          System.out.println("  $" + entry.getKey() + " -> " +
                                           entry.getValue().size() + " orders"));
        }
    }

    public static void demonstrateOrderBook() {
        System.out.println("\n=== Real-world Order Book Example ===");

        OrderBook orderBook = new OrderBook();

        // Add buy orders
        orderBook.addBuyOrder(new Order("AAPL", "BUY001", new BigDecimal("150.00"), 100));
        orderBook.addBuyOrder(new Order("AAPL", "BUY002", new BigDecimal("149.95"), 200));
        orderBook.addBuyOrder(new Order("AAPL", "BUY003", new BigDecimal("150.05"), 150));

        // Add sell orders
        orderBook.addSellOrder(new Order("AAPL", "SELL001", new BigDecimal("150.10"), 100));
        orderBook.addSellOrder(new Order("AAPL", "SELL002", new BigDecimal("150.15"), 75));
        orderBook.addSellOrder(new Order("AAPL", "SELL003", new BigDecimal("150.08"), 200));

        orderBook.printOrderBook();
    }

    public static void main(String[] args) {
        demonstrateCollections();
        demonstrateOrderBook();

        System.out.println("\n=== Key Takeaways for C++ Developers ===");
        System.out.println("1. Java collections are reference-based (no value semantics)");
        System.out.println("2. Use generics <T> for type safety (like C++ templates but different)");
        System.out.println("3. Enhanced for-loop and streams make iteration elegant");
        System.out.println("4. Concurrent collections handle thread-safety automatically");
        System.out.println("5. Collections.* utility methods provide common algorithms");
        System.out.println("6. Choose collection based on access patterns and performance needs");
    }
}

/*
=== Collection Choice Guidelines ===

For Trading Applications:

1. Order Storage: LinkedList or ArrayDeque for FIFO processing
2. Price Levels: TreeMap for sorted price levels
3. Symbol Lookup: HashMap for O(1) symbol-to-data mapping
4. Unique Symbols: HashSet for symbol validation
5. Priority Orders: PriorityQueue for order prioritization
6. Thread-Safe: ConcurrentHashMap, BlockingQueue for multi-threaded processing
7. Large Datasets: ArrayList for memory efficiency
8. Frequent Inserts/Deletes: LinkedList or ArrayDeque

Performance Rules:
- ArrayList: Best for read-heavy, random access
- LinkedList: Best for frequent insertions/deletions
- HashMap: Best for key-based lookups
- TreeMap: Best when you need sorted keys
- ConcurrentHashMap: Best for thread-safe key-value storage
- ArrayDeque: Best for queue/stack operations
*/
