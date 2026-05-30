/**
 * ============================================================================
 * Java Off-Heap Memory & Object Pooling — Complete Guide
 * ============================================================================
 *
 * PROBLEM:
 *   Every Java 'new' puts objects on heap → GC scans them → Stop-The-World
 *   In ULL trading: a 50ms GC pause = missed fills, losses
 *
 * SOLUTIONS:
 *   ┌─────────────────────────────────────────────────────────────────────┐
 *   │ OFF-HEAP (GC never sees it)    │ ON-HEAP (GC sees it, pool reuses) │
 *   ├────────────────────────────────┼───────────────────────────────────┤
 *   │ 1. ByteBuffer.allocateDirect() │ 5. Object Pool (manual freelist)  │
 *   │ 2. sun.misc.Unsafe             │ 6. ArrayDeque pool                │
 *   │ 3. MemorySegment (Java 22+)    │ 7. ThreadLocal pool               │
 *   │ 4. Agrona UnsafeBuffer         │ 8. Disruptor ring buffer          │
 *   │    mmap / shared memory        │ 9. Apache Commons ObjectPool      │
 *   └────────────────────────────────┴───────────────────────────────────┘
 *
 * C++ equivalent mindset:
 *   Off-heap  = malloc/mmap  (you manage lifetime)
 *   Object pool = pre-allocated array + free-list (zero new on hot path)
 *
 * Build:
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/javac \
 *       Java_OffHeap_Memory_ObjectPool_Complete.java
 *   /Applications/CLion.app/Contents/jbr/Contents/Home/bin/java \
 *       Java_OffHeap_Memory_ObjectPool_Complete
 * ============================================================================
 */

import java.nio.*;
import java.nio.channels.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.function.*;

public class Java_OffHeap_Memory_ObjectPool_Complete {

    // =========================================================================
    // SECTION 1: ByteBuffer.allocateDirect() — Simplest Off-Heap
    // =========================================================================
    /**
     * C++ equivalent: void* buf = malloc(N);   // or mmap
     *
     * allocateDirect() allocates memory OUTSIDE the Java heap.
     * GC never touches this memory.
     * Freed when the DirectByteBuffer object is GC'd (NOT when data is freed).
     *
     * Best for: network I/O buffers, message encoding/decoding,
     *           fixed-size trade message structs
     */
    static class DirectByteBufferDemo {

        // ── Layout a trade message struct directly in off-heap memory ────────
        // C++ equivalent:
        // struct TradeMsg { int64_t orderId; double price; int64_t qty; int32_t side; };
        // TradeMsg* msg = (TradeMsg*)malloc(sizeof(TradeMsg));

        static final int ORDER_ID_OFFSET = 0;   // long  (8 bytes)
        static final int PRICE_OFFSET    = 8;   // double(8 bytes)
        static final int QTY_OFFSET      = 16;  // long  (8 bytes)
        static final int SIDE_OFFSET     = 24;  // int   (4 bytes)
        static final int MSG_SIZE        = 28;

        static void demo() {
            System.out.println("\n=== 1. ByteBuffer.allocateDirect() ===");

            // Allocate 1000 trade messages off-heap — NO GC involvement
            // C++ equivalent: TradeMsg* pool = (TradeMsg*)malloc(1000 * sizeof(TradeMsg));
            ByteBuffer offHeap = ByteBuffer.allocateDirect(1000 * MSG_SIZE);
            offHeap.order(ByteOrder.nativeOrder()); // match CPU byte order

            // Write a trade message at slot 0
            // C++ equivalent: pool[0].orderId = 12345L;
            int slot = 0;
            int base = slot * MSG_SIZE;
            offHeap.putLong  (base + ORDER_ID_OFFSET, 12345L);
            offHeap.putDouble(base + PRICE_OFFSET,    150.25);
            offHeap.putLong  (base + QTY_OFFSET,      1000L);
            offHeap.putInt   (base + SIDE_OFFSET,     1);       // 1=BUY

            // Read back
            long   orderId = offHeap.getLong  (base + ORDER_ID_OFFSET);
            double price   = offHeap.getDouble(base + PRICE_OFFSET);
            long   qty     = offHeap.getLong  (base + QTY_OFFSET);
            int    side    = offHeap.getInt   (base + SIDE_OFFSET);

            System.out.printf("  Off-heap trade: orderId=%d price=%.2f qty=%d side=%s%n",
                orderId, price, qty, side == 1 ? "BUY" : "SELL");

            // Heap vs Direct comparison
            System.out.println("\n  Heap ByteBuffer:");
            ByteBuffer heapBuf = ByteBuffer.allocate(1024);
            System.out.println("    On JVM heap → GC scans it");

            System.out.println("  Direct ByteBuffer:");
            System.out.println("    Off heap → GC never scans content");
            System.out.println("    Freed when DirectByteBuffer obj is collected OR cleaner called");

            // Force release (don't wait for GC):
            // Direct memory is freed when the buffer is GC'd.
            // For explicit release use Cleaner (Java 9+):
            //   ((DirectBuffer) offHeap).cleaner().clean();
            //
            // Or in Java 9+ via:
            //   UnsafeAccess.invokeCleaner(offHeap);  // sun.misc.Unsafe.invokeCleaner

            System.out.printf("  Buffer capacity: %d bytes, isDirect: %b%n",
                offHeap.capacity(), offHeap.isDirect());
        }
    }

