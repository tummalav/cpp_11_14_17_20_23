import java.util.*;
import java.util.stream.*;
import java.util.function.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.time.*;
import java.time.format.*;
import java.nio.file.*;
import java.nio.charset.*;
import java.lang.invoke.*;
import java.util.regex.*;

/**
 * Java Evolution — All Versions from Java 1.0 to Java 23
 * ========================================================
 * Every major language feature, JVM improvement, and API addition
 * organized by version with working code examples.
 *
 * Versions covered:
 *   Java 1.0  (1996) — The beginning
 *   Java 1.1  (1997) — Inner classes, Reflection
 *   Java 1.2  (1998) — Collections Framework, Swing
 *   Java 1.3  (2000) — HotSpot JVM, JNDI
 *   Java 1.4  (2002) — assert, NIO, regex
 *   Java 5    (2004) — Generics, Autoboxing, Enums, Annotations, Varargs, for-each
 *   Java 6    (2006) — Scripting API, JDBC 4.0
 *   Java 7    (2011) — Diamond, try-with-resources, switch on String, NIO.2
 *   Java 8    (2014) — Lambdas, Streams, Optional, new Date/Time API (LTS)
 *   Java 9    (2017) — Modules (JPMS), JShell, private interface methods
 *   Java 10   (2018) — var (local type inference)
 *   Java 11   (2018) — String methods, HTTP client, single-file programs (LTS)
 *   Java 12   (2019) — Switch expressions (preview)
 *   Java 13   (2019) — Text blocks (preview)
 *   Java 14   (2020) — Records (preview), helpful NPE messages
 *   Java 15   (2020) — Sealed classes (preview), Text blocks final
 *   Java 16   (2021) — Records final, Pattern matching instanceof final
 *   Java 17   (2021) — Sealed classes final, strong encapsulation (LTS)
 *   Java 18   (2022) — UTF-8 by default, simple web server
 *   Java 19   (2022) — Virtual threads (preview), structured concurrency (preview)
 *   Java 20   (2023) — Scoped values (preview), record patterns (preview)
 *   Java 21   (2023) — Virtual threads final, record patterns, sequenced collections (LTS)
 *   Java 22   (2024) — Unnamed variables, stream gatherers (preview)
 *   Java 23   (2024) — Primitive patterns (preview), module imports
 *
 * Build: javac --enable-preview --release 21 Java_Evolution_All_Versions.java
 * Run:   java  --enable-preview Java_Evolution_All_Versions
 */
public class Java_Evolution_All_Versions {

    // =========================================================================
    // JAVA 1.0 — 1.4: Foundation
    // =========================================================================
    static class JavaFoundations {
        /*
         * Java 1.0 (January 1996):
         *   - "Write Once Run Anywhere" — JVM bytecode
         *   - Basic OOP: classes, interfaces, inheritance
         *   - Primitives: byte, short, int, long, float, double, boolean, char
         *   - Arrays, String, Object
         *   - Applets (browser), AWT (UI), java.io, java.net
         *   - Multithreading: Thread, Runnable, synchronized
         *   - Garbage collection (mark-and-sweep initially)
         *
         * Java 1.1 (February 1997):
         *   - Inner classes (static and non-static)
         *   - Anonymous classes (event handlers in AWT)
         *   - Reflection (java.lang.reflect)
         *   - JavaBeans specification
         *   - JDBC (database connectivity)
         *   - RMI (remote method invocation)
         *   - char/String → Unicode support
         *
         * Java 1.2 (December 1998) — "Java 2":
         *   - Collections Framework: List, Set, Map, Iterator
         *   - ArrayList, HashMap, HashSet, LinkedList, TreeMap
         *   - Swing (platform-independent GUI replacing AWT)
         *   - JIT (Just-In-Time) compiler introduced
         *   - strictfp keyword
         *
         * Java 1.3 (May 2000):
         *   - HotSpot JVM (Sun's production JVM)
         *   - JNDI (naming/directory service)
         *   - JavaSound API
         *   - Minor language: no new keywords
         *
         * Java 1.4 (February 2002):
         *   - assert keyword
         *   - NIO (Non-blocking I/O): java.nio, ByteBuffer, Channel, Selector
         *   - Regular expressions: java.util.regex
         *   - Logging API: java.util.logging
         *   - XML parsing (JAXP)
         *   - Chained exceptions: new Exception("msg", cause)
         */

        // Java 1.1 — Inner class, Anonymous class
        interface Comparator14<T> {
            int compare(T a, T b);
        }

        static class StringSorter {
            void sort(String[] arr) {
                // Anonymous class — Java 1.1 style (replaced by lambdas in Java 8)
                java.util.Arrays.sort(arr, new java.util.Comparator<String>() {
                    @Override public int compare(String a, String b) { return a.compareTo(b); }
                });
            }
        }

        // Java 1.2 — Collections framework
        static void collectionsDemo() {
            List<String> list = new ArrayList<>(Arrays.asList("QQQ", "SPY", "AAPL"));
            Map<String, Double> prices = new HashMap<>();
            prices.put("SPY", 450.0); prices.put("QQQ", 380.0);
            Set<String> symbols = new HashSet<>(list);

            Collections.sort(list);
            System.out.println("  Java 1.2 Collections: sorted=" + list + " prices=" + prices);
        }

