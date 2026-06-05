#!/usr/bin/env python3
"""
Python vs C++: Practical Examples for Direct Comparison
Run with: python3 python_cpp_practical_examples.py
"""

import time
import sys
import bisect
import heapq
from collections import defaultdict, deque, Counter
import array
import itertools
import functools

# ============================================================================
# SECTION 1: Built-in Types Comparison
# ============================================================================

def builtin_types_demo():
    print("\n=== Built-in Types Demo ===")

    # Python types (all objects, dynamic typing)
    python_bool = True
    python_int = 42  # Unlimited precision
    python_big_int = 12345678901234567890123456789012345678901234567890
    python_float = 3.141592653589793  # Always double precision
    python_string = "Hello, Python World!"  # Unicode by default

    print("Python Types and Characteristics:")
    print(f"bool: {python_bool} (type: {type(python_bool)})")
    print(f"int: {python_int} (type: {type(python_int)})")
    print(f"big int: {python_big_int}")
    print(f"float: {python_float} (type: {type(python_float)})")
    print(f"string: \"{python_string}\" (length: {len(python_string)})")

    # Memory usage
    print(f"int memory size: {sys.getsizeof(python_int)} bytes")
    print(f"string memory size: {sys.getsizeof(python_string)} bytes")

    # String operations (Unicode by default)
    unicode_string = "Hello, 世界! 🌍"
    print(f"Unicode string: \"{unicode_string}\" (length: {len(unicode_string)})")

    # Type information
    print(f"Max int size: {sys.maxsize}")
    print(f"Float info: {sys.float_info.dig} digits precision")

# ============================================================================
# SECTION 2: Container Demonstrations
# ============================================================================

def list_demo():
    print("\n=== List vs C++ Vector Demo ===")

    # Python list (dynamic array)
    python_list = [1, 2, 3, 4, 5]

    # Add elements
    python_list.append(6)
    python_list.insert(2, 10)

    print(f"Python list after modifications: {python_list}")
    print(f"List length: {len(python_list)}")

    # List comprehension (more Pythonic than loops)
    squares = [x**2 for x in python_list]
    print(f"Squares: {squares}")

    # List slicing (not available in C++ vectors)
    subset = python_list[1:4]
    print(f"Slice [1:4]: {subset}")

    # Negative indexing
    print(f"Last element: {python_list[-1]}")
    print(f"Second to last: {python_list[-2]}")

    # List operations
    another_list = [7, 8, 9]
    combined = python_list + another_list
    print(f"Combined lists: {combined}")

    # Memory efficiency consideration
    print(f"List memory size: {sys.getsizeof(python_list)} bytes")

    # Typed arrays for memory efficiency (closer to C++ vectors)
    typed_array = array.array('i', range(1000))  # 'i' for integers
    print(f"Typed array size: {sys.getsizeof(typed_array)} bytes")

def dict_demo():
    print("\n=== Dict vs C++ Map Demo ===")

    # Python dict (hash table, maintains insertion order since 3.7)
    python_dict = {
        "apple": 5,
        "banana": 3,
        "cherry": 8,
        "date": 2
    }

    print(f"Python dict: {python_dict}")

    # Access and modify
    python_dict["elderberry"] = 7
    python_dict.update({"fig": 4})

    # Check existence
    if "cherry" in python_dict:
        print(f"Cherry count: {python_dict['cherry']}")

    # Get with default value
    grape_count = python_dict.get("grape", 0)
    print(f"Grape count (with default): {grape_count}")

    # Dict comprehension
    high_counts = {k: v for k, v in python_dict.items() if v > 4}
    print(f"High counts: {high_counts}")

    # Advanced operations
    keys = list(python_dict.keys())
    values = list(python_dict.values())
    items = list(python_dict.items())
    print(f"Keys: {keys}")
    print(f"Values: {values}")

    # Performance measurement
    large_dict = {}
    start = time.time()
    for i in range(100000):
        large_dict[i] = i * 2
    end = time.time()
    print(f"Dict insertion time: {(end - start) * 1000:.2f} ms")

