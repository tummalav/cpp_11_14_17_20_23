/*
 * solarflare_onload_tcp_example.cpp
 *
 * TCP sender/receiver example intended to be launched with Solarflare Onload.
 * The code uses standard BSD sockets so Onload can accelerate it transparently.
 *
 * Build:
 *   g++ -std=c++17 -O2 -pthread solarflare_onload_tcp_example.cpp -o solarflare_onload_tcp_example
 *
 * Run with Onload acceleration:
 *   onload --profile=latency ./solarflare_onload_tcp_example server 0.0.0.0 9101
 *   onload --profile=latency ./solarflare_onload_tcp_example client 127.0.0.1 9101 200000
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono;

namespace {

[[noreturn]] void die(const char* msg) {
    std::perror(msg);
    std::exit(EXIT_FAILURE);
}

int run_server(const char* bind_addr, uint16_t port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) die("socket");

    int one = 1;
    if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        die("setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind address\n";
        return EXIT_FAILURE;
    }

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) die("bind");
    if (::listen(listen_fd, 256) < 0) die("listen");

    std::cout << "Onload TCP echo server listening on " << bind_addr << ":" << port << "\n";
    while (true) {
        int cfd = ::accept(listen_fd, nullptr, nullptr);
        if (cfd < 0) {
            std::perror("accept");
            continue;
        }
        ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        std::thread([cfd]() {
            std::vector<char> buf(4096);
            while (true) {
                ssize_t n = ::recv(cfd, buf.data(), static_cast<int>(buf.size()), 0);
                if (n <= 0) break;
                ssize_t off = 0;
                while (off < n) {
                    ssize_t w = ::send(cfd, buf.data() + off, static_cast<size_t>(n - off), 0);
                    if (w <= 0) break;
                    off += w;
                }
            }
            ::close(cfd);
        }).detach();
    }
}

int run_client(const char* server_addr, uint16_t port, int iters) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (::inet_pton(AF_INET, server_addr, &srv.sin_addr) != 1) {
        std::cerr << "Invalid server address\n";
        ::close(fd);
        return EXIT_FAILURE;
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&srv), sizeof(srv)) < 0) die("connect");

    std::vector<long long> rtts;
    rtts.reserve(static_cast<size_t>(iters));
    const char payload[] = "PING";
    char recv_buf[4]{};

    for (int i = 0; i < iters; ++i) {
        auto t1 = high_resolution_clock::now();
        if (::send(fd, payload, sizeof(payload), 0) < 0) die("send");
        ssize_t got = 0;
        while (got < static_cast<ssize_t>(sizeof(payload))) {
            const ssize_t n = ::recv(fd, recv_buf + got, sizeof(payload) - got, 0);
            if (n <= 0) die("recv");
            got += n;
        }
        auto t2 = high_resolution_clock::now();
        rtts.push_back(duration_cast<microseconds>(t2 - t1).count());
    }

    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);

    auto mm = std::minmax_element(rtts.begin(), rtts.end());
    const double avg = std::accumulate(rtts.begin(), rtts.end(), 0.0) / rtts.size();
    std::cout << "Onload TCP RTT (us): min=" << *mm.first
              << " max=" << *mm.second
              << " avg=" << avg << "\n";
    return 0;
}

void usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " server <bind_addr> <port>\n"
              << "  " << prog << " client <server_addr> <port> [iters]\n";
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
    const int iters = argc >= 5 ? std::stoi(argv[4]) : 100000;

    if (mode == "server") return run_server(addr, port);
    if (mode == "client") return run_client(addr, port, iters);
    usage(argv[0]);
    return 1;
}