        // Java 1.4 — assert, regex, NIO ByteBuffer
        @SuppressWarnings("AssertWithSideEffects")
        static void java14Features() {
            // assert (requires -ea JVM flag to enable)
            assert true : "assertion failed";

            // Regex — java.util.regex
            Pattern p = Pattern.compile("\\d{4}-\\d{2}-\\d{2}");
            Matcher m = p.matcher("Trade date: 2026-05-31");
            if (m.find()) System.out.println("  Java 1.4 regex match: " + m.group());

            // Chained exceptions
            try {
                try { throw new IllegalArgumentException("bad price"); }
                catch (Exception e) { throw new RuntimeException("order failed", e); }
            } catch (RuntimeException e) {
                System.out.println("  Java 1.4 chained exception cause: " + e.getCause().getMessage());
            }
        }

        public void run() {
            System.out.println("  Java 1.0: Classes, interfaces, threads, GC, JVM bytecode");
            System.out.println("  Java 1.1: Inner/anonymous classes, Reflection, JDBC");
            collectionsDemo();
            java14Features();
        }
    }

    // =========================================================================
    // JAVA 5 (2004) — Tiger: The big language overhaul
    // =========================================================================
    static class Java5Features {
        /*
         * Java 5 was the most impactful language release before Java 8.
         * Introduced: Generics, Autoboxing, Enums, Annotations, Varargs,
         *             Enhanced for-loop, Static imports, Concurrent utilities
         */

        // 1. GENERICS — type-safe collections, no raw types
        static <T extends Comparable<T>> T max(T a, T b) {
            return a.compareTo(b) >= 0 ? a : b;
        }

        static class Pair<A, B> {
            final A first; final B second;
            Pair(A first, B second) { this.first = first; this.second = second; }
            @Override public String toString() { return "(" + first + "," + second + ")"; }
        }

        // 2. AUTOBOXING — int ↔ Integer automatic conversion
        static void autoboxingDemo() {
            List<Integer> ints = new ArrayList<>();
            ints.add(42);       // autobox int → Integer
            int val = ints.get(0);  // unbox Integer → int
            System.out.println("  Autobox: list=" + ints + " unboxed=" + val);

            // WARNING: Integer cache only covers -128 to 127
            Integer a = 127, b = 127;
            Integer c = 128, d = 128;
            System.out.println("  Integer cache: 127==" + (a == b) + " 128==" + (c == d) + " (use .equals!)");
        }

        // 3. ENUMS — type-safe constants with methods
        enum Side { BUY, SELL;
            public Side opposite() { return this == BUY ? SELL : BUY; }
        }

        enum OrderType {
            MARKET("MKT"), LIMIT("LMT"), STOP("STP");
            private final String code;
            OrderType(String code) { this.code = code; }
            public String code() { return code; }
        }

        // 4. ANNOTATIONS — metadata on classes/methods/fields
        @java.lang.annotation.Retention(java.lang.annotation.RetentionPolicy.RUNTIME)
        @interface TradingStrategy {
            String name();
            boolean enabled() default true;
            String[] exchanges() default {};
        }

        @TradingStrategy(name="ETF-MM", enabled=true, exchanges={"NYSE","NASDAQ"})
        static class ETFMarketMaker { }

        // 5. VARARGS — variable-argument methods
        static double sumPrices(String label, double... prices) {
            double sum = 0;
            for (double p : prices) sum += p;
            System.out.println("  Varargs " + label + ": count=" + prices.length + " sum=" + sum);
            return sum;
        }

        // 6. ENHANCED FOR-EACH
        static void forEachDemo() {
            int[] arr = {1, 2, 3, 4, 5};
            int sum = 0;
            for (int n : arr) sum += n;  // no index needed
            System.out.println("  Enhanced for-each sum=" + sum);
        }

        // 7. java.util.concurrent — java.util.concurrent package
        // AtomicInteger, ConcurrentHashMap, ExecutorService, Semaphore, CountDownLatch

        public void run() {
            System.out.println("  Generics max: " + max(42, 99) + " pair: " + new Pair<>("SPY", 450.0));
            autoboxingDemo();
            System.out.println("  Enum: " + Side.BUY + " opposite=" + Side.BUY.opposite()
                + " OrderType=" + OrderType.LIMIT.code());
            sumPrices("SPY+QQQ+AAPL", 450.0, 380.0, 175.0);
            forEachDemo();

            // Annotations — read at runtime via reflection
            TradingStrategy ann = ETFMarketMaker.class.getAnnotation(TradingStrategy.class);
            System.out.println("  Annotation: name=" + ann.name()
                + " enabled=" + ann.enabled()
                + " exchanges=" + Arrays.toString(ann.exchanges()));
        }
    }

    // =========================================================================
    // JAVA 7 (2011) — Dolphin: Developer productivity
    // =========================================================================
    static class Java7Features {
        /*
         * Java 7 key improvements:
         *   - Diamond operator <> (type inference for generics)
         *   - try-with-resources (AutoCloseable)
         *   - switch on String
         *   - Multi-catch (catch (A | B e))
         *   - Binary literals: 0b1010
         *   - Underscores in numeric literals: 1_000_000
         *   - NIO.2: Path, Files, WatchService
         *   - Fork/Join framework
         */

        // 1. DIAMOND OPERATOR — no need to repeat type on RHS
        static void diamondDemo() {
            // Java 5/6: Map<String, List<Integer>> m = new HashMap<String, List<Integer>>();
            Map<String, List<Integer>> m = new HashMap<>();  // <> infers types
            m.put("prices", Arrays.asList(100, 200, 300));
            System.out.println("  Diamond operator: " + m);
        }

        // 2. TRY-WITH-RESOURCES — AutoCloseable auto-closed
        static class MarketDataConnection implements AutoCloseable {
            final String host;
            MarketDataConnection(String host) {
                this.host = host;
                System.out.println("  Connected to " + host);
            }
            String fetchPrice() { return "450.25"; }
            @Override public void close() {
                System.out.println("  Disconnected from " + host);
            }
        }

