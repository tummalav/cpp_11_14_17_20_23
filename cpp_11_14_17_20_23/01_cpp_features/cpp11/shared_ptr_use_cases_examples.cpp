#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>

/*
 * Comprehensive examples of std::shared_ptr use cases
 * Smart pointer for shared ownership and reference counting
 */

// =============================================================================
// 1. BASIC SHARED_PTR USAGE AND CREATION
// =============================================================================

namespace basic_shared_ptr {

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
        std::cout << "\n=== BASIC SHARED_PTR CREATION AND USAGE ===\n";

        // Method 1: Using make_shared (preferred - more efficient)
        auto ptr1 = std::make_shared<Resource>("SharedResource1", 1);
        std::cout << "Reference count after creation: " << ptr1.use_count() << "\n";

        // Method 2: Using constructor with new (less efficient)
        std::shared_ptr<Resource> ptr2(new Resource("SharedResource2", 2));
        std::cout << "Reference count for ptr2: " << ptr2.use_count() << "\n";

        // Copy construction - increases reference count
        auto ptr1_copy = ptr1;
        std::cout << "Reference count after copy: " << ptr1.use_count() << "\n";

        // Copy assignment
        auto ptr1_assign = std::make_shared<Resource>("TempResource", 999);
        std::cout << "Before assignment, ptr1_assign count: " << ptr1_assign.use_count() << "\n";
        ptr1_assign = ptr1;  // TempResource is destroyed here
        std::cout << "After assignment, reference count: " << ptr1.use_count() << "\n";

        // Using the resources
        ptr1->use();
        (*ptr2).use();

        // Check ownership
        if (ptr1) {
            std::cout << "ptr1 owns resource: " << ptr1->getName() << "\n";
        }

        // Get raw pointer (use carefully)
        Resource* raw = ptr1.get();
        std::cout << "Raw pointer access: " << raw->getName() << "\n";

        std::cout << "Final reference count for ptr1: " << ptr1.use_count() << "\n";
        std::cout << "Exiting scope - resources will be destroyed when ref count reaches 0\n";
    }

    void reference_counting_demo() {
        std::cout << "\n=== REFERENCE COUNTING DEMONSTRATION ===\n";

        std::shared_ptr<Resource> main_ptr;

        {
            auto local_ptr = std::make_shared<Resource>("CountedResource", 100);
            std::cout << "In inner scope, count: " << local_ptr.use_count() << "\n";

            main_ptr = local_ptr;
            std::cout << "After assignment, count: " << local_ptr.use_count() << "\n";

            {
                auto another_ptr = main_ptr;
                std::cout << "With another copy, count: " << main_ptr.use_count() << "\n";
            } // another_ptr destroyed here

            std::cout << "After inner destruction, count: " << main_ptr.use_count() << "\n";
        } // local_ptr destroyed here

        std::cout << "After scope exit, count: " << main_ptr.use_count() << "\n";
        main_ptr.reset(); // Explicitly release
        std::cout << "After reset, count: " << (main_ptr ? main_ptr.use_count() : 0) << "\n";
    }
}

// =============================================================================
// 2. SHARED OWNERSHIP SCENARIOS
// =============================================================================

namespace shared_ownership {

    using Resource = basic_shared_ptr::Resource;

    class ResourceUser {
        std::shared_ptr<Resource> resource;
        std::string user_name;

    public:
        ResourceUser(const std::string& name, std::shared_ptr<Resource> res)
            : user_name(name), resource(res) {
            std::cout << "ResourceUser " << user_name << " created\n";
        }

        ~ResourceUser() {
            std::cout << "ResourceUser " << user_name << " destroyed\n";
        }

        void use_resource() {
            if (resource) {
                std::cout << user_name << " is ";
                resource->use();
                std::cout << "Resource ref count: " << resource.use_count() << "\n";
            }
        }

        std::shared_ptr<Resource> get_resource() const { return resource; }
    };

