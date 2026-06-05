// C++ vs Python: Practical Examples with Working Code
// Compile with: g++ -std=c++17 -O2 cpp_python_practical_examples.cpp -o examples

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <string>
#include <chrono>
#include <memory>
#include <functional>
#include <deque>
#include <queue>
#include <stack>
#include <array>
#include <list>
#include <iterator>

// ============================================================================
// SECTION 1: Built-in Types Comparison
// ============================================================================

void builtinTypesDemo() {
    std::cout << "\n=== Built-in Types Demo ===" << std::endl;

    // C++ numeric types with explicit sizes
    bool cpp_bool = true;
    char cpp_char = 'A';
    int cpp_int = 42;
    long cpp_long = 1234567890L;
    long long cpp_long_long = 12345678901234567890LL;
    float cpp_float = 3.14f;
    double cpp_double = 3.141592653589793;

    std::cout << "C++ Types and Sizes:" << std::endl;
    std::cout << "bool: " << cpp_bool << " (size: " << sizeof(cpp_bool) << " bytes)" << std::endl;
    std::cout << "char: " << cpp_char << " (size: " << sizeof(cpp_char) << " bytes)" << std::endl;
    std::cout << "int: " << cpp_int << " (size: " << sizeof(cpp_int) << " bytes)" << std::endl;
    std::cout << "long: " << cpp_long << " (size: " << sizeof(cpp_long) << " bytes)" << std::endl;
    std::cout << "long long: " << cpp_long_long << " (size: " << sizeof(cpp_long_long) << " bytes)" << std::endl;
    std::cout << "float: " << cpp_float << " (size: " << sizeof(cpp_float) << " bytes)" << std::endl;
    std::cout << "double: " << cpp_double << " (size: " << sizeof(cpp_double) << " bytes)" << std::endl;

    // String types
    std::string cpp_string = "Hello, C++ World!";
    const char* c_string = "C-style string";

    std::cout << "string: \"" << cpp_string << "\" (size: " << cpp_string.size() << " chars)" << std::endl;
    std::cout << "const char*: \"" << c_string << "\"" << std::endl;

    // Python equivalent (conceptual - would be in Python file):
    /*
    Python equivalent:
    python_bool = True
    python_int = 42  # Unlimited precision
    python_float = 3.141592653589793  # Always double precision
    python_string = "Hello, Python World!"  # Unicode by default

    # Python automatically handles large numbers
    big_number = 12345678901234567890123456789012345678901234567890
    */
}

// ============================================================================
// SECTION 2: Container Demonstrations
// ============================================================================

void vectorVsListDemo() {
    std::cout << "\n=== Vector vs Python List Demo ===" << std::endl;

    // C++ vector
    std::vector<int> cpp_vector = {1, 2, 3, 4, 5};

    // Add elements
    cpp_vector.push_back(6);
    cpp_vector.insert(cpp_vector.begin() + 2, 10);

    std::cout << "C++ vector after modifications: ";
    for (const auto& elem : cpp_vector) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;

    // Vector-specific operations
    std::cout << "Vector size: " << cpp_vector.size() << std::endl;
    std::cout << "Vector capacity: " << cpp_vector.capacity() << std::endl;

    // Reserve space (Python lists don't have this concept)
    cpp_vector.reserve(100);
    std::cout << "After reserve(100), capacity: " << cpp_vector.capacity() << std::endl;

    // Direct memory access (Python lists don't allow this)
    int* data_ptr = cpp_vector.data();
    std::cout << "Direct memory access, first element: " << *data_ptr << std::endl;

    /*
    Python equivalent:
    python_list = [1, 2, 3, 4, 5]

    # Add elements
    python_list.append(6)
    python_list.insert(2, 10)

    print(f"Python list after modifications: {python_list}")
    print(f"List length: {len(python_list)}")

    # List comprehension (more Pythonic)
    squares = [x**2 for x in python_list]
    print(f"Squares: {squares}")
    */
}

