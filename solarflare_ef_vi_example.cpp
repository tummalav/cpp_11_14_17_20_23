/*
 * solarflare_ef_vi_example.cpp
 *
 * Example demonstrating how to structure code to use Solarflare ef_vi (OpenOnload) API
 * while keeping a POSIX fallback so the file compiles and runs without the vendor SDK.
 *
 * This file provides:
 *  - A POSIX echo server and client (same behavior as standard sockets)
 *  - #ifdef USE_EFVI sections (placeholders) where ef_vi initialization, send and recv
 *    would be used when the SDK is available
 *  - Build/run instructions for POSIX fallback and notes for enabling ef_vi
 *
 * Build (POSIX fallback):
 *   g++ -std=c++17 -O2 solarflare_ef_vi_example.cpp -o solarflare_ef_vi_example -pthread
 *
 * Build (with ef_vi/OpenOnload):
 *   g++ -std=c++17 -O2 solarflare_ef_vi_example.cpp -o solarflare_ef_vi_example -pthread \
 *       -DUSE_EFVI -I/path/to/onload/include -L/path/to/onload/lib -lef_vi
 *
 * Run server:
 *   ./solarflare_ef_vi_example server 0.0.0.0 9001
 *
 * Run client:
 *   ./solarflare_ef_vi_example client 127.0.0.1 9001 100
 *
 * Notes for ef_vi integration:
 *  - The ef_vi API is low-level and vendor-specific. Typical steps include:
 *      1) Create/attach ef_driver and ef_vi objects
 *      2) Allocate and register packet buffers
 *      3) Use ef_vi_transmit() / ef_vi_receive() or ef_vi_poll()/ef_vi_receivev() family
 *      4) Reclaim descriptors and handle completions
 *  - Consult Solarflare/OpenOnload documentation for precise function names and required
 *    initialization/teardown sequences. Keep ef_vi usage confined to small wrapper
 *    functions so the rest of your code is portable.
 */

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>

#ifdef USE_EFVI
// Placeholder headers for ef_vi / OpenOnload - adjust to your SDK layout
// #include <onload/ef_vi.h>
// #include <onload/ef_driver.h>
#endif

using namespace std::chrono;

static void perror_exit(const char* msg) {
    std::perror(msg);
    std::exit(EXIT_FAILURE);
}

// Small portable wrapper API - POSIX by default, ef_vi when enabled.
// The goal: keep the main logic unchanged and swap the I/O backend behind this API.

struct NetHandle {
    int fd; // POSIX socket
#ifdef USE_EFVI
    // Add ef_vi-specific handles here (ef_driver_handle, ef_vi_handle, rx/tx queues, etc.)
    // void* ef_driver; void* ef_vi; ...
#endif
};

// Create client connection (POSIX connect or ef_vi connect equivalent)
NetHandle net_connect(const char* addr, uint16_t port) {
    NetHandle h{};
#ifdef USE_EFVI
    // TODO: Initialize ef_driver / ef_vi, bind to local interface, etc.
    // Example (pseudocode):
    //   ef_driver_open(&h.ef_driver);
    //   ef_vi_create_tx(&h.ef_vi, h.ef_driver, interface_index, mtu, ...);
    //   ef_vi_connect(h.ef_vi, addr, port);
    // For now, fall back to POSIX so file compiles without SDK
#endif
    h.fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (h.fd < 0) perror_exit("socket");

    // Disable Nagle
    int flag = 1;
    setsockopt(h.fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    struct sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &srv.sin_addr) != 1) {
        ::close(h.fd);
        perror_exit("inet_pton");
    }

    if (::connect(h.fd, reinterpret_cast<struct sockaddr*>(&srv), sizeof(srv)) < 0) {
        ::close(h.fd);
        perror_exit("connect");
    }

    return h;
}

void net_close(NetHandle& h) {
#ifdef USE_EFVI
    // ef_vi teardown: destroy vi, close driver
#endif
    if (h.fd >= 0) {
        ::shutdown(h.fd, SHUT_RDWR);
        ::close(h.fd);
        h.fd = -1;
    }
}

ssize_t net_send(NetHandle& h, const void* buf, size_t len) {
#ifdef USE_EFVI
    // Use ef_vi transmit API: prepare descriptors, post transmit, and return bytes sent
    // Pseudocode example:
    //   ef_vi_transmit(h.ef_vi, buf, len);
    //   return len_on_success;
    // For portability, fall back to POSIX send below
#endif
    ssize_t s = ::send(h.fd, buf, len, 0);
    return s;
}

ssize_t net_recv(NetHandle& h, void* buf, size_t len) {
#ifdef USE_EFVI
    // Use ef_vi receive/poll API to pop a packet into buf. Return bytes received.
    // Pseudocode:
    //   int n = ef_vi_receive(h.ef_vi, buf, len);
    //   return n;
    // Fall back to POSIX recv
#endif
    ssize_t r = ::recv(h.fd, buf, (int)len, 0);
    return r;
}

