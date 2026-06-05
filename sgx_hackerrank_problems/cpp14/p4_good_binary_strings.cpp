// =============================================================================
// Problem 4: Good Binary Strings  |  C++14
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
// C++14 additions:
//   - Return type deduction
//   - Generic lambda for sort comparator
// =============================================================================
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
using namespace std;

// C++14: return type deduced
auto largestMagical(const string& s) -> string {    // still allowed with explicit
    if (s.empty()) return {};

    const int n = static_cast<int>(s.size());
    vector<string> parts;
    int balance = 0, start = 0;

    for (int i = 0; i < n; ++i) {
        balance += (s[i] == '1') ? 1 : -1;
        if (balance == 0) {
            parts.emplace_back(s.substr(start, i - start + 1)); // C++14: emplace_back
            start = i + 1;
        }
    }

    for (auto& p : parts) {
        if (p.size() > 2) {
            p = '1' + largestMagical(p.substr(1, p.size()-2)) + '0';
        }
    }

    // C++14: generic lambda – auto parameters
    sort(parts.begin(), parts.end(), [](auto& a, auto& b){ return a > b; });

    string result;
    result.reserve(static_cast<size_t>(n));
    for (auto& p : parts) result += p;
    return result;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    string s; cin >> s;
    cout << largestMagical(s) << '\n';
    return 0;
}