    void shared_ownership_example() {
        std::cout << "\n=== SHARED OWNERSHIP EXAMPLE ===\n";

        // Create a shared resource
        auto shared_resource = std::make_shared<Resource>("SharedResource", 42);
        std::cout << "Initial ref count: " << shared_resource.use_count() << "\n";

        // Multiple users sharing the same resource
        std::vector<std::unique_ptr<ResourceUser>> users;

        users.push_back(std::make_unique<ResourceUser>("User1", shared_resource));
        std::cout << "After User1, ref count: " << shared_resource.use_count() << "\n";

        users.push_back(std::make_unique<ResourceUser>("User2", shared_resource));
        std::cout << "After User2, ref count: " << shared_resource.use_count() << "\n";

        users.push_back(std::make_unique<ResourceUser>("User3", shared_resource));
        std::cout << "After User3, ref count: " << shared_resource.use_count() << "\n";

        // All users use the shared resource
        for (auto& user : users) {
            user->use_resource();
        }

        // Remove some users
        users.erase(users.begin());  // Remove User1
        std::cout << "After removing User1, ref count: " << shared_resource.use_count() << "\n";

        users.clear();  // Remove all users
        std::cout << "After removing all users, ref count: " << shared_resource.use_count() << "\n";

        // Resource is still alive because we still have shared_resource
        shared_resource->use();

    } // shared_resource destroyed here when ref count reaches 0
}

// =============================================================================
// 3. WEAK_PTR TO BREAK CYCLES
// =============================================================================

namespace weak_ptr_examples {

    class Node : public std::enable_shared_from_this<Node> {
    public:
        std::string name;
        std::shared_ptr<Node> next;
        std::weak_ptr<Node> parent;  // Use weak_ptr to break cycles
        std::vector<std::shared_ptr<Node>> children;

        Node(const std::string& n) : name(n) {
            std::cout << "Node " << name << " created\n";
        }

        ~Node() {
            std::cout << "Node " << name << " destroyed\n";
        }

        void add_child(std::shared_ptr<Node> child) {
            children.push_back(child);
            child->parent = shared_from_this();
        }

        void print_info() const {
            std::cout << "Node: " << name << ", Children: " << children.size();
            if (auto p = parent.lock()) {
                std::cout << ", Parent: " << p->name;
            } else {
                std::cout << ", Parent: none";
            }
            std::cout << "\n";
        }
    };

    // Enable shared_from_this
    class SafeNode : public std::enable_shared_from_this<SafeNode> {
    public:
        std::string name;
        std::vector<std::shared_ptr<SafeNode>> children;
        std::weak_ptr<SafeNode> parent;

        SafeNode(const std::string& n) : name(n) {
            std::cout << "SafeNode " << name << " created\n";
        }

        ~SafeNode() {
            std::cout << "SafeNode " << name << " destroyed\n";
        }

        void add_child(std::shared_ptr<SafeNode> child) {
            children.push_back(child);
            child->parent = shared_from_this();  // Safe to use
        }

        std::shared_ptr<SafeNode> get_parent() {
            return parent.lock();  // Convert weak_ptr to shared_ptr
        }

        void print_hierarchy(int depth = 0) const {
            std::string indent(depth * 2, ' ');
            std::cout << indent << "- " << name << " (ref_count: " <<
                        shared_from_this().use_count() << ")\n";

            for (const auto& child : children) {
                child->print_hierarchy(depth + 1);
            }
        }
    };

    void weak_ptr_cycle_breaking() {
        std::cout << "\n=== WEAK_PTR CYCLE BREAKING ===\n";

        auto root = std::make_shared<SafeNode>("Root");
        auto child1 = std::make_shared<SafeNode>("Child1");
        auto child2 = std::make_shared<SafeNode>("Child2");
        auto grandchild = std::make_shared<SafeNode>("GrandChild");

        std::cout << "Building hierarchy...\n";
        root->add_child(child1);
        root->add_child(child2);
        child1->add_child(grandchild);

        std::cout << "\nHierarchy structure:\n";
        root->print_hierarchy();

        std::cout << "\nReference counts:\n";
        std::cout << "Root: " << root.use_count() << "\n";
        std::cout << "Child1: " << child1.use_count() << "\n";
        std::cout << "Child2: " << child2.use_count() << "\n";
        std::cout << "GrandChild: " << grandchild.use_count() << "\n";

        // Test weak_ptr access
        auto parent_of_grandchild = grandchild->get_parent();
        if (parent_of_grandchild) {
            std::cout << "GrandChild's parent: " << parent_of_grandchild->name << "\n";
        }

        std::cout << "\nClearing local references...\n";
        child1.reset();
        child2.reset();
        grandchild.reset();

        std::cout << "Root still exists: " << (root ? "Yes" : "No") << "\n";
        std::cout << "Root ref count: " << root.use_count() << "\n";

    } // All nodes properly destroyed due to weak_ptr breaking cycles

