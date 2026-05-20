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
#include <cassert>

/*
 * Comprehensive examples of std::weak_ptr use cases
 * Smart pointer for non-owning observation and cycle breaking
 */

// =============================================================================
// 1. BASIC WEAK_PTR USAGE AND OPERATIONS
// =============================================================================

namespace basic_weak_ptr {

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

    void basic_weak_ptr_operations() {
        std::cout << "\n=== BASIC WEAK_PTR OPERATIONS ===\n";

        std::weak_ptr<Resource> weak_resource;

        {
            // Create shared_ptr
            auto shared_resource = std::make_shared<Resource>("WeakResource", 1);
            std::cout << "Shared ref count: " << shared_resource.use_count() << "\n";

            // Assign to weak_ptr (doesn't increase ref count)
            weak_resource = shared_resource;
            std::cout << "After weak assignment, shared ref count: " << shared_resource.use_count() << "\n";
            std::cout << "Weak pointer expired: " << weak_resource.expired() << "\n";
            std::cout << "Weak pointer use_count: " << weak_resource.use_count() << "\n";

            // Try to access through weak_ptr
            if (auto locked = weak_resource.lock()) {
                std::cout << "Successfully locked weak_ptr, using resource:\n";
                locked->use();
                std::cout << "Locked shared_ptr ref count: " << locked.use_count() << "\n";
            } else {
                std::cout << "Failed to lock weak_ptr - resource no longer exists\n";
            }

        } // shared_resource destroyed here

        std::cout << "\nAfter shared_ptr destruction:\n";
        std::cout << "Weak pointer expired: " << weak_resource.expired() << "\n";
        std::cout << "Weak pointer use_count: " << weak_resource.use_count() << "\n";

        // Try to access after resource is destroyed
        if (auto locked = weak_resource.lock()) {
            std::cout << "Successfully locked weak_ptr\n";
        } else {
            std::cout << "Failed to lock weak_ptr - resource no longer exists\n";
        }

        // Reset weak_ptr
        weak_resource.reset();
        std::cout << "After reset, weak pointer expired: " << weak_resource.expired() << "\n";
    }

    void weak_ptr_assignment_and_comparison() {
        std::cout << "\n=== WEAK_PTR ASSIGNMENT AND COMPARISON ===\n";

        auto shared1 = std::make_shared<Resource>("Resource1", 1);
        auto shared2 = std::make_shared<Resource>("Resource2", 2);

        std::weak_ptr<Resource> weak1 = shared1;
        std::weak_ptr<Resource> weak2 = shared2;
        std::weak_ptr<Resource> weak3;

        // Copy assignment
        weak3 = weak1;
        std::cout << "After copy assignment, weak3 points to same resource as weak1\n";

        // Move assignment
        std::weak_ptr<Resource> weak4 = std::move(weak2);
        std::cout << "After move assignment, weak4 has moved from weak2\n";
        std::cout << "weak2 expired after move: " << weak2.expired() << "\n";

        // Comparison
        std::cout << "weak1.owner_before(weak3): " << weak1.owner_before(weak3) << "\n";
        std::cout << "weak3.owner_before(weak1): " << weak3.owner_before(weak1) << "\n";

        // Check if they point to same object
        auto locked1 = weak1.lock();
        auto locked3 = weak3.lock();
        std::cout << "weak1 and weak3 point to same object: " << (locked1.get() == locked3.get()) << "\n";
    }
}

// =============================================================================
// 2. BREAKING CIRCULAR REFERENCES
// =============================================================================

namespace circular_reference_breaking {

    // Example 1: Parent-Child relationship
    class Parent;
    class Child;

    class Child {
    public:
        std::string name;
        std::weak_ptr<Parent> parent;  // Use weak_ptr to avoid cycle

        Child(const std::string& n) : name(n) {
            std::cout << "Child " << name << " created\n";
        }

        ~Child() {
            std::cout << "Child " << name << " destroyed\n";
        }

        void print_parent() const;

        std::shared_ptr<Parent> get_parent() const {
            return parent.lock();
        }
    };

    // Need to inherit from enable_shared_from_this for shared_from_this()
    class Parent : public std::enable_shared_from_this<Parent> {
    public:
        std::string name;
        std::vector<std::shared_ptr<Child>> children;