        static void tryWithResourcesDemo() {
            try (MarketDataConnection conn = new MarketDataConnection("NYSE-MD")) {
                System.out.println("  Price: " + conn.fetchPrice());
            }  // close() called automatically — even if exception thrown
        }

        // 3. SWITCH ON STRING
        static double getTickSize(String symbol) {
            return switch (symbol) {
                case "SPY", "QQQ" -> 0.01;
                case "BTC"        -> 1.00;
                case "FX-EURUSD"  -> 0.00001;
                default           -> 0.01;
            };
        }

        // 4. MULTI-CATCH
        static void multiCatch(String input) {
            try {
                int val = Integer.parseInt(input);
                int[] arr = new int[val];
                arr[val] = 1;
            } catch (NumberFormatException | ArrayIndexOutOfBoundsException e) {
                System.out.println("  Multi-catch: " + e.getClass().getSimpleName());
            }
        }

        // 5. NUMERIC LITERALS
        static void numericLiterals() {
            int million  = 1_000_000;           // underscores for readability
            int binary   = 0b1010_1010;         // binary literal = 170
            long nano    = 1_000_000_000L;       // nanoseconds per second
            System.out.println("  Numeric literals: million=" + million
                + " binary=" + binary + " nano=" + nano);
        }

        public void run() {
            diamondDemo();
            tryWithResourcesDemo();
            System.out.println("  Switch on String SPY tick=" + getTickSize("SPY")
                + " BTC tick=" + getTickSize("BTC"));
            multiCatch("abc");
            numericLiterals();
        }
    }

    // =========================================================================
    // JAVA 8 (2014) — LTS: The functional revolution
    // =========================================================================
    static class Java8Features {
        /*
         * Java 8 — most transformative release since Java 5:
         *   - Lambda expressions
         *   - Functional interfaces (@FunctionalInterface)
         *   - Stream API (map/filter/reduce/collect)
         *   - Optional<T>
         *   - Default and static interface methods
         *   - Method references (Class::method)
         *   - New Date/Time API (java.time)
         *   - CompletableFuture
         *   - Parallel streams
         *   - Base64 encoding/decoding
         *   - Nashorn JavaScript engine
         *   - StringJoiner
         */

        // 1. LAMBDA EXPRESSIONS
        @FunctionalInterface
        interface PriceFilter { boolean accept(double price); }

        // 2. STREAM API
        static void streamDemo() {
            List<String> symbols = Arrays.asList("SPY","QQQ","AAPL","MSFT","GOOG","AMZN","META");
            Map<String, Double> prices = Map.of(
                "SPY",450.0,"QQQ",380.0,"AAPL",175.0,"MSFT",290.0,
                "GOOG",140.0,"AMZN",185.0,"META",490.0
            );

            // filter + map + collect
            List<String> expensive = symbols.stream()
                .filter(s -> prices.getOrDefault(s,0.0) > 200.0)
                .sorted()
                .collect(Collectors.toList());
            System.out.println("  Stream filter>200: " + expensive);

            // reduce
            double totalValue = symbols.stream()
                .mapToDouble(s -> prices.getOrDefault(s, 0.0))
                .sum();
            System.out.printf("  Stream sum: $%.2f%n", totalValue);

            // groupingBy
            Map<String, List<String>> grouped = symbols.stream()
                .collect(Collectors.groupingBy(s -> prices.getOrDefault(s,0.0) > 300 ? "HIGH" : "LOW"));
            System.out.println("  Stream groupingBy: " + grouped);

            // flatMap
            List<List<String>> nested = Arrays.asList(
                Arrays.asList("NYSE:SPY","NYSE:QQQ"),
                Arrays.asList("NASDAQ:AAPL","NASDAQ:MSFT")
            );
            List<String> flat = nested.stream().flatMap(Collection::stream).collect(Collectors.toList());
            System.out.println("  Stream flatMap: " + flat);
        }

        // 3. OPTIONAL
        static Optional<Double> getPrice(Map<String, Double> prices, String symbol) {
            return Optional.ofNullable(prices.get(symbol));
        }

        static void optionalDemo() {
            Map<String, Double> prices = new HashMap<>();
            prices.put("SPY", 450.0);

            // orElse, orElseGet, map, ifPresent
            double spy  = getPrice(prices, "SPY").orElse(0.0);
            double btc  = getPrice(prices, "BTC").orElseGet(() -> -1.0);
            String fmt  = getPrice(prices, "SPY").map(p -> String.format("$%.2f", p)).orElse("N/A");
            getPrice(prices, "SPY").ifPresent(p -> System.out.println("  Optional ifPresent: SPY=" + p));

            System.out.println("  Optional: spy=" + spy + " btc=" + btc + " formatted=" + fmt);
        }

        // 4. DEFAULT INTERFACE METHODS
        interface Tradeable {
            String symbol();
            default String displayName() { return "Instrument[" + symbol() + "]"; }
            static Tradeable of(String sym) { return () -> sym; }
        }

        // 5. METHOD REFERENCES
        static void methodRefDemo() {
            List<String> syms = Arrays.asList("QQQ","SPY","AAPL");

            // Static method ref
            syms.stream().map(String::toUpperCase).forEach(System.out::println);

            // Constructor ref
            List<StringBuilder> sbs = syms.stream()
                .map(StringBuilder::new)
                .collect(Collectors.toList());
            System.out.println("  Method refs StringBuilders: " + sbs.size());
        }

