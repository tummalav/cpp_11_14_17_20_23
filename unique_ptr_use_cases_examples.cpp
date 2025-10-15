#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <functional>
#include <array>
#include <algorithm>

/*
 * Comprehensive examples of std::unique_ptr use cases
 * Smart pointer for exclusive ownership and automatic memory management
 */

// =============================================================================
// 1. BASIC UNIQUE_PTR USAGE AND CREATION
// =============================================================================

namespace basic_unique_ptr {

    class Resource {
        std::string name;
        int id;

    public:
        Resource(const std::string& n, int i) : name(n), id(i) {
            std::cout << "Resource " << name << " (id: " << id << ") created\n";
        }

        ~Resource() {
            std::cout << "Resource " << name << " (id: " << id << ") destroyed\n";
        }

        void use() const {
            std::cout << "Using resource " << name << " (id: " << id << ")\n";
        }

        const std::string& getName() const { return name; }
        int getId() const { return id; }
    };

    void basic_creation_and_usage() {
        std::cout << "\n=== BASIC UNIQUE_PTR CREATION AND USAGE ===\n";

        // Method 1: Using make_unique (C++14, preferred)
        auto ptr1 = std::make_unique<Resource>("Resource1", 1);

        // Method 2: Using constructor with new (pre-C++14)
        std::unique_ptr<Resource> ptr2(new Resource("Resource2", 2));

        // Method 3: Using reset()
        std::unique_ptr<Resource> ptr3;
        ptr3.reset(new Resource("Resource3", 3));

        // Using the resources
        ptr1->use();
        (*ptr2).use();
        if (ptr3) {
            ptr3->use();
        }

        // Get raw pointer (use carefully)
        Resource* raw = ptr1.get();
        std::cout << "Raw pointer access: " << raw->getName() << "\n";

        std::cout << "Exiting scope - automatic cleanup will occur\n";
    } // Resources automatically destroyed here

    void array_unique_ptr() {
        std::cout << "\n=== UNIQUE_PTR WITH ARRAYS ===\n";

        // For arrays, use std::unique_ptr<T[]>
        const size_t size = 5;
        auto int_array = std::make_unique<int[]>(size);

        // Initialize array
        for (size_t i = 0; i < size; ++i) {
            int_array[i] = static_cast<int>(i * i);
        }

        // Access array elements
        std::cout << "Array contents: ";
        for (size_t i = 0; i < size; ++i) {
            std::cout << int_array[i] << " ";
        }
        std::cout << "\n";

        // Note: For dynamic arrays, prefer std::vector over unique_ptr<T[]>
        std::cout << "Note: std::vector is usually better than unique_ptr<T[]>\n";
    }
}

// =============================================================================
// 2. MOVE SEMANTICS AND OWNERSHIP TRANSFER
// =============================================================================

namespace move_semantics {

    using Resource = basic_unique_ptr::Resource;

    // Function that takes ownership
    void consume_resource(std::unique_ptr<Resource> resource) {
        if (resource) {
            std::cout << "Consuming ";
            resource->use();
        }
    } // resource destroyed here

    // Function that returns ownership
    std::unique_ptr<Resource> create_resource(const std::string& name, int id) {
        return std::make_unique<Resource>(name, id);
    }

    // Function that borrows (doesn't take ownership)
    void borrow_resource(const Resource* resource) {
        if (resource) {
            std::cout << "Borrowing ";
            resource->use();
        }
    }

    void ownership_transfer() {
        std::cout << "\n=== OWNERSHIP TRANSFER WITH MOVE SEMANTICS ===\n";

        // Create resource
        auto resource = std::make_unique<Resource>("MovableResource", 100);

        // Borrow without transferring ownership
        borrow_resource(resource.get());

        std::cout << "Resource still owned locally: " << (resource ? "Yes" : "No") << "\n";

        // Transfer ownership using move
        consume_resource(std::move(resource));

        // resource is now nullptr
        std::cout << "Resource still owned locally: " << (resource ? "Yes" : "No") << "\n";

        // Create and immediately transfer
        consume_resource(create_resource("TempResource", 200));

        std::cout << "All resources have been consumed\n";
    }

