// =============================================================================
// Problem 1: Count Maximum Days (Borrower / Lender)
// =============================================================================
// A borrower visits lenders one per day.
//   - Each day they pick ONE unused lender j, borrow lend[j].
//   - They repay the PREVIOUS day's loan (payback[prev]).
//   - They need lend[j] >= payback[prev] or they DEFAULT.
//   - The new debt becomes payback[j].
//
// Find the MAXIMUM number of days before a default.
//
// Constraints:  1 <= n <= 1e5,  1 <= lend[i] <= payback[i] <= 1e9
//
// Strategy (Greedy – O(n log n)):
//   Pick the valid lender (lend >= current_debt) with MINIMUM payback each day.
//   Debt is non-decreasing (lend[i] <= payback[i]), so lazy deletion is safe.
//
// ── Ultra-Low-Latency techniques applied ─────────────────────────────────────
//   1. int64 bit-packing: encode {payback, lend} as ONE uint64_t
//        bits[63:32] = payback,  bits[31:0] = lend
//      → single 64-bit comparison in every heap op  (was: two-level pair<int,int>)
//      → heap elements are 8 bytes, same as pair but compared with ONE instruction
//   2. O(n) heap build via make_heap + move  (was: n × O(log n) push)
//   3. __builtin_expect on lazy-delete branch  → tells GCC the "skip" path is rare
//   4. priority_queue backed by pre-reserved vector  → zero realloc during pop
//   5. Extended pragma target: avx2 + bmi/bmi2 + popcnt  → wider SIMD, bit ops
// =============================================================================

// Compiler support:
//   GCC (primary)  : pragmas enable O3+AVX2+BMI at TU level
//   Clang (second) : pragmas guarded; all other constructs supported natively.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC optimize("O3,unroll-loops")
#  pragma GCC target("avx2,bmi,bmi2,popcnt")
#endif
#include <cstdint>
#include <vector>
#include <queue>
#include <utility>
#include <iostream>
using namespace std;

// ---------------------------------------------------------------------------
// Bit-packing helpers (no branches, pure arithmetic)
// payback in bits [63:32], lend in bits [31:0]
// Both fields fit in 30 bits (max = 1e9 < 2^30), sign bit of each half is 0.
// Heap comparison on uint64 orders by payback first → correct min-heap by payback.
// ---------------------------------------------------------------------------
using u64 = uint64_t;

[[nodiscard]] static inline u64 pack(int pb, int ln) noexcept {
    return ((u64)(unsigned int)pb << 32) | (unsigned int)ln;
}
[[nodiscard]] static inline int unpack_pb(u64 v) noexcept { return (int)(v >> 32); }
[[nodiscard]] static inline int unpack_ln(u64 v) noexcept { return (int)(unsigned int)v; }

// ---------------------------------------------------------------------------
// Core function
// ---------------------------------------------------------------------------
int countMaximumDays(vector<int> lend, vector<int> payback) {
    const int n = static_cast<int>(lend.size());
    if (__builtin_expect(n == 0, 0)) return 0;

    // Build backing vector with O(1) amortised push, then O(n) heapify.
    vector<u64> backing;
    backing.reserve(n);
    for (int i = 0; i < n; ++i)
        backing.push_back(pack(payback[i], lend[i]));

    make_heap(backing.begin(), backing.end(), greater<u64>{});          // O(n)
    priority_queue<u64, vector<u64>, greater<u64>>
        pq(greater<u64>{}, std::move(backing));                         // zero copy

    int       days         = 0;
    long long current_debt = 0;

    while (__builtin_expect(!pq.empty(), 1)) {
        const u64 top = pq.top();  pq.pop();
        const int ln  = unpack_ln(top);

        // Lazy deletion: debt is non-decreasing so lend < debt → permanently invalid.
        // __builtin_expect(..., 0): this branch is taken rarely (most lenders are valid).
        if (__builtin_expect(ln < current_debt, 0)) continue;

        current_debt = unpack_pb(top);
        ++days;
    }

    return days;
}

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------
int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;  cin >> n;
    vector<int> lend(n), payback(n);
    for (int i = 0; i < n; ++i) cin >> lend[i];
    for (int i = 0; i < n; ++i) cin >> payback[i];

    cout << countMaximumDays(lend, payback) << '\n';
    return 0;
}

// ---------------------------------------------------------------------------
// Complexity
//   Time : O(n log n)  – O(n) build + each element popped once
//   Space: O(n)        – single contiguous heap (vector), no tree nodes
//
// Why bit-packing beats pair<int,int>:
//   pair comparison:  load a.first, load b.first, cmp, branch, load a.second…
//   uint64 comparison: load a, load b, cmp  ← ONE instruction per heap sift
//
// Sample:  lend=[2,1,5], payback=[2,2,5] → 3
//   pop(u64 for pb=2,ln=1): ln=1>=0 → debt=2, days=1
//   pop(u64 for pb=2,ln=2): ln=2>=2 → debt=2, days=2
//   pop(u64 for pb=5,ln=5): ln=5>=2 → debt=5, days=3
// ---------------------------------------------------------------------------
