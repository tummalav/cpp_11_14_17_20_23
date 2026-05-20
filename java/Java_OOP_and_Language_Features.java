/**
 * Java OOP and Language Features for C++ Developers
 *
 * This file covers Java's object-oriented features, generics, lambdas,
 * and other language features with trading examples.
 */

import java.util.*;
import java.util.function.*;
import java.math.BigDecimal;
import java.time.LocalDateTime;
import java.io.*;

public class Java_OOP_and_Language_Features {

    // =============================================
    // 1. INTERFACES vs C++ PURE VIRTUAL CLASSES
    // =============================================

    // Interface - like C++ pure virtual class but more powerful
    interface TradingStrategy {
        // Abstract method (implicitly public abstract)
        BigDecimal calculateSignal(String symbol, BigDecimal currentPrice);

        // Default method (Java 8+) - provides implementation
        default void logTrade(String symbol, String action, BigDecimal price) {
            System.out.println(LocalDateTime.now() + ": " + action + " " + symbol + " at $" + price);
        }

        // Static method (Java 8+)
        static boolean isValidSymbol(String symbol) {
            return symbol != null && symbol.length() >= 3 && symbol.length() <= 5;
        }

        // Constants (implicitly public static final)
        double RISK_THRESHOLD = 0.02;  // 2% risk threshold
    }

    // Multiple interface implementation (like multiple inheritance but safer)
    interface RiskManager {
        boolean isRiskAcceptable(BigDecimal position, BigDecimal price);
    }

    interface PerformanceTracker {
        void recordTrade(String symbol, BigDecimal pnl);
        BigDecimal getTotalPnL();
    }

    // Concrete implementation
    static class MomentumStrategy implements TradingStrategy, RiskManager, PerformanceTracker {
        private BigDecimal totalPnL = BigDecimal.ZERO;
        private final BigDecimal maxPositionSize;

        public MomentumStrategy(BigDecimal maxPositionSize) {
            this.maxPositionSize = maxPositionSize;
        }

        @Override
        public BigDecimal calculateSignal(String symbol, BigDecimal currentPrice) {
            // Simple momentum calculation (buy if price > moving average)
            BigDecimal movingAverage = getMovingAverage(symbol);  // Stub
            if (currentPrice.compareTo(movingAverage.multiply(new BigDecimal("1.02"))) > 0) {
                return new BigDecimal("1000");  // Buy 1000 shares
            } else if (currentPrice.compareTo(movingAverage.multiply(new BigDecimal("0.98"))) < 0) {
                return new BigDecimal("-1000");  // Sell 1000 shares
            }
            return BigDecimal.ZERO;  // Hold
        }

        @Override
        public boolean isRiskAcceptable(BigDecimal position, BigDecimal price) {
            BigDecimal totalExposure = position.multiply(price).abs();
            return totalExposure.compareTo(maxPositionSize) <= 0;
        }

        @Override
        public void recordTrade(String symbol, BigDecimal pnl) {
            totalPnL = totalPnL.add(pnl);
            System.out.println("Trade PnL: $" + pnl + ", Total PnL: $" + totalPnL);
        }

        @Override
        public BigDecimal getTotalPnL() {
            return totalPnL;
        }

        private BigDecimal getMovingAverage(String symbol) {
            // Stub implementation
            return new BigDecimal("150.00");
        }
    }

    // =============================================
    // 2. ABSTRACT CLASSES vs INTERFACES
    // =============================================

    // Abstract class - can have state and concrete methods
    abstract class BaseOrder {
        protected String orderId;
        protected String symbol;
        protected BigDecimal price;
        protected int quantity;
        protected LocalDateTime timestamp;

        // Constructor in abstract class
        public BaseOrder(String orderId, String symbol, BigDecimal price, int quantity) {
            this.orderId = orderId;
            this.symbol = symbol;
            this.price = price;
            this.quantity = quantity;
            this.timestamp = LocalDateTime.now();
        }

        // Concrete method
        public String getOrderSummary() {
            return String.format("Order %s: %s %d shares of %s at $%.2f",
                orderId, getOrderType(), quantity, symbol, price);
        }

        // Abstract method - must be implemented by subclasses
        public abstract String getOrderType();
        public abstract BigDecimal calculateCommission();