    void weak_ptr_observer_pattern() {
        std::cout << "\n=== WEAK_PTR OBSERVER PATTERN ===\n";

        class Subject;

        class Observer {
            std::weak_ptr<Subject> subject;
            std::string name;

        public:
            Observer(const std::string& n) : name(n) {}

            void observe(std::shared_ptr<Subject> subj) {
                subject = subj;
                std::cout << "Observer " << name << " started observing\n";
            }

            void notify() {
                if (auto subj = subject.lock()) {
                    std::cout << "Observer " << name << " received notification\n";
                } else {
                    std::cout << "Observer " << name << " subject no longer exists\n";
                }
            }

            bool is_subject_alive() const {
                return !subject.expired();
            }
        };

        class Subject : public std::enable_shared_from_this<Subject> {
            std::vector<std::weak_ptr<Observer>> observers;
            std::string name;

        public:
            Subject(const std::string& n) : name(n) {}

            void add_observer(std::shared_ptr<Observer> obs) {
                observers.push_back(obs);
                obs->observe(shared_from_this());
            }

            void notify_all() {
                std::cout << "Subject " << name << " notifying observers...\n";

                // Clean up expired observers
                observers.erase(
                    std::remove_if(observers.begin(), observers.end(),
                        [](const std::weak_ptr<Observer>& weak_obs) {
                            return weak_obs.expired();
                        }),
                    observers.end());

                for (auto& weak_obs : observers) {
                    if (auto obs = weak_obs.lock()) {
                        obs->notify();
                    }
                }
            }
        };

        auto subject = std::make_shared<Subject>("TestSubject");
        auto obs1 = std::make_shared<Observer>("Observer1");
        auto obs2 = std::make_shared<Observer>("Observer2");

        subject->add_observer(obs1);
        subject->add_observer(obs2);

        subject->notify_all();

        // Remove one observer
        obs1.reset();
        std::cout << "\nAfter removing Observer1:\n";
        subject->notify_all();
    }
}

// =============================================================================
// 4. CUSTOM DELETERS WITH SHARED_PTR
// =============================================================================

namespace custom_deleters_shared {

    // Custom deleter for arrays
    template<typename T>
    struct ArrayDeleter {
        void operator()(T* ptr) const {
            std::cout << "Array deleter called\n";
            delete[] ptr;
        }
    };

    // Custom deleter for file handles
    struct FileDeleter {
        void operator()(std::FILE* file) const {
            if (file) {
                std::cout << "Closing file with shared_ptr custom deleter\n";
                std::fclose(file);
            }
        }
    };

    void custom_deleter_examples() {
        std::cout << "\n=== CUSTOM DELETERS WITH SHARED_PTR ===\n";

        // Array with custom deleter
        {
            std::shared_ptr<int> int_array(new int[10], ArrayDeleter<int>());

            // Initialize array
            for (int i = 0; i < 10; ++i) {
                int_array.get()[i] = i * i;
            }

            // Share the array
            auto shared_array = int_array;
            std::cout << "Array ref count: " << int_array.use_count() << "\n";

            std::cout << "Array contents: ";
            for (int i = 0; i < 10; ++i) {
                std::cout << shared_array.get()[i] << " ";
            }
            std::cout << "\n";
        } // Array deleted with custom deleter

        // File handle with custom deleter
        {
            std::shared_ptr<std::FILE> file_ptr(
                std::fopen("shared_test.txt", "w"),
                FileDeleter()
            );

            if (file_ptr) {
                std::fprintf(file_ptr.get(), "Hello from shared_ptr!\n");

                // Share the file handle
                auto shared_file = file_ptr;
                std::cout << "File ref count: " << file_ptr.use_count() << "\n";

                std::fprintf(shared_file.get(), "Written by shared reference!\n");
            }
        } // File automatically closed

        // Lambda deleter
        {
            auto lambda_deleter = [](double* ptr) {
                std::cout << "Lambda deleter for double array\n";
                delete[] ptr;
            };

            std::shared_ptr<double> double_array(new double[5], lambda_deleter);

            // Initialize and share
            for (int i = 0; i < 5; ++i) {
                double_array.get()[i] = i * 3.14;
            }

            auto shared_doubles = double_array;
            std::cout << "Double array ref count: " << double_array.use_count() << "\n";
        }
    }
}

