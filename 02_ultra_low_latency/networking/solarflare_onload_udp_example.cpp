/*
 * solarflare_onload_udp_example.cpp
 *
 * UDP sender/receiver example for Solarflare Onload acceleration.
 * It uses standard sockets by default (accelerated when launched via `onload`).
 *
 * Build:
 *   g++ -std=c++17 -O2 -pthread solarflare_onload_udp_example.cpp -o solarflare_onload_udp_example
 *
 * Run receiver:
 *   onload --profile=latency ./solarflare_onload_udp_example recv 0.0.0.0 9201 200000
 *
 * Run sender:
 *   onload --profile=latency ./solarflare_onload_udp_example send 127.0.0.1 9201 200000 64 0
 *
 * Onload note:
 *  - Same socket API, just accelerated by Onload at runtime.
 *  - Good fit for UDP multicast or unicast feeds without rewriting I/O.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {

struct alignas(64) Datagram {
    uint64_t seq;
    uint64_t tx_ns;
    char payload[48];
};
static_assert(sizeof(Datagram) == 64, "Datagram must be 64 bytes");

[[nodiscard]] uint64_t now_ns() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

[[noreturn]] void die(const char* msg) {
    std::perror(msg);
    std::exit(EXIT_FAILURE);
}

int run_receiver(const char* bind_addr, uint16_t port, uint64_t packet_target) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) die("socket");

    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    int rcvbuf = 16 * 1024 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind address\n";
        ::close(fd);
        return EXIT_FAILURE;
    }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) die("bind");

    uint64_t received = 0;
    uint64_t drops = 0;
    uint64_t expected_seq = 0;
    uint64_t min_lat = std::numeric_limits<uint64_t>::max();
    uint64_t max_lat = 0;
    long double sum_lat = 0.0;

    Datagram pkt{};
    while (received < packet_target) {
        const ssize_t n = ::recvfrom(fd, &pkt, sizeof(pkt), 0, nullptr, nullptr);
        if (n < 0) die("recvfrom");
        if (static_cast<size_t>(n) < sizeof(pkt)) continue;

        if (received != 0 && pkt.seq > expected_seq) drops += (pkt.seq - expected_seq);
        expected_seq = pkt.seq + 1;

        const uint64_t latency = now_ns() >= pkt.tx_ns ? now_ns() - pkt.tx_ns : 0;
        min_lat = std::min(min_lat, latency);
        max_lat = std::max(max_lat, latency);
        sum_lat += latency;
        ++received;

        if ((received % 10000) == 0 || received == packet_target) {
            std::cout << "[recv] packets=" << received
                      << " drops=" << drops
                      << " last_seq=" << pkt.seq
                      << " one_way_ns=" << latency << "\n";
        }
    }

    ::close(fd);
    const double avg = static_cast<double>(sum_lat / packet_target);
    std::cout << "[recv] summary min_ns=" << min_lat
              << " max_ns=" << max_lat
              << " avg_ns=" << avg
              << " drops=" << drops << "\n";
    return 0;
}

int run_sender(const char* dst_addr, uint16_t port, uint64_t packets, size_t bytes, uint64_t interval_us) {
    if (bytes < sizeof(Datagram)) bytes = sizeof(Datagram);
    if (bytes > sizeof(Datagram)) bytes = sizeof(Datagram);

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) die("socket");

    int sndbuf = 4 * 1024 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    if (::inet_pton(AF_INET, dst_addr, &dst.sin_addr) != 1) {
        std::cerr << "Invalid destination address\n";
        ::close(fd);
        return EXIT_FAILURE;
    }

    Datagram pkt{};
    std::memset(pkt.payload, 'M', sizeof(pkt.payload));

    for (uint64_t i = 0; i < packets; ++i) {
        pkt.seq = i;
        pkt.tx_ns = now_ns();
        const ssize_t n = ::sendto(fd, &pkt, bytes, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
        if (n != static_cast<ssize_t>(bytes)) die("sendto");
        if (interval_us != 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
        }
    }

    std::cout << "[send] sent packets=" << packets << " bytes=" << bytes << "\n";
    ::close(fd);
    return 0;
}

void usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " recv <bind_addr> <port> [packets]\n"
              << "  " << prog << " send <dst_addr> <port> [packets] [bytes] [interval_us]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }
    const std::string mode = argv[1];
    const char* addr = argv[2];
    const auto port = static_cast<uint16_t>(std::stoi(argv[3]));

    if (mode == "recv") {
        const uint64_t packets = argc >= 5 ? std::stoull(argv[4]) : 100000;
        return run_receiver(addr, port, packets);
    }
    if (mode == "send") {
        const uint64_t packets = argc >= 5 ? std::stoull(argv[4]) : 100000;
        const size_t bytes = argc >= 6 ? static_cast<size_t>(std::stoull(argv[5])) : sizeof(Datagram);
        const uint64_t interval_us = argc >= 7 ? std::stoull(argv[6]) : 0;
        return run_sender(addr, port, packets, bytes, interval_us);
    }
    usage(argv[0]);
    return 1;
}
