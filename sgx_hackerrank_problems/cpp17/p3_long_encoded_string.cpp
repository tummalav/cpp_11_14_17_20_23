// =============================================================================
// Problem 3: Long Encoded String  |  C++17
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
// 23#(2)24#25#26#23#3)
// Sampple out 3
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 5 1 1 1
// =============================================================================
// C++17 features used (over C++14):
//   - std::string_view parameter  : non-owning view, no string copy at call site
//   - [[nodiscard]]               : compiler warns if caller ignores the return value
//   - if-with-initializer         : if (int count = 1; s[i] == '(') { ... }
//                                   scopes 'count' to the if/else block only
//   - Aggregate {} zero-init      : array<int,26> freq{} (cleaner than .fill(0))
//   - std::string_view::data()    : raw pointer access without owning a string
// =============================================================================
#include <array>
#include <string>
#include <string_view>   // C++17
#include <vector>
#include <iostream>
using namespace std;

// C++17: [[nodiscard]] – compiler warns if the return value is ignored
[[nodiscard]] vector<int> frequency(string_view s) {   // C++17: string_view
    array<int, 26> freq{};   // aggregate zero-init (cleaner than fill(0))

    const int n = static_cast<int>(s.size());
    int i = 0;

    while (i < n) {
        int idx;

        // Detect two-digit code: current + next digit + '#'  (e.g. "26#" = 'z')
        // Digit check: (unsigned char)(c - '0') <= 9u avoids a call to isdigit()
        if (i + 2 < n && (unsigned char)(s[i+1] - '0') <= 9u && s[i+2] == '#') {
            idx = 10 * (s[i] - '0') + (s[i+1] - '0') - 1;  // 10→j(9) … 26→z(25)
            i += 3;
        } else {
            idx = (s[i] - '0') - 1;   // 1→a(0) … 9→i(8)
            ++i;
        }

        // C++17: if-with-initializer – 'count' is scoped to this if/else block.
        // If a repeat-count '(N)' follows, count=N; otherwise count stays 1.
        if (int count = 1; i < n && s[i] == '(') {
            ++i;                       // skip '('
            count = 0;
            while (i < n && s[i] != ')') count = count * 10 + (s[i++] - '0');
            ++i;                       // skip ')'
            freq[idx] += count;
        } else {
            freq[idx] += count;        // count == 1 (single occurrence)
        }
    }

    return {freq.begin(), freq.end()};  // C++17: brace-init deduces vector<int>
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
//   Time : O(n)   – single forward pass, no backtracking
//   Space: O(1)   – 26-int stack array; output vector aside
//
// Key C++17 design decisions:
//   1. string_view parameter    → avoids copying the input string (O(1) view)
//   2. if-with-initializer      → 'count' lifetime is limited to the if/else;
//                                  prevents accidental reuse after the block
//   3. [[nodiscard]]            → makes it a compile warning to discard result
//   4. Unsigned digit check     → (unsigned char)(c - '0') <= 9u is branchless
//                                  and avoids locale-dependent isdigit()
//
// Test cases:
//   "1226#24#"             → [1,1,0,...,0,1,0,1]  (a,b,x,z)
//   "1(2)23(3)"            → [2,1,3,0,...,0]       (a×2,b,c×3)
//   "2110#(2)"             → [1,1,0,...,2,0,...]   (a,b,j×2)
//   "23#(2)24#25#26#23#(3)"→ [0,...,5,1,1,1]      (w×5,x,y,z)
// =============================================================================
