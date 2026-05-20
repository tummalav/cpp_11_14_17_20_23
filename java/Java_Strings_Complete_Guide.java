/**
 * Java Strings - Complete Guide with C++ Comparison
 *
 * Topics Covered:
 * 1. String (Immutable)
 * 2. StringBuilder (Mutable, NOT thread-safe)
 * 3. StringBuffer (Mutable, thread-safe)
 * 4. String creation methods
 * 5. String pool and interning
 * 6. String operations and performance
 * 7. Comparison with C++ std::string
 * 8. Trading system examples
 *
 * Author: Interview Preparation Guide
 * Date: February 8, 2026
 */

public class Java_Strings_Complete_Guide {

    //=============================================================================
    // 1. STRING (IMMUTABLE)
    //=============================================================================

    /**
     * Java String is IMMUTABLE
     * - Once created, cannot be changed
     * - Any modification creates a NEW string object
     * - Thread-safe by design (can be shared)
     * - Stored in String Pool (for literals)
     */
    static void demonstrateStringImmutability() {
        System.out.println("╔════════════════════════════════════════════════════════╗");
        System.out.println("║  1. STRING (IMMUTABLE)                                 ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        // String creation
        String s1 = "Hello";                    // String literal (pool)
        String s2 = "Hello";                    // Same object from pool
        String s3 = new String("Hello");        // New object (heap)

        System.out.println("s1 == s2: " + (s1 == s2));           // true (same object)
        System.out.println("s1 == s3: " + (s1 == s3));           // false (different objects)
        System.out.println("s1.equals(s3): " + s1.equals(s3));   // true (same content)

        // Immutability demonstration
        String original = "Trading";
        String modified = original.concat(" System");  // Creates NEW string

        System.out.println("\nOriginal: " + original);    // "Trading" (unchanged!)
        System.out.println("Modified: " + modified);      // "Trading System"

        // Common operations (all create new strings)
        String upper = original.toUpperCase();     // New string
        String lower = original.toLowerCase();     // New string
        String sub = original.substring(0, 5);     // New string
        String replaced = original.replace('T', 't'); // New string

        System.out.println("\nAll operations create NEW objects:");
        System.out.println("toUpperCase(): " + upper);
        System.out.println("toLowerCase(): " + lower);
        System.out.println("substring(): " + sub);
        System.out.println("replace(): " + replaced);

        // ⚠️ Performance issue with concatenation in loops
        long start = System.nanoTime();
        String result = "";
        for (int i = 0; i < 1000; i++) {
            result += "x";  // ⚠️ BAD! Creates 1000 new strings!
        }
        long immutableTime = System.nanoTime() - start;
        System.out.println("\n⚠️ Loop concatenation time: " + immutableTime + " ns");
        System.out.println("   (Creates many intermediate objects → GC pressure)");
    }

    //=============================================================================
    // 2. STRINGBUILDER (MUTABLE, NOT THREAD-SAFE)
    //=============================================================================

    /**
     * StringBuilder is MUTABLE
     * - Can be modified in-place
     * - NOT thread-safe (faster than StringBuffer)
     * - Use for single-threaded string building
     * - Best for loops and concatenation
     */
    static void demonstrateStringBuilder() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  2. STRINGBUILDER (MUTABLE, NOT THREAD-SAFE)           ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        // Creation
        StringBuilder sb = new StringBuilder();              // Empty, capacity 16
        StringBuilder sb2 = new StringBuilder(100);          // Empty, capacity 100
        StringBuilder sb3 = new StringBuilder("Initial");    // With initial string

        System.out.println("Initial capacity: " + sb.capacity());
        System.out.println("Initial length: " + sb.length());

        // Append operations (modify in-place)
        sb.append("Order");
        sb.append(" ");
        sb.append("ID");
        sb.append(": ");
        sb.append(12345);

        System.out.println("\nAfter appends: " + sb.toString());
        System.out.println("Capacity: " + sb.capacity());
        System.out.println("Length: " + sb.length());

        // Other operations
        sb.insert(6, "Book ");          // Insert at position
        System.out.println("After insert: " + sb);

        sb.delete(6, 11);               // Delete range
        System.out.println("After delete: " + sb);

        sb.reverse();                   // Reverse in-place
        System.out.println("After reverse: " + sb);
        sb.reverse();                   // Reverse back

        sb.replace(0, 5, "Trade");      // Replace range
        System.out.println("After replace: " + sb);

        // Performance comparison
        long start = System.nanoTime();
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < 1000; i++) {
            builder.append("x");  // ✅ GOOD! Modifies in-place
        }
        long builderTime = System.nanoTime() - start;

