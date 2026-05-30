/**
 * ============================================================================
 * Java: int[] vs Integer[] — Primitive vs Object Arrays
 * Use Cases, Memory Layout, Latency Cost, ULL Trading Guide
 * ============================================================================
 *
 * THE CORE PROBLEM:
 *   int[]     → contiguous memory, no GC overhead, cache-friendly  (like C++ int[])
 *   Integer[] → array of pointers to heap objects, GC scans each, cache-hostile
 *
 * MEMORY LAYOUT:
 *
 *   int[4]   = [  4  |  8  | 15  | 16  ]     ← 4 ints, 16 bytes, ONE cache line
 *               addr  addr+4 addr+8 addr+12    ← contiguous, no pointer chasing
 *
 *   Integer[4]= [ ptr0 | ptr1 | ptr2 | ptr3 ]  ← 4 pointers (32 bytes on 64-bit)
 *                  ↓       ↓      ↓      ↓
 *               [hdr|4] [hdr|8] [hdr|15] [hdr|16]  ← 4 heap objects, scattered
 *                16 bytes each = 64 bytes + GC metadata + pointer chasing
 *
 * C++ EQUIVALENT:
 *   int[]     ≡  int arr[N]  or  std::array<int,N>  or  int* arr = new int[N]
 *   Integer[] ≡  std::vector<std::unique_ptr<int>>  (nobody does this in C++)
 *
 * Build:
 *   javac Java_Primitive_vs_Object_Array_ULL.java
 *   java -Xmx512m Java_Primitive_vs_Object_Array_ULL
 * ============================================================================
 */

import java.util.*;
import java.util.concurrent.atomic.*;
import java.util.stream.*;

public class Java_Primitive_vs_Object_Array_ULL {

    // =========================================================================
    // SECTION 1: MEMORY LAYOUT — What actually lives in RAM
    // =========================================================================
    static void memoryLayoutExplained() {
        System.out.println("""
        ══════════════════════════════════════════════════════════════════════
         SECTION 1: Memory Layout — int[] vs Integer[]
        ══════════════════════════════════════════════════════════════════════

        int[] layout (contiguous, cache-friendly):
        ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
        │hdr 8│len 4│  4  │  8  │  15 │  16 │  23 │  42 │  ← ONE block
        └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
         Object header (8 bytes) + length (4 bytes) + values packed tightly
         Total: 8 + 4 + (N * 4) bytes = no gaps, NO pointer chasing

        Integer[] layout (array of references, cache-hostile):
        ┌─────┬─────┬──────┬──────┬──────┬──────┐
        │hdr 8│len 4│ ref0 │ ref1 │ ref2 │ ref3 │  ← reference array
        └─────┴─────┴──┬───┴──┬───┴──┬───┴──┬───┘
                       │      │      │      │
                  ┌────▼┐ ┌───▼─┐ ┌──▼──┐ ┌▼────┐  ← 4 SEPARATE heap objects
                  │hdr 8│ │hdr 8│ │hdr 8│ │hdr 8│    each 16 bytes (8 hdr + 4 val + 4 pad)
                  │val 4│ │val 4│ │val 4│ │val 4│    scattered across heap
                  └─────┘ └─────┘ └─────┘ └─────┘

        Sizes (approximate, JVM-dependent):
          int[1000]     =  8 + 4 + 4000     =   ~4 KB   (1 GC object)
          Integer[1000] =  8 + 4 + 8000     +   16000   =  ~24 KB  (1001 GC objects)
                          (ref array)         (1000 Integer heap objects)

        Cache lines (64 bytes each):
          int[1000]:     1000 * 4 / 64 = 63 cache lines  (all useful data)
          Integer[1000]: ref array = 1000 * 8 / 64 = 125 lines
                         + Integer objects = 1000 * 16 / 64 = 250 lines scattered
                         = 375 cache lines, most are cache misses
        """);
    }

    // =========================================================================
    // SECTION 2: MICRO-BENCHMARK — Measure actual latency difference
    // =========================================================================
    static class Benchmark {

        static final int SIZE     = 1_000_000;
        static final int WARMUP   = 5;
        static final int RUNS     = 10;

        // Prevent JIT dead-code elimination
        static volatile long sink;