        Parent(const std::string& n) : name(n) {
            std::cout << "Parent " << name << " created\n";
        }

        ~Parent() {
            std::cout << "Parent " << name << " destroyed\n";
        }

        void add_child(std::shared_ptr<Child> child) {
            children.push_back(child);
            child->parent = shared_from_this();  // Safe to use now
        }

        void print_children() const {
            std::cout << "Parent " << name << " has " << children.size() << " children:\n";
            for (const auto& child : children) {
                std::cout << "  - " << child->name << "\n";
            }
        }
    };

    // Implementation after Parent is fully defined
    void Child::print_parent() const {
        if (auto p = parent.lock()) {
            std::cout << "Child " << name << " has parent: " << p->name << "\n";
        } else {
            std::cout << "Child " << name << " has no parent (or parent destroyed)\n";
        }
    }

    void parent_child_cycle_breaking() {
        std::cout << "\n=== PARENT-CHILD CYCLE BREAKING ===\n";

        {
            auto parent = std::make_shared<Parent>("Dad");
            auto child1 = std::make_shared<Child>("Alice");
            auto child2 = std::make_shared<Child>("Bob");

            std::cout << "Initial ref counts:\n";
            std::cout << "Parent: " << parent.use_count() << "\n";
            std::cout << "Child1: " << child1.use_count() << "\n";
            std::cout << "Child2: " << child2.use_count() << "\n";

            parent->add_child(child1);
            parent->add_child(child2);

            std::cout << "\nAfter adding children:\n";
            std::cout << "Parent: " << parent.use_count() << "\n";
            std::cout << "Child1: " << child1.use_count() << "\n";
            std::cout << "Child2: " << child2.use_count() << "\n";

            parent->print_children();
            child1->print_parent();
            child2->print_parent();

            // Test accessing parent through child
            auto parent_through_child = child1->get_parent();
            if (parent_through_child) {
                std::cout << "Accessed parent through child: " << parent_through_child->name << "\n";
                std::cout << "Parent ref count: " << parent_through_child.use_count() << "\n";
            }

        } // All objects destroyed properly due to weak_ptr breaking cycle

        std::cout << "All objects destroyed successfully!\n";
    }

    // Example 2: Doubly linked list with weak_ptr
    class ListNode {
    public:
        int data;
        std::shared_ptr<ListNode> next;
        std::weak_ptr<ListNode> prev;  // Use weak_ptr for backward reference

        ListNode(int value) : data(value) {
            std::cout << "ListNode " << data << " created\n";
        }

        ~ListNode() {
            std::cout << "ListNode " << data << " destroyed\n";
        }

        void print_connections() const {
            std::cout << "Node " << data << ":";
            if (auto p = prev.lock()) {
                std::cout << " prev=" << p->data;
            } else {
                std::cout << " prev=null";
            }
            if (next) {
                std::cout << " next=" << next->data;
            } else {
                std::cout << " next=null";
            }
            std::cout << "\n";
        }
    };

    void doubly_linked_list_example() {
        std::cout << "\n=== DOUBLY LINKED LIST WITH WEAK_PTR ===\n";

        auto node1 = std::make_shared<ListNode>(1);
        auto node2 = std::make_shared<ListNode>(2);
        auto node3 = std::make_shared<ListNode>(3);

        // Connect nodes
        node1->next = node2;
        node2->prev = node1;
        node2->next = node3;
        node3->prev = node2;

        std::cout << "Linked list structure:\n";
        node1->print_connections();
        node2->print_connections();
        node3->print_connections();

        std::cout << "\nReference counts:\n";
        std::cout << "Node1: " << node1.use_count() << "\n";
        std::cout << "Node2: " << node2.use_count() << "\n";
        std::cout << "Node3: " << node3.use_count() << "\n";

        // Navigate backwards using weak_ptr
        auto current = node3;
        std::cout << "\nNavigating backwards from node3:\n";
        while (current) {
            std::cout << "Current node: " << current->data << "\n";
            current = current->prev.lock();  // Get previous node safely
        }

    } // All nodes destroyed properly
}

// =============================================================================
// 3. OBSERVER PATTERN WITH WEAK_PTR
// =============================================================================

namespace observer_pattern {

    class Subject;