        // 6. DATE/TIME API
        static void dateTimeDemo() {
            LocalDate today     = LocalDate.now();
            LocalTime now       = LocalTime.now();
            LocalDateTime dt    = LocalDateTime.now();
            ZonedDateTime nyc   = ZonedDateTime.now(ZoneId.of("America/New_York"));
            ZonedDateTime hk    = ZonedDateTime.now(ZoneId.of("Asia/Hong_Kong"));

            Duration dur    = Duration.ofNanos(500);
            Period  period  = Period.ofDays(30);

            System.out.println("  DateTime: today=" + today + " NYC=" + nyc.toLocalTime().withNano(0)
                + " HK=" + hk.toLocalTime().withNano(0));

            // Market open check
            LocalTime nyseOpen  = LocalTime.of(9, 30);
            LocalTime nyseClose = LocalTime.of(16, 0);
            LocalTime marketTime = LocalTime.of(10, 0);
            boolean isOpen = !marketTime.isBefore(nyseOpen) && marketTime.isBefore(nyseClose);
            System.out.println("  Market open at 10:00: " + isOpen);
        }

        public void run() {
            // Lambda
            PriceFilter highPrice = price -> price > 300.0;
            System.out.println("  Lambda filter 450: " + highPrice.accept(450.0));

            streamDemo();
            optionalDemo();

            Tradeable t = Tradeable.of("SPY");
            System.out.println("  Default interface method: " + t.displayName());

            methodRefDemo();
            dateTimeDemo();
        }
    }

    // =========================================================================
    // JAVA 9 (2017): Modules, JShell, private interface methods
    // =========================================================================
    static class Java9Features {
        /*
         * Java 9 key features:
         *   - JPMS (Java Platform Module System) — Project Jigsaw
         *     module-info.java: module com.bank.trading { requires java.net.http; exports com.bank.trading.api; }
         *   - JShell — interactive REPL (first Java REPL)
         *   - Private interface methods
         *   - Collection factory methods: List.of(), Set.of(), Map.of()
         *   - Stream API additions: takeWhile, dropWhile, ofNullable, iterate with predicate
         *   - Optional additions: ifPresentOrElse, stream(), or()
         *   - Process API improvements (ProcessHandle)
         *   - HTTP/2 client (incubator, finalized Java 11)
         *   - Reactive Streams: Flow.Publisher/Subscriber
         *   - Stack walking API
         */

        // 1. COLLECTION FACTORY METHODS — immutable
        static void collectionFactories() {
            List<String>        immutableList = List.of("SPY","QQQ","AAPL");
            Set<String>         immutableSet  = Set.of("BUY","SELL","CANCEL");
            Map<String, Double> immutableMap  = Map.of("SPY",450.0,"QQQ",380.0);
            // immutableList.add("MSFT"); // throws UnsupportedOperationException

            System.out.println("  Java 9 List.of: " + immutableList);
            System.out.println("  Java 9 Map.of:  " + immutableMap);

            // Map.copyOf, List.copyOf, Set.copyOf (defensive copy)
            List<String> mutable = new ArrayList<>(immutableList);
            mutable.add("MSFT");
            List<String> copy = List.copyOf(mutable);  // immutable copy
            System.out.println("  List.copyOf: " + copy);
        }

        // 2. STREAM ADDITIONS
        static void streamAdditions() {
            // takeWhile — take elements while predicate holds (stops at first false)
            List<Integer> prices = List.of(100, 150, 200, 250, 180, 300);
            List<Integer> rising = prices.stream()
                .takeWhile(p -> p < 260)
                .collect(Collectors.toList());
            System.out.println("  takeWhile <260: " + rising);

            // dropWhile — skip elements while predicate holds
            List<Integer> afterDrop = prices.stream()
                .dropWhile(p -> p < 200)
                .collect(Collectors.toList());
            System.out.println("  dropWhile <200: " + afterDrop);

            // Stream.iterate with predicate (like for-loop)
            List<Integer> fibs = Stream.iterate(new int[]{0,1}, f -> new int[]{f[1],f[0]+f[1]})
                .limit(8).map(f -> f[0]).collect(Collectors.toList());
            System.out.println("  Stream.iterate fibs: " + fibs);

            // Stream.ofNullable
            String nullSym = null;
            long count = Stream.ofNullable(nullSym).count();  // 0, not NPE
            System.out.println("  Stream.ofNullable(null) count=" + count);
        }

        // 3. OPTIONAL ADDITIONS
        static void optionalAdditions() {
            Optional<String> sym = Optional.of("SPY");
            Optional<String> empty = Optional.empty();

            // ifPresentOrElse
            sym.ifPresentOrElse(
                s -> System.out.println("  Optional ifPresentOrElse present: " + s),
                () -> System.out.println("  Optional empty")
            );

            // or() — supply alternative Optional
            Optional<String> result = empty.or(() -> Optional.of("DEFAULT"));
            System.out.println("  Optional.or(): " + result.get());

            // stream() — Optional → Stream (0 or 1 element)
            long cnt = sym.stream().count();
            System.out.println("  Optional.stream() count=" + cnt);
        }

        // 4. PRIVATE INTERFACE METHODS
        interface FeeCalculator {
            double grossAmount();
            default double netAmount() { return grossAmount() - calculateFee(); }
            private double calculateFee() { return grossAmount() * 0.001; } // private helper
        }

        public void run() {
            collectionFactories();
            streamAdditions();
            optionalAdditions();

            FeeCalculator calc = () -> 10000.0;
            System.out.println("  Private interface method: net=" + calc.netAmount());
        }
    }