        // ── Test 1: Sequential sum ───────────────────────────────────────────
        static long sumPrimitive(int[] arr) {
            long sum = 0;
            for (int v : arr) sum += v;
            return sum;
        }

        static long sumBoxed(Integer[] arr) {
            long sum = 0;
            for (Integer v : arr) sum += v;  // unboxing on every element
            return sum;
        }

        // ── Test 2: Random access (cache miss pattern) ───────────────────────
        static long randomAccessPrimitive(int[] arr, int[] indices) {
            long sum = 0;
            for (int idx : indices) sum += arr[idx];
            return sum;
        }

        static long randomAccessBoxed(Integer[] arr, int[] indices) {
            long sum = 0;
            for (int idx : indices) sum += arr[idx];  // unbox each access
            return sum;
        }

        // ── Test 3: Write (fill array) ───────────────────────────────────────
        static void fillPrimitive(int[] arr) {
            for (int i = 0; i < arr.length; i++) arr[i] = i * 3;
        }

        static void fillBoxed(Integer[] arr) {
            for (int i = 0; i < arr.length; i++) arr[i] = i * 3;  // autoboxing
        }

        // ── Test 4: Sort ─────────────────────────────────────────────────────
        static void sortPrimitive(int[] arr) {
            Arrays.sort(arr);
        }

        static void sortBoxed(Integer[] arr) {
            Arrays.sort(arr);  // uses TimSort (slower, object comparisons)
        }

        static long measureNs(Runnable task) {
            long start = System.nanoTime();
            task.run();
            return System.nanoTime() - start;
        }

        static void run() {
            System.out.println("\n=== SECTION 2: Micro-Benchmark Results ===");
            System.out.printf("  Array size: %,d elements%n%n", SIZE);

            // Setup
            int[] primitive = new int[SIZE];
            Integer[] boxed = new Integer[SIZE];
            int[] indices   = new int[SIZE];
            Random rng = new Random(42);

            for (int i = 0; i < SIZE; i++) {
                primitive[i] = rng.nextInt(1000);
                boxed[i]     = primitive[i];           // same values
                indices[i]   = rng.nextInt(SIZE);
            }

            // ── Test 1: Sequential Sum ───────────────────────────────────────
            // Warmup
            for (int w = 0; w < WARMUP; w++) {
                sink = sumPrimitive(primitive);
                sink = sumBoxed(boxed);
            }
            long primSum = 0, boxSum = 0;
            for (int r = 0; r < RUNS; r++) {
                primSum += measureNs(() -> sink = sumPrimitive(primitive));
                boxSum  += measureNs(() -> sink = sumBoxed(boxed));
            }
            primSum /= RUNS; boxSum /= RUNS;
            System.out.printf("  Sequential Sum (1M elements):%n");
            System.out.printf("    int[]     : %,6d µs%n", primSum / 1000);
            System.out.printf("    Integer[] : %,6d µs%n", boxSum  / 1000);
            System.out.printf("    Overhead  : %.1fx slower%n%n",
                (double) boxSum / Math.max(primSum, 1));

            // ── Test 2: Random Access ────────────────────────────────────────
            for (int w = 0; w < WARMUP; w++) {
                sink = randomAccessPrimitive(primitive, indices);
                sink = randomAccessBoxed(boxed, indices);
            }
            long primRand = 0, boxRand = 0;
            for (int r = 0; r < RUNS; r++) {
                primRand += measureNs(() -> sink = randomAccessPrimitive(primitive, indices));
                boxRand  += measureNs(() -> sink = randomAccessBoxed(boxed, indices));
            }
            primRand /= RUNS; boxRand /= RUNS;
            System.out.printf("  Random Access (1M accesses):%n");
            System.out.printf("    int[]     : %,6d µs%n", primRand / 1000);
            System.out.printf("    Integer[] : %,6d µs%n", boxRand  / 1000);
            System.out.printf("    Overhead  : %.1fx slower%n%n",
                (double) boxRand / Math.max(primRand, 1));

            // ── Test 3: Write / Fill ─────────────────────────────────────────
            for (int w = 0; w < WARMUP; w++) {
                fillPrimitive(primitive);
                fillBoxed(boxed);
            }
            long primFill = 0, boxFill = 0;
            for (int r = 0; r < RUNS; r++) {
                primFill += measureNs(() -> fillPrimitive(primitive));
                boxFill  += measureNs(() -> fillBoxed(boxed));
            }
            primFill /= RUNS; boxFill /= RUNS;
            System.out.printf("  Array Fill / Write (1M writes):%n");
            System.out.printf("    int[]     : %,6d µs%n", primFill / 1000);
            System.out.printf("    Integer[] : %,6d µs%n", boxFill  / 1000);
            System.out.printf("    Overhead  : %.1fx slower%n%n",
                (double) boxFill / Math.max(primFill, 1));

            // ── Test 4: Sort ─────────────────────────────────────────────────
            int[] primCopy = Arrays.copyOf(primitive, SIZE);
            Integer[] boxCopy = Arrays.copyOf(boxed, SIZE);
            for (int w = 0; w < WARMUP; w++) {
                Arrays.sort(Arrays.copyOf(primCopy, SIZE));
                Arrays.sort(Arrays.copyOf(boxCopy, SIZE));
            }
            long primSort = measureNs(() -> Arrays.sort(Arrays.copyOf(primCopy, SIZE)));
            long boxSort  = measureNs(() -> Arrays.sort(Arrays.copyOf(boxCopy,  SIZE)));
            System.out.printf("  Sort (1M elements):%n");
            System.out.printf("    int[]     : %,6d µs  (dual-pivot quicksort)%n",
                primSort / 1000);
            System.out.printf("    Integer[] : %,6d µs  (TimSort, Comparator overhead)%n",
                boxSort  / 1000);
            System.out.printf("    Overhead  : %.1fx slower%n%n",
                (double) boxSort / Math.max(primSort, 1));

            // ── Test 5: Autoboxing cache hit vs miss ─────────────────────────
            // Integer cache: -128 to 127 are cached (no new object)
            // Outside range: new Integer object every time
            System.out.printf("  Autoboxing cache (JVM Integer cache -128 to 127):%n");
            long inCache  = measureNs(() -> {
                for (int i = 0; i < 1_000_000; i++) {
                    Integer x = (i % 200) - 100;  // -100 to 99: IN cache
                    sink += x;
                }
            });
            long outCache = measureNs(() -> {
                for (int i = 0; i < 1_000_000; i++) {
                    Integer x = i + 1000;          // OUT of cache: new Integer()
                    sink += x;
                }
            });
            System.out.printf("    In-cache  (-128 to 127): %,6d µs (no allocation)%n",
                inCache  / 1000);
            System.out.printf("    Out-cache (> 127):       %,6d µs (new Integer each time)%n",
                outCache / 1000);
            System.out.printf("    GC pressure: out-cache creates 1M new Integer objects!%n");
        }
    }