def set_demo():
    print("\n=== Set Operations Demo ===")

    set1 = {1, 3, 5, 7, 9}
    set2 = {2, 4, 6, 7, 8, 9}

    print(f"Set1: {set1}")
    print(f"Set2: {set2}")
    print(f"Intersection: {set1 & set2}")
    print(f"Union: {set1 | set2}")
    print(f"Difference (set1 - set2): {set1 - set2}")
    print(f"Symmetric difference: {set1 ^ set2}")

    # Set comprehension
    even_squares = {x**2 for x in range(10) if x % 2 == 0}
    print(f"Even squares: {even_squares}")

    # Membership testing (very fast)
    large_set = set(range(1000000))
    start = time.time()
    result = 999999 in large_set
    end = time.time()
    print(f"Set membership test time: {(end - start) * 1000000:.2f} μs")

def container_adapters_demo():
    print("\n=== Container Adapters Demo ===")

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
    python_queue = deque()
    python_queue.append(1)
    python_queue.append(2)
    python_queue.append(3)

    print("Queue (FIFO):", end=" ")
    while python_queue:
        print(python_queue.popleft(), end=" ")
    print()

    # Priority Queue using heapq (min heap)
    heap = []
    heapq.heappush(heap, 3)
    heapq.heappush(heap, 1)
    heapq.heappush(heap, 4)
    heapq.heappush(heap, 2)

    print("Priority Queue (min heap):", end=" ")
    while heap:
        print(heapq.heappop(heap), end=" ")
    print()

    # Max heap (negate values)
    max_heap = []
    for val in [3, 1, 4, 2]:
        heapq.heappush(max_heap, -val)

    print("Max heap:", end=" ")
    while max_heap:
        print(-heapq.heappop(max_heap), end=" ")
    print()

# ============================================================================
# SECTION 3: Algorithm Demonstrations
# ============================================================================

def algorithm_demo():
    print("\n=== Algorithm Demo ===")

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
    # Alternative: squares = list(map(lambda n: n**2, numbers))
    print(f"Squares: {squares}")

    # Sum
    total = sum(numbers)
    print(f"Sum: {total}")

    # Count with condition
    count_gt_5 = sum(1 for n in numbers if n > 5)
    # Alternative: count_gt_5 = len([n for n in numbers if n > 5])
    print(f"Count > 5: {count_gt_5}")

    # Min/Max
    print(f"Min: {min(numbers)}, Max: {max(numbers)}")

    # Partition (even numbers first)
    evens = [n for n in numbers if n % 2 == 0]
    odds = [n for n in numbers if n % 2 == 1]
    partitioned = evens + odds
    print(f"Partitioned (even first): {partitioned}")

    # All/Any
    all_positive = all(n > 0 for n in numbers)
    any_greater_8 = any(n > 8 for n in numbers)
    print(f"All positive: {all_positive}, Any > 8: {any_greater_8}")

def advanced_algorithm_demo():
    print("\n=== Advanced Algorithm Demo ===")

    words = ["apple", "banana", "cherry", "date", "elderberry"]

    # Sort by length
    by_length = sorted(words, key=len)
    print(f"Sorted by length: {by_length}")

    # Sort by multiple criteria (length, then alphabetically)
    by_criteria = sorted(words, key=lambda w: (len(w), w))
    print(f"Sorted by length then alphabetically: {by_criteria}")

    # Group by first letter
    grouped = defaultdict(list)
    for word in words:
        grouped[word[0]].append(word)

    print("Grouped by first letter:")
    for letter, word_list in grouped.items():
        print(f"  {letter}: {word_list}")

    # Using itertools.groupby (requires pre-sorted data)
    words_by_first = sorted(words, key=lambda w: w[0])
    grouped_iter = [(k, list(g)) for k, g in itertools.groupby(words_by_first, key=lambda w: w[0])]
    print(f"Grouped with itertools: {grouped_iter}")

    # Binary search
    sorted_numbers = [1, 3, 5, 7, 9, 11, 13, 15]
    pos = bisect.bisect_left(sorted_numbers, 7)
    found = pos < len(sorted_numbers) and sorted_numbers[pos] == 7
    print(f"Binary search for 7: {'found' if found else 'not found'}")

    # Lower bound
    pos = bisect.bisect_left(sorted_numbers, 8)
    print(f"Lower bound for 8 at position: {pos}")

    # Advanced itertools operations
    print("\nAdvanced itertools operations:")

    # Chain multiple iterables
    chained = list(itertools.chain([1, 2], [3, 4], [5, 6]))
    print(f"Chain: {chained}")

    # Product (Cartesian product)
    product = list(itertools.product([1, 2], ['a', 'b']))
    print(f"Product: {product}")

    # Combinations
    combinations = list(itertools.combinations(words[:3], 2))
    print(f"Combinations: {combinations}")

    # Permutations
    permutations = list(itertools.permutations([1, 2, 3], 2))
    print(f"Permutations: {permutations}")

