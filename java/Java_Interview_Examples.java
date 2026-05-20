/**
 * ================================================================================================
 * JAVA INTERVIEW EXAMPLES - COMPREHENSIVE GUIDE
 * ================================================================================================
 *
 * This file contains commonly asked Java interview questions with detailed explanations,
 * code examples, and best practices for software engineering interviews.
 *
 * Topics Covered:
 * 1. String Manipulation
 * 2. Array and List Operations
 * 3. HashMap and Collections
 * 4. Tree and Graph Problems
 * 5. Sorting and Searching
 * 6. Dynamic Programming
 * 7. Concurrency Problems
 * 8. Design Patterns
 * 9. Java-Specific Questions
 * 10. System Design Concepts
 *
 * ================================================================================================
 */

import java.util.*;
import java.util.concurrent.*;
import java.util.stream.*;
import java.util.function.*;

// ================================================================================================
// 1. STRING MANIPULATION PROBLEMS
// ================================================================================================

class StringProblems {

    /**
     * Q1: Reverse a String
     * Input: "hello"
     * Output: "olleh"
     */
    public static String reverseString(String str) {
        if (str == null || str.isEmpty()) {
            return str;
        }

        // Method 1: Using StringBuilder
        return new StringBuilder(str).reverse().toString();

        // Method 2: Using char array
        // char[] chars = str.toCharArray();
        // int left = 0, right = chars.length - 1;
        // while (left < right) {
        //     char temp = chars[left];
        //     chars[left++] = chars[right];
        //     chars[right--] = temp;
        // }
        // return new String(chars);
    }