void mapVsDictDemo() {
    std::cout << "\n=== Map vs Python Dict Demo ===" << std::endl;

    // C++ ordered map (red-black tree)
    std::map<std::string, int> ordered_map;
    ordered_map["apple"] = 5;
    ordered_map["banana"] = 3;
    ordered_map["cherry"] = 8;
    ordered_map["date"] = 2;

    std::cout << "C++ std::map (ordered): ";
    for (const auto& [key, value] : ordered_map) {
        std::cout << key << ":" << value << " ";
    }
    std::cout << std::endl;

    // C++ hash map
    std::unordered_map<std::string, int> hash_map;
    hash_map["apple"] = 5;
    hash_map["banana"] = 3;
    hash_map["cherry"] = 8;
    hash_map["date"] = 2;

    std::cout << "C++ std::unordered_map (hash): ";
    for (const auto& [key, value] : hash_map) {
        std::cout << key << ":" << value << " ";
    }
    std::cout << std::endl;

    // Performance comparison: find operation
    auto start = std::chrono::high_resolution_clock::now();
    auto it = ordered_map.find("cherry");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ordered = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    auto hash_it = hash_map.find("cherry");
    end = std::chrono::high_resolution_clock::now();
    auto duration_hash = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    std::cout << "Find performance - Ordered map: " << duration_ordered.count() << "ns, ";
    std::cout << "Hash map: " << duration_hash.count() << "ns" << std::endl;

    /*
    Python equivalent:
    # Python dict (hash table, maintains insertion order since 3.7)
    python_dict = {
        "apple": 5,
        "banana": 3,
        "cherry": 8,
        "date": 2
    }

    print(f"Python dict: {python_dict}")

    # Check existence
    if "cherry" in python_dict:
        print(f"Cherry count: {python_dict['cherry']}")

    # Dict comprehension
    high_counts = {k: v for k, v in python_dict.items() if v > 4}
    print(f"High counts: {high_counts}")
    */
}

void setDemo() {
    std::cout << "\n=== Set Operations Demo ===" << std::endl;

    // C++ ordered set
    std::set<int> set1 = {1, 3, 5, 7, 9};
    std::set<int> set2 = {2, 4, 6, 7, 8, 9};

    // Set operations
    std::set<int> intersection;
    std::set_intersection(set1.begin(), set1.end(),
                         set2.begin(), set2.end(),
                         std::inserter(intersection, intersection.begin()));

    std::set<int> union_set;
    std::set_union(set1.begin(), set1.end(),
                   set2.begin(), set2.end(),
                   std::inserter(union_set, union_set.begin()));

    std::cout << "Set1: ";
    for (int x : set1) std::cout << x << " ";
    std::cout << std::endl;

    std::cout << "Set2: ";
    for (int x : set2) std::cout << x << " ";
    std::cout << std::endl;

    std::cout << "Intersection: ";
    for (int x : intersection) std::cout << x << " ";
    std::cout << std::endl;

    std::cout << "Union: ";
    for (int x : union_set) std::cout << x << " ";
    std::cout << std::endl;

    /*
    Python equivalent:
    set1 = {1, 3, 5, 7, 9}
    set2 = {2, 4, 6, 7, 8, 9}

    print(f"Set1: {set1}")
    print(f"Set2: {set2}")
    print(f"Intersection: {set1 & set2}")
    print(f"Union: {set1 | set2}")
    print(f"Difference: {set1 - set2}")
    print(f"Symmetric difference: {set1 ^ set2}")
    */
}