    class Observer {
    protected:
        std::string name_;
        std::weak_ptr<Subject> subject_;

    public:
        Observer(const std::string& name) : name_(name) {
            std::cout << "Observer " << name_ << " created\n";
        }

        virtual ~Observer() {
            std::cout << "Observer " << name_ << " destroyed\n";
        }

        virtual void update(const std::string& message) = 0;

        void observe(std::shared_ptr<Subject> subject) {
            subject_ = subject;
        }

        bool is_observing() const {
            return !subject_.expired();
        }

        std::shared_ptr<Subject> get_subject() const {
            return subject_.lock();
        }

        const std::string& get_name() const { return name_; }
    };

    class ConcreteObserver : public Observer {
    public:
        ConcreteObserver(const std::string& name) : Observer(name) {}

        void update(const std::string& message) override {
            std::cout << "Observer " << name_ << " received: " << message << "\n";
        }
    };

    class Subject : public std::enable_shared_from_this<Subject> {
        std::vector<std::weak_ptr<Observer>> observers_;
        std::string name_;
        std::string state_;

    public:
        Subject(const std::string& name) : name_(name) {
            std::cout << "Subject " << name_ << " created\n";
        }

        ~Subject() {
            std::cout << "Subject " << name_ << " destroyed\n";
        }

        void add_observer(std::shared_ptr<Observer> observer) {
            observers_.push_back(observer);
            observer->observe(shared_from_this());
            std::cout << "Added observer " << observer->get_name() << " to subject " << name_ << "\n";
        }

        void remove_observer(std::shared_ptr<Observer> observer) {
            observers_.erase(
                std::remove_if(observers_.begin(), observers_.end(),
                    [&observer](const std::weak_ptr<Observer>& weak_obs) {
                        if (auto obs = weak_obs.lock()) {
                            return obs.get() == observer.get();
                        }
                        return true; // Remove expired observers too
                    }),
                observers_.end());
            std::cout << "Removed observer " << observer->get_name() << " from subject " << name_ << "\n";
        }

        void set_state(const std::string& new_state) {
            state_ = new_state;
            notify_observers("State changed to: " + state_);
        }

        void notify_observers(const std::string& message) {
            std::cout << "Subject " << name_ << " notifying observers: " << message << "\n";

            // Clean up expired observers while notifying
            auto it = observers_.begin();
            while (it != observers_.end()) {
                if (auto observer = it->lock()) {
                    observer->update(message);
                    ++it;
                } else {
                    std::cout << "Removing expired observer from subject " << name_ << "\n";
                    it = observers_.erase(it);
                }
            }
        }

        size_t observer_count() const {
            size_t count = 0;
            for (const auto& weak_obs : observers_) {
                if (!weak_obs.expired()) {
                    ++count;
                }
            }
            return count;
        }

        const std::string& get_name() const { return name_; }
        const std::string& get_state() const { return state_; }
    };

    void observer_pattern_example() {
        std::cout << "\n=== OBSERVER PATTERN WITH WEAK_PTR ===\n";

        auto subject = std::make_shared<Subject>("NewsPublisher");
        auto obs1 = std::make_shared<ConcreteObserver>("Reader1");
        auto obs2 = std::make_shared<ConcreteObserver>("Reader2");
        auto obs3 = std::make_shared<ConcreteObserver>("Reader3");

        // Add observers
        subject->add_observer(obs1);
        subject->add_observer(obs2);
        subject->add_observer(obs3);

        std::cout << "Observer count: " << subject->observer_count() << "\n";

        // Notify all observers
        subject->set_state("Breaking News!");

        // Remove one observer explicitly
        subject->remove_observer(obs2);
        std::cout << "Observer count after removal: " << subject->observer_count() << "\n";

        // Another notification
        subject->set_state("Weather Update");

        // Destroy an observer (simulating observer going out of scope)
        obs1.reset();
        std::cout << "Observer1 destroyed, count before cleanup: " << subject->observer_count() << "\n";

        // Next notification will clean up expired observer
        subject->set_state("Sports News");
        std::cout << "Observer count after automatic cleanup: " << subject->observer_count() << "\n";

        // Test observer trying to access destroyed subject
        std::weak_ptr<Subject> weak_subject = subject;
        subject.reset();  // Destroy subject

        if (obs3->is_observing()) {
            std::cout << "Observer3 thinks it's still observing\n";
        } else {
            std::cout << "Observer3 knows subject is destroyed\n";
        }

        auto subject_ptr = obs3->get_subject();
        if (subject_ptr) {
            std::cout << "Observer3 can still access subject\n";
        } else {
            std::cout << "Observer3 cannot access destroyed subject\n";
        }
    }
}

