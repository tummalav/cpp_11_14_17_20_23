/*
 * solarflare_ef_vi_example.cpp
 *
 * Realistic UDP sender/receiver example for Solarflare ef_vi, with a POSIX UDP
 * fallback so it compiles and runs without vendor SDK/hardware.
 *
 * Build (fallback path):
 *   g++ -std=c++17 -O2 -pthread solarflare_ef_vi_example.cpp -o solarflare_ef_vi_example
 *
 * Build (ef_vi path; Linux + Solarflare/OpenOnload SDK):
 *   g++ -std=c++17 -O2 -pthread -DUSE_EFVI solarflare_ef_vi_example.cpp \
 *       -I/usr/include/etherfabric -L/usr/lib -letherfabric \
 *       -o solarflare_ef_vi_example
 *
 * Run receiver:
 *   ./solarflare_ef_vi_example recv eth0 239.1.1.10 5000 100000
 *
 * Run sender:
 *   ./solarflare_ef_vi_example send eth0 127.0.0.1 5000 100000 64 0
 */

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <chrono>
#include <thread>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>

#if defined(USE_EFVI) && defined(__linux__)
#include <etherfabric/ef_vi.h>
#include <etherfabric/memreg.h>
#include <etherfabric/pd.h>
#include <etherfabric/vi.h>
#define EFVI_ENABLED 1
#else
#define EFVI_ENABLED 0
#endif

namespace {

constexpr size_t CACHE_LINE = 64;
constexpr size_t ETH_IP_UDP_HEADER_BYTES = 42;
constexpr size_t RX_DMA_BUF_COUNT = 512;
constexpr size_t TX_DMA_BUF_COUNT = 256;
constexpr size_t DMA_BUF_SIZE = 2048;

struct alignas(CACHE_LINE) WirePacket {
    uint64_t sequence;
    uint64_t tx_timestamp_ns;
    char payload[48];
};
static_assert(sizeof(WirePacket) == 64, "WirePacket should be one cache line");

[[nodiscard]] uint64_t now_ns() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

[[noreturn]] void die(const char* msg) {
    std::perror(msg);
    std::exit(EXIT_FAILURE);
}

struct RecvPacketView {
    const uint8_t* data{nullptr};
    uint16_t len{0};
    uint64_t ts_ns{0};
    int request_id{-1};
};

#if EFVI_ENABLED
class EfViUdpReceiver {
public:
    EfViUdpReceiver() = default;
    ~EfViUdpReceiver() { close(); }

    bool open(const std::string& interface_name) {
        if (ef_driver_open(&driver_handle_) < 0) return false;
        if (ef_pd_alloc_by_name(&pd_, driver_handle_, interface_name.c_str(),
                                EF_PD_DEFAULT | EF_PD_MCAST_LOOP) < 0) return false;
        if (ef_vi_alloc_from_pd(&vi_, driver_handle_, &pd_, driver_handle_,
                                -1, 1024, 0, nullptr, -1,
                                EF_VI_FLAGS_DEFAULT | EF_VI_RX_TIMESTAMPS) < 0) return false;

        dma_memory_.resize(RX_DMA_BUF_COUNT * DMA_BUF_SIZE);
        if (ef_memreg_alloc(&memreg_, driver_handle_, &pd_, driver_handle_,
                            dma_memory_.data(), dma_memory_.size()) < 0) return false;

        const ef_addr base_dma = ef_memreg_dma_addr(&memreg_, 0);
        for (int i = 0; i < static_cast<int>(RX_DMA_BUF_COUNT); ++i) {
            ef_vi_receive_post(&vi_, base_dma + i * DMA_BUF_SIZE, i);
        }
        open_ = true;
        return true;
    }

    bool recv_one(RecvPacketView& pkt) noexcept {
        ef_event events[16];
        const int n = ef_vi_receive_poll(&vi_, events, 16);
        for (int i = 0; i < n; ++i) {
            if (EF_EVENT_TYPE(events[i]) != EF_EVENT_TYPE_RX) continue;
            const int rq_id = EF_EVENT_RX_RQ_ID(events[i]);
            const uint16_t bytes = static_cast<uint16_t>(EF_EVENT_RX_BYTES(events[i]));

            uint64_t hw_ts_ns = 0;
            ef_vi_receive_get_timestamp_with_sync_flags(&vi_, &events[i], &hw_ts_ns, nullptr);

            uint8_t* dma_ptr = dma_memory_.data() + static_cast<size_t>(rq_id) * DMA_BUF_SIZE;
            const uint16_t payload_len =
                (bytes > ETH_IP_UDP_HEADER_BYTES) ? static_cast<uint16_t>(bytes - ETH_IP_UDP_HEADER_BYTES) : 0;

            pkt.data = dma_ptr + ETH_IP_UDP_HEADER_BYTES;
            pkt.len = payload_len;
            pkt.ts_ns = hw_ts_ns;
            pkt.request_id = rq_id;
            return true;
        }
        return false;
    }

