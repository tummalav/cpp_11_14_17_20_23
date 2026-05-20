import java.util.*;
import java.util.concurrent.*;

public class CollectionsDemo {

    public static void main(String[] args) {

        // ── 1. NORMAL COLLECTIONS ──────────────────────────────────────
        List<String>        arrayList       = new ArrayList<>();
        List<String>        linkedList      = new LinkedList<>();
        Set<String>         hashSet         = new HashSet<>();
        Set<String>         linkedHashSet   = new LinkedHashSet<>();
        Set<String>         treeSet         = new TreeSet<>();
        Map<String, String> hashMap         = new HashMap<>();
        Map<String, String> linkedHashMap   = new LinkedHashMap<>();
        Map<String, String> treeMap         = new TreeMap<>();
        Queue<String>       priorityQueue   = new PriorityQueue<>();
        Deque<String>       arrayDeque      = new ArrayDeque<>();

        // ── 2. SYNCHRONIZED COLLECTIONS ───────────────────────────────
        List<String>        syncList        = Collections.synchronizedList(new ArrayList<>());
        Set<String>         syncSet         = Collections.synchronizedSet(new HashSet<>());
        Set<String>         syncSortedSet   = Collections.synchronizedSortedSet(new TreeSet<>());
        Map<String, String> syncMap         = Collections.synchronizedMap(new HashMap<>());
        Map<String, String> syncSortedMap   = Collections.synchronizedSortedMap(new TreeMap<>());
        Collection<String>  syncCollection  = Collections.synchronizedCollection(new ArrayList<>());

        // Must manually synchronize when iterating!
        syncList.add("A");
        synchronized (syncList) {
            for (String s : syncList) {
                System.out.println("syncList item: " + s);
            }
        }

        // ── 3. CONCURRENT COLLECTIONS ──────────────────────────────────

        // Lists
        CopyOnWriteArrayList<String>   cowList         = new CopyOnWriteArrayList<>();

        // Sets
        CopyOnWriteArraySet<String>    cowSet          = new CopyOnWriteArraySet<>();
        ConcurrentSkipListSet<String>  skipListSet     = new ConcurrentSkipListSet<>();

        // Maps
        ConcurrentHashMap<String, String>      concurrentMap     = new ConcurrentHashMap<>();
        ConcurrentSkipListMap<String, String>  skipListMap       = new ConcurrentSkipListMap<>();

        // Queues
        ConcurrentLinkedQueue<String>  concurrentQueue  = new ConcurrentLinkedQueue<>();
        ConcurrentLinkedDeque<String>  concurrentDeque  = new ConcurrentLinkedDeque<>();

        // Blocking Queues
        ArrayBlockingQueue<String>     arrayBQ          = new ArrayBlockingQueue<>(100);
        LinkedBlockingQueue<String>    linkedBQ         = new LinkedBlockingQueue<>();
        LinkedBlockingDeque<String>    linkedBD         = new LinkedBlockingDeque<>();
        PriorityBlockingQueue<String>  priorityBQ       = new PriorityBlockingQueue<>();
        DelayQueue<DelayedTask>        delayQueue       = new DelayQueue<>();
        SynchronousQueue<String>       synchronousQ     = new SynchronousQueue<>();
        LinkedTransferQueue<String>    transferQueue     = new LinkedTransferQueue<>();

        // Example: ConcurrentHashMap usage
        concurrentMap.put("key1", "value1");
        concurrentMap.putIfAbsent("key2", "value2");
        concurrentMap.computeIfAbsent("key3", k -> k + "_computed");
        System.out.println("ConcurrentHashMap: " + concurrentMap);

        // Example: CopyOnWriteArrayList — safe iteration without locking
        cowList.add("X");
        cowList.add("Y");
        for (String s : cowList) {          // snapshot, never throws ConcurrentModificationException
            System.out.println("COW item: " + s);
        }

        // Example: LinkedBlockingQueue producer-consumer
        Thread producer = new Thread(() -> {
            try {
                linkedBQ.put("task1");
                linkedBQ.put("task2");
            } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        });
        Thread consumer = new Thread(() -> {
            try {
                System.out.println("Consumed: " + linkedBQ.take());
                System.out.println("Consumed: " + linkedBQ.take());
            } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        });
        producer.start();
        consumer.start();
    }

    // ── Helper: Delayed element for DelayQueue ─────────────────────────
    static class DelayedTask implements Delayed {
        private final String name;
        private final long   startTime; // ms

        DelayedTask(String name, long delayMs) {
            this.name      = name;
            this.startTime = System.currentTimeMillis() + delayMs;
        }

        @Override
        public long getDelay(TimeUnit unit) {
            return unit.convert(startTime - System.currentTimeMillis(), TimeUnit.MILLISECONDS);
        }

        @Override
        public int compareTo(Delayed o) {
            return Long.compare(this.startTime, ((DelayedTask) o).startTime);
        }

        @Override public String toString() { return name; }
    }
}

