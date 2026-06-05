# C++ vs Python: Built-in Types, STL Containers, and Algorithms Comparison

## 🎯 **Overview**

Comprehensive comparison between C++ and Python for built-in types, container classes, and algorithms. This guide helps developers understand the equivalents, performance characteristics, and usage patterns in both languages.

## 📊 **Built-in Types Comparison**

### **Numeric Types**

| C++ Type | Size | Python Type | Notes |
|----------|------|-------------|-------|
| `bool` | 1 byte | `bool` | C++: true/false, Python: True/False |
| `char` | 1 byte | N/A | Python uses strings instead |
| `int` | 4 bytes | `int` | Python int has unlimited precision |
| `long` | 8 bytes | `int` | Python int auto-extends |
| `long long` | 8 bytes | `int` | Python int handles large numbers |
| `float` | 4 bytes | N/A | Python doesn't have single precision |
| `double` | 8 bytes | `float` | Python float is double precision |
| `long double` | 16 bytes | `decimal.Decimal` | For high precision |

### **String Types**

| C++ Type | Python Type | Notes |
|----------|-------------|-------|
| `std::string` | `str` | C++: mutable, Python: immutable |
| `std::wstring` | `str` | Python str is Unicode by default |
| `std::string_view` | N/A | Python strings are already lightweight |
| `const char*` | `str` | C-style strings vs Python strings |

### **Example Code Comparison**

#### **C++ Built-in Types**
```cpp
#include <iostream>
#include <string>
#include <limits>

int main() {
    // Numeric types
    bool flag = true;
    int number = 42;
    long big_number = 1234567890L;
    double pi = 3.14159;

    // String types
    std::string text = "Hello, World!";
    const char* c_str = "C-style string";

    // Type limits
    std::cout << "Int max: " << std::numeric_limits<int>::max() << std::endl;
    std::cout << "Double precision: " << std::numeric_limits<double>::digits10 << std::endl;

    return 0;
}
```

#### **Python Built-in Types**
```python
# Numeric types

 = True
number = 42
big_number = 12345678901234567890  # Unlimited precision
pi = 3.14159

# String types
text = "Hello, World!"
unicode_text = "Hello, 世界!"  # Unicode by default

# Type information
import sys
print(f"Int max size: {sys.maxsize}")
print(f"Float precision: {sys.float_info.dig}")
```

## 🗂️ **STL Containers vs Python Collections**

### **Sequential Containers**

| C++ Container | Python Equivalent | Performance | Use Case |
|---------------|-------------------|-------------|-----------|
| `std::vector<T>` | `list` | C++: O(1) append*, Python: O(1) append | Dynamic arrays |
| `std::array<T,N>` | `tuple` (fixed) | C++: stack allocated, Python: heap | Fixed-size sequences |
| `std::deque<T>` | `collections.deque` | Both: O(1) front/back operations | Double-ended queues |
| `std::list<T>` | N/A (use deque) | C++: O(1) insert/delete anywhere | Doubly-linked lists |
| `std::forward_list<T>` | N/A | C++: memory efficient | Singly-linked lists |

### **Associative Containers**

| C++ Container | Python Equivalent | Performance | Use Case |
|---------------|-------------------|-------------|-----------|
| `std::map<K,V>` | `dict` | C++: O(log n), Python: O(1) average | Ordered key-value pairs |
| `std::unordered_map<K,V>` | `dict` | Both: O(1) average | Hash-based key-value |
| `std::set<T>` | `set` | C++: O(log n), Python: O(1) average | Unique elements |
| `std::unordered_set<T>` | `set` | Both: O(1) average | Hash-based unique elements |
| `std::multimap<K,V>` | `collections.defaultdict(list)` | Different approaches | Multiple values per key |
| `std::multiset<T>` | `collections.Counter` | Different implementations | Duplicate elements allowed |

### **Container Adapters**

| C++ Adapter | Python Equivalent | Use Case |
|-------------|-------------------|-----------|
| `std::stack<T>` | `list` (append/pop) | LIFO operations |
| `std::queue<T>` | `collections.deque` | FIFO operations |
| `std::priority_queue<T>` | `heapq` module | Priority-based operations |

## 📝 **Detailed Container Examples**

### **Vector vs List**