// =============================================================================
// 4. CACHE IMPLEMENTATION WITH WEAK_PTR
// =============================================================================

namespace cache_implementation {

    class ExpensiveObject {
        std::string id_;
        std::vector<double> data_;

    public:
        ExpensiveObject(const std::string& id, size_t size) : id_(id), data_(size) {
            std::cout << "Creating expensive object: " << id_ << " (size: " << size << ")\n";

            // Simulate expensive computation
            for (size_t i = 0; i < size; ++i) {
                data_[i] = i * 3.14159;
            }

            // Simulate time-consuming operation
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ~ExpensiveObject() {
            std::cout << "Destroying expensive object: " << id_ << "\n";
        }

        const std::string& get_id() const { return id_; }
        double get_value(size_t index) const {
            return index < data_.size() ? data_[index] : 0.0;
        }
        size_t get_size() const { return data_.size(); }

        void use() const {
            std::cout << "Using expensive object: " << id_ << "\n";
        }
    };

    class WeakPtrCache {
        std::unordered_map<std::string, std::weak_ptr<ExpensiveObject>> cache_;
        mutable std::mutex mutex_;

    public:
        std::shared_ptr<ExpensiveObject> get_object(const std::string& id, size_t size = 100) {
            std::lock_guard<std::mutex> lock(mutex_);

            // Check if object exists in cache and is still alive
            auto it = cache_.find(id);
            if (it != cache_.end()) {
                if (auto existing = it->second.lock()) {
                    std::cout << "Cache hit: returning cached object " << id << "\n";
                    return existing;
                } else {
                    std::cout << "Cache entry expired for " << id << ", removing\n";
                    cache_.erase(it);
                }
            }

            // Cache miss - create new object
            std::cout << "Cache miss: creating new object " << id << "\n";
            auto new_object = std::make_shared<ExpensiveObject>(id, size);
            cache_[id] = new_object;

            return new_object;
        }

        void cleanup_expired() {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto it = cache_.begin(); it != cache_.end();) {
                if (it->second.expired()) {
                    std::cout << "Cleaning up expired cache entry: " << it->first << "\n";
                    it = cache_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return cache_.size();
        }

        size_t active_objects() const {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (const auto& pair : cache_) {
                if (!pair.second.expired()) {
                    ++count;
                }
            }
            return count;
        }

        void print_cache_status() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::cout << "Cache status - Total entries: " << cache_.size()
                      << ", Active objects: " << active_objects() << "\n";
        }
    };

    void cache_example() {
        std::cout << "\n=== CACHE IMPLEMENTATION WITH WEAK_PTR ===\n";

        WeakPtrCache cache;

        // Request objects multiple times
        {
            auto obj1 = cache.get_object("Object_A", 50);
            cache.print_cache_status();

            auto obj2 = cache.get_object("Object_A", 50);  // Should hit cache
            std::cout << "obj1 == obj2: " << (obj1.get() == obj2.get()) << "\n";

            auto obj3 = cache.get_object("Object_B", 75);
            cache.print_cache_status();

            // Use objects
            obj1->use();
            obj3->use();

            std::cout << "Reference counts - obj1: " << obj1.use_count()
                      << ", obj2: " << obj2.use_count() << ", obj3: " << obj3.use_count() << "\n";

            // Release some references
            obj2.reset();
            std::cout << "After releasing obj2, obj1 ref count: " << obj1.use_count() << "\n";

        } // obj1 and obj3 go out of scope here

        std::cout << "\nAfter objects go out of scope:\n";
        cache.print_cache_status();

        // Try to get objects again - should create new ones since old ones are destroyed
        auto obj4 = cache.get_object("Object_A", 50);
        cache.print_cache_status();

        // Cleanup expired entries
        cache.cleanup_expired();
        cache.print_cache_status();

        // Keep one object alive
        {
            auto obj5 = cache.get_object("Object_C", 25);
            cache.print_cache_status();

            // Cleanup while object is still alive
            cache.cleanup_expired();
            cache.print_cache_status();

        } // obj5 destroyed here

        std::cout << "\nFinal cleanup:\n";
        cache.cleanup_expired();
        cache.print_cache_status();
    }
}

// =============================================================================
// 5. THREAD-SAFE WEAK_PTR USAGE
// =============================================================================

namespace thread_safe_weak_ptr {