    // =========================================================================
    // SECTION 2: sun.misc.Unsafe — Raw Memory (Most Powerful Off-Heap)
    // =========================================================================
    /**
     * C++ equivalent: malloc() / free() / pointer arithmetic
     *
     * Unsafe.allocateMemory(n)  = malloc(n)
     * Unsafe.freeMemory(addr)   = free(ptr)
     * Unsafe.putLong(addr, val) = *(int64_t*)addr = val
     * Unsafe.getLong(addr)      = *(int64_t*)addr
     *
     * WARNING: No bounds checking. No GC. YOU manage the lifetime.
     *          Memory leak if you forget freeMemory().
     *
     * Used by: java.nio internals, Agrona, Disruptor, Chronicle Map
     */
    static class UnsafeOffHeapDemo {

        static sun.misc.Unsafe UNSAFE;

        static {
            try {
                Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
                f.setAccessible(true);
                UNSAFE = (sun.misc.Unsafe) f.get(null);
            } catch (Exception e) {
                throw new RuntimeException("Cannot get Unsafe", e);
            }
        }

        // ── Off-heap ring buffer using Unsafe ───────────────────────────────
        // C++ equivalent of: int64_t* ring = (int64_t*)malloc(SIZE * 8);
        static class UnsafeRingBuffer {
            private final long  baseAddr;    // raw memory address
            private final int   capacity;
            private final long  mask;
            private long writePos = 0;
            private long readPos  = 0;

            // sizeof(long) = 8 bytes per slot
            static final int ELEMENT_SIZE = Long.BYTES;

            UnsafeRingBuffer(int capacity) {
                if ((capacity & (capacity - 1)) != 0)
                    throw new IllegalArgumentException("Must be power of 2");
                this.capacity = capacity;
                this.mask     = capacity - 1;
                // Allocate off-heap: C++ = malloc(capacity * 8)
                this.baseAddr = UNSAFE.allocateMemory((long) capacity * ELEMENT_SIZE);
                // Zero-fill: C++ = memset(ptr, 0, size)
                UNSAFE.setMemory(baseAddr, (long) capacity * ELEMENT_SIZE, (byte) 0);
            }

            // Write value at writePos
            // C++ equivalent: ring[writePos & mask] = value;
            boolean offer(long value) {
                if (writePos - readPos >= capacity) return false; // full
                long addr = baseAddr + ((writePos & mask) * ELEMENT_SIZE);
                UNSAFE.putLong(addr, value);   // = *(int64_t*)addr = value
                writePos++;
                return true;
            }

            // Read value at readPos
            // C++ equivalent: return ring[readPos & mask];
            long poll() {
                if (readPos == writePos) return Long.MIN_VALUE; // empty sentinel
                long addr = baseAddr + ((readPos & mask) * ELEMENT_SIZE);
                long val  = UNSAFE.getLong(addr);   // = *(int64_t*)addr
                readPos++;
                return val;
            }

            // MUST call this! C++ equivalent: free(ring);
            void destroy() {
                UNSAFE.freeMemory(baseAddr);    // = free(ptr)
            }

            int size()      { return (int)(writePos - readPos); }
            boolean isEmpty(){ return writePos == readPos; }
        }

        // ── Off-heap struct: Order record ───────────────────────────────────
        // C++ equivalent:
        // struct Order { int64_t id; double price; int64_t qty; int32_t side; };
        static class UnsafeOrderRecord {
            // Field offsets within the struct
            static final int OFFSET_ID    = 0;   // 8 bytes
            static final int OFFSET_PRICE = 8;   // 8 bytes
            static final int OFFSET_QTY   = 16;  // 8 bytes
            static final int OFFSET_SIDE  = 24;  // 4 bytes
            static final int STRUCT_SIZE  = 32;  // padded to 8-byte boundary

            private final long baseAddr;  // raw pointer to this struct

            UnsafeOrderRecord() {
                // C++ equivalent: Order* o = (Order*)malloc(sizeof(Order));
                this.baseAddr = UNSAFE.allocateMemory(STRUCT_SIZE);
            }

            // Setters — C++ equivalent: o->id = id;
            void setId   (long id)    { UNSAFE.putLong  (baseAddr + OFFSET_ID,    id);    }
            void setPrice(double p)   { UNSAFE.putDouble(baseAddr + OFFSET_PRICE, p);     }
            void setQty  (long qty)   { UNSAFE.putLong  (baseAddr + OFFSET_QTY,   qty);   }
            void setSide (int side)   { UNSAFE.putInt   (baseAddr + OFFSET_SIDE,  side);  }