    void move_assignment() {
        std::cout << "\n=== MOVE ASSIGNMENT ===\n";

        auto resource1 = std::make_unique<Resource>("Resource1", 1);
        auto resource2 = std::make_unique<Resource>("Resource2", 2);

        std::cout << "Before move assignment:\n";
        std::cout << "resource1: " << (resource1 ? resource1->getName() : "nullptr") << "\n";
        std::cout << "resource2: " << (resource2 ? resource2->getName() : "nullptr") << "\n";

        // Move assignment - resource1's object is destroyed, resource2's object is transferred to resource1
        resource1 = std::move(resource2);

        std::cout << "After move assignment:\n";
        std::cout << "resource1: " << (resource1 ? resource1->getName() : "nullptr") << "\n";
        std::cout << "resource2: " << (resource2 ? resource2->getName() : "nullptr") << "\n";
    }
}

// =============================================================================
// 3. CUSTOM DELETERS
// =============================================================================

namespace custom_deleters {

    // Custom deleter for file handles
    struct FileDeleter {
        void operator()(std::FILE* file) const {
            if (file) {
                std::cout << "Closing file with custom deleter\n";
                std::fclose(file);
            }
        }
    };

    // Custom deleter for arrays allocated with malloc
    struct MallocDeleter {
        void operator()(void* ptr) const {
            std::cout << "Freeing memory with malloc deleter\n";
            std::free(ptr);
        }
    };

    // Lambda deleter
    auto lambda_deleter = [](int* ptr) {
        std::cout << "Deleting with lambda deleter\n";
        delete ptr;
    };

    void custom_deleter_examples() {
        std::cout << "\n=== CUSTOM DELETERS ===\n";

        // File handle with custom deleter
        {
            std::unique_ptr<std::FILE, FileDeleter> file_ptr(std::fopen("temp.txt", "w"));
            if (file_ptr) {
                std::fprintf(file_ptr.get(), "Hello, Custom Deleter!\n");
            }
        } // File automatically closed here

        // Memory allocated with malloc
        {
            std::unique_ptr<int, MallocDeleter> malloc_ptr(
                static_cast<int*>(std::malloc(sizeof(int)))
            );
            if (malloc_ptr) {
                *malloc_ptr = 42;
                std::cout << "Malloc pointer value: " << *malloc_ptr << "\n";
            }
        } // Memory freed with custom deleter

        // Lambda deleter
        {
            std::unique_ptr<int, decltype(lambda_deleter)> lambda_ptr(
                new int(99), lambda_deleter
            );
            std::cout << "Lambda pointer value: " << *lambda_ptr << "\n";
        } // Deleted with lambda

        // Function pointer deleter
        {
            auto func_deleter = [](int* p) {
                std::cout << "Function deleter called\n";
                delete p;
            };

            std::unique_ptr<int, std::function<void(int*)>> func_ptr(
                new int(77), func_deleter
            );
            std::cout << "Function pointer value: " << *func_ptr << "\n";
        }
    }
}

// =============================================================================
// 4. UNIQUE_PTR IN CONTAINERS
// =============================================================================

namespace containers_with_unique_ptr {

    using Resource = basic_unique_ptr::Resource;

    void vector_of_unique_ptrs() {
        std::cout << "\n=== VECTOR OF UNIQUE_PTRS ===\n";

        std::vector<std::unique_ptr<Resource>> resources;

        // Add resources to vector
        resources.push_back(std::make_unique<Resource>("VectorResource1", 1));
        resources.push_back(std::make_unique<Resource>("VectorResource2", 2));
        resources.emplace_back(std::make_unique<Resource>("VectorResource3", 3));

        // Use resources
        for (const auto& resource : resources) {
            resource->use();
        }

        // Find resource by ID
        auto it = std::find_if(resources.begin(), resources.end(),
            [](const std::unique_ptr<Resource>& res) {
                return res->getId() == 2;
            });

        if (it != resources.end()) {
            std::cout << "Found resource with ID 2: " << (*it)->getName() << "\n";
        }

        // Remove a resource (this will destroy it)
        resources.erase(resources.begin() + 1);
        std::cout << "Removed middle resource\n";

        // Move a resource out of the vector
        auto moved_resource = std::move(resources.front());
        resources.erase(resources.begin());

        std::cout << "Moved resource: " << moved_resource->getName() << "\n";

        std::cout << "Vector now has " << resources.size() << " resources\n";

    } // All remaining resources automatically destroyed

