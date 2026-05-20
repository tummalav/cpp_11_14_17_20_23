/**
 * Java vs C++ Object-Oriented Programming Differences
 *
 * Complete comparison of OOP features that differ between Java and C++
 *
 * Topics Covered:
 * 1. Multiple Inheritance (C++ YES, Java NO - uses interfaces)
 * 2. Virtual Functions (Java all virtual, C++ explicit)
 * 3. Operator Overloading (C++ YES, Java NO)
 * 4. References (lvalue/rvalue in C++, hidden in Java)
 * 5. Value vs Reference Semantics
 * 6. Copy/Move Constructors (C++ YES, Java NO)
 * 7. Destructors (C++ YES, Java NO - has finalizers/GC)
 * 8. Memory Management (C++ manual, Java GC)
 * 9. Default Constructors
 * 10. Access Control Differences
 * 11. Abstract Classes and Interfaces
 * 12. Pointers (C++ YES, Java NO - references only)
 * 13. Templates vs Generics
 * 14. Friend Functions (C++ YES, Java NO)
 * 15. Inner/Nested Classes
 * 16. Static Members and Methods
 * 17. Final/Const Keyword Differences
 * 18. Method Overloading and Overriding
 * 19. This Pointer vs This Reference
 * 20. Summary and Complete Comparison
 *
 * Author: Complete OOP Comparison Guide
 * Date: February 8, 2026
 */

public class Java_vs_CPP_OOP_Differences {

    //=============================================================================
    // 1. MULTIPLE INHERITANCE
    //=============================================================================

    /**
     * C++: Supports MULTIPLE INHERITANCE (can inherit from multiple classes)
     * Java: NO multiple inheritance (can only extend ONE class)
     *       But can implement MULTIPLE interfaces
     */
    static void demonstrateMultipleInheritance() {
        System.out.println("╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  1. MULTIPLE INHERITANCE                                   ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Multiple Inheritance:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Trader { };");
        System.out.println("class Analyst { };");
        System.out.println("class QuantTrader : public Trader, public Analyst {");
        System.out.println("    // ✅ Inherits from BOTH Trader and Analyst");
        System.out.println("};");

        System.out.println("\nJava Approach (Single Inheritance + Interfaces):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("interface Tradable { void trade(); }");
        System.out.println("interface Analyzable { void analyze(); }");
        System.out.println("class Trader { }");
        System.out.println("class QuantTrader extends Trader implements Tradable, Analyzable {");
        System.out.println("    // ✅ Extends ONE class, implements MULTIPLE interfaces");
        System.out.println("}");

        // Java example
        QuantTrader qt = new QuantTrader();
        qt.trade();
        qt.analyze();

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Multiple class inheritance (can cause diamond problem)");
        System.out.println("• Java: Single class inheritance + multiple interfaces (avoids diamond problem)");

        System.out.println("\nDiamond Problem (C++ can have, Java avoids):");
        System.out.println("     Base");
        System.out.println("    /    \\");
        System.out.println("  Left  Right");
        System.out.println("    \\    /");
        System.out.println("   Derived");
        System.out.println("• C++:  Must use virtual inheritance to resolve");
        System.out.println("• Java: Impossible with classes, safe with interfaces");
    }

    // Java interfaces (workaround for multiple inheritance)
    interface Tradable {
        void trade();
    }

    interface Analyzable {
        void analyze();
    }

    static class Trader {
        protected String name;
        public void executeTrade() {
            System.out.println("Executing trade...");
        }
    }

    static class QuantTrader extends Trader implements Tradable, Analyzable {
        @Override
        public void trade() {
            System.out.println("Quant trading with algorithms");
        }

        @Override
        public void analyze() {
            System.out.println("Analyzing market data");
        }
    }

    //=============================================================================
    // 2. VIRTUAL FUNCTIONS
    //=============================================================================

    /**
     * C++: Methods are NON-VIRTUAL by default (must use 'virtual' keyword)
     * Java: ALL methods are VIRTUAL by default (except final, static, private)
     */
    static void demonstrateVirtualFunctions() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  2. VIRTUAL FUNCTIONS                                      ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ (Explicit virtual keyword required):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    void process() { }          // ❌ NOT virtual (static binding)");
        System.out.println("    virtual void execute() { }  // ✅ Virtual (dynamic binding)");
        System.out.println("};");
        System.out.println("class LimitOrder : public Order {");
        System.out.println("    void process() { }          // Hides base method (NOT override!)");
        System.out.println("    void execute() override { } // ✅ Overrides base method");
        System.out.println("};");

        System.out.println("\nJava (All methods virtual by default):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    void process() { }          // ✅ Virtual (dynamic binding)");
        System.out.println("    final void validate() { }   // ❌ NOT virtual (cannot override)");
        System.out.println("    static void count() { }     // ❌ NOT virtual (class method)");
        System.out.println("    private void internal() { } // ❌ NOT virtual (not inherited)");
        System.out.println("}");

        // Java demonstration
        Order order = new LimitOrder();
        order.process();  // Calls LimitOrder.process() (virtual dispatch)

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Opt-in polymorphism (declare virtual explicitly)");
        System.out.println("• Java: Opt-out polymorphism (all virtual except final/static/private)");

        System.out.println("\nPerformance:");
        System.out.println("• C++:  Non-virtual calls = direct call (faster, ~1ns)");
        System.out.println("        Virtual calls = vtable lookup (~5-10ns)");
        System.out.println("• Java: All calls through vtable (~5-10ns, but JIT optimizes)");
    }

    static class Order {
        public void process() {
            System.out.println("Order.process() called");
        }

        // final = NOT virtual (cannot be overridden)
        public final void validate() {
            System.out.println("Validating order...");
        }

        // static = NOT virtual (class method)
        public static void count() {
            System.out.println("Counting orders...");
        }

        // private = NOT virtual (not inherited)
        private void internal() {
            System.out.println("Internal method");
        }
    }