    class SharedResource {
        std::string name_;
        std::atomic<int> usage_count_{0};
        mutable std::mutex mutex_;

    public:
        SharedResource(const std::string& name) : name_(name) {
            std::cout << "SharedResource " << name_ << " created\n";
        }

        ~SharedResource() {
            std::cout << "SharedResource " << name_ << " destroyed (used "
                      << usage_count_.load() << " times)\n";
        }

        void use() {
            std::lock_guard<std::mutex> lock(mutex_);
            ++usage_count_;
            std::cout << "Thread " << std::this_thread::get_id()
                      << " using " << name_ << " (usage #" << usage_count_.load() << ")\n";

            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        const std::string& get_name() const { return name_; }
        int get_usage_count() const { return usage_count_.load(); }
    };

    class ResourceManager {
        std::weak_ptr<SharedResource> resource_;
        std::string manager_name_;

    public:
        ResourceManager(const std::string& name) : manager_name_(name) {}

        void set_resource(std::shared_ptr<SharedResource> resource) {
            resource_ = resource;
            std::cout << "Manager " << manager_name_ << " assigned resource "
                      << resource->get_name() << "\n";
        }

        bool try_use_resource() {
            if (auto resource = resource_.lock()) {
                std::cout << "Manager " << manager_name_ << " acquired resource\n";
                resource->use();
                return true;
            } else {
                std::cout << "Manager " << manager_name_ << " - resource no longer available\n";
                return false;
            }
        }

        bool is_resource_available() const {
            return !resource_.expired();
        }

        const std::string& get_name() const { return manager_name_; }
    };

    void worker_thread(std::shared_ptr<ResourceManager> manager, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            if (manager->try_use_resource()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else {
                std::cout << "Worker for " << manager->get_name()
                          << " stopping - resource unavailable\n";
                break;
            }
        }
    }

    void thread_safe_example() {
        std::cout << "\n=== THREAD-SAFE WEAK_PTR USAGE ===\n";

        auto manager1 = std::make_shared<ResourceManager>("Manager1");
        auto manager2 = std::make_shared<ResourceManager>("Manager2");
        auto manager3 = std::make_shared<ResourceManager>("Manager3");

        std::vector<std::thread> threads;

        {
            auto shared_resource = std::make_shared<SharedResource>("ThreadSafeResource");

            // Assign resource to managers
            manager1->set_resource(shared_resource);
            manager2->set_resource(shared_resource);
            manager3->set_resource(shared_resource);

            std::cout << "Resource ref count: " << shared_resource.use_count() << "\n";

            // Start worker threads
            threads.emplace_back(worker_thread, manager1, 3);
            threads.emplace_back(worker_thread, manager2, 3);
            threads.emplace_back(worker_thread, manager3, 3);

            // Let threads run for a while
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::cout << "\nDestroying shared resource...\n";
        } // shared_resource destroyed here

        // Wait for threads to complete
        std::cout << "Waiting for worker threads to complete...\n";
        for (auto& t : threads) {
            t.join();
        }

        std::cout << "\nChecking manager states after resource destruction:\n";
        std::cout << "Manager1 resource available: " << manager1->is_resource_available() << "\n";
        std::cout << "Manager2 resource available: " << manager2->is_resource_available() << "\n";
        std::cout << "Manager3 resource available: " << manager3->is_resource_available() << "\n";
    }
}

// =============================================================================
// 6. WEAK_PTR IN CALLBACK SYSTEMS
// =============================================================================

namespace callback_systems {

    class EventHandler {
        std::string name_;

    public:
        EventHandler(const std::string& name) : name_(name) {
            std::cout << "EventHandler " << name_ << " created\n";
        }

        ~EventHandler() {
            std::cout << "EventHandler " << name_ << " destroyed\n";
        }