    // Factory pattern with unique_ptr
    class ResourceFactory {
    public:
        enum class ResourceType { Basic, Advanced, Premium };

        static std::unique_ptr<Resource> createResource(ResourceType type, int id) {
            switch (type) {
                case ResourceType::Basic:
                    return std::make_unique<Resource>("BasicResource", id);
                case ResourceType::Advanced:
                    return std::make_unique<Resource>("AdvancedResource", id);
                case ResourceType::Premium:
                    return std::make_unique<Resource>("PremiumResource", id);
                default:
                    return nullptr;
            }
        }
    };

    void factory_pattern() {
        std::cout << "\n=== FACTORY PATTERN WITH UNIQUE_PTR ===\n";

        std::vector<std::unique_ptr<Resource>> mixed_resources;

        mixed_resources.push_back(ResourceFactory::createResource(
            ResourceFactory::ResourceType::Basic, 1));
        mixed_resources.push_back(ResourceFactory::createResource(
            ResourceFactory::ResourceType::Advanced, 2));
        mixed_resources.push_back(ResourceFactory::createResource(
            ResourceFactory::ResourceType::Premium, 3));

        for (const auto& resource : mixed_resources) {
            if (resource) {
                resource->use();
            }
        }
    }
}

// =============================================================================
// 5. PIMPL IDIOM WITH UNIQUE_PTR
// =============================================================================

namespace pimpl_idiom {

    // Forward declaration
    class Widget {
    public:
        Widget();
        ~Widget(); // Must be defined in .cpp file where Impl is complete

        Widget(const Widget&) = delete;
        Widget& operator=(const Widget&) = delete;

        Widget(Widget&&) = default;
        Widget& operator=(Widget&&) = default;

        void doSomething();
        void setValue(int value);
        int getValue() const;

    private:
        class Impl; // Forward declaration
        std::unique_ptr<Impl> pImpl; // Pointer to implementation
    };

    // Implementation class (would typically be in .cpp file)
    class Widget::Impl {
    public:
        void doSomething() {
            std::cout << "Widget implementation doing something with value: " << value << "\n";
        }

        void setValue(int v) { value = v; }
        int getValue() const { return value; }

    private:
        int value = 42;
        std::string internal_string = "Hidden implementation detail";
        std::vector<double> internal_data = {1.1, 2.2, 3.3};
    };

    // Widget public interface implementation
    Widget::Widget() : pImpl(std::make_unique<Impl>()) {}

    // Destructor must be defined where Impl is complete
    Widget::~Widget() = default;

    void Widget::doSomething() {
        pImpl->doSomething();
    }

    void Widget::setValue(int value) {
        pImpl->setValue(value);
    }

    int Widget::getValue() const {
        return pImpl->getValue();
    }

    void pimpl_example() {
        std::cout << "\n=== PIMPL IDIOM WITH UNIQUE_PTR ===\n";

        Widget widget;
        std::cout << "Initial value: " << widget.getValue() << "\n";

        widget.setValue(100);
        widget.doSomething();

        // Move semantics work automatically
        Widget widget2 = std::move(widget);
        widget2.setValue(200);
        widget2.doSomething();

        std::cout << "PIMPL idiom provides:\n";
        std::cout << "- Compilation firewall\n";
        std::cout << "- Stable ABI\n";
        std::cout << "- Reduced compile times\n";
        std::cout << "- Implementation hiding\n";
    }
}