    static class LimitOrder extends Order {
        @Override
        public void process() {  // ✅ Overrides (virtual dispatch)
            System.out.println("LimitOrder.process() called");
        }

        // Cannot override final method
        // public void validate() { }  // ❌ Compile error!
    }

    //=============================================================================
    // 3. OPERATOR OVERLOADING
    //=============================================================================

    /**
     * C++: Supports OPERATOR OVERLOADING
     * Java: Does NOT support operator overloading (except String + for concatenation)
     */
    static void demonstrateOperatorOverloading() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  3. OPERATOR OVERLOADING                                   ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Operator Overloading (Supported):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Price {");
        System.out.println("    double value;");
        System.out.println("public:");
        System.out.println("    Price operator+(const Price& other) {");
        System.out.println("        return Price(value + other.value);");
        System.out.println("    }");
        System.out.println("    Price operator*(double multiplier) {");
        System.out.println("        return Price(value * multiplier);");
        System.out.println("    }");
        System.out.println("    bool operator<(const Price& other) {");
        System.out.println("        return value < other.value;");
        System.out.println("    }");
        System.out.println("};");
        System.out.println("// Usage:");
        System.out.println("Price p1(100.0), p2(50.0);");
        System.out.println("Price total = p1 + p2;           // ✅ Uses operator+");
        System.out.println("Price doubled = p1 * 2;          // ✅ Uses operator*");
        System.out.println("if (p1 < p2) { }                 // ✅ Uses operator<");

        System.out.println("\nJava Approach (No operator overloading, use methods):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Price {");
        System.out.println("    private double value;");
        System.out.println("    public Price add(Price other) {");
        System.out.println("        return new Price(value + other.value);");
        System.out.println("    }");
        System.out.println("    public Price multiply(double multiplier) {");
        System.out.println("        return new Price(value * multiplier);");
        System.out.println("    }");
        System.out.println("    public boolean lessThan(Price other) {");
        System.out.println("        return value < other.value;");
        System.out.println("    }");
        System.out.println("}");
        System.out.println("// Usage:");
        System.out.println("Price p1 = new Price(100.0), p2 = new Price(50.0);");
        System.out.println("Price total = p1.add(p2);        // ❌ Cannot use +");
        System.out.println("Price doubled = p1.multiply(2);  // ❌ Cannot use *");
        System.out.println("if (p1.lessThan(p2)) { }         // ❌ Cannot use <");

        // Java example
        Price p1 = new Price(100.0);
        Price p2 = new Price(50.0);
        Price total = p1.add(p2);
        System.out.println("\nJava result: " + p1.value + " + " + p2.value + " = " + total.value);

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Supports operator overloading (can make code cleaner)");
        System.out.println("• Java: NO operator overloading (must use explicit methods)");
        System.out.println("• Exception: String + is built-in for concatenation in Java");

        System.out.println("\nWhy Java doesn't have it:");
        System.out.println("• Simplicity: Avoid complex/confusing operator semantics");
        System.out.println("• Consistency: All operations are method calls");
        System.out.println("• Safety: No hidden behavior in operators");
    }

    static class Price {
        double value;

        public Price(double value) {
            this.value = value;
        }

        public Price add(Price other) {
            return new Price(this.value + other.value);
        }

        public Price multiply(double multiplier) {
            return new Price(this.value * multiplier);
        }

        public boolean lessThan(Price other) {
            return this.value < other.value;
        }
    }

    //=============================================================================
    // 4. REFERENCES (lvalue/rvalue)
    //=============================================================================

    /**
     * C++: Explicit lvalue references (&) and rvalue references (&&)
     * Java: All object variables are HIDDEN REFERENCES (no explicit syntax)
     */
    static void demonstrateReferences() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  4. REFERENCES (lvalue/rvalue)                             ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ References (Explicit):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Lvalue reference (refers to existing object)");
        System.out.println("Order order;");
        System.out.println("Order& ref = order;              // ✅ Lvalue reference");
        System.out.println("ref.process();                   // Same as order.process()");
        System.out.println();
        System.out.println("// Rvalue reference (for temporary objects, move semantics)");
        System.out.println("Order&& rref = createOrder();    // ✅ Rvalue reference");
        System.out.println("Order moved = std::move(order);  // ✅ Move semantics");
        System.out.println();
        System.out.println("// Function parameters");
        System.out.println("void process(Order& order);      // Pass by lvalue reference");
        System.out.println("void process(Order&& order);     // Pass by rvalue reference");

        System.out.println("\nJava References (Hidden/Implicit):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// ALL object variables are references (hidden)");
        System.out.println("Order order = new Order();       // 'order' is a REFERENCE");
        System.out.println("Order ref = order;               // 'ref' points to SAME object");
        System.out.println("ref.process();                   // Same object as 'order'");
        System.out.println();
        System.out.println("// NO rvalue references");
        System.out.println("// NO move semantics");
        System.out.println("// NO explicit reference syntax (&, &&)");
        System.out.println();
        System.out.println("// Function parameters (always pass by reference for objects)");
        System.out.println("void process(Order order);       // 'order' is a reference");