void containerAdaptersDemo() {
    std::cout << "\n=== Container Adapters Demo ===" << std::endl;

    // Stack (LIFO)
    std::stack<int> cpp_stack;
    cpp_stack.push(1);
    cpp_stack.push(2);
    cpp_stack.push(3);

    std::cout << "Stack (LIFO): ";
    while (!cpp_stack.empty()) {
        std::cout << cpp_stack.top() << " ";
        cpp_stack.pop();
    }
    std::cout << std::endl;

    // Queue (FIFO)
    std::queue<int> cpp_queue;
    cpp_queue.push(1);
    cpp_queue.push(2);
    cpp_queue.push(3);

    std::cout << "Queue (FIFO): ";
    while (!cpp_queue.empty()) {
        std::cout << cpp_queue.front() << " ";
        cpp_queue.pop();
    }
    std::cout << std::endl;

    // Priority Queue (heap)
    std::priority_queue<int> pq;
    pq.push(3);
    pq.push(1);
    pq.push(4);
    pq.push(2);

    std::cout << "Priority Queue (max heap): ";
    while (!pq.empty()) {
        std::cout << pq.top() << " ";
        pq.pop();
    }
    std::cout << std::endl;

    /*
    Python equivalent:
    # Stack using list
    python_stack = []
    python_stack.append(1)
    python_stack.append(2)
    python_stack.append(3)

    print("Stack (LIFO):", end=" ")
    while python_stack:
        print(python_stack.pop(), end=" ")
    print()

    # Queue using collections.deque
    from collections import deque
    python_queue = deque()
    python_queue.append(1)
    python_queue.append(2)
    python_queue.append(3)

    print("Queue (FIFO):", end=" ")
    while python_queue:
        print(python_queue.popleft(), end=" ")
    print()

    # Priority Queue using heapq
    import heapq
    heap = []
    heapq.heappush(heap, 3)
    heapq.heappush(heap, 1)
    heapq.heappush(heap, 4)
    heapq.heappush(heap, 2)

    print("Priority Queue (min heap):", end=" ")
    while heap:
        print(heapq.heappop(heap), end=" ")
    print()
    */
}

// ============================================================================
// SECTION 3: Algorithm Demonstrations
// ============================================================================

void algorithmDemo() {
    std::cout << "\n=== Algorithm Demo ===" << std::endl;

    std::vector<int> numbers = {5, 2, 8, 1, 9, 3, 7, 4, 6};

    std::cout << "Original: ";
    for (int n : numbers) std::cout << n << " ";
    std::cout << std::endl;

    // Sort
    std::vector<int> sorted_nums = numbers;
    std::sort(sorted_nums.begin(), sorted_nums.end());
    std::cout << "Sorted: ";
    for (int n : sorted_nums) std::cout << n << " ";
    std::cout << std::endl;

    // Find
    auto it = std::find(numbers.begin(), numbers.end(), 8);
    if (it != numbers.end()) {
        std::cout << "Found 8 at position: " << (it - numbers.begin()) << std::endl;
    }

    // Transform (square all elements)
    std::vector<int> squares(numbers.size());
    std::transform(numbers.begin(), numbers.end(), squares.begin(),
                   [](int n) { return n * n; });
    std::cout << "Squares: ";
    for (int n : squares) std::cout << n << " ";
    std::cout << std::endl;

    // Accumulate (sum)
    int sum = std::accumulate(numbers.begin(), numbers.end(), 0);
    std::cout << "Sum: " << sum << std::endl;

    // Count with condition
    int count_gt_5 = std::count_if(numbers.begin(), numbers.end(),
                                   [](int n) { return n > 5; });
    std::cout << "Count > 5: " << count_gt_5 << std::endl;

    // Min/Max
    auto [min_it, max_it] = std::minmax_element(numbers.begin(), numbers.end());
    std::cout << "Min: " << *min_it << ", Max: " << *max_it << std::endl;

    // Partition
    std::vector<int> partition_nums = numbers;
    auto partition_it = std::partition(partition_nums.begin(), partition_nums.end(),
                                      [](int n) { return n % 2 == 0; });
    std::cout << "Partitioned (even first): ";
    for (int n : partition_nums) std::cout << n << " ";
    std::cout << std::endl;

    /*
    Python equivalent:
    numbers = [5, 2, 8, 1, 9, 3, 7, 4, 6]

    print(f"Original: {numbers}")

    # Sort
    sorted_nums = sorted(numbers)
    print(f"Sorted: {sorted_nums}")

    # Find
    if 8 in numbers:
        position = numbers.index(8)
        print(f"Found 8 at position: {position}")

    # Transform (square all elements)
    squares = [n**2 for n in numbers]
    print(f"Squares: {squares}")

    # Sum
    total = sum(numbers)
    print(f"Sum: {total}")

    # Count with condition
    count_gt_5 = sum(1 for n in numbers if n > 5)
    print(f"Count > 5: {count_gt_5}")

    # Min/Max
    print(f"Min: {min(numbers)}, Max: {max(numbers)}")

    # Partition
    evens = [n for n in numbers if n % 2 == 0]
    odds = [n for n in numbers if n % 2 == 1]
    partitioned = evens + odds
    print(f"Partitioned (even first): {partitioned}")
    */
}