// =============================================================================
// 6. POLYMORPHISM AND INHERITANCE
// =============================================================================

namespace polymorphism_examples {

    class Shape {
    public:
        virtual ~Shape() = default;
        virtual void draw() const = 0;
        virtual double area() const = 0;
        virtual std::unique_ptr<Shape> clone() const = 0;
    };

    class Circle : public Shape {
        double radius;

    public:
        Circle(double r) : radius(r) {}

        void draw() const override {
            std::cout << "Drawing circle with radius " << radius << "\n";
        }

        double area() const override {
            return 3.14159 * radius * radius;
        }

        std::unique_ptr<Shape> clone() const override {
            return std::make_unique<Circle>(radius);
        }
    };

    class Rectangle : public Shape {
        double width, height;

    public:
        Rectangle(double w, double h) : width(w), height(h) {}

        void draw() const override {
            std::cout << "Drawing rectangle " << width << "x" << height << "\n";
        }

        double area() const override {
            return width * height;
        }

        std::unique_ptr<Shape> clone() const override {
            return std::make_unique<Rectangle>(width, height);
        }
    };

    // Factory function
    std::unique_ptr<Shape> createShape(const std::string& type, double param1, double param2 = 0) {
        if (type == "circle") {
            return std::make_unique<Circle>(param1);
        } else if (type == "rectangle") {
            return std::make_unique<Rectangle>(param1, param2);
        }
        return nullptr;
    }

    void polymorphism_example() {
        std::cout << "\n=== POLYMORPHISM WITH UNIQUE_PTR ===\n";

        std::vector<std::unique_ptr<Shape>> shapes;

        shapes.push_back(createShape("circle", 5.0));
        shapes.push_back(createShape("rectangle", 4.0, 6.0));
        shapes.push_back(std::make_unique<Circle>(3.0));

        // Polymorphic behavior
        for (const auto& shape : shapes) {
            shape->draw();
            std::cout << "Area: " << shape->area() << "\n\n";
        }

        // Clone shapes
        std::vector<std::unique_ptr<Shape>> cloned_shapes;
        for (const auto& shape : shapes) {
            cloned_shapes.push_back(shape->clone());
        }

        std::cout << "Cloned shapes:\n";
        for (const auto& shape : cloned_shapes) {
            shape->draw();
        }
    }
}

// =============================================================================
// 7. EXCEPTION SAFETY AND RAII
// =============================================================================

namespace exception_safety {

    class ResourceManager {
        std::unique_ptr<int[]> buffer;
        std::unique_ptr<std::ofstream> file;
        size_t size;

    public:
        ResourceManager(size_t buffer_size, const std::string& filename)
            : buffer(std::make_unique<int[]>(buffer_size))
            , file(std::make_unique<std::ofstream>(filename))
            , size(buffer_size) {

            if (!file->is_open()) {
                throw std::runtime_error("Failed to open file: " + filename);
            }

            // Initialize buffer
            for (size_t i = 0; i < size; ++i) {
                buffer[i] = static_cast<int>(i);
            }

            std::cout << "ResourceManager created with buffer size " << size << "\n";
        }

        ~ResourceManager() {
            std::cout << "ResourceManager destroyed - resources automatically cleaned up\n";
        }

        void processData() {
            if (!file || !buffer) {
                throw std::runtime_error("Resources not available");
            }

            // Simulate some work that might throw
            for (size_t i = 0; i < size; ++i) {
                *file << buffer[i] << " ";

                // Simulate potential exception
                if (i == size/2 && rand() % 10 == 0) {
                    throw std::runtime_error("Processing error at index " + std::to_string(i));
                }
            }
            *file << "\n";
            file->flush();

            std::cout << "Data processing completed successfully\n";
        }

        // Move-only semantics
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        ResourceManager(ResourceManager&&) = default;
        ResourceManager& operator=(ResourceManager&&) = default;
    };

