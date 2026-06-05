// Problem 5: Regex Validation | C++11
/*
Given a list of strings made up of the characters 'a' and 'b', create a regular expression thatwill match strings
that begin and end with the same letter.

Example
'a', 'aa', and 'bababbb' match.
'ab' and 'baba' do not match.

Replacing the blank(i.e, " ") with a regular expression that matches strings as described. locked
code in the editor prints True for each correct match and False for each incorrect match.

Constraints
    1 <= qquery < 1000
    1 <= |string| <= 1000
    Each character string[i] {1,b}

Input format for custom testing
sample case 0
STDIN - 5, Function - number of strings to be tested, query = 5
a -> query strings = 'a', 'b', 'ab', 'ba', 'aba'
b
ab
ba
aba

Sample Output

True
True
False
False
True


*/

// C++11: std::regex (introduced C++11), raw string literals R"(...)"
#include <string>
#include <regex>
#include <iostream>
using namespace std;
int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    const regex regularExpression(R"(^([ab])([ab]*\1)?$)");
    int query; cin >> query;
    while (query--) {
        string s; cin >> s;
        cout << (regex_match(s, regularExpression) ? "True" : "False") << '\n';
    }
    return 0;
}
