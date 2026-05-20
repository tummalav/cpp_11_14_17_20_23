/**
 * Java Streams, I/O, and Modern Features for C++ Developers
 *
 * This file covers Java 8+ Stream API, I/O operations, and modern Java features
 * with trading and financial examples.
 */

import java.util.*;
import java.util.stream.*;
import java.util.function.*;
import java.math.BigDecimal;
import java.math.RoundingMode;
import java.time.*;
import java.time.format.DateTimeFormatter;
import java.io.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class Java_Streams_IO_and_Modern_Features {

    // Sample data classes for examples
    static class Trade {
        private final String symbol;
        private final String tradeId;
        private final BigDecimal price;
        private final int quantity;
        private final LocalDateTime timestamp;
        private final TradeType type;

        public Trade(String symbol, String tradeId, BigDecimal price, int quantity, TradeType type) {
            this.symbol = symbol;
            this.tradeId = tradeId;
            this.price = price;
            this.quantity = quantity;
            this.type = type;
            this.timestamp = LocalDateTime.now();
        }

        // Getters
        public String getSymbol() { return symbol; }
        public String getTradeId() { return tradeId; }
        public BigDecimal getPrice() { return price; }
        public int getQuantity() { return quantity; }
        public LocalDateTime getTimestamp() { return timestamp; }
        public TradeType getType() { return type; }

        public BigDecimal getTradeValue() {
            return price.multiply(new BigDecimal(quantity));
        }

        @Override
        public String toString() {
            return String.format("Trade{%s, %s, %s, $%.2f, qty:%d, $%.2f}",
                tradeId, symbol, type, price, quantity, getTradeValue());
        }
    }

    enum TradeType { BUY, SELL }

    static class MarketData {
        private final String symbol;
        private final BigDecimal bid;
        private final BigDecimal ask;
        private final int volume;
        private final LocalDateTime timestamp;

        public MarketData(String symbol, BigDecimal bid, BigDecimal ask, int volume) {
            this.symbol = symbol;
            this.bid = bid;
            this.ask = ask;
            this.volume = volume;
            this.timestamp = LocalDateTime.now();
        }

        // Getters
        public String getSymbol() { return symbol; }
        public BigDecimal getBid() { return bid; }
        public BigDecimal getAsk() { return ask; }
        public int getVolume() { return volume; }
        public LocalDateTime getTimestamp() { return timestamp; }

        public BigDecimal getSpread() {
            return ask.subtract(bid);
        }

        public BigDecimal getMidPrice() {
            return bid.add(ask).divide(new BigDecimal("2"), 4, RoundingMode.HALF_UP);
        }

        @Override
        public String toString() {
            return String.format("MarketData{%s, bid:$%.2f, ask:$%.2f, vol:%d, spread:$%.4f}",
                symbol, bid, ask, volume, getSpread());
        }
    }

    // =============================================
    // 1. STREAM API - Functional Collection Processing
    // =============================================

    public static void demonstrateBasicStreams() {
        System.out.println("=== Basic Stream Operations ===");

        // Sample trade data
        List<Trade> trades = Arrays.asList(
            new Trade("AAPL", "T001", new BigDecimal("150.50"), 100, TradeType.BUY),
            new Trade("GOOGL", "T002", new BigDecimal("2800.00"), 50, TradeType.SELL),
            new Trade("AAPL", "T003", new BigDecimal("151.25"), 200, TradeType.BUY),
            new Trade("MSFT", "T004", new BigDecimal("300.75"), 150, TradeType.SELL),
            new Trade("AAPL", "T005", new BigDecimal("149.75"), 75, TradeType.SELL),
            new Trade("TSLA", "T006", new BigDecimal("800.25"), 25, TradeType.BUY)
        );

        // Filter: Find all AAPL trades
        System.out.println("AAPL Trades:");
        trades.stream()
              .filter(trade -> "AAPL".equals(trade.getSymbol()))
              .forEach(System.out::println);

        // Map: Convert trades to trade values
        System.out.println("\nTrade Values:");
        trades.stream()
              .map(Trade::getTradeValue)
              .forEach(value -> System.out.println("$" + value));

        // Reduce: Calculate total trade volume
        BigDecimal totalValue = trades.stream()
                                     .map(Trade::getTradeValue)
                                     .reduce(BigDecimal.ZERO, BigDecimal::add);
        System.out.println("\nTotal Trade Value: $" + totalValue);

        // Collect: Group trades by symbol
        Map<String, List<Trade>> tradesBySymbol = trades.stream()
                .collect(Collectors.groupingBy(Trade::getSymbol));

        System.out.println("\nTrades grouped by symbol:");
        tradesBySymbol.forEach((symbol, symbolTrades) -> {
            System.out.println(symbol + ": " + symbolTrades.size() + " trades");
        });
    }

    public static void demonstrateAdvancedStreams() {
        System.out.println("\n=== Advanced Stream Operations ===");

        // Generate sample market data
        List<MarketData> marketData = Arrays.asList(
            new MarketData("AAPL", new BigDecimal("150.45"), new BigDecimal("150.55"), 10000),
            new MarketData("GOOGL", new BigDecimal("2799.50"), new BigDecimal("2800.50"), 5000),
            new MarketData("MSFT", new BigDecimal("300.25"), new BigDecimal("300.35"), 8000),
            new MarketData("TSLA", new BigDecimal("799.75"), new BigDecimal("800.25"), 12000),
            new MarketData("AMZN", new BigDecimal("3200.10"), new BigDecimal("3201.90"), 3000)
        );

        // Find securities with tight spreads (< $0.20)
        System.out.println("Securities with tight spreads:");
        marketData.stream()
                  .filter(md -> md.getSpread().compareTo(new BigDecimal("0.20")) < 0)
                  .map(md -> md.getSymbol() + " (spread: $" + md.getSpread() + ")")
                  .forEach(System.out::println);

        // Calculate average volume
        double avgVolume = marketData.stream()
                                   .mapToInt(MarketData::getVolume)
                                   .average()
                                   .orElse(0.0);
        System.out.println("\nAverage Volume: " + String.format("%.0f", avgVolume));

        // Find top 3 most liquid securities
        System.out.println("\nTop 3 Most Liquid Securities:");
        marketData.stream()
                  .sorted(Comparator.comparing(MarketData::getVolume).reversed())
                  .limit(3)
                  .forEach(md -> System.out.println(md.getSymbol() + ": " + md.getVolume()));

        // Complex aggregation: Statistics by price range
        Map<String, List<MarketData>> byPriceRange = marketData.stream()
                .collect(Collectors.groupingBy(md -> {
                    BigDecimal midPrice = md.getMidPrice();
                    if (midPrice.compareTo(new BigDecimal("500")) < 0) return "Low";
                    else if (midPrice.compareTo(new BigDecimal("1000")) < 0) return "Medium";
                    else return "High";
                }));

        System.out.println("\nSecurities by price range:");
        byPriceRange.forEach((range, securities) -> {
            System.out.println(range + " price range: " + securities.size() + " securities");
            double avgSpread = securities.stream()
                                       .mapToDouble(md -> md.getSpread().doubleValue())
                                       .average()
                                       .orElse(0.0);
            System.out.println("  Average spread: $" + String.format("%.4f", avgSpread));
        });
    }

    public static void demonstrateStreamCreation() {
        System.out.println("\n=== Stream Creation Methods ===");

        // From arrays
        String[] symbols = {"AAPL", "GOOGL", "MSFT"};
        Arrays.stream(symbols)
              .forEach(symbol -> System.out.println("Symbol: " + symbol));

        // From ranges
        System.out.println("\nGenerating trade IDs:");
        IntStream.rangeClosed(1, 5)
                 .mapToObj(i -> "TRADE_" + String.format("%03d", i))
                 .forEach(System.out::println);

        // From generator function
        System.out.println("\nRandom prices:");
        Stream.generate(() -> new BigDecimal(100 + Math.random() * 100))
              .limit(5)
              .forEach(price -> System.out.println("$" + price.setScale(2, RoundingMode.HALF_UP)));

        // From iterate
        System.out.println("\nPrice sequence (starting at $100, increasing by $5):");
        Stream.iterate(new BigDecimal("100"), price -> price.add(new BigDecimal("5")))
              .limit(5)
              .forEach(price -> System.out.println("$" + price));

        // Parallel streams for performance
        System.out.println("\nParallel processing example:");
        List<Integer> numbers = IntStream.rangeClosed(1, 1000)
                                       .boxed()
                                       .collect(Collectors.toList());

        long startTime = System.currentTimeMillis();
        long sum = numbers.parallelStream()
                         .mapToLong(Integer::longValue)
                         .sum();
        long endTime = System.currentTimeMillis();

        System.out.println("Sum: " + sum + " (computed in " + (endTime - startTime) + "ms)");
    }

    // =============================================
    // 2. FILE I/O - Traditional and NIO.2
    // =============================================

    public static void demonstrateFileIO() {
        System.out.println("\n=== File I/O Examples ===");

        String filename = "trades.csv";
        String dataDir = "trading_data";

        try {
            // Create directory if it doesn't exist
            Path dirPath = Paths.get(dataDir);
            Files.createDirectories(dirPath);

            Path filePath = dirPath.resolve(filename);

            // Writing to file using NIO.2
            List<String> tradeData = Arrays.asList(
                "TradeId,Symbol,Price,Quantity,Type,Timestamp",
                "T001,AAPL,150.50,100,BUY," + LocalDateTime.now(),
                "T002,GOOGL,2800.00,50,SELL," + LocalDateTime.now(),
                "T003,MSFT,300.75,200,BUY," + LocalDateTime.now()
            );

            Files.write(filePath, tradeData, StandardCharsets.UTF_8);
            System.out.println("Written trade data to: " + filePath);

            // Reading from file
            System.out.println("\nReading trade data:");
            Files.lines(filePath)
                 .forEach(System.out::println);

            // Processing CSV data with streams
            System.out.println("\nProcessing trade data:");
            Files.lines(filePath)
                 .skip(1)  // Skip header
                 .map(line -> line.split(","))
                 .filter(parts -> parts.length >= 5)
                 .forEach(parts -> {
                     String symbol = parts[1];
                     String price = parts[2];
                     String quantity = parts[3];
                     System.out.println("Processed: " + symbol + " - " + quantity + " @ $" + price);
                 });

            // File operations
            System.out.println("\nFile info:");
            System.out.println("File size: " + Files.size(filePath) + " bytes");
            System.out.println("Last modified: " + Files.getLastModifiedTime(filePath));
            System.out.println("Is readable: " + Files.isReadable(filePath));
            System.out.println("Is writable: " + Files.isWritable(filePath));

        } catch (IOException e) {
            System.err.println("File I/O error: " + e.getMessage());
        }
    }

    public static void demonstrateAdvancedIO() {
        System.out.println("\n=== Advanced I/O Examples ===");

        try {
            // Watch directory for changes
            Path watchDir = Paths.get("trading_data");

            // Binary file I/O example
            String binaryFile = "trading_data/market_data.bin";
            Path binaryPath = Paths.get(binaryFile);

            // Write binary data
            try (DataOutputStream dos = new DataOutputStream(
                    Files.newOutputStream(binaryPath))) {

                // Write market data in binary format
                dos.writeUTF("AAPL");        // Symbol
                dos.writeDouble(150.50);     // Price
                dos.writeInt(10000);         // Volume
                dos.writeLong(System.currentTimeMillis()); // Timestamp

                dos.writeUTF("GOOGL");
                dos.writeDouble(2800.00);
                dos.writeInt(5000);
                dos.writeLong(System.currentTimeMillis());
            }

            System.out.println("Written binary market data");

            // Read binary data
            try (DataInputStream dis = new DataInputStream(
                    Files.newInputStream(binaryPath))) {

                System.out.println("Reading binary market data:");
                while (dis.available() > 0) {
                    String symbol = dis.readUTF();
                    double price = dis.readDouble();
                    int volume = dis.readInt();
                    long timestamp = dis.readLong();

                    System.out.println(String.format("%s: $%.2f, vol:%d, time:%d",
                        symbol, price, volume, timestamp));
                }
            }

            // Memory-mapped file for high-performance I/O
            demonstrateMemoryMappedFile();

        } catch (IOException e) {
            System.err.println("Advanced I/O error: " + e.getMessage());
        }
    }

    private static void demonstrateMemoryMappedFile() throws IOException {
        System.out.println("\nMemory-mapped file example:");

        Path mmapFile = Paths.get("trading_data/mmap_prices.dat");
        int fileSize = 1024 * 1024; // 1MB

        // Create and write to memory-mapped file
        try (var channel = (java.nio.channels.FileChannel) Files.newByteChannel(
                mmapFile,
                StandardOpenOption.CREATE,
                StandardOpenOption.READ,
                StandardOpenOption.WRITE)) {

            var buffer = channel.map(
                java.nio.channels.FileChannel.MapMode.READ_WRITE, 0, fileSize);

            // Write some price data
            for (int i = 0; i < 100; i++) {
                buffer.putDouble(100.0 + i * 0.5); // Price progression
            }

            // Read back the data
            buffer.position(0);
            System.out.println("First 5 prices from memory-mapped file:");
            for (int i = 0; i < 5; i++) {
                double price = buffer.getDouble();
                System.out.println("Price " + (i + 1) + ": $" + String.format("%.2f", price));
            }
        }
    }

    // =============================================
    // 3. DATE/TIME API - Modern java.time package
    // =============================================

    public static void demonstrateDateTimeAPI() {
        System.out.println("\n=== Modern Date/Time API ===");

        // Current time
        LocalDateTime now = LocalDateTime.now();
        LocalDate today = LocalDate.now();
        LocalTime currentTime = LocalTime.now();

        System.out.println("Current date/time: " + now);
        System.out.println("Today's date: " + today);
        System.out.println("Current time: " + currentTime);

        // Trading-specific date/time operations

        // Market open/close times
        LocalTime marketOpen = LocalTime.of(9, 30);  // 9:30 AM
        LocalTime marketClose = LocalTime.of(16, 0); // 4:00 PM

        System.out.println("\nMarket hours:");
        System.out.println("Market opens at: " + marketOpen);
        System.out.println("Market closes at: " + marketClose);
        System.out.println("Is market open now? " +
            (currentTime.isAfter(marketOpen) && currentTime.isBefore(marketClose)));

        // Calculate trading days
        LocalDate startDate = LocalDate.of(2024, 1, 1);
        LocalDate endDate = LocalDate.of(2024, 12, 31);

        long tradingDays = startDate.datesUntil(endDate.plusDays(1))
                                   .filter(date -> date.getDayOfWeek().getValue() <= 5) // Weekdays only
                                   .count();

        System.out.println("\nTrading days in 2024: " + tradingDays);

        // Time zone handling for global markets
        ZonedDateTime nyTime = ZonedDateTime.now(ZoneId.of("America/New_York"));
        ZonedDateTime londonTime = ZonedDateTime.now(ZoneId.of("Europe/London"));
        ZonedDateTime tokyoTime = ZonedDateTime.now(ZoneId.of("Asia/Tokyo"));

        DateTimeFormatter formatter = DateTimeFormatter.ofPattern("HH:mm:ss z");

        System.out.println("\nGlobal market times:");
        System.out.println("New York: " + nyTime.format(formatter));
        System.out.println("London: " + londonTime.format(formatter));
        System.out.println("Tokyo: " + tokyoTime.format(formatter));

        // Duration and Period calculations
        Duration tradingDay = Duration.between(marketOpen, marketClose);
        System.out.println("\nTrading day duration: " + tradingDay.toHours() + " hours " +
                          (tradingDay.toMinutes() % 60) + " minutes");

        // Calculate settlement date (T+2)
        LocalDate tradeDate = LocalDate.now();
        LocalDate settlementDate = tradeDate.plusDays(2);
        while (settlementDate.getDayOfWeek().getValue() > 5) {
            settlementDate = settlementDate.plusDays(1); // Skip weekends
        }

        System.out.println("Trade date: " + tradeDate);
        System.out.println("Settlement date (T+2): " + settlementDate);
    }

    // =============================================
    // 4. MODERN JAVA FEATURES (Java 8+)
    // =============================================

    public static void demonstrateModernJavaFeatures() {
        System.out.println("\n=== Modern Java Features ===");

        // Optional to avoid null pointer exceptions
        demonstrateOptional();

        // Method references
        List<String> symbols = Arrays.asList("AAPL", "googl", "MsFt", "tsla");

        System.out.println("\nOriginal symbols: " + symbols);

        // Method reference to static method
        List<String> upperCaseSymbols = symbols.stream()
                .map(String::toUpperCase)  // Method reference
                .collect(Collectors.toList());
        System.out.println("Upper case: " + upperCaseSymbols);

        // Method reference to instance method
        symbols.stream()
               .map(String::toLowerCase)
               .forEach(System.out::println);  // Method reference

        // Constructor reference
        symbols.stream()
               .map(StringBuilder::new)  // Constructor reference
               .forEach(sb -> System.out.println("StringBuilder: " + sb));
    }

    private static void demonstrateOptional() {
        System.out.println("\nOptional examples:");

        // Simulate finding a trade by ID
        Optional<Trade> trade = findTradeById("T001");

        // Safe handling with Optional
        trade.ifPresent(t -> System.out.println("Found trade: " + t));

        String tradeInfo = trade.map(Trade::toString)
                               .orElse("Trade not found");
        System.out.println("Trade info: " + tradeInfo);

        // Optional chaining
        Optional<BigDecimal> tradeValue = trade.map(Trade::getTradeValue);
        tradeValue.ifPresent(value -> System.out.println("Trade value: $" + value));

        // Optional with filter
        trade.filter(t -> t.getQuantity() > 50)
             .ifPresent(t -> System.out.println("Large trade: " + t));
    }

    private static Optional<Trade> findTradeById(String tradeId) {
        // Simulate database lookup
        if ("T001".equals(tradeId)) {
            return Optional.of(new Trade("AAPL", tradeId, new BigDecimal("150.50"), 100, TradeType.BUY));
        }
        return Optional.empty();
    }

    // =============================================
    // 5. PERFORMANCE CONSIDERATIONS
    // =============================================

    public static void demonstratePerformanceConsiderations() {
        System.out.println("\n=== Performance Considerations ===");

        List<Integer> largeList = IntStream.rangeClosed(1, 1_000_000)
                                         .boxed()
                                         .collect(Collectors.toList());

        // Sequential vs Parallel streams
        long startTime = System.currentTimeMillis();
        long sequentialSum = largeList.stream()
                                    .mapToLong(Integer::longValue)
                                    .sum();
        long sequentialTime = System.currentTimeMillis() - startTime;

        startTime = System.currentTimeMillis();
        long parallelSum = largeList.parallelStream()
                                  .mapToLong(Integer::longValue)
                                  .sum();
        long parallelTime = System.currentTimeMillis() - startTime;

        System.out.println("Sequential sum: " + sequentialSum + " (took " + sequentialTime + "ms)");
        System.out.println("Parallel sum: " + parallelSum + " (took " + parallelTime + "ms)");
        System.out.println("Parallel speedup: " + (double)sequentialTime / parallelTime + "x");

        // Memory efficiency with primitive streams
        System.out.println("\nPrimitive streams for better performance:");

        // Using IntStream instead of Stream<Integer>
        int sumPrimitive = IntStream.rangeClosed(1, 1000)
                                  .sum();
        System.out.println("Sum using IntStream: " + sumPrimitive);

        // Double stream for financial calculations
        double averagePrice = DoubleStream.of(150.50, 151.25, 149.75, 152.00)
                                        .average()
                                        .orElse(0.0);
        System.out.println("Average price: $" + String.format("%.2f", averagePrice));
    }

    // =============================================
    // MAIN DEMONSTRATION METHOD
    // =============================================

    public static void main(String[] args) {
        demonstrateBasicStreams();
        demonstrateAdvancedStreams();
        demonstrateStreamCreation();
        demonstrateFileIO();
        demonstrateAdvancedIO();
        demonstrateDateTimeAPI();
        demonstrateModernJavaFeatures();
        demonstratePerformanceConsiderations();

        System.out.println("\n=== Key Modern Java Features Summary ===");
        System.out.println("1. Stream API: Functional collection processing with lazy evaluation");
        System.out.println("2. Lambda expressions: Concise anonymous functions");
        System.out.println("3. Method references: Even more concise function references");
        System.out.println("4. Optional: Null-safe programming");
        System.out.println("5. New Date/Time API: Thread-safe, immutable temporal objects");
        System.out.println("6. NIO.2: Modern file I/O with Path API");
        System.out.println("7. Parallel streams: Easy parallelization for CPU-intensive tasks");
        System.out.println("8. Collectors: Powerful reduction operations");

        System.out.println("\n=== Performance Tips ===");
        System.out.println("1. Use primitive streams (IntStream, DoubleStream) for numbers");
        System.out.println("2. Consider parallel streams for CPU-intensive operations");
        System.out.println("3. Use method references for better readability");
        System.out.println("4. Avoid boxing/unboxing with primitive streams");
        System.out.println("5. Use Optional to eliminate null checks");
        System.out.println("6. Memory-mapped files for high-performance I/O");
        System.out.println("7. NIO.2 for modern file operations");
    }
}

/*
=== Trading Application Patterns with Modern Java ===

1. Market Data Processing:
   - Use parallel streams for real-time tick data processing
   - Stream API for filtering and aggregating market data
   - Memory-mapped files for historical data storage

2. Risk Management:
   - Optional for safe position lookups
   - Stream filtering for risk rule validation
   - Date/Time API for time-based risk calculations

3. Order Management:
   - Stream collectors for order book aggregation
   - File I/O for order audit trails
   - Functional interfaces for order validation rules

4. Portfolio Analytics:
   - Parallel streams for large portfolio calculations
   - Grouping collectors for sector/geography analysis
   - Time-based calculations with Duration/Period

5. Reporting:
   - Stream to CSV/file conversion
   - Collectors for statistical summaries
   - Formatting with DateTimeFormatter

Best Practices:
- Use streams for data transformation, not just iteration
- Prefer method references over lambdas when possible
- Use Optional consistently to avoid null checks
- Choose appropriate stream types (primitive vs object)
- Consider parallel processing for CPU-intensive tasks
- Use modern I/O APIs for better performance
*/
