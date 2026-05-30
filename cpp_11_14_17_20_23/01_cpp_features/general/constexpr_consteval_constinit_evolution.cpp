/*
 * ============================================================================
 * constexpr vs consteval vs constinit — Complete Evolution Guide
 * C++11 → C++14 → C++17 → C++20 → C++23
 * ============================================================================
 *
 * QUICK REFERENCE:
 * ┌─────────────┬──────────┬───────────┬──────────────────────────────────────┐
 * │ Keyword     │ Intro'd  │ Runtime?  │ Purpose                              │
 * ├─────────────┼──────────┼───────────┼──────────────────────────────────────┤
 * │ constexpr   │ C++11    │ YES       │ CAN be compile-time (not guaranteed) │
 * │ consteval   │ C++20    │ NO        │ MUST be compile-time (immediate fn)  │
 * │ constinit   │ C++20    │ YES(mutable)│ MUST be statically initialized    │
 * └─────────────┴──────────┴───────────┴──────────────────────────────────────┘
 *
 * Build: g++ -std=c++23 -O2 -Wall constexpr_consteval_constinit_evolution.cpp -o const_evolution
 * ============================================================================
 */

#include <iostream>
#include <array>
#include <string_view>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <type_traits>
#include <bit>          // C++20 std::popcount
#include <cmath>
#include <cassert>
#include <span>         // C++20

// ============================================================================
// SECTION 1: constexpr EVOLUTION — C++11 to C++23
// ============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// C++11: constexpr INTRODUCED
// Rules: single return statement ONLY, no local variables, no loops, no if
// ─────────────────────────────────────────────────────────────────────────────
namespace cpp11 {

    // C++11: body must be a single return expression
    constexpr int factorial(int n) {
        return n <= 1 ? 1 : n * factorial(n - 1);   // recursion OK, loop NOT OK
    }

    constexpr int square(int x) { return x * x; }

    // C++11: constexpr member functions — implicitly const
    struct Point {
        int x, y;
        constexpr int length_sq() const { return x*x + y*y; }  // single return
    };

    // C++11: constexpr constructors
    struct Vec2 {
        int x, y;
        constexpr Vec2(int x, int y) : x(x), y(y) {}
    };

    void demo() {
        std::cout << "\n=== C++11 constexpr ===\n";

        // Compile-time evaluation
        constexpr int f5  = factorial(5);       // 120 — guaranteed compile-time
        constexpr int sq7 = square(7);          // 49
        constexpr Point p{3, 4};
        constexpr int len = p.length_sq();      // 25

        std::cout << "factorial(5)    = " << f5  << "\n";
        std::cout << "square(7)       = " << sq7 << "\n";
        std::cout << "length_sq(3,4)  = " << len << "\n";

        // Runtime use also valid (constexpr functions are dual-mode)
        int n = 6;
        std::cout << "factorial(6) RT = " << factorial(n) << "\n"; // runtime
    }
}

// ────────────────────────────────────────���────────────────────────────────────
// C++14: constexpr RELAXED
// Now allows: local variables, loops, if/else, multiple statements
// ─────────────────────���───────────────────────────────────────────────────────
namespace cpp14 {

    // C++14: can now use local vars and loops inside constexpr
    constexpr int factorial(int n) {
        int result = 1;                         // local variable — NOT allowed C++11
        for (int i = 2; i <= n; ++i)            // loop — NOT allowed in C++11
            result *= i;
        return result;
    }

    // C++14: if/else allowed
    constexpr int abs_val(int x) {
        if (x < 0) return -x;                  // if/else — NOT allowed in C++11
        return x;
    }

    // C++14: constexpr member functions no longer implicitly const
    struct Counter {
        int value = 0;
        constexpr void increment() { ++value; } // mutation OK in C++14
        constexpr int get() const { return value; }
    };

    // C++14: std::array algorithms are constexpr
    constexpr int sum_array() {
        std::array<int, 5> arr = {1, 2, 3, 4, 5};
        int total = 0;
        for (auto v : arr) total += v;          // range-for in constexpr: C++14
        return total;
    }