            // Getters — C++ equivalent: return o->price;
            long   getId()    { return UNSAFE.getLong  (baseAddr + OFFSET_ID);    }
            double getPrice() { return UNSAFE.getDouble(baseAddr + OFFSET_PRICE); }
            long   getQty()   { return UNSAFE.getLong  (baseAddr + OFFSET_QTY);   }
            int    getSide()  { return UNSAFE.getInt   (baseAddr + OFFSET_SIDE);  }

            // C++ equivalent: free(o);
            void free() { UNSAFE.freeMemory(baseAddr); }
        }

        // ── Unsafe array of structs (pre-allocated flat array) ───────────────
        // C++ equivalent: Order* orders = new Order[N];  (stack or heap array)
        static class UnsafeOrderArray {
            private final long baseAddr;
            private final int  capacity;

            UnsafeOrderArray(int capacity) {
                this.capacity = capacity;
                // C++ equivalent: Order* arr = (Order*)malloc(N * sizeof(Order));
                this.baseAddr = UNSAFE.allocateMemory(
                    (long) capacity * UnsafeOrderRecord.STRUCT_SIZE);
                UNSAFE.setMemory(baseAddr,
                    (long) capacity * UnsafeOrderRecord.STRUCT_SIZE, (byte) 0);
            }

            private long addrOf(int idx) {
                return baseAddr + (long) idx * UnsafeOrderRecord.STRUCT_SIZE;
            }

            // Write: C++ equivalent: arr[idx].price = price;
            void setPrice(int idx, double price) {
                UNSAFE.putDouble(addrOf(idx) + UnsafeOrderRecord.OFFSET_PRICE, price);
            }
            void setId   (int idx, long id)   {
                UNSAFE.putLong(addrOf(idx) + UnsafeOrderRecord.OFFSET_ID, id);
            }
            void setQty  (int idx, long qty)  {
                UNSAFE.putLong(addrOf(idx) + UnsafeOrderRecord.OFFSET_QTY, qty);
            }

            // Read: C++ equivalent: return arr[idx].price;
            double getPrice(int idx) {
                return UNSAFE.getDouble(addrOf(idx) + UnsafeOrderRecord.OFFSET_PRICE);
            }
            long   getId(int idx) {
                return UNSAFE.getLong(addrOf(idx) + UnsafeOrderRecord.OFFSET_ID);
            }

            void destroy() { UNSAFE.freeMemory(baseAddr); }
        }

        static void demo() {
            System.out.println("\n=== 2. sun.misc.Unsafe — Raw Off-Heap Memory ===");

            // Ring buffer
            UnsafeRingBuffer ring = new UnsafeRingBuffer(1024);
            ring.offer(100L);
            ring.offer(200L);
            ring.offer(300L);
            System.out.printf("  Ring: %d %d %d (size=%d)%n",
                ring.poll(), ring.poll(), ring.poll(), ring.size());
            ring.destroy(); // = free()

            // Single struct
            UnsafeOrderRecord order = new UnsafeOrderRecord();
            order.setId(9999L);
            order.setPrice(202.50);
            order.setQty(500L);
            order.setSide(0);
            System.out.printf("  Order: id=%d price=%.2f qty=%d side=%s%n",
                order.getId(), order.getPrice(), order.getQty(),
                order.getSide() == 0 ? "SELL" : "BUY");
            order.free(); // = free()

            // Array of 10 orders
            UnsafeOrderArray arr = new UnsafeOrderArray(10);
            for (int i = 0; i < 10; i++) {
                arr.setId(i, i * 1000L);
                arr.setPrice(i, 100.0 + i * 0.25);
            }
            System.out.printf("  OrderArray[5]: id=%d price=%.2f%n",
                arr.getId(5), arr.getPrice(5));
            arr.destroy(); // = free()

            System.out.println("  ✅ All Unsafe memory freed explicitly (no GC needed)");
        }
    }

    // =========================================================================
    // SECTION 3: MemorySegment (Java 22+ Foreign Memory API)
    // =========================================================================
    /**
     * Modern replacement for Unsafe. Safe, bounded, lifecycle-managed.
     * C++ equivalent: unique_ptr<T> with custom allocator
     *
     * Java 22+: java.lang.foreign.MemorySegment (stable API)
     * Java 19–21: preview feature
     *
     * Advantages over Unsafe:
     *   - Bounded: access outside segment = exception (not crash)
     *   - Scoped: auto-freed when Arena closes (like unique_ptr / RAII)
     *   - Type-safe: VarHandle layout descriptors
     */
    static class MemorySegmentDemo {

