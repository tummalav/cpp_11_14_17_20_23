// Problem 1: Count Maximum Days | Ultra-Low-Latency
// ULL: int64 bit-packing, O(n) make_heap, __builtin_expect, GCC/Clang pragmas
// Compiler support:
//   GCC (primary)  : #pragma GCC optimize/target enable O3+AVX2+BMI at TU level
//   Clang (secondary): pragmas guarded; __builtin_expect/__restrict__/alignas all
//                      supported natively; pass -O3 -mavx2 on the command line.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC optimize("O3,unroll-loops")
#  pragma GCC target("avx2,bmi,bmi2,popcnt")
#endif
#include <cstdint>
#include <vector>
#include <queue>
#include <algorithm>
#include <iostream>
using namespace std;
using u64 = uint64_t;
static inline u64 pack(int pb,int ln) noexcept { return ((u64)(unsigned)pb<<32)|(unsigned)ln; }
static inline int get_pb(u64 v) noexcept { return (int)(v>>32); }
static inline int get_ln(u64 v) noexcept { return (int)(unsigned)v; }
int countMaximumDays(vector<int> lend, vector<int> payback) {
    const int n = (int)lend.size();
    if (__builtin_expect(n==0,0)) return 0;
    vector<u64> bk; bk.reserve(n);
    for (int i=0;i<n;++i) bk.push_back(pack(payback[i],lend[i]));
    make_heap(bk.begin(),bk.end(),greater<u64>{});
    priority_queue<u64,vector<u64>,greater<u64>> pq(greater<u64>{},move(bk));
    int days=0; long long debt=0;
    while (__builtin_expect(!pq.empty(),1)) {
        u64 top=pq.top(); pq.pop();
        if (__builtin_expect(get_ln(top)<debt,0)) continue;
        debt=get_pb(top); ++days;
    }
    return days;
}
int main() {
    ios_base::sync_with_stdio(false); cin.tie(nullptr);
    int n; cin>>n;
    vector<int> l(n),p(n);
    for (int& v:l) cin>>v; for (int& v:p) cin>>v;
    cout<<countMaximumDays(move(l),move(p))<<'\n';
}