    void demo() {
        std::cout << "\n=== C++14 constexpr ===\n";

        constexpr int f6   = factorial(6);      // 720
        constexpr int av   = abs_val(-42);      // 42
        constexpr int sum  = sum_array();       // 15

        [[maybe_unused]] constexpr Counter c{};
        // constexpr Counter mutated: can't mutate constexpr object after init
        // but constexpr functions with mutation ARE valid

        std::cout << "factorial(6)  = " << f6   << "\n";
        std::cout << "abs_val(-42)  = " << av   << "\n";
        std::cout << "sum_array()   = " << sum  << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// C++17: constexpr FURTHER EXPANDED
// Adds: constexpr lambda, if constexpr, fold expressions
// std::string_view constexpr, more STL algorithms constexpr
// ─────────────────────────────────────────────────────────────────────────────
namespace cpp17 {

    // C++17: constexpr lambdas
    constexpr auto multiply = [](int a, int b) constexpr { return a * b; };

    // C++17: if constexpr — compile-time branch selection
    template<typename T>
    constexpr auto type_name_value(T val) {
        if constexpr (std::is_integral_v<T>)         // branch eliminated at compile-time
            return val * 2;
        else if constexpr (std::is_floating_point_v<T>)
            return val * 2.0;
        else
            return val;
    }

    // C++17: string_view is constexpr-friendly (no heap alloc)
    constexpr std::string_view greeting = "Hello ULL World";

    constexpr std::size_t count_char(std::string_view sv, char c) {
        std::size_t count = 0;
        for (char ch : sv)                          // range-for on string_view
            if (ch == c) ++count;
        return count;
    }

    // C++17: fold expressions in constexpr templates
    template<typename... Args>
    constexpr auto sum_all(Args... args) {
        return (... + args);                        // C++17 fold expression
    }

    // C++17: constexpr if for template specialization without SFINAE
    template<typename T>
    constexpr std::string_view type_str() {
        if constexpr (std::is_same_v<T, int>)       return "int";
        else if constexpr (std::is_same_v<T, double>) return "double";
        else if constexpr (std::is_same_v<T, float>)  return "float";
        else                                           return "unknown";
    }

    void demo() {
        std::cout << "\n=== C++17 constexpr ===\n";

        constexpr int  m  = multiply(6, 7);         // 42 — constexpr lambda
        constexpr int  tv = type_name_value(10);    // 20
        constexpr auto lc = count_char(greeting, 'l'); // 3
        constexpr auto s  = sum_all(1, 2, 3, 4, 5); // 15

        std::cout << "multiply(6,7)          = " << m  << "\n";
        std::cout << "type_name_value(10)    = " << tv << "\n";
        std::cout << "count 'l' in greeting  = " << lc << "\n";
        std::cout << "sum_all(1..5)          = " << s  << "\n";
        std::cout << "type_str<int>()        = " << type_str<int>()    << "\n";
        std::cout << "type_str<double>()     = " << type_str<double>() << "\n";

        // if constexpr at runtime scope — branch eliminated
        constexpr bool is_int = std::is_integral_v<int>;
        if constexpr (is_int) {
            std::cout << "int IS integral — this branch compiled in\n";
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// C++20: constexpr MASSIVELY EXPANDED + consteval + constinit INTRODUCED
//
// constexpr gains:
//   - std::string, std::vector in constexpr context
//   - virtual functions can be constexpr
//   - try/catch in constexpr (but throw still disallowed at compile-time)
//   - std::is_constant_evaluated()
//   - dynamic_cast in constexpr
//
// consteval: NEW — immediate functions, MUST evaluate at compile-time
// constinit: NEW — static/thread_local vars MUST be statically initialized
// ────────────────────────────────────────────────────���────────────────────────
namespace cpp20 {

    // ── constexpr in C++20 ──────────────────────────────���───────────────────

    // C++20: std::vector is constexpr (heap alloc allowed in constexpr context)
    constexpr int sum_vector() {
        std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        return std::accumulate(v.begin(), v.end(), 0);  // 55
    }

    // C++20: std::string is constexpr
    constexpr std::size_t string_length() {
        std::string s = "Hello C++20";                  // heap alloc inside constexpr!
        return s.size();
    }

    // C++20: std::is_constant_evaluated() — different behavior compile vs runtime
    constexpr double fast_sqrt(double x) {
        if (std::is_constant_evaluated()) {
            // Compile-time: manual Newton-Raphson (no std::sqrt at compile-time)
            double r = x;
            for (int i = 0; i < 10; ++i)
                r = (r + x / r) / 2.0;
            return r;
        } else {
            return std::sqrt(x);                        // runtime: use hardware sqrt
        }
    }

    // C++20: virtual + constexpr
    struct Shape {
        virtual constexpr double area() const = 0;
        virtual ~Shape() = default;
    };
    struct Circle : Shape {
        double r;
        constexpr Circle(double r) : r(r) {}
        constexpr double area() const override { return 3.14159 * r * r; }
    };

    // ── consteval: C++20 IMMEDIATE FUNCTIONS ────────────────────────────────
    //
    // Key difference from constexpr:
    //   constexpr → MAY be compile-time (can also run at runtime)
    //   consteval → MUST be compile-time (calling with runtime arg = ERROR)

    consteval int compile_only_square(int x) {
        return x * x;                                   // guaranteed compile-time
    }

    // consteval for compile-time validated config
    consteval int make_port(int port) {
        if (port < 1024 || port > 65535)
            throw "port out of valid range";            // compile-time error if bad
        return port;
    }

    // consteval — hash a string at compile-time (for switch statements)
    consteval uint64_t hash(std::string_view sv) {
        uint64_t h = 14695981039346656037ULL;
        for (char c : sv) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ULL;
        }
        return h;
    }

    // consteval template: generate lookup table at compile-time
    template<std::size_t N>
    consteval std::array<int, N> make_squares() {
        std::array<int, N> arr{};
        for (std::size_t i = 0; i < N; ++i)
            arr[i] = static_cast<int>(i * i);
        return arr;
    }

    // consteval vs constexpr — the critical difference
    constexpr int maybe_compiletime(int x) { return x * 2; }   // dual-mode
    consteval int always_compiletime(int x) { return x * 2; }  // compile-time only

    // ── constinit: C++20 ────────────────────────────────────────────────────
    //
    // Problem it solves: STATIC INITIALIZATION ORDER FIASCO
    //
    // Without constinit: global variables in different TUs initialized in
    // undefined order → one global may use another before it's initialized
    //
    // constinit guarantees: initialized BEFORE any runtime code runs
    //                       (static/zero initialization phase)
    //
    // Key rules:
    //   - Only for static storage / thread_local duration variables
    //   - Initializer must be a constant expression
    //   - Variable CAN be modified at runtime (unlike constexpr)
    //   - Does NOT make the variable const

    constinit int    server_port    = 8080;         // static init guaranteed
    constinit double tick_size      = 0.01;         // can be changed at runtime
    constinit int    max_orders     = 1'000'000;

    // constinit with constexpr function result
    constexpr int compute_buffer_size() { return 1 << 16; }    // 65536
    constinit int buffer_size = compute_buffer_size();

    // constinit thread_local
    thread_local constinit int thread_id = 0;       // each thread: compile-time init

    // CONTRAST:
    //   constexpr int x = 42;    → const + compile-time init (cannot change)
    //   constinit int x = 42;    → mutable + compile-time init (can change)
    //   const int x = 42;        → const + MAYBE runtime init

    // Static initialization order fiasco — solved by constinit
    // File A: int config_value = load_config();        ← runtime init
    // File B: int uses_config  = config_value * 2;    ← UB if A init after B
    //
    // Fix:
    // File A: constinit int config_value = 42;         ← static init, always first
    // File B: constinit int uses_config  = 42 * 2;    ← safe

    void demo() {
        std::cout << "\n=== C++20 constexpr / consteval / constinit ===\n";

        // constexpr C++20
        constexpr int sv = sum_vector();            // 55
        constexpr auto sl = string_length();        // 11
        constexpr double sq = fast_sqrt(2.0);       // compile-time Newton

        std::cout << "sum_vector()     = " << sv  << "\n";
        std::cout << "string_length()  = " << sl  << "\n";
        std::cout << "fast_sqrt(2.0)   = " << sq  << "\n";
        std::cout << "runtime sqrt(2)  = " << fast_sqrt(2.0) << "\n"; // uses std::sqrt

        // consteval
        constexpr int cs = compile_only_square(9);  // 81
        constexpr int port = make_port(9090);        // validated at compile-time
        constexpr uint64_t h = hash("SPY");          // compile-time hash

        constexpr auto squares = make_squares<8>();  // {0,1,4,9,16,25,36,49}

        std::cout << "compile_only_square(9) = " << cs   << "\n";
        std::cout << "make_port(9090)        = " << port << "\n";
        std::cout << "hash(\"SPY\")            = " << h    << "\n";
        std::cout << "squares[7]             = " << squares[7] << "\n";

        // consteval cannot be called with runtime variable:
        // int n = 5; compile_only_square(n);  ← COMPILE ERROR
        // constexpr: OK either way
        int n = 5;
        std::cout << "maybe_compiletime(5)   = " << maybe_compiletime(n) << "\n"; // OK

        // constinit
        std::cout << "\n--- constinit ---\n";
        std::cout << "server_port  = " << server_port  << "\n";
        std::cout << "tick_size    = " << tick_size    << "\n";
        std::cout << "buffer_size  = " << buffer_size  << "\n";

        // constinit IS mutable at runtime
        server_port = 9090;
        std::cout << "server_port (modified) = " << server_port << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// C++23: constexpr EVEN FURTHER EXPANDED
//
// Key additions:
//  - std::optional, std::variant constexpr
//  - static constexpr local variables
//  - constexpr std::unique_ptr (P2273)
//  - if consteval — check inside whether we're in consteval context
//  - constexpr std::bitset
//  - More standard library algorithms constexpr
// ─────────────────────────────────────────────────────────────────────────────
namespace cpp23 {

    // C++23: static constexpr local variables
    constexpr int get_magic() {
        static constexpr int magic = 42;    // NOT allowed before C++23
        return magic;
    }

    // C++23: if consteval — distinguish consteval from constexpr context
    constexpr double precise_sqrt(double x) {
        if consteval {
            // We ARE in an immediate (consteval) context — use manual method
            double r = x;
            for (int i = 0; i < 20; ++i) r = (r + x/r) / 2.0;
            return r;
        } else {
            // Runtime context — use hardware instruction
            return std::sqrt(x);
        }
    }
    // Note: if consteval is cleaner than std::is_constant_evaluated() (C++20)
    // because is_constant_evaluated() had subtle bugs in some contexts

    // C++23: constexpr unique_ptr (P2273)
    constexpr int use_smart_ptr() {
        auto p = std::make_unique<int>(42);     // C++23: constexpr unique_ptr
        *p += 8;
        return *p;                              // 50
    }

    // C++23: more STL algorithms constexpr
    constexpr int find_max() {
        std::array<int, 6> arr = {3, 1, 4, 1, 5, 9};
        return *std::max_element(arr.begin(), arr.end());   // fully constexpr C++23
    }

    void demo() {
        std::cout << "\n=== C++23 constexpr ===\n";

        constexpr int  mg  = get_magic();       // 42
        constexpr double sq = precise_sqrt(2.0);// compile-time, uses if consteval
        constexpr int  sp  = use_smart_ptr();   // 50
        constexpr int  mx  = find_max();        // 9

        std::cout << "get_magic()       = " << mg  << "\n";
        std::cout << "precise_sqrt(2.0) = " << sq  << "\n";
        std::cout << "use_smart_ptr()   = " << sp  << "\n";
        std::cout << "find_max()        = " << mx  << "\n";
    }
}

// ============================================================================
// SECTION 2: DIRECT COMPARISON — constexpr vs consteval vs constinit
// ============================================================================
namespace comparison {

    // ── 1. Evaluation guarantee ──────────────────────────────────────────────
    constexpr int cx_func(int x) { return x * x; }     // MAY be compile-time
    consteval int ce_func(int x) { return x * x; }     // MUST be compile-time

    // ── 2. consteval prevents accidental runtime use ──────────────────────
    // Use case: compile-time validated config — you WANT a compile error
    // if someone passes a runtime value
    consteval uint32_t make_symbol_id(std::string_view ticker) {
        // FNV-1a hash of ticker string → compile-time integer ID
        uint32_t h = 2166136261u;
        for (char c : ticker) {
            h ^= static_cast<uint8_t>(c);
            h *= 16777619u;
        }
        return h;
    }

    // IDs computed at compile-time — stored as constexpr integers
    constexpr uint32_t SYMBOL_SPY  = make_symbol_id("SPY");
    constexpr uint32_t SYMBOL_QQQ  = make_symbol_id("QQQ");
    constexpr uint32_t SYMBOL_AAPL = make_symbol_id("AAPL");

    // ── 3. constinit use case: latency-sensitive globals ─────────────────
    // In a trading system: global config that MUST be initialized before
    // any thread starts — constinit guarantees this

    constinit int    LOT_SIZE        = 100;
    constinit double MIN_TICK        = 0.01;
    constinit int    MAX_ORDER_SIZE  = 50'000;
    constinit bool   KILL_SWITCH     = false;

    // These can be modified by a config loader at startup (unlike constexpr)
    void load_config(int lot, double tick, int max_ord) {
        LOT_SIZE       = lot;
        MIN_TICK       = tick;
        MAX_ORDER_SIZE = max_ord;
    }

    // ── 4. std::is_constant_evaluated() — dual-mode function ─────────────
    constexpr double compute(double x) {
        if (std::is_constant_evaluated()) {
            // Compile-time path — no std library functions with runtime deps
            return x * x;
        } else {
            return std::sqrt(x * x + 0.001); // runtime: full precision
        }
    }

    void demo() {
        std::cout << "\n=== Comparison: constexpr vs consteval vs constinit ===\n";

        // constexpr: works both ways
        constexpr int ct = cx_func(5);  // compile-time
        int n = 5;
        int rt = cx_func(n);            // runtime — OK

        // consteval: compile-time only
        constexpr int ce = ce_func(5);  // compile-time — OK
        // int bad = ce_func(n);        // COMPILE ERROR: n is not constexpr

        // Symbol IDs — computed once at compile-time, used as integer constants
        std::cout << "SYMBOL_SPY  = " << SYMBOL_SPY  << "\n";
        std::cout << "SYMBOL_QQQ  = " << SYMBOL_QQQ  << "\n";
        std::cout << "SYMBOL_AAPL = " << SYMBOL_AAPL << "\n";

        // constinit — mutable globals with guaranteed static init
        std::cout << "LOT_SIZE (before) = " << LOT_SIZE << "\n";
        load_config(200, 0.001, 100'000);
        std::cout << "LOT_SIZE (after)  = " << LOT_SIZE << "\n";

        // Dual-mode function
        constexpr double c1 = compute(4.0); // compile-time path
        double x = 4.0;
        double r1 = compute(x);             // runtime path
        std::cout << "compute CT = " << c1 << ", RT = " << r1 << "\n";

        (void)ct; (void)rt; (void)ce;
    }
}

// ============================================================================
// SECTION 3: ULL TRADING — PRACTICAL USE CASES
// ============================================================================
namespace trading {

    // ── Compile-time lookup table (no runtime computation) ────────────────
    // Pre-compute sin/cos for fast options pricing
    consteval std::array<double, 360> make_sin_table() {
        std::array<double, 360> t{};
        constexpr double PI = 3.14159265358979323846;
        for (int i = 0; i < 360; ++i) {
            double r = i * PI / 180.0;
            // Newton approximation for consteval context
            // sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
            double x = r;
            t[i] = x - (x*x*x)/6.0 + (x*x*x*x*x)/120.0
                     - (x*x*x*x*x*x*x)/5040.0;
        }
        return t;
    }

    constexpr auto SIN_TABLE = make_sin_table();  // baked into binary at compile-time

    // ── Compile-time validated exchange config ─────────────────────────────
    struct ExchangeConfig {
        std::string_view name;
        int              port;
        double           tick_size;
        int              lot_size;
    };

    consteval ExchangeConfig make_exchange(std::string_view name,
                                           int port, double tick,
                                           int lot) {
        if (port < 1024)           throw "invalid port";
        if (tick <= 0.0)           throw "invalid tick size";
        if (lot <= 0)              throw "invalid lot size";
        return {name, port, tick, lot};
    }

    // All validated at compile-time — no runtime checks needed
    constexpr auto NYSE_CFG  = make_exchange("NYSE",   9000, 0.01,  100);
    [[maybe_unused]] constexpr auto NASDAQ_CFG = make_exchange("NASDAQ",9001, 0.01,  100);
    constexpr auto CME_CFG   = make_exchange("CME",    9002, 0.25,    1);

    // ── constinit for hot-path globals ────────────────────────────────────
    constinit bool   feed_connected  = false;
    constinit int    msg_count       = 0;
    constinit double last_mid_price  = 0.0;

    // ── Compile-time power-of-2 check for ring buffer ��────────────────────
    template<std::size_t N>
    consteval std::size_t validate_ring_size() {
        static_assert(N > 0 && (N & (N-1)) == 0,
                      "Ring buffer size must be power of 2");
        return N;
    }

    constexpr std::size_t RING_SIZE = validate_ring_size<1024>(); // OK
    // constexpr std::size_t BAD_SIZE = validate_ring_size<1000>(); // COMPILE ERROR

    void demo() {
        std::cout << "\n=== Trading: Practical constexpr/consteval/constinit ===\n";

        std::cout << "SIN_TABLE[90]  = " << SIN_TABLE[90]  << "  (expect ~1.0)\n";
        std::cout << "SIN_TABLE[30]  = " << SIN_TABLE[30]  << "  (expect ~0.5)\n";

        std::cout << "NYSE  port=" << NYSE_CFG.port
                  << " tick=" << NYSE_CFG.tick_size << "\n";
        std::cout << "CME   port=" << CME_CFG.port
                  << " tick=" << CME_CFG.tick_size  << "\n";

        // constinit globals — safe to read/write, init guaranteed before main()
        feed_connected = true;
        msg_count++;
        last_mid_price = 150.25;
        std::cout << "feed_connected = " << feed_connected  << "\n";
        std::cout << "msg_count      = " << msg_count       << "\n";
        std::cout << "last_mid       = " << last_mid_price  << "\n";

        std::cout << "RING_SIZE      = " << RING_SIZE << "\n";
    }
}

// ============================================================================
// SECTION 4: EVOLUTION SUMMARY TABLE (printed at runtime)
// ============================================================================
void print_evolution_table() {
    std::cout << R"(
╔═══════════════════════���══════════════════════════════════════════════════════════════╗
║              constexpr / consteval / constinit  — Evolution Table                  ║
╠══════════╦═══════════════════════════════════════════════════════════════════════════╣
║ Standard ║ What Changed                                                             ║
╠══════════╬══════════��════════════════════════════════════════════════════════════════╣
║  C++11   ║ constexpr INTRODUCED                                                     ║
║          ║  • Single return expression only                                         ║
║          ║  • No local vars, no loops, no if/else                                   ║
║          ║  • Recursion allowed                                                     ║
║          ║  • constexpr constructors                                                ║
║          ║  • constexpr member functions (implicitly const)                         ║
╠══════════╬═══════════════════════════════════════════════════════════════════════════╣
║  C++14   ║ constexpr RELAXED                                                        ║
║          ║  • Local variables allowed                                               ║
║          ║  • Loops (for, while) allowed                                            ║
║          ║  • if/else, switch allowed                                               ║
║          ║  • Multiple return statements                                            ║
║          ║  • constexpr member functions no longer implicitly const                 ║
║          ║  • Mutation of local variables inside constexpr OK                       ║
╠══════════╬═══════════════════════════════════════════════════════════════════════════╣
║  C++17   ║ constexpr EXPANDED + new language features                               ║
║          ║  • constexpr lambdas                                                     ║
║          ║  • if constexpr (compile-time branch selection)                          ║
║          ║  • std::string_view constexpr                                            ║
║          ║  • More STL algorithms become constexpr                                  ║
║          ║  • Fold expressions (used with constexpr templates)                      ║
╠══════════╬═══════════════════════════════════════════════════════════════════════════╣
║  C++20   ║ constexpr MASSIVE EXPANSION                                              ║
║          ║  • std::vector, std::string usable in constexpr                          ║
║          ║  • virtual functions can be constexpr                                    ║
║          ║  • dynamic_cast in constexpr                                             ║
║          ║  • try/catch blocks (not throw) in constexpr                             ║
║          ║  • std::is_constant_evaluated()                                          ║
║          ║                                                                          ║
║          ║ consteval INTRODUCED (immediate functions)                               ║
║          ║  • MUST be evaluated at compile-time                                     ║
║          ║  • Calling with runtime arg = compile error                              ║
║          ║  • Use for: validated configs, lookup tables, compile-time IDs           ║
║          ║                                                                          ║
║          ║ constinit INTRODUCED                                                     ║
║          ║  • Guarantees static initialization (before runtime)                     ║
║          ║  • Solves static initialization order fiasco                             ║
║          ║  • Variable IS mutable at runtime (unlike constexpr)                     ║
║          ║  • Only for static/thread_local storage duration                         ║
╠══════════╬═══════════════════════════════════════════════════════════════════════════╣
║  C++23   ║ constexpr FURTHER EXPANDED                                               ║
║          ║  • static constexpr local variables                                      ║
║          ║  • if consteval (cleaner than is_constant_evaluated)                     ║
║          ║  • constexpr std::unique_ptr                                             ║
║          ║  • constexpr std::optional, std::variant                                 ║
║          ║  • More STL algorithms become constexpr                                  ║
╚══════════╩═══════════════════════════════════════════════════════════════════════════╝
)";

    std::cout << R"(
╔══════════════════════════════════════════════════════════════════════╗
║          constexpr vs consteval vs constinit — Key Differences      ║
╠══════════════╦═══════════════╦═══════════════╦════════════════════════╣
║ Property     ║   constexpr   ║  consteval    ║   constinit            ║
╠══════════════╬═══════════════╬═══════════════╬════════════════════════╣
║ Introduced   ║    C++11      ║   C++20       ║   C++20                ║
║ Applies to   ║ var/fn/ctor   ║ functions only║ variables only         ║
║ Compile-time ║ MAY be        ║ MUST be       ║ init MUST be           ║
║ Runtime use  ║ YES           ║ NO            ║ YES (mutable)          ║
║ Const?       ║ YES (var)     ║ N/A           ║ NO                     ║
║ Storage      ║ any           ║ N/A           ║ static/thread_local    ║
║ Use for      ║ dual-mode fn  ║ compile-only  ║ safe global state      ║
╚══════════════╩═══════════════╩═══════════════╩════════════════════════╝
)";
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "============================================================\n";
    std::cout << "  constexpr / consteval / constinit — Complete Evolution\n";
    std::cout << "============================================================\n";

    cpp11::demo();
    cpp14::demo();
    cpp17::demo();
    cpp20::demo();
    cpp23::demo();
    comparison::demo();
    trading::demo();
    print_evolution_table();

    std::cout << "\n✅ All demos completed successfully.\n";
    return 0;
}

