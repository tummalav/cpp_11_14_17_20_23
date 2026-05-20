/**
 * nasdaq_itch_feed_handler.cpp — CRTP ITCH 5.0 Feed Handler Implementation
 *
 * Zero virtual dispatch. No heap allocation in hot path. No mutex on hot path.
 * Two threads: recv_loop (socket → SPSC) and proc_loop (SPSC → decode → dispatch → book).
 */

#include "nasdaq_itch_feed_handler.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <cstring>
#include <cstdio>

namespace nasdaq::itch {

// ============================================================================
// HELPERS
// ============================================================================

template<typename Derived>
void ITCHFeedHandler<Derived>::pin_thread(int core_id) noexcept {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    struct sched_param sp{};
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
#elif defined(__APPLE__)
    struct sched_param sp{};
    sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    (void)core_id;
#endif
}

template<typename Derived>
bool ITCHFeedHandler<Derived>::setup_socket() noexcept {
    sock_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) return false;

    // Non-blocking — recv_loop busy-polls (no sleep_for, no OS scheduler jitter)
    int flags = ::fcntl(sock_fd_, F_GETFL, 0);
    ::fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    int rcvbuf = static_cast<int>(cfg_.rcvbuf_bytes);
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    int reuse = 1;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(cfg_.multicast_port);
    if (::bind(sock_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sock_fd_); sock_fd_ = -1; return false;
    }

