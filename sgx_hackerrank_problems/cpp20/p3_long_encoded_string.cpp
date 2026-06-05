// =============================================================================
// Problem 3: Long Encoded String  |  C++20
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
// C++20 features used (over C++17):
//   - [[nodiscard("reason")]]  : [[nodiscard]] with diagnostic message (C++20)
//   - std::ssize()             : signed size, avoids signed/unsigned warnings
//   - std::string_view         : inherited from C++17, promoted as idiomatic C++20
//   - Pointer-based scan       : const char* p/end avoids repeated operator[]
// Note: [[likely]]/[[unlikely]] on if-branches are C++20 standard attributes
//       but require GCC≥10 or Clang≥14; they are omitted here for portability.
// =============================================================================
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <iterator>
#include <iostream>
using namespace std;

// C++20: [[nodiscard]] with message – caller gets an explanatory warning
[[nodiscard("character frequencies must not be discarded")]]
vector<int> frequency(string_view s) {
    array<int, 26> freq{};

    const char* p   = s.data();
    const char* end = p + s.size();   // one-past-end sentinel

    while (p < end) {
        int idx;

        // Two-digit code: digit + digit + '#'  (j=10#…z=26#)
        if (p + 2 < end && (unsigned char)(p[1] - '0') <= 9u && p[2] == '#') {
            idx = 10 * (p[0] - '0') + (p[1] - '0') - 1;  // 10→9…26→25
            p += 3;
        } else {
            idx = (p[0] - '0') - 1;   // 1→0…9→8
            p += 1;
        }

        // Repeat-count suffix: optional '(N)' where 2 <= N <= 10000
        int count = 1;
        if (p < end && *p == '(') {
            ++p;            // skip '('
            int num = 0;
            while (p < end && *p != ')') num = num * 10 + (*p++ - '0');
            ++p;            // skip ')'
            count = num;
        }

        freq[idx] += count;
    }

    return vector<int>(freq.begin(), freq.end());
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    string s; cin >> s;
    auto result = frequency(s);
    // C++20: std::ssize returns ptrdiff_t – avoids signed/unsigned comparison
    for (auto i = 0; i < static_cast<int>(std::ssize(result)); ++i)
        cout << result[i] << (i < 25 ? ' ' : '\n');
}
// =============================================================================
// Complexity
//   Time : O(n)  – single forward pass
//   Space: O(1)  – 26-int stack array; return vector is O(26)=O(1)
//
// Corner cases verified:
//   Single char    "1"               → [1,0,...,0]
//   All z's        "26#(5)"          → [0,...,5]
//   Mixed runs     "23#(2)24#25#26#23#(3)" → [0,...,5,1,1,1]
//   Large count    "1(10000)"        → [10000,0,...,0]
//   Two-digit adj  "10#11#12#"       → j=1,k=1,l=1
// =============================================================================