        void handle_event(const std::string& event) {
            std::cout << "Handler " << name_ << " processing event: " << event << "\n";
        }

        const std::string& get_name() const { return name_; }
    };

    class EventSystem {
        struct CallbackInfo {
            std::weak_ptr<EventHandler> handler;
            std::string event_type;

            CallbackInfo(std::weak_ptr<EventHandler> h, const std::string& type)
                : handler(h), event_type(type) {}
        };

        std::vector<CallbackInfo> callbacks_;
        mutable std::mutex mutex_;

    public:
        void register_handler(std::shared_ptr<EventHandler> handler, const std::string& event_type) {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_.emplace_back(handler, event_type);
            std::cout << "Registered handler " << handler->get_name()
                      << " for event type: " << event_type << "\n";
        }

        void unregister_handler(std::shared_ptr<EventHandler> handler, const std::string& event_type) {
            std::lock_guard<std::mutex> lock(mutex_);

            callbacks_.erase(
                std::remove_if(callbacks_.begin(), callbacks_.end(),
                    [&](const CallbackInfo& info) {
                        if (auto h = info.handler.lock()) {
                            return h.get() == handler.get() && info.event_type == event_type;
                        }
                        return true; // Remove expired handlers too
                    }),
                callbacks_.end());

            std::cout << "Unregistered handler " << handler->get_name()
                      << " from event type: " << event_type << "\n";
        }

        void fire_event(const std::string& event_type, const std::string& event_data) {
            std::lock_guard<std::mutex> lock(mutex_);

            std::cout << "Firing event: " << event_type << " with data: " << event_data << "\n";

            // Process callbacks and clean up expired ones
            auto it = callbacks_.begin();
            while (it != callbacks_.end()) {
                if (it->event_type == event_type) {
                    if (auto handler = it->handler.lock()) {
                        handler->handle_event(event_data);
                        ++it;
                    } else {
                        std::cout << "Removing expired handler for event: " << event_type << "\n";
                        it = callbacks_.erase(it);
                    }
                } else {
                    ++it;
                }
            }
        }

        void cleanup_expired_handlers() {
            std::lock_guard<std::mutex> lock(mutex_);

            size_t initial_size = callbacks_.size();
            callbacks_.erase(
                std::remove_if(callbacks_.begin(), callbacks_.end(),
                    [](const CallbackInfo& info) {
                        return info.handler.expired();
                    }),
                callbacks_.end());

            size_t removed = initial_size - callbacks_.size();
            if (removed > 0) {
                std::cout << "Cleaned up " << removed << " expired handlers\n";
            }
        }

        size_t handler_count() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return callbacks_.size();
        }

        size_t active_handler_count() const {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (const auto& info : callbacks_) {
                if (!info.handler.expired()) {
                    ++count;
                }
            }
            return count;
        }
    };

    void callback_system_example() {
        std::cout << "\n=== CALLBACK SYSTEM WITH WEAK_PTR ===\n";

        EventSystem event_system;

        auto handler1 = std::make_shared<EventHandler>("NetworkHandler");
        auto handler2 = std::make_shared<EventHandler>("FileHandler");
        auto handler3 = std::make_shared<EventHandler>("UIHandler");

        // Register handlers for different events
        event_system.register_handler(handler1, "network_event");
        event_system.register_handler(handler2, "file_event");
        event_system.register_handler(handler3, "ui_event");
        event_system.register_handler(handler1, "ui_event");  // Handler can handle multiple event types

        std::cout << "Total handlers: " << event_system.handler_count() << "\n";
        std::cout << "Active handlers: " << event_system.active_handler_count() << "\n";

        // Fire some events
        event_system.fire_event("network_event", "Connection established");
        event_system.fire_event("ui_event", "Button clicked");
        event_system.fire_event("file_event", "File opened");

        // Destroy one handler
        handler2.reset();
        std::cout << "\nAfter destroying FileHandler:\n";
        std::cout << "Total handlers: " << event_system.handler_count() << "\n";
        std::cout << "Active handlers: " << event_system.active_handler_count() << "\n";

        // Fire events again - expired handler will be cleaned up
        event_system.fire_event("file_event", "File saved");
        event_system.fire_event("ui_event", "Menu selected");

        std::cout << "After automatic cleanup:\n";
        std::cout << "Total handlers: " << event_system.handler_count() << "\n";
        std::cout << "Active handlers: " << event_system.active_handler_count() << "\n";

        // Explicit cleanup
        event_system.cleanup_expired_handlers();
        std::cout << "After explicit cleanup:\n";
        std::cout << "Total handlers: " << event_system.handler_count() << "\n";

        // Unregister a handler explicitly
        event_system.unregister_handler(handler1, "network_event");
        std::cout << "After unregistering NetworkHandler from network_event:\n";
        std::cout << "Total handlers: " << event_system.handler_count() << "\n";

        // Final event
        event_system.fire_event("ui_event", "Application closing");
    }
}

// =============================================================================
// 7. PERFORMANCE CONSIDERATIONS AND BEST PRACTICES
// =============================================================================

namespace performance_and_best_practices {