    // =========================================================================
    // JAVA 10 (2018): var — local variable type inference
    // =========================================================================
    static class Java10Features {
        /*
         * Java 10 key feature: var (JEP 286)
         *   - Local variable type inference ONLY (not fields, not parameters)
         *   - Compiler infers type from initializer
         *   - var is NOT a keyword — it's a reserved type name
         *   - Cannot use var without initializer
         *   - Cannot use var with null initializer (type unknown)
         *   - Makes code more readable for complex generic types
         */

        static void varDemo() {
            // var infers type from RHS
            var prices    = new HashMap<String, Double>();    // HashMap<String,Double>
            var symbols   = List.of("SPY", "QQQ", "AAPL");  // List<String>
            var threshold = 300.0;                            // double

            prices.put("SPY", 450.0);
            prices.put("QQQ", 380.0);

            // var in for-each
            for (var entry : prices.entrySet()) {
                // entry is Map.Entry<String,Double>
                if (entry.getValue() > threshold) {
                    System.out.println("  var for-each: " + entry.getKey() + "=" + entry.getValue());
                }
            }

            // var with streams
            var expensiveSymbols = prices.entrySet().stream()
                .filter(e -> e.getValue() > 300)
                .map(Map.Entry::getKey)
                .collect(Collectors.toList());
            System.out.println("  var stream result: " + expensiveSymbols);

            // var in try-with-resources
            try (var conn = new Java7Features.MarketDataConnection("NASDAQ")) {
                var price = conn.fetchPrice();
                System.out.println("  var try-with-resources price: " + price);
            }
        }

        public void run() {
            varDemo();
            System.out.println("  var rules: only local vars, must have initializer, not null");
        }
    }

    // =========================================================================
    // JAVA 11 (2018) — LTS: String/File methods, HTTP client
    // =========================================================================
    static class Java11Features {
        /*
         * Java 11 key additions:
         *   - String methods: isBlank, lines, strip, repeat, stripLeading/Trailing
         *   - Files.readString/writeString
         *   - HTTP Client (java.net.http) — finalized from Java 9 incubator
         *   - var in lambda parameters: (var x, var y) -> x + y
         *   - Single-file program execution: java Hello.java (no javac needed)
         *   - Epsilon GC (no-op GC for testing)
         *   - ZGC (experimental)
         *   - Removed: Applets, CORBA, Java EE modules
         */

        static void stringMethods() {
            // isBlank — true if empty or only whitespace
            System.out.println("  isBlank: '  '.isBlank()=" + "  ".isBlank());
            System.out.println("  isBlank: 'SPY'.isBlank()=" + "SPY".isBlank());

            // lines() — stream of lines
            String multiline = "BUY SPY 100\nSELL QQQ 50\nBUY AAPL 200";
            long buyCount = multiline.lines()
                .filter(l -> l.startsWith("BUY"))
                .count();
            System.out.println("  lines() BUY count=" + buyCount);

            // strip() vs trim() — strip handles Unicode whitespace
            String padded = "  SPY  ";
            System.out.println("  strip: '" + padded.strip() + "'"
                + " stripLeading: '" + padded.stripLeading() + "'"
                + " stripTrailing: '" + padded.stripTrailing() + "'");

            // repeat()
            String sep = "-".repeat(40);
            System.out.println("  repeat: " + sep);

            // String.formatted() (similar to printf)
            String msg = "Symbol: %s Price: %.2f".formatted("SPY", 450.25);
            System.out.println("  formatted: " + msg);
        }

        static void filesMethods() throws Exception {
            // Files.writeString / readString
            Path tmp = Files.createTempFile("trade_", ".csv");
            Files.writeString(tmp, "SPY,450.0\nQQQ,380.0\n");
            String content = Files.readString(tmp);
            System.out.println("  Files.readString: " + content.lines().count() + " lines");
            Files.deleteIfExists(tmp);
        }

        public void run() throws Exception {
            stringMethods();
            filesMethods();
        }
    }

    // =========================================================================
    // JAVA 14—16: Records, Pattern Matching, Text Blocks, Switch Expressions
    // =========================================================================
    static class Java14to16Features {
        /*
         * Java 12-13: Switch expressions (preview) → final Java 14
         * Java 13:    Text blocks (preview) → final Java 15
         * Java 14:    Records (preview), helpful NPE messages
         * Java 15:    Sealed classes (preview)
         * Java 16:    Records (final), Pattern matching instanceof (final)
         */

        // 1. SWITCH EXPRESSIONS (final Java 14) — returns a value
        static String orderStatus(int code) {
            return switch (code) {
                case 0      -> "NEW";
                case 1      -> "PENDING";
                case 2      -> "FILLED";
                case 3      -> "CANCELLED";
                default     -> {
                    String msg = "UNKNOWN-" + code;
                    yield msg;  // yield: return value from block
                }
            };
        }

        // 2. TEXT BLOCKS (final Java 15) — multi-line strings
        static final String FIX_MESSAGE = """
                8=FIX.4.4
                35=D
                49=CLIENT1
                56=BROKER1
                11=ORD-001
                55=SPY
                54=1
                38=100
                """;

        static final String JSON_CONFIG = """
                {
                  "symbol": "SPY",
                  "maxPosition": 10000,
                  "spreadBps": 2.5
                }
                """;

        // 3. RECORDS (final Java 16) — immutable data classes
        // Compiler auto-generates: constructor, getters, equals, hashCode, toString
        record Order(String orderId, String symbol, double price, long qty, Side side) {
            // Compact constructor — validation
            Order {
                if (price <= 0) throw new IllegalArgumentException("Price must be positive");
                if (qty <= 0)   throw new IllegalArgumentException("Qty must be positive");
                orderId = orderId.toUpperCase();
            }

