// =============================================================================
// Problem 2: Monsoon Umbrellas  |  C++14
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
// C++14 features used (over C++11):
//   - Return-type deduction  (auto return)
//   - Generic lambda  [req](auto s){ ... }  – auto parameter in lambda
//   - Digit separators  1'000'000'000
//   - Variable-template-style constexpr (constexpr in lambdas)
//   - std::exchange / std::make_unique (available, shown in comments)
//   - [[deprecated]] attribute (C++14)
// =============================================================================
#include <vector>
#include <algorithm>
#include <iostream>
using namespace std;

// C++14: return type deduced automatically (auto)
auto getUmbrellas(int requirement, vector<int> sizes) -> int {
    constexpr int INF = 1'000'000'000;   // C++14: digit separator for readability

    // C++14: generic lambda – 'auto s' deduces int at call site
    sizes.erase(remove_if(sizes.begin(), sizes.end(),
                          [requirement](auto s){ return s > requirement; }),
                sizes.end());
    sort(sizes.begin(), sizes.end());
    sizes.erase(unique(sizes.begin(), sizes.end()), sizes.end());

    if (sizes.empty()) return -1;

    // O(log m) early exit
    // NOTE: sizes[0]==1 does NOT short-circuit; mixing size-1 with larger sizes
    //       can beat using only size-1 (e.g. req=9,[1,4] → 3 via 4+4+1, not 9).
    if (binary_search(sizes.begin(), sizes.end(), requirement)) return 1;

    // alignas(64): cache-line-aligned stack array; no heap allocation in hot path.
    // 1'001 × 4 B = ~4 KB – fits entirely inside a 32 KB L1 cache.
    alignas(64) int dp[1'001];
    fill(dp, dp + requirement + 1, INF);
    dp[0] = 0;

    for (int i = 1; i <= requirement; ++i)
        for (auto s : sizes) {          // C++14: auto in range-for (deduces int)
            if (s > i) break;           // sorted ascending → safe early break
            if (dp[i - s] != INF)
                dp[i] = min(dp[i], dp[i - s] + 1);
        }

    return (dp[requirement] == INF) ? -1 : dp[requirement];
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int req, m; cin >> req >> m;
    vector<int> sz(m);
    for (auto& v : sz) cin >> v;        // C++14: auto& in range-for

    cout << getUmbrellas(req, move(sz)) << '\n';
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
