/**
 * Java Built-in Types vs C++ Types - Complete Comparison
 *
 * This file demonstrates all Java built-in (primitive) types
 * and their C++ equivalents with practical examples
 *
 * Topics Covered:
 * 1. Primitive types comparison
 * 2. Size guarantees
 * 3. Default values
 * 4. Wrapper classes (Java) vs value types (C++)
 * 5. Type conversions
 * 6. Trading system examples
 */
public class JavaCppBuiltInTypes {

    //=============================================================================
    // PRIMITIVE TYPES COMPARISON
    //=============================================================================

    /**
     * Java has 8 primitive types (built-in)
     * C++ has similar types but with different guarantees
     */

    // ────────────────────────────────────────────────────────────────────────
    // 1. INTEGER TYPES
    // ────────────────────────────────────────────────────────────────────────

    // Java: byte (8-bit signed, -128 to 127)
    // C++:  int8_t or signed char (exact equivalent)
    //       char (usually 8-bit but not guaranteed)
    static void demonstrateByte() {
        byte javaByte = 127;                    // Max value
        byte minByte = -128;                    // Min value

        // C++ equivalent:
        // int8_t cppByte = 127;
        // signed char cppChar = -128;

        System.out.println("Java byte: " + Byte.MIN_VALUE + " to " + Byte.MAX_VALUE);
        System.out.println("Size: " + Byte.BYTES + " bytes");

        // Use case: Flags, small counters
        byte orderStatus = 1;  // 0=pending, 1=filled, 2=cancelled
    }

    // Java: short (16-bit signed, -32,768 to 32,767)
    // C++:  int16_t or short (int16_t is guaranteed, short is not)
    static void demonstrateShort() {
        short javaShort = 32767;                // Max value
        short minShort = -32768;                // Min value

        // C++ equivalent:
        // int16_t cppShort = 32767;
        // short cppShortMaybe = 32767;  // Usually 16-bit but not guaranteed

        System.out.println("Java short: " + Short.MIN_VALUE + " to " + Short.MAX_VALUE);
        System.out.println("Size: " + Short.BYTES + " bytes");

        // Use case: Medium-sized counters
        short tickCount = 1000;
    }

    // Java: int (32-bit signed, -2^31 to 2^31-1)
    // C++:  int32_t (guaranteed 32-bit) or int (NOT guaranteed!)
    static void demonstrateInt() {
        int javaInt = 2147483647;               // Max value (2^31 - 1)
        int minInt = -2147483648;               // Min value (-2^31)

        // C++ equivalent:
        // int32_t cppInt32 = 2147483647;        // Guaranteed 32-bit
        // int cppInt = 2147483647;              // NOT guaranteed (could be 16 or 64-bit!)

        System.out.println("Java int: " + Integer.MIN_VALUE + " to " + Integer.MAX_VALUE);
        System.out.println("Size: " + Integer.BYTES + " bytes");

        // Use case: Most common integer type
        int orderQuantity = 1000;
        int symbolId = 12345;
    }