        // Template method pattern
        public final void processOrder() {
            validateOrder();
            submitOrder();
            logOrder();
        }

        protected void validateOrder() {
            if (price.compareTo(BigDecimal.ZERO) <= 0) {
                throw new IllegalArgumentException("Price must be positive");
            }
        }

        protected abstract void submitOrder();

        protected void logOrder() {
            System.out.println("Order logged: " + getOrderSummary());
        }
    }

    // Concrete subclass
    static class MarketOrder extends BaseOrder {
        public MarketOrder(String orderId, String symbol, BigDecimal price, int quantity) {
            super(orderId, symbol, price, quantity);
        }

        @Override
        public String getOrderType() {
            return "MARKET";
        }

        @Override
        public BigDecimal calculateCommission() {
            return new BigDecimal("9.99");  // Fixed commission
        }

        @Override
        protected void submitOrder() {
            System.out.println("Submitting market order to exchange");
        }
    }

    static class LimitOrder extends BaseOrder {
        private final BigDecimal limitPrice;

        public LimitOrder(String orderId, String symbol, BigDecimal price,
                         int quantity, BigDecimal limitPrice) {
            super(orderId, symbol, price, quantity);
            this.limitPrice = limitPrice;
        }

        @Override
        public String getOrderType() {
            return "LIMIT";
        }

        @Override
        public BigDecimal calculateCommission() {
            // Commission based on trade size
            BigDecimal tradeValue = limitPrice.multiply(new BigDecimal(quantity));
            return tradeValue.multiply(new BigDecimal("0.001"));  // 0.1%
        }

        @Override
        protected void submitOrder() {
            System.out.println("Submitting limit order with limit price $" + limitPrice);
        }
    }

    // =============================================
    // 3. GENERICS - Type Safety without Templates
    // =============================================

    // Generic class with bounded type parameters
    static class OrderBook<T extends BaseOrder> {
        private final List<T> orders = new ArrayList<>();
        private final Class<T> orderType;

        // Constructor with class token (handles type erasure)
        public OrderBook(Class<T> orderType) {
            this.orderType = orderType;
        }

        public void addOrder(T order) {
            orders.add(order);
            System.out.println("Added " + orderType.getSimpleName() + ": " + order.getOrderSummary());
        }

        public List<T> getOrders() {
            return new ArrayList<>(orders);  // Defensive copy
        }

        // Generic method with wildcards
        public void mergeOrderBook(OrderBook<? extends T> otherBook) {
            orders.addAll(otherBook.getOrders());
        }

        // Upper bounded wildcard (? extends T)
        public void copyFrom(List<? extends T> sourceOrders) {
            orders.addAll(sourceOrders);
        }

        // Lower bounded wildcard (? super T)
        public void copyTo(List<? super T> targetOrders) {
            targetOrders.addAll(orders);
        }
    }

    // Generic method examples
    public static <T> void swap(T[] array, int i, int j) {
        T temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }

    public static <T extends Comparable<T>> T findMax(List<T> list) {
        if (list.isEmpty()) return null;

        T max = list.get(0);
        for (T item : list) {
            if (item.compareTo(max) > 0) {
                max = item;
            }
        }
        return max;
    }

    // =============================================
    // 4. FUNCTIONAL INTERFACES AND LAMBDAS
    // =============================================

    // Functional interface (can be implemented with lambda)
    @FunctionalInterface
    interface PriceFilter {
        boolean test(BigDecimal price);

        // Default methods are allowed
        default PriceFilter and(PriceFilter other) {
            return price -> test(price) && other.test(price);
        }

        default PriceFilter or(PriceFilter other) {
            return price -> test(price) || other.test(price);
        }
    }

    static class TradingFilters {
        // Method that accepts functional interface
        public static List<BigDecimal> filterPrices(List<BigDecimal> prices, PriceFilter filter) {
            List<BigDecimal> filtered = new ArrayList<>();
            for (BigDecimal price : prices) {
                if (filter.test(price)) {
                    filtered.add(price);
                }
            }
            return filtered;
        }

