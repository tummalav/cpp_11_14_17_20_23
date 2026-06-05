// =============================================================================
// Problem 3: Long Encoded String  |  C++11
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
// String "wwxyzwww" is encodedas s = "23#(2)24#25#26#23#(3)"

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
// C++11 features:
//   - std::array<int,26> (fixed-size stack array)
//   - auto, range-based for
//   - nullptr, noexcept
//   - Index-based string iteration
// =============================================================================
#include <array>
#include <string>
#include <vector>
#include <cctype>
#include <iostream>
using namespace std;

vector<int> frequency(const string& s) {
    array<int, 26> freq;              // C++11: std::array
    freq.fill(0);                     // C++11: fill()

    const int n = static_cast<int>(s.size());
    int i = 0;

    while (i < n) {
        int letter_idx;

        // Two-digit code: digit + digit + '#'
        if (i + 2 < n && isdigit(static_cast<unsigned char>(s[i+1]))
                       && s[i+2] == '#') {
            letter_idx = 10*(s[i]-'0') + (s[i+1]-'0') - 1;
            i += 3;
        } else {
            letter_idx = (s[i]-'0') - 1;
            i += 1;
        }

        int count = 1;
        if (i < n && s[i] == '(') {
            ++i;
            int num = 0;
            while (i < n && s[i] != ')') { num = num*10 + (s[i]-'0'); ++i; }
            ++i;
            count = num;
        }
        freq[letter_idx] += count;
    }
    return vector<int>(freq.begin(), freq.end());  // C++11: range constructor
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    string s; cin >> s;
    auto result = frequency(s);                    // C++11: auto
    for (int i = 0; i < 26; ++i)                  // C++11: range-based for
        cout << result[i] << (i < 25 ? ' ' : '\n');
    return 0;
}