    // Java: long (64-bit signed, -2^63 to 2^63-1)
    // C++:  int64_t (guaranteed) or long long (usually 64-bit)
    //       long (NOT guaranteed - could be 32 or 64-bit!)
    static void demonstrateLong() {
        long javaLong = 9223372036854775807L;   // Max value (2^63 - 1)
        long minLong = -9223372036854775808L;   // Min value (-2^63)

        // C++ equivalent:
        // int64_t cppInt64 = 9223372036854775807LL;     // Guaranteed 64-bit
        // long long cppLongLong = 9223372036854775807LL; // Usually 64-bit
        // long cppLong = 9223372036854775807L;          // NOT guaranteed!

        System.out.println("Java long: " + Long.MIN_VALUE + " to " + Long.MAX_VALUE);
        System.out.println("Size: " + Long.BYTES + " bytes");

        // Use case: Timestamps, large quantities, IDs
        long timestamp = System.currentTimeMillis();
        long orderId = 1234567890123456789L;
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2. FLOATING-POINT TYPES
    // ────────────────────────────────────────────────────────────────────────

    // Java: float (32-bit IEEE 754 floating point)
    // C++:  float (same, 32-bit IEEE 754)
    static void demonstrateFloat() {
        float javaFloat = 3.14159f;             // 'f' suffix required
        float maxFloat = Float.MAX_VALUE;       // ~3.4e38
        float minFloat = Float.MIN_VALUE;       // ~1.4e-45 (smallest positive)

        // C++ equivalent:
        // float cppFloat = 3.14159f;
        // float maxFloat = std::numeric_limits<float>::max();

        System.out.println("Java float range: " + Float.MIN_VALUE + " to " + Float.MAX_VALUE);
        System.out.println("Size: " + Float.BYTES + " bytes");
        System.out.println("Precision: ~6-7 decimal digits");

        // Use case: Space-constrained applications (NOT for money!)
        float approximatePrice = 100.5f;  // ⚠️ Don't use for financial calculations!
    }

    // Java: double (64-bit IEEE 754 floating point)
    // C++:  double (same, 64-bit IEEE 754)
    static void demonstrateDouble() {
        double javaDouble = 3.141592653589793;  // No suffix needed (default)
        double maxDouble = Double.MAX_VALUE;    // ~1.8e308
        double minDouble = Double.MIN_VALUE;    // ~4.9e-324 (smallest positive)

        // C++ equivalent:
        // double cppDouble = 3.141592653589793;
        // double maxDouble = std::numeric_limits<double>::max();

        System.out.println("Java double range: " + Double.MIN_VALUE + " to " + Double.MAX_VALUE);
        System.out.println("Size: " + Double.BYTES + " bytes");
        System.out.println("Precision: ~15-16 decimal digits");

        // Use case: Scientific calculations, prices (with care!)
        double price = 100.50;
        double calculatedValue = price * 1.05;

        // ⚠️ WARNING: Floating-point precision issues!
        System.out.println("0.1 + 0.2 = " + (0.1 + 0.2));  // 0.30000000000000004 (!)
    }

    // ────────────────────────────────────────────────────────────────────────
    // 3. BOOLEAN TYPE
    // ────────────────────────────────────────────────────────────────────────

    // Java: boolean (true or false, size not specified)
    // C++:  bool (true or false, usually 1 byte)
    static void demonstrateBoolean() {
        boolean javaBool = true;
        boolean isFilled = false;

        // C++ equivalent:
        // bool cppBool = true;
        // bool isFilled = false;

        System.out.println("Java boolean values: true, false");
        // Size is implementation-dependent (usually 1 byte)

        // Use case: Flags, conditions
        boolean isMarketOpen = true;
        boolean hasPosition = false;

        // Java: No implicit conversion from int to boolean
        // boolean bad = 1;  // ❌ Compile error!

        // C++: Allows implicit conversion (0=false, non-zero=true)
        // bool cppFromInt = 42;  // ✅ OK in C++ (true)
    }

    // ────────────────────────────────────────────────────────────────────────
    // 4. CHARACTER TYPE
    // ────────────────────────────────────────────────────────────────────────

    // Java: char (16-bit Unicode, 0 to 65,535)
    // C++:  char (usually 8-bit ASCII)
    //       wchar_t, char16_t, char32_t (for Unicode)
    static void demonstrateChar() {
        char javaChar = 'A';                    // Single quotes required
        char unicodeChar = '\u0041';            // Unicode for 'A'
        char chineseChar = '中';                // Unicode character

        // C++ equivalent:
        // char cppChar = 'A';                  // 8-bit (ASCII)
        // char16_t cppChar16 = u'A';           // 16-bit (UTF-16)
        // wchar_t cppWChar = L'A';             // Platform-dependent

        System.out.println("Java char: " + Character.MIN_VALUE + " to " + Character.MAX_VALUE);
        System.out.println("Size: " + Character.BYTES + " bytes (16-bit)");

        // Use case: Single characters
        char orderSide = 'B';  // 'B' for Buy, 'S' for Sell
        char currency = '$';

        // Java char is ALWAYS 16-bit (UTF-16)
        // C++ char is USUALLY 8-bit (depends on platform)
    }

    //=============================================================================
    // DEFAULT VALUES
    //=============================================================================

    static class DefaultValues {
        // Java: Instance variables have default values
        byte defaultByte;       // 0
        short defaultShort;     // 0
        int defaultInt;         // 0
        long defaultLong;       // 0L
        float defaultFloat;     // 0.0f
        double defaultDouble;   // 0.0
        boolean defaultBool;    // false
        char defaultChar;       // '\u0000' (null character)

        void showDefaults() {
            System.out.println("byte default: " + defaultByte);
            System.out.println("short default: " + defaultShort);
            System.out.println("int default: " + defaultInt);
            System.out.println("long default: " + defaultLong);
            System.out.println("float default: " + defaultFloat);
            System.out.println("double default: " + defaultDouble);
            System.out.println("boolean default: " + defaultBool);
            System.out.println("char default: " + (int)defaultChar);
        }

        // C++ equivalent: NO DEFAULT INITIALIZATION!
        // class CppDefaults {
        //     int value;  // ⚠️ UNINITIALIZED! (garbage value)
        // };
        //
        // Always initialize in C++:
        // class CppDefaults {
        //     int value = 0;  // C++11 in-class initialization
        // };
    }

    //=============================================================================
    // WRAPPER CLASSES (Java) vs VALUE TYPES (C++)
    //=============================================================================

    static void demonstrateWrappers() {
        // Java: Each primitive has a wrapper class
        Byte byteObj = 127;                     // Autoboxing
        Short shortObj = 1000;
        Integer intObj = 100000;
        Long longObj = 1000000000L;
        Float floatObj = 3.14f;
        Double doubleObj = 3.14159;
        Boolean boolObj = true;
        Character charObj = 'A';

        // C++ has no wrapper classes (no autoboxing/unboxing)
        // Everything is a value type by default
        // Use pointers/references for indirection:
        // int* cppIntPtr = new int(100);
        // std::unique_ptr<int> cppSmartPtr = std::make_unique<int>(100);

        // Java: Wrappers are objects (can be null)
        Integer nullableInt = null;             // ✅ Valid in Java

        // C++ equivalent:
        // std::optional<int> cppOptional;      // C++17 optional type
        // int* cppPtr = nullptr;               // Pointer can be null

        // Autoboxing/Unboxing (Java only)
        int primitive = intObj;                 // Unboxing (automatic)
        Integer wrapper = primitive;            // Boxing (automatic)

        // ⚠️ Performance: Boxing creates objects (heap allocation)
        // Avoid in performance-critical code!
    }

    //=============================================================================
    // TYPE CONVERSIONS
    //=============================================================================

    static void demonstrateConversions() {
        // ─────────────────────────────────────────────────────────────────
        // 1. Implicit widening conversions (safe, no data loss)
        // ─────────────────────────────────────────────────────────────────

        byte b = 10;
        short s = b;        // byte → short (OK)
        int i = s;          // short → int (OK)
        long l = i;         // int → long (OK)
        float f = l;        // long → float (OK, may lose precision)
        double d = f;       // float → double (OK)

        // C++ same implicit conversions

        // ─────────────────────────────────────────────────────────────────
        // 2. Explicit narrowing conversions (may lose data)
        // ─────────────────────────────────────────────────────────────────

        long bigValue = 1000000L;
        int narrowed = (int) bigValue;          // Explicit cast required

        double doubleVal = 3.14159;
        int truncated = (int) doubleVal;        // 3 (fractional part lost)

        // C++ same explicit casting:
        // int cppNarrowed = static_cast<int>(bigValue);
        // int cppTruncated = static_cast<int>(doubleVal);

        // ─────────────────────────────────────────────────────────────────
        // 3. String conversions
        // ─────────────────────────────────────────────────────────────────

        // Java: Wrapper class methods
        int fromString = Integer.parseInt("123");
        String toString = Integer.toString(123);

        // C++ equivalent:
        // int cppFromString = std::stoi("123");
        // std::string cppToString = std::to_string(123);
    }

    //=============================================================================
    // TRADING SYSTEM EXAMPLES
    //=============================================================================

    static class TradingTypes {
        // Order structure using primitive types
        long orderId;           // Unique order ID
        int symbolId;           // Symbol identifier
        double price;           // Order price (⚠️ use BigDecimal for real money!)
        int quantity;           // Order quantity
        char side;              // 'B' for Buy, 'S' for Sell
        byte status;            // 0=pending, 1=filled, 2=cancelled
        long timestamp;         // Order timestamp (milliseconds)

        // C++ equivalent:
        // struct Order {
        //     int64_t orderId;
        //     int32_t symbolId;
        //     double price;
        //     int32_t quantity;
        //     char side;
        //     uint8_t status;
        //     int64_t timestamp;
        // };

        void demonstrateTradingCalculations() {
            // Price calculations
            double bidPrice = 100.50;
            double askPrice = 100.55;
            double spread = askPrice - bidPrice;    // 0.05

            // Quantity calculations
            int totalQuantity = 1000;
            int filledQuantity = 750;
            int remainingQuantity = totalQuantity - filledQuantity;  // 250

            // Notional value
            double notionalValue = bidPrice * totalQuantity;  // 100,500.0

            // Tick calculations
            double tickSize = 0.01;
            int numTicks = (int)((askPrice - bidPrice) / tickSize);  // 5

            System.out.println("Spread: " + spread);
            System.out.println("Remaining: " + remainingQuantity);
            System.out.println("Notional: " + notionalValue);
            System.out.println("Ticks: " + numTicks);
        }
    }

    //=============================================================================
    // SIZE COMPARISON TABLE
    //=============================================================================

    static void printSizeComparison() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  Java vs C++ Built-in Types Comparison                    ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("┌──────────────┬────────────┬──────────────────┬──────────────┐");
        System.out.println("│ Java Type    │ Size       │ C++ Guaranteed   │ C++ Common   │");
        System.out.println("├──────────────┼────────────┼──────────────────┼──────────────┤");
        System.out.println("│ byte         │ 8-bit      │ int8_t           │ signed char  │");
        System.out.println("│ short        │ 16-bit     │ int16_t          │ short        │");
        System.out.println("│ int          │ 32-bit     │ int32_t          │ int          │");
        System.out.println("│ long         │ 64-bit     │ int64_t          │ long long    │");
        System.out.println("│ float        │ 32-bit     │ float            │ float        │");
        System.out.println("│ double       │ 64-bit     │ double           │ double       │");
        System.out.println("│ boolean      │ (varies)   │ bool             │ bool         │");
        System.out.println("│ char         │ 16-bit     │ char16_t         │ wchar_t      │");
        System.out.println("└──────────────┴────────────┴──────────────────┴──────────────┘\n");

        System.out.println("Key Differences:");
        System.out.println("✅ Java: Sizes are GUARANTEED across all platforms");
        System.out.println("⚠️  C++: Use <cstdint> types (int32_t, etc.) for guaranteed sizes");
        System.out.println("❌ C++: int, long sizes are NOT guaranteed!");
        System.out.println("    • int could be 16 or 32-bit");
        System.out.println("    • long could be 32 or 64-bit");
    }