    void release(int request_id) noexcept {
        const ef_addr base_dma = ef_memreg_dma_addr(&memreg_, 0);
        ef_vi_receive_post(&vi_, base_dma + request_id * DMA_BUF_SIZE, request_id);
    }

    void close() noexcept {
        if (!open_) return;
        ef_vi_free(&vi_, driver_handle_);
        ef_memreg_free(&memreg_, driver_handle_);
        ef_pd_free(&pd_, driver_handle_);
        ef_driver_close(driver_handle_);
        open_ = false;
    }

private:
    ef_driver_handle driver_handle_{};
    ef_pd pd_{};
    ef_vi vi_{};
    ef_memreg memreg_{};
    std::vector<uint8_t> dma_memory_{};
    bool open_{false};
};

class EfViUdpSender {
public:
    EfViUdpSender() = default;
    ~EfViUdpSender() { close(); }

    bool open(const std::string& interface_name) {
        if (ef_driver_open(&driver_handle_) < 0) return false;
        if (ef_pd_alloc_by_name(&pd_, driver_handle_, interface_name.c_str(), EF_PD_DEFAULT) < 0) return false;
        if (ef_vi_alloc_from_pd(&vi_, driver_handle_, &pd_, driver_handle_,
                                -1, 0, 1024, nullptr, -1, EF_VI_FLAGS_DEFAULT) < 0) return false;

        dma_memory_.resize(TX_DMA_BUF_COUNT * DMA_BUF_SIZE);
        if (ef_memreg_alloc(&memreg_, driver_handle_, &pd_, driver_handle_,
                            dma_memory_.data(), dma_memory_.size()) < 0) return false;
        open_ = true;
        return true;
    }

    bool send_one(const void* payload, size_t len) noexcept {
        if (!open_) return false;
        if (len > DMA_BUF_SIZE) return false;

        const uint32_t slot = tx_slot_++ % TX_DMA_BUF_COUNT;
        uint8_t* buf = dma_memory_.data() + static_cast<size_t>(slot) * DMA_BUF_SIZE;
        std::memcpy(buf, payload, len);

        const ef_addr dma_addr = ef_memreg_dma_addr(&memreg_, slot * DMA_BUF_SIZE);
        if (ef_vi_transmit_init(&vi_, dma_addr, static_cast<int>(len), slot) < 0) return false;
        ef_vi_transmit_push(&vi_);
        return true;
    }

    void close() noexcept {
        if (!open_) return;
        ef_vi_free(&vi_, driver_handle_);
        ef_memreg_free(&memreg_, driver_handle_);
        ef_pd_free(&pd_, driver_handle_);
        ef_driver_close(driver_handle_);
        open_ = false;
    }

private:
    ef_driver_handle driver_handle_{};
    ef_pd pd_{};
    ef_vi vi_{};
    ef_memreg memreg_{};
    std::vector<uint8_t> dma_memory_{};
    uint32_t tx_slot_{0};
    bool open_{false};
};
#endif

class PosixUdpReceiver {
public:
    ~PosixUdpReceiver() { close(); }

    bool open(const std::string& interface_name, const std::string& group_or_ip, uint16_t port) {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;

        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags != -1) fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

        int reuse = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(port);
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) return false;

        ip_mreqn mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(group_or_ip.c_str());
        mreq.imr_address.s_addr = INADDR_ANY;
        mreq.imr_ifindex = if_nametoindex(interface_name.c_str());
        if (mreq.imr_ifindex != 0) {
            setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        }
        return true;
    }

    bool recv_one(RecvPacketView& pkt) noexcept {
        const ssize_t n = recvfrom(fd_, buffer_, sizeof(buffer_), 0, nullptr, nullptr);
        if (n <= 0) return false;
        pkt.data = buffer_;
        pkt.len = static_cast<uint16_t>(n);
        pkt.ts_ns = now_ns();
        pkt.request_id = 0;
        return true;
    }

    void release(int) noexcept {}

    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_{-1};
    alignas(CACHE_LINE) uint8_t buffer_[DMA_BUF_SIZE]{};
};

class PosixUdpSender {
public:
    ~PosixUdpSender() { close(); }

    bool open(const std::string&, const std::string& dst_ip, uint16_t dst_port) {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;

        dst_.sin_family = AF_INET;
        dst_.sin_port = htons(dst_port);
        if (inet_pton(AF_INET, dst_ip.c_str(), &dst_.sin_addr) != 1) return false;
        return true;
    }

