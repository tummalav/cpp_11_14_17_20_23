// =============================================================================
// Problem 2: Monsoon Umbrellas  |  C++11
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
// C++11 features used:
//   - std::numeric_limits<int>::max()
//   - Lambda expression (value-capture) for remove_if predicate
//   - Range-based for loop
//   - nullptr
//   - std::move (move semantics)
//   - alignas(64)  ← introduced in C++11; aligns dp[] to a cache-line boundary
//   - binary_search early-exit  O(log m)
// =============================================================================
#include <vector>
#include <algorithm>
#include <limits>
#include <iostream>
using namespace std;

int getUmbrellas(int requirement, vector<int> sizes) {
    const int INF = numeric_limits<int>::max() / 2;  // half-max avoids overflow on +1

    // ── Prune: remove sizes > requirement ──────────────────────────────────────
    // Lambda capture (C++11): sizes > requirement can never contribute to exact sum
    sizes.erase(remove_if(sizes.begin(), sizes.end(),
                          [requirement](int s){ return s > requirement; }),
                sizes.end());
    sort(sizes.begin(), sizes.end());                          // ascending for break-early
    sizes.erase(unique(sizes.begin(), sizes.end()), sizes.end()); // dedup

    if (sizes.empty()) return -1;

    // ── O(log m) early exit ─────────────────────────────────────────────────────
    // If requirement itself is an available size, one umbrella is enough (minimum possible).
    // NOTE: sizes[0]==1 does NOT short-circuit to `requirement`; size-1 + larger sizes
    //       may produce a smaller count (e.g. req=9,[1,4] → 3 via 4+4+1, not 9).
    if (binary_search(sizes.begin(), sizes.end(), requirement)) return 1; // exact match

    // ── DP on cache-line-aligned stack array ───────────────────────────────────
    // alignas(64) – C++11 keyword: start the array on a 64-byte cache-line boundary.
    // 1001 × 4 B = ~4 KB → fits entirely in a typical 32 KB L1 data cache.
    // Using a fixed-size stack array (no heap alloc) because constraint says req <= 1000.
    alignas(64) int dp[1001];
    fill(dp, dp + requirement + 1, INF);
    dp[0] = 0;

    for (int i = 1; i <= requirement; ++i) {
        for (int s : sizes) {        // C++11: range-based for
            if (s > i) break;        // sorted ascending → safe early break
            if (dp[i - s] != INF)
                dp[i] = min(dp[i], dp[i - s] + 1);
        }
    }
    return (dp[requirement] == INF) ? -1 : dp[requirement];
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);                 // C++11: nullptr

    int req; cin >> req;
    int m;   cin >> m;
    vector<int> sizes(m);
    for (int i = 0; i < m; ++i) cin >> sizes[i];

    cout << getUmbrellas(req, move(sizes)) << '\n';  // C++11: std::move
    return 0;
}
// =============================================================================
// Complexity
//   Time : O(m log m  +  req × m')   m' = unique pruned sizes ≤ m
//   Space: O(req)  – stack-allocated dp[], zero heap traffic in the DP loop
//
// Test cases:
//   requirement=5, sizes=[3,5]  → 1   (binary_search early-exit)
//   requirement=8, sizes=[3,5]  → 2
//   requirement=7, sizes=[3,5]  → -1
//   requirement=1, sizes=[2]    → -1  (after prune, sizes empty)
//   requirement=6, sizes=[1,4]  → 3   (4+1+1=6, dp correctly finds minimum)
// =============================================================================