// =============================================================================
// 5. THREAD SAFETY WITH SHARED_PTR
// =============================================================================

namespace thread_safety {

    class ThreadSafeCounter {
        mutable std::mutex mutex_;
        int count_ = 0;
        std::string name_;

    public:
        ThreadSafeCounter(const std::string& name) : name_(name) {
            std::cout << "Counter " << name_ << " created\n";
        }

        ~ThreadSafeCounter() {
            std::cout << "Counter " << name_ << " destroyed\n";
        }

        void increment() {
            std::lock_guard<std::mutex> lock(mutex_);
            ++count_;
            std::cout << name_ << " incremented to " << count_ << "\n";
        }

        int get_count() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return count_;
        }

        const std::string& get_name() const { return name_; }
    };

    void worker_thread(std::shared_ptr<ThreadSafeCounter> counter, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            counter->increment();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void thread_safety_example() {
        std::cout << "\n=== THREAD SAFETY WITH SHARED_PTR ===\n";

        auto shared_counter = std::make_shared<ThreadSafeCounter>("SharedCounter");
        std::cout << "Initial ref count: " << shared_counter.use_count() << "\n";

        const int num_threads = 3;
        const int iterations_per_thread = 5;

        std::vector<std::thread> threads;

        // Create threads that share the counter
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker_thread, shared_counter, iterations_per_thread);
        }

        std::cout << "Ref count with " << num_threads << " threads: " <<
                     shared_counter.use_count() << "\n";

        // Wait for all threads to complete
        for (auto& t : threads) {
            t.join();
        }

        std::cout << "Final count: " << shared_counter->get_count() << "\n";
        std::cout << "Final ref count: " << shared_counter.use_count() << "\n";

    } // Counter destroyed when last reference goes out of scope

    void atomic_shared_ptr_example() {
        std::cout << "\n=== ATOMIC SHARED_PTR OPERATIONS ===\n";

        std::shared_ptr<ThreadSafeCounter> global_counter;
        std::mutex global_mutex;

        auto updater_thread = [&](const std::string& name, int iterations) {
            for (int i = 0; i < iterations; ++i) {
                auto new_counter = std::make_shared<ThreadSafeCounter>(name + std::to_string(i));

                // Atomic update of shared_ptr
                {
                    std::lock_guard<std::mutex> lock(global_mutex);
                    global_counter = new_counter;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                // Use the current counter
                std::shared_ptr<ThreadSafeCounter> local_copy;
                {
                    std::lock_guard<std::mutex> lock(global_mutex);
                    local_copy = global_counter;
                }

                if (local_copy) {
                    local_copy->increment();
                }
            }
        };

        std::thread t1(updater_thread, "Thread1_Counter", 3);
        std::thread t2(updater_thread, "Thread2_Counter", 3);

        t1.join();
        t2.join();

        std::cout << "Final global counter: ";
        if (global_counter) {
            std::cout << global_counter->get_name() << "\n";
        } else {
            std::cout << "nullptr\n";
        }
    }
}

// =============================================================================
// 6. FACTORY PATTERNS AND CACHING
// =============================================================================

namespace factory_and_caching {

    class ExpensiveResource {
        std::string id_;
        std::vector<double> data_;