    bool send_one(const void* payload, size_t len) noexcept {
        const ssize_t n = sendto(fd_, payload, len, 0,
                                 reinterpret_cast<const sockaddr*>(&dst_), sizeof(dst_));
        return n == static_cast<ssize_t>(len);
    }

    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_{-1};
    sockaddr_in dst_{};
};

void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " recv <interface> <listen_ip_or_mcast_group> <port> [packets]\n"
              << "  " << prog << " send <interface> <dst_ip> <port> [packets] [payload_bytes] [interval_us]\n\n"
              << "Examples:\n"
              << "  " << prog << " recv eth0 239.1.1.10 5000 100000\n"
              << "  " << prog << " send eth0 127.0.0.1 5000 100000 64 0\n";
}

int run_receiver(const std::string& interface_name,
                 const std::string& group_or_ip,
                 uint16_t port,
                 uint64_t max_packets) {
#if EFVI_ENABLED
    EfViUdpReceiver receiver;
    if (!receiver.open(interface_name)) die("ef_vi receiver open");
    std::cout << "[recv] ef_vi path enabled\n";
#else
    PosixUdpReceiver receiver;
    if (!receiver.open(interface_name, group_or_ip, port)) die("udp receiver open");
    std::cout << "[recv] POSIX fallback path enabled\n";
#endif

    uint64_t received = 0;
    uint64_t drops = 0;
    uint64_t expected_sequence = 0;

    while (received < max_packets) {
        RecvPacketView pkt{};
        if (!receiver.recv_one(pkt)) {
            std::this_thread::yield();
            continue;
        }
        if (pkt.len < sizeof(WirePacket)) {
            receiver.release(pkt.request_id);
            continue;
        }

        const auto* msg = reinterpret_cast<const WirePacket*>(pkt.data);
        if (received > 0 && msg->sequence > expected_sequence) {
            drops += (msg->sequence - expected_sequence);
        }
        expected_sequence = msg->sequence + 1;

        const uint64_t rx_ns = now_ns();
        const uint64_t latency_ns = (rx_ns >= msg->tx_timestamp_ns)
                                  ? (rx_ns - msg->tx_timestamp_ns)
                                  : 0;

        ++received;
        if ((received % 10000) == 0 || received == max_packets) {
            std::cout << "[recv] packets=" << received
                      << " drops=" << drops
                      << " last_seq=" << msg->sequence
                      << " latency_ns=" << latency_ns << "\n";
        }

        receiver.release(pkt.request_id);
    }

    return 0;
}

int run_sender(const std::string& interface_name,
               const std::string& dst_ip,
               uint16_t dst_port,
               uint64_t packet_count,
               size_t payload_bytes,
               uint64_t interval_us) {
    if (payload_bytes < sizeof(WirePacket)) payload_bytes = sizeof(WirePacket);
    if (payload_bytes > DMA_BUF_SIZE) payload_bytes = DMA_BUF_SIZE;

#if EFVI_ENABLED
    EfViUdpSender sender;
    if (!sender.open(interface_name)) die("ef_vi sender open");
    std::cout << "[send] ef_vi path enabled\n";
#else
    PosixUdpSender sender;
    if (!sender.open(interface_name, dst_ip, dst_port)) die("udp sender open");
    std::cout << "[send] POSIX fallback path enabled\n";
#endif

    alignas(CACHE_LINE) uint8_t buffer[DMA_BUF_SIZE]{};
    auto* msg = reinterpret_cast<WirePacket*>(buffer);
    std::memset(msg->payload, 'A', sizeof(msg->payload));

    for (uint64_t i = 0; i < packet_count; ++i) {
        msg->sequence = i;
        msg->tx_timestamp_ns = now_ns();

        if (!sender.send_one(buffer, payload_bytes)) die("send packet");

        if ((i % 10000) == 0 && i != 0) {
            std::cout << "[send] packets=" << i << "\n";
        }

        if (interval_us > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
        }
    }

    std::cout << "[send] completed packets=" << packet_count << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string mode = argv[1];
    const std::string interface_name = argv[2];
    const std::string addr = argv[3];
    const uint16_t port = static_cast<uint16_t>(std::stoi(argv[4]));

    if (mode == "recv") {
        const uint64_t packets = (argc >= 6) ? std::stoull(argv[5]) : 100000;
        return run_receiver(interface_name, addr, port, packets);
    }

    if (mode == "send") {
        const uint64_t packets = (argc >= 6) ? std::stoull(argv[5]) : 100000;
        const size_t payload_bytes = (argc >= 7) ? static_cast<size_t>(std::stoull(argv[6])) : sizeof(WirePacket);
        const uint64_t interval_us = (argc >= 8) ? std::stoull(argv[7]) : 0;
        return run_sender(interface_name, addr, port, packets, payload_bytes, interval_us);
    }

    print_usage(argv[0]);
    return 1;
}
