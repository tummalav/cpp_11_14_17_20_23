/*
 * ============================================================================
 * SMART ORDER ROUTER (SOR) - MOODYCAMEL CONCURRENT QUEUE VARIANT
 * ============================================================================
 *
 * Routing-decision fan-out: one strategy/router core decides "route this
 * slice to venue X" and hands the decision off to per-venue gateway threads
 * that own the actual FIX/binary session to each exchange/ECN.
 *
 * Two topologies are shown:
 *
 *  1. MPMC: multiple strategy cores enqueue routing decisions into a single
 *     shared queue consumed by a pool of venue-gateway threads.
 *     -> moodycamel::ConcurrentQueue (lock-free, supports bulk enqueue/dequeue
 *        which matters when routing bursts occur, e.g. a large parent order
 *        sliced into many child orders at once).
 *
 *  2. SPSC-per-venue (preferred when you can shard by venue): the router
 *     shards its output by venue_id so each venue-gateway thread has its own
 *     dedicated single-producer/single-consumer queue -- this turns an MPMC
 *     problem into N independent SPSC problems, which is always faster
 *     (no CAS retry loop) when the topology allows it.
 *     -> moodycamel::ReaderWriterQueue (SPSC).
 *
 * Build (single-header, vendored under third_party/moodycamel/):
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG sor_moodycamel_queue.cpp \
 *       -I../../third_party/moodycamel -lpthread -o sor_moodycamel_queue
 * ============================================================================
 */

#include "concurrentqueue.h"
#include "readerwriterqueue.h"

#include <cstdint>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>

namespace ull::sor::moodycamel_variant {

struct RoutingDecision {
    uint64_t parent_id;
    uint64_t child_id;
    uint32_t venue_id;
    uint32_t qty;
    int64_t  limit_price_ticks;
};

// ----------------------------------------------------------------------
// 1. MPMC: shared queue, pool of venue-gateway consumer threads.
// ----------------------------------------------------------------------
void run_mpmc_demo() {
    moodycamel::ConcurrentQueue<RoutingDecision> queue;
    constexpr int PRODUCERS = 4;
    constexpr int CONSUMERS = 4;
    constexpr int PER_PRODUCER = 250'000;

    std::atomic<uint64_t> consumed{0};
    std::atomic<bool> done{false};

    std::vector<std::thread> producers, consumers;
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < PER_PRODUCER; ++i) {
                queue.enqueue(RoutingDecision{
                    (uint64_t)(p * PER_PRODUCER + i), (uint64_t)(p * PER_PRODUCER + i),
                    (uint32_t)(i % 20), 100u, 10000 + (int64_t)i});
            }
        });
    }
    for (int c = 0; c < CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            RoutingDecision d;
            while (!done.load(std::memory_order_relaxed) || queue.size_approx() > 0) {
                if (queue.try_dequeue(d)) consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : producers) t.join();
    done.store(true, std::memory_order_relaxed);
    for (auto& t : consumers) t.join();

    auto t1 = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::cout << "=== moodycamel MPMC SOR Routing Queue ===\n";
    std::cout << "Producers=" << PRODUCERS << " Consumers=" << CONSUMERS
              << " total=" << (PRODUCERS * PER_PRODUCER) << " consumed=" << consumed.load() << "\n";
    std::cout << "Wall time: " << std::fixed << std::setprecision(1) << us << " us ("
              << (us * 1000.0 / (PRODUCERS * PER_PRODUCER)) << " ns/decision throughput)\n";
}

// ----------------------------------------------------------------------
// 2. SPSC-per-venue: shard by venue_id, one dedicated queue per gateway.
//    Preferred topology whenever the router can pin decisions to a venue
//    up front -- avoids all CAS overhead of the MPMC path.
// ----------------------------------------------------------------------
void run_spsc_per_venue_demo() {
    constexpr int VENUES = 8;
    constexpr int PER_VENUE = 250'000;

    std::vector<moodycamel::ReaderWriterQueue<RoutingDecision>> queues;
    queues.reserve(VENUES);
    for (int v = 0; v < VENUES; ++v) queues.emplace_back(PER_VENUE + 16);

    std::atomic<uint64_t> consumed{0};
    std::vector<std::thread> producers, consumers;
    std::vector<std::atomic<bool>> venue_done(VENUES);
    for (auto& d : venue_done) d.store(false);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int v = 0; v < VENUES; ++v) {
        producers.emplace_back([&, v] {
            for (int i = 0; i < PER_VENUE; ++i) {
                queues[v].enqueue(RoutingDecision{
                    (uint64_t)(v * PER_VENUE + i), (uint64_t)(v * PER_VENUE + i),
                    (uint32_t)v, 100u, 10000 + (int64_t)i});
            }
            venue_done[v].store(true, std::memory_order_release);
        });
    }
    for (int v = 0; v < VENUES; ++v) {
        consumers.emplace_back([&, v] {
            RoutingDecision d;
            while (!venue_done[v].load(std::memory_order_acquire) || queues[v].peek() != nullptr) {
                if (queues[v].try_dequeue(d)) consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    auto t1 = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::cout << "=== moodycamel SPSC-per-venue SOR Routing Queues ===\n";
    std::cout << "Venues=" << VENUES << " total=" << (VENUES * PER_VENUE) << " consumed=" << consumed.load() << "\n";
    std::cout << "Wall time: " << std::fixed << std::setprecision(1) << us << " us ("
              << (us * 1000.0 / (VENUES * PER_VENUE)) << " ns/decision throughput)\n";
}

} // namespace ull::sor::moodycamel_variant

int main() {
    ull::sor::moodycamel_variant::run_mpmc_demo();
    std::cout << "\n";
    ull::sor::moodycamel_variant::run_spsc_per_venue_demo();
    return 0;
}