    public:
        ExpensiveResource(const std::string& id, size_t size) : id_(id), data_(size) {
            std::cout << "Creating expensive resource: " << id_ << " (size: " << size << ")\n";

            // Simulate expensive computation
            for (size_t i = 0; i < size; ++i) {
                data_[i] = i * 3.14159;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        ~ExpensiveResource() {
            std::cout << "Destroying expensive resource: " << id_ << "\n";
        }

        const std::string& get_id() const { return id_; }
        double get_value(size_t index) const {
            return index < data_.size() ? data_[index] : 0.0;
        }
        size_t get_size() const { return data_.size(); }
    };

    class ResourceFactory {
        std::unordered_map<std::string, std::weak_ptr<ExpensiveResource>> cache_;
        mutable std::mutex mutex_;

    public:
        std::shared_ptr<ExpensiveResource> get_resource(const std::string& id, size_t size = 100) {
            std::lock_guard<std::mutex> lock(mutex_);

            // Check if resource exists in cache
            auto it = cache_.find(id);
            if (it != cache_.end()) {
                if (auto existing = it->second.lock()) {
                    std::cout << "Returning cached resource: " << id << "\n";
                    return existing;
                } else {
                    // Weak pointer expired, remove from cache
                    cache_.erase(it);
                }
            }

            // Create new resource
            auto new_resource = std::make_shared<ExpensiveResource>(id, size);
            cache_[id] = new_resource;

            return new_resource;
        }

        void cleanup_cache() {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto it = cache_.begin(); it != cache_.end();) {
                if (it->second.expired()) {
                    std::cout << "Removing expired resource from cache: " << it->first << "\n";
                    it = cache_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        size_t cache_size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return cache_.size();
        }
    };

    void factory_caching_example() {
        std::cout << "\n=== FACTORY PATTERN WITH CACHING ===\n";

        ResourceFactory factory;

        // Request same resource multiple times
        auto res1 = factory.get_resource("Resource_A", 50);
        std::cout << "Cache size: " << factory.cache_size() << "\n";

        auto res2 = factory.get_resource("Resource_A", 50);  // Should return cached
        std::cout << "res1 == res2: " << (res1.get() == res2.get()) << "\n";
        std::cout << "Reference count: " << res1.use_count() << "\n";

        // Request different resource
        auto res3 = factory.get_resource("Resource_B", 75);
        std::cout << "Cache size: " << factory.cache_size() << "\n";

        // Use resources
        std::cout << "Resource A value[10]: " << res1->get_value(10) << "\n";
        std::cout << "Resource B value[20]: " << res3->get_value(20) << "\n";

        // Release one reference
        res2.reset();
        std::cout << "After releasing res2, ref count: " << res1.use_count() << "\n";

        // Release all references to Resource_A
        res1.reset();
        std::cout << "Released all references to Resource_A\n";

        // Cleanup expired cache entries
        factory.cleanup_cache();
        std::cout << "Cache size after cleanup: " << factory.cache_size() << "\n";

        // Request Resource_A again - should create new one
        auto res4 = factory.get_resource("Resource_A", 50);
        std::cout << "Cache size: " << factory.cache_size() << "\n";

    } // All resources destroyed when going out of scope
}

// =============================================================================
// 7. POLYMORPHISM AND SHARED_PTR
// =============================================================================

namespace polymorphism_shared {

    class Animal {
    public:
        virtual ~Animal() = default;
        virtual void make_sound() const = 0;
        virtual std::string get_type() const = 0;
    };

    class Dog : public Animal {
        std::string name_;

    public:
        Dog(const std::string& name) : name_(name) {
            std::cout << "Dog " << name_ << " created\n";
        }

        ~Dog() {
            std::cout << "Dog " << name_ << " destroyed\n";
        }

        void make_sound() const override {
            std::cout << name_ << " says: Woof!\n";
        }

        std::string get_type() const override {
            return "Dog(" + name_ + ")";
        }
    };

    class Cat : public Animal {
        std::string name_;

    public:
        Cat(const std::string& name) : name_(name) {
            std::cout << "Cat " << name_ << " created\n";
        }

        ~Cat() {
            std::cout << "Cat " << name_ << " destroyed\n";
        }

        void make_sound() const override {
            std::cout << name_ << " says: Meow!\n";
        }

        std::string get_type() const override {
            return "Cat(" + name_ + ")";
        }
    };

    class AnimalShelter {
        std::vector<std::shared_ptr<Animal>> animals_;

    public:
        void add_animal(std::shared_ptr<Animal> animal) {
            animals_.push_back(animal);
            std::cout << "Added " << animal->get_type() << " to shelter\n";
        }

        void remove_animal(std::shared_ptr<Animal> animal) {
            auto it = std::find(animals_.begin(), animals_.end(), animal);
            if (it != animals_.end()) {
                std::cout << "Removing " << (*it)->get_type() << " from shelter\n";
                animals_.erase(it);
            }
        }

        void make_all_sounds() const {
            std::cout << "All animals in shelter:\n";
            for (const auto& animal : animals_) {
                animal->make_sound();
            }
        }

        std::shared_ptr<Animal> find_animal_by_type(const std::string& type) const {
            auto it = std::find_if(animals_.begin(), animals_.end(),
                [&type](const std::shared_ptr<Animal>& animal) {
                    return animal->get_type().find(type) != std::string::npos;
                });

            return (it != animals_.end()) ? *it : nullptr;
        }

        size_t size() const { return animals_.size(); }
    };

    void polymorphism_example() {
        std::cout << "\n=== POLYMORPHISM WITH SHARED_PTR ===\n";

        AnimalShelter shelter;

        // Create animals
        auto dog1 = std::make_shared<Dog>("Buddy");
        auto dog2 = std::make_shared<Dog>("Rex");
        auto cat1 = std::make_shared<Cat>("Whiskers");
        auto cat2 = std::make_shared<Cat>("Mittens");

        std::cout << "Dog1 ref count: " << dog1.use_count() << "\n";

        // Add to shelter
        shelter.add_animal(dog1);
        shelter.add_animal(dog2);
        shelter.add_animal(cat1);
        shelter.add_animal(cat2);

        std::cout << "Dog1 ref count after adding to shelter: " << dog1.use_count() << "\n";

        // Make all animals sound
        shelter.make_all_sounds();

        // Find specific animal
        auto found_dog = shelter.find_animal_by_type("Buddy");
        if (found_dog) {
            std::cout << "Found animal: ";
            found_dog->make_sound();
            std::cout << "Found animal ref count: " << found_dog.use_count() << "\n";
        }

        // Remove animal from shelter
        shelter.remove_animal(dog2);
        std::cout << "Dog2 ref count after removal: " << dog2.use_count() << "\n";

        std::cout << "Shelter size: " << shelter.size() << "\n";

        // Create additional owner
        std::vector<std::shared_ptr<Animal>> my_pets;
        my_pets.push_back(dog1);  // Shared ownership
        my_pets.push_back(cat1);  // Shared ownership

        std::cout << "Dog1 ref count with shared ownership: " << dog1.use_count() << "\n";
        std::cout << "Cat1 ref count with shared ownership: " << cat1.use_count() << "\n";

        std::cout << "My pets:\n";
        for (const auto& pet : my_pets) {
            pet->make_sound();
        }

    } // Animals destroyed when all references go out of scope
}

// =============================================================================
// 8. PERFORMANCE CONSIDERATIONS
// =============================================================================

namespace performance_considerations {

    void make_shared_vs_new() {
        std::cout << "\n=== MAKE_SHARED VS NEW PERFORMANCE ===\n";

        const size_t iterations = 100000;

        // Timing make_shared
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            auto ptr = std::make_shared<int>(42);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto make_shared_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Timing shared_ptr with new
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            std::shared_ptr<int> ptr(new int(42));
        }
        end = std::chrono::high_resolution_clock::now();
        auto new_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Performance comparison (" << iterations << " iterations):\n";
        std::cout << "make_shared: " << make_shared_time.count() << " microseconds\n";
        std::cout << "shared_ptr(new): " << new_time.count() << " microseconds\n";
        std::cout << "make_shared is " <<
                     (double(new_time.count()) / make_shared_time.count()) << "x faster\n";

        std::cout << "\nWhy make_shared is faster:\n";
        std::cout << "- Single allocation for object + control block\n";
        std::cout << "- Better cache locality\n";
        std::cout << "- Exception safe\n";
    }

    void reference_counting_overhead() {
        std::cout << "\n=== REFERENCE COUNTING OVERHEAD ===\n";

        const size_t iterations = 1000000;

        // Raw pointer timing (for comparison)
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            int* raw = new int(42);
            int* copy = raw;  // No ref counting
            delete raw;
            (void)copy;  // Prevent optimization
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto raw_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // shared_ptr timing
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            auto shared = std::make_shared<int>(42);
            auto copy = shared;  // Reference counting overhead
        }
        end = std::chrono::high_resolution_clock::now();
        auto shared_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Reference counting overhead (" << iterations << " iterations):\n";
        std::cout << "Raw pointer: " << raw_time.count() << " microseconds\n";
        std::cout << "shared_ptr:  " << shared_time.count() << " microseconds\n";
        std::cout << "Overhead:    " << (shared_time.count() - raw_time.count()) << " microseconds\n";
        std::cout << "Percentage:  " <<
                     (100.0 * (shared_time.count() - raw_time.count()) / raw_time.count()) << "%\n";

        std::cout << "\nNote: Overhead is acceptable for the safety and convenience gained\n";
    }

