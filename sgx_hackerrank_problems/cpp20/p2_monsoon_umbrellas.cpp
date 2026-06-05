// =============================================================================
// Problem 2: Monsoon Umbrellas  |  C++20
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
// C++20 features used (Apple Clang 12+ / GCC 10+):
//   - std::span<const int>        : non-owning view of input – zero-copy
//   - std::erase_if               : one-call in-place filter for containers (C++20)
//   - Template lambda             : []<typename T>(...) – explicit type parameter
//   - [[nodiscard("reason")]]     : nodiscard with descriptive message (C++20)
//   - std::ssize                  : signed size; avoids signed/unsigned warnings
//   - alignas(64)                 : cache-line-aligned stack array
//
// NOTE: <ranges> (ranges::sort, views::filter) and <concepts> (concept IntRange)
//       require GCC 10+, Clang 14+, or MSVC 2019 16.10+.
//       Those are shown as inline comments below for reference.
// =============================================================================
#include <vector>
#include <span>
#include <algorithm>
#include <iostream>
using namespace std;

// C++20: [[nodiscard]] with a descriptive reason string
[[nodiscard("return value encodes failure as -1; always check it")]]
int getUmbrellas(int req, span<const int> sizes_in) {
    constexpr int INF = 1'000'000'000;

    // C++20: std::erase_if – single-call in-place removal (cleaner than remove+erase)
    // Prune sizes > req: they can never contribute to an exact sum
    vector<int> sz(sizes_in.begin(), sizes_in.end());
    erase_if(sz, [req](int s){ return s > req; });   // C++20: erase_if

    sort(sz.begin(), sz.end());
    sz.erase(unique(sz.begin(), sz.end()), sz.end());

    // Equivalent with ranges (GCC 10+/Clang 14+):
    //   ranges::sort(sz);
    //   auto [lo, hi] = ranges::unique(sz); sz.erase(lo, hi);

    if (sz.empty()) return -1;

    // O(log m) early exit
    // NOTE: sz.front()==1 does NOT short-circuit to `req`.
    //       size-1 + larger sizes can yield fewer umbrellas than `req`
    //       (e.g. req=9,[1,4] → 3 via 4+4+1, NOT 9).
    if (binary_search(sz.begin(), sz.end(), req)) return 1;

    // ── DP on cache-line-aligned stack array ───────────────────────────────────
    alignas(64) int dp[1001];
    fill(dp, dp + req + 1, INF);
    dp[0] = 0;

    // C++20: std::ssize returns ptrdiff_t; avoids signed/unsigned comparison warning
    for (int i = 1; i <= req; ++i)
        for (int j = 0; j < std::ssize(sz); ++j) {
            if (sz[j] > i) break;
            if (dp[i - sz[j]] != INF)
                dp[i] = min(dp[i], dp[i - sz[j]] + 1);
        }

    return (dp[req] == INF) ? -1 : dp[req];
}

// ---------------------------------------------------------------------------
// C++20: template lambda – explicit template type parameter []<typename T>(...)
// Reads a contiguous sequence of T from stdin into a vector<T>.
// Equivalent concept-constrained version (GCC 10+/Clang 14+):
//   template <std::integral T> vector<T> readVec(int n) { ... }
// ---------------------------------------------------------------------------
auto readInput = []<typename T>(int n) -> vector<T> {
    vector<T> v(n);
    for (auto& x : v) cin >> x;
    return v;
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int req, m; cin >> req >> m;
    vector<int> v = readInput.operator()<int>(m);  // C++20 template lambda

    // std::span wraps the vector – no copy, just a view
    cout << getUmbrellas(req, span<const int>(v)) << '\n';
}
// =============================================================================
// Complexity
//   Time : O(m log m  +  req × m')   m' = unique pruned sizes ≤ m
//   Space: O(req)  – stack-allocated dp[], zero heap traffic in the DP loop
//
// Key C++20 design decisions:
//   1. std::span<const int>    → caller retains ownership; function only observes
//   2. std::erase_if           → replaces erase(remove_if(...), end) idiom cleanly
//   3. template lambda         → reusable typed reader without a named template fn
//   4. [[nodiscard("reason")]] → richer warning message for careless callers
//   5. std::ssize              → signed arithmetic throughout; no -Wsign-compare
//
// Test cases:
//   requirement=5, sizes=[3,5]  → 1   (binary_search early-exit)
//   requirement=8, sizes=[3,5]  → 2
//   requirement=7, sizes=[3,5]  → -1
//   requirement=1, sizes=[2]    → -1  (after erase_if, sz empty)
//   requirement=6, sizes=[1,4]  → 3   (4+1+1=6, dp correctly finds minimum)
// =============================================================================
