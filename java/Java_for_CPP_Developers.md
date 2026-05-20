# Java for C++ Developers - Quick Learning Guide

## Table of Contents
1. [Key Conceptual Differences](#key-conceptual-differences)
2. [Memory Management](#memory-management)
3. [Type System](#type-system)
4. [Object-Oriented Features](#object-oriented-features)
5. [Exception Handling](#exception-handling)
6. [Generics vs Templates](#generics-vs-templates)
7. [Concurrency](#concurrency)
8. [Collections Framework](#collections-framework)

## Key Conceptual Differences

### What Java DOESN'T Have (that C++ has):
- **No Pointers**: Everything is reference-based (except primitives)
- **No Multiple Inheritance**: Only single inheritance + interfaces
- **No Operator Overloading**: Except for String (+) and some built-ins
- **No Templates**: Uses type erasure-based Generics instead
- **No Manual Memory Management**: Garbage collected
- **No Destructors**: Uses finalize() (deprecated) or try-with-resources
- **No Header Files**: Everything in .java files
- **No Preprocessor**: No #define, #include, etc.
- **No Function Pointers**: Uses functional interfaces/lambdas instead
- **No const**: Uses final keyword with different semantics
- **No unsigned types**: All integer types are signed

### What Java HAS that C++ doesn't (or does differently):
- **Automatic Memory Management**: Garbage collection
- **Reflection**: Runtime type information and manipulation
- **Bytecode Compilation**: Write once, run anywhere (WORA)
- **Built-in Multithreading**: Thread class, synchronized keyword
- **Package System**: Namespace-like but more structured
- **Interfaces**: Multiple interface implementation
- **Native String Support**: String is a first-class type
- **Checked Exceptions**: Compile-time exception handling requirements

## Memory Management

### C++ vs Java Memory Model

```cpp
// C++ - Manual memory management
class Person {
    string* name;
public:
    Person() : name(new string("John")) {}
    ~Person() { delete name; }  // Manual cleanup
};

Person* p = new Person();
delete p;  // Manual deletion
```

```java
// Java - Automatic memory management
class Person {
    private String name;
    
    public Person() {
        name = new String("John");
    }
    // No destructor needed - GC handles cleanup
}

Person p = new Person();  // No manual deletion needed
```

### Key Points:
- **Stack**: Primitives and references (like C++ pointers but safer)
- **Heap**: All objects allocated here, managed by GC
- **No memory leaks**: GC prevents most memory leaks
- **Can have memory bloat**: If references are held unnecessarily

## Type System

### Primitive Types vs Object Types

```java
// Primitive types (similar to C++ built-ins)
int i = 10;           // 32-bit signed integer
long l = 10L;         // 64-bit signed integer  
float f = 10.5f;      // 32-bit floating point
double d = 10.5;      // 64-bit floating point
boolean b = true;     // true/false (not 0/1 like C++)
char c = 'A';         // 16-bit Unicode character
byte by = 127;        // 8-bit signed integer
short s = 32767;      // 16-bit signed integer

// Wrapper classes (objects)
Integer intObj = 10;         // Auto-boxing
Long longObj = 10L;
Double doubleObj = 10.5;
Boolean boolObj = true;
```

### Reference Types
```java
// All user-defined types are reference types
String str = "Hello";        // Reference to String object
Person person = new Person(); // Reference to Person object

// Unlike C++, you can't do:
// Person person;  // This would be a compilation error
```

## Object-Oriented Features

### Classes and Inheritance

```java
// Base class
public class Animal {
    protected String name;
    
    public Animal(String name) {
        this.name = name;
    }
    
    public void makeSound() {
        System.out.println("Some generic sound");
    }
    
    // Virtual by default (unlike C++)
    public void move() {
        System.out.println("Moving...");
    }
}

// Derived class (single inheritance only)
public class Dog extends Animal {
    public Dog(String name) {
        super(name);  // Call parent constructor
    }
    
    @Override  // Annotation for method overriding
    public void makeSound() {
        System.out.println("Woof!");
    }
    
    @Override
    public void move() {
        System.out.println("Running on four legs");
    }
}
```

### Interfaces (Multiple Implementation)

```java
// Interface (like pure virtual class in C++)
public interface Flyable {
    void fly();  // implicitly public abstract
    
    // Default method (Java 8+)
    default void glide() {
        System.out.println("Gliding...");
    }
    
    // Static method (Java 8+)
    static void checkWeather() {
        System.out.println("Checking weather for flying");
    }
}

public interface Swimmable {
    void swim();
}

// Multiple interface implementation
public class Duck extends Animal implements Flyable, Swimmable {
    public Duck(String name) {
        super(name);
    }
    
    @Override
    public void fly() {
        System.out.println("Flying like a duck");
    }
    
    @Override
    public void swim() {
        System.out.println("Swimming like a duck");
    }
}
```

### Access Modifiers

```java
public class AccessExample {
    public int publicField;        // Accessible everywhere
    protected int protectedField;  // Accessible in package and subclasses
    int packageField;              // Package-private (default)
    private int privateField;      // Only within this class
    
    // Methods follow same rules
    public void publicMethod() {}
    protected void protectedMethod() {}
    void packageMethod() {}
    private void privateMethod() {}
}
```

## Exception Handling

### Checked vs Unchecked Exceptions

```java
// Checked exception - must be handled or declared
public void readFile(String filename) throws IOException {
    FileInputStream file = new FileInputStream(filename);
    // Must handle IOException or declare it
}

// Method that calls readFile must handle or declare
public void processFile() {
    try {
        readFile("data.txt");
    } catch (IOException e) {
        System.err.println("File error: " + e.getMessage());
    }
}

// Unchecked exceptions (RuntimeException subclasses)
public void divide(int a, int b) {
    if (b == 0) {
        throw new IllegalArgumentException("Division by zero");
    }
    int result = a / b;
}
```

### Try-with-resources (like RAII in C++)

```java
// Automatic resource management
try (FileInputStream fis = new FileInputStream("file.txt");
     BufferedReader reader = new BufferedReader(new InputStreamReader(fis))) {
    
    String line = reader.readLine();
    // Resources automatically closed when leaving try block
} catch (IOException e) {
    e.printStackTrace();
}
```

## Generics vs Templates

### Java Generics (Type Erasure)

```java
// Generic class
public class Container<T> {
    private T item;
    
    public void setItem(T item) {
        this.item = item;
    }
    
    public T getItem() {
        return item;
    }
}

// Usage
Container<String> stringContainer = new Container<>();
Container<Integer> intContainer = new Container<>();

// Generic method
public static <T> void swap(T[] array, int i, int j) {
    T temp = array[i];
    array[i] = array[j];
    array[j] = temp;
}
```

### Key Differences from C++ Templates:
- **Type Erasure**: Generic type info removed at runtime
- **No Specialization**: Can't specialize generics like C++ templates
- **Runtime Type Loss**: `Container<String>` becomes `Container` at runtime
- **Wildcard Types**: `? extends Type`, `? super Type`

## Package System

```java
// File: com/company/trading/Order.java
package com.company.trading;

import java.util.List;
import java.util.ArrayList;
import com.company.common.Logger;

public class Order {
    // Class implementation
}
```

### Key Points:
- **Package declaration**: Must be first non-comment line
- **Import statements**: Similar to C++ #include but different mechanism
- **CLASSPATH**: Java's equivalent to library/include paths
- **Directory structure**: Must match package structure

## String Handling

```java
// String is immutable (like const string in C++)
String str1 = "Hello";
String str2 = "World";
String result = str1 + " " + str2;  // Creates new string

// StringBuilder for mutable strings (like C++ string)
StringBuilder sb = new StringBuilder();
sb.append("Hello");
sb.append(" ");
sb.append("World");
String result2 = sb.toString();

// String comparison
String s1 = "Hello";
String s2 = "Hello";
boolean same = s1.equals(s2);     // Content comparison
boolean sameRef = s1 == s2;       // Reference comparison (usually true due to string pooling)

String s3 = new String("Hello");
boolean different = s1 == s3;     // false - different references
boolean sameContent = s1.equals(s3); // true - same content
```

## Key Learning Tips for C++ Developers:

1. **Think "References Everywhere"**: Except primitives, everything is a reference
2. **No Manual Memory Management**: Trust the GC, focus on algorithm logic
3. **Interfaces over Multiple Inheritance**: Design with composition and interfaces
4. **Exception Handling**: Embrace checked exceptions for robust error handling
5. **Collections Framework**: Learn ArrayList, HashMap, etc. - they're your STL equivalent
6. **Final vs Const**: `final` for constants and preventing reassignment/inheritance
7. **Package Organization**: Think in terms of namespaces but more structured
8. **IDE Dependency**: Java development heavily relies on IDEs (IntelliJ, Eclipse)

## Next Steps:
1. Set up development environment (JDK, IDE)
2. Learn Collections Framework thoroughly
3. Understand concurrency utilities (java.util.concurrent)
4. Practice with design patterns in Java context
5. Learn build tools (Maven/Gradle)
6. Understand JVM concepts (ClassLoader, GC types, etc.)
