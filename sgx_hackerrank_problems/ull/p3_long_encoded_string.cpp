// =============================================================================
// Problem 3: Long Encoded String  |  Ultra-Low-Latency (ULL)
// Consider a string that consists of lowercase English, letters, i.e, [a-z] only. The following  rules are used to
// encode all of its characters into the string s.
// a is encoded as 1, b is encoded as 2, c is encoded as 3, ,,,, and i is encoded as 9.
// j is encoded as 10#, k is encoded as 11#, l is encoded as 12#, ..., and z is encoded as 26#
// if any character occurs two or more consequently , its count immediately follows the character
// in parantheses, e.g 'aa' is encoded as '1(2)'
// Examples

// String "abzx" is encoded as s = "1226#24#".
// String "aabccc" os encoded as s = "1(2)23(3)"
// String "bajj" is encoded as s = "2110#(2)
//String "wwxyzwww" is encodedas s = "23#(2)24#25#26#23#(3)"

// Given an encoded string s, determine the character count for each letter of the original, decoded
// string. Return array of 26 integers where index 0 contains the number of 'a' characters, index 1 conatins
// number of 'b' characters and so on.

// Function description

// Complete the frequencyfunction
// frequency has the following paramater :
// string s: an encoded string

// Return :
// int[26]: the character frequencies as described.

// Constraints

// String s consists of decimal integers from 0 to 9, #s, () only
// 1 <= lenght of s <= 100000
// it is guaranteed that string s is a valid encoded string
// 2<= c << 10000 , where c is a parenthetical count of consecutive occurences of an encoded charcter.

// Input format for custom testing
// Input from stdin will be processes as follows and passed to the function

// the only line contains the string s, the encoded string
// sample case 0
// Sample input for custom string
// 1226#24#
// Sample output 0
// 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1

// Sample case 1
// Sample input 1
// 1(2)23(3)
// Sample outpu1
// 2 1 3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

// Sample case 2
// Sample input 2
// 2110#(2)
// 1 1 0 0 0 0 0 0 0 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

// Sample case 3
// Sample input 3
// 23#(2)24#25#26#23#(3)
// Sampple out 3
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 5 1 1 1
// =============================================================================
// ULL techniques applied:
//   1. #pragma GCC optimize / target  : O3 + AVX2/BMI/BMI2/POPCNT — GCC only
//                                       (guarded with #ifndef __clang__)
//                                       Clang: pass -O3 -mavx2 -mbmi -mbmi2
//   2. Raw const char* pointers       : *p++ is pure load+advance; no index
//                                       arithmetic or bounds-check machinery
//   3. alignas(64) int freq[26]       : 104 B → exactly 2 cache lines, never
//                                       split across 3 (64+40 B boundary)
//   4. __builtin_expect(expr,likely)  : feed branch-predictor hints to the
//                                       backend; outer loop → likely(1),
//                                       '(' repeat → unlikely(0)
//   5. [[gnu::always_inline]]         : forces inlining at every call site;
//                                       eliminates call/ret overhead and exposes
//                                       surrounding context for optimisation
//   6. __restrict__                   : tells the compiler p and end do not
//                                       alias any other pointer in scope
//   7. Unsigned digit check           : (unsigned char)(p[1]-'0') <= 9u is a
//                                       single SUB+CMP, avoids isdigit() call
// =============================================================================
// Compiler support:
//   GCC (primary)  : pragmas enable O3+AVX2+BMI at translation-unit level
//   Clang (second) : pragmas guarded; [[gnu::always_inline]], __builtin_expect,
//                    __restrict__, alignas all supported natively by Clang.
//                    Pass -O3 -mavx2 -mbmi -mbmi2 on the command line.
// =============================================================================
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC optimize("O3,unroll-loops")
#  pragma GCC target("avx2,bmi,bmi2,popcnt")
#endif
#include <array>
#include <string>
#include <vector>
#include <iostream>
using namespace std;

[[gnu::always_inline]]
static inline vector<int> freq_impl(const char* __restrict__ p,
                                     const char* __restrict__ end) {
    // alignas(64): 26×4 B = 104 B fits in 2 cache lines, cache-line aligned.
    alignas(64) int freq[26] = {};

    while (__builtin_expect(p < end, 1)) {   // outer loop: very likely to continue
        int idx;

        // ── Token decode ─────────────────────────────────────────────────────
        // Two-digit code: XY# where X∈[1-2], Y∈[0-9], value 10-26 → j-z
        // (unsigned char)(p[1] - '0') <= 9u: branch-free digit test, faster than isdigit()
        if (__builtin_expect(
                p + 2 < end &&
                (unsigned char)(p[1] - '0') <= 9u &&
                p[2] == '#',
                1)) {                              // two-digit codes (j-z): likely
            idx = 10 * (p[0] - '0') + (p[1] - '0') - 1;   // 10→9 … 26→25
            p += 3;
        } else {
            idx = (p[0] - '0') - 1;               // single digit (a-i): 1→0 … 9→8
            p += 1;
        }

        // ── Repeat count ─────────────────────────────────────────────────────
        // '(' only when 2+ consecutive identical chars; mark unlikely.
        int cnt = 1;
        if (__builtin_expect(p < end && *p == '(', 0)) {
            ++p;                                   // skip '('
            int num = 0;
            // Safety guard (p < end) even though valid input always has ')'
            while (__builtin_expect(p < end && *p != ')', 1)) {
                num = num * 10 + (*p++ - '0');
            }
            ++p;                                   // skip ')'
            cnt = num;
        }

        freq[idx] += cnt;
    }

    return vector<int>(freq, freq + 26);
}

vector<int> frequency(const string& s) {
    return freq_impl(s.data(), s.data() + s.size());
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    string s; cin >> s;
    auto result = frequency(s);
    for (int i = 0; i < 26; ++i)
        cout << result[i] << (i < 25 ? ' ' : '\n');
}
// =============================================================================
// Complexity
//   Time : O(n) – single forward pass, no backtracking
//   Space: O(1) – 26-int stack array; output vector is O(26)=O(1)
//
// Why raw pointer over s[i]:
//   *p++   → one deref + post-increment, no bounds-check, no aliasing concern
//   s[i]   → base_addr + i*sizeof(char) + possible bounds-check overhead
//
// Cache layout of alignas(64) freq[26]:
//   offset 0–63  : freq[0..15]   ← cache line 0
//   offset 64–103: freq[16..25]  ← cache line 1
//   Without alignas the array might start at offset 52 → 3 cache lines touched.
// =============================================================================
