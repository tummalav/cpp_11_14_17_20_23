/*
 * ============================================================================
 * C++23 Features Overview - Comprehensive Examples
 * ============================================================================
 *
 * C++23 is the fifth major revision of the C++ standard. It builds on C++20
 * and adds significant library and language improvements.
 *
 * Key C++23 Features:
 *  1.  std::print / std::println         - type-safe printf replacement
 *  2.  std::expected<T,E>                - error handling without exceptions
 *  3.  if consteval                      - distinguish consteval vs runtime
 *  4.  std::flat_map / std::flat_set     - cache-friendly sorted containers
 *  5.  std::mdspan                       - multidimensional span
 *  6.  std::ranges improvements          - zip, chunk, slide, enumerate
 *  7.  Deducing this (explicit object parameter)
 *  8.  std::generator<T>                 - stackless coroutine generator
 *  9.  std::stacktrace                   - portable stack trace
 *  10. Multidimensional subscript operator[]
 *  11. std::byteswap, std::to_underlying
 *  12. Attributes: [[assume(expr)]]
 *  13. import std;                       - standard library as module
 *  14. static operator() and operator[]
 * ============================================================================
 *
 * NOTE: Full C++23 support varies by compiler:
 *   - GCC 14+   (g++ -std=c++23)
 *   - Clang 17+ (clang++ -std=c++23)
 *   - MSVC  19.36+ (/std:c++23preview)
 *
 * Compile: g++ -std=c++23 -Wall -Wextra -O2 cpp23_features_overview.cpp
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <expected>
#include <print>
#include <ranges>
#include <span>
#include <algorithm>
#include <numeric>
#include <utility>
#include <type_traits>
#include <bit>
#include <cstdint>
#include <optional>
#include <variant>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <flat_map>
#include <flat_set>
#include <generator>
#include <stacktrace>
#include <mdspan>
#include <format>

// ============================================================================
// 1. std::print / std::println
// ============================================================================
// Combines the type safety of std::format with the simplicity of printf.
// No more mis-matched format specifiers vs argument types.

void demonstrate_print() {
    std::println("=== 1. std::print / std::println ===");

    // std::print does NOT append newline
    std::print("Hello, ");
    std::print("World!\n");

    // std::println appends newline automatically
    std::println("Name: {}, Age: {}, Score: {:.2f}", "Alice", 30, 98.765);

    // Type-safe: won't compile with wrong types
    int  qty   = 1000;
    double price = 150.75;
    std::println("Order: {:>8} shares @ ${:.4f}", qty, price);

    // Print to stderr
    std::print(stderr, "Error: {}\n", "something went wrong");

    // Padding, alignment
    std::println("{:^20}", "centered");
    std::println("{:<20}|{:>20}", "left", "right");
    std::println("{:#010x}", 0xDEADBEEF);
}

// ============================================================================
// 2. std::expected<T,E>
// ============================================================================
// Represents either a value T or an error E.
// Alternative to exceptions for error-prone operations on hot paths.
// Very popular in trading/HFT code where exceptions are banned.

enum class ParseError { EMPTY_INPUT, INVALID_FORMAT, OUT_OF_RANGE };

std::expected<double, ParseError> parse_price(std::string_view input) {
    if (input.empty()) return std::unexpected(ParseError::EMPTY_INPUT);

    try {
        size_t pos = 0;
        double val = std::stod(std::string(input), &pos);
        if (pos != input.size()) return std::unexpected(ParseError::INVALID_FORMAT);
        if (val < 0 || val > 1'000'000) return std::unexpected(ParseError::OUT_OF_RANGE);
        return val;
    } catch (...) {
        return std::unexpected(ParseError::INVALID_FORMAT);
    }
}

std::expected<int64_t, ParseError> parse_quantity(std::string_view input) {
    if (input.empty()) return std::unexpected(ParseError::EMPTY_INPUT);
    try {
        size_t pos = 0;
        long long val = std::stoll(std::string(input), &pos);
        if (pos != input.size()) return std::unexpected(ParseError::INVALID_FORMAT);
        if (val <= 0) return std::unexpected(ParseError::OUT_OF_RANGE);
        return static_cast<int64_t>(val);
    } catch (...) {
        return std::unexpected(ParseError::INVALID_FORMAT);
    }
}

// Chain multiple expected operations with monadic interface
std::expected<double, ParseError> compute_notional(std::string_view price_str,
                                                    std::string_view qty_str) {
    return parse_price(price_str)
        .and_then([&](double p) -> std::expected<double, ParseError> {
            return parse_quantity(qty_str)
                .transform([p](int64_t q) { return p * q; });
        });
}

void demonstrate_expected() {
    std::println("\n=== 2. std::expected<T,E> ===");

    auto results = std::vector<std::pair<std::string_view, std::string_view>>{
        {"150.75", "1000"},
        {"",       "500"},
        {"bad",    "200"},
        {"99.99",  "-5"},
        {"50.00",  "2000"},
    };

    for (auto [p, q] : results) {
        auto result = compute_notional(p, q);
        if (result) {
            std::println("  price={:>8s} qty={:>6s} -> notional = {:>12.2f}", p, q, *result);
        } else {
            std::string_view err;
            switch (result.error()) {
                case ParseError::EMPTY_INPUT:    err = "EMPTY_INPUT";    break;
                case ParseError::INVALID_FORMAT: err = "INVALID_FORMAT"; break;
                case ParseError::OUT_OF_RANGE:   err = "OUT_OF_RANGE";   break;
            }
            std::println("  price={:>8s} qty={:>6s} -> ERROR: {}", p, q, err);
        }
    }

    // value_or for defaults
    auto safe_price = parse_price("invalid").value_or(0.0);
    std::println("Safe price with value_or: {}", safe_price);

    // or_else for error handling
    auto handled = parse_price("")
        .or_else([](ParseError e) -> std::expected<double, ParseError> {
            if (e == ParseError::EMPTY_INPUT) return 0.0;  // default for empty
            return std::unexpected(e);
        });
    std::println("Empty input handled: {}", *handled);
}

// ============================================================================
// 3. if consteval
// ============================================================================
// Allows distinguishing between compile-time and runtime evaluation
// within a constexpr/consteval context.

constexpr double fast_sqrt(double x) {
    if consteval {
        // Compile-time: use a simple iterative method (no FPU)
        double result = x;
        double prev   = 0.0;
        while (result != prev) {
            prev   = result;
            result = 0.5 * (result + x / result);
        }
        return result;
    } else {
        // Runtime: use hardware sqrt (x86 SQRTSD or ARM VFP)
        return __builtin_sqrt(x);
    }
}

consteval double compile_time_sqrt(double x) {
    return fast_sqrt(x);  // always compile-time
}

void demonstrate_if_consteval() {
    std::println("\n=== 3. if consteval ===");

    // Compile-time
    constexpr double ct_sqrt2  = compile_time_sqrt(2.0);
    constexpr double ct_sqrt16 = compile_time_sqrt(16.0);
    std::println("sqrt(2)  at compile time: {:.10f}", ct_sqrt2);
    std::println("sqrt(16) at compile time: {:.10f}", ct_sqrt16);

    // Runtime
    double x = 2.0;
    double rt_sqrt = fast_sqrt(x);  // uses __builtin_sqrt at runtime
    std::println("sqrt(2)  at runtime:      {:.10f}", rt_sqrt);

    static_assert(compile_time_sqrt(4.0) == 2.0);
}

// ============================================================================
// 4. std::flat_map / std::flat_set
// ============================================================================
// Sorted, contiguous containers - better cache performance than std::map/set.
// O(log n) lookup like map/set, but stored as sorted vector pairs.
// MUCH better for read-heavy, low-insertion workloads.

void demonstrate_flat_containers() {
    std::println("\n=== 4. std::flat_map / std::flat_set ===");

    // flat_map: keys and values stored in separate contiguous arrays
    std::flat_map<std::string, double> prices;
    prices["AAPL"] = 150.50;
    prices["GOOG"] = 2800.00;
    prices["MSFT"] = 330.25;
    prices["TSLA"] = 800.00;

    // O(log n) lookup, iterates contiguously (cache-friendly)
    auto it = prices.find("GOOG");
    if (it != prices.end()) {
        std::println("GOOG price: {:.2f}", it->second);
    }

    // Update
    prices["AAPL"] = 155.00;

    std::println("All prices:");
    for (auto& [sym, px] : prices) {
        std::println("  {:6s}: {:.2f}", sym, px);
    }

    // flat_set: single contiguous sorted array
    std::flat_set<std::string> universe{"AAPL", "GOOG", "MSFT", "AMZN"};
    universe.insert("TSLA");
    std::println("Universe size: {}", universe.size());
    std::println("Contains AAPL: {}", universe.contains("AAPL"));
    std::println("Contains META: {}", universe.contains("META"));

    // flat_map vs map: flat_map has better iteration cache behaviour
    // because keys and values are stored in contiguous memory
    std::println("flat_map is cache-friendly: keys contiguous, values contiguous");
}

// ============================================================================
// 5. std::mdspan (Multidimensional Span)
// ============================================================================
// Non-owning view of multidimensional data with customizable layout.
// Critical for HPC, trading analytics (price matrix, correlation matrix).

void demonstrate_mdspan() {
    std::println("\n=== 5. std::mdspan ===");

    // 2D price matrix: [symbol_idx][time_step]
    const int N_SYMS = 3, N_TICKS = 4;
    std::vector<double> raw(N_SYMS * N_TICKS);

    // Fill with mock prices
    for (int i = 0; i < N_SYMS; ++i)
        for (int j = 0; j < N_TICKS; ++j)
            raw[i * N_TICKS + j] = 100.0 + i * 10 + j * 0.5;

    // Create 2D view over raw data (row-major by default)
    auto price_matrix = std::mdspan(raw.data(), N_SYMS, N_TICKS);

    std::println("Price matrix [symbol x time]:");
    for (int i = 0; i < N_SYMS; ++i) {
        std::print("  Symbol {}: ", i);
        for (int j = 0; j < N_TICKS; ++j) {
            std::print("{:.1f} ", price_matrix[i, j]);  // C++23 multi-dim subscript!
        }
        std::println("");
    }

    // 3D tensor: [date][symbol][feature]
    const int N_DATES = 2, N_FEATURES = 3;
    std::vector<double> tensor_data(N_DATES * N_SYMS * N_FEATURES, 0.0);
    auto tensor = std::mdspan(tensor_data.data(), N_DATES, N_SYMS, N_FEATURES);

    tensor[0, 0, 0] = 1.5;   // date=0, sym=0, feat=0 = VWAP
    tensor[0, 0, 1] = 2.3;   // date=0, sym=0, feat=1 = spread
    tensor[1, 2, 2] = 99.9;
    std::println("tensor[0,0,0] = {}, tensor[1,2,2] = {}", tensor[0,0,0], tensor[1,2,2]);

    // Different memory layouts
    using RowMajor = std::layout_right;   // C-order (default)
    using ColMajor = std::layout_left;    // Fortran-order

    std::vector<double> col_data(N_SYMS * N_TICKS);
    auto col_view = std::mdspan<double, std::dextents<int, 2>, ColMajor>(
        col_data.data(), N_SYMS, N_TICKS);
    std::println("Column-major mdspan created (layout_left)");
}

// ============================================================================
// 6. std::ranges ADDITIONS (zip, chunk, slide, enumerate, to)
// ============================================================================

void demonstrate_ranges_additions() {
    std::println("\n=== 6. std::ranges Additions ===");

    std::vector<std::string> symbols  = {"AAPL", "GOOG", "MSFT", "TSLA", "AMZN"};
    std::vector<double>      prices   = {150.5,  2800.0, 330.25, 800.0,  3400.0};
    std::vector<int>         volumes  = {5000,   200,    1500,   800,    300};

    // ZIP: iterate multiple ranges together
    std::println("Zip symbols + prices:");
    for (auto [sym, px] : std::views::zip(symbols, prices)) {
        std::print("  {:6s}: {:.2f}  ", sym, px);
    }
    std::println("");

    // ENUMERATE: index + value
    std::println("Enumerate:");
    for (auto [idx, sym] : std::views::enumerate(symbols)) {
        std::println("  [{}] {}", idx, sym);
    }

    // CHUNK: split into fixed-size groups
    std::println("Chunks of 2:");
    for (auto chunk : symbols | std::views::chunk(2)) {
        std::print("  [");
        for (auto& s : chunk) std::print(" {}", s);
        std::println(" ]");
    }

    // SLIDE: sliding window
    std::vector<double> returns = {0.01, -0.02, 0.03, 0.015, -0.01, 0.025};
    std::println("Slide window=3 on returns:");
    for (auto window : returns | std::views::slide(3)) {
        double sum = 0;
        for (auto v : window) sum += v;
        std::print("  avg={:.4f}", sum / 3);
    }
    std::println("");

    // CARTESIAN_PRODUCT
    std::vector<int> bid_sizes = {100, 200};
    std::vector<int> ask_sizes = {50,  150, 300};
    std::println("Cartesian product (bid x ask sizes):");
    for (auto [b, a] : std::views::cartesian_product(bid_sizes, ask_sizes)) {
        std::print("  ({},{})", b, a);
    }
    std::println("");

    // RANGES_TO: collect into container
    auto big_prices = prices
        | std::views::filter([](double p) { return p > 500.0; })
        | std::ranges::to<std::vector<double>>();
    std::println("Prices > 500: {}", big_prices.size());

    // ADJACENT / ADJACENT_TRANSFORM: compute differences
    std::println("Adjacent pairs:");
    for (auto [a, b] : prices | std::views::adjacent<2>) {
        std::print("  ({:.1f},{:.1f})", a, b);
    }
    std::println("");
}

// ============================================================================
// 7. DEDUCING THIS (Explicit Object Parameter)
// ============================================================================
// Allows the type of 'this' to be deduced, enabling:
//  - CRTP without a base class
//  - Unified const/non-const member functions
//  - Recursive lambdas

void demonstrate_deducing_this() {
    std::println("\n=== 7. Deducing this ===");

    // 1. Recursive lambda (was impossible before C++23)
    auto fibonacci = [](this auto self, int n) -> int {
        return n <= 1 ? n : self(n - 1) + self(n - 2);
    };
    std::println("fib(10) = {}", fibonacci(10));

    // 2. Unified const/non-const member via deducing this
    struct OrderBook {
        std::vector<double> bids = {150.0, 149.5, 149.0};

        // Single function handles both const and non-const
        auto& best_bid(this auto& self) {
            return self.bids.front();
        }
    };

    OrderBook ob;
    ob.best_bid() = 151.0;  // non-const: modifies
    const OrderBook& cob = ob;
    std::println("Best bid (const): {}", cob.best_bid());  // const: read-only

    // 3. CRTP without a base class template
    struct Printable {
        void print(this auto const& self) {
            // 'self' is the most-derived type
            std::println("Printing: {}", self.to_string());
        }
    };

    struct Order : Printable {
        uint64_t id; double price; uint64_t qty;
        std::string to_string() const {
            return std::format("Order[{}] {}@{:.2f}", id, qty, price);
        }
    };

    Order o{42, 150.50, 1000};
    o.print();  // calls Order::to_string() via deducing this

    // 4. Perfect forwarding member functions
    struct Event {
        std::string data;
        template<typename Self>
        auto get_data(this Self&& self) -> decltype(auto) {
            return std::forward<Self>(self).data;
        }
    };
    Event ev{"tick_data"};
    std::println("Event data: {}", ev.get_data());
    std::println("Moved data: {}", std::move(ev).get_data());
}

// ============================================================================
// 8. std::generator<T> - Stackless Coroutine Generator
// ============================================================================
// Lazily generates a sequence of values using coroutines.
// Perfect for on-demand tick data, synthetic order flow, etc.

std::generator<double> price_stream(double start, double tick_size, int n) {
    double price = start;
    for (int i = 0; i < n; ++i) {
        co_yield price;
        // Simulate random-walk price movement
        price += (i % 2 == 0 ? tick_size : -tick_size * 0.5);
    }
}

std::generator<std::pair<std::string, double>> symbol_price_stream(
    std::vector<std::string> syms, double base_price) {
    int i = 0;
    for (;;) {
        co_yield {syms[i % syms.size()], base_price + i * 0.01};
        ++i;
    }
}

void demonstrate_generator() {
    std::println("\n=== 8. std::generator<T> ===");

    // Consume a finite sequence
    std::println("Price stream:");
    for (double p : price_stream(100.0, 0.25, 8)) {
        std::print("{:.2f} ", p);
    }
    std::println("");

    // Take first N from infinite stream
    std::vector<std::string> syms = {"AAPL", "GOOG", "MSFT"};
    int count = 0;
    for (auto [sym, px] : symbol_price_stream(syms, 100.0)) {
        std::println("  {}: {:.2f}", sym, px);
        if (++count >= 6) break;
    }

    // Generator with ranges
    auto prices_vec = price_stream(50.0, 0.50, 5)
        | std::ranges::to<std::vector<double>>();
    std::println("Collected {} prices", prices_vec.size());
}

// ============================================================================
// 9. std::stacktrace
// ============================================================================
// Portable stack trace capture and display (no more platform-specific hacks).

void inner_function() {
    auto trace = std::stacktrace::current();
    std::println("=== Stack trace (top 5 frames) ===");
    int n = 0;
    for (auto& frame : trace) {
        if (n++ >= 5) break;
        std::println("  [{}] {}:{}", n, frame.description(), frame.source_line());
    }
}

void middle_function() { inner_function(); }
void outer_function()  { middle_function(); }

void demonstrate_stacktrace() {
    std::println("\n=== 9. std::stacktrace ===");
    outer_function();

    // In real trading systems: log stacktrace on assertion failure or error
    auto on_critical_error = [](std::string_view msg) {
        auto trace = std::stacktrace::current();
        std::println("CRITICAL: {}", msg);
        std::print("{}", std::to_string(trace));
    };
    // on_critical_error("Order rejected: invalid price");  // would print full trace
    std::println("(stacktrace logging enabled for critical errors)");
}

// ============================================================================
// 10. Multidimensional Subscript Operator[]
// ============================================================================
// operator[] can now take multiple arguments in C++23.

struct Matrix {
    int rows, cols;
    std::vector<double> data;

    Matrix(int r, int c) : rows(r), cols(c), data(r * c, 0.0) {}

    // Multi-dimensional subscript (C++23)
    double& operator[](int r, int c) {
        return data[r * cols + c];
    }
    const double& operator[](int r, int c) const {
        return data[r * cols + c];
    }

    void print() const {
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j)
                std::print("{:8.2f}", (*this)[i, j]);
            std::println("");
        }
    }
};

void demonstrate_multidim_subscript() {
    std::println("\n=== 10. Multidimensional operator[] ===");

    Matrix correlation(3, 3);
    // Set correlation matrix (identity + some off-diag)
    correlation[0, 0] = 1.0;  correlation[0, 1] = 0.3;  correlation[0, 2] = 0.1;
    correlation[1, 0] = 0.3;  correlation[1, 1] = 1.0;  correlation[1, 2] = 0.5;
    correlation[2, 0] = 0.1;  correlation[2, 1] = 0.5;  correlation[2, 2] = 1.0;

    std::println("Correlation matrix:");
    correlation.print();
}

// ============================================================================
// 11. std::byteswap, std::to_underlying
// ============================================================================

enum class OrderStatus : uint8_t {
    NEW       = 0x01,
    PARTIAL   = 0x02,
    FILLED    = 0x04,
    CANCELLED = 0x08,
};

void demonstrate_byteswap_to_underlying() {
    std::println("\n=== 11. std::byteswap + std::to_underlying ===");

    // std::byteswap: reverse bytes (useful for network byte order)
    uint32_t host_price = 0x00015E94;  // 89748 = 897.48 in fixed-point
    uint32_t net_price  = std::byteswap(host_price);
    std::println("host: {:08x} -> network: {:08x}", host_price, net_price);
    std::println("Roundtrip: {:08x}", std::byteswap(net_price));

    // Useful for FIX/ITCH binary protocol parsing
    uint16_t msg_len_be = 0x0102;  // big-endian from wire
    uint16_t msg_len_le = std::byteswap(msg_len_be);
    std::println("BE msg_len: {} -> LE: {}", msg_len_be, msg_len_le);

    // std::to_underlying: get the underlying integer of an enum
    OrderStatus status = OrderStatus::PARTIAL;
    auto raw = std::to_underlying(status);  // = 0x02
    std::println("OrderStatus::PARTIAL underlying value: {:#04x}", raw);

    // Useful for bitmask operations with enums
    auto is_active = [](OrderStatus s) {
        auto v = std::to_underlying(s);
        return (v & std::to_underlying(OrderStatus::NEW)) ||
               (v & std::to_underlying(OrderStatus::PARTIAL));
    };
    std::println("NEW is active:     {}", is_active(OrderStatus::NEW));
    std::println("FILLED is active:  {}", is_active(OrderStatus::FILLED));
}

// ============================================================================
// 12. [[assume(expr)]] ATTRIBUTE
// ============================================================================
// Tells the compiler to assume an expression is always true.
// Enables aggressive optimizations without undefined behavior of __builtin_unreachable.

double process_positive_price(double price) {
    [[assume(price > 0.0)]];  // hint: price is always positive
    [[assume(price < 1'000'000.0)]];
    // Compiler can now optimize sqrt, log, etc. without negative-check paths
    return price * price;
}

void demonstrate_assume() {
    std::println("\n=== 12. [[assume(expr)]] ===");
    std::println("process_positive_price(150.5) = {:.2f}", process_positive_price(150.5));
    std::println("[[assume]] gives optimizer hints at zero runtime cost");

    // Trading example: qty is always > 0 and <= max_qty
    auto fill_order = [](int64_t qty, double price) {
        [[assume(qty > 0)]];
        [[assume(qty <= 1'000'000)]];
        [[assume(price > 0.0)]];
        return static_cast<double>(qty) * price;
    };
    std::println("fill_order(1000, 150.5) = {:.2f}", fill_order(1000, 150.5));
}

// ============================================================================
// 13. static operator() and static operator[]
// ============================================================================
// Stateless function objects can now declare operator() as static,
// which avoids passing the implicit 'this' pointer.

struct PriceTicker {
    // C++23: static operator() for stateless functor
    static double operator()(double price, double tick) {
        return std::round(price / tick) * tick;
    }
};

struct SymbolHasher {
    static size_t operator[](std::string_view sym) {
        size_t hash = 0;
        for (char c : sym) hash = hash * 31 + c;
        return hash;
    }
};

void demonstrate_static_call_operator() {
    std::println("\n=== 13. static operator() / operator[] ===");

    // No instance needed (or instance with no overhead)
    std::println("Round 150.37 to 0.25 tick: {:.2f}", PriceTicker{}(150.37, 0.25));
    std::println("Round 99.13 to 0.05 tick:  {:.2f}", PriceTicker{}(99.13,  0.05));

    // static operator[] for hasher
    std::println("Hash(AAPL) = {}", SymbolHasher{}["AAPL"]);
    std::println("Hash(GOOG) = {}", SymbolHasher{}["GOOG"]);

    // Use in std::sort with stateless comparator (no 'this' overhead)
    std::vector<double> prices = {101.5, 99.0, 100.25, 98.75, 102.0};
    struct PriceDesc {
        static bool operator()(double a, double b) { return a > b; }
    };
    std::sort(prices.begin(), prices.end(), PriceDesc{});
    std::print("Sorted desc: ");
    for (auto p : prices) std::print("{:.2f} ", p);
    std::println("");
}

// ============================================================================
// 14. TRADING SYSTEM PRACTICAL EXAMPLE - C++23 Features Combined
// ============================================================================
// std::expected for error handling, std::flat_map for order cache,
// std::generator for order flow, std::print for structured logging

enum class SubmitError : uint8_t {
    INVALID_SYMBOL,
    INVALID_PRICE,
    INVALID_QTY,
    DUPLICATE_ID,
    RISK_BREACH,
};

struct TradeOrder {
    uint64_t    id;
    std::string symbol;
    double      price;
    int64_t     qty;
    bool        is_buy;
};

std::expected<TradeOrder, SubmitError>
validate_order(uint64_t id, std::string_view sym, double price,
               int64_t qty, bool is_buy) {
    if (sym.empty() || sym.size() > 6)
        return std::unexpected(SubmitError::INVALID_SYMBOL);
    if (price <= 0.0 || price > 1'000'000.0)
        return std::unexpected(SubmitError::INVALID_PRICE);
    if (qty <= 0 || qty > 10'000'000)
        return std::unexpected(SubmitError::INVALID_QTY);

    return TradeOrder{id, std::string(sym), price, qty, is_buy};
}

std::generator<TradeOrder> synthetic_order_flow(int count) {
    static uint64_t id = 0;
    std::vector<std::string> syms = {"AAPL", "GOOG", "MSFT", "TSLA"};
    double prices[] = {150.0, 2800.0, 330.0, 800.0};

    for (int i = 0; i < count; ++i) {
        size_t idx = i % 4;
        co_yield TradeOrder{
            ++id, syms[idx], prices[idx] + i * 0.01,
            (i % 3 + 1) * 100LL, i % 2 == 0
        };
    }
}

void demonstrate_trading_cpp23() {
    std::println("\n=== Trading Example (C++23 Features Combined) ===");

    // Order cache: flat_map for cache-friendly lookup
    std::flat_map<uint64_t, TradeOrder> order_cache;

    // Process synthetic order flow
    int accepted = 0, rejected = 0;
    for (auto order : synthetic_order_flow(10)) {
        auto result = validate_order(order.id, order.symbol,
                                     order.price, order.qty, order.is_buy);
        if (result) {
            order_cache[result->id] = *result;
            std::println("  ACCEPTED {:>4}: {:6s} {} {:>6} @ {:.2f}",
                result->id, result->symbol,
                result->is_buy ? "BUY " : "SELL",
                result->qty, result->price);
            ++accepted;
        } else {
            std::println("  REJECTED {:>4}: error={}", order.id,
                         std::to_underlying(result.error()));
            ++rejected;
        }
    }

    std::println("Summary: {} accepted, {} rejected", accepted, rejected);
    std::println("Order cache size: {}", order_cache.size());

    // Lookup via flat_map (cache-friendly)
    if (auto it = order_cache.find(3); it != order_cache.end()) {
        std::println("Found order 3: {} {}", it->second.symbol, it->second.qty);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::println("============================================");
    std::println("        C++23 Features Overview");
    std::println("============================================");

    demonstrate_print();
    demonstrate_expected();
    demonstrate_if_consteval();
    demonstrate_flat_containers();
    demonstrate_mdspan();
    demonstrate_ranges_additions();
    demonstrate_deducing_this();
    demonstrate_generator();
    demonstrate_stacktrace();
    demonstrate_multidim_subscript();
    demonstrate_byteswap_to_underlying();
    demonstrate_assume();
    demonstrate_static_call_operator();
    demonstrate_trading_cpp23();

    std::println("\n=== All C++23 Features Demonstrated ===");
    return 0;
}