        static void printCode() {
            System.out.println("""

            === 3. MemorySegment (Java 22+ Foreign Memory API) ===

            import java.lang.foreign.*;
            import java.lang.invoke.*;

            // Define struct layout (like C struct definition)
            // C++: struct Order { int64_t id; double price; int64_t qty; };
            StructLayout ORDER_LAYOUT = MemoryLayout.structLayout(
                ValueLayout.JAVA_LONG.withName("id"),       // 8 bytes
                ValueLayout.JAVA_DOUBLE.withName("price"),  // 8 bytes
                ValueLayout.JAVA_LONG.withName("qty")       // 8 bytes
            );

            // VarHandles = typed accessors (like C++ struct member pointers)
            VarHandle VH_ID    = ORDER_LAYOUT.varHandle(
                MemoryLayout.PathElement.groupElement("id"));
            VarHandle VH_PRICE = ORDER_LAYOUT.varHandle(
                MemoryLayout.PathElement.groupElement("price"));
            VarHandle VH_QTY   = ORDER_LAYOUT.varHandle(
                MemoryLayout.PathElement.groupElement("qty"));

            // ── Allocate single struct (auto-freed on try-close) ────────────
            // C++ equivalent: { auto order = std::make_unique<Order>(); }
            try (Arena arena = Arena.ofConfined()) {
                MemorySegment seg = arena.allocate(ORDER_LAYOUT);

                VH_ID.set(seg, 0L, 12345L);       // seg->id    = 12345
                VH_PRICE.set(seg, 0L, 150.25);    // seg->price = 150.25
                VH_QTY.set(seg, 0L, 1000L);       // seg->qty   = 1000

                long   id    = (long)   VH_ID.get(seg, 0L);
                double price = (double) VH_PRICE.get(seg, 0L);
                System.out.printf("  Segment: id=%d price=%.2f%n", id, price);
            } // ← arena closed here → memory freed (RAII)

            // ── Allocate array of 1000 orders ───────────────────────────────
            // C++ equivalent: Order* orders = new Order[1000];
            try (Arena arena = Arena.ofShared()) {  // ofShared = multi-thread safe
                MemorySegment array = arena.allocateArray(ORDER_LAYOUT, 1000);

                // Write order at index 5
                long offset = 5 * ORDER_LAYOUT.byteSize();
                VH_ID.set(array, offset, 5000L);
                VH_PRICE.set(array, offset, 200.0);

                // Read back
                long id5 = (long) VH_ID.get(array, offset);
                System.out.printf("  Array[5].id = %d%n", id5);
            }

            // ── Memory-mapped file (shared memory) ──────────────────────────
            // C++ equivalent: void* p = mmap(NULL, size, PROT_RW, MAP_SHARED, fd, 0);
            try (Arena arena = Arena.ofConfined()) {
                FileChannel fc = FileChannel.open(
                    Path.of("/dev/shm/trading_orders"),
                    StandardOpenOption.READ, StandardOpenOption.WRITE,
                    StandardOpenOption.CREATE);
                MemorySegment shm = fc.map(
                    FileChannel.MapMode.READ_WRITE, 0, 4096, arena);
                // Now shm is shared memory — visible to other processes
            }

            Arena types:
              Arena.ofConfined()     → single-thread, fastest
              Arena.ofShared()       → multi-thread safe
              Arena.ofAuto()         → GC-managed (like unique_ptr with GC)
              Arena.global()         → never freed (like static allocation)
            """);
        }
    }

    // =========================================================================
    // SECTION 4: Agrona UnsafeBuffer — Production Off-Heap (Used by LMAX)
    // =========================================================================
    /**
     * Agrona (Real Logic) is the standard library for off-heap in Java trading.
     * UnsafeBuffer wraps Unsafe or DirectByteBuffer with a clean typed API.
     *
     * Maven: org.agrona:agrona:1.21.0
     *
     * Used by: Aeron, SBE (Simple Binary Encoding), LMAX Disruptor
     */
    static class AgronaDemo {