        // Using built-in functional interfaces
        public static void demonstrateLambdas() {
            List<BigDecimal> prices = Arrays.asList(
                new BigDecimal("150.50"),
                new BigDecimal("175.25"),
                new BigDecimal("125.75"),
                new BigDecimal("200.00")
            );

            // Lambda expressions
            PriceFilter expensive = price -> price.compareTo(new BigDecimal("160")) > 0;
            PriceFilter cheap = price -> price.compareTo(new BigDecimal("140")) < 0;

            System.out.println("Expensive prices: " + filterPrices(prices, expensive));
            System.out.println("Cheap prices: " + filterPrices(prices, cheap));

            // Combining filters
            PriceFilter extremes = expensive.or(cheap);
            System.out.println("Extreme prices: " + filterPrices(prices, extremes));

            // Built-in functional interfaces
            Predicate<BigDecimal> highPrice = price -> price.compareTo(new BigDecimal("180")) > 0;
            Function<BigDecimal, String> priceFormatter = price -> "$" + price.toString();
            Consumer<String> printer = System.out::println;  // Method reference
            Supplier<BigDecimal> randomPrice = () -> new BigDecimal(Math.random() * 200);

            // Stream API with lambdas
            prices.stream()
                  .filter(highPrice)
                  .map(priceFormatter)
                  .forEach(printer);

            // Generate random prices
            System.out.println("Random price: " + priceFormatter.apply(randomPrice.get()));
        }
    }

    // =============================================
    // 5. ENUM - More Powerful than C++ enums
    // =============================================

    enum OrderStatus {
        PENDING("Order is pending execution"),
        PARTIALLY_FILLED("Order is partially filled"),
        FILLED("Order is completely filled"),
        CANCELLED("Order has been cancelled"),
        REJECTED("Order was rejected");

        private final String description;

        // Enum constructor
        OrderStatus(String description) {
            this.description = description;
        }

        public String getDescription() {
            return description;
        }

        // Enum methods
        public boolean isActive() {
            return this == PENDING || this == PARTIALLY_FILLED;
        }

        public boolean isFinal() {
            return this == FILLED || this == CANCELLED || this == REJECTED;
        }

        // Static method in enum
        public static OrderStatus fromString(String status) {
            for (OrderStatus s : values()) {
                if (s.name().equalsIgnoreCase(status)) {
                    return s;
                }
            }
            throw new IllegalArgumentException("Unknown status: " + status);
        }
    }

    // Enum with abstract methods
    enum TradingAction {
        BUY {
            @Override
            public BigDecimal calculateImpact(BigDecimal price, int quantity) {
                return price.multiply(new BigDecimal(quantity));
            }
        },
        SELL {
            @Override
            public BigDecimal calculateImpact(BigDecimal price, int quantity) {
                return price.multiply(new BigDecimal(-quantity));
            }
        };

        public abstract BigDecimal calculateImpact(BigDecimal price, int quantity);
    }

    // =============================================
    // 6. INNER CLASSES - Static vs Non-static
    // =============================================

    static class Portfolio {
        private final String accountId;
        private final Map<String, Position> positions = new HashMap<>();

        public Portfolio(String accountId) {
            this.accountId = accountId;
        }

        // Static nested class - doesn't need reference to outer class
        public static class Position {
            private final String symbol;
            private int quantity;
            private BigDecimal averagePrice;

            public Position(String symbol, int quantity, BigDecimal averagePrice) {
                this.symbol = symbol;
                this.quantity = quantity;
                this.averagePrice = averagePrice;
            }

            public BigDecimal getMarketValue(BigDecimal currentPrice) {
                return currentPrice.multiply(new BigDecimal(quantity));
            }

            // Getters
            public String getSymbol() { return symbol; }
            public int getQuantity() { return quantity; }
            public BigDecimal getAveragePrice() { return averagePrice; }
        }

        // Non-static inner class - has access to outer class members
        public class PositionIterator implements Iterator<Position> {
            private final Iterator<Position> iterator = positions.values().iterator();

            @Override
            public boolean hasNext() {
                return iterator.hasNext();
            }

            @Override
            public Position next() {
                Position position = iterator.next();
                // Can access outer class members
                System.out.println("Position in portfolio " + accountId + ": " + position.getSymbol());
                return position;
            }
        }

        public void addPosition(String symbol, int quantity, BigDecimal price) {
            positions.put(symbol, new Position(symbol, quantity, price));
        }

        public PositionIterator getPositionIterator() {
            return new PositionIterator();
        }