    /**
     * Q2: Check if String is Palindrome
     * Input: "racecar"
     * Output: true
     */
    public static boolean isPalindrome(String str) {
        if (str == null || str.length() <= 1) {
            return true;
        }

        int left = 0, right = str.length() - 1;
        while (left < right) {
            if (str.charAt(left++) != str.charAt(right--)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Q3: Check if Two Strings are Anagrams
     * Input: "listen", "silent"
     * Output: true
     */
    public static boolean areAnagrams(String str1, String str2) {
        if (str1 == null || str2 == null || str1.length() != str2.length()) {
            return false;
        }

        // Method 1: Using sorting
        char[] chars1 = str1.toCharArray();
        char[] chars2 = str2.toCharArray();
        Arrays.sort(chars1);
        Arrays.sort(chars2);
        return Arrays.equals(chars1, chars2);

        // Method 2: Using HashMap (better for large strings)
        // Map<Character, Integer> map = new HashMap<>();
        // for (char c : str1.toCharArray()) {
        //     map.put(c, map.getOrDefault(c, 0) + 1);
        // }
        // for (char c : str2.toCharArray()) {
        //     if (!map.containsKey(c)) return false;
        //     map.put(c, map.get(c) - 1);
        //     if (map.get(c) == 0) map.remove(c);
        // }
        // return map.isEmpty();
    }

    /**
     * Q4: First Non-Repeating Character
     * Input: "leetcode"
     * Output: 'l'
     */
    public static char firstNonRepeatingChar(String str) {
        if (str == null || str.isEmpty()) {
            return '\0';
        }

        Map<Character, Integer> charCount = new LinkedHashMap<>();
        for (char c : str.toCharArray()) {
            charCount.put(c, charCount.getOrDefault(c, 0) + 1);
        }

        for (Map.Entry<Character, Integer> entry : charCount.entrySet()) {
            if (entry.getValue() == 1) {
                return entry.getKey();
            }
        }

        return '\0';  // Not found
    }

    /**
     * Q5: Longest Substring Without Repeating Characters
     * Input: "abcabcbb"
     * Output: 3 (substring: "abc")
     */
    public static int lengthOfLongestSubstring(String s) {
        if (s == null || s.isEmpty()) {
            return 0;
        }

        Map<Character, Integer> charIndex = new HashMap<>();
        int maxLength = 0;
        int start = 0;

        for (int end = 0; end < s.length(); end++) {
            char c = s.charAt(end);

            if (charIndex.containsKey(c)) {
                start = Math.max(start, charIndex.get(c) + 1);
            }

            charIndex.put(c, end);
            maxLength = Math.max(maxLength, end - start + 1);
        }

        return maxLength;
    }

    /**
     * Q6: String Compression
     * Input: "aabcccccaaa"
     * Output: "a2b1c5a3"
     */
    public static String compressString(String str) {
        if (str == null || str.length() <= 1) {
            return str;
        }

        StringBuilder compressed = new StringBuilder();
        int count = 1;

        for (int i = 1; i < str.length(); i++) {
            if (str.charAt(i) == str.charAt(i - 1)) {
                count++;
            } else {
                compressed.append(str.charAt(i - 1)).append(count);
                count = 1;
            }
        }

        // Add last character
        compressed.append(str.charAt(str.length() - 1)).append(count);

        return compressed.length() < str.length() ? compressed.toString() : str;
    }
}

// ================================================================================================
// 2. ARRAY AND LIST OPERATIONS
// ================================================================================================

class ArrayProblems {

    /**
     * Q7: Find Two Numbers that Sum to Target (Two Sum)
     * Input: nums = [2,7,11,15], target = 9
     * Output: [0,1]
     */
    public static int[] twoSum(int[] nums, int target) {
        Map<Integer, Integer> map = new HashMap<>();

        for (int i = 0; i < nums.length; i++) {
            int complement = target - nums[i];
            if (map.containsKey(complement)) {
                return new int[] { map.get(complement), i };
            }
            map.put(nums[i], i);
        }

        return new int[] { -1, -1 };
    }

    /**
     * Q8: Find Maximum Subarray Sum (Kadane's Algorithm)
     * Input: [-2,1,-3,4,-1,2,1,-5,4]
     * Output: 6 (subarray: [4,-1,2,1])
     */
    public static int maxSubArray(int[] nums) {
        if (nums == null || nums.length == 0) {
            return 0;
        }

        int maxSum = nums[0];
        int currentSum = nums[0];

        for (int i = 1; i < nums.length; i++) {
            currentSum = Math.max(nums[i], currentSum + nums[i]);
            maxSum = Math.max(maxSum, currentSum);
        }

        return maxSum;
    }

    /**
     * Q9: Rotate Array
     * Input: nums = [1,2,3,4,5,6,7], k = 3
     * Output: [5,6,7,1,2,3,4]
     */
    public static void rotateArray(int[] nums, int k) {
        if (nums == null || nums.length <= 1) {
            return;
        }

        k = k % nums.length;
        reverse(nums, 0, nums.length - 1);
        reverse(nums, 0, k - 1);
        reverse(nums, k, nums.length - 1);
    }

    private static void reverse(int[] nums, int start, int end) {
        while (start < end) {
            int temp = nums[start];
            nums[start++] = nums[end];
            nums[end--] = temp;
        }
    }

    /**
     * Q10: Find Missing Number
     * Input: [3,0,1]
     * Output: 2
     */
    public static int findMissingNumber(int[] nums) {
        int n = nums.length;
        int expectedSum = n * (n + 1) / 2;
        int actualSum = 0;

        for (int num : nums) {
            actualSum += num;
        }

        return expectedSum - actualSum;
    }

    /**
     * Q11: Remove Duplicates from Sorted Array (In-Place)
     * Input: [1,1,2,2,3,4,4]
     * Output: 4, array becomes [1,2,3,4,_,_,_]
     */
    public static int removeDuplicates(int[] nums) {
        if (nums == null || nums.length == 0) {
            return 0;
        }

        int writeIndex = 1;

        for (int readIndex = 1; readIndex < nums.length; readIndex++) {
            if (nums[readIndex] != nums[readIndex - 1]) {
                nums[writeIndex++] = nums[readIndex];
            }
        }

        return writeIndex;
    }

    /**
     * Q12: Merge Two Sorted Arrays
     * Input: nums1 = [1,2,3,0,0,0], m = 3, nums2 = [2,5,6], n = 3
     * Output: [1,2,2,3,5,6]
     */
    public static void mergeSortedArrays(int[] nums1, int m, int[] nums2, int n) {
        int i = m - 1;
        int j = n - 1;
        int k = m + n - 1;

        while (i >= 0 && j >= 0) {
            if (nums1[i] > nums2[j]) {
                nums1[k--] = nums1[i--];
            } else {
                nums1[k--] = nums2[j--];
            }
        }

        while (j >= 0) {
            nums1[k--] = nums2[j--];
        }
    }
}

// ================================================================================================
// 3. HASHMAP AND COLLECTIONS PROBLEMS
// ================================================================================================

class HashMapProblems {

    /**
     * Q13: Group Anagrams
     * Input: ["eat","tea","tan","ate","nat","bat"]
     * Output: [["bat"],["nat","tan"],["ate","eat","tea"]]
     */
    public static List<List<String>> groupAnagrams(String[] strs) {
        Map<String, List<String>> map = new HashMap<>();

        for (String str : strs) {
            char[] chars = str.toCharArray();
            Arrays.sort(chars);
            String key = new String(chars);

            map.computeIfAbsent(key, k -> new ArrayList<>()).add(str);
        }

        return new ArrayList<>(map.values());
    }

    /**
     * Q14: Implement LRU Cache
     */
    static class LRUCache {
        private final int capacity;
        private final Map<Integer, Node> cache;
        private final Node head;
        private final Node tail;

        static class Node {
            int key;
            int value;
            Node prev;
            Node next;

            Node(int key, int value) {
                this.key = key;
                this.value = value;
            }
        }

        public LRUCache(int capacity) {
            this.capacity = capacity;
            this.cache = new HashMap<>();
            this.head = new Node(0, 0);
            this.tail = new Node(0, 0);
            head.next = tail;
            tail.prev = head;
        }

        public int get(int key) {
            if (!cache.containsKey(key)) {
                return -1;
            }

            Node node = cache.get(key);
            remove(node);
            addToHead(node);

            return node.value;
        }

        public void put(int key, int value) {
            if (cache.containsKey(key)) {
                Node node = cache.get(key);
                node.value = value;
                remove(node);
                addToHead(node);
            } else {
                if (cache.size() >= capacity) {
                    Node lru = tail.prev;
                    remove(lru);
                    cache.remove(lru.key);
                }

                Node newNode = new Node(key, value);
                cache.put(key, newNode);
                addToHead(newNode);
            }
        }

        private void remove(Node node) {
            node.prev.next = node.next;
            node.next.prev = node.prev;
        }

        private void addToHead(Node node) {
            node.next = head.next;
            node.prev = head;
            head.next.prev = node;
            head.next = node;
        }
    }

    /**
     * Q15: Top K Frequent Elements
     * Input: nums = [1,1,1,2,2,3], k = 2
     * Output: [1,2]
     */
    public static int[] topKFrequent(int[] nums, int k) {
        Map<Integer, Integer> frequencyMap = new HashMap<>();
        for (int num : nums) {
            frequencyMap.put(num, frequencyMap.getOrDefault(num, 0) + 1);
        }

        PriorityQueue<Map.Entry<Integer, Integer>> heap = new PriorityQueue<>(
            (a, b) -> b.getValue() - a.getValue()
        );

        heap.addAll(frequencyMap.entrySet());

        int[] result = new int[k];
        for (int i = 0; i < k; i++) {
            result[i] = heap.poll().getKey();
        }

        return result;
    }
}

// ================================================================================================
// 4. TREE PROBLEMS
// ================================================================================================

class TreeNode {
    int val;
    TreeNode left;
    TreeNode right;

    TreeNode(int val) {
        this.val = val;
    }
}

class TreeProblems {

    /**
     * Q16: Binary Tree Level Order Traversal
     * Input: [3,9,20,null,null,15,7]
     * Output: [[3],[9,20],[15,7]]
     */
    public static List<List<Integer>> levelOrder(TreeNode root) {
        List<List<Integer>> result = new ArrayList<>();
        if (root == null) {
            return result;
        }

        Queue<TreeNode> queue = new LinkedList<>();
        queue.offer(root);

        while (!queue.isEmpty()) {
            int levelSize = queue.size();
            List<Integer> currentLevel = new ArrayList<>();

            for (int i = 0; i < levelSize; i++) {
                TreeNode node = queue.poll();
                currentLevel.add(node.val);

                if (node.left != null) queue.offer(node.left);
                if (node.right != null) queue.offer(node.right);
            }

            result.add(currentLevel);
        }

        return result;
    }

    /**
     * Q17: Maximum Depth of Binary Tree
     */
    public static int maxDepth(TreeNode root) {
        if (root == null) {
            return 0;
        }

        int leftDepth = maxDepth(root.left);
        int rightDepth = maxDepth(root.right);

        return Math.max(leftDepth, rightDepth) + 1;
    }

    /**
     * Q18: Validate Binary Search Tree
     */
    public static boolean isValidBST(TreeNode root) {
        return isValidBST(root, null, null);
    }

    private static boolean isValidBST(TreeNode node, Integer min, Integer max) {
        if (node == null) {
            return true;
        }

        if ((min != null && node.val <= min) || (max != null && node.val >= max)) {
            return false;
        }

        return isValidBST(node.left, min, node.val) &&
               isValidBST(node.right, node.val, max);
    }

    /**
     * Q19: Lowest Common Ancestor of Binary Tree
     */
    public static TreeNode lowestCommonAncestor(TreeNode root, TreeNode p, TreeNode q) {
        if (root == null || root == p || root == q) {
            return root;
        }

        TreeNode left = lowestCommonAncestor(root.left, p, q);
        TreeNode right = lowestCommonAncestor(root.right, p, q);

        if (left != null && right != null) {
            return root;
        }

        return left != null ? left : right;
    }

    /**
     * Q20: Serialize and Deserialize Binary Tree
     */
    public static String serialize(TreeNode root) {
        StringBuilder sb = new StringBuilder();
        serializeHelper(root, sb);
        return sb.toString();
    }

    private static void serializeHelper(TreeNode node, StringBuilder sb) {
        if (node == null) {
            sb.append("null,");
            return;
        }

        sb.append(node.val).append(",");
        serializeHelper(node.left, sb);
        serializeHelper(node.right, sb);
    }

    public static TreeNode deserialize(String data) {
        Queue<String> nodes = new LinkedList<>(Arrays.asList(data.split(",")));
        return deserializeHelper(nodes);
    }

    private static TreeNode deserializeHelper(Queue<String> nodes) {
        String val = nodes.poll();
        if ("null".equals(val)) {
            return null;
        }

        TreeNode node = new TreeNode(Integer.parseInt(val));
        node.left = deserializeHelper(nodes);
        node.right = deserializeHelper(nodes);

        return node;
    }
}

// ================================================================================================
// 5. DYNAMIC PROGRAMMING
// ================================================================================================

class DynamicProgrammingProblems {

    /**
     * Q21: Climbing Stairs
     * Input: n = 3
     * Output: 3 (1+1+1, 1+2, 2+1)
     */
    public static int climbStairs(int n) {
        if (n <= 2) {
            return n;
        }

        int prev2 = 1;
        int prev1 = 2;

        for (int i = 3; i <= n; i++) {
            int current = prev1 + prev2;
            prev2 = prev1;
            prev1 = current;
        }

        return prev1;
    }

    /**
     * Q22: Coin Change
     * Input: coins = [1,2,5], amount = 11
     * Output: 3 (11 = 5 + 5 + 1)
     */
    public static int coinChange(int[] coins, int amount) {
        int[] dp = new int[amount + 1];
        Arrays.fill(dp, amount + 1);
        dp[0] = 0;

        for (int i = 1; i <= amount; i++) {
            for (int coin : coins) {
                if (coin <= i) {
                    dp[i] = Math.min(dp[i], dp[i - coin] + 1);
                }
            }
        }

        return dp[amount] > amount ? -1 : dp[amount];
    }

    /**
     * Q23: Longest Increasing Subsequence
     * Input: [10,9,2,5,3,7,101,18]
     * Output: 4 ([2,3,7,101])
     */
    public static int lengthOfLIS(int[] nums) {
        if (nums == null || nums.length == 0) {
            return 0;
        }

        int[] dp = new int[nums.length];
        Arrays.fill(dp, 1);
        int maxLength = 1;

        for (int i = 1; i < nums.length; i++) {
            for (int j = 0; j < i; j++) {
                if (nums[i] > nums[j]) {
                    dp[i] = Math.max(dp[i], dp[j] + 1);
                }
            }
            maxLength = Math.max(maxLength, dp[i]);
        }

        return maxLength;
    }

    /**
     * Q24: House Robber
     * Input: [2,7,9,3,1]
     * Output: 12 (rob house 0, 2, 4)
     */
    public static int rob(int[] nums) {
        if (nums == null || nums.length == 0) {
            return 0;
        }

        if (nums.length == 1) {
            return nums[0];
        }

        int prev2 = nums[0];
        int prev1 = Math.max(nums[0], nums[1]);

        for (int i = 2; i < nums.length; i++) {
            int current = Math.max(prev1, prev2 + nums[i]);
            prev2 = prev1;
            prev1 = current;
        }

        return prev1;
    }
}

// ================================================================================================
// 6. CONCURRENCY PROBLEMS
// ================================================================================================

class ConcurrencyProblems {

    /**
     * Q25: Print in Order
     * Ensure three threads print in order: first, second, third
     */
    static class PrintInOrder {
        private volatile int order = 1;

        public void first() throws InterruptedException {
            System.out.print("first");
            order = 2;
        }

        public void second() throws InterruptedException {
            while (order != 2) {
                Thread.yield();
            }
            System.out.print("second");
            order = 3;
        }

        public void third() throws InterruptedException {
            while (order != 3) {
                Thread.yield();
            }
            System.out.print("third");
        }
    }

    /**
     * Q26: Producer-Consumer Problem
     */
    static class ProducerConsumer {
        private final Queue<Integer> queue = new LinkedList<>();
        private final int capacity;

        public ProducerConsumer(int capacity) {
            this.capacity = capacity;
        }

        public synchronized void produce(int item) throws InterruptedException {
            while (queue.size() == capacity) {
                wait();
            }

            queue.add(item);
            System.out.println("Produced: " + item);
            notifyAll();
        }

        public synchronized int consume() throws InterruptedException {
            while (queue.isEmpty()) {
                wait();
            }

            int item = queue.remove();
            System.out.println("Consumed: " + item);
            notifyAll();

            return item;
        }
    }

    /**
     * Q27: Thread-Safe Singleton
     */
    static class ThreadSafeSingleton {
        private static volatile ThreadSafeSingleton instance;

        private ThreadSafeSingleton() {
            // Private constructor
        }

        public static ThreadSafeSingleton getInstance() {
            if (instance == null) {
                synchronized (ThreadSafeSingleton.class) {
                    if (instance == null) {
                        instance = new ThreadSafeSingleton();
                    }
                }
            }
            return instance;
        }
    }
}

// ================================================================================================
// 7. DESIGN PATTERNS
// ================================================================================================

class DesignPatterns {

    /**
     * Q28: Singleton Pattern (Enum-based - best practice)
     */
    enum Singleton {
        INSTANCE;

        public void doSomething() {
            System.out.println("Singleton instance");
        }
    }

    /**
     * Q29: Factory Pattern
     */
    interface Shape {
        void draw();
    }

    static class Circle implements Shape {
        @Override
        public void draw() {
            System.out.println("Drawing Circle");
        }
    }

    static class Rectangle implements Shape {
        @Override
        public void draw() {
            System.out.println("Drawing Rectangle");
        }
    }

    static class ShapeFactory {
        public Shape getShape(String shapeType) {
            if (shapeType == null) {
                return null;
            }

            switch (shapeType.toUpperCase()) {
                case "CIRCLE":
                    return new Circle();
                case "RECTANGLE":
                    return new Rectangle();
                default:
                    return null;
            }
        }
    }

    /**
     * Q30: Observer Pattern
     */
    interface Observer {
        void update(String message);
    }

    static class Subject {
        private final List<Observer> observers = new ArrayList<>();

        public void attach(Observer observer) {
            observers.add(observer);
        }

        public void detach(Observer observer) {
            observers.remove(observer);
        }

        public void notifyObservers(String message) {
            for (Observer observer : observers) {
                observer.update(message);
            }
        }
    }
}

// ================================================================================================
// 8. JAVA-SPECIFIC QUESTIONS
// ================================================================================================

class JavaSpecificQuestions {

    /**
     * Q31: Demonstrate equals() and hashCode() contract
     */
    static class Employee {
        private final int id;
        private final String name;

        public Employee(int id, String name) {
            this.id = id;
            this.name = name;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            Employee employee = (Employee) o;
            return id == employee.id && Objects.equals(name, employee.name);
        }

        @Override
        public int hashCode() {
            return Objects.hash(id, name);
        }
    }

    /**
     * Q32: Demonstrate Java 8 Streams
     */
    public static void streamExamples() {
        List<Integer> numbers = Arrays.asList(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

        // Filter even numbers and square them
        List<Integer> result = numbers.stream()
            .filter(n -> n % 2 == 0)
            .map(n -> n * n)
            .collect(Collectors.toList());

        System.out.println("Even squares: " + result);

        // Sum of all numbers
        int sum = numbers.stream()
            .reduce(0, Integer::sum);

        System.out.println("Sum: " + sum);
    }

    /**
     * Q33: Custom Comparator
     */
    static class Person {
        String name;
        int age;

        Person(String name, int age) {
            this.name = name;
            this.age = age;
        }
    }

    public static void sortPersons() {
        List<Person> people = Arrays.asList(
            new Person("Alice", 30),
            new Person("Bob", 25),
            new Person("Charlie", 35)
        );

        // Sort by age
        people.sort(Comparator.comparingInt(p -> p.age));

        // Sort by name
        people.sort(Comparator.comparing(p -> p.name));
    }

    /**
     * Q34: Exception Handling Best Practices
     */
    public static void exceptionHandling() {
        // Try-with-resources (automatic resource management)
        try (Scanner scanner = new Scanner(System.in)) {
            int number = scanner.nextInt();
            System.out.println("Number: " + number);
        } catch (InputMismatchException e) {
            System.err.println("Invalid input: " + e.getMessage());
        }
    }

    /**
     * Q35: Custom Exception
     */
    static class InsufficientFundsException extends Exception {
        public InsufficientFundsException(String message) {
            super(message);
        }
    }

    static class BankAccount {
        private double balance;

        public void withdraw(double amount) throws InsufficientFundsException {
            if (amount > balance) {
                throw new InsufficientFundsException("Insufficient funds: " + balance);
            }
            balance -= amount;
        }
    }
}

// ================================================================================================
// MAIN CLASS - DEMONSTRATION
// ================================================================================================

public class Java_Interview_Examples {

    public static void main(String[] args) {
        System.out.println("=== JAVA INTERVIEW EXAMPLES ===\n");

        // String Problems
        System.out.println("1. Reverse String: " +
            StringProblems.reverseString("hello"));

        System.out.println("2. Is Palindrome: " +
            StringProblems.isPalindrome("racecar"));

        System.out.println("3. Are Anagrams: " +
            StringProblems.areAnagrams("listen", "silent"));

        System.out.println("4. First Non-Repeating Char: " +
            StringProblems.firstNonRepeatingChar("leetcode"));

        System.out.println("5. Longest Substring Without Repeating: " +
            StringProblems.lengthOfLongestSubstring("abcabcbb"));

        System.out.println("6. Compress String: " +
            StringProblems.compressString("aabcccccaaa"));

        // Array Problems
        System.out.println("\n7. Two Sum: " +
            Arrays.toString(ArrayProblems.twoSum(new int[]{2,7,11,15}, 9)));

        System.out.println("8. Max Subarray Sum: " +
            ArrayProblems.maxSubArray(new int[]{-2,1,-3,4,-1,2,1,-5,4}));

        System.out.println("10. Find Missing Number: " +
            ArrayProblems.findMissingNumber(new int[]{3,0,1}));

        // HashMap Problems
        System.out.println("\n13. Group Anagrams: " +
            HashMapProblems.groupAnagrams(new String[]{"eat","tea","tan","ate","nat","bat"}));

        // LRU Cache
        HashMapProblems.LRUCache cache = new HashMapProblems.LRUCache(2);
        cache.put(1, 1);
        cache.put(2, 2);
        System.out.println("14. LRU Cache get(1): " + cache.get(1));
        cache.put(3, 3);
        System.out.println("LRU Cache get(2): " + cache.get(2));

        // Tree Problems
        TreeNode root = new TreeNode(3);
        root.left = new TreeNode(9);
        root.right = new TreeNode(20);
        root.right.left = new TreeNode(15);
        root.right.right = new TreeNode(7);

        System.out.println("\n16. Level Order: " + TreeProblems.levelOrder(root));
        System.out.println("17. Max Depth: " + TreeProblems.maxDepth(root));

        // Dynamic Programming
        System.out.println("\n21. Climbing Stairs (n=5): " +
            DynamicProgrammingProblems.climbStairs(5));

        System.out.println("22. Coin Change: " +
            DynamicProgrammingProblems.coinChange(new int[]{1,2,5}, 11));

        System.out.println("23. Longest Increasing Subsequence: " +
            DynamicProgrammingProblems.lengthOfLIS(new int[]{10,9,2,5,3,7,101,18}));

        System.out.println("24. House Robber: " +
            DynamicProgrammingProblems.rob(new int[]{2,7,9,3,1}));

        // Design Patterns
        System.out.println("\n28. Singleton: ");
        DesignPatterns.Singleton.INSTANCE.doSomething();

        System.out.println("\n29. Factory Pattern:");
        DesignPatterns.ShapeFactory factory = new DesignPatterns.ShapeFactory();
        DesignPatterns.Shape circle = factory.getShape("CIRCLE");
        circle.draw();

        // Java 8 Features
        System.out.println("\n32. Java 8 Streams:");
        JavaSpecificQuestions.streamExamples();

        System.out.println("\n=== END OF EXAMPLES ===");
    }
}

/**
 * ================================================================================================
 * ADDITIONAL INTERVIEW TIPS
 * ================================================================================================
 *
 * 1. TIME COMPLEXITY ANALYSIS:
 *    - Always analyze time and space complexity
 *    - Big O notation: O(1), O(log n), O(n), O(n log n), O(n²)
 *
 * 2. CODING BEST PRACTICES:
 *    - Write clean, readable code
 *    - Use meaningful variable names
 *    - Handle edge cases (null, empty, negative)
 *    - Add comments for complex logic
 *
 * 3. PROBLEM-SOLVING APPROACH:
 *    - Understand the problem
 *    - Ask clarifying questions
 *    - Discuss approach before coding
 *    - Test with examples
 *    - Optimize if needed
 *
 * 4. COMMON DATA STRUCTURES:
 *    - Array, ArrayList, LinkedList
 *    - HashMap, HashSet, TreeMap, TreeSet
 *    - Stack, Queue, PriorityQueue
 *    - StringBuilder
 *
 * 5. COMMON ALGORITHMS:
 *    - Two Pointers
 *    - Sliding Window
 *    - Binary Search
 *    - BFS/DFS
 *    - Dynamic Programming
 *    - Backtracking
 *
 * 6. JAVA SPECIFIC:
 *    - Autoboxing/Unboxing
 *    - Generics
 *    - Lambda Expressions
 *    - Stream API
 *    - Optional
 *    - CompletableFuture
 *
 * ================================================================================================
 */

