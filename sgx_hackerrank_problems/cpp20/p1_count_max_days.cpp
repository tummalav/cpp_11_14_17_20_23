// =============================================================================
// Problem 1: Count Maximum Days  |  C++20
// =============================================================================
// C++20 additions:
//   - Concepts + requires clauses  (guarded: GCC>=10 / Clang>=10)
//   - [[likely]] / [[unlikely]] standard attributes (guarded: GCC>=9 / Clang>=12)
//   - std::span  (not used here – input is vector)
//   - Three-way operator <=> (not needed here)
//
// Compiler support:
//   GCC 10+ (primary)    : all C++20 features available with -std=c++20
//   Clang 10+ (secondary): <concepts> available; [[likely]] requires Clang>=12
//   Apple Clang 12       : <concepts> unavailable; falls back gracefully via
//                          __has_include / __has_cpp_attribute guards.
// =============================================================================
#include <vector>
#include <queue>
#include <utility>
#include <iostream>
#if __has_include(<concepts>)
#  include <concepts>
#  define HAS_CONCEPTS 1
#else
#  define HAS_CONCEPTS 0
#endif
using namespace std;

#if HAS_CONCEPTS
// C++20: concept constraining the input to an integer range
template<typename R>
concept IntRange = ranges::range<R> &&
                   same_as<ranges::range_value_t<R>, int>;

// C++20: constrained template — compiler error if wrong type passed
template<IntRange Lend, IntRange Payback>
[[nodiscard]] int countMaximumDays(Lend lend_r, Payback payback_r) {
    vector<int> lend(lend_r.begin(), lend_r.end());
    vector<int> payback(payback_r.begin(), payback_r.end());
#else
// Fallback for compilers without <concepts> (e.g. Apple Clang 12)
[[nodiscard]] int countMaximumDays(vector<int> lend, vector<int> payback) {
#endif

    using P = pair<int,int>;
    priority_queue<P, vector<P>, greater<P>> pq;

    const int n = static_cast<int>(lend.size());
    for (int i = 0; i < n; ++i) pq.emplace(payback[i], lend[i]);

    int       days  = 0;
    long long debt  = 0;

    while (!pq.empty()) {
        auto [pb, ln] = pq.top(); pq.pop();       // C++17 structured binding
#if __has_cpp_attribute(unlikely)
        if (ln < debt) [[unlikely]] continue;      // C++20: [[unlikely]] hint
#else
        if (ln < debt) continue;
#endif
        debt = pb;
        ++days;
    }
    return days;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int n; cin >> n;
    vector<int> lend(n), payback(n);
    for (int& v : lend)    cin >> v;
    for (int& v : payback) cin >> v;

    cout << countMaximumDays(lend, payback) << '\n';
    return 0;
}