#### **C++ std::vector**
```cpp
#include <vector>
#include <iostream>

void vectorExample() {
    std::vector<int> vec = {1, 2, 3, 4, 5};

    // Add elements
    vec.push_back(6);
    vec.insert(vec.begin() + 2, 10);  // Insert at position 2

    // Access elements
    std::cout << "Element at index 2: " << vec[2] << std::endl;
    std::cout << "Front: " << vec.front() << ", Back: " << vec.back() << std::endl;

    // Iterate
    for (const auto& element : vec) {
        std::cout << element << " ";
    }

    // Size and capacity
    std::cout << "\nSize: " << vec.size() << ", Capacity: " << vec.capacity() << std::endl;

    // Reserve space
    vec.reserve(100);  // Pre-allocate capacity
}
```

#### **Python list**
```python
def list_example():
    lst = [1, 2, 3, 4, 5]

    # Add elements
    lst.append(6)
    lst.insert(2, 10)  # Insert at position 2

    # Access elements
    print(f"Element at index 2: {lst[2]}")
    print(f"Front: {lst[0]}, Back: {lst[-1]}")

    # Iterate
    for element in lst:
        print(element, end=" ")

    # Size (no capacity concept in Python)
    print(f"\nLength: {len(lst)}")

    # List comprehension (Pythonic)
    squares = [x**2 for x in lst]
    print(f"Squares: {squares}")
```

### **Map vs Dictionary**

#### **C++ std::map and std::unordered_map**
```cpp
#include <map>
#include <unordered_map>
#include <string>
#include <iostream>

void mapExample() {
    // Ordered map (red-black tree)
    std::map<std::string, int> ordered_map;
    ordered_map["apple"] = 5;
    ordered_map["banana"] = 3;
    ordered_map["cherry"] = 8;

    // Hash map
    std::unordered_map<std::string, int> hash_map;
    hash_map["apple"] = 5;
    hash_map["banana"] = 3;
    hash_map["cherry"] = 8;

    // Access and modify
    ordered_map["date"] = 12;
    hash_map.insert({"elderberry", 7});

    // Check existence
    if (ordered_map.find("apple") != ordered_map.end()) {
        std::cout << "Apple found in ordered map" << std::endl;
    }

    if (hash_map.count("banana") > 0) {
        std::cout << "Banana found in hash map" << std::endl;
    }

    // Iterate (ordered map maintains order)
    std::cout << "Ordered map: ";
    for (const auto& [key, value] : ordered_map) {
        std::cout << key << ":" << value << " ";
    }
    std::cout << std::endl;
}
```

#### **Python dict**
```python
def dict_example():
    # Python dict (hash table, insertion order preserved since 3.7)
    fruit_counts = {
        "apple": 5,
        "banana": 3,
        "cherry": 8
    }

    # Access and modify
    fruit_counts["date"] = 12
    fruit_counts.update({"elderberry": 7})

    # Check existence
    if "apple" in fruit_counts:
        print("Apple found in dictionary")

    # Get with default
    grape_count = fruit_counts.get("grape", 0)

    # Iterate
    print("Dictionary items:")
    for key, value in fruit_counts.items():
        print(f"{key}: {value}")

    # Dictionary comprehension
    squared_counts = {k: v**2 for k, v in fruit_counts.items() if v > 5}
    print(f"Squared counts (>5): {squared_counts}")

    # Advanced operations
    keys = list(fruit_counts.keys())
    values = list(fruit_counts.values())
    print(f"Keys: {keys}")
    print(f"Values: {values}")
```

## 🔧 **Algorithms Comparison**

### **STL Algorithms vs Python Built-ins**

| C++ Algorithm | Python Equivalent | Example |
|---------------|-------------------|---------|
| `std::find` | `in` operator | `found = item in container` |
| `std::sort` | `sorted()` / `list.sort()` | `sorted_list = sorted(lst)` |
| `std::binary_search` | `bisect` module | `import bisect; bisect.bisect_left()` |
| `std::accumulate` | `sum()` | `total = sum(numbers)` |
| `std::transform` | `map()` | `mapped = list(map(func, lst))` |
| `std::copy` | slice assignment | `dest[:] = source` |
| `std::reverse` | `reversed()` / `list.reverse()` | `reversed_list = list(reversed(lst))` |
| `std::min_element` | `min()` | `minimum = min(lst)` |
| `std::max_element` | `max()` | `maximum = max(lst)` |
| `std::count` | `list.count()` | `count = lst.count(item)` |
| `std::unique` | `set()` or manual | `unique_items = list(set(lst))` |