    // =========================================================================
    // SECTION 3: AUTOBOXING PITFALLS — Silent performance killers
    // =========================================================================
    static void autoboxingPitfalls() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 3: Autoboxing Pitfalls — Silent Performance Killers
        ══════════════════════════════════════════════════════════════════════

        ─── Pitfall 1: Unintended boxing in loops ────────────────────────────

          // BAD: autoboxes i → new Integer(i) on every iteration = 1M objects
          Integer sum = 0;
          for (int i = 0; i < 1_000_000; i++) {
              sum += i;    // sum = Integer.valueOf(sum.intValue() + i)  ← 2 operations!
          }
          // Generated bytecode:
          //   INVOKEVIRTUAL Integer.intValue()    ← unbox
          //   IADD
          //   INVOKESTATIC Integer.valueOf(int)   ← rebox → may allocate

          // GOOD: zero boxing
          int sum = 0;
          for (int i = 0; i < 1_000_000; i++) sum += i;


        ─── Pitfall 2: HashMap<Integer, ...> on hot path ────────────────────

          // BAD: HashMap.get(int) requires boxing
          HashMap<Integer, Order> orderMap = new HashMap<>();
          int orderId = 12345;
          Order o = orderMap.get(orderId);  // boxes orderId → new Integer(12345)

          // GOOD alternatives:
          // 1. Use Agrona Int2ObjectHashMap (no boxing):
          //    Int2ObjectHashMap<Order> map = new Int2ObjectHashMap<>();
          //    map.get(orderId);  // no boxing, O(1)
          //
          // 2. Use array-based lookup (if IDs are bounded):
          //    Order[] orderById = new Order[MAX_ORDER_ID];
          //    orderById[orderId];  // O(1), no boxing, cache-friendly
          //
          // 3. Use Eclipse Collections IntObjectHashMap:
          //    IntObjectHashMap<Order> map = new IntObjectHashMap<>();