// POSIX echo server - can be swapped to ef_vi listen/accept if desired
int run_echo_server(const char* bind_addr, uint16_t port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        perror_exit("socket");

    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        perror_exit("setsockopt(SO_REUSEADDR)");

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind address\n";
        ::close(listen_fd);
        return EXIT_FAILURE;
    }

    if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        perror_exit("bind");

    if (listen(listen_fd, 64) < 0)
        perror_exit("listen");

    std::cout << "Echo server (POSIX) listening on " << bind_addr << ":" << port << "\n";

    while (true) {
        struct sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(listen_fd, reinterpret_cast<struct sockaddr*>(&cli), &cli_len);
        if (cfd < 0) {
            std::perror("accept");
            continue;
        }

        // Disable Nagle to reduce latency
        int flag = 1;
        setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "Accepted connection from " << client_ip << ":" << ntohs(cli.sin_port) << "\n";

        std::thread([cfd]() {
            const size_t BUF_SZ = 4096;
            std::vector<char> buf(BUF_SZ);
            while (true) {
                ssize_t n = ::recv(cfd, buf.data(), (int)buf.size(), 0);
                if (n < 0) { std::perror("recv"); break; }
                if (n == 0) break; // closed
                ssize_t off = 0;
                while (off < n) {
                    ssize_t w = ::send(cfd, buf.data() + off, (size_t)(n - off), 0);
                    if (w < 0) { std::perror("send"); break; }
                    off += w;
                }
            }
            ::close(cfd);
            std::cout << "Connection closed\n";
        }).detach();
    }

    ::close(listen_fd);
    return 0;
}

int run_client_measure_rtt(const char* server_addr, uint16_t port, int iters) {
    NetHandle h = net_connect(server_addr, port);
    std::string payload = "ping";
    std::vector<long long> rtts;
    rtts.reserve(iters);

    for (int i = 0; i < iters; ++i) {
        auto t1 = high_resolution_clock::now();
        ssize_t sent = net_send(h, payload.data(), payload.size());
        if (sent < 0) { std::perror("net_send"); break; }

        std::vector<char> buf(payload.size());
        ssize_t recvd = 0;
        while (recvd < (ssize_t)payload.size()) {
            ssize_t n = net_recv(h, buf.data() + recvd, payload.size() - recvd);
            if (n < 0) { std::perror("net_recv"); goto out; }
            if (n == 0) { std::cerr << "Connection closed by peer\n"; goto out; }
            recvd += n;
        }
        auto t2 = high_resolution_clock::now();
        rtts.push_back(duration_cast<microseconds>(t2 - t1).count());
        std::this_thread::sleep_for(milliseconds(1));
    }

out:
    net_close(h);

    if (!rtts.empty()) {
        auto mm = std::minmax_element(rtts.begin(), rtts.end());
        double avg = std::accumulate(rtts.begin(), rtts.end(), 0.0) / rtts.size();
        std::cout << "RTT (us): min=" << *mm.first << " max=" << *mm.second << " avg=" << avg << "\n";
    } else {
        std::cout << "No RTT samples\n";
    }
    return 0;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << prog << " server <bind_addr> <port>\n";
    std::cerr << "  " << prog << " client <server_addr> <port> [iters]\n";
}

int main(int argc, char** argv) {
    if (argc < 4) { print_usage(argv[0]); return 1; }
    std::string mode = argv[1];
    const char* addr = argv[2];
    int port = std::stoi(argv[3]);
    if (mode == "server") return run_echo_server(addr, (uint16_t)port);
    if (mode == "client") {
        int iters = 100; if (argc >= 5) iters = std::stoi(argv[4]);
        return run_client_measure_rtt(addr, (uint16_t)port, iters);
    }
    print_usage(argv[0]);
    return 1;
}
</attachment>great, we created file twice...). The attachments contain duplication; but current file in workspace likely is last version. We must handle duplication. Now respond: start with one-liner and checklist, then state I've created file, run errors. We must call get_errors. Use functions.get_errors. Then attempt compile with run_in_terminal. Need one-sentence preface before tool calls. Provide build command in explanation; but per toolUseInstructions, before notable tool batches, briefly tell the user what you're about to do and why. Also need to include progress after tool results. Let's call get_errors now. Then run compile. Use run_in_terminal to compile: g++ -std=c++17 -O2 solarflare_ef_vi_example.cpp -o solarflare_ef_vi_example -pthread. Then report outcome. If compile fails, fix. Let's call get_errors first. Use functions.get_errors. Then run compile. Proceed. We'll include tool call preamble sentence. Then interpret results. Let's do it. We must ensure JSON input for tools. Use functions.get_errors then functions.run_in_terminal. Let's proceed. Elicit the tool calls. We'll include preamble before each batch per toolUseInstructions: one-sentence