### **Algorithm Examples**

#### **C++ STL Algorithms**
```cpp
#include <algorithm>
#include <vector>
#include <numeric>
#include <iostream>
#include <functional>

void algorithmExample() {
    std::vector<int> numbers = {5, 2, 8, 1, 9, 3};

    // Sort
    std::sort(numbers.begin(), numbers.end());
    std::cout << "Sorted: ";
    for (int n : numbers) std::cout << n << " ";
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

    // Accumulate (sum)
    int sum = std::accumulate(numbers.begin(), numbers.end(), 0);
    std::cout << "Sum: " << sum << std::endl;

    // Count
    int count_gt_5 = std::count_if(numbers.begin(), numbers.end(),
                                   [](int n) { return n > 5; });
    std::cout << "Count > 5: " << count_gt_5 << std::endl;

    // Min/Max
    auto [min_it, max_it] = std::minmax_element(numbers.begin(), numbers.end());
    std::cout << "Min: " << *min_it << ", Max: " << *max_it << std::endl;

    // Binary search (requires sorted container)
    bool found = std::binary_search(numbers.begin(), numbers.end(), 5);
    std::cout << "Binary search for 5: " << (found ? "found" : "not found") << std::endl;

    // Remove duplicates (requires sorted container)
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
}
```

#### **Python Algorithms**
```python
def algorithm_example():
    numbers = [5, 2, 8, 1, 9, 3]

    # Sort
    sorted_numbers = sorted(numbers)
    print(f"Sorted: {sorted_numbers}")

    # Find
    if 8 in numbers:
        position = numbers.index(8)
        print(f"Found 8 at position: {position}")

    # Transform (square all elements)
    squares = [n**2 for n in numbers]  # List comprehension
    # or: squares = list(map(lambda n: n**2, numbers))
    print(f"Squares: {squares}")

    # Sum
    total = sum(numbers)
    print(f"Sum: {total}")

    # Count with condition
    count_gt_5 = sum(1 for n in numbers if n > 5)
    # or: count_gt_5 = len([n for n in numbers if n > 5])
    print(f"Count > 5: {count_gt_5}")

    # Min/Max
    minimum = min(numbers)
    maximum = max(numbers)
    print(f"Min: {minimum}, Max: {maximum}")

    # Binary search
    import bisect
    sorted_nums = sorted(numbers)
    pos = bisect.bisect_left(sorted_nums, 5)
    found = pos < len(sorted_nums) and sorted_nums[pos] == 5
    print(f"Binary search for 5: {'found' if found else 'not found'}")

    # Remove duplicates
    unique_numbers = list(set(numbers))  # Unordered
    # For ordered: unique_numbers = list(dict.fromkeys(numbers))
    print(f"Unique: {unique_numbers}")
```

## 🔄 **Iterators vs Python Iteration**

### **C++ Iterators**
```cpp
#include <vector>
#include <iterator>
#include <iostream>

void iteratorExample() {
    std::vector<int> vec = {1, 2, 3, 4, 5};

    // Forward iterator
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Reverse iterator
    for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Random access
    auto it = vec.begin();
    std::advance(it, 2);  // Move iterator by 2 positions
    std::cout << "Element at position 2: " << *it << std::endl;

    // Distance
    auto distance = std::distance(vec.begin(), vec.end());
    std::cout << "Distance: " << distance << std::endl;
}
```

### **Python Iteration**
```python
def iteration_example():
    lst = [1, 2, 3, 4, 5]

    # Forward iteration
    for item in lst:
        print(item, end=" ")
    print()

    # Reverse iteration
    for item in reversed(lst):
        print(item, end=" ")
    print()

    # Index-based iteration
    for i, item in enumerate(lst):
        print(f"Index {i}: {item}")

    # Slice iteration
    for item in lst[1:4]:  # Elements 1, 2, 3
        print(item, end=" ")
    print()

    # Generator expression (memory efficient)
    squares_gen = (x**2 for x in lst)
    for square in squares_gen:
        print(square, end=" ")
    print()
```