            // Custom method
            double notional() { return price * qty; }
        }

        enum Side { BUY, SELL }

        record Quote(String symbol, double bid, double ask) {
            double spread()  { return ask - bid; }
            double midpoint(){ return (bid + ask) / 2.0; }
        }

        // Records can implement interfaces
        interface Printable { void print(); }
        record TradeReport(String tradeId, double pnl) implements Printable {
            @Override public void print() {
                System.out.println("  TradeReport: " + tradeId + " PnL=" + pnl);
            }
        }

        // 4. PATTERN MATCHING instanceof (final Java 16)
        static void patternMatchingDemo() {
            Object obj = new Quote("SPY", 449.90, 450.10);

            // Old way: if (obj instanceof Quote) { Quote q = (Quote)obj; ... }
            if (obj instanceof Quote q) {
                System.out.println("  Pattern matching instanceof: spread=" + q.spread());
            }

            // Negation pattern
            if (!(obj instanceof Order o)) {
                System.out.println("  Pattern matching negation: not an Order");
            }
        }

        public void run() {
            System.out.println("  Switch expression: code=2 → " + orderStatus(2));
            System.out.println("  Text block FIX lines=" + FIX_MESSAGE.lines().filter(l -> !l.isBlank()).count());
            System.out.println("  JSON config: " + JSON_CONFIG.trim().substring(0, 20) + "...");

            Order order = new Order("ord-001", "SPY", 450.0, 100L, Side.BUY);
            System.out.println("  Record: " + order);
            System.out.println("  Record notional: " + order.notional());

            Quote quote = new Quote("SPY", 449.90, 450.10);
            System.out.println("  Record Quote: spread=" + quote.spread() + " mid=" + quote.midpoint());

            new TradeReport("T-001", 1250.50).print();

            patternMatchingDemo();
        }
    }

    // =========================================================================
    // JAVA 17 (2021) — LTS: Sealed classes, strong encapsulation
    // =========================================================================
    static class Java17Features {
        /*
         * Java 17 (LTS) key features:
         *   - Sealed classes (final) — restrict which classes can extend
         *   - Pattern matching enhancements
         *   - Strong encapsulation of JDK internals (--illegal-access removed)
         *   - Pseudo-random number generators (PRNG) API
         *   - Context-specific deserialization filters
         *   - Foreign Function & Memory API (incubator)
         *   - Removed: Applet API, Security Manager
         */

        // SEALED CLASSES — closed class hierarchy
        // Only permitted subclasses can extend
        sealed interface Instrument permits Stock17, Future17, Option17 { }

        // Final — no further extension
        record Stock17(String ticker, String exchange) implements Instrument { }

        // Sealed — only its subclasses can extend
        sealed interface Future17 extends Instrument permits IndexFuture17, BondFuture17 { }
        record IndexFuture17(String underlying, String expiry) implements Future17 { }
        record BondFuture17(String issuer, String maturity) implements Future17 { }

        // Non-sealed — can be extended by anyone
        non-sealed interface Option17 extends Instrument { }

        // Pattern matching switch (preview in 17, final in 21)
        // Using if-instanceof chain for Java 17 compatibility
        static String describe(Instrument inst) {
            if (inst instanceof Stock17 s)        return "Stock: " + s.ticker() + "@" + s.exchange();
            if (inst instanceof IndexFuture17 f)  return "IndexFuture on " + f.underlying() + " exp=" + f.expiry();
            if (inst instanceof BondFuture17 b)   return "BondFuture " + b.issuer() + " matures=" + b.maturity();
            return "Unknown instrument";
        }

        public void run() {
            Instrument spy  = new Stock17("SPY", "NYSE");
            Instrument es   = new IndexFuture17("SP500", "2026-06-20");
            Instrument bond = new BondFuture17("UST", "2030-01-15");

            System.out.println("  Sealed classes:");
            System.out.println("    " + describe(spy));
            System.out.println("    " + describe(es));
            System.out.println("    " + describe(bond));
            System.out.println("  Sealed hierarchy: Instrument permits Stock, Future, Option");
            System.out.println("  Future permits: IndexFuture, BondFuture (only these two!)");
        }
    }

    // =========================================================================
    // JAVA 21 (2023) — LTS: Virtual threads, Pattern matching switch, Sequenced collections
    // =========================================================================
    static class Java21Features {
        /*
         * Java 21 (LTS) — major release:
         *   - Virtual threads (final) — Project Loom
         *   - Sequenced collections — SequencedCollection, SequencedMap
         *   - Pattern matching switch (final)
         *   - Record patterns (final)
         *   - String templates (preview — dropped in Java 23)
         *   - Unnamed classes and instance main (preview)
         *   - Structured concurrency (preview)
         *   - Scoped values (preview)
         *   - Key encapsulation mechanism API
         */

        // 1. PATTERN MATCHING SWITCH (final Java 21)
        sealed interface MarketEvent permits Trade21, Quote21, Status21 {}
        record Trade21(String symbol, double price, long qty) implements MarketEvent {}
        record Quote21(String symbol, double bid, double ask) implements MarketEvent {}
        record Status21(String symbol, String state) implements MarketEvent {}

        static void processEvent(MarketEvent event) {
            String result = switch (event) {
                case Trade21 t when t.qty() > 1000 -> "LARGE trade " + t.symbol() + " qty=" + t.qty();
                case Trade21 t                     -> "Trade " + t.symbol() + "@" + t.price();
                case Quote21 q                     -> "Quote " + q.symbol() + " " + q.bid() + "/" + q.ask();
                case Status21 s when "HALTED".equals(s.state()) -> "⚠ HALTED: " + s.symbol();
                case Status21 s                    -> "Status " + s.symbol() + "=" + s.state();
            };
            System.out.println("    " + result);
        }