        static void printCode() {
            System.out.println("""

            === 4. Agrona UnsafeBuffer — Production Off-Heap ===

            Maven: <dependency>
                     <groupId>org.agrona</groupId>
                     <artifactId>agrona</artifactId>
                     <version>1.21.0</version>
                   </dependency>

            import org.agrona.concurrent.UnsafeBuffer;
            import org.agrona.concurrent.AtomicBuffer;

            // ── Allocate off-heap buffer ─────────────────────────────────────
            // C++ equivalent: uint8_t* buf = (uint8_t*)malloc(4096);
            UnsafeBuffer buffer = new UnsafeBuffer(
                ByteBuffer.allocateDirect(4096));    // off-heap backing

            // Or directly from address:
            // UnsafeBuffer buffer = new UnsafeBuffer(address, length);

            // ── Write typed values at byte offsets ───────────────────────────
            int offset = 0;
            buffer.putLong  (offset,      123456789L);   offset += 8;  // order ID
            buffer.putDouble(offset,      150.25);        offset += 8;  // price
            buffer.putLong  (offset,      1000L);         offset += 8;  // qty
            buffer.putInt   (offset,      1);             offset += 4;  // side
            buffer.putStringWithoutLengthAscii(offset, "SPY");         // symbol

            // ── Read back ────────────────────────────────────────────────────
            long   orderId = buffer.getLong  (0);
            double price   = buffer.getDouble(8);
            long   qty     = buffer.getLong  (16);
            int    side    = buffer.getInt   (24);
            String symbol  = buffer.getStringWithoutLengthAscii(28, 3);

            System.out.printf("Order: %d %.2f %d %s %s%n",
                orderId, price, qty, side==1?"BUY":"SELL", symbol);

            // ── Atomic operations (lock-free) ────────────────────────────────
            AtomicBuffer atomic = new UnsafeBuffer(ByteBuffer.allocateDirect(64));
            atomic.putLongOrdered(0, 0L);                 // ordered write
            long cur = atomic.getLongVolatile(0);         // volatile read
            boolean ok = atomic.compareAndSetLong(0, cur, cur + 1); // CAS

            // ── Zero-copy message encoding with SBE ──────────────────────────
            // SBE (Simple Binary Encoding) writes directly to UnsafeBuffer
            // No object creation on hot path:
            //   OrderEncoder encoder = new OrderEncoder();
            //   encoder.wrap(buffer, 0).orderId(123).price().mantissa(15025).exponent(-2);
            """);
        }
    }

    // =========================================================================
    // SECTION 5: Object Pool — On-Heap, Zero New() on Hot Path
    // =========================================================================
    /**
     * C++ equivalent: pre-allocated std::array<Order, N> + free-list index
     *
     * Objects stay ON heap but are REUSED — GC pressure drops to near zero
     * because no new objects are created (only old ones are recycled).
     *
     * Key insight: GC pause = proportional to LIVE object count + allocation rate
     *              Pool = fixed live count + zero allocation on hot path
     */
    static class ObjectPoolDemo {

        // ── The pooled object (reusable) ────────────────────────────────────
        static class Order {
            long   orderId;
            double price;
            long   qty;
            int    side;
            String symbol;
            boolean active;

            // Reset method — called when returned to pool (like placement-new in C++)
            void reset() {
                orderId = 0; price = 0; qty = 0; side = 0;
                symbol = null; active = false;
            }
        }

        // ── Generic Object Pool (ArrayDeque-backed) ──────────────────────────
        /**
         * C++ equivalent:
         *   template<typename T, int N>
         *   class ObjectPool {
         *       std::array<T, N> storage_;
         *       std::stack<T*>   free_list_;
         *       T* acquire() { return free_list_.pop(); }
         *       void release(T* obj) { obj->reset(); free_list_.push(obj); }
         *   };
         */
        static class ObjectPool<T> {
            private final ArrayDeque<T> pool;
            private final Supplier<T>  factory;
            private final Consumer<T>  resetFn;
            private final int          maxSize;

            // Stats
            private final AtomicLong acquireCount = new AtomicLong();
            private final AtomicLong newAllocCount = new AtomicLong(); // should be near 0 on hot path

            ObjectPool(int initialSize, int maxSize,
                       Supplier<T> factory, Consumer<T> resetFn) {
                this.factory = factory;
                this.resetFn = resetFn;
                this.maxSize = maxSize;
                this.pool    = new ArrayDeque<>(initialSize);

                // Pre-allocate all objects at startup (NEVER on hot path)
                for (int i = 0; i < initialSize; i++) {
                    pool.push(factory.get());
                    newAllocCount.incrementAndGet();
                }
                System.out.printf("  ObjectPool: pre-allocated %d objects%n", initialSize);
            }

            // Acquire (= C++ pool.acquire() / free_list_.pop())
            T acquire() {
                acquireCount.incrementAndGet();
                T obj = pool.poll();
                if (obj == null) {
                    // Pool exhausted — create new (should not happen on hot path)
                    newAllocCount.incrementAndGet();
                    System.out.println("  ⚠️  Pool exhausted — new allocation (BAD on hot path)");
                    return factory.get();
                }
                return obj;
            }

            // Release (= C++ pool.release(obj) / free_list_.push(ptr))
            void release(T obj) {
                resetFn.accept(obj);          // zero out fields
                if (pool.size() < maxSize) {
                    pool.push(obj);           // back to pool
                }
                // If pool full: let GC collect (rare)
            }

            void printStats() {
                System.out.printf("  Pool stats: acquires=%d, new-allocs=%d, pool-size=%d%n",
                    acquireCount.get(), newAllocCount.get(), pool.size());
            }
        }