        // Local class inside method
        public void printPortfolioSummary() {
            class PortfolioSummary {
                private BigDecimal totalValue = BigDecimal.ZERO;
                private int totalShares = 0;

                void calculate() {
                    for (Position position : positions.values()) {
                        totalShares += position.getQuantity();
                        // Assuming current price equals average price for demo
                        totalValue = totalValue.add(position.getMarketValue(position.getAveragePrice()));
                    }
                }

                void print() {
                    System.out.println("Portfolio " + accountId + " Summary:");
                    System.out.println("Total value: $" + totalValue);
                    System.out.println("Total shares: " + totalShares);
                }
            }

            PortfolioSummary summary = new PortfolioSummary();
            summary.calculate();
            summary.print();
        }

        // Anonymous class example
        public Comparator<Position> createPositionComparator() {
            return new Comparator<Position>() {
                @Override
                public int compare(Position p1, Position p2) {
                    return p1.getSymbol().compareTo(p2.getSymbol());
                }
            };
        }

        // Lambda equivalent of anonymous class
        public Comparator<Position> createPositionComparatorLambda() {
            return (p1, p2) -> p1.getSymbol().compareTo(p2.getSymbol());
        }
    }

    // =============================================
    // 7. EXCEPTION HANDLING - Checked vs Unchecked
    // =============================================

    // Custom checked exception
    static class InsufficientFundsException extends Exception {
        private final BigDecimal availableBalance;
        private final BigDecimal requiredAmount;

        public InsufficientFundsException(BigDecimal availableBalance, BigDecimal requiredAmount) {
            super("Insufficient funds: available=$" + availableBalance + ", required=$" + requiredAmount);
            this.availableBalance = availableBalance;
            this.requiredAmount = requiredAmount;
        }

        public BigDecimal getAvailableBalance() { return availableBalance; }
        public BigDecimal getRequiredAmount() { return requiredAmount; }
    }

    // Custom unchecked exception
    static class InvalidSymbolException extends RuntimeException {
        public InvalidSymbolException(String symbol) {
            super("Invalid trading symbol: " + symbol);
        }
    }

    static class TradingService {
        private BigDecimal accountBalance = new BigDecimal("10000.00");

        // Method that throws checked exception - caller must handle
        public void executeTrade(String symbol, BigDecimal price, int quantity)
                throws InsufficientFundsException {

            // Validate symbol (unchecked exception)
            if (!TradingStrategy.isValidSymbol(symbol)) {
                throw new InvalidSymbolException(symbol);
            }

            BigDecimal tradeValue = price.multiply(new BigDecimal(quantity));

            // Check funds (checked exception)
            if (accountBalance.compareTo(tradeValue) < 0) {
                throw new InsufficientFundsException(accountBalance, tradeValue);
            }

            accountBalance = accountBalance.subtract(tradeValue);
            System.out.println("Trade executed: " + quantity + " shares of " + symbol + " at $" + price);
        }

        // Method demonstrating exception handling
        public void handleTradeExecution(String symbol, BigDecimal price, int quantity) {
            try {
                executeTrade(symbol, price, quantity);
            } catch (InsufficientFundsException e) {
                System.err.println("Trade failed: " + e.getMessage());
                System.err.println("Available: $" + e.getAvailableBalance());
                System.err.println("Required: $" + e.getRequiredAmount());
            } catch (InvalidSymbolException e) {
                System.err.println("Invalid symbol: " + e.getMessage());
            } catch (Exception e) {
                System.err.println("Unexpected error: " + e.getMessage());
                e.printStackTrace();
            } finally {
                System.out.println("Trade processing completed");
            }
        }

