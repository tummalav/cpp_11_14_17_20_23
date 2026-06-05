// =============================================================================
// Problem 1: Count Maximum Days  |  C++11

// =============================================================================
// C++11 features demonstrated:
//   - auto type deduction (local variables)
//   - Range-based for loop
//   - std::move
//   - emplace() on priority_queue
//   - nullptr, noexcept
//   - typedef for pair alias
//   - Lambda (not needed here; comparator via std::greater)
// =============================================================================
#include <vector>
#include <queue>
#include <utility>
#include <iostream>
using namespace std;

int countMaximumDays(vector<int> lend, vector<int> payback) {
    typedef pair<int,int> P;   // C++11: typedef (not 'using' yet common)

    // Min-heap ordered by payback (first element of pair).
    // std::greater<P> does lexicographic: compare first, then second.
    priority_queue<P, vector<P>, greater<P> > pq;   // note: "> >" spacing for C++03 compat

    const int n = static_cast<int>(lend.size());
    for (int i = 0; i < n; ++i)
        pq.emplace(payback[i], lend[i]);    // C++11: emplace

    int       days         = 0;
    long long current_debt = 0;

    while (!pq.empty()) {
        auto top = pq.top();                         // C++11: auto
        pq.pop();
        int pb = top.first;
        int ln = top.second;

        // Lazy deletion: debt never decreases, so lend < debt is permanent.
        if (ln < current_debt) continue;

        current_debt = pb;
        ++days;
    }
    return days;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);                               // C++11: nullptr

    int n; cin >> n;
    vector<int> lend(n), payback(n);
    for (int i = 0; i < n; ++i) cin >> lend[i];
    for (int i = 0; i < n; ++i) cin >> payback[i];

    cout << countMaximumDays(move(lend), move(payback)) << '\n'; // C++11: move
    return 0;
}