    //=============================================================================
    // KEY DIFFERENCES SUMMARY
    //=============================================================================

    static void printKeyDifferences() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  Key Differences: Java vs C++                             ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("1. Size Guarantees:");
        System.out.println("   Java:  ✅ ALWAYS guaranteed (portable)");
        System.out.println("   C++:   ⚠️  Use <cstdint> for guarantees (int32_t, etc.)");

        System.out.println("\n2. Default Values:");
        System.out.println("   Java:  ✅ Instance variables auto-initialized");
        System.out.println("   C++:   ❌ NO default initialization (garbage values!)");

        System.out.println("\n3. Wrapper Classes:");
        System.out.println("   Java:  ✅ Has wrapper classes (Integer, Double, etc.)");
        System.out.println("   C++:   ❌ No wrappers (use pointers/references)");

        System.out.println("\n4. Autoboxing:");
        System.out.println("   Java:  ✅ Automatic boxing/unboxing");
        System.out.println("   C++:   ❌ No autoboxing (explicit conversion)");

        System.out.println("\n5. Boolean Conversion:");
        System.out.println("   Java:  ❌ No implicit int ↔ boolean conversion");
        System.out.println("   C++:   ✅ Allows int ↔ bool (0=false, other=true)");

        System.out.println("\n6. Character Encoding:");
        System.out.println("   Java:  char is ALWAYS 16-bit Unicode (UTF-16)");
        System.out.println("   C++:   char is USUALLY 8-bit ASCII");