        // 2. RECORD PATTERNS (final Java 21)
        record Position(String symbol, long qty, double avgPrice) {}
        record Portfolio(String name, Position[] positions) {}

        static double computePnl(Object obj, double currentPrice) {
            if (obj instanceof Position(var sym, var qty, var avg)) {
                return qty * (currentPrice - avg);
            }
            return 0.0;
        }

        // 3. SEQUENCED COLLECTIONS (Java 21) — unified first/last access
        static void sequencedCollections() {
            // SequencedCollection: getFirst(), getLast(), addFirst(), addLast(), reversed()
            SequencedCollection<String> deque = new ArrayDeque<>(List.of("order1","order2","order3"));
            System.out.println("  SequencedCollection first=" + deque.getFirst()
                + " last=" + deque.getLast());

            // SequencedMap: firstEntry, lastEntry, reversed()
            SequencedMap<String, Double> map = new LinkedHashMap<>();
            map.put("9:30", 450.0); map.put("10:00", 451.5); map.put("16:00", 449.0);
            System.out.println("  SequencedMap firstEntry=" + map.firstEntry()
                + " lastEntry=" + map.lastEntry());
            System.out.println("  SequencedMap reversed: " + new ArrayList<>(map.reversed().keySet()));
        }

        // 4. VIRTUAL THREADS (final Java 21)
        static void virtualThreadsDemo() throws InterruptedException {
            AtomicLong count = new AtomicLong(0);

            // Create 10,000 virtual threads — impossible with platform threads (1-2MB stack each)
            Thread[] vts = new Thread[10_000];
            for (int i = 0; i < vts.length; i++) {
                vts[i] = Thread.ofVirtual().start(() -> {
                    count.incrementAndGet();
                });
            }
            for (Thread t : vts) t.join();
            System.out.println("  Virtual threads: 10,000 threads → count=" + count.get());

            // Virtual thread executor
            try (var exec = Executors.newVirtualThreadPerTaskExecutor()) {
                var futures = new ArrayList<Future<Long>>();
                for (int i = 0; i < 100; i++) {
                    final long val = i;
                    futures.add(exec.submit(() -> val * val));
                }
                long sumSquares = 0;
                for (var f : futures) sumSquares += f.get();
                System.out.println("  Virtual thread executor sum of squares 0-99: " + sumSquares);
            } catch (ExecutionException e) {
                System.out.println("  Virtual thread executor error: " + e.getMessage());
            }
        }

        public void run() throws Exception {
            System.out.println("  Pattern matching switch (Java 21):");
            processEvent(new Trade21("SPY", 450.25, 5000));
            processEvent(new Trade21("QQQ", 380.10, 100));
            processEvent(new Quote21("AAPL", 174.90, 175.10));
            processEvent(new Status21("XYZ", "HALTED"));

            Position pos = new Position("SPY", 1000L, 440.0);
            System.out.println("  Record pattern PnL: " + computePnl(pos, 450.0));

            sequencedCollections();
            virtualThreadsDemo();
        }
    }

    // =========================================================================
    // JAVA 22-23 (2024): Unnamed variables, Stream gatherers
    // =========================================================================
    static class Java22to23Features {
        /*
         * Java 22 (March 2024):
         *   - Unnamed variables & patterns: _ (underscore)
         *   - Stream gatherers (preview)
         *   - Structured concurrency (preview → stabilizing)
         *   - Scoped values (preview → stabilizing)
         *   - Foreign function & memory API (final)
         *   - Class-file API (preview)
         *   - Launch multi-file programs: java *.java
         *
         * Java 23 (September 2024):
         *   - Primitive types in patterns (preview)
         *   - Module import declarations
         *   - Markdown Javadoc comments (///)
         *   - ZGC: Generational mode by default
         *   - Deprecate string templates (withdrawn!)
         */

        // 1. UNNAMED VARIABLES (Java 22) — _ to discard
        static void unnamedVariables() {
            // Discard exception variable
            try {
                Integer.parseInt("not-a-number");
            } catch (NumberFormatException _) {  // _ = don't care about the exception object
                System.out.println("  Unnamed variable: caught parse error (exception discarded)");
            }

            // Discard loop variable
            int count = 0;
            for (var _ : List.of("SPY","QQQ","AAPL","MSFT")) {
                count++;  // just count, don't use the element
            }
            System.out.println("  Unnamed loop var: count=" + count);

            // Discard in pattern matching
            Object obj = new Java21Features.Trade21("SPY", 450.0, 100);
            if (obj instanceof Java21Features.Trade21(var sym, _, _)) {
                System.out.println("  Unnamed record pattern: symbol=" + sym + " (price,qty discarded)");
            }
        }

        public void run() {
            unnamedVariables();
            System.out.println("  Java 22: Unnamed vars(_), Stream gatherers(preview), FFM API(final)");
            System.out.println("  Java 23: Primitive patterns(preview), Module imports, ZGC generational default");
        }
    }