void advancedAlgorithmDemo() {
    std::cout << "\n=== Advanced Algorithm Demo ===" << std::endl;

    std::vector<std::string> words = {"apple", "banana", "cherry", "date", "elderberry"};

    // Sort by length
    std::vector<std::string> by_length = words;
    std::sort(by_length.begin(), by_length.end(),
              [](const std::string& a, const std::string& b) {
                  return a.length() < b.length();
              });

    std::cout << "Sorted by length: ";
    for (const auto& word : by_length) std::cout << word << " ";
    std::cout << std::endl;

    // Group by first letter (conceptual)
    std::map<char, std::vector<std::string>> grouped;
    for (const auto& word : words) {
        grouped[word[0]].push_back(word);
    }

    std::cout << "Grouped by first letter:" << std::endl;
    for (const auto& [letter, word_list] : grouped) {
        std::cout << letter << ": ";
        for (const auto& word : word_list) std::cout << word << " ";
        std::cout << std::endl;
    }

    // Binary search (requires sorted container)
    std::vector<int> sorted_numbers = {1, 3, 5, 7, 9, 11, 13, 15};
    bool found = std::binary_search(sorted_numbers.begin(), sorted_numbers.end(), 7);
    std::cout << "Binary search for 7: " << (found ? "found" : "not found") << std::endl;

    // Lower bound (first position where element could be inserted)
    auto lb = std::lower_bound(sorted_numbers.begin(), sorted_numbers.end(), 8);
    std::cout << "Lower bound for 8 at position: " << (lb - sorted_numbers.begin()) << std::endl;

    /*
    Python equivalent:
    words = ["apple", "banana", "cherry", "date", "elderberry"]

    # Sort by length
    by_length = sorted(words, key=len)
    print(f"Sorted by length: {by_length}")

    # Group by first letter
    from collections import defaultdict
    grouped = defaultdict(list)
    for word in words:
        grouped[word[0]].append(word)

    print("Grouped by first letter:")
    for letter, word_list in grouped.items():
        print(f"{letter}: {word_list}")

    # Binary search
    import bisect
    sorted_numbers = [1, 3, 5, 7, 9, 11, 13, 15]
    pos = bisect.bisect_left(sorted_numbers, 7)
    found = pos < len(sorted_numbers) and sorted_numbers[pos] == 7
    print(f"Binary search for 7: {'found' if found else 'not found'}")

    # Lower bound
    pos = bisect.bisect_left(sorted_numbers, 8)
    print(f"Lower bound for 8 at position: {pos}")
    */
}

// ============================================================================
// SECTION 4: Performance Comparison
// ============================================================================

