/*
 * solarflare_tcpdirect_example.cpp
 *
 * TCP sender/receiver example with:
 *  - POSIX fallback client/server (always available)
 *  - TCPDirect client path using real zf/zft calls when available
 *
 * Conceptually:
 *  - Onload accelerates normal socket code with minimal rewrite.
 *  - TCPDirect replaces the TCP socket API with zf/zft user-space calls.
 *  - Use TCPDirect for hot TCP order-entry paths where you can rewrite I/O.
 *  - ef_vi is packet/NIC-queue level, not a TCP stack.
 *
 * PIO vs CTPIO:
 *  - PIO copies the whole message into NIC memory before transmit.
 *  - CTPIO starts transmitting while bytes are still being pushed.
 *  - CTPIO is faster when timing hits, but PIO is simpler and steadier.
 *
 * Build (fallback):
 *   g++ -std=c++17 -O2 -pthread solarflare_tcpdirect_example.cpp -o solarflare_tcpdirect_example
 *
 * Build (TCPDirect, dynamic linking):
 *   g++ -std=c++17 -O2 -pthread -DUSE_TCPDIRECT solarflare_tcpdirect_example.cpp \
 *       -I/opt/onload/include -L/opt/onload/lib -Wl,-rpath,/opt/onload/lib \
 *       -lzf -lonload_ext -ldl -lrt -o solarflare_tcpdirect_example
 *
 * Required Solarflare libraries for TCPDirect mode:
 *   Shared: /opt/onload/lib/libzf.so, /opt/onload/lib/libonload_ext.so
 *   Static: /opt/onload/lib/libzf.a,  /opt/onload/lib/libonload_ext.a
 *
 * Run POSIX echo server:
 *   ./solarflare_tcpdirect_example server 0.0.0.0 9001
 *
 * Run POSIX client:
 *   ./solarflare_tcpdirect_example client 127.0.0.1 9001 100
 *
 * Run TCPDirect client (talking to same server):
 *   ./solarflare_tcpdirect_example td_client 127.0.0.1 9001 100
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#if defined(USE_TCPDIRECT) && defined(__linux__) && __has_include(<zf/zf.h>)
#include <zf/zf.h>
#define TCPDIRECT_ENABLED 1
#else
#define TCPDIRECT_ENABLED 0
#endif

using namespace std::chrono;

namespace {

[[noreturn]] void perror_exit(const char* msg) {
    std::perror(msg);
    std::exit(EXIT_FAILURE);
}

int run_echo_server(const char* bind_addr, uint16_t port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) perror_exit("socket");

    int reuse = 1;
    if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror_exit("setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind address\n";
        return EXIT_FAILURE;
    }

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) perror_exit("bind");
    if (::listen(listen_fd, 256) < 0) perror_exit("listen");

    std::cout << "TCP echo server listening on " << bind_addr << ":" << port << "\n";

    while (true) {
        sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&cli), &cli_len);
        if (cfd < 0) {
            std::perror("accept");
            continue;
        }

        int one = 1;
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

int run_posix_client(const char* server_addr, uint16_t port, int iters) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) perror_exit("socket");

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

    if (::connect(fd, reinterpret_cast<sockaddr*>(&srv), sizeof(srv)) < 0) perror_exit("connect");

    std::string payload = "ping";
    std::vector<long long> rtts;
    rtts.reserve(static_cast<size_t>(iters));

    for (int i = 0; i < iters; ++i) {
        auto t1 = high_resolution_clock::now();
        if (::send(fd, payload.data(), payload.size(), 0) < 0) perror_exit("send");

        std::vector<char> buf(payload.size());
        ssize_t recvd = 0;
        while (recvd < static_cast<ssize_t>(payload.size())) {
            ssize_t n = ::recv(fd, buf.data() + recvd, payload.size() - recvd, 0);
            if (n <= 0) perror_exit("recv");
            recvd += n;
        }

        auto t2 = high_resolution_clock::now();
        rtts.push_back(duration_cast<microseconds>(t2 - t1).count());
    }

    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);

    auto mm = std::minmax_element(rtts.begin(), rtts.end());
    double avg = std::accumulate(rtts.begin(), rtts.end(), 0.0) / rtts.size();
    std::cout << "POSIX RTT (us): min=" << *mm.first
              << " max=" << *mm.second
              << " avg=" << avg << "\n";
    return 0;
}

#if TCPDIRECT_ENABLED
int run_tcpdirect_client(const char* server_addr, uint16_t port, int iters) {
    if (zf_init() < 0) {
        std::cerr << "zf_init failed\n";
        return EXIT_FAILURE;
    }

    zf_attr* attr = nullptr;
    if (zf_attr_alloc(&attr) < 0) {
        std::cerr << "zf_attr_alloc failed\n";
        zf_deinit();
        return EXIT_FAILURE;
    }

    zf_stack* stack = nullptr;
    if (zf_stack_alloc(attr, &stack) < 0) {
        std::cerr << "zf_stack_alloc failed\n";
        zf_attr_free(attr);
        zf_deinit();
        return EXIT_FAILURE;
    }

    zft_handle* handle = nullptr;
    if (zft_alloc(stack, attr, &handle) < 0) {
        std::cerr << "zft_alloc failed\n";
        zf_stack_free(stack);
        zf_attr_free(attr);
        zf_deinit();
        return EXIT_FAILURE;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (::inet_pton(AF_INET, server_addr, &srv.sin_addr) != 1) {
        std::cerr << "Invalid server address\n";
        zf_stack_free(stack);
        zf_attr_free(attr);
        zf_deinit();
        return EXIT_FAILURE;
    }

    zft* zock = nullptr;
    if (zft_connect(handle, reinterpret_cast<sockaddr*>(&srv), sizeof(srv), &zock) < 0) {
        std::cerr << "zft_connect failed\n";
        zf_stack_free(stack);
        zf_attr_free(attr);
        zf_deinit();
        return EXIT_FAILURE;
    }

    std::string payload = "ping";
    std::vector<long long> rtts;
    rtts.reserve(static_cast<size_t>(iters));

    for (int i = 0; i < iters; ++i) {
        auto t1 = high_resolution_clock::now();

        if (zft_send_single(zock, payload.data(), payload.size(), 0) < 0) {
            std::cerr << "zft_send_single failed\n";
            break;
        }

        bool got_echo = false;
        while (!got_echo) {
            zf_reactor_perform(stack);

            zft_msg msg{};
            int rc = zft_zc_recv(zock, &msg, 0);
            if (rc > 0 && msg.iovcnt > 0 && msg.iov[0].iov_len >= payload.size()) {
                got_echo = true;
                zft_zc_recv_done(zock, &msg);
            }
        }

        auto t2 = high_resolution_clock::now();
        rtts.push_back(duration_cast<microseconds>(t2 - t1).count());
    }

    zft_free(zock);
    zf_stack_free(stack);
    zf_attr_free(attr);
    zf_deinit();

    if (!rtts.empty()) {
        auto mm = std::minmax_element(rtts.begin(), rtts.end());
        double avg = std::accumulate(rtts.begin(), rtts.end(), 0.0) / rtts.size();
        std::cout << "TCPDirect RTT (us): min=" << *mm.first
                  << " max=" << *mm.second
                  << " avg=" << avg << "\n";
    }
    return 0;
}
#else
int run_tcpdirect_client(const char*, uint16_t, int) {
    std::cerr << "TCPDirect path not available. Rebuild with:\n"
              << "  -DUSE_TCPDIRECT and TCPDirect headers in include path\n"
              << "  plus: -lzf -lonload_ext\n";
    return EXIT_FAILURE;
}
#endif

void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " server <bind_addr> <port>\n"
              << "  " << prog << " client <server_addr> <port> [iters]\n"
              << "  " << prog << " td_client <server_addr> <port> [iters]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    const char* addr = argv[2];
    int port = std::stoi(argv[3]);
    int iters = (argc >= 5) ? std::stoi(argv[4]) : 100;

    if (mode == "server") return run_echo_server(addr, static_cast<uint16_t>(port));
    if (mode == "client") return run_posix_client(addr, static_cast<uint16_t>(port), iters);
    if (mode == "td_client") return run_tcpdirect_client(addr, static_cast<uint16_t>(port), iters);

    print_usage(argv[0]);
    return 1;
}