    // =========================================================================
    // VERSION TIMELINE: JVM Evolution
    // =========================================================================
    static class JVMEvolution {
        public void run() {
            System.out.println("\n  Java Version Timeline:");
            System.out.println("  ┌────────┬──────┬─────┬─────────────────────────────────────────────────────┐");
            System.out.println("  │ Version│ Year │ LTS │ Key Features                                         │");
            System.out.println("  ├────────┼──────┼─────┼─────────────────────────────────────────────────────┤");
            System.out.println("  │ 1.0    │ 1996 │     │ OOP, JVM, Threads, GC, AWT                          │");
            System.out.println("  │ 1.1    │ 1997 │     │ Inner classes, Reflection, JDBC, RMI                 │");
            System.out.println("  │ 1.2    │ 1998 │     │ Collections Framework, Swing, JIT                    │");
            System.out.println("  │ 1.3    │ 2000 │     │ HotSpot JVM, JNDI, JavaSound                        │");
            System.out.println("  │ 1.4    │ 2002 │     │ assert, NIO, Regex, Logging, XML                     │");
            System.out.println("  │ 5      │ 2004 │     │ Generics, Autoboxing, Enums, Annotations, Varargs    │");
            System.out.println("  │ 6      │ 2006 │     │ Scripting API, JDBC 4.0, Compiler API                │");
            System.out.println("  │ 7      │ 2011 │     │ Diamond, try-resources, switch String, NIO.2         │");
            System.out.println("  │ 8      │ 2014 │ LTS │ Lambdas, Streams, Optional, Date/Time API            │");
            System.out.println("  │ 9      │ 2017 │     │ Modules (JPMS), JShell, List.of(), private iface     │");
            System.out.println("  │ 10     │ 2018 │     │ var (local type inference)                           │");
            System.out.println("  │ 11     │ 2018 │ LTS │ String.isBlank/lines/strip, HTTP Client, ZGC(exp)    │");
            System.out.println("  │ 12     │ 2019 │     │ Switch expressions (preview), Shenandoah GC          │");
            System.out.println("  │ 13     │ 2019 │     │ Text blocks (preview)                                │");
            System.out.println("  │ 14     │ 2020 │     │ Records (preview), Switch expr (final), NPE help     │");
            System.out.println("  │ 15     │ 2020 │     │ Text blocks (final), Sealed classes (preview)        │");
            System.out.println("  │ 16     │ 2021 │     │ Records (final), instanceof pattern (final)          │");
            System.out.println("  │ 17     │ 2021 │ LTS │ Sealed classes (final), strong encapsulation         │");
            System.out.println("  │ 18     │ 2022 │     │ UTF-8 default, simple web server, @snippet Javadoc   │");
            System.out.println("  │ 19     │ 2022 │     │ Virtual threads (preview), Structured concurrency    │");
            System.out.println("  │ 20     │ 2023 │     │ Scoped values (preview), Record patterns (preview)   │");
            System.out.println("  │ 21     │ 2023 │ LTS │ Virtual threads(final), Pattern switch, Sequenced col│");
            System.out.println("  │ 22     │ 2024 │     │ Unnamed vars(_), FFM API(final), Stream gatherers    │");
            System.out.println("  │ 23     │ 2024 │     │ Primitive patterns(preview), Module imports          │");
            System.out.println("  │ 24     │ 2025 │     │ Ahead-of-time compilation (AOT, JEP 483)             │");
            System.out.println("  │ 25     │ 2025 │ LTS │ Next LTS (upcoming)                                  │");
            System.out.println("  └────────┴──────┴─────┴─────────────────────────────────────────────────────┘");

            System.out.println("\n  Release cadence: 6-month releases since Java 9");
            System.out.println("  LTS support: Oracle 8yr+, others 3yr min. LTS versions: 8,11,17,21,25");
            System.out.println("\n  GC Evolution:");
            System.out.println("  Serial GC (1.0) → Parallel GC (1.4) → CMS (1.4.1,deprecated 14)");
            System.out.println("  → G1 GC default (Java 9) → ZGC (Java 11,stable 15) → Shenandoah (Java 12)");
            System.out.println("  → ZGC generational default (Java 23)");
        }
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {
        System.out.println("==========================================================================");
        System.out.println(" Java Evolution — All Versions from 1.0 to 23");
        System.out.println("==========================================================================\n");

        System.out.println("--- Java 1.0–1.4: Foundations ---");
        new JavaFoundations().run();

        System.out.println("\n--- Java 5 (2004): Generics, Autoboxing, Enums, Annotations, Varargs ---");
        new Java5Features().run();

        System.out.println("\n--- Java 7 (2011): Diamond, try-resources, switch String ---");
        new Java7Features().run();

        System.out.println("\n--- Java 8 (2014) LTS: Lambdas, Streams, Optional, Date/Time ---");
        new Java8Features().run();

        System.out.println("\n--- Java 9 (2017): Modules, List.of, takeWhile, Optional additions ---");
        new Java9Features().run();

        System.out.println("\n--- Java 10 (2018): var ---");
        new Java10Features().run();

        System.out.println("\n--- Java 11 (2018) LTS: String methods, Files.readString ---");
        new Java11Features().run();

        System.out.println("\n--- Java 14–16: Records, Pattern matching, Switch expr, Text blocks ---");
        new Java14to16Features().run();

        System.out.println("\n--- Java 17 (2021) LTS: Sealed classes ---");
        new Java17Features().run();

        System.out.println("\n--- Java 21 (2023) LTS: Virtual threads, Pattern switch, Sequenced ---");
        new Java21Features().run();

        System.out.println("\n--- Java 22–23 (2024): Unnamed variables ---");
        new Java22to23Features().run();

        System.out.println("\n--- JVM Evolution Timeline ---");
        new JVMEvolution().run();

        System.out.println("\n==========================================================================");
        System.out.println(" All Java version demos completed successfully");
        System.out.println("==========================================================================");
    }
}