        ─── Pitfall 3: ArrayList<Integer> ──────────────────────────────────

          // BAD: every add() boxes the int
          List<Integer> prices = new ArrayList<>();
          prices.add(15025);  // new Integer(15025) each time

          // GOOD: Eclipse Collections / Trove / IntArrayList
          // org.eclipse.collections.impl.list.mutable.primitive.IntArrayList
          IntArrayList prices = new IntArrayList();
          prices.add(15025);   // stored as int, zero boxing

          // Or just use primitive array if size is known:
          int[] prices = new int[MAX_DEPTH];


        ─── Pitfall 4: Stream on primitives ────────────────────────────────

          int[] arr = {1, 2, 3, 4, 5};

          // BAD: boxes each element to Integer
          Stream<Integer> boxedStream = Arrays.stream(arr).boxed();
          int sum = boxedStream.mapToInt(Integer::intValue).sum();  // unbox again!

          // GOOD: IntStream — stays primitive throughout
          int sum = Arrays.stream(arr).sum();           // IntStream, no boxing
          int max = Arrays.stream(arr).max().getAsInt();
          int[] doubled = Arrays.stream(arr).map(x -> x * 2).toArray();


        ─── Pitfall 5: Null check on Integer can NPE ────────────────────────

          Integer[] arr = new Integer[10];   // all null initially!
          int val = arr[0];  // NullPointerException! unboxing null

          int[] arr = new int[10];           // all 0 initially — safe
          int val = arr[0];                  // 0, no NPE possible


        ─── Pitfall 6: == vs equals() on Integer ────────────────────────────

          Integer a = 127;
          Integer b = 127;
          System.out.println(a == b);    // true  (cached instance)

          Integer c = 128;
          Integer d = 128;
          System.out.println(c == d);    // false (different objects!)
          System.out.println(c.equals(d));  // true (correct comparison)

