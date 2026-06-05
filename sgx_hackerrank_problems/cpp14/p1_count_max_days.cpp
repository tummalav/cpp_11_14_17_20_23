// =============================================================================
// Problem 1: Count Maximum Days  |  C++14
// =============================================================================
// C++14 additions over C++11:
//   - Return type deduction  (auto fn() { ... })
//   - Generic lambdas        (auto parameters)
//   - std::make_unique (not used here but available)
//   - Digit separators       1'000'000
//   - Variable templates     (not needed here)
//   - Relaxed constexpr
// =============================================================================
#include <vector>
#include <queue>
#include <utility>
#include <iostream>
using namespace std;

// C++14: return type deduction – no explicit '-> int' needed
auto countMaximumDays(vector<int> lend, vector<int> payback) {
    using P = pair<int,int>;

    // C++14: generic lambda comparator (auto parameters)
    auto cmp = [](auto& a, auto& b){ return a.first > b.first; };
    priority_queue<P, vector<P>, decltype(cmp)> pq(cmp);

    const int n = static_cast<int>(lend.size());
    for (int i = 0; i < n; ++i)
        pq.emplace(payback[i], lend[i]);

    int       days         = 0;
    long long current_debt = 0;

    while (!pq.empty()) {
        auto top = pq.top();  pq.pop();
        if (top.second < current_debt) continue;   // lazy deletion
        current_debt = top.first;
        ++days;
    }
    return days;     // C++14: return type deduced as int
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int n; cin >> n;
    vector<int> lend(n), payback(n);
    for (int& v : lend)    cin >> v;
    for (int& v : payback) cin >> v;

    // C++14: digit separator in constant (demonstrating the feature)
    constexpr int MAX_N = 100'000;   // 1e5 with C++14 digit separator
    (void)MAX_N;

    cout << countMaximumDays(move(lend), move(payback)) << '\n';
    return 0;
}

