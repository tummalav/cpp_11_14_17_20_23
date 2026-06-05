//
// Created by Krapa Haritha on 28/10/25.
/* Problem :
Write a function that, given a string S, returns an integer that represents the number of ways we can select a non-empty substring of S in which all of the characters are identical.

For example, the string "zzzyz" contains 8 such substrings: four instances of "z", two of "zz", one of "zzz" and one of "y".

String "k" contains only one such substring: "k".

You may assume that the length of S is between 1 and 100, and each character in S is a lowercase letter (a-z).

int solution(string S) {
}
*/

#include <string>
#include <iostream>

using namespace std;

/*
Algorithm:
- For each sequence of consecutive identical characters of length n,
  the number of substrings is n*(n+1)/2
- Time Complexity: O(n) where n is the length of string
- Space Complexity: O(1)
*/

int solution(string S) {
    if (S.empty()) {
        return 0;
    }

    int totalCount = 0;
    int currentSequenceLength = 1;

    // Iterate through the string starting from the second character
    for (size_t i = 1; i < S.length(); ++i) {
        if (S[i] == S[i - 1]) {
            // Same character as previous, extend current sequence
            ++currentSequenceLength;
        } else {
            // Different character, calculate substrings for current sequence
            // For n consecutive identical chars: n*(n+1)/2 substrings
            totalCount += (currentSequenceLength * (currentSequenceLength + 1)) / 2;
            currentSequenceLength = 1;  // Reset for new sequence
        }
    }

    // Don't forget to add the last sequence
    totalCount += (currentSequenceLength * (currentSequenceLength + 1)) / 2;

    return totalCount;
}

// Test function to verify the solution
void testSolution() {
    cout << "Testing Identical Character Substring Counter:\n\n";

    // Test case 1: Given example "zzzyz"
    string test1 = "zzzyz";
    cout << "Test 1: \"" << test1 << "\"\n";
    cout << "Result: " << solution(test1) << "\n";
    cout << "Expected: 8 (zzz=6, y=1, z=1 -> Total=8)\n";
    cout << "Breakdown: 3 'z' + 2 'zz' + 1 'zzz' + 1 'y' + 1 'z' = 8\n\n";

    // Test case 2: Single character
    string test2 = "k";
    cout << "Test 2: \"" << test2 << "\"\n";
    cout << "Result: " << solution(test2) << "\n";
    cout << "Expected: 1\n\n";

    // Test case 3: All same characters
    string test3 = "aaaa";
    cout << "Test 3: \"" << test3 << "\"\n";
    cout << "Result: " << solution(test3) << "\n";
    cout << "Expected: 10 (4*5/2 = 10)\n";
    cout << "Breakdown: 4 'a' + 3 'aa' + 2 'aaa' + 1 'aaaa' = 10\n\n";

    // Test case 4: All different characters
    string test4 = "abcd";
    cout << "Test 4: \"" << test4 << "\"\n";
    cout << "Result: " << solution(test4) << "\n";
    cout << "Expected: 4 (each character contributes 1)\n\n";

    // Test case 5: Mixed pattern
    string test5 = "aabbcc";
    cout << "Test 5: \"" << test5 << "\"\n";
    cout << "Result: " << solution(test5) << "\n";
    cout << "Expected: 9 (3 pairs, each pair contributes 3)\n";
    cout << "Breakdown: aa=3, bb=3, cc=3 -> Total=9\n\n";

    // Test case 6: Long sequence
    string test6 = "aaaabbbbcccc";
    cout << "Test 6: \"" << test6 << "\"\n";
    cout << "Result: " << solution(test6) << "\n";
    cout << "Expected: 30 (aaaa=10, bbbb=10, cccc=10)\n\n";

    // Test case 7: Alternating pattern
    string test7 = "ababab";
    cout << "Test 7: \"" << test7 << "\"\n";
    cout << "Result: " << solution(test7) << "\n";
    cout << "Expected: 6 (each character contributes 1)\n\n";

    // Test case 8: Complex pattern
    string test8 = "aabbbccccdddd";
    cout << "Test 8: \"" << test8 << "\"\n";
    cout << "Result: " << solution(test8) << "\n";
    cout << "Expected: 29 (aa=3, bbb=6, cccc=10, dddd=10 -> 3+6+10+10=29)\n\n";
}

int main() {
    testSolution();
    return 0;
}