          // In trading: NEVER compare order IDs with ==
          // Always: orderId1.equals(orderId2) OR use int primitives
        """);
    }

    // =========================================================================
    // SECTION 4: ULL TRADING USE CASES — Concrete Examples
    // =========================================================================
    static void ullTradingUseCases() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 4: ULL Trading Use Cases — int[] vs Integer[]
        ══════════════════════════════════════════════════════════════════════

        ─── Use Case 1: Price Level Array (Order Book) ──────────────────────

          // C++ equivalent: int64_t bid_qty[TICK_RANGE];
          // Store bid/ask quantities at each price tick

          // BAD: Integer[] — GC scans 200,000 Integer objects per book update
          Integer[] bidQty = new Integer[200_000];

          // GOOD: int[] — one contiguous block, GC ignores values
          int[] bidQty = new int[200_000];   // 200K ticks × 4 bytes = 800KB
          long[] bidQtyLong = new long[200_000]; // if qty > Integer.MAX_VALUE

          // Access: O(1) by price tick, no pointer chasing
          int tick = (int)(price / TICK_SIZE);
          bidQty[tick] += fillQty;   // O(1), likely L1 cache hit


        ─── Use Case 2: Symbol ID → Price Mapping ───────────────────────────

          // C++ equivalent: double prices[MAX_SYMBOLS];
          // Map symbol integer ID → last price (no String hash on hot path)

          // BAD:
          HashMap<Integer, Double> lastPrice = new HashMap<>();  // 2x boxing

          // GOOD: direct-indexed primitive array
          double[] lastPrice  = new double[MAX_SYMBOLS];  // O(1) access
          int[]    lastQty    = new int[MAX_SYMBOLS];
          int[]    lastSide   = new int[MAX_SYMBOLS];

          // Usage:
          lastPrice[symbolId] = 150.25;   // zero overhead
          double p = lastPrice[symbolId]; // one array access


        ─── Use Case 3: Volume Curve (VWAP) ────────────────────────────────

          // C++ equivalent: double vol_curve[390];
          // 390 minute-bins of historical volume fractions

          // BAD:
          Double[] volCurve = new Double[390];   // 390 Double objects on heap

          // GOOD:
          double[] volCurve    = new double[390]; // 390 × 8 = 3120 bytes = 49 cache lines
          double[] cumCurve    = new double[390]; // cumulative sum
          double[] targetSlice = new double[390]; // target qty per bin

          // Fits entirely in L2 cache (typically 256KB–1MB)
          // VWAP engine reads entire curve with zero cache misses


        ─── Use Case 4: Order ID Free-List Pool ─────────────────────────────

          // C++ equivalent: int free_list[POOL_SIZE]; int head = 0;
          // Pre-allocated pool of order IDs, zero allocation on hot path

          // BAD:
          Deque<Integer> freeList = new ArrayDeque<>();  // boxes each ID

          // GOOD:
          int[] freeList = new int[MAX_ORDERS];
          int   freeHead = 0;
          // Pre-fill:
          for (int i = 0; i < MAX_ORDERS; i++) freeList[i] = i;

          // Acquire: O(1), zero allocation
          int orderId = freeList[freeHead++];
          // Release: O(1)
          freeList[--freeHead] = orderId;


        ─── Use Case 5: Circular Ring Buffer of Prices ──────────────────────

          // C++ equivalent: double ring[1024]; (power of 2 for fast modulo)

          // BAD:
          Double[] priceRing = new Double[1024];  // 1024 Double heap objects

          // GOOD:
          double[] priceRing = new double[1024];  // 8KB, fits in L1 cache (32KB)
          int      writePos  = 0;
          static final int MASK = 1023;  // 1024 - 1 (power of 2 trick)

          // Write:
          priceRing[writePos++ & MASK] = newPrice;  // O(1), likely L1 hit
          // Read last N:
          double last = priceRing[(writePos - 1) & MASK];  // O(1)


        ─── Use Case 6: Greeks Array (Options) ──────────────────────────────

          // Store delta/gamma/vega for a strip of options (100 strikes)

          // BAD: array of Option objects — pointer chasing on each access
          OptionData[] options = new OptionData[100];

          // GOOD: Structure of Arrays (SoA) — each array is contiguous
          double[] delta  = new double[100];   // scan all deltas: 1 cache line
          double[] gamma  = new double[100];   // scan all gammas: 1 cache line
          double[] vega   = new double[100];
          double[] theta  = new double[100];
          int[]    strikes = new int[100];     // integer strikes (× tick_size)

          // Total hedge delta: sequential scan, all cache-hot
          double totalDelta = 0;
          for (int i = 0; i < 100; i++) totalDelta += delta[i] * position[i];


        ─── Use Case 7: Tick Buffer (Last N Prints) ─────────────────────────

          // Keep last 1000 trade prints for VWAP volume calculation

          // BAD: LinkedList<TradeData> — pointer chasing, GC pressure
          // MEDIUM: ArrayList<TradeData> — better, but still boxed doubles

          // GOOD: parallel primitive arrays
          long[]   tickTimestamps = new long  [1000];
          int[]    tickPrices     = new int   [1000];  // as integer ticks
          long[]   tickVolumes    = new long  [1000];
          int      tickHead       = 0;
          static final int TICK_MASK = 999;  // NOT power of 2 here, use %

          void onTrade(long ts, int priceTick, long vol) {
              int slot = tickHead % 1000;
              tickTimestamps[slot] = ts;
              tickPrices    [slot] = priceTick;
              tickVolumes   [slot] = vol;
              tickHead++;
          }
        """);
    }

    // =========================================================================
    // SECTION 5: WHEN TO USE Integer[] — Valid Use Cases
    // =========================================================================
    static void whenToUseObjectArray() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 5: When Integer[] IS Appropriate (Non-Hot Paths)
        ══════════════════════════════════════════════════════════════════════

        ✅ Use Integer[] (or List<Integer>) when:

        1. You need null values to represent "no data" / "missing"
           Integer[] arr = new Integer[10];  // arr[5] = null means "no order here"
           int[]     arr = new int[10];      // arr[5] = 0 — ambiguous (0 = valid price?)

        2. You need to use with generics:
           List<Integer> sortedIds = new ArrayList<>();  // Collections API requires Object
           Collections.sort(sortedIds);                   // needs Comparable<Integer>

        3. Config / startup data (loaded once, never on hot path):
           List<Integer> allowedPorts = Arrays.asList(9090, 9091, 9092);

        4. You need stream operations with collectors:
           List<Integer> filtered = intList.stream()
               .filter(x -> x > 100)
               .collect(Collectors.toList());  // startup only

        5. Interop with third-party APIs that require Object types:
           SomeLibrary.process(Integer[] params);  // you have no choice

        6. When the array size is tiny (< 10 elements) and not on hot path:
           Integer[] supportedSides = {0, 1};  // BUY/SELL — allocated once

        ❌ NEVER use Integer[] (or boxed collections) for:
          - Price arrays (bid/ask levels)
          - Volume curves (VWAP bins)
          - Order ID pools / free lists
          - Ring buffers of prices/quantities
          - Symbol → price mappings
          - Greeks arrays
          - Anything called on every market data tick
        """);
    }

    // =========================================================================
    // SECTION 6: COLLECTIONS — Primitive Collections Libraries
    // =========================================================================
    static void primitiveCollections() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 6: Primitive Collection Libraries (No Boxing)
        ══════════════════════════════════════════════════════════════════════

        Standard Java collections (List, Map, Set) all require Object types.
        These libraries provide primitive-typed collections:

        ┌──────────────────────────────┬───────────────────────────────────────────┐
        │ Library                      │ Key Classes                               │
        ├──────────────────────────────┼───────────────────────────────────────────┤
        │ Eclipse Collections          │ IntArrayList, IntHashSet,                 │
        │ (org.eclipse.collections)    │ IntIntHashMap, IntObjectHashMap           │
        ├──────────────────────────────┼───────────────────────────────────────────┤
        │ Agrona (Real Logic)          │ Int2IntHashMap, Int2ObjectHashMap,        │
        │ (org.agrona)                 │ Long2LongHashMap, IntHashSet              │
        ├──────────────────────────────┼───────────────────────────────────────────┤
        │ Koloboke                     │ IntIntMap, IntObjMap, LongLongMap         │
        │ (com.koloboke)               │ Very fast open-addressing hash maps       │
        ├──────────────────────────────┼───────────────────────────────────────────┤
        │ HPPC (High Perf Primitive    │ IntArrayList, IntIntHashMap,              │
        │ Collections)                 │ IntObjectHashMap                          │
        └──────────────────────────────┴───────────────────────────────────────────┘

        ─── Agrona Int2ObjectHashMap — No boxing for int keys ────────────────

          // C++ equivalent: std::unordered_map<int, Order*>
          // Maven: org.agrona:agrona:1.21.0

          import org.agrona.collections.Int2ObjectHashMap;

          // BAD (standard Java):
          Map<Integer, Order> byId = new HashMap<>();  // boxes every key lookup
          byId.get(orderId);   // Integer.valueOf(orderId) on every call

          // GOOD (Agrona):
          Int2ObjectHashMap<Order> byId = new Int2ObjectHashMap<>();
          byId.put(orderId, order);    // no boxing
          Order o = byId.get(orderId); // no boxing, open addressing, cache-friendly


        ─── Eclipse Collections IntArrayList ────────────────────────────────

          // C++ equivalent: std::vector<int>
          import org.eclipse.collections.impl.list.mutable.primitive.IntArrayList;

          // BAD:
          List<Integer> prices = new ArrayList<>();
          prices.add(15025);   // new Integer(15025)

          // GOOD:
          IntArrayList prices = new IntArrayList(1000);
          prices.add(15025);   // stored as int, no object creation
          int p = prices.get(0);  // returns primitive int


        ─── IntStream — Primitive stream (no boxing) ────────────────────────

          int[] arr = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

          // All primitive, no boxing:
          int  sum  = IntStream.of(arr).sum();
          int  max  = IntStream.of(arr).max().getAsInt();
          int  min  = IntStream.of(arr).min().getAsInt();
          long cnt  = IntStream.of(arr).filter(x -> x > 5).count();
          int[] dbl = IntStream.of(arr).map(x -> x * 2).toArray();

          // Range (like Python range):
          int sum = IntStream.range(0, 100).sum();         // 0+1+...+99
          int sum = IntStream.rangeClosed(1, 100).sum();   // 1+2+...+100

          // BAD (auto-boxing):
          Stream<Integer> boxed = Arrays.stream(arr).boxed();  // avoid
        """);
    }

    // =========================================================================
    // SECTION 7: COMPLETE TRADING SYSTEM DATA STRUCTURE GUIDE
    // =========================================================================
    static void tradingDataStructureGuide() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         SECTION 7: Complete Trading System — Right Array Types
        ══════════════════════════════════════════════════════════════════════

        Component                  Type              Reason
        ─────────────────────────  ────────────────  ──────────────────────────────────
        Order book bid qty levels  long[]            fits > 2B qty, cache line packed
        Order book ask qty levels  long[]            same
        Price levels (ticks)       int[]             integer ticks, 4 bytes each
        Order count per level      int[]             always positive
        Order ID pool / free-list  int[]             pre-alloc, zero alloc hot path
        VWAP volume curve          double[390]       390 bins, fits in L2 cache
        VWAP cumulative curve      double[390]       parallel array
        Symbol → last price        double[MAX_SYM]   direct index, O(1) O(1)
        Symbol → last qty          long[MAX_SYM]     same
        Symbol → tick size         double[MAX_SYM]   same
        Greeks (delta strip)       double[NUM_OPTS]  SoA layout, scan efficiently
        Greeks (gamma strip)       double[NUM_OPTS]  parallel to delta
        Tick buffer timestamps     long[BUF_SIZE]    RDTSC nanoseconds
        Tick buffer prices         int[BUF_SIZE]     as integer ticks
        Tick buffer volumes        long[BUF_SIZE]    qty
        Ring buffer (prices)       double[power-of-2] & mask = fast modulo
        Sequence numbers           long[]            always 64-bit
        Fill quantities            long[]            fits large block trades
        Execution IDs              int[]             fits in int (< 2B per day)
        Side flags (BUY=1,SELL=0)  byte[] or int[]   1 or 4 bytes per entry
        PnL per symbol             double[MAX_SYM]   signed, fractional
        Position per symbol        long[MAX_SYM]     signed, can be negative (short)

        ─── The Golden Rule for ULL Java ────────────────────────────────────

          1. Use int[], long[], double[] for ALL hot-path data
          2. Never Integer[], Long[], Double[] on hot path
          3. Never HashMap<Integer,...> on hot path → use int[MAX] or Agrona
          4. Structure of Arrays (SoA) beats Array of Structures (AoS):
             // SoA (GOOD): scan all prices without touching other fields
             double[] prices = ...; long[] qtys = ...; int[] sides = ...;

             // AoS (BAD for cache): loading price also loads unused fields
             class Level { double price; long qty; int side; }
             Level[] levels = ...;

          5. Size matters: if array fits in L1 (32KB) → near-zero latency
             int[8000] = 32KB → L1 hit (< 1ns)
             double[4000] = 32KB → L1 hit
             double[390]  = 3KB  → deep L1 resident

          6. Pre-allocate at startup, reuse always:
             static final double[] VWAP_CURVE = new double[390]; // once
             Arrays.fill(VWAP_CURVE, 0.0);  // reset, don't reallocate

        ─── Memory size quick reference ──────────────────────────────────

          Type      Size/element  1K elements  1M elements
          ────────  ────────────  ───────────  ───────────
          byte[]    1 byte        1 KB         1 MB
          short[]   2 bytes       2 KB         2 MB
          int[]     4 bytes       4 KB         4 MB
          long[]    8 bytes       8 KB         8 MB
          float[]   4 bytes       4 KB         4 MB
          double[]  8 bytes       8 KB         8 MB
          boolean[] 1 byte (JVM)  1 KB         1 MB
          Integer[] 4+16 bytes    ~20 KB       ~20 MB  (ref + heap object)
          Long[]    8+16 bytes    ~24 KB       ~24 MB

          L1 cache: ~32KB  → keep hot arrays under 8K elements (int[]) or 4K (long[])
          L2 cache: ~256KB → keep working set under 64K elements (int[])
          L3 cache: ~8MB   → all trading arrays typically fit here
        """);
    }

    // =========================================================================
    // SECTION 8: WORKING ULL ORDER BOOK DEMO (all primitive arrays)
    // =========================================================================
    static class PrimitiveOrderBook {

        // All primitive — zero boxing, zero GC pressure
        private final long[]  bidQty;      // C++: int64_t bid_qty[TICK_RANGE]
        private final long[]  askQty;      // C++: int64_t ask_qty[TICK_RANGE]
        private final int[]   bidCount;    // order count per level
        private final int[]   askCount;
        private final int     tickRange;
        private int           bestBidTick = -1;
        private int           bestAskTick = Integer.MAX_VALUE;

        static final double TICK_SIZE = 0.01;

        PrimitiveOrderBook(int tickRange) {
            this.tickRange = tickRange;
            this.bidQty    = new long[tickRange];
            this.askQty    = new long[tickRange];
            this.bidCount  = new int [tickRange];
            this.askCount  = new int [tickRange];
        }

        // O(1) price → tick conversion (integer arithmetic only)
        int toTick(double price) {
            return (int) Math.round(price / TICK_SIZE);
        }

        // Add order — O(1), zero allocation
        void addBid(double price, long qty) {
            int tick = toTick(price);
            bidQty  [tick] += qty;
            bidCount[tick]++;
            if (tick > bestBidTick) bestBidTick = tick;
        }

        void addAsk(double price, long qty) {
            int tick = toTick(price);
            askQty  [tick] += qty;
            askCount[tick]++;
            if (tick < bestAskTick) bestAskTick = tick;
        }

        // Cancel — O(1)
        void cancelBid(double price, long qty) {
            int tick = toTick(price);
            bidQty  [tick] -= qty;
            bidCount[tick]--;
            if (bidQty[tick] <= 0 && tick == bestBidTick) {
                // Walk down to find new best bid
                while (bestBidTick >= 0 && bidQty[bestBidTick] <= 0)
                    bestBidTick--;
            }
        }

        double bestBid() { return bestBidTick >= 0 ? bestBidTick * TICK_SIZE : 0; }
        double bestAsk() { return bestAskTick < tickRange ? bestAskTick * TICK_SIZE : 0; }
        double spread()  { return bestAsk() - bestBid(); }
        long   bestBidQty() { return bestBidTick >= 0 ? bidQty[bestBidTick] : 0; }
        long   bestAskQty() { return bestAskTick < tickRange ? askQty[bestAskTick] : 0; }

        static void demo() {
            System.out.println("\n=== SECTION 8: Primitive Array Order Book Demo ===");

            PrimitiveOrderBook book = new PrimitiveOrderBook(200_000);

            // Add some orders
            book.addBid(150.00, 1000);
            book.addBid(150.01, 500);
            book.addBid(149.99, 2000);
            book.addAsk(150.02, 800);
            book.addAsk(150.03, 1200);
            book.addAsk(150.04, 600);

            System.out.printf("  Best bid: %.2f x %,d%n", book.bestBid(), book.bestBidQty());
            System.out.printf("  Best ask: %.2f x %,d%n", book.bestAsk(), book.bestAskQty());
            System.out.printf("  Spread:   %.4f%n", book.spread());

            book.cancelBid(150.01, 500);
            System.out.printf("  After cancel: best bid %.2f x %,d%n",
                book.bestBid(), book.bestBidQty());

            // Memory usage
            long bookMem = (long)(200_000) * (8 + 8 + 4 + 4); // 4 arrays × elements
            System.out.printf("  Book memory: ~%,d KB (all primitive, zero GC objects)%n",
                bookMem / 1024);
        }
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) {
        System.out.println("=".repeat(70));
        System.out.println("  Java int[] vs Integer[] — ULL Trading Complete Guide");
        System.out.println("=".repeat(70));

        memoryLayoutExplained();
        Benchmark.run();
        autoboxingPitfalls();
        ullTradingUseCases();
        whenToUseObjectArray();
        primitiveCollections();
        tradingDataStructureGuide();
        PrimitiveOrderBook.demo();

        System.out.println("\n=".repeat(70));
        System.out.println("  Summary");
        System.out.println("=".repeat(70));
        System.out.println("""
          int[]     → packed memory, GC-invisible values, O(1) cache-friendly
          Integer[] → pointer array + heap objects, GC overhead, cache-hostile

          RULE: In ULL Java trading, treat Integer[] the same way C++ treats
                std::vector<std::unique_ptr<int>> — never use it on hot paths.
                Use int[], long[], double[] everywhere in the trading engine.
          """);

        System.out.printf("%nJava: %s | Processors: %d%n",
            System.getProperty("java.version"),
            Runtime.getRuntime().availableProcessors());
        System.out.println("✅ Complete.");
    }
}

