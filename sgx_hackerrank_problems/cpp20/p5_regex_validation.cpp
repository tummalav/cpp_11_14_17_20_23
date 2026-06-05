// Problem 5: Regex Validation | C++20
// C++20: [[likely]]/[[unlikely]], std::string_view, regex_constants::optimize
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
// Compiler support:
//   GCC 9+  (primary)    : [[likely]]/[[unlikely]] supported
//   Clang 12+ (secondary): [[likely]]/[[unlikely]] supported
//   Apple Clang 12       : NOT supported; guarded with __has_cpp_attribute.
#include <string>
#include <string_view>
#include <regex>
#include <iostream>
using namespace std;
int main() {
    ios_base::sync_with_stdio(false); cin.tie(nullptr);
    const regex re(R"(^([ab])([ab]*\1)?$)",
                   regex_constants::ECMAScript | regex_constants::optimize);
    int q; cin >> q;
    while (q--) {
        string s; cin >> s;
        // C++20: [[likely]]/[[unlikely]] — guarded for compilers that lack support
#if __has_cpp_attribute(likely)
        if (regex_match(s, re)) [[likely]]
            cout << "True\n";
        else [[unlikely]]
            cout << "False\n";
#else
        cout << (regex_match(s, re) ? "True\n" : "False\n");
#endif
    }
}