    class LargeObject {
        std::vector<double> data;
        std::string description;

    public:
        LargeObject(size_t size, const std::string& desc)
            : data(size, 3.14159), description(desc) {}

        size_t get_size() const { return data.size(); }
        const std::string& get_description() const { return description; }
    };

    void memory_usage_analysis() {
        std::cout << "\n=== MEMORY USAGE ANALYSIS ===\n";

        std::cout << "sizeof(shared_ptr<int>): " << sizeof(std::shared_ptr<int>) << " bytes\n";
        std::cout << "sizeof(unique_ptr<int>): " << sizeof(std::unique_ptr<int>) << " bytes\n";
        std::cout << "sizeof(int*): " << sizeof(int*) << " bytes\n";

        // Control block overhead
        auto shared_int = std::make_shared<int>(42);
        auto unique_int = std::make_unique<int>(42);

        std::cout << "\nshared_ptr includes:\n";
        std::cout << "- Pointer to object\n";
        std::cout << "- Pointer to control block\n";
        std::cout << "- Control block contains: ref count, weak count, deleter\n";

        std::cout << "\nFor large objects, the control block overhead is negligible:\n";
        auto large_shared = std::make_shared<LargeObject>(10000, "LargeSharedObject");
        std::cout << "Large object size: ~" <<
                     (large_shared->get_size() * sizeof(double)) << " bytes\n";
        std::cout << "Control block overhead: ~24-32 bytes (minimal)\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL EXAMPLES
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE EXAMPLES: std::shared_ptr Use Cases\n";
    std::cout << "=============================================================================\n";

    // Basic usage
    basic_shared_ptr::basic_creation_and_usage();
    basic_shared_ptr::reference_counting_demo();

    // Shared ownership
    shared_ownership::shared_ownership_example();

    // Weak pointer examples
    weak_ptr_examples::weak_ptr_cycle_breaking();
    weak_ptr_examples::weak_ptr_observer_pattern();

    // Custom deleters
    custom_deleters_shared::custom_deleter_examples();

    // Thread safety
    thread_safety::thread_safety_example();
    thread_safety::atomic_shared_ptr_example();

    // Factory and caching
    factory_and_caching::factory_caching_example();

    // Polymorphism
    polymorphism_shared::polymorphism_example();

    // Performance considerations
    performance_considerations::make_shared_vs_new();
    performance_considerations::reference_counting_overhead();
    performance_considerations::memory_usage_analysis();

    std::cout << "\n=============================================================================\n";
    std::cout << "KEY TAKEAWAYS:\n";
    std::cout << "1. Use std::make_shared for creation (more efficient than new)\n";
    std::cout << "2. Reference counting enables shared ownership\n";
    std::cout << "3. Use std::weak_ptr to break circular references\n";
    std::cout << "4. Thread-safe reference counting (but not object access)\n";
    std::cout << "5. Perfect for polymorphism and shared resources\n";
    std::cout << "6. Factory patterns and caching benefit from shared ownership\n";
    std::cout << "7. Small overhead compared to safety and convenience\n";
    std::cout << "8. enable_shared_from_this for safe shared_from_this() calls\n";
    std::cout << "=============================================================================\n";

    return 0;
}