        // Try-with-resources for automatic resource management
        public void processTradeFile(String filename) {
            try (BufferedReader reader = new BufferedReader(new FileReader(filename))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    // Process trade data
                    System.out.println("Processing: " + line);
                }
            } catch (IOException e) {
                System.err.println("File processing error: " + e.getMessage());
            }
            // reader is automatically closed here
        }
    }

    // =============================================
    // DEMONSTRATION METHODS
    // =============================================

    public static void demonstrateOOP() {
        System.out.println("=== Object-Oriented Programming Examples ===");

        // Interface implementation
        MomentumStrategy strategy = new MomentumStrategy(new BigDecimal("100000"));
        BigDecimal signal = strategy.calculateSignal("AAPL", new BigDecimal("155.00"));
        System.out.println("Trading signal: " + signal);

        // Abstract class usage
        BaseOrder marketOrder = new MarketOrder("MKT001", "AAPL", new BigDecimal("155.00"), 100);
        BaseOrder limitOrder = new LimitOrder("LMT001", "GOOGL", new BigDecimal("2800.00"),
                                             50, new BigDecimal("2750.00"));

        marketOrder.processOrder();
        limitOrder.processOrder();

        // Generics
        OrderBook<MarketOrder> marketOrderBook = new OrderBook<>(MarketOrder.class);
        marketOrderBook.addOrder((MarketOrder) marketOrder);

        // Enum usage
        OrderStatus status = OrderStatus.PENDING;
        System.out.println("Order status: " + status.getDescription());
        System.out.println("Is active: " + status.isActive());
    }

    public static void demonstrateGenerics() {
        System.out.println("\n=== Generics Examples ===");

        // Generic collections
        List<String> symbols = Arrays.asList("AAPL", "GOOGL", "MSFT", "TSLA");
        String maxSymbol = findMax(symbols);
        System.out.println("Max symbol alphabetically: " + maxSymbol);

        // Wildcard usage
        List<BigDecimal> prices = Arrays.asList(
            new BigDecimal("150"), new BigDecimal("200"), new BigDecimal("175")
        );
        BigDecimal maxPrice = findMax(prices);
        System.out.println("Max price: $" + maxPrice);
    }

    public static void demonstrateInnerClasses() {
        System.out.println("\n=== Inner Classes Examples ===");

        Portfolio portfolio = new Portfolio("ACC123");
        portfolio.addPosition("AAPL", 100, new BigDecimal("150.00"));
        portfolio.addPosition("GOOGL", 50, new BigDecimal("2800.00"));

        // Using inner class
        Portfolio.PositionIterator iterator = portfolio.getPositionIterator();
        while (iterator.hasNext()) {
            Portfolio.Position position = iterator.next();
            System.out.println("Position: " + position.getSymbol() + " - " + position.getQuantity() + " shares");
        }

        portfolio.printPortfolioSummary();
    }

    public static void main(String[] args) {
        demonstrateOOP();
        demonstrateGenerics();
        TradingFilters.demonstrateLambdas();
        demonstrateInnerClasses();

        // Exception handling demo
        TradingService service = new TradingService();
        service.handleTradeExecution("AAPL", new BigDecimal("155.00"), 100);
        service.handleTradeExecution("INVALID", new BigDecimal("100.00"), 50);
        service.handleTradeExecution("GOOGL", new BigDecimal("5000.00"), 100);  // Insufficient funds

        System.out.println("\n=== Key OOP Differences from C++ ===");
        System.out.println("1. Interfaces provide multiple inheritance alternative");
        System.out.println("2. All methods are virtual by default (can be overridden)");
        System.out.println("3. Generics use type erasure (runtime type info lost)");
        System.out.println("4. Inner classes can access outer class private members");
        System.out.println("5. Enums are full classes with methods and constructors");
        System.out.println("6. Checked exceptions must be handled or declared");
        System.out.println("7. Lambda expressions provide functional programming features");
        System.out.println("8. No multiple inheritance - use composition with interfaces");
    }
}

/*
=== Key Learning Points for C++ Developers ===

1. Interface vs Abstract Class:
   - Interface: Contract only (methods + constants), multiple implementation
   - Abstract Class: Can have state and implementation, single inheritance

2. Generics vs Templates:
   - Type erasure at runtime (no specialization like C++)
   - Bounded type parameters (? extends, ? super)
   - Cannot create arrays of generic types

3. Exception Handling:
   - Checked exceptions must be caught or declared
   - Unchecked exceptions (RuntimeException) are optional
   - Try-with-resources for automatic cleanup

4. Inner Classes:
   - Static nested: No reference to outer instance
   - Non-static inner: Has reference to outer instance
   - Local classes: Defined inside methods
   - Anonymous classes: One-time use implementations

5. Functional Programming:
   - Lambda expressions and method references
   - Built-in functional interfaces (Predicate, Function, Consumer, Supplier)
   - Stream API for functional collection processing

6. Enum Features:
   - Can have constructors, methods, and fields
   - Can implement interfaces
   - Each enum constant is an instance
*/