## ⚡ **Performance Characteristics**

### **Time Complexity Comparison**

| Operation | C++ vector | Python list | C++ map | Python dict | C++ set | Python set |
|-----------|------------|-------------|---------|-------------|---------|------------|
| Access by index | O(1) | O(1) | N/A | N/A | N/A | N/A |
| Insert at end | O(1)* | O(1)* | N/A | N/A | N/A | N/A |
| Insert at beginning | O(n) | O(n) | N/A | N/A | N/A | N/A |
| Insert at middle | O(n) | O(n) | N/A | N/A | N/A | N/A |
| Delete at end | O(1) | O(1) | N/A | N/A | N/A | N/A |
| Search | O(n) | O(n) | O(log n) | O(1)* | O(log n) | O(1)* |
| Insert | O(1)* | O(1)* | O(log n) | O(1)* | O(log n) | O(1)* |
| Delete | O(n) | O(n) | O(log n) | O(1)* | O(log n) | O(1)* |

*Amortized time complexity

### **Memory Usage**

#### **C++ Memory Management**
```cpp
#include <vector>
#include <memory>

void memoryExample() {
    // Stack allocation
    std::vector<int> stack_vec(1000);  // Fixed size on stack

    // Heap allocation
    auto heap_vec = std::make_unique<std::vector<int>>(1000);

    // Memory efficiency
    std::vector<int> vec;
    vec.reserve(1000);  // Pre-allocate to avoid reallocations

    // Shrink to fit
    vec.shrink_to_fit();  // Release unused capacity

    // Memory layout - contiguous
    int* data_ptr = vec.data();  // Direct access to underlying array
}
```

#### **Python Memory Management**
```python
def memory_example():
    import sys

    # All objects are heap-allocated in Python
    lst = [i for i in range(1000)]

    # Memory usage
    print(f"List size in bytes: {sys.getsizeof(lst)}")

    # Memory-efficient alternatives
    import array
    arr = array.array('i', range(1000))  # Typed array
    print(f"Array size in bytes: {sys.getsizeof(arr)}")

    # Generator for memory efficiency
    gen = (i for i in range(1000))
    print(f"Generator size in bytes: {sys.getsizeof(gen)}")
```

## 🎯 **Best Practices**

### **C++ Best Practices**
```cpp
#include <vector>
#include <algorithm>
#include <memory>

// 1. Use containers instead of raw arrays
void goodPractice() {
    std::vector<int> vec;  // Good
    // int* arr = new int[100];  // Avoid

    // 2. Use algorithms instead of manual loops
    std::vector<int> numbers = {1, 2, 3, 4, 5};

    // Good: Use STL algorithm
    auto it = std::find(numbers.begin(), numbers.end(), 3);

    // 3. Reserve capacity when size is known
    std::vector<int> large_vec;
    large_vec.reserve(10000);

    // 4. Use emplace for in-place construction
    std::vector<std::pair<int, std::string>> pairs;
    pairs.emplace_back(1, "one");  // Better than push_back(std::make_pair(1, "one"))

    // 5. Use range-based for loops
    for (const auto& number : numbers) {
        // Process number
    }
}
```

### **Python Best Practices**
```python
def good_practice():
    # 1. Use list comprehensions when appropriate
    numbers = [1, 2, 3, 4, 5]

    # Good: List comprehension
    squares = [x**2 for x in numbers]

    # 2. Use built-in functions
    total = sum(numbers)  # Better than manual loop
    maximum = max(numbers)

    # 3. Use generators for large datasets
    def fibonacci_gen():
        a, b = 0, 1
        while True:
            yield a
            a, b = b, a + b

    # 4. Use collections module for specialized containers
    from collections import defaultdict, Counter, deque

    dd = defaultdict(list)
    counter = Counter(numbers)
    queue = deque(numbers)

    # 5. Use enumerate instead of range(len())
    for i, value in enumerate(numbers):
        print(f"Index {i}: {value}")
```

## 🔧 **Advanced Features**