    struct ip_mreq mreq{};
    ::inet_pton(AF_INET, cfg_.multicast_ip, &mreq.imr_multiaddr);
    ::inet_pton(AF_INET, cfg_.interface_ip, &mreq.imr_interface);
    if (::setsockopt(sock_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ::close(sock_fd_); sock_fd_ = -1; return false;
    }
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<typename Derived>
bool ITCHFeedHandler<Derived>::connect(const ITCHConfig& cfg) noexcept {
    cfg_ = cfg;
    if (!setup_socket()) return false;

    stop_.store(false, std::memory_order_relaxed);
    connected_.store(true, std::memory_order_release);

    recv_thread_ = std::thread([this]() { pin_thread(cfg_.recv_cpu_core); recv_loop(); });
    proc_thread_ = std::thread([this]() { pin_thread(cfg_.proc_cpu_core); proc_loop(); });
    return true;
}

template<typename Derived>
void ITCHFeedHandler<Derived>::disconnect() noexcept {
    stop_.store(true, std::memory_order_release);
    connected_.store(false, std::memory_order_relaxed);
    if (recv_thread_.joinable()) recv_thread_.join();
    if (proc_thread_.joinable()) proc_thread_.join();
    if (sock_fd_ >= 0) { ::close(sock_fd_); sock_fd_ = -1; }
}

// ============================================================================
// SECTION A: RECEIVE LOOP
//
//  ✓ Non-blocking socket + CPU_PAUSE() — zero OS scheduling jitter
//  ✓ RawPacket in SPSC slot — NO std::vector, NO heap allocation per packet
//  ✓ rdtsc_start() captures timestamp before any decoding (lfence + rdtsc + lfence)
// ============================================================================

template<typename Derived>
void ITCHFeedHandler<Derived>::recv_loop() noexcept {
    RawPacket pkt;
    while (!stop_.load(std::memory_order_relaxed)) {
        const ssize_t n = ::recv(sock_fd_, pkt.data, RAW_PKT_CAPACITY, MSG_DONTWAIT);
        if (__builtin_expect(n > 0, 1)) {
            pkt.recv_tsc = rdtsc_start();   // 3-7 ns, zero syscall
            pkt.len = static_cast<uint16_t>(n);
            msgs_recv_.fetch_add(1, std::memory_order_relaxed);
            while (!raw_queue_.push(pkt)) {
                pkts_drop_.fetch_add(1, std::memory_order_relaxed);
                CPU_PAUSE();
            }
        } else {
            CPU_PAUSE();  // EAGAIN — busy-spin instead of sleep_for (no OS jitter)
        }
    }
}

// ============================================================================
// SECTION B: PROCESSING LOOP
//
//  ✓ Single thread owns all data — no locks
//  ✓ Book maintenance before user dispatch (user sees updated book in callback)
//  ✓ CRTP DispatchTable — zero virtual overhead
// ============================================================================

template<typename Derived>
void ITCHFeedHandler<Derived>::proc_loop() noexcept {
    RawPacket pkt;
    while (!stop_.load(std::memory_order_relaxed)) {
        if (!raw_queue_.pop(pkt)) { CPU_PAUSE(); continue; }
        process_raw_packet(pkt);
    }
}

// ============================================================================
// SECTION C: RAW PACKET → ITCH MESSAGES (MoldUDP64 framing)
// ============================================================================

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::process_raw_packet(const RawPacket& pkt) noexcept {
    const uint8_t* data = pkt.data;
    uint16_t       len  = pkt.len;

    if (cfg_.enable_mold_udp) {
        // MoldUDP64 header = 10 (session) + 8 (seq) + 2 (count) = 20 bytes
        if (__builtin_expect(len < 20, 0)) return;
        data += 20; len -= 20;
    }

    uint16_t offset = 0;
    while (offset + 2 <= len) {
        uint16_t msg_len;
        std::memcpy(&msg_len, data + offset, 2);
        msg_len = static_cast<uint16_t>(__builtin_bswap16(msg_len));  // BE → LE
        if (__builtin_expect(offset + 2u + msg_len > len, 0)) break;
        process_itch_msg(data + offset + 2, msg_len, pkt.recv_tsc);
        offset = static_cast<uint16_t>(offset + 2 + msg_len);
    }
    msgs_proc_.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// SECTION D: ITCH MESSAGE → BOOK UPDATE + USER DISPATCH
// ============================================================================

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::process_itch_msg(const uint8_t* data,
                                                          uint16_t       len,
                                                          uint64_t       recv_tsc) noexcept {
    if (__builtin_expect(len < 1, 0)) return;
    const uint8_t msg_type = data[0];

    // Book maintenance first — user sees updated book in their callback
    if (cfg_.enable_book_building) {
        switch (msg_type) {
            case 'A': book_add_order(*reinterpret_cast<const AddOrderMessage*>(data));               break;
            case 'F': book_add_order(*reinterpret_cast<const AddOrderWithMPIDMessage*>(data));        break;
            case 'E': book_executed (*reinterpret_cast<const OrderExecutedMessage*>(data));           break;
            case 'C': book_executed (*reinterpret_cast<const OrderExecutedWithPriceMessage*>(data));  break;
            case 'X': book_cancel   (*reinterpret_cast<const OrderCancelMessage*>(data));             break;
            case 'D': book_delete   (*reinterpret_cast<const OrderDeleteMessage*>(data));             break;
            case 'U': book_replace  (*reinterpret_cast<const OrderReplaceMessage*>(data));            break;
            default:  break;
        }
    }

    // CRTP dispatch — compile-time resolved, zero virtual overhead
    const uint64_t t0 = rdtsc_now();
    dispatch_table_.dispatch(static_cast<Derived&>(*this), msg_type, data, recv_tsc);
    last_dispatch_ticks_.store(rdtsc_now() - t0, std::memory_order_relaxed);
}

// ============================================================================
// SECTION E: ORDER BOOK MAINTENANCE
//
//  Single writer (proc_thread_) → no mutex needed on any of these paths.
//  All storage: pre-allocated flat arrays → zero heap allocation.
// ============================================================================

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_add_order(const AddOrderMessage& m) noexcept {
    const SymbolKey key = pack_symbol(m.stock);
    if (!symbols_.is_subscribed(key)) return;
    symbols_.get_or_create(key);
    OrderBook* book = symbols_.get_book(key);
    if (!book) return;

    orders_.insert({m.order_reference_number, key, m.price, m.shares, m.buy_sell_indicator, {}});
    orders_cnt_.fetch_add(1, std::memory_order_relaxed);

    if (m.buy_sell_indicator == Side::BUY) book->add_bid(m.price, m.shares);
    else                                   book->add_ask(m.price, m.shares);
}

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_add_order(const AddOrderWithMPIDMessage& m) noexcept {
    const SymbolKey key = pack_symbol(m.stock);
    if (!symbols_.is_subscribed(key)) return;
    symbols_.get_or_create(key);
    OrderBook* book = symbols_.get_book(key);
    if (!book) return;

    orders_.insert({m.order_reference_number, key, m.price, m.shares, m.buy_sell_indicator, {}});
    orders_cnt_.fetch_add(1, std::memory_order_relaxed);

    if (m.buy_sell_indicator == Side::BUY) book->add_bid(m.price, m.shares);
    else                                   book->add_ask(m.price, m.shares);
}

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_executed(const OrderExecutedMessage& m) noexcept {
    OrderInfo* oi = orders_.find(m.order_reference_number);
    if (!oi) return;
    OrderBook* book = symbols_.get_book(oi->symbol_key);
    if (!book) return;

    const uint32_t qty = m.executed_shares;
    if (oi->side == Side::BUY) book->remove_bid_qty(oi->price, qty);
    else                       book->remove_ask_qty(oi->price, qty);

    book->last_trade_price  = oi->price;
    book->last_trade_shares = qty;
    book->total_volume     += qty;

    oi->remaining_shares = (oi->remaining_shares > qty) ? oi->remaining_shares - qty : 0;
    if (oi->remaining_shares == 0) orders_.remove(m.order_reference_number);
    trades_cnt_.fetch_add(1, std::memory_order_relaxed);
}

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_executed(const OrderExecutedWithPriceMessage& m) noexcept {
    OrderInfo* oi = orders_.find(m.order_reference_number);
    if (!oi) return;
    OrderBook* book = symbols_.get_book(oi->symbol_key);
    if (!book) return;

    const uint32_t qty   = m.executed_shares;
    const uint32_t price = m.printable ? m.execution_price : oi->price;

    if (oi->side == Side::BUY) book->remove_bid_qty(oi->price, qty);
    else                       book->remove_ask_qty(oi->price, qty);

    book->last_trade_price  = price;
    book->last_trade_shares = qty;
    book->total_volume     += qty;

    oi->remaining_shares = (oi->remaining_shares > qty) ? oi->remaining_shares - qty : 0;
    if (oi->remaining_shares == 0) orders_.remove(m.order_reference_number);
    trades_cnt_.fetch_add(1, std::memory_order_relaxed);
}

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_cancel(const OrderCancelMessage& m) noexcept {
    OrderInfo* oi = orders_.find(m.order_reference_number);
    if (!oi) return;
    OrderBook* book = symbols_.get_book(oi->symbol_key);
    if (!book) return;

    const uint32_t qty = m.cancelled_shares;
    if (oi->side == Side::BUY) book->remove_bid_qty(oi->price, qty);
    else                       book->remove_ask_qty(oi->price, qty);

    oi->remaining_shares = (oi->remaining_shares > qty) ? oi->remaining_shares - qty : 0;
}

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_delete(const OrderDeleteMessage& m) noexcept {
    OrderInfo* oi = orders_.find(m.order_reference_number);
    if (!oi) return;
    OrderBook* book = symbols_.get_book(oi->symbol_key);
    if (book) {
        if (oi->side == Side::BUY) book->remove_bid_qty(oi->price, oi->remaining_shares);
        else                       book->remove_ask_qty(oi->price, oi->remaining_shares);
    }
    orders_.remove(m.order_reference_number);
}

template<typename Derived>
HOT_PATH void ITCHFeedHandler<Derived>::book_replace(const OrderReplaceMessage& m) noexcept {
    OrderInfo* oi = orders_.find(m.original_order_reference_number);
    if (!oi) return;
    OrderBook* book = symbols_.get_book(oi->symbol_key);
    if (book) {
        if (oi->side == Side::BUY) { book->remove_bid_qty(oi->price, oi->remaining_shares); book->add_bid(m.price, m.shares); }
        else                       { book->remove_ask_qty(oi->price, oi->remaining_shares); book->add_ask(m.price, m.shares); }
    }
    const OrderInfo new_oi{m.new_order_reference_number, oi->symbol_key, m.price, m.shares, oi->side, {}};
    orders_.remove(m.original_order_reference_number);
    orders_.insert(new_oi);
}

} // namespace nasdaq::itch
