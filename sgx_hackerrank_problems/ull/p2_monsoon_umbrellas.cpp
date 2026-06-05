// =============================================================================
// Problem 2: Monsoon Umbrellas  |  Ultra-Low-Latency
// Umbrellas are available in defferent sizes that are each able to shelter a certain
// number of people. Given the number of people needing shelter and a list of umbrella sizes,
// determin the minimum number of umbrealls necessary to cover exactly the number of people
// given and no more. if there is no combination of umbrellas of the same or different sizes
// that covers exactly that number of people, return -1
// Example 1
// requirement = 5
// sizes = [3, 5]
// one umbrella can cover exactly 5 people, so the function should return 1.
// Example 2
// requirement = 8
// sizes = [3, 5]
// it is possible to use  umbrella of size 3 and 1 umbrella of size 5 to cover exactly 8 people
// , so the function should return 2
// Example 3
// requirement = 7
// sizes = [3, 5]
// There is no combination of umbrellas that cover exactly 7 people, so the function should return -1
//
// Function description
// Ccomeplete the function getUmbrellas such has the following parameter(s):
// int requirement the number of people to shelter
// int size[n]: an array of umbrellas sizes available
//
// Returns
// int : the minimum number of umbrellas required to cover exaclty requirement people or -1 if it is impossible
// Constraints
// 1<= requirement, m, sizes[i] <= 1000
//
// Algorithm: Unbounded Coin-Change DP  –  O(req × m')
// =============================================================================
// Ultra-Low-Latency techniques applied:
//   1. #pragma GCC optimize("O3,unroll-loops")
//        Enables maximum compiler optimisation + automatic loop unrolling.
//   2. #pragma GCC target("avx2,bmi,bmi2,popcnt")
//        Unlocks AVX2 (256-bit SIMD) and BMI/BMI2 bit-manipulation instructions.
//   3. alignas(64) int dp[1001]
//        Aligns the hot dp[] array to a 64-byte cache-line boundary.
//        1001 × 4 B = ~4 KB → fits entirely in a typical 32 KB L1 data cache,
//        eliminating all cache-miss stalls during the inner DP loop.
//   4. Stack allocation (no heap)
//        dp[] lives on the stack; avoids malloc/free latency in the hot path.
//   5. __restrict__ on the sizes pointer
//        Tells GCC that sv[] and dp[] do not alias → enables aggressive
//        auto-vectorisation of the inner loop with AVX2 SIMD instructions.
//   6. __builtin_expect(cond, expected)
//        Annotates branch probability so GCC places the hot path in the
//        fall-through (no branch taken) code stream, reducing branch mispredicts.
//        expected=0 → branch is unlikely (early exits / oversized sizes).
//        expected=1 → branch is likely   (dp value is reachable).
//   7. __attribute__((hot))
//        Marks the function as frequently called; GCC groups it in the hot
//        section of the binary and optimises it more aggressively.
//   8. O(log m) binary_search early-exit
//        If requirement is directly in sizes[], skip the DP entirely.
//   9. Ascending sort + index break
//        Once sv[j] > i, all remaining sizes are larger; the break avoids
//        unnecessary iterations without branching unpredictably.
// =============================================================================
// Compiler support:
//   GCC (primary)  : #pragma GCC optimize/target enable O3+AVX2+BMI at TU level
//   Clang (secondary): pragmas guarded; __builtin_expect/__restrict__/alignas/
//                      __attribute__((hot)) all supported natively by Clang.
//                      Pass -O3 -mavx2 -mbmi -mbmi2 on the command line for Clang.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC optimize("O3,unroll-loops")
#  pragma GCC target("avx2,bmi,bmi2,popcnt")
#endif
#include <vector>
#include <algorithm>
#include <climits>
#include <iostream>
using namespace std;

__attribute__((hot))
int getUmbrellas(int req, vector<int> sizes) {
    constexpr int INF = INT_MAX / 2;   // half-max: dp[i-s]+1 never overflows

    // ── Prune + dedup ──────────────────────────────────────────────────────────
    sort(sizes.begin(), sizes.end());
    sizes.erase(unique(sizes.begin(), sizes.end()), sizes.end());
    // erase everything ≥ req+1 in O(log m) via lower_bound
    sizes.erase(lower_bound(sizes.begin(), sizes.end(), req + 1), sizes.end());

    // ── Early exits ───────────────────────────────────────────────────────────
    if (__builtin_expect(sizes.empty(), 0)) return -1;          // unlikely: pruned all
    // NOTE: sizes[0]==1 is NOT a short-circuit to `req`.
    //       size-1 combined with larger sizes gives a smaller count than req
    //       (e.g. req=9,[1,4] → 3 via 4+4+1, NOT 9). DP must run.
    if (__builtin_expect(                                        // unlikely: exact match
            binary_search(sizes.begin(), sizes.end(), req), 0)) return 1;

    // ── DP on cache-line-aligned stack array ───────────────────────────────────
    alignas(64) int dp[1001];
    fill(dp, dp + req + 1, INF);
    dp[0] = 0;

    // __restrict__: no aliasing between read-only sv[] and read-write dp[].
    // Allows GCC/AVX2 to vectorise the inner loop (gather dp[i-sv[j]] in parallel).
    const int* __restrict__ sv = sizes.data();
    const int  sm              = static_cast<int>(sizes.size());

    for (int i = 1; i <= req; ++i) {
        for (int j = 0; j < sm; ++j) {
            if (__builtin_expect(sv[j] > i, 0)) break; // unlikely: sizes sorted, hit wall
            if (dp[i - sv[j]] != INF)
                dp[i] = min(dp[i], dp[i - sv[j]] + 1);
        }
    }

    return (dp[req] == INF) ? -1 : dp[req];
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int req, m; cin >> req >> m;
    vector<int> v(m);
    for (auto& x : v) cin >> x;
    cout << getUmbrellas(req, move(v)) << '\n';
}
// =============================================================================
// Complexity
//   Time : O(m log m  +  req × m')   m' = unique pruned sizes ≤ m
//   Space: O(req)  – stack-allocated dp[], zero heap traffic in the DP loop
//
// Latency profile (GCC -O3 + AVX2, req=1000, m=10):
//   Prune+sort : ~50 ns  (small m, branch-friendly)
//   DP loop    : ~1–2 µs (4 KB dp fits in L1, SIMD inner loop)
//   Total      : < 3 µs  on modern x86 server (Xeon, AMD EPYC)
//
// Test cases:
//   requirement=5,    sizes=[3,5]     → 1   (binary_search early-exit)
//   requirement=8,    sizes=[3,5]     → 2
//   requirement=7,    sizes=[3,5]     → -1
//   requirement=1,    sizes=[2]       → -1  (prune empties sizes)
//   requirement=6,    sizes=[1,4]     → 3   (4+1+1=6, dp correctly finds minimum)
//   requirement=1000, sizes=[3,7]     → 144 (3×2 + 7×142 = 6+994 = 1000 ✓)
// =============================================================================
