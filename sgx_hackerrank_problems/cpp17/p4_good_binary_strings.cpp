// =============================================================================
// Problem 4: Good Binary Strings – Largest Magical String
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
// =============================================================================
// Good string: equal 0s and 1s, every prefix has #1s >= #0s.
// Task: rearrange adjacent good substrings to maximise numeric value.
//
// Constraints:  1 <= |binString| <= 50,  binString is a good string.
//
// Strategy: recursive primitive-decompose + descending sort, O(n² log n).
//
// ── Ultra-Low-Latency techniques applied ─────────────────────────────────────
//   Note: n <= 50 means absolute time is negligible (< 1 µs) regardless of
//   technique.  Improvements here are structural / style, not hotpath-critical.
//
//   1. Index-based recursion:  pass (lo, hi) indices into the original string
//      instead of copying substrings with substr().  Each recursive call works
//      on a string_view-like window; substrings are copied only when building
//      the final result → reduces O(n) string copies per level to O(1) copy
//      at the leaf, and one O(n) concatenation at each level.
//   2. result.reserve(n):  pre-allocate the exact output size → zero realloc.
//   3. Small-vector optimisation for parts:  a good string of length n has at
//      most n/2 primitive parts.  For n=50, max 25 parts.  Using a fixed-size
//      array avoids heap allocation for the parts vector entirely.
//   4. __attribute__((noinline)) on the recursive helper:  prevents accidental
//      code-size explosion from deep inlining of the recursive function.
//   5. Extended pragma target.
// =============================================================================

// Compiler support:
//   GCC (primary)  : pragmas enable O3+AVX2+BMI at TU level
//   Clang (second) : pragmas guarded; __attribute__((noinline)) supported natively.
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

// ---------------------------------------------------------------------------
// Recursive helper – works on s[lo..hi) using indices (no substr copies)
// ---------------------------------------------------------------------------
__attribute__((noinline))
static void solve(const string& s, int lo, int hi, string& result) {
    if (lo >= hi) return;

    // ── Step 1: collect primitive segments as (start, end) index pairs ──────
    // Max primitives for n=50: 25  → fixed-size stack array, zero heap alloc.
    array<pair<int8_t, int8_t>, 26> segs;   // (seg_lo, seg_hi) into s
    int  nseg    = 0;
    int  balance = 0;
    int8_t seg_lo = static_cast<int8_t>(lo);

    for (int i = lo; i < hi; ++i) {
        balance += (s[i] == '1') ? 1 : -1;
        if (balance == 0) {
            segs[nseg++] = { seg_lo, static_cast<int8_t>(i + 1) };
            seg_lo = static_cast<int8_t>(i + 1);
        }
    }

    // ── Step 2: build transformed string for each primitive ──────────────
    // Each primitive has the form  '1' [inner] '0'.
    // We build the transformed version into a small fixed-size buffer.
    array<string, 26> transformed;
    for (int k = 0; k < nseg; ++k) {
        const int a = segs[k].first, b = segs[k].second;
        if (b - a == 2) {
            transformed[k] = "10";           // minimal primitive
        } else {
            transformed[k].reserve(b - a);
            transformed[k] += '1';
            solve(s, a + 1, b - 1, transformed[k]);  // recurse on inner
            transformed[k] += '0';
        }
    }

    // ── Step 3: sort transformed parts descending, append to result ──────
    // Build index array for indirect sort (avoids moving strings around).
    array<int, 26> idx;
    iota(idx.begin(), idx.begin() + nseg, 0);
    sort(idx.begin(), idx.begin() + nseg, [&](int a, int b) {
        return transformed[a] > transformed[b];
    });

    for (int k = 0; k < nseg; ++k)
        result += transformed[idx[k]];
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
string largestMagical(const string& binString) {
    string result;
    result.reserve(binString.size());   // exact size known upfront → zero realloc
    solve(binString, 0, static_cast<int>(binString.size()), result);
    return result;
}

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------
int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    string binString;  cin >> binString;
    cout << largestMagical(binString) << '\n';
    return 0;
}

// ---------------------------------------------------------------------------
// Complexity
//   Time : O(n² log n)  – same as before, negligible for n <= 50
//   Space: O(n)          – stack frames only; no heap allocs except final string
//
// Sample cases:
//   "11011000"   → "11100100"
//   "1100"       → "1100"
//   "1101001100" → "1101001100"
//   "1010111000" → "1110001010"
// ---------------------------------------------------------------------------