        // Java demonstration
        OrderObj o1 = new OrderObj(100);
        OrderObj o2 = o1;  // o2 references SAME object as o1
        o2.price = 200;
        System.out.println("\nJava reference demonstration:");
        System.out.println("o1.price: " + o1.price);  // 200 (same object!)
        System.out.println("o2.price: " + o2.price);  // 200

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Explicit references (&), rvalue references (&&)");
        System.out.println("        Can pass by value, reference, or pointer");
        System.out.println("• Java: ALL objects are references (hidden)");
        System.out.println("        NO lvalue/rvalue distinction");
        System.out.println("        NO move semantics");
        System.out.println("        Primitives are ALWAYS by value");
    }

    static class OrderObj {
        int price;

        public OrderObj(int price) {
            this.price = price;
        }
    }

    //=============================================================================
    // 5. VALUE vs REFERENCE SEMANTICS
    //=============================================================================

    /**
     * C++: Can choose value or reference semantics
     * Java: Primitives are VALUE, Objects are REFERENCE
     */
    static void demonstrateValueVsReference() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  5. VALUE vs REFERENCE SEMANTICS                           ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ (Flexible - can choose):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Value semantics (copy)");
        System.out.println("Order o1;");
        System.out.println("Order o2 = o1;              // ✅ COPY (separate objects)");
        System.out.println();
        System.out.println("// Reference semantics (alias)");
        System.out.println("Order& ref = o1;            // ✅ REFERENCE (same object)");
        System.out.println();
        System.out.println("// Pointer semantics");
        System.out.println("Order* ptr = &o1;           // ✅ POINTER (address)");
        System.out.println();
        System.out.println("// Heap allocation");
        System.out.println("Order* heap = new Order();  // ✅ Heap object (manual delete)");

        System.out.println("\nJava (Fixed - primitives value, objects reference):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("// Primitives: ALWAYS value semantics");
        System.out.println("int x = 10;");
        System.out.println("int y = x;                  // ✅ COPY (separate values)");
        System.out.println("y = 20;                     // x is still 10");
        System.out.println();
        System.out.println("// Objects: ALWAYS reference semantics");
        System.out.println("Order o1 = new Order();     // 'o1' is REFERENCE (heap)");
        System.out.println("Order o2 = o1;              // 'o2' is REFERENCE (SAME object)");
        System.out.println("o2.price = 200;             // o1.price is also 200!");
        System.out.println();
        System.out.println("// ALL objects on heap (GC managed)");
        System.out.println("// NO stack objects for user-defined types");
        System.out.println("// NO manual delete/free");

        // Java demonstration
        System.out.println("\nJava Demonstration:");

        // Primitives (value semantics)
        int x = 10;
        int y = x;
        y = 20;
        System.out.println("Primitives (value): x=" + x + ", y=" + y);  // x=10, y=20

        // Objects (reference semantics)
        OrderObj o1 = new OrderObj(100);
        OrderObj o2 = o1;  // Same object!
        o2.price = 200;
        System.out.println("Objects (reference): o1.price=" + o1.price +
                          ", o2.price=" + o2.price);  // Both 200!

        System.out.println("\nSummary:");
        System.out.println("┌─────────────────┬──────────────┬──────────────────────┐");
        System.out.println("│ Type            │ C++          │ Java                 │");
        System.out.println("├─────────────────┼──────────────┼──────────────────────┤");
        System.out.println("│ Primitives      │ Value        │ Value                │");
        System.out.println("│ User objects    │ Value/Ref    │ Reference (ALWAYS)   │");
        System.out.println("│ Stack objects   │ YES          │ NO (primitives only) │");
        System.out.println("│ Heap objects    │ Explicit     │ Automatic (new)      │");
        System.out.println("│ References      │ Explicit (&) │ Hidden (all objects) │");
        System.out.println("└─────────────────┴──────────────┴──────────────────────┘");
    }

    //=============================================================================
    // 6. COPY AND MOVE CONSTRUCTORS
    //=============================================================================

    /**
     * C++: Compiler generates copy/move constructors automatically
     * Java: NO copy/move constructors (must implement clone() manually)
     */
    static void demonstrateCopyMove() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  6. COPY AND MOVE CONSTRUCTORS                             ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ (Automatic copy/move constructors):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    // Compiler automatically generates:");
        System.out.println("    Order(const Order& other);        // ✅ Copy constructor");
        System.out.println("    Order(Order&& other);             // ✅ Move constructor (C++11)");
        System.out.println("    Order& operator=(const Order&);   // ✅ Copy assignment");
        System.out.println("    Order& operator=(Order&&);        // ✅ Move assignment (C++11)");
        System.out.println("};");
        System.out.println();
        System.out.println("// Usage:");
        System.out.println("Order o1;");
        System.out.println("Order o2 = o1;              // ✅ Copy constructor");
        System.out.println("Order o3 = std::move(o1);   // ✅ Move constructor (efficient!)");

        System.out.println("\nJava (NO automatic copy/move):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    // NO copy constructor");
        System.out.println("    // NO move constructor");
        System.out.println("    // Must implement clone() manually:");
        System.out.println("    @Override");
        System.out.println("    protected Object clone() throws CloneNotSupportedException {");
        System.out.println("        return super.clone();  // Shallow copy");
        System.out.println("    }");
        System.out.println("}");
        System.out.println();
        System.out.println("// Usage:");
        System.out.println("Order o1 = new Order();");
        System.out.println("Order o2 = o1;              // ❌ NOT a copy (same reference!)");
        System.out.println("Order o3 = (Order) o1.clone(); // ✅ Copy (must cast)");

        // Java demonstration
        try {
            CloneableOrder o1 = new CloneableOrder(100);
            CloneableOrder o2 = o1;  // Same reference
            CloneableOrder o3 = (CloneableOrder) o1.clone();  // Copy

            o2.price = 200;
            o3.price = 300;

            System.out.println("\nJava cloning demonstration:");
            System.out.println("o1.price: " + o1.price);  // 200 (o2 changed it)
            System.out.println("o2.price: " + o2.price);  // 200 (same as o1)
            System.out.println("o3.price: " + o3.price);  // 300 (independent copy)
        } catch (CloneNotSupportedException e) {
            e.printStackTrace();
        }

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Copy/move constructors generated automatically");
        System.out.println("        Move semantics for efficiency (C++11)");
        System.out.println("        Deep copy vs shallow copy control");
        System.out.println("• Java: NO copy/move constructors");
        System.out.println("        Must implement Cloneable + clone() manually");
        System.out.println("        NO move semantics (GC handles everything)");
        System.out.println("        Assignment = reference copy (NOT object copy)");
    }

    static class CloneableOrder implements Cloneable {
        int price;

        public CloneableOrder(int price) {
            this.price = price;
        }

        @Override
        protected Object clone() throws CloneNotSupportedException {
            return super.clone();  // Shallow copy
        }
    }

    //=============================================================================
    // 7. DESTRUCTORS vs FINALIZERS/GC
    //=============================================================================

    /**
     * C++: Explicit destructors (~ClassName) called deterministically
     * Java: NO destructors, has finalize() (deprecated) and GC
     */
    static void demonstrateDestructors() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  7. DESTRUCTORS vs FINALIZERS/GC                           ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Destructors (Deterministic cleanup):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Connection {");
        System.out.println("    ~Connection() {               // ✅ Destructor");
        System.out.println("        close();                  // Called IMMEDIATELY");
        System.out.println("        release_resources();      // when object destroyed");
        System.out.println("    }");
        System.out.println("};");
        System.out.println();
        System.out.println("// Usage:");
        System.out.println("{");
        System.out.println("    Connection conn;              // Stack object");
        System.out.println("    conn.use();");
        System.out.println("}  // ✅ Destructor called HERE (deterministic!)");
        System.out.println();
        System.out.println("Connection* ptr = new Connection();");
        System.out.println("delete ptr;                       // ✅ Destructor called HERE");

        System.out.println("\nJava GC (Non-deterministic cleanup):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Connection {");
        System.out.println("    // NO destructor");
        System.out.println("    // finalize() deprecated since Java 9");
        System.out.println("    @Override");
        System.out.println("    protected void finalize() throws Throwable {");
        System.out.println("        // ⚠️ UNPREDICTABLE when/if called!");
        System.out.println("        // ⚠️ Should use try-with-resources instead");
        System.out.println("    }");
        System.out.println("}");
        System.out.println();
        System.out.println("// Modern approach: try-with-resources");
        System.out.println("try (Connection conn = new Connection()) {");
        System.out.println("    conn.use();");
        System.out.println("}  // ✅ close() called HERE (deterministic!)");

        // Java modern approach
        System.out.println("\nJava try-with-resources demonstration:");
        try (AutoCloseableConnection conn = new AutoCloseableConnection()) {
            conn.use();
        } catch (Exception e) {
            e.printStackTrace();
        }

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Destructor called IMMEDIATELY and DETERMINISTICALLY");
        System.out.println("        Perfect for RAII (Resource Acquisition Is Initialization)");
        System.out.println("        Manual memory management");
        System.out.println("• Java: NO destructors");
        System.out.println("        GC runs UNPREDICTABLY (when memory pressure)");
        System.out.println("        finalize() deprecated (unreliable)");
        System.out.println("        Use try-with-resources for deterministic cleanup");
        System.out.println("        Automatic memory management (GC)");

        System.out.println("\nMemory Management:");
        System.out.println("┌─────────────────┬─────────────────┬─────────────────────┐");
        System.out.println("│ Aspect          │ C++             │ Java                │");
        System.out.println("├─────────────────┼─────────────────┼─────────────────────┤");
        System.out.println("│ Cleanup         │ Destructor      │ GC + finalizer      │");
        System.out.println("│ When            │ Deterministic   │ Non-deterministic   │");
        System.out.println("│ Control         │ Full control    │ No control          │");
        System.out.println("│ Performance     │ Predictable     │ GC pauses           │");
        System.out.println("│ Memory leaks    │ Possible        │ Rare (GC handles)   │");
        System.out.println("│ Resource leaks  │ Manual tracking │ try-with-resources  │");
        System.out.println("└─────────────────┴─────────────────┴─────────────────────┘");
    }

    static class AutoCloseableConnection implements AutoCloseable {
        public void use() {
            System.out.println("Using connection...");
        }

        @Override
        public void close() {
            System.out.println("Connection closed (deterministic cleanup)");
        }
    }

    //=============================================================================
    // 8. DEFAULT CONSTRUCTORS
    //=============================================================================

    /**
     * C++: Default constructor generated if NO constructors defined
     * Java: Default constructor generated if NO constructors defined
     *       (Similar behavior but different semantics)
     */
    static void demonstrateDefaultConstructors() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  8. DEFAULT CONSTRUCTORS                                   ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Default Constructor:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    // If NO constructors defined:");
        System.out.println("    // Order() = default;         // ✅ Generated automatically");
        System.out.println("    //                            // Members UNINITIALIZED!");
        System.out.println("};");
        System.out.println();
        System.out.println("class Order2 {");
        System.out.println("    Order2(int x) { }            // User-defined constructor");
        System.out.println("    // NO default constructor generated!");
        System.out.println("};");
        System.out.println();
        System.out.println("Order o1;                        // ✅ OK (default constructor)");
        System.out.println("Order2 o2;                       // ❌ ERROR! No default constructor");
        System.out.println("Order2 o3(10);                   // ✅ OK (uses defined constructor)");

        System.out.println("\nJava Default Constructor:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    // If NO constructors defined:");
        System.out.println("    // public Order() { }         // ✅ Generated automatically");
        System.out.println("    //                            // Members initialized to defaults!");
        System.out.println("}");
        System.out.println();
        System.out.println("class Order2 {");
        System.out.println("    Order2(int x) { }            // User-defined constructor");
        System.out.println("    // NO default constructor generated!");
        System.out.println("}");
        System.out.println();
        System.out.println("Order o1 = new Order();          // ✅ OK (default constructor)");
        System.out.println("Order2 o2 = new Order2();        // ❌ ERROR! No default constructor");
        System.out.println("Order2 o3 = new Order2(10);      // ✅ OK (uses defined constructor)");

        // Java demonstration
        DefaultOrder o1 = new DefaultOrder();
        System.out.println("\nJava default initialization: price=" + o1.price);  // 0 (auto-init)

        // CustomOrder o2 = new CustomOrder();  // ❌ Compile error!
        CustomOrder o3 = new CustomOrder(100);
        System.out.println("Custom constructor: price=" + o3.price);

        System.out.println("\nKey Similarity:");
        System.out.println("• Both: Default constructor generated ONLY if no constructors defined");
        System.out.println("• Both: Defining ANY constructor suppresses default constructor");

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Default constructor leaves members UNINITIALIZED");
        System.out.println("        (undefined behavior if used before assignment)");
        System.out.println("• Java: Default constructor initializes members to DEFAULTS");
        System.out.println("        (0, false, null - always safe)");
    }

    static class DefaultOrder {
        int price;  // Automatically initialized to 0
    }

    static class CustomOrder {
        int price;

        // User-defined constructor (suppresses default constructor)
        public CustomOrder(int price) {
            this.price = price;
        }
    }

    //=============================================================================
    // 9. ACCESS CONTROL
    //=============================================================================

    /**
     * C++: public, protected, private (class-level and inheritance)
     * Java: public, protected, private, package-private (default)
     */
    static void demonstrateAccessControl() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  9. ACCESS CONTROL DIFFERENCES                             ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Access Control:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("public:");
        System.out.println("    int id;              // Accessible everywhere");
        System.out.println("protected:");
        System.out.println("    int price;           // Accessible in derived classes");
        System.out.println("private:");
        System.out.println("    int qty;             // Accessible only in this class");
        System.out.println("};");
        System.out.println();
        System.out.println("// Default access: PRIVATE");
        System.out.println("class Order2 {");
        System.out.println("    int x;               // ❌ PRIVATE by default!");
        System.out.println("};");

        System.out.println("\nJava Access Control:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    public int id;              // Accessible everywhere");
        System.out.println("    protected int price;        // Package + subclasses");
        System.out.println("    private int qty;            // Only this class");
        System.out.println("    int status;                 // Package-private (default)");
        System.out.println("}");
        System.out.println();
        System.out.println("// Default access: PACKAGE-PRIVATE");
        System.out.println("class Order2 {");
        System.out.println("    int x;               // ✅ PACKAGE-PRIVATE by default");
        System.out.println("}");

        System.out.println("\nAccess Level Comparison:");
        System.out.println("┌────────────────┬─────────┬──────────┬────────────┬───────────┐");
        System.out.println("│ Modifier       │ Class   │ Package  │ Subclass   │ World     │");
        System.out.println("├────────────────┼─────────┼──────────┼────────────┼───────────┤");
        System.out.println("│ public         │ ✅      │ ✅       │ ✅         │ ✅        │");
        System.out.println("│ protected (C++) │ ✅      │ ❌       │ ✅         │ ❌        │");
        System.out.println("│ protected (Java)│ ✅      │ ✅       │ ✅         │ ❌        │");
        System.out.println("│ package (Java) │ ✅      │ ✅       │ ❌         │ ❌        │");
        System.out.println("│ private        │ ✅      │ ❌       │ ❌         │ ❌        │");
        System.out.println("└────────────────┴─────────┴──────────┴────────────┴───────────┘");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  Default is PRIVATE");
        System.out.println("        protected = class + derived classes only");
        System.out.println("        No package-level access");
        System.out.println("• Java: Default is PACKAGE-PRIVATE");
        System.out.println("        protected = class + package + derived classes");
        System.out.println("        Has package-level access (default)");
    }

    //=============================================================================
    // 10. ABSTRACT CLASSES AND INTERFACES
    //=============================================================================

    /**
     * C++: Abstract classes (with pure virtual functions) and interfaces (abstract classes with only pure virtual functions)
     * Java: Abstract classes (with or without abstract methods) and interfaces (with only abstract methods)
     */
    static void demonstrateAbstractClassesAndInterfaces() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  10. ABSTRACT CLASSES AND INTERFACES                       ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Abstract Classes and Interfaces:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Shape {                              // Abstract class");
        System.out.println("public:");
        System.out.println("    virtual void draw() = 0;              // Pure virtual function");
        System.out.println("};");
        System.out.println();
        System.out.println("class Circle : public Shape {              // Concrete class");
        System.out.println("public:");
        System.out.println("    void draw() override { }               // Overrides base method");
        System.out.println("};");
        System.out.println();
        System.out.println("Shape* s = new Circle();                  // Can use base class pointer");
        System.out.println("s->draw();                                // Calls Circle's draw()");

        System.out.println("\nJava Abstract Classes and Interfaces:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("abstract class Shape {                     // Abstract class");
        System.out.println("    abstract void draw();                 // Abstract method");
        System.out.println("}");
        System.out.println();
        System.out.println("class Circle extends Shape {               // Concrete class");
        System.out.println("    void draw() { }                       // Implements abstract method");
        System.out.println("}");
        System.out.println();
        System.out.println("Shape s = new Circle();                  // Reference variable");
        System.out.println("s.draw();                                // Calls Circle's draw()");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  Abstract class = any virtual method, Interface = pure virtual class");
        System.out.println("• Java: Abstract class = may have concrete methods, Interface = all abstract methods");
        System.out.println("        Interfaces are implemented, not inherited");
    }

    //=============================================================================
    // 11. POINTERS (C++ YES, Java NO - references only)
    //=============================================================================

    /**
     * C++: Supports POINTERS (explicit pointer syntax)
     * Java: NO pointers, only references (implicit)
     */
    static void demonstratePointers() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  11. POINTERS (C++ YES, Java NO - references only)      ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Pointers:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("int x = 10;");
        System.out.println("int* p = &x;               // Pointer to x");
        System.out.println("*p = 20;                   // Change value of x");
        System.out.println("int y = *p;                // y is 20");
        System.out.println();
        System.out.println("class Order { };");
        System.out.println("Order* ptr = new Order();   // Pointer to Order");
        System.out.println("delete ptr;                 // Free memory");

        System.out.println("\nJava References (No explicit pointers):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("int x = 10;");
        System.out.println("int y = x;                  // Copy of value");
        System.out.println("y = 20;                     // x is still 10");
        System.out.println();
        System.out.println("class Order { };");
        System.out.println("Order obj = new Order();    // Reference to Order");
        System.out.println("Order ref = obj;            // Another reference to same object");

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Pointers are explicit, can be null, pointer arithmetic");
        System.out.println("        Must manage memory (new/delete)");
        System.out.println("• Java: References are implicit, cannot be null (but objects can be)");
        System.out.println("        No pointer arithmetic, no manual memory management");
    }

    //=============================================================================
    // 12. TEMPLATES vs GENERICS
    //=============================================================================

    /**
     * C++: Supports TEMPLATES (compile-time polymorphism)
     * Java: Supports GENERICS (runtime polymorphism)
     */
    static void demonstrateTemplatesVsGenerics() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  12. TEMPLATES vs GENERICS                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Templates:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("template <typename T>");
        System.out.println("class Box {");
        System.out.println("    T value;");
        System.out.println("public:");
        System.out.println("    Box(T val) : value(val) { }");
        System.out.println("    T getValue() { return value; }");
        System.out.println("};");
        System.out.println();
        System.out.println("Box<int> intBox(123);");
        System.out.println("Box<double> dblBox(456.78);");
        System.out.println("intBox.getValue();                // Returns int");
        System.out.println("dblBox.getValue();                // Returns double");

        System.out.println("\nJava Generics:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Box<T> {");
        System.out.println("    private T value;");
        System.out.println("    public Box(T val) { value = val; }");
        System.out.println("    public T getValue() { return value; }");
        System.out.println("}");
        System.out.println();
        System.out.println("Box<Integer> intBox = new Box<>(123);");
        System.out.println("Box<Double> dblBox = new Box<>(456.78);");
        System.out.println("intBox.getValue();                // Returns Integer");
        System.out.println("dblBox.getValue();                // Returns Double");

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Templates are resolved at compile-time (type safety)");
        System.out.println("        Can have non-type parameters");
        System.out.println("• Java: Generics are implemented at runtime (type erasure)");
        System.out.println("        Cannot have primitive types, must use wrapper classes");
    }

    //=============================================================================
    // 13. FRIEND FUNCTIONS
    //=============================================================================

    /**
     * C++: Supports FRIEND FUNCTIONS (non-member functions with access to private members)
     * Java: NO friend functions (all functions are members of a class)
     */
    static void demonstrateFriendFunctions() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  13. FRIEND FUNCTIONS                                      ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Friend Functions:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    private:");
        System.out.println("        int price;");
        System.out.println("    public:");
        System.out.println("        Order(int p) : price(p) { }");
        System.out.println("        friend void displayPrice(Order& o);  // Friend function");
        System.out.println("};");
        System.out.println();
        System.out.println("void displayPrice(Order& o) {");
        System.out.println("    cout << o.price;                // Accesses private member");
        System.out.println("}");
        System.out.println();
        System.out.println("Order order(100);");
        System.out.println("displayPrice(order);                // Calls friend function");

        System.out.println("\nJava (No friend functions):");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    private int price;");
        System.out.println("    public Order(int p) { price = p; }");
        System.out.println("}");
        System.out.println();
        System.out.println("// Cannot access private member directly");
        System.out.println("Order order = new Order(100);");
        System.out.println("// displayPrice(order);              // ❌ Compile error!");

        System.out.println("\nKey Difference:");
        System.out.println("• C++:  Friend functions can access private members");
        System.out.println("        Useful for operator overloading, non-member functions");
        System.out.println("• Java: NO friend functions");
        System.out.println("        All functions are members of a class");
    }

    //=============================================================================
    // 14. INNER/NESTED CLASSES
    //=============================================================================

    /**
     * C++: Supports INNER/NESTED CLASSES (defined within another class)
     * Java: Supports INNER/NESTED CLASSES and STATIC NESTED CLASSES
     */
    static void demonstrateInnerClasses() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  14. INNER/NESTED CLASSES                                  ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Inner/Nested Classes:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Outer {");
        System.out.println("    class Inner {                  // Inner class");
        System.out.println("        void display() { }");
        System.out.println("    };");
        System.out.println("};");
        System.out.println();
        System.out.println("Outer::Inner obj;                // Instantiate inner class");

        System.out.println("\nJava Inner/Nested Classes:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Outer {");
        System.out.println("    class Inner {                  // Inner class");
        System.out.println("        void display() { }");
        System.out.println("    }");
        System.out.println();
        System.out.println("    static class StaticNested {    // Static nested class");
        System.out.println("        void show() { }");
        System.out.println("    }");
        System.out.println("}");
        System.out.println();
        System.out.println("Outer.Inner obj = new Outer().new Inner();          // Inner class");
        System.out.println("Outer.StaticNested staticObj = new Outer.StaticNested();  // Static nested class");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  Inner classes have access to outer class's private members");
        System.out.println("        Must use scope resolution (::) to access");
        System.out.println("• Java: Inner classes also have access to outer class's private members");
        System.out.println("        Can use 'new' keyword to instantiate");
        System.out.println("        Static nested classes do not have this reference");
    }

    //=============================================================================
    // 15. STATIC MEMBERS AND METHODS
    //=============================================================================

    /**
     * C++: Supports STATIC MEMBERS and METHODS (class-level, shared across all instances)
     * Java: Supports STATIC MEMBERS and METHODS (class-level, shared across all instances)
     */
    static void demonstrateStaticMembersAndMethods() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  15. STATIC MEMBERS AND METHODS                            ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Static Members and Methods:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    static int count;            // Static member");
        System.out.println("public:");
        System.out.println("    Order() { count++; }         // Increment count");
        System.out.println("    static int getCount() { return count; }  // Static method");
        System.out.println("};");
        System.out.println();
        System.out.println("Order o1, o2, o3;");
        System.out.println("int total = Order::getCount();    // Access static method");

        System.out.println("\nJava Static Members and Methods:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    static int count;            // Static member");
        System.out.println("    public Order() { count++; }  // Increment count");
        System.out.println("    static int getCount() { return count; }  // Static method");
        System.out.println("}");
        System.out.println();
        System.out.println("Order o1 = new Order(), o2 = new Order(), o3 = new Order();");
        System.out.println("int total = Order.getCount();      // Access static method");

        System.out.println("\nKey Similarities:");
        System.out.println("• Both C++ and Java support static members and methods");
        System.out.println("• Static members are shared across all instances");
        System.out.println("• Static methods can be called without an instance");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  Must use scope resolution (::) to access static members/methods");
        System.out.println("• Java: Can access directly using class name (no :: needed)");
    }

    //=============================================================================
    // 16. FINAL/CONST KEYWORD DIFFERENCES
    //=============================================================================

    /**
     * C++: const (variables, pointers, member functions), constexpr (compile-time constants)
     * Java: final (variables, methods, classes), strictfp (floating-point consistency)
     */
    static void demonstrateFinalConstDifferences() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  16. FINAL/CONST KEYWORD DIFFERENCES                     ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ const and constexpr:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("const int x = 10;                // Constant variable");
        System.out.println("constexpr int y = 20;            // Compile-time constant");
        System.out.println("const int* p = &x;              // Pointer to constant");
        System.out.println("int* const q = &x;              // Constant pointer");
        System.out.println("const int& r = x;               // Reference to constant");
        System.out.println();
        System.out.println("class Order {");
        System.out.println("    void process() const { }      // Const member function");
        System.out.println("};");

        System.out.println("\nJava final and strictfp:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("final int x = 10;                // Constant variable");
        System.out.println("int y = 20;");
        System.out.println("y = 30;                         // Not final, can be changed");
        System.out.println();
        System.out.println("class Order {");
        System.out.println("    final void process() { }      // Final method (cannot be overridden)");
        System.out.println("    static final int MAX_ORDERS = 100;  // Constant static variable");
        System.out.println("}");
        System.out.println();
        System.out.println("strictfp class Calculator {       // Strict floating-point consistency");
        System.out.println("    strictfp void calculate() { }  // All calculations in this method are strict");
        System.out.println("}");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  const = variable or pointer is constant");
        System.out.println("        constexpr = compile-time constant");
        System.out.println("        Can have const member functions");
        System.out.println("• Java: final = variable, method, or class cannot be changed/overridden");
        System.out.println("        strictfp = strict floating-point consistency");
    }

    //=============================================================================
    // 17. METHOD OVERLOADING AND OVERRIDING
    //=============================================================================

    /**
     * C++: Supports METHOD OVERLOADING and OVERRIDING
     * Java: Supports METHOD OVERLOADING and OVERRIDING
     */
    static void demonstrateMethodOverloadingAndOverriding() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  17. METHOD OVERLOADING AND OVERRIDING                    ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Method Overloading and Overriding:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("public:");
        System.out.println("    void process() { }          // Original method");
        System.out.println("    void process(int id) { }    // Overloaded method");
        System.out.println("    virtual void execute() { }  // Virtual method");
        System.out.println("};");
        System.out.println();
        System.out.println("class LimitOrder : public Order {");
        System.out.println("public:");
        System.out.println("    void process() { }          // Hides base method");
        System.out.println("    void execute() override { } // Overrides base method");
        System.out.println("};");

        System.out.println("\nJava Method Overloading and Overriding:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    void process() { }          // Original method");
        System.out.println("    void process(int id) { }    // Overloaded method");
        System.out.println("    void execute() { }          // Non-virtual method");
        System.out.println("}");
        System.out.println();
        System.out.println("class LimitOrder extends Order {");
        System.out.println("    void process() { }          // Overrides (not hiding)");
        System.out.println("    void execute() { }          // Cannot override (non-virtual)");
        System.out.println("}");

        System.out.println("\nKey Similarities:");
        System.out.println("• Both C++ and Java support method overloading (same method name, different parameters)");
        System.out.println("• Both support method overriding (subclass provides specific implementation)");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  Must use 'virtual' keyword for polymorphic behavior");
        System.out.println("        Can overload based on constness (const int vs int)");
        System.out.println("• Java: All non-static methods are virtual by default (except final, private, static)");
        System.out.println("        Cannot overload based on return type alone");
    }

    //=============================================================================
    // 18. THIS POINTER vs THIS REFERENCE
    //=============================================================================

    /**
     * C++: 'this' is an explicit pointer to the current object
     * Java: 'this' is an implicit reference to the current object
     */
    static void demonstrateThisPointerVsThisReference() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  18. THIS POINTER vs THIS REFERENCE                        ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ This Pointer:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("public:");
        System.out.println("    void process() {");
        System.out.println("        this->validate();         // Explicit this pointer");
        System.out.println("    }");
        System.out.println("    void validate() { }");
        System.out.println("};");
        System.out.println();
        System.out.println("Order order;");
        System.out.println("order.process();                  // Calls process()");

        System.out.println("\nJava This Reference:");
        System.out.println("───────────────────────────────────────────────────────────");
        System.out.println("class Order {");
        System.out.println("    void process() {");
        System.out.println("        this.validate();          // Implicit this reference");
        System.out.println("    }");
        System.out.println("    void validate() { }");
        System.out.println("}");
        System.out.println();
        System.out.println("Order order = new Order();        // Create new instance");
        System.out.println("order.process();                  // Calls process()");

        System.out.println("\nKey Differences:");
        System.out.println("• C++:  this is an explicit pointer, must use '->' to access members");
        System.out.println("        Can be null, must check in member functions");
        System.out.println("• Java: this is an implicit reference, no need to use '->'");
        System.out.println("        Cannot be null, always refers to current object");
    }

    //=============================================================================
    // 19. SUMMARY AND COMPLETE COMPARISON
    //=============================================================================

    static void printCompleteComparison() {
        System.out.println("\n╔════════════════════════════════════════════════════════════╗");
        System.out.println("║  19. COMPLETE OOP COMPARISON                               ║");
        System.out.println("╚════════════════════════════════════════════════════════════╝\n");

        System.out.println("┌──────────────────────────┬────────────────────────┬────────────────────────┐");
        System.out.println("│ Feature                  │ C++                    │ Java                   │");
        System.out.println("├──────────────────────────┼────────────────────────┼────────────────────────┤");
        System.out.println("│ Multiple Inheritance     │ ✅ YES (classes)       │ ❌ NO (single class)   │");
        System.out.println("│ Interface Inheritance    │ Abstract classes       │ ✅ Multiple interfaces │");
        System.out.println("│ Virtual by default       │ ❌ NO (explicit)       │ ✅ YES (implicit)      │");
        System.out.println("│ Operator overloading     │ ✅ YES                 │ ❌ NO (except String)  │");
        System.out.println("│ lvalue/rvalue refs       │ ✅ YES (& and &&)      │ ❌ NO (hidden)         │");
        System.out.println("│ Move semantics           │ ✅ YES (C++11)         │ ❌ NO                  │");
        System.out.println("│ Copy constructor         │ ✅ Auto-generated      │ ❌ NO (manual clone)   │");
        System.out.println("│ Destructor               │ ✅ YES (~Class)        │ ❌ NO (GC)             │");
        System.out.println("│ Primitives               │ Value semantics        │ Value semantics        │");
        System.out.println("│ Objects                  │ Value/Reference        │ Reference (ALWAYS)     │");
        System.out.println("│ Stack objects            │ ✅ YES                 │ ❌ NO (primitives only)│");
        System.out.println("│ Heap objects             │ Explicit (new/delete)  │ Automatic (new + GC)   │");
        System.out.println("│ Memory management        │ Manual (RAII)          │ Automatic (GC)         │");
        System.out.println("│ Default constructor      │ If no constructors     │ If no constructors     │");
        System.out.println("│ Default access           │ private                │ package-private        │");
        System.out.println("│ Friend functions         │ ✅ YES                 │ ❌ NO                  │");
        System.out.println("│ Templates/Generics       │ Templates (compile)    │ Generics (runtime)     │");
        System.out.println("│ Multiple dispatch        │ ❌ NO                  │ ❌ NO                  │");
        System.out.println("│ Abstract classes         │ Pure virtual functions  │ Abstract classes       │");
        System.out.println("│ Pointers                 │ ✅ YES                 │ ❌ NO (references only)│");
        System.out.println("│ Static members           │ ✅ YES                 │ ✅ YES                 │");
        System.out.println("│ Final/Const              │ const, constexpr       │ final                  │");
        System.out.println("│ Method overloading       │ ✅ YES                 │ ✅ YES                 │");
        System.out.println("│ Method overriding        │ ✅ YES                 │ ✅ YES                 │");
        System.out.println("│ This pointer/reference   │ Pointer (explicit)    │ Reference (implicit)   │");
        System.out.println("└──────────────────────────┴────────────────────────┴────────────────────────┘");
    }

    //=============================================================================
    // MAIN
    //=============================================================================

    public static void main(String[] args) {
        System.out.println("╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║                                                                ║");
        System.out.println("║       Java vs C++ Object-Oriented Programming Differences      ║");
        System.out.println("║                  Complete Comparison Guide                     ║");
        System.out.println("║                                                                ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        demonstrateMultipleInheritance();
        demonstrateVirtualFunctions();
        demonstrateOperatorOverloading();
        demonstrateReferences();
        demonstrateValueVsReference();
        demonstrateCopyMove();
        demonstrateDestructors();
        demonstrateDefaultConstructors();
        demonstrateAccessControl();
        demonstrateAbstractClassesAndInterfaces();
        demonstratePointers();
        demonstrateTemplatesVsGenerics();
        demonstrateFriendFunctions();
        demonstrateInnerClasses();
        demonstrateStaticMembersAndMethods();
        demonstrateFinalConstDifferences();
        demonstrateMethodOverloadingAndOverriding();
        demonstrateThisPointerVsThisReference();
        printCompleteComparison();

        System.out.println("\n╔════════════════════════════════════════════════════════════════╗");
        System.out.println("║  KEY TAKEAWAYS                                                 ║");
        System.out.println("╚════════════════════════════════════════════════════════════════╝\n");

        System.out.println("C++ Philosophy:");
        System.out.println("• Explicit control (virtual, references, memory)");
        System.out.println("• Value and reference semantics (programmer's choice)");
        System.out.println("• Zero-cost abstractions (pay for what you use)");
        System.out.println("• RAII (Resource Acquisition Is Initialization)");
        System.out.println("• Multiple inheritance (power and complexity)");

        System.out.println("\nJava Philosophy:");
        System.out.println("• Implicit control (all virtual, hidden references, GC)");
        System.out.println("• Reference semantics for objects (no choice)");
        System.out.println("• Simplicity over control (safer, less flexible)");
        System.out.println("• Garbage collection (automatic memory management)");
        System.out.println("• Single inheritance + interfaces (safety over power)");

        System.out.println("\nFor Trading Systems:");
        System.out.println("• C++:  Ultra-low latency, deterministic performance");
        System.out.println("        Full control over memory and performance");
        System.out.println("        Complexity and manual management");
        System.out.println("• Java: Good latency, predictable but with GC pauses");
        System.out.println("        Easier development, automatic memory");
        System.out.println("        Less control, GC overhead");

        System.out.println("\n🚀 Choose based on latency requirements and team expertise!");
    }
}

