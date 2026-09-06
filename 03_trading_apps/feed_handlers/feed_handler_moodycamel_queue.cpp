/*
 * ============================================================================
 * MARKET DATA FEED HANDLER - MOODYCAMEL QUEUE VARIANT (NIC -> decode threads)
 * ============================================================================
 *
 * Companion to the in-house SPSC ring buffers already used in this repo
 * (see `orderbook/ull_orderbook.cpp` SPSCQueue and
 * `02_ultra_low_latency/lockfree/ringbuffer_all_variants_capital_markets.cpp`).
 * This file shows the two moodycamel queue topologies for handing raw
 * packets from NIC RX to decode threads:
 *
 *  1. MPSC via moodycamel::ConcurrentQueue: multiple NIC RX queues/cores
 *     (e.g. multicast feed A + feed B for redundancy, or multiple exchange
 *     multicast groups) feed a single decode thread (or small pool). Handles
 *     bursty message rates (options market data bursts) well via bulk
 *     enqueue/dequeue.
 *
 *  2. SPSC via moodycamel::ReaderWriterQueue: one NIC RX ring -> one
 *     dedicated decode thread. Lowest latency, no CAS -- use this whenever
 *     you can dedicate a core per feed instead of sharing a decode pool.
 *
 * Build:
 *   g++ -std=c++17 -O3 -march=native -DNDEBUG feed_handler_moodycamel_queue.cpp \
 *       -I../../third_party/moodycamel -lpthread -o feed_handler_moodycamel_queue
 * ============================================================================
 */

#include "concurrentqueue.h"
#include "readerwriterqueue.h"

#include <cstdint>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>

namespace ull::feed_handlers::moodycamel_variant {

// Raw market-data packet as it arrives off the NIC (fixed size, no allocation).
struct alignas(64) RawPacket {
    uint64_t seq_num;
    uint32_t symbol_id;
    uint32_t length;
    uint8_t  payload[48];  // ITCH/OUCH/MDP3 message bytes (truncated for demo)
};
static_assert(sizeof(RawPacket) == 64, "RawPacket must be exactly one cache line");

// ----------------------------------------------------------------------
// 1. MPSC: N NIC RX threads -> shared moodycamel::ConcurrentQueue -> pool of
//    decode threads. Use when feeds are cross-wired for redundancy/failover
//    (e.g. line A + line B of the same multicast feed) and a shared decode
//    pool amortizes CPU better than dedicating a core per feed line.
// ----------------------------------------------------------------------
void run_mpsc_demo() {
    moodycamel::ConcurrentQueue<RawPacket> queue;
    constexpr int NIC_LINES = 2;     // e.g. redundant multicast feed A/B
    constexpr int DECODERS = 2;
    constexpr int PACKETS_PER_LINE = 500'000;

    std::atomic<uint64_t> decoded{0};
    std::atomic<bool> done{false};

    std::vector<std::thread> rx_threads, decode_threads;
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int line = 0; line < NIC_LINES; ++line) {
        rx_threads.emplace_back([&, line] {
            for (uint32_t i = 0; i < PACKETS_PER_LINE; ++i) {
                RawPacket pkt{};
                pkt.seq_num = i;
                pkt.symbol_id = i % 4096;
                pkt.length = 32;
                queue.enqueue(pkt);
            }
        });
    }
    for (int d = 0; d < DECODERS; ++d) {
        decode_threads.emplace_back([&] {
            RawPacket pkt;
            while (!done.load(std::memory_order_relaxed) || queue.size_approx() > 0) {
                if (queue.try_dequeue(pkt)) decoded.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : rx_threads) t.join();
    done.store(true, std::memory_order_relaxed);
    for (auto& t : decode_threads) t.join();

    auto t1 = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    auto total = NIC_LINES * PACKETS_PER_LINE;

    std::cout << "=== moodycamel MPSC Feed Handler (NIC lines -> decode pool) ===\n";
    std::cout << "NIC lines=" << NIC_LINES << " decoders=" << DECODERS
              << " total=" << total << " decoded=" << decoded.load() << "\n";
    std::cout << "Wall time: " << std::fixed << std::setprecision(1) << us << " us ("
              << (us * 1000.0 / total) << " ns/packet throughput)\n";
}

// ----------------------------------------------------------------------
// 2. SPSC: one NIC RX thread -> one dedicated decode thread. Preferred when
//    a core can be pinned per feed -- no CAS loop, lowest and most
//    predictable per-packet latency (matches the in-house SPSC ring design
//    used for order ingress in `ull_orderbook.cpp`).
// ----------------------------------------------------------------------
void run_spsc_demo() {
    constexpr int PACKETS = 1'000'000;
    moodycamel::ReaderWriterQueue<RawPacket> queue(PACKETS + 16);

    std::atomic<uint64_t> decoded{0};
    std::atomic<bool> producer_done{false};

    auto t0 = std::chrono::high_resolution_clock::now();
    std::thread producer([&] {
        for (uint32_t i = 0; i < PACKETS; ++i) {
            RawPacket pkt{};
            pkt.seq_num = i;
            pkt.symbol_id = i % 4096;
            pkt.length = 32;
            queue.enqueue(pkt);
        }
        producer_done.store(true, std::memory_order_release);
    });
    std::thread decoder([&] {
        RawPacket pkt;
        while (!producer_done.load(std::memory_order_acquire) || queue.peek() != nullptr) {
            if (queue.try_dequeue(pkt)) decoded.fetch_add(1, std::memory_order_relaxed);
        }
    });
    producer.join();
    decoder.join();
    auto t1 = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::cout << "=== moodycamel SPSC Feed Handler (1 NIC line -> 1 decode thread) ===\n";
    std::cout << "total=" << PACKETS << " decoded=" << decoded.load() << "\n";
    std::cout << "Wall time: " << std::fixed << std::setprecision(1) << us << " us ("
              << (us * 1000.0 / PACKETS) << " ns/packet throughput)\n";
}

} // namespace ull::feed_handlers::moodycamel_variant

int main() {
    ull::feed_handlers::moodycamel_variant::run_mpsc_demo();
    std::cout << "\n";
    ull::feed_handlers::moodycamel_variant::run_spsc_demo();
    return 0;
}
