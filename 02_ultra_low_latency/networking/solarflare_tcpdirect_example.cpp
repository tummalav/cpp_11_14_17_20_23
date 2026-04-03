/*
 * solarflare_tcpdirect_example.cpp
 *
 * Self-contained example showing how to structure Solarflare "TCPDirect" / OpenOnload
 * integration, with a portable POSIX fallback echo server/client so you can compile
 * and run it immediately. Vendor-specific code must be enabled with -DUSE_ONLOAD and
 * appropriate include/library paths.
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

#ifdef USE_ONLOAD
// Placeholder: include your OpenOnload / TCPDirect SDK headers here
// #include <onload/tcpdirect.h>
#endif

using namespace std::chrono;

static void perror_exit(const char* msg) {
    std::perror(msg);
    std::exit(EXIT_FAILURE);
}

// Simple POSIX echo server for testing
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
        return EXIT_FAILURE;
    }

    if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        perror_exit("bind");

    if (listen(listen_fd, 64) < 0)
        perror_exit("listen");

    std::cout << "Echo server listening on " << bind_addr << ":" << port << "\n";

    while (true) {
        struct sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(listen_fd, reinterpret_cast<struct sockaddr*>(&cli), &cli_len);
        if (cfd < 0) {
            std::perror("accept");
            continue;
        }

        // Disable Nagle's algorithm on accepted connection to reduce latency
        {
            int flag = 1;
            if (setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
                std::perror("setsockopt(TCP_NODELAY) on accepted socket");
                // non-fatal; continue with connection
            }
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "Accepted connection from " << client_ip << ":" << ntohs(cli.sin_port) << "\n";

        // Simple echo loop - read and write back in a detached thread
        std::thread([cfd]() {
            const size_t BUF_SZ = 4096;
            std::vector<char> buf(BUF_SZ);
            while (true) {
                ssize_t n = ::recv(cfd, buf.data(), (int)buf.size(), 0);
                if (n < 0) {
                    std::perror("recv");
                    break;
                } else if (n == 0) {
                    // peer closed
                    break;
                }
                ssize_t off = 0;
                while (off < n) {
                    ssize_t w = ::send(cfd, buf.data() + off, (size_t)(n - off), 0);
                    if (w < 0) {
                        std::perror("send");
                        break;
                    }
                    off += w;
                }
            }
            ::close(cfd);
            std::cout << "Connection closed (worker)\n";
        }).detach();
    }

    ::close(listen_fd);
    return 0;
}

// Client using POSIX sockets by default. Replace the inner I/O path with TCPDirect calls when available.
int run_client_measure_rtt(const char* server_addr, uint16_t port, int iters) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        perror_exit("socket");

    // Disable Nagle on the client socket to measure low-latency RTT
    {
        int flag = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            std::perror("setsockopt(TCP_NODELAY) on client socket");
            // non-fatal
        }
    }

    struct sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, server_addr, &srv.sin_addr) != 1) {
        std::cerr << "Invalid server address\n";
        ::close(fd);
        return EXIT_FAILURE;
    }

#ifdef USE_ONLOAD
    // Placeholder for Onload/TCPDirect connect flow.
    // Example:
    //   - You may need to call onload initialization routines
    //   - Replace ::connect() with the vendor's connect wrapper if required
    //   - Register memory regions if zero-copy APIs demand it
    // Here we still call POSIX connect as a baseline; replace when you enable Onload
#endif

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&srv), sizeof(srv)) < 0)
        perror_exit("connect");

    std::cout << "Connected to " << server_addr << ":" << port << "\n";

    std::string payload = "ping"; // short payload so RTT dominated by network+stack
    std::vector<long long> rtts;
    rtts.reserve(iters);

    for (int i = 0; i < iters; ++i) {
        auto t1 = high_resolution_clock::now();

#ifdef USE_ONLOAD
        // Example placeholder: use tcp_direct_send() or similar zero-copy call instead of ::send
        // ssize_t sent = tcp_direct_send(fd, payload.data(), payload.size());
        // For demonstration we use ::send
#endif

        ssize_t sent = ::send(fd, payload.data(), payload.size(), 0);
        if (sent < 0) {
            std::perror("send");
            break;
        }

        // receive exact same length
        ssize_t recvd = 0;
        std::vector<char> buf(payload.size());
        while (recvd < (ssize_t)payload.size()) {
#ifdef USE_ONLOAD
            // Placeholder: tcp_direct_recv() or vendor API
#endif
            ssize_t n = ::recv(fd, buf.data() + recvd, (int)(payload.size() - recvd), 0);
            if (n < 0) {
                std::perror("recv");
                goto out;
            } else if (n == 0) {
                std::cerr << "Connection closed by peer\n";
                goto out;
            }
            recvd += n;
        }

        auto t2 = high_resolution_clock::now();
        auto rtt_us = duration_cast<microseconds>(t2 - t1).count();
        rtts.push_back(rtt_us);

        // tiny sleep to avoid flooding -- in low-latency tests you'd not sleep
        std::this_thread::sleep_for(milliseconds(1));
    }

out:
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);

    if (!rtts.empty()) {
        auto minmax = std::minmax_element(rtts.begin(), rtts.end());
        double avg = std::accumulate(rtts.begin(), rtts.end(), 0.0) / rtts.size();
        std::cout << "RTT (us): min=" << *minmax.first << " max=" << *minmax.second << " avg=" << avg << "\n";
    } else {
        std::cout << "No RTT samples collected\n";
    }

    return 0;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << prog << " server <bind_addr> <port>\n";
    std::cerr << "  " << prog << " client <server_addr> <port> [iters]\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    const char* addr = argv[2];
    int port = std::stoi(argv[3]);

    if (mode == "server") {
        return run_echo_server(addr, (uint16_t)port);
    } else if (mode == "client") {
        int iters = 100;
        if (argc >= 5) iters = std::stoi(argv[4]);
        return run_client_measure_rtt(addr, (uint16_t)port, iters);
    } else {
        print_usage(argv[0]);
        return 1;
    }
}
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void perror_exit(const char* msg) {
    std::perror(msg);
    exit(EXIT_FAILURE);
}

int run_server(uint16_t port) {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        perror_exit("socket");

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        perror_exit("setsockopt(SO_REUSEADDR)");

    struct sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&srv), sizeof(srv)) < 0)
        perror_exit("bind");

    if (listen(listen_fd, SOMAXCONN) < 0)
        perror_exit("listen");

    while (true) {
        struct sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(listen_fd, reinterpret_cast<struct sockaddr*>(&cli), &cli_len);
        if (cfd < 0) {
            std::perror("accept");
            continue;
        }

        // Disable Nagle's algorithm on accepted connection to reduce latency
        {
            int flag = 1;
            if (setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
                std::perror("setsockopt(TCP_NODELAY) on accepted socket");
                // non-fatal; continue with connection
            }
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "Accepted connection from " << client_ip << ":" << ntohs(cli.sin_port) << "\n";

        // Handle the connection...

        close(cfd);
    }

    close(listen_fd);
    return 0;
}

int run_client_measure_rtt(const char* server_addr, uint16_t port, int iters) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        perror_exit("socket");

    // Disable Nagle on the client socket to measure low-latency RTT
    {
        int flag = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            std::perror("setsockopt(TCP_NODELAY) on client socket");
            // non-fatal
        }
    }

    struct sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, server_addr, &srv.sin_addr);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&srv), sizeof(srv)) < 0)
        perror_exit("connect");

    // Measure RTT...

    close(fd);
    return 0;
}