    void weak_ptr_vs_raw_pointer_performance() {
        std::cout << "\n=== WEAK_PTR VS RAW POINTER PERFORMANCE ===\n";

        const size_t iterations = 1000000;

        // Setup shared_ptr for weak_ptr test
        auto shared_obj = std::make_shared<int>(42);
        std::weak_ptr<int> weak_obj = shared_obj;
        int* raw_obj = shared_obj.get();

        // Timing weak_ptr.lock()
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            if (auto locked = weak_obj.lock()) {
                volatile int value = *locked;  // Prevent optimization
                (void)value;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto weak_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Timing raw pointer access
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            if (raw_obj) {  // Similar null check
                volatile int value = *raw_obj;  // Prevent optimization
                (void)value;
            }
        }
        end = std::chrono::high_resolution_clock::now();
        auto raw_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Performance comparison (" << iterations << " iterations):\n";
        std::cout << "weak_ptr.lock(): " << weak_time.count() << " microseconds\n";
        std::cout << "raw pointer:     " << raw_time.count() << " microseconds\n";
        std::cout << "Overhead factor: " <<
                     (double(weak_time.count()) / raw_time.count()) << "x\n";

        std::cout << "\nNote: weak_ptr provides safety at the cost of performance\n";
        std::cout << "Use weak_ptr when safety is more important than raw speed\n";
    }

    void best_practices_examples() {
        std::cout << "\n=== WEAK_PTR BEST PRACTICES ===\n";

        std::cout << "1. ALWAYS check if weak_ptr can be locked before use:\n";
        {
            std::weak_ptr<int> weak_int;
            // GOOD:
            if (auto shared_int = weak_int.lock()) {
                std::cout << "Safe access: " << *shared_int << "\n";
            } else {
                std::cout << "Object no longer exists\n";
            }

            // BAD: Don't do this
            // auto shared_int = weak_int.lock();
            // *shared_int = 42;  // Potential null pointer dereference
        }

        std::cout << "\n2. Cache the locked shared_ptr if using multiple times:\n";
        {
            std::weak_ptr<std::vector<int>> weak_vec;
            auto shared_vec = std::make_shared<std::vector<int>>(10, 42);
            weak_vec = shared_vec;
            shared_vec.reset();  // Simulate destruction

            // GOOD: Lock once and reuse
            if (auto locked_vec = weak_vec.lock()) {
                std::cout << "Vector size: " << locked_vec->size() << "\n";
                std::cout << "First element: " << (*locked_vec)[0] << "\n";
                // More operations with locked_vec...
            }

            // BAD: Multiple locks (inefficient and potentially unsafe)
            // if (!weak_vec.expired() && weak_vec.lock()) {
            //     auto vec1 = weak_vec.lock();  // First lock
            //     auto vec2 = weak_vec.lock();  // Second lock (wasteful)
            //     // Use vec1 and vec2...
            // }
        }

        std::cout << "\n3. Use weak_ptr for breaking cycles in data structures:\n";
        std::cout << "   - Parent-child relationships\n";
        std::cout << "   - Doubly-linked lists\n";
        std::cout << "   - Graph structures\n";
        std::cout << "   - Observer patterns\n";

        std::cout << "\n4. Use weak_ptr for non-owning references:\n";
        std::cout << "   - Caches that don't control object lifetime\n";
        std::cout << "   - Callback systems\n";
        std::cout << "   - Observer patterns\n";
        std::cout << "   - Temporary references\n";

        std::cout << "\n5. Regularly clean up expired weak_ptr references:\n";
        std::cout << "   - In containers holding weak_ptr\n";
        std::cout << "   - In callback/observer systems\n";
        std::cout << "   - In cache implementations\n";

        std::cout << "\n6. Thread safety considerations:\n";
        std::cout << "   - weak_ptr operations are thread-safe\n";
        std::cout << "   - But the pointed-to object access is not\n";
        std::cout << "   - Use proper synchronization for object access\n";
    }