        System.out.println("\n✅ StringBuilder loop time: " + builderTime + " ns");
        System.out.println("   (Much faster - no intermediate objects)");

        // Method chaining
        StringBuilder chain = new StringBuilder()
            .append("Symbol: ")
            .append("AAPL")
            .append(", Price: ")
            .append(150.50)
            .append(", Qty: ")
            .append(100);

        System.out.println("\nMethod chaining: " + chain);
    }

    //=============================================================================
    // 3. STRINGBUFFER (MUTABLE, THREAD-SAFE)
    //=============================================================================

    /**
     * StringBuffer is MUTABLE and THREAD-SAFE
     * - Can be modified in-place
     * - Thread-safe (synchronized methods)
     * - Slower than StringBuilder (synchronization overhead)
     * - Use for multi-threaded string building
     */
    static void demonstrateStringBuffer() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  3. STRINGBUFFER (MUTABLE, THREAD-SAFE)                ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        // Creation (same as StringBuilder)
        StringBuffer buffer = new StringBuffer();
        StringBuffer buffer2 = new StringBuffer(100);
        StringBuffer buffer3 = new StringBuffer("Initial");

        // Operations (same API as StringBuilder)
        buffer.append("Thread-safe ");
        buffer.append("string ");
        buffer.append("building");

        System.out.println("Result: " + buffer);

        // Thread-safety demonstration
        StringBuffer shared = new StringBuffer();

        Thread t1 = new Thread(() -> {
            for (int i = 0; i < 100; i++) {
                shared.append("A");  // ✅ Thread-safe
            }
        });

        Thread t2 = new Thread(() -> {
            for (int i = 0; i < 100; i++) {
                shared.append("B");  // ✅ Thread-safe
            }
        });

        t1.start();
        t2.start();

        try {
            t1.join();
            t2.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        System.out.println("\nMulti-threaded result length: " + shared.length());
        System.out.println("(Should be 200 - thread-safe guarantee)");

        // Performance comparison with StringBuilder
        long start = System.nanoTime();
        StringBuffer buf = new StringBuffer();
        for (int i = 0; i < 1000; i++) {
            buf.append("x");
        }
        long bufferTime = System.nanoTime() - start;

        start = System.nanoTime();
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < 1000; i++) {
            builder.append("x");
        }
        long builderTime = System.nanoTime() - start;

        System.out.println("\nPerformance:");
        System.out.println("StringBuffer:  " + bufferTime + " ns (synchronized)");
        System.out.println("StringBuilder: " + builderTime + " ns (not synchronized)");
        System.out.println("Overhead:      " + (bufferTime - builderTime) + " ns");
    }

    //=============================================================================
    // 4. STRING POOL AND INTERNING
    //=============================================================================

    static void demonstrateStringPool() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  4. STRING POOL AND INTERNING                          ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        // String literals go to pool
        String s1 = "AAPL";
        String s2 = "AAPL";
        System.out.println("s1 == s2: " + (s1 == s2));  // true (same object from pool)

        // new String() creates heap object
        String s3 = new String("AAPL");
        System.out.println("s1 == s3: " + (s1 == s3));  // false (different objects)

        // intern() moves to pool
        String s4 = new String("AAPL").intern();
        System.out.println("s1 == s4: " + (s1 == s4));  // true (s4 now from pool)

        // Concatenation creates new object (not in pool)
        String s5 = "AA" + "PL";  // Compile-time constant (pool)
        System.out.println("s1 == s5: " + (s1 == s5));  // true

        String prefix = "AA";
        String s6 = prefix + "PL";  // Runtime concatenation (heap)
        System.out.println("s1 == s6: " + (s1 == s6));  // false

        String s7 = s6.intern();    // Move to pool
        System.out.println("s1 == s7: " + (s1 == s7));  // true

        System.out.println("\nKey Points:");
        System.out.println("• String literals → Pool (memory efficient)");
        System.out.println("• new String() → Heap (new object)");
        System.out.println("• intern() → Moves to pool (saves memory)");
    }

    //=============================================================================
    // 5. STRING OPERATIONS
    //=============================================================================

    static void demonstrateStringOperations() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  5. STRING OPERATIONS                                  ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        String symbol = "AAPL";
        String message = "  Order ID: 12345, Symbol: AAPL, Price: 150.50  ";

        // Length and character access
        System.out.println("Length: " + symbol.length());
        System.out.println("charAt(0): " + symbol.charAt(0));

        // Comparison
        System.out.println("\nComparison:");
        System.out.println("equals(): " + symbol.equals("AAPL"));
        System.out.println("equalsIgnoreCase(): " + symbol.equalsIgnoreCase("aapl"));
        System.out.println("compareTo(): " + symbol.compareTo("MSFT"));  // < 0
        System.out.println("startsWith(): " + symbol.startsWith("AA"));
        System.out.println("endsWith(): " + symbol.endsWith("PL"));

        // Searching
        System.out.println("\nSearching:");
        System.out.println("indexOf('P'): " + symbol.indexOf('P'));
        System.out.println("lastIndexOf('P'): " + symbol.lastIndexOf('P'));
        System.out.println("contains('AP'): " + symbol.contains("AP"));

        // Substring
        System.out.println("\nSubstring:");
        System.out.println("substring(0, 2): " + symbol.substring(0, 2));
        System.out.println("substring(2): " + symbol.substring(2));

        // Modification (creates new strings)
        System.out.println("\nModification:");
        System.out.println("toUpperCase(): " + symbol.toUpperCase());
        System.out.println("toLowerCase(): " + symbol.toLowerCase());
        System.out.println("trim(): '" + message.trim() + "'");
        System.out.println("replace('A', 'X'): " + symbol.replace('A', 'X'));

        // Splitting
        System.out.println("\nSplitting:");
        String data = "AAPL,150.50,100,BUY";
        String[] parts = data.split(",");
        for (int i = 0; i < parts.length; i++) {
            System.out.println("  [" + i + "]: " + parts[i]);
        }

        // Joining (Java 8+)
        String joined = String.join("|", "AAPL", "MSFT", "GOOGL");
        System.out.println("\nJoined: " + joined);

        // Formatting
        String formatted = String.format("Order: %s, Price: %.2f, Qty: %d",
                                        "AAPL", 150.5, 100);
        System.out.println("\nFormatted: " + formatted);
    }

    //=============================================================================
    // 6. PERFORMANCE COMPARISON
    //=============================================================================

    static void demonstratePerformance() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  6. PERFORMANCE COMPARISON                             ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        int iterations = 10000;

        // String concatenation (WORST)
        long start = System.nanoTime();
        String s = "";
        for (int i = 0; i < iterations; i++) {
            s += "x";  // ⚠️ Creates new object every iteration
        }
        long stringTime = System.nanoTime() - start;

        // StringBuilder (BEST for single thread)
        start = System.nanoTime();
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < iterations; i++) {
            sb.append("x");  // ✅ Modifies in-place
        }
        long builderTime = System.nanoTime() - start;

        // StringBuffer (BEST for multi-thread)
        start = System.nanoTime();
        StringBuffer buf = new StringBuffer();
        for (int i = 0; i < iterations; i++) {
            buf.append("x");  // ✅ Thread-safe
        }
        long bufferTime = System.nanoTime() - start;

        System.out.println("Performance for " + iterations + " concatenations:");
        System.out.println("┌────────────────┬──────────────┬──────────┐");
        System.out.println("│ Method         │ Time (μs)    │ Relative │");
        System.out.println("├────────────────┼──────────────┼──────────┤");
        System.out.printf("│ String +=      │ %,12d │ %6.1fx   │%n", stringTime / 1000,
                         stringTime / (double)builderTime);
        System.out.printf("│ StringBuilder  │ %,12d │ %6.1fx   │%n", builderTime / 1000, 1.0);
        System.out.printf("│ StringBuffer   │ %,12d │ %6.1fx   │%n", bufferTime / 1000,
                         bufferTime / (double)builderTime);
        System.out.println("└────────────────┴──────────────┴──────────┘");

        System.out.println("\nRecommendations:");
        System.out.println("✅ Use StringBuilder for single-threaded (FASTEST)");
        System.out.println("✅ Use StringBuffer for multi-threaded (THREAD-SAFE)");
        System.out.println("❌ Avoid String += in loops (SLOWEST)");
    }

    //=============================================================================
    // 7. COMPARISON WITH C++ std::string
    //=============================================================================

    static void compareWithCppString() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  7. JAVA STRING vs C++ std::string                     ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        System.out.println("┌─────────────────────┬─────────────────────┬─────────────────────┐");
        System.out.println("│ Feature             │ Java String         │ C++ std::string     │");
        System.out.println("├─────────────────────┼─────────────────────┼─────────────────────┤");
        System.out.println("│ Mutability          │ IMMUTABLE           │ MUTABLE             │");
        System.out.println("│ Thread-safe         │ YES (immutable)     │ NO                  │");
        System.out.println("│ Memory overhead     │ Higher (object)     │ Lower (value)       │");
        System.out.println("│ Concatenation       │ Creates new object  │ Modifies in-place   │");
        System.out.println("│ Character encoding  │ UTF-16 (16-bit)     │ Usually UTF-8       │");
        System.out.println("│ String pool         │ YES                 │ NO                  │");
        System.out.println("│ Null safety         │ Can be null         │ Cannot be null      │");
        System.out.println("│ Move semantics      │ NO (copies)         │ YES (C++11)         │");
        System.out.println("│ Small string opt    │ NO                  │ YES (SSO)           │");
        System.out.println("│ Reference counting  │ NO (GC)             │ NO (RAII)           │");
        System.out.println("└─────────────────────┴─────────────────────┴─────────────────────┘");

        System.out.println("\nCode Comparison:");
        System.out.println("\n// Java String (Immutable)");
        System.out.println("String s1 = \"Hello\";");
        System.out.println("String s2 = s1.concat(\" World\");  // Creates NEW string");
        System.out.println("// s1 is still \"Hello\" (unchanged)");

        System.out.println("\n// C++ std::string (Mutable)");
        System.out.println("std::string s1 = \"Hello\";");
        System.out.println("s1 += \" World\";  // Modifies in-place");
        System.out.println("// s1 is now \"Hello World\" (changed!)");

        System.out.println("\n// Java StringBuilder (like C++ std::string)");
        System.out.println("StringBuilder sb = new StringBuilder(\"Hello\");");
        System.out.println("sb.append(\" World\");  // Modifies in-place");
        System.out.println("String result = sb.toString();");

        System.out.println("\nKey Differences:");
        System.out.println("1. Java String is IMMUTABLE (thread-safe, memory overhead)");
        System.out.println("2. C++ std::string is MUTABLE (faster, less safe)");
        System.out.println("3. Java StringBuilder ≈ C++ std::string (mutable)");
        System.out.println("4. Java has string pool, C++ doesn't");
        System.out.println("5. C++ has Small String Optimization (SSO), Java doesn't");

        System.out.println("\nC++ Small String Optimization (SSO):");
        System.out.println("• Short strings (≤15 chars) stored in-place (no heap)");
        System.out.println("• Long strings allocated on heap");
        System.out.println("• Automatic, transparent to user");
        System.out.println("• Java always uses heap for String objects");
    }

    //=============================================================================
    // 8. TRADING SYSTEM EXAMPLES
    //=============================================================================

    static void tradingExamples() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  8. TRADING SYSTEM EXAMPLES                            ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        // Example 1: Build FIX message (use StringBuilder)
        System.out.println("Example 1: Building FIX Message");
        StringBuilder fixMsg = new StringBuilder();
        fixMsg.append("8=FIX.4.2").append('|')
              .append("35=D").append('|')
              .append("49=SENDER").append('|')
              .append("56=TARGET").append('|')
              .append("55=AAPL").append('|')
              .append("54=1").append('|')
              .append("38=100").append('|')
              .append("44=150.50");

        System.out.println("FIX Message: " + fixMsg.toString());

        // Example 2: Parse order data
        System.out.println("\nExample 2: Parsing Order Data");
        String orderData = "AAPL|BUY|150.50|1000";
        String[] fields = orderData.split("\\|");
        System.out.println("Symbol: " + fields[0]);
        System.out.println("Side: " + fields[1]);
        System.out.println("Price: " + fields[2]);
        System.out.println("Quantity: " + fields[3]);

        // Example 3: Format log message
        System.out.println("\nExample 3: Format Log Message");
        String logMsg = String.format("[%s] Order %d: %s %d @ %.2f %s",
                                     "2026-02-08 19:45:00",
                                     123456789L,
                                     "BUY",
                                     1000,
                                     150.50,
                                     "AAPL");
        System.out.println(logMsg);

        // Example 4: Build JSON (StringBuilder)
        System.out.println("\nExample 4: Building JSON");
        StringBuilder json = new StringBuilder();
        json.append("{")
            .append("\"orderId\":").append(12345).append(",")
            .append("\"symbol\":\"").append("AAPL").append("\",")
            .append("\"price\":").append(150.50).append(",")
            .append("\"quantity\":").append(1000)
            .append("}");
        System.out.println(json.toString());

        // Example 5: Symbol normalization
        System.out.println("\nExample 5: Symbol Normalization");
        String rawSymbol = "  aapl.us  ";
        String normalized = rawSymbol.trim().toUpperCase().replace(".US", "");
        System.out.println("Raw: '" + rawSymbol + "'");
        System.out.println("Normalized: '" + normalized + "'");
    }

    //=============================================================================
    // 9. BEST PRACTICES
    //=============================================================================

    static void bestPractices() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  9. BEST PRACTICES                                     ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        System.out.println("✅ DO:");
        System.out.println("  • Use String for immutable data (symbol, side, etc.)");
        System.out.println("  • Use StringBuilder for single-threaded concatenation");
        System.out.println("  • Use StringBuffer for multi-threaded concatenation");
        System.out.println("  • Use String.format() for complex formatting");
        System.out.println("  • Use equals() for string comparison (not ==)");
        System.out.println("  • Reuse StringBuilder instances when possible");
        System.out.println("  • Pre-size StringBuilder if length known");

        System.out.println("\n❌ DON'T:");
        System.out.println("  • Don't use += in loops (creates many objects)");
        System.out.println("  • Don't use == for string comparison (compares references)");
        System.out.println("  • Don't use StringBuffer if not multi-threaded");
        System.out.println("  • Don't create unnecessary String objects");
        System.out.println("  • Don't intern() unless you understand memory impact");

        System.out.println("\nPerformance Tips:");
        System.out.println("  • StringBuilder is 100-1000x faster than String +=");
        System.out.println("  • Pre-sizing StringBuilder avoids array resizing");
        System.out.println("  • String pool saves memory but uses PermGen/Metaspace");
        System.out.println("  • StringBuffer ~20-30% slower than StringBuilder");

        System.out.println("\nTrading System Recommendations:");
        System.out.println("  • Hot path: Use StringBuilder, avoid String +=");
        System.out.println("  • Logging: StringBuilder for message construction");
        System.out.println("  • Parsing: String.split() or manual parsing");
        System.out.println("  • Symbol keys: Use String (immutable, pool)");
        System.out.println("  • Message building: StringBuilder (mutable, fast)");
    }

    //=============================================================================
    // 10. SUMMARY TABLE
    //=============================================================================

    static void printSummaryTable() {
        System.out.println("\n╔════════════════════════════════════════════════════════╗");
        System.out.println("║  10. SUMMARY COMPARISON TABLE                          ║");
        System.out.println("╚════════════════════════════════════════════════════════╝\n");

        System.out.println("┌──────────────────┬────────────┬──────────────┬──────────────┬─────────────┐");
        System.out.println("│ Feature          │ String     │ StringBuilder│ StringBuffer │ C++ string  │");
        System.out.println("├──────────────────┼────────────┼──────────────┼──────────────┼─────────────┤");
        System.out.println("│ Mutable          │ NO         │ YES          │ YES          │ YES         │");
        System.out.println("│ Thread-safe      │ YES        │ NO           │ YES          │ NO          │");
        System.out.println("│ Performance      │ Slow (++)  │ Fast         │ Medium       │ Very Fast   │");
        System.out.println("│ Memory           │ Higher     │ Medium       │ Medium       │ Lower       │");
        System.out.println("│ Use case         │ Constants  │ Loops        │ Multi-thread │ General     │");
        System.out.println("│ Pool support     │ YES        │ NO           │ NO           │ NO          │");
        System.out.println("│ Concat overhead  │ Very High  │ Low          │ Medium       │ Very Low    │");
        System.out.println("│ Null allowed     │ YES        │ N/A          │ N/A          │ NO          │");
        System.out.println("│ Move semantics   │ NO         │ NO           │ NO           │ YES         │");
        System.out.println("│ SSO (Small Str)  │ NO         │ NO           │ NO           │ YES         │");
        System.out.println("└──────────────────┴────────────┴──────────────┴──────────────┴─────────────┘");

        System.out.println("\nWhen to Use:");
        System.out.println("┌──────────────────┬─────────────────────────────────────────────────┐");
        System.out.println("│ String           │ Immutable data, symbols, constants              │");
        System.out.println("│ StringBuilder    │ Loops, single-threaded string building (BEST)   │");
        System.out.println("│ StringBuffer     │ Multi-threaded string building                  │");
        System.out.println("│ C++ std::string  │ General purpose, mutable, high performance      │");
        System.out.println("└──────────────────┴─────────────────────────────────────────────────┘");
    }

    //=============================================================================
    // MAIN
    //=============================================================================

    public static void main(String[] args) {
        System.out.println("╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║                                                                ║");
        System.out.println("║       Java Strings Complete Guide with C++ Comparison         ║");
        System.out.println("║                                                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        demonstrateStringImmutability();
        demonstrateStringBuilder();
        demonstrateStringBuffer();
        demonstrateStringPool();
        demonstrateStringOperations();
        demonstratePerformance();
        compareWithCppString();
        tradingExamples();
        bestPractices();
        printSummaryTable();

        System.out.println("\n╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║  Key Takeaways                                                 ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");
        System.out.println("1. String: Immutable, thread-safe, memory overhead");
        System.out.println("2. StringBuilder: Mutable, NOT thread-safe, FAST (use this!)");
        System.out.println("3. StringBuffer: Mutable, thread-safe, slower than StringBuilder");
        System.out.println("4. C++ std::string: Mutable, fast, SSO, no thread safety");
        System.out.println("\nFor Trading Systems:");
        System.out.println("✅ Use StringBuilder for message construction");
        System.out.println("✅ Use String for immutable symbols/keys");
        System.out.println("✅ Avoid String += in loops (100-1000x slower!)");
        System.out.println("\n🚀 StringBuilder ≈ C++ std::string (both mutable and fast!)");
    }
}

