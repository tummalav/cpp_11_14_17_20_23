// Problem 4: Good Binary Strings | Ultra-Low-Latency
/*  Two definitions follows:
    A binary string consists of 0's and/or 1's. for example, 010111, 1111, and 00 are binary strings.
    The prefix of a string is any of its substrings that include the beginning of the string, for example,
    the prefixes of 11010 are 1, 11, 110, 1101, and 11010

A non-empty binary string is good if the following two conditions are true:

    1. The number of 0's is equal to the number of 1's.
    2. For every prefix of the string, the number of 1's is not less than the number of 0's

For example, 11010 is not good because because it does not have an equal number of 0's and 1's but
110010 is good because it satisfies both conditions.

A good string can contain multiple good substrings. if two consecutive substrings are good, then they can be swapped as
long as the resulting string is still a good string. Given a good binary string, binString, performzero or more swap
operations on its adjacent good substrings such that the resulting string is the largestpossible numeric value.
Two substrings are adjacent if the last character of the first substring occurs exactly one index before the first
 character of the second substring.

Example

binString = 1010111000

There are two good binary substrings, 1010 and 111000, among others. Swap these two substrings to get a larger value: 1110001010
. This is the largets possible good string that can be formed.

Function Description
Complete the function largestMagical in the editor below.

Function Parameters
    str binString a binary string.

Returns
    str the largest possible binary value as a string.

Constraints :

    each character of binString {01}.
    1 <= \binString\ <= 50
    binString is good string.

Input format for custom testing

the only line input contains the string binString

Sample Case 0
STDIN - 11011000, Function parameters - binString = "11011000"

Sample Out 0
11100100

Sample case 1
Sample input 1
STDIN - 1100, Function Parameters - binString = "1100"
Sample Out 1
1100

Sample Case 2;
Sample Input for custom testing

STDIN 1101001100 , Function Parameters binString ="1101001100
Sample Output 2
1101001100

*/
// ULL: index-based recursion (no substr), fixed-size stack arrays, reserve
// Compiler support:
//   GCC (primary)  : #pragma GCC optimize/target enable O3+AVX2+BMI at TU level
//   Clang (secondary): pragmas guarded; __attribute__((noinline)) supported natively.
//                      Pass -O3 -mavx2 on the command line for Clang.
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC optimize("O3,unroll-loops")
#  pragma GCC target("avx2,bmi,bmi2,popcnt")
#endif
#include <string>
#include <array>
#include <algorithm>
#include <numeric>
#include <iostream>
using namespace std;
__attribute__((noinline))
static void solve(const string& s, int lo, int hi, string& result) {
    if (lo>=hi) return;
    array<pair<int8_t,int8_t>,26> segs;
    int nseg=0, bal=0;
    int8_t sl=(int8_t)lo;
    for (int i=lo;i<hi;++i) {
        bal+=(s[i]=='1')?1:-1;
        if (bal==0){segs[nseg++]={(int8_t)sl,(int8_t)(i+1)};sl=(int8_t)(i+1);}
    }
    array<string,26> tr;
    for (int k=0;k<nseg;++k) {
        int a=segs[k].first, b=segs[k].second;
        if (b-a==2){tr[k]="10";}
        else {tr[k].reserve(b-a);tr[k]+='1';solve(s,a+1,b-1,tr[k]);tr[k]+='0';}
    }
    array<int,26> idx; iota(idx.begin(),idx.begin()+nseg,0);
    sort(idx.begin(),idx.begin()+nseg,[&](int a,int b){return tr[a]>tr[b];});
    for (int k=0;k<nseg;++k) result+=tr[idx[k]];
}
string largestMagical(const string& s) {
    string r; r.reserve(s.size());
    solve(s,0,(int)s.size(),r); return r;
}
int main() {
    ios_base::sync_with_stdio(false); cin.tie(nullptr);
    string s; cin>>s; cout<<largestMagical(s)<<'\n';
}