### **C++ Advanced Container Features**
```cpp
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <functional>

void advancedFeatures() {
    // Custom comparators
    std::set<int, std::greater<int>> desc_set;  // Descending order

    // Custom hash function for unordered containers
    struct CustomHash {
        std::size_t operator()(const std::pair<int, int>& p) const {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
        }
    };
    std::unordered_set<std::pair<int, int>, CustomHash> custom_set;

    // Lambda expressions with algorithms
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::transform(numbers.begin(), numbers.end(), numbers.begin(),
                   [](int n) { return n * 2; });

    // Parallel algorithms (C++17)
    std::sort(std::execution::par, numbers.begin(), numbers.end());
}
```

### **Python Advanced Features**
```python
def advanced_features():
    # Custom sorting
    words = ["apple", "pie", "banana"]
    sorted_by_length = sorted(words, key=len)

    # Multiple criteria sorting
    students = [("Alice", 85), ("Bob", 90), ("Charlie", 85)]
    sorted_students = sorted(students, key=lambda x: (-x[1], x[0]))  # By grade desc, then name asc

    # itertools for advanced iteration
    import itertools

    # Chain multiple iterables
    chained = list(itertools.chain([1, 2], [3, 4], [5, 6]))

    # Product (Cartesian product)
    product = list(itertools.product([1, 2], ['a', 'b']))

    # Groupby
    data = [1, 1, 2, 2, 2, 3, 3]
    grouped = [(k, list(g)) for k, g in itertools.groupby(data)]

    # functools for functional programming
    import functools

    # Reduce
    numbers = [1, 2, 3, 4, 5]
    product = functools.reduce(lambda x, y: x * y, numbers)

    # Partial functions
    multiply_by_two = functools.partial(lambda x, y: x * y, 2)
    result = multiply_by_two(5)  # Returns 10
```

## 📊 **Performance Benchmarking Example**

### **C++ Performance Test**
```cpp
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>

void performanceTest() {
    const size_t SIZE = 1000000;
    std::vector<int> vec(SIZE);

    // Fill with random data
    std::generate(vec.begin(), vec.end(), []() { return rand() % 1000; });

    // Measure sort performance
    auto start = std::chrono::high_resolution_clock::now();
    std::sort(vec.begin(), vec.end());
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "C++ sort time: " << duration.count() << " ms" << std::endl;

    // Measure search performance
    start = std::chrono::high_resolution_clock::now();
    auto it = std::binary_search(vec.begin(), vec.end(), 500);
    end = std::chrono::high_resolution_clock::now();

    auto search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "C++ binary search time: " << search_duration.count() << " μs" << std::endl;
}
```

### **Python Performance Test**
```python
import time
import random
import bisect

def performance_test():
    SIZE = 1000000
    lst = [random.randint(0, 999) for _ in range(SIZE)]

    # Measure sort performance
    start = time.time()
    sorted_list = sorted(lst)
    end = time.time()

    print(f"Python sort time: {(end - start) * 1000:.2f} ms")

    # Measure search performance
    start = time.time()
    pos = bisect.bisect_left(sorted_list, 500)
    found = pos < len(sorted_list) and sorted_list[pos] == 500
    end = time.time()

    print(f"Python binary search time: {(end - start) * 1000000:.2f} μs")

if __name__ == "__main__":
    performance_test()
```

## 🎓 **Summary and Recommendations**

### **When to Use C++**
- **Performance-critical applications** (HFT, game engines, embedded systems)
- **Memory-constrained environments**
- **Need for precise memory control**
- **Real-time systems** with deterministic behavior
- **Large-scale systems** where performance matters

### **When to Use Python**
- **Rapid prototyping and development**
- **Data analysis and machine learning**
- **Scripting and automation**
- **Web development** (with frameworks)
- **Scientific computing** (with NumPy/SciPy)

### **Key Takeaways**

1. **C++ provides better performance** but requires more careful memory management
2. **Python offers better productivity** with simpler syntax and rich libraries
3. **C++ containers are more memory-efficient** for large datasets
4. **Python has more expressive syntax** for common operations
5. **Both languages have excellent algorithm libraries** suitable for different use cases

### **Hybrid Approach**
Many production systems use both:
- **C++ for performance-critical components** (core algorithms, real-time processing)
- **Python for scripting, configuration, and high-level logic**
- **Pybind11 or Cython** for seamless integration

This comparison provides a foundation for choosing the right tool for specific tasks and understanding the trade-offs between performance and development productivity.