def functional_programming_demo():
    print("\n=== Functional Programming Demo ===")

    numbers = [1, 2, 3, 4, 5]

    # Map, filter, reduce
    squared = list(map(lambda x: x**2, numbers))
    evens = list(filter(lambda x: x % 2 == 0, numbers))
    product = functools.reduce(lambda x, y: x * y, numbers)

    print(f"Original: {numbers}")
    print(f"Squared: {squared}")
    print(f"Evens: {evens}")
    print(f"Product: {product}")

    # Partial functions
    multiply_by_three = functools.partial(lambda x, y: x * y, 3)
    results = [multiply_by_three(n) for n in numbers]
    print(f"Multiply by 3: {results}")

    # Function composition
    def compose(f, g):
        return lambda x: f(g(x))

    add_one = lambda x: x + 1
    square = lambda x: x**2
    add_one_then_square = compose(square, add_one)

    result = add_one_then_square(4)  # (4 + 1)^2 = 25
    print(f"Compose add_one and square for 4: {result}")

# ============================================================================
# SECTION 4: Performance Comparison
# ============================================================================

def performance_comparison():
    print("\n=== Performance Comparison ===")

    SIZE = 1000000

    # List append performance
    start = time.time()
    python_list = []
    for i in range(SIZE):
        python_list.append(i)
    end = time.time()
    print(f"Python list append: {(end - start) * 1000:.2f} ms")

    # List comprehension (often faster)
    start = time.time()
    comp_list = [i for i in range(SIZE)]
    end = time.time()
    print(f"List comprehension: {(end - start) * 1000:.2f} ms")

    # Dict insertion
    start = time.time()
    python_dict = {}
    for i in range(SIZE // 100):
        python_dict[i] = i * 2
    end = time.time()
    print(f"Dict insertion: {(end - start) * 1000:.2f} ms")

    # Dict comprehension
    start = time.time()
    comp_dict = {i: i * 2 for i in range(SIZE // 100)}
    end = time.time()
    print(f"Dict comprehension: {(end - start) * 1000:.2f} ms")

    # Set operations
    large_set = set(range(SIZE // 10))
    start = time.time()
    result = (SIZE // 10 - 1) in large_set
    end = time.time()
    print(f"Set membership: {(end - start) * 1000000:.2f} μs")

    # Sorting performance
    unsorted_list = list(range(SIZE // 100, 0, -1))  # Reverse order
    start = time.time()
    sorted_list = sorted(unsorted_list)
    end = time.time()
    print(f"Sorting {len(unsorted_list)} elements: {(end - start) * 1000:.2f} ms")

def memory_usage_demo():
    print("\n=== Memory Usage Demo ===")

    # Regular list
    regular_list = list(range(1000))
    print(f"Regular list memory: {sys.getsizeof(regular_list)} bytes")

    # Typed array (more memory efficient)
    typed_array = array.array('i', range(1000))
    print(f"Typed array memory: {sys.getsizeof(typed_array)} bytes")

    # Generator (very memory efficient)
    generator = (i for i in range(1000))
    print(f"Generator memory: {sys.getsizeof(generator)} bytes")

    # String vs bytes
    text = "Hello, World!" * 100
    bytes_data = text.encode('utf-8')
    print(f"String memory: {sys.getsizeof(text)} bytes")
    print(f"Bytes memory: {sys.getsizeof(bytes_data)} bytes")

    # Tuple vs list (tuples are immutable and more memory efficient)
    tuple_data = tuple(range(1000))
    list_data = list(range(1000))
    print(f"Tuple memory: {sys.getsizeof(tuple_data)} bytes")
    print(f"List memory: {sys.getsizeof(list_data)} bytes")

# ============================================================================
# SECTION 5: Python-Specific Features
# ============================================================================

def python_specific_features():
    print("\n=== Python-Specific Features ===")

    # Multiple assignment
    a, b, c = 1, 2, 3
    print(f"Multiple assignment: a={a}, b={b}, c={c}")

    # Tuple unpacking
    coordinates = (10, 20)
    x, y = coordinates
    print(f"Tuple unpacking: x={x}, y={y}")

    # Dictionary unpacking
    config = {"host": "localhost", "port": 8080}
    print("Dictionary unpacking: {host}:{port}".format(**config))

    # List unpacking
    numbers = [1, 2, 3, 4, 5]
    first, *middle, last = numbers
    print(f"List unpacking: first={first}, middle={middle}, last={last}")

    # Enumerate and zip
    words = ["apple", "banana", "cherry"]
    for i, word in enumerate(words):
        print(f"  {i}: {word}")

    prices = [1.20, 0.80, 2.50]
    for word, price in zip(words, prices):
        print(f"  {word}: ${price}")

    # Dictionary iteration patterns
    fruit_counts = {"apple": 5, "banana": 3, "cherry": 8}

    print("Keys:", list(fruit_counts.keys()))
    print("Values:", list(fruit_counts.values()))
    print("Items:", list(fruit_counts.items()))

    # Conditional expressions (ternary operator)
    age = 25
    status = "adult" if age >= 18 else "minor"
    print(f"Status: {status}")

    # List/dict/set comprehensions with conditions
    numbers = range(10)
    evens = [n for n in numbers if n % 2 == 0]
    squares_dict = {n: n**2 for n in numbers if n % 2 == 1}
    even_set = {n for n in numbers if n % 2 == 0}

    print(f"Even numbers: {evens}")
    print(f"Odd squares dict: {squares_dict}")
    print(f"Even set: {even_set}")

def collections_module_demo():
    print("\n=== Collections Module Demo ===")

    # Counter - count occurrences
    text = "hello world"
    letter_counts = Counter(text)
    print(f"Letter counts: {letter_counts}")
    print(f"Most common: {letter_counts.most_common(3)}")

    # defaultdict - provides default values
    word_lengths = defaultdict(list)
    words = ["apple", "pie", "banana", "cherry", "date"]
    for word in words:
        word_lengths[len(word)].append(word)
    print(f"Words by length: {dict(word_lengths)}")

    # deque - double-ended queue
    dq = deque([1, 2, 3])
    dq.appendleft(0)
    dq.append(4)
    print(f"Deque: {list(dq)}")

    # Rotating deque
    dq.rotate(2)
    print(f"After rotate(2): {list(dq)}")

    # namedtuple - lightweight object
    from collections import namedtuple
    Point = namedtuple('Point', ['x', 'y'])
    p = Point(10, 20)
    print(f"Point: {p}, x={p.x}, y={p.y}")

# ============================================================================
# MAIN FUNCTION
# ============================================================================

def main():
    print("Python vs C++: Built-in Types, Containers, and Algorithms")
    print("==========================================================")

    builtin_types_demo()
    list_demo()
    dict_demo()
    set_demo()
    container_adapters_demo()
    algorithm_demo()
    advanced_algorithm_demo()
    functional_programming_demo()
    performance_comparison()
    memory_usage_demo()
    python_specific_features()
    collections_module_demo()

    print("\n=== Summary ===")
    print("Python advantages:")
    print("  - Simpler, more readable syntax")
    print("  - Dynamic typing and flexibility")
    print("  - Rich built-in operations and libraries")
    print("  - Automatic memory management")
    print("  - Interactive development and testing")
    print("  - Extensive standard library")
    print("\nC++ advantages:")
    print("  - Better performance and memory control")
    print("  - Compile-time optimizations")
    print("  - More precise type system")
    print("  - Lower-level control when needed")
    print("  - Deterministic resource management")

if __name__ == "__main__":
    main()