    void exception_safety_example() {
        std::cout << "\n=== EXCEPTION SAFETY WITH UNIQUE_PTR ===\n";

        try {
            ResourceManager manager(10, "output.txt");
            manager.processData();

            // Even if an exception is thrown during processing,
            // the ResourceManager destructor will be called,
            // and unique_ptrs will automatically clean up resources

        } catch (const std::exception& e) {
            std::cout << "Exception caught: " << e.what() << "\n";
            std::cout << "Resources were automatically cleaned up\n";
        }

        std::cout << "Function completed - no memory leaks\n";
    }
}

// =============================================================================
// 8. PERFORMANCE CONSIDERATIONS
// =============================================================================

namespace performance_considerations {

    void performance_comparison() {
        std::cout << "\n=== PERFORMANCE CONSIDERATIONS ===\n";

        const size_t iterations = 1000000;

        // Raw pointer timing (for comparison - NOT recommended for production)
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            int* raw = new int(42);
            delete raw;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto raw_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // unique_ptr timing
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            auto unique = std::make_unique<int>(42);
        }
        end = std::chrono::high_resolution_clock::now();
        auto unique_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Performance comparison (" << iterations << " iterations):\n";
        std::cout << "Raw pointer: " << raw_time.count() << " microseconds\n";
        std::cout << "unique_ptr:  " << unique_time.count() << " microseconds\n";
        std::cout << "Overhead:    " << (unique_time.count() - raw_time.count()) << " microseconds\n";
        std::cout << "Percentage:  " <<
                     (100.0 * (unique_time.count() - raw_time.count()) / raw_time.count()) << "%\n";

        std::cout << "\nNote: unique_ptr overhead is minimal, and the safety benefits far outweigh the cost\n";
    }

    void zero_overhead_examples() {
        std::cout << "\n=== ZERO-OVERHEAD EXAMPLES ===\n";

        // These operations have zero runtime overhead compared to raw pointers:

        auto ptr = std::make_unique<int>(42);

        // Dereferencing
        int value = *ptr;  // Same as *raw_ptr

        // Arrow operator
        // ptr->member;    // Same as raw_ptr->member

        // Boolean conversion
        if (ptr) {  // Same as if (raw_ptr != nullptr)
            std::cout << "Pointer is valid\n";
        }

        // Get raw pointer
        int* raw = ptr.get();  // Zero overhead

        // Move operations
        auto ptr2 = std::move(ptr);  // Zero overhead

        std::cout << "Value: " << value << "\n";
        std::cout << "All operations above have zero runtime overhead!\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL EXAMPLES
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE EXAMPLES: std::unique_ptr Use Cases\n";
    std::cout << "=============================================================================\n";

    // Basic usage
    basic_unique_ptr::basic_creation_and_usage();
    basic_unique_ptr::array_unique_ptr();

    // Move semantics
    move_semantics::ownership_transfer();
    move_semantics::move_assignment();

    // Custom deleters
    custom_deleters::custom_deleter_examples();

    // Containers
    containers_with_unique_ptr::vector_of_unique_ptrs();
    containers_with_unique_ptr::factory_pattern();

    // PIMPL idiom
    pimpl_idiom::pimpl_example();

    // Polymorphism
    polymorphism_examples::polymorphism_example();

    // Exception safety
    exception_safety::exception_safety_example();

    // Performance considerations
    performance_considerations::performance_comparison();
    performance_considerations::zero_overhead_examples();

    std::cout << "\n=============================================================================\n";
    std::cout << "KEY TAKEAWAYS:\n";
    std::cout << "1. Use std::make_unique for creation (exception safe)\n";
    std::cout << "2. Move semantics for ownership transfer\n";
    std::cout << "3. Custom deleters for non-standard cleanup\n";
    std::cout << "4. Perfect for PIMPL idiom and polymorphism\n";
    std::cout << "5. Exception-safe RAII resource management\n";
    std::cout << "6. Zero runtime overhead for most operations\n";
    std::cout << "7. Move-only semantics prevent accidental copying\n";
    std::cout << "8. Automatic cleanup prevents memory leaks\n";
    std::cout << "=============================================================================\n";

    return 0;
}
