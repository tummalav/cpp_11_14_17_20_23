//
// Created by Krapa Haritha on 28/10/25.
/* problem:
Write a function that, given a zero-indexed array A consisting o N integers representing the initial test scores of a row of students, returns an array of integers representing their final tes scores (in the same order).

There is a group of students who sit next to each other in a row.

Each day, the students study together and take a test at the end of the day. Test scores for a given student can only change once per day as follows:

﻿﻿If a student sits immediately between two students with better scores, that student's score will improve by 1 when they take the next test.
﻿﻿If a student sits between two students wit worse scores, that student's test score will decrease by 1.
This process will repeat each day as long as at least one student's score changes. Note that the first and last student in the row never change their scores as they never sit between two students.

Return an array representing the final test scores for each student.

You can assume that:

• The number of studente ic in the rende from 1 ta 1000



You can assume that:

﻿﻿The number of students is in the range from 1 to 1,000.
﻿﻿Scores are in the range from 0 to 1,000.
Example 1:

Confiden

Input: [1, 6, 3, 4, 3, 51

Output: (1, 4, 4, 4, 4, 5]

On the first day, the second student's score will decrease, the third student's score will increase, the fourth student's score will decrease and the fifth student's score will increase, yielding 11, 5, 4, 3, 4, 5].

On the second day, the second student's score will decrease again and the fourth student's score will increase, yielding 11, 4.

4. 4. 4, 51. There will be no more changes in scores after that.

Example 2:

Input: [100, 50, 40, 30]

Output: [100, 50, 40, 30]

No scores will change.
*/

#include <vector>
#include <iostream>
#include <algorithm>

using namespace std;

/*
Optimal Algorithm:
- Simulate the daily score changes until convergence
- Use two arrays to avoid interference during simultaneous updates
- Time Complexity: O(n * d) where n = students, d = days until convergence
- Space Complexity: O(n) for the working array
- Corner cases handled: empty array, single student, no changes needed
*/

vector<int> solution(vector<int> A) {
    // Handle corner cases
    if (A.empty()) {
        return {};
    }

    if (A.size() <= 2) {
        return A;  // First and last students never change
    }

    vector<int> current = A;
    vector<int> next = A;
    bool hasChanges = true;

    // Continue until no more changes occur
    while (hasChanges) {
        hasChanges = false;

        // Process middle students (indices 1 to n-2)
        for (size_t i = 1; i < current.size() - 1; ++i) {
            int leftScore = current[i - 1];
            int rightScore = current[i + 1];
            int currentScore = current[i];

            // Check if student is between two better students
            if (leftScore > currentScore && rightScore > currentScore) {
                next[i] = currentScore + 1;
                hasChanges = true;
            }
            // Check if student is between two worse students
            else if (leftScore < currentScore && rightScore < currentScore) {
                next[i] = currentScore - 1;
                hasChanges = true;
            }
            // Otherwise, score remains the same
            else {
                next[i] = currentScore;
            }
        }

        // Copy next state to current for next iteration
        current = next;
    }

    return current;
}

// Test function with comprehensive corner cases
void testSolution() {
    cout << "Testing Student Score Adjustment Solution:\n\n";

    // Test case 1: Given example 1
    vector<int> test1 = {1, 6, 3, 4, 3, 5};
    auto result1 = solution(test1);
    cout << "Test 1: [1, 6, 3, 4, 3, 5]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result1.size(); ++i) {
        cout << result1[i];
        if (i < result1.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [1, 4, 4, 4, 4, 5]\n\n";

    // Test case 2: Given example 2
    vector<int> test2 = {100, 50, 40, 30};
    auto result2 = solution(test2);
    cout << "Test 2: [100, 50, 40, 30]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result2.size(); ++i) {
        cout << result2[i];
        if (i < result2.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [100, 50, 40, 30]\n\n";

    // Test case 3: Empty array
    vector<int> test3 = {};
    auto result3 = solution(test3);
    cout << "Test 3: []\n";
    cout << "Result: [";
    for (size_t i = 0; i < result3.size(); ++i) {
        cout << result3[i];
        if (i < result3.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: []\n\n";

    // Test case 4: Single student
    vector<int> test4 = {50};
    auto result4 = solution(test4);
    cout << "Test 4: [50]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result4.size(); ++i) {
        cout << result4[i];
        if (i < result4.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [50]\n\n";

    // Test case 5: Two students
    vector<int> test5 = {10, 20};
    auto result5 = solution(test5);
    cout << "Test 5: [10, 20]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result5.size(); ++i) {
        cout << result5[i];
        if (i < result5.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [10, 20]\n\n";

    // Test case 6: All same scores
    vector<int> test6 = {50, 50, 50, 50, 50};
    auto result6 = solution(test6);
    cout << "Test 6: [50, 50, 50, 50, 50]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result6.size(); ++i) {
        cout << result6[i];
        if (i < result6.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [50, 50, 50, 50, 50]\n\n";

    // Test case 7: Alternating high-low pattern
    vector<int> test7 = {10, 1, 10, 1, 10};
    auto result7 = solution(test7);
    cout << "Test 7: [10, 1, 10, 1, 10]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result7.size(); ++i) {
        cout << result7[i];
        if (i < result7.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [10, 2, 9, 2, 10] (converges after multiple iterations)\n\n";

    // Test case 8: Mountain pattern
    vector<int> test8 = {1, 2, 3, 2, 1};
    auto result8 = solution(test8);
    cout << "Test 8: [1, 2, 3, 2, 1]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result8.size(); ++i) {
        cout << result8[i];
        if (i < result8.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [1, 2, 3, 2, 1] (no changes)\n\n";

    // Test case 9: Valley pattern
    vector<int> test9 = {5, 3, 1, 3, 5};
    auto result9 = solution(test9);
    cout << "Test 9: [5, 3, 1, 3, 5]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result9.size(); ++i) {
        cout << result9[i];
        if (i < result9.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: [5, 3, 2, 3, 5] (middle student improves)\n\n";

    // Test case 10: Large array with extreme values
    vector<int> test10 = {0, 1000, 0, 1000, 0};
    auto result10 = solution(test10);
    cout << "Test 10: [0, 1000, 0, 1000, 0]\n";
    cout << "Result: [";
    for (size_t i = 0; i < result10.size(); ++i) {
        cout << result10[i];
        if (i < result10.size() - 1) cout << ", ";
    }
    cout << "]\n";
    cout << "Expected: Converges after many iterations\n\n";
}

int main() {
    testSolution();
    return 0;
}