        static void demo() {
            System.out.println("\n=== 5. Object Pool — On-Heap, Zero new() on Hot Path ===");

            // Create pool with 1000 pre-allocated Orders
            ObjectPool<Order> orderPool = new ObjectPool<>(
                1000,                       // initial size
                2000,                       // max size
                Order::new,                 // factory (called only at startup)
                o -> o.reset()              // reset function (called on release)
            );

            // Hot path: acquire → use → release (ZERO allocation)
            // C++ equivalent: Order* o = pool.acquire(); ... pool.release(o);
            for (int i = 0; i < 5; i++) {
                Order o = orderPool.acquire();        // O(1), no heap alloc
                o.orderId = 1000L + i;
                o.price   = 150.0 + i * 0.25;
                o.qty     = 100L;
                o.side    = i % 2;
                o.symbol  = "SPY";
                o.active  = true;

                System.out.printf("  Using order: id=%d price=%.2f%n", o.orderId, o.price);

                orderPool.release(o);                 // O(1), back to pool
            }

            orderPool.printStats();
            // new-allocs should = initial size (1000), not growing
        }
    }

    // =========================================================================
    // SECTION 6: Lock-Free SPSC Ring Buffer Pool (Zero Alloc, No Lock)
    // =========================================================================
    /**
     * C++ equivalent: T pool_[N]; std::atomic<uint64_t> write_, read_;
     *
     * Pre-allocate N objects in a ring. Producer acquires from one end,
     * consumer returns to other end. No mutex, no GC, cache-line friendly.
     * This is the pattern used by LMAX Disruptor.
     */
    static class SPSCObjectRingBuffer<T> {

        @SuppressWarnings("unchecked")
        private final T[]     ring;
        private final int     mask;

        // Separate cache lines to prevent false sharing
        // Producer writes to writePos
        @jdk.internal.vm.annotation.Contended
        private final AtomicLong writePos = new AtomicLong(0);
        // Consumer reads from readPos
        @jdk.internal.vm.annotation.Contended
        private final AtomicLong readPos  = new AtomicLong(0);

        @SuppressWarnings("unchecked")
        SPSCObjectRingBuffer(int capacity, Supplier<T> factory) {
            if ((capacity & (capacity - 1)) != 0)
                throw new IllegalArgumentException("Capacity must be power of 2");
            ring = (T[]) new Object[capacity];
            mask = capacity - 1;
            // Pre-allocate all objects once at startup
            for (int i = 0; i < capacity; i++) {
                ring[i] = factory.get();
            }
        }

        // Producer side: get next object to write into
        T claimNext() {
            long w = writePos.get();
            long r = readPos.get();
            if (w - r >= ring.length) return null; // full
            return ring[(int)(w & mask)];
        }

        void publish() {
            writePos.incrementAndGet();
        }

        // Consumer side: get next available object
        T poll() {
            long r = readPos.get();
            long w = writePos.get();
            if (r == w) return null;
            return ring[(int)(r & mask)];
        }

        void consume() {
            readPos.incrementAndGet();
        }

        static void demo() {
            System.out.println("\n=== 6. Lock-Free SPSC Ring Buffer (Pre-Allocated Objects) ===");

            // Pre-allocate 1024 Order objects in ring — zero alloc on hot path
            SPSCObjectRingBuffer<ObjectPoolDemo.Order> ring =
                new SPSCObjectRingBuffer<>(1024, ObjectPoolDemo.Order::new);

            // Producer: claim slot, fill in data, publish
            ObjectPoolDemo.Order slot = ring.claimNext();
            if (slot != null) {
                slot.orderId = 42L;
                slot.price   = 155.50;
                slot.qty     = 200L;
                slot.side    = 1;
                ring.publish();
            }

            // Consumer: poll, process, consume
            ObjectPoolDemo.Order consumed = ring.poll();
            if (consumed != null) {
                System.out.printf("  SPSC consumed: id=%d price=%.2f qty=%d%n",
                    consumed.orderId, consumed.price, consumed.qty);
                ring.consume();
            }

            System.out.println("  Zero allocations on hot path — objects pre-allocated in ring");
        }
    }

    // =========================================================================
    // SECTION 7: ThreadLocal Object Pool — Per-Thread Cache
    // =========================================================================
    /**
     * C++ equivalent: thread_local Order* local_pool[N];
     *
     * Each thread has its own pool — no contention between threads.
     * Much faster than shared pool (no CAS, no locks).
     * Pattern used for: StringBuilder, byte[] encode buffers, temp structs
     */
    static class ThreadLocalPoolDemo {

        // ── Thread-local byte array pool (for message encoding) ──────────────
        // C++ equivalent: thread_local uint8_t encode_buffer[4096];
        static final ThreadLocal<byte[]> ENCODE_BUFFER =
            ThreadLocal.withInitial(() -> new byte[4096]);

        // ── Thread-local Order pool ──────────────────────────────────────────
        static final ThreadLocal<ArrayDeque<ObjectPoolDemo.Order>> LOCAL_ORDER_POOL =
            ThreadLocal.withInitial(() -> {
                ArrayDeque<ObjectPoolDemo.Order> pool = new ArrayDeque<>(64);
                for (int i = 0; i < 64; i++) pool.push(new ObjectPoolDemo.Order());
                return pool;
            });