void performanceComparison() {
    std::cout << "\n=== Performance Comparison ===" << std::endl;

    const size_t SIZE = 1000000;

    // Vector vs list performance
    {
        std::vector<int> vec;
        vec.reserve(SIZE);  // Pre-allocate to avoid reallocations

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < SIZE; ++i) {
            vec.push_back(i);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Vector push_back (reserved): " << duration.count() << " ms" << std::endl;
    }

    {
        std::list<int> lst;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < SIZE; ++i) {
            lst.push_back(i);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "List push_back: " << duration.count() << " ms" << std::endl;
    }

    // Map vs unordered_map performance
    {
        std::map<int, int> ordered_map;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < SIZE / 100; ++i) {  // Smaller size for map test
            ordered_map[i] = i * 2;
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Map insertion: " << duration.count() << " ms" << std::endl;
    }

    {
        std::unordered_map<int, int> hash_map;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < SIZE / 100; ++i) {  // Smaller size for map test
            hash_map[i] = i * 2;
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Unordered_map insertion: " << duration.count() << " ms" << std::endl;
    }

    /*
    Python performance test would be:
    import time

    SIZE = 1000000

    # List append
    start = time.time()
    python_list = []
    for i in range(SIZE):
        python_list.append(i)
    end = time.time()
    print(f"Python list append: {(end - start) * 1000:.2f} ms")

    # Dict insertion
    start = time.time()
    python_dict = {}
    for i in range(SIZE // 100):
        python_dict[i] = i * 2
    end = time.time()
    print(f"Python dict insertion: {(end - start) * 1000:.2f} ms")
    */
}

// ============================================================================
// SECTION 5: Memory Management Comparison
// ============================================================================

void memoryManagementDemo() {
    std::cout << "\n=== Memory Management Demo ===" << std::endl;

    // Stack allocation (C++ only)
    {
        std::array<int, 1000> stack_array;  // Allocated on stack
        std::cout << "Stack array size: " << stack_array.size() << std::endl;
        // Automatically destroyed when leaving scope
    }

    // Heap allocation with RAII
    {
        auto heap_vector = std::make_unique<std::vector<int>>(1000);
        std::cout << "Heap vector size: " << heap_vector->size() << std::endl;
        // Automatically destroyed by unique_ptr
    }

    // Manual memory management (avoid in modern C++)
    {
        int* raw_ptr = new int[1000];
        std::cout << "Raw pointer allocated for 1000 ints" << std::endl;
        delete[] raw_ptr;  // Must manually delete
        std::cout << "Raw pointer deleted" << std::endl;
    }

    // Memory pool example
    {
        std::vector<int> large_vector;
        large_vector.reserve(10000);  // Pre-allocate capacity

        std::cout << "Vector reserved capacity: " << large_vector.capacity() << std::endl;
        std::cout << "Vector actual size: " << large_vector.size() << std::endl;

        // Fill vector
        for (int i = 0; i < 5000; ++i) {
            large_vector.push_back(i);
        }

        std::cout << "After filling, size: " << large_vector.size()
                  << ", capacity: " << large_vector.capacity() << std::endl;

        // Shrink to fit
        large_vector.shrink_to_fit();
        std::cout << "After shrink_to_fit, capacity: " << large_vector.capacity() << std::endl;
    }

    /*
    Python memory management:
    - All objects are heap-allocated
    - Automatic garbage collection
    - Reference counting with cycle detection
    - No manual memory management needed

    # Python example
    import sys

    # All objects on heap
    python_list = [i for i in range(1000)]
    print(f"List memory size: {sys.getsizeof(python_list)} bytes")

    # No explicit memory management
    large_list = list(range(10000))
    # Automatically garbage collected when out of scope
    */
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    std::cout << "C++ vs Python: Built-in Types, Containers, and Algorithms" << std::endl;
    std::cout << "==========================================================" << std::endl;

    builtinTypesDemo();
    vectorVsListDemo();
    mapVsDictDemo();
    setDemo();
    containerAdaptersDemo();
    algorithmDemo();
    advancedAlgorithmDemo();
    performanceComparison();
    memoryManagementDemo();

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "C++ provides:" << std::endl;
    std::cout << "  - Better performance and memory control" << std::endl;
    std::cout << "  - More container types and precise semantics" << std::endl;
    std::cout << "  - Compile-time optimizations" << std::endl;
    std::cout << "  - Manual memory management when needed" << std::endl;
    std::cout << "\nPython provides:" << std::endl;
    std::cout << "  - Simpler syntax and faster development" << std::endl;
    std::cout << "  - Automatic memory management" << std::endl;
    std::cout << "  - Rich built-in operations and libraries" << std::endl;
    std::cout << "  - Dynamic typing and flexibility" << std::endl;

    return 0;
}
