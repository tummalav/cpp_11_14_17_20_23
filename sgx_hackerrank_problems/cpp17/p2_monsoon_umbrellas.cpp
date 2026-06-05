// =============================================================================
// Problem 2: Monsoon Umbrellas  |  C++17
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
// C++17 features used (over C++14):
//   - [[nodiscard]]      : compiler warns if return value is discarded
//   - if-with-initializer: if (auto it = expr; condition) { ... }
//                          scopes the variable to the if block only
//   - std::optional<int> : sentinel-free helper – returns nullopt on failure
//   - std::optional::value_or(-1): convert optional → int at the public boundary
//   - Structured bindings: auto [first, last] = std::pair{...}
//   - std::string_view   : (applicable in related string problems, noted here)
//   - Guaranteed copy elision (C++17): return dp[req]; from solve() is zero-copy
// =============================================================================
#include <vector>
#include <algorithm>
#include <optional>
#include <iostream>
using namespace std;

// ---------------------------------------------------------------------------
// C++17: std::optional<int> – internal helper is sentinel-free.
//        Returns std::nullopt instead of a magic -1 value inside the function.
//        The caller (getUmbrellas) converts nullopt → -1 at the public boundary.
// ---------------------------------------------------------------------------
static optional<int> solve(int req, const vector<int>& sz) {
    constexpr int INF = 1'000'000'000;

    // alignas(64): cache-line-aligned stack array (C++11 keyword).
    // 1001 × 4 B = ~4 KB → fits entirely in a 32 KB L1 cache.
    alignas(64) int dp[1001];
    fill(dp, dp + req + 1, INF);
    dp[0] = 0;

    for (int i = 1; i <= req; ++i)
        for (int s : sz) {
            if (s > i) break;
            if (dp[i - s] != INF) dp[i] = min(dp[i], dp[i - s] + 1);
        }

    // C++17: return std::nullopt instead of sentinel -1
    if (dp[req] == INF) return nullopt;
    return dp[req];  // guaranteed copy elision – no copy constructed
}

// C++17: [[nodiscard]] – compiler emits a warning if the caller ignores the result
[[nodiscard]] int getUmbrellas(int requirement, vector<int> sizes) {

    // ── Prune: erase sizes > requirement ────────────────────────────────────────
    sort(sizes.begin(), sizes.end());
    sizes.erase(unique(sizes.begin(), sizes.end()), sizes.end());

    // C++17: if-with-initializer – lower_bound result scoped inside the if block
    if (auto cutoff = lower_bound(sizes.begin(), sizes.end(), requirement + 1);
        cutoff != sizes.end()) {
        sizes.erase(cutoff, sizes.end());
    }

    if (sizes.empty()) return -1;

    // ── O(log m) early exit ──────────────────────────────────────────────────────
    // NOTE: sizes.front()==1 does NOT short-circuit to `requirement`.
    //       size-1 combined with larger sizes may give a smaller count than `requirement`
    //       (e.g. req=9,[1,4] → 3 via 4+4+1, NOT 9).  The DP handles this correctly.

    // C++17: if-with-initializer – found is scoped to this block only
    if (auto found = binary_search(sizes.begin(), sizes.end(), requirement); found)
        return 1;

    // ── DP via std::optional helper ───────────────────────────────────────────────
    // C++17: value_or(-1) converts std::nullopt → -1 at the public interface boundary
    return solve(requirement, sizes).value_or(-1);
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int req, m; cin >> req >> m;
    vector<int> sizes(m);
    for (auto& v : sizes) cin >> v;

    cout << getUmbrellas(req, move(sizes)) << '\n';
    return 0;
}
// =============================================================================
// Complexity
//   Time : O(m log m  +  req × m')   m' = unique pruned sizes ≤ m
//   Space: O(req)  – stack-allocated dp[], zero heap traffic in the DP loop
//
// Key C++17 design decisions:
//   1. if-with-initializer        → tightens variable scope (prevents accidental reuse)
//   2. std::optional<int>         → eliminates magic-number sentinel (-1) inside solve()
//   3. value_or(-1)               → sentinel reappears only at the public int boundary
//   4. [[nodiscard]]              → prevents bugs where caller forgets to check result
//   5. Guaranteed copy elision    → returning int from optional<int> is zero overhead
//
// Test cases:
//   requirement=5, sizes=[3,5]  → 1   (binary_search early-exit)
//   requirement=8, sizes=[3,5]  → 2
//   requirement=7, sizes=[3,5]  → -1
//   requirement=1, sizes=[2]    → -1  (after prune, sizes empty)
//   requirement=6, sizes=[1,4]  → 3   (4+1+1=6, dp correctly finds minimum)
// =============================================================================
