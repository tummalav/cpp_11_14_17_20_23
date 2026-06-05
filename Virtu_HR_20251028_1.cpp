#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;

/* Problem:
*/
/* Implement most efficiently possible solution with best time complexity. */
There is a box with a capacity of 5000 grams. The box may already contain some items, reducing its capacity. You'll be adding apples to that box until it is full.

Write a function that, given a zero-indexed array A consisting of N integers, representing the weight of items already in the box and each apple's weight, returns the maximum number of apples that could fit in the box, without exceeding its capacity.

The input array consists of an integer K as the first element, representing the sum of the weights of items already contained in the box, followed by zero or more integers representing individual apple weights.

You may assume that A contains between 1 and 100 elements and that every number in it is between 0 and 5000.

Example

Input: (4650, 150, 150, 150]

Output: 2

The box already contains 4650 grams of items, so only 2 more apples of weight 150 would fit (bringing the total weight to 4950, still below the capacity).

int solution(vector<int> A) {
    // Handle edge cases
    if (A.empty()) {
        return 0;
    }

    const int BOX_CAPACITY = 5000;
    int currentWeight = A[0];

    // Handle negative current weight (invalid input)
    if (currentWeight < 0) {
        return 0;
    }

    // Handle current weight exceeding maximum possible value
    if (currentWeight > BOX_CAPACITY) {
        return 0;
    }

    // If box is already at capacity
    if (currentWeight == BOX_CAPACITY) {
        return 0;
    }

    int remainingCapacity = BOX_CAPACITY - currentWeight;

    // Handle case where only current weight is provided (no apples)
    if (A.size() <= 1) {
        return 0;
    }

    // Extract apple weights (skip first element which is current weight)
    vector<int> appleWeights;
    appleWeights.reserve(A.size() - 1);
=-1`    for (size_t i = 1; i < A.size(); ++i) {
        // Only consider positive weight apples within valid range
        if (A[i] > 0 && A[i] <= BOX_CAPACITY) {
            appleWeights.push_back(A[i]);
        }
    }

    // If no valid apples available
    if (appleWeights.empty()) {
        return 0;
    }

    // Sort apples by weight (lightest first) - greedy approach
    sort(appleWeights.begin(), appleWeights.end());

    int appleCount = 0;
    long long totalAppleWeight = 0;  // Use long long to prevent overflow

    // Greedily pick lightest apples first
    for (int appleWeight : appleWeights) {
        // Check for potential overflow and capacity constraint
        if (totalAppleWeight + appleWeight <= remainingCapacity) {
            totalAppleWeight += appleWeight;
            ++appleCount;
        } else {
            break;  // Can't fit this apple or any heavier ones
        }
    }

    return appleCount;
}
-*/
// Comprehensive test function to verify all corner cases
void testSolution() {
    cout << "Testing Apple Box Solution - Comprehensive Corner Cases:\n\n";

    // Test case 1: Given example
    vector<int> test1 = {4650, 150, 150, 150};
    cout << "Test 1: [4650, 150, 150, 150]\n";
    cout << "Result: " << solution(test1) << "\n";
    cout << "Expected: 2\n\n";

    // Test case 2: Empty box
    vector<int> test2 = {0, 100, 200, 300, 400, 500};
    cout << "Test 2: [0, 100, 200, 300, 400, 500]\n";
    cout << "Result: " << solution(test2) << "\n";
    cout << "Expected: 5 (all apples fit)\n\n";

    // Test case 3: Box already full
    vector<int> test3 = {5000, 100, 200};
    cout << "Test 3: [5000, 100, 200]\n";
    cout << "Result: " << solution(test3) << "\n";
    cout << "Expected: 0\n\n";

    // Test case 4: Very light apples
    vector<int> test4 = {4990, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    cout << "Test 4: [4990, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]\n";
    cout << "Result: " << solution(test4) << "\n";
    cout << "Expected: 10 (can fit 10 apples of weight 1)\n\n";

    // Test case 5: Mixed weights
    vector<int> test5 = {1000, 500, 1000, 100, 200, 300, 50, 75};
    cout << "Test 5: [1000, 500, 1000, 100, 200, 300, 50, 75]\n";
    cout << "Result: " << solution(test5) << "\n";
    cout << "Expected: 6 (50, 75, 100, 200, 300, 500)\n\n";

    // Test case 6: Edge case - one heavy apple that fits exactly
    vector<int> test6 = {0, 5000};
    cout << "Test 6: [0, 5000]\n";
    cout << "Result: " << solution(test6) << "\n";
    cout << "Expected: 1\n\n";

    // Test case 7: Edge case - one heavy apple that doesn't fit
    vector<int> test7 = {1, 5000};
    cout << "Test 7: [1, 5000]\n";
    cout << "Result: " << solution(test7) << "\n";
    cout << "Expected: 0\n\n";

    // Test case 8: Empty array
    vector<int> test8 = {};
    cout << "Test 8: []\n";
    cout << "Result: " << solution(test8) << "\n";
    cout << "Expected: 0\n\n";

    // Test case 9: Only current weight, no apples
    vector<int> test9 = {1000};
    cout << "Test 9: [1000]\n";
    cout << "Result: " << solution(test9) << "\n";
    cout << "Expected: 0\n\n";

    // Test case 10: Negative current weight
    vector<int> test10 = {-100, 100, 200};
    cout << "Test 10: [-100, 100, 200]\n";
    cout << "Result: " << solution(test10) << "\n";
    cout << "Expected: 0 (invalid negative weight)\n\n";

    // Test case 11: Current weight exceeds capacity
    vector<int> test11 = {6000, 100, 200};
    cout << "Test 11: [6000, 100, 200]\n";
    cout << "Result: " << solution(test11) << "\n";
    cout << "Expected: 0\n\n";

    // Test case 12: Zero weight apples (should be ignored)
    vector<int> test12 = {100, 0, 0, 100, 0, 200};
    cout << "Test 12: [100, 0, 0, 100, 0, 200]\n";
    cout << "Result: " << solution(test12) << "\n";
    cout << "Expected: 2 (only positive weight apples count)\n\n";

    // Test case 13: Negative weight apples (should be ignored)
    vector<int> test13 = {100, -50, 100, -100, 200};
    cout << "Test 13: [100, -50, 100, -100, 200]\n";
    cout << "Result: " << solution(test13) << "\n";
    cout << "Expected: 2 (negative weights ignored)\n\n";

    // Test case 14: Apple weights exceeding capacity (should be ignored)
    vector<int> test14 = {100, 6000, 100, 7000, 200};
    cout << "Test 14: [100, 6000, 100, 7000, 200]\n";
    cout << "Result: " << solution(test14) << "\n";
    cout << "Expected: 2 (oversized apples ignored)\n\n";

    // Test case 15: All apples too heavy individually
    vector<int> test15 = {100, 5000, 4950, 4900};
    cout << "Test 15: [100, 5000, 4950, 4900]\n";
    cout << "Result: " << solution(test15) << "\n";
    cout << "Expected: 1 (only one 4900 fits)\n\n";

    // Test case 16: Exact capacity match
    vector<int> test16 = {0, 2500, 2500};
    cout << "Test 16: [0, 2500, 2500]\n";
    cout << "Result: " << solution(test16) << "\n";
    cout << "Expected: 2 (exactly 5000)\n\n";

    // Test case 17: Single apple of weight 1 in almost full box
    vector<int> test17 = {4999, 1};
    cout << "Test 17: [4999, 1]\n";
    cout << "Result: " << solution(test17) << "\n";
    cout << "Expected: 1\n\n";

    // Test case 18: Many small apples
    vector<int> test18 = {0};
    for (int i = 0; i < 50; ++i) {
        test18.push_back(100);  // 50 apples of 100g each
    }
    cout << "Test 18: [0, 100x50]\n";
    cout << "Result: " << solution(test18) << "\n";
    cout << "Expected: 50 (5000/100 = 50)\n\n";
}

int main() {
    testSolution();
    return 0;
}