        static ObjectPoolDemo.Order acquireOrder() {
            ArrayDeque<ObjectPoolDemo.Order> pool = LOCAL_ORDER_POOL.get();
            ObjectPoolDemo.Order o = pool.poll();
            return (o != null) ? o : new ObjectPoolDemo.Order();
        }

        static void releaseOrder(ObjectPoolDemo.Order o) {
            o.reset();
            ArrayDeque<ObjectPoolDemo.Order> pool = LOCAL_ORDER_POOL.get();
            if (pool.size() < 64) pool.push(o);
        }

        static void demo() throws InterruptedException {
            System.out.println("\n=== 7. ThreadLocal Object Pool ===");

            Thread t1 = new Thread(() -> {
                // Each thread gets its OWN pool — zero contention
                ObjectPoolDemo.Order o = acquireOrder();
                o.orderId = 1001L;
                o.price   = 100.0;
                System.out.printf("  Thread %s: acquired order id=%d%n",
                    Thread.currentThread().getName(), o.orderId);
                releaseOrder(o);

                byte[] buf = ENCODE_BUFFER.get(); // thread-local byte array
                System.out.printf("  Thread %s: encode buffer %d bytes (no alloc)%n",
                    Thread.currentThread().getName(), buf.length);
            }, "trading-thread-1");

            Thread t2 = new Thread(() -> {
                ObjectPoolDemo.Order o = acquireOrder(); // completely separate pool from t1
                o.orderId = 2002L;
                o.price   = 200.0;
                System.out.printf("  Thread %s: acquired order id=%d%n",
                    Thread.currentThread().getName(), o.orderId);
                releaseOrder(o);
            }, "trading-thread-2");

            t1.start(); t2.start();
            t1.join();  t2.join();

            System.out.println("  No lock contention — each thread has own pool");
        }
    }

    // =========================================================================
    // SECTION 8: Memory-Mapped File (mmap) — Shared Off-Heap
    // =========================================================================
    /**
     * C++ equivalent: void* p = mmap(NULL, size, PROT_RW, MAP_SHARED, fd, 0);
     *
     * Use cases:
     *   - Shared memory IPC between Java processes (fastest IPC)
     *   - Persist order state to disk without serialization
     *   - Read exchange feed (another process writes, you read)
     */
    static class MMapDemo {

        static void demo() {
            System.out.println("\n=== 8. Memory-Mapped File (mmap equivalent) ===");

            try {
                File f = File.createTempFile("trading_shm", ".dat");
                f.deleteOnExit();

                // Write via MappedByteBuffer
                try (RandomAccessFile raf = new RandomAccessFile(f, "rw");
                     FileChannel fc = raf.getChannel()) {

                    // C++ equivalent: void* p = mmap(NULL, 4096, PROT_RW, MAP_SHARED, fd, 0);
                    MappedByteBuffer mmap = fc.map(
                        FileChannel.MapMode.READ_WRITE, 0, 4096);
                    mmap.order(ByteOrder.nativeOrder());

                    // Write order book top-of-book into shared memory
                    mmap.putDouble(0,  150.25);   // best bid
                    mmap.putDouble(8,  150.26);   // best ask
                    mmap.putLong  (16, 5000L);    // bid qty
                    mmap.putLong  (24, 3000L);    // ask qty
                    mmap.force();                 // flush to disk/shared memory

                    // Read back (same process or different process)
                    double bestBid = mmap.getDouble(0);
                    double bestAsk = mmap.getDouble(8);
                    System.out.printf("  mmap BBO: bid=%.2f ask=%.2f spread=%.4f%n",
                        bestBid, bestAsk, bestAsk - bestBid);
                }

                System.out.println("  In production: use /dev/shm/<name> (tmpfs, RAM-backed)");
                System.out.println("  Example: new File(\"/dev/shm/order_book_bbo\")");

            } catch (Exception e) {
                System.out.println("  mmap demo: " + e.getMessage());
            }
        }
    }