    void common_pitfalls() {
        std::cout << "\n=== COMMON WEAK_PTR PITFALLS ===\n";

        std::cout << "1. PITFALL: Not checking if weak_ptr can be locked\n";
        {
            std::weak_ptr<int> weak_int;
            // This could crash:
            // auto shared_int = weak_int.lock();
            // int value = *shared_int;  // Null pointer dereference!

            // Correct approach:
            if (auto shared_int = weak_int.lock()) {
                int value = *shared_int;
                std::cout << "   Safe access: " << value << "\n";
            } else {
                std::cout << "   Object no longer exists - avoided crash!\n";
            }
        }

        std::cout << "\n2. PITFALL: Race condition between expired() and lock()\n";
        {
            std::weak_ptr<int> weak_int;
            // This is NOT thread-safe:
            // if (!weak_int.expired()) {
            //     auto shared_int = weak_int.lock();  // Could fail if object destroyed here
            //     *shared_int = 42;  // Potential crash
            // }

            // Correct approach:
            if (auto shared_int = weak_int.lock()) {
                // Use shared_int safely
                std::cout << "   Thread-safe access\n";
            }
        }

        std::cout << "\n3. PITFALL: Creating cycles with shared_ptr instead of weak_ptr\n";
        std::cout << "   Always use weak_ptr for back-references to avoid memory leaks\n";

        std::cout << "\n4. PITFALL: Excessive locking in tight loops\n";
        std::cout << "   Cache the locked shared_ptr instead of locking repeatedly\n";

        std::cout << "\n5. PITFALL: Forgetting to clean up expired weak_ptr in containers\n";
        std::cout << "   This can lead to memory waste and degraded performance\n";
    }
}

// =============================================================================
// MAIN FUNCTION - DEMONSTRATING ALL EXAMPLES
// =============================================================================

int main() {
    std::cout << "=============================================================================\n";
    std::cout << "COMPREHENSIVE EXAMPLES: std::weak_ptr Use Cases\n";
    std::cout << "=============================================================================\n";

    // Basic weak_ptr operations
    basic_weak_ptr::basic_weak_ptr_operations();
    basic_weak_ptr::weak_ptr_assignment_and_comparison();

    // Breaking circular references
    circular_reference_breaking::parent_child_cycle_breaking();
    circular_reference_breaking::doubly_linked_list_example();

    // Observer pattern
    observer_pattern::observer_pattern_example();

    // Cache implementation
    cache_implementation::cache_example();

    // Thread safety
    thread_safe_weak_ptr::thread_safe_example();

    // Callback systems
    callback_systems::callback_system_example();

    // Performance and best practices
    performance_and_best_practices::weak_ptr_vs_raw_pointer_performance();
    performance_and_best_practices::best_practices_examples();
    performance_and_best_practices::common_pitfalls();

    std::cout << "\n=============================================================================\n";
    std::cout << "KEY TAKEAWAYS:\n";
    std::cout << "1. weak_ptr provides non-owning observation of shared_ptr objects\n";
    std::cout << "2. Essential for breaking circular references and avoiding memory leaks\n";
    std::cout << "3. Always use lock() to safely access the pointed-to object\n";
    std::cout << "4. Perfect for observer patterns and callback systems\n";
    std::cout << "5. Enables safe caching without controlling object lifetime\n";
    std::cout << "6. Thread-safe operations but pointed-to object access needs synchronization\n";
    std::cout << "7. Regular cleanup of expired weak_ptr prevents resource waste\n";
    std::cout << "8. Small performance overhead compared to safety benefits\n";
    std::cout << "=============================================================================\n";

    return 0;
}