        System.out.println("\n7. Unsigned Types:");
        System.out.println("   Java:  ❌ No unsigned types (except char)");
        System.out.println("   C++:   ✅ Has unsigned int, unsigned long, etc.");
    }

    //=============================================================================
    // PERFORMANCE CONSIDERATIONS
    //=============================================================================

    static void demonstratePerformance() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  Performance Considerations                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        // Boxing overhead
        long start = System.nanoTime();
        for (int i = 0; i < 1_000_000; i++) {
            int primitive = i;  // No allocation
        }
        long primitiveTime = System.nanoTime() - start;

        start = System.nanoTime();
        for (int i = 0; i < 1_000_000; i++) {
            Integer boxed = i;  // Heap allocation (may be cached)
        }
        long boxedTime = System.nanoTime() - start;

        System.out.println("Primitive operations: " + primitiveTime + " ns");
        System.out.println("Boxed operations:     " + boxedTime + " ns");
        System.out.println("Overhead:             " + (boxedTime - primitiveTime) + " ns");

        System.out.println("\n⚠️  For trading systems:");
        System.out.println("    • Use primitives for performance-critical code");
        System.out.println("    • Avoid autoboxing in hot paths");
        System.out.println("    • Wrapper objects cause GC pressure");
        System.out.println("    • C++ has no boxing overhead (all value types)");
    }

    //=============================================================================
    // MAIN
    //=============================================================================

    public static void main(String[] args) {
        System.out.println("╔════════════════════════════════════════════════════════════╗");
        System.out.println("║                                                            ║");
        System.out.println("║     Java Built-in Types vs C++ Comparison                 ║");
        System.out.println("║                                                            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("=== Primitive Types ===\n");
        demonstrateByte();
        System.out.println();
        demonstrateShort();
        System.out.println();
        demonstrateInt();
        System.out.println();
        demonstrateLong();
        System.out.println();
        demonstrateFloat();
        System.out.println();
        demonstrateDouble();
        System.out.println();
        demonstrateBoolean();
        System.out.println();
        demonstrateChar();

        System.out.println("\n=== Default Values ===\n");
        new DefaultValues().showDefaults();

        System.out.println("\n=== Wrapper Classes ===\n");
        demonstrateWrappers();

        System.out.println("\n=== Type Conversions ===\n");
        demonstrateConversions();

        System.out.println("\n=== Trading Examples ===\n");
        new TradingTypes().demonstrateTradingCalculations();

        printSizeComparison();
        printKeyDifferences();
        demonstratePerformance();

        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  Summary                                                   ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");
        System.out.println("Java primitives = Guaranteed sizes, auto-init, wrappers");
        System.out.println("C++ types      = Use <cstdint>, manual init, no wrappers");
        System.out.println("\nFor trading: Use primitives for performance! 🚀");
    }
}