    // =========================================================================
    // SECTION 9: Comparison Table — All Approaches
    // =========================================================================
    static void printComparisonTable() {
        System.out.println("""

        ══════════════════════════════════════════════════════════════════════
         COMPARISON: Off-Heap vs On-Heap Object Pool
        ══════════════════════════════════════════════════════════════════════

        ┌────────────────────────┬──────────┬──────────┬──────────┬─────────────────────────────┐
        │ Approach               │ GC?      │ Safety   │ Latency  │ Best Use Case               │
        ├────────────────────────┼──────────┼──────────┼──────────┼─────────────────────────────┤
        │ ByteBuffer.direct()    │ NO       │ Bounded  │ ~50ns    │ Network I/O, message bufs   │
        │ sun.misc.Unsafe        │ NO       │ NONE     │ ~10ns    │ Ring buffers, raw structs   │
        │ MemorySegment (J22+)   │ NO       │ Bounded  │ ~30ns    │ Modern replacement for Unsafe│
        │ Agrona UnsafeBuffer    │ NO       │ Medium   │ ~15ns    │ SBE encoding, Aeron IPC     │
        │ mmap / MappedByteBuffer│ NO       │ Bounded  │ ~100ns   │ IPC, persist order state    │
        ├────────────────────────┼──────────┼──────────┼──────────┼─────────────────────────────┤
        │ Object Pool (ArrayDeque)│ Minimal │ Full     │ ~20ns    │ Order/event objects         │
        │ SPSC Ring Buffer Pool  │ Minimal  │ Full     │ ~10ns    │ Producer→consumer pipeline  │
        │ ThreadLocal Pool       │ Minimal  │ Full     │ ~5ns     │ Per-thread encode buffers   │
        │ Disruptor RingBuffer   │ Minimal  │ Full     │ ~5ns     │ Event pipelines             │
        └────────────────────────┴──────────┴──────────┴──────────┴─────────────────────────────┘

        ══════════════════════════════════════════════════════════════════════
         DECISION GUIDE: Which to Use?
        ══════════════════════════════════════════════════════════════════════

        Scenario                                   Recommended Approach
        ─────────────────────────────────────────  ──────────────────────────
        Large buffers (GB scale), IPC              sun.misc.Unsafe / MemorySegment
        Network encode/decode buffers              ByteBuffer.allocateDirect() / Agrona
        Reuse trade/order message objects          Object Pool (ArrayDeque)
        Single-producer single-consumer pipeline   SPSC Ring Buffer Pool
        Per-thread temp buffers                    ThreadLocal Pool
        Exchange feeds via shared memory           mmap (/dev/shm)
        Production trading lib (most complete)     Agrona + SBE + Aeron
        Modern Java 22+, type-safe off-heap        MemorySegment + Arena

        ══════════════════════════════════════════════════════════════════════
         RULE: How to eliminate GC on hot path
        ══════════════════════════════════════════════════════════════════════

        1. Pre-allocate everything at startup (before first trade)
        2. NEVER call 'new' in the trading loop
           - Replace: new Order()     → pool.acquire()
           - Replace: new byte[N]     → ThreadLocal cached array
           - Replace: new String(...)  → pre-built symbol table
           - Replace: new ArrayList()  → pre-allocated List with clear()
        3. Use primitive types where possible (avoid Integer, Long boxing)
        4. Use off-heap for large buffers that outlive individual trades
        5. Use GC-friendly: ZGC or Shenandoah (sub-millisecond pause)

        ══════════════════════════════════════════════════════════════════════
         C++ → Java Equivalents
        ══════════════════════════════════════════════════════════════════════

        C++ (what you know)              Java Equivalent
        ───────────────────────────────  ─────────────────────────────────────
        malloc(N)                        Unsafe.allocateMemory(N)
        free(ptr)                        Unsafe.freeMemory(addr)
        mmap(NULL, N, PROT_RW, ...)      FileChannel.map() / MappedByteBuffer
        new T[N] (pre-allocated)         Object Pool (N objects pre-created)
        *(int64_t*)(addr+off)            Unsafe.getLong(addr + off)
        *(int64_t*)(addr+off) = val      Unsafe.putLong(addr + off, val)
        memset(ptr, 0, N)                Unsafe.setMemory(addr, N, (byte)0)
        memcpy(dst, src, N)              Unsafe.copyMemory(src, dst, N)
        pool.acquire() / pool.release()  pool.acquire() / pool.release()  (same pattern)
        thread_local T buf[N]            ThreadLocal<T> pool
        placement new / reset()          obj.reset() before returning to pool
        std::array<T,N> + free-list      ArrayDeque<T> + pre-alloc N objects
        alignas(64) struct               @Contended annotation (JVM-managed)
        """);
    }

    // =========================================================================
    // MAIN
    // =========================================================================
    public static void main(String[] args) throws Exception {
        System.out.println("=".repeat(70));
        System.out.println("  Java Off-Heap Memory & Object Pooling — Complete Guide");
        System.out.println("=".repeat(70));

        DirectByteBufferDemo.demo();
        UnsafeOffHeapDemo.demo();
        MemorySegmentDemo.printCode();
        AgronaDemo.printCode();
        ObjectPoolDemo.demo();
        SPSCObjectRingBuffer.demo();
        ThreadLocalPoolDemo.demo();
        MMapDemo.demo();
        printComparisonTable();

        System.out.println("\n--- Runtime Info ---");
        long totalMem  = Runtime.getRuntime().totalMemory() / (1024 * 1024);
        long freeMem   = Runtime.getRuntime().freeMemory()  / (1024 * 1024);
        long maxMem    = Runtime.getRuntime().maxMemory()   / (1024 * 1024);
        System.out.printf("Heap: used=%dMB free=%dMB max=%dMB%n",
            totalMem - freeMem, freeMem, maxMem);
        System.out.println("Java: " + System.getProperty("java.version"));

        System.out.println("\n✅ All demos complete.");
    }
}

