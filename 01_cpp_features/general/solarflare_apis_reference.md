# Solarflare / AMD OpenOnload, TCPDirect & ef_vi APIs — Latest Reference

**Last Updated:** 2024-2025  
**Latest Release:** v9.0.2 (as of Oct 2024)  
**Repository:** https://github.com/Xilinx-CNS/onload  
**License:** GPLv2.0 + BSD-2-Clause (Open Source)

---

## Overview

The Solarflare / AMD Xilinx accelerated networking stack consists of three layers:

1. **OpenOnload** — Kernel module + user-space library; intercepts BSD sockets, accelerates TCP/UDP transparently. Binary-compatible with existing apps.
2. **TCPDirect** — Explicit low-latency TCP API (higher-level than ef_vi), focus on ultra-low, bounded latency for trading systems.
3. **ef_vi** — Lowest-level EtherFabric Virtual Interface; direct hardware access, zero-copy, fine-grained control.

---

## Part 1 — OpenOnload (Latest v9.0.2)

### Overview

- **User-level network stack** with kernel module support.
- Accelerates existing BSD socket apps without recompilation on Solarflare NICs.
- Also supports AF_XDP for generic NICs (community-supported, not production-ready yet).

### Supported Hardware

**Solarflare / AMD NICs (native ef_vi acceleration):**
- SFN8522, SFN8542, SFN8042
- X2522, X2522-25G, X2541
- X3522

**Generic NICs (AF_XDP only, community-supported):**
- Any Linux driver with AF_XDP support (Intel i40e, ice, etc.)
- Generic XDP mode available as fallback

### Key Features

- **Binary compatible** — just prefix app with `onload`
- **Handles fork(), exec(), socket passing** — full POSIX compatibility
- **Memory ordering and protocol advancement** even when app not scheduled
- **Zero-copy RX/TX** on Solarflare hardware via ef_vi
- **Mixed mode** — same NIC can accelerate some sockets/processes, kernel-route others

### Installation & Build

```bash
# Clone latest source
git clone https://github.com/Xilinx-CNS/onload.git
cd onload

# Build & install (see DEVELOPING.md for details)
./onload_build
sudo ./onload_install
```

### Compatible OS

- Debian 12+
- Ubuntu LTS 24.04+
- EL 9.0+, 10.0+
- Linux kernel 6.1 – 7.0 (head of tree)

**Supported releases** (commercial support): https://www.xilinx.com/support/download/nic-software-and-drivers.html#open

### Running with Onload

```bash
onload <application>
onload -l <application>  # load Onload library only
```

### AF_XDP Registration (for non-Solarflare NICs)

```bash
echo ens2f0 > /sys/module/sfc_resource/afxdp/register
```

---

## Part 2 — TCPDirect (Explicit Low-Latency TCP API)

TCPDirect is Solarflare's **explicit** ultra-low-latency TCP library, designed for **deterministic, microsecond-scale latency** in trading/financial systems.

### Key Characteristics

- **Not a transparent interceptor** — explicit API calls (explicit socket creation, connect, send/receive).
- **Bounded latency** — guaranteed low-latency path with predictable code paths (no page faults, no allocations in hot path).
- **Lock-free operation** in most paths — single-threaded assumed for best performance.
- **Zero-copy** RX/TX buffers — direct hardware access.
- **Timestamping** support (hardware and software).
- **Supported only on Solarflare NICs** — not AF_XDP.

### Typical TCPDirect Use Case (Trading)

```
Order entry thread (1 core, TCPDirect TCP session to exchange):
  - Connect to exchange gateway via TCPDirect
  - Send orders → immediate hardware transmission
  - Receive fills → DMA into pre-allocated RX buffer, hardware timestamp
  - Round-trip latency: sub-microsecond after software starts

Market data ingestion (1 core per feed, ef_vi for multicast):
  - Parse market data straight from DMA'd memory (zero-copy)
  - Post ticks to strategy threads via SPMC lock-free ring buffer
```

### TCPDirect API Basics

```cpp
// Create TCP socket context
tcs_t* tcs = ef_tcp_open(...);

// Connect to peer
ef_tcp_connect(tcs, peer_addr, peer_port);

// Send data
ssize_t sent = ef_tcp_send(tcs, buf, len);

// Poll for incoming data
ef_event event;
ef_eventq_poll(...);  // Check for RX completions, timestamps

// Receive data
ssize_t recv = ef_tcp_recv(tcs, recv_buf, max_len);
```

### Availability

- **Source**: Included in OpenOnload repo (libonload_zf for ZeroFabric / TCPDirect)
- **License**: Proprietary (AMD Solarflare) — may require commercial license for production
- **Documentation**: Typically under `src/lib/zf_*` directories in the repo

---

## Part 3 — ef_vi API (Lowest Level, Maximum Control)

`ef_vi` is the **lowest-level EtherFabric Virtual Interface** API — direct hardware access, DMA rings, event queues, timestamps, filtering. Used by OpenOnload and TCPDirect internally.

### Core Concepts

- **Protection Domain (ef_pd)** — resource container allocated from a NIC.
- **Virtual Interface (ef_vi)** — per-application access point; contains RX ring, TX ring, event queue.
- **Descriptor Rings** — RX/TX queues (typically 512–4096 slots, power-of-2).
- **Event Queue** — reports RX completions, TX completions, errors.
- **DMA Ring Buffers** — user allocates, hardware writes/reads directly, zero-copy.

### ef_vi Header File (v9.0.2)

**Location:** `src/include/etherfabric/ef_vi.h`

#### Key Types & Structures

```c
typedef int ef_driver_handle;          // Handle to NIC / driver
typedef struct ef_vi ef_vi;            // Virtual interface
typedef struct ef_pd ef_pd;            // Protection domain
typedef uint32_t ef_eventq_ptr;        // Event queue pointer

/* Event types */
typedef union {
  struct {
    unsigned type         :16;
    unsigned q_id         :8;
    unsigned __reserved   :8;
    unsigned rq_id        :32;
    unsigned len          :16;  // RX packet length
    unsigned flags        :16;
    unsigned ofs          :16;  /* AF_XDP offset */
  } rx;
  
  struct {
    unsigned type         :16;
    unsigned q_id         :8;
    unsigned flags        :8;
    unsigned desc_id      :16;
    unsigned deferred_evs :16;
  } tx;
  
  struct {  /* RX with timestamp */
    unsigned type         :16;
    unsigned q_id         :8;
    unsigned flags        :8;
    unsigned rq_id        :32;
    unsigned ts_sec       :32;
    unsigned ts_nsec      :30;
    unsigned ts_flags     :2;
  } rx_timestamp;
  
  struct {  /* TX with timestamp */
    unsigned type         :16;
    unsigned q_id         :8;
    unsigned flags        :8;
    unsigned ts_nsec_frac :4;
    unsigned desc_id      :16;
    unsigned deferred_evs :16;
    unsigned ts_sec       :32;
    unsigned ts_nsec      :30;
    unsigned ts_flags     :2;
  } tx_timestamp;
} ef_event;

/* Allocate Protection Domain */
int ef_pd_alloc_by_name(ef_pd** pd, ef_driver_handle dh,
                        const char* intf_name);

/* Allocate Virtual Interface from PD */
int ef_vi_alloc_from_pd(ef_vi** vi, ef_driver_handle vi_dh,
                        ef_pd* pd, ef_driver_handle pd_dh,
                        int evq_capacity, int rxq_capacity, int txq_capacity,
                        ef_vi* evq_opt, ef_driver_handle evq_dh, int flags);

/* RX descriptor post (queue a buffer for RX DMA) */
void ef_vi_receive_init(ef_vi* vi, ef_addr addr, ef_request_id dma_id);
void ef_vi_receive_post(ef_vi* vi, const ef_iovec* iov, int iov_len,
                        ef_request_id dma_id);

/* TX descriptor post (queue a buffer for TX) */
void ef_vi_transmit_init(ef_vi* vi, ef_addr addr, ef_request_id dma_id);
void ef_vi_transmit_post(ef_vi* vi, ef_addr addr, int len, ef_request_id dma_id);
void ef_vi_transmit_push(ef_vi* vi);  /* Doorbell: flush TX */

/* Poll event queue */
ef_event* ef_eventq_poll(ef_vi* vi, ef_event* ev_out, int ev_out_size);

/* Query descriptor */
int ef_vi_transmit_query_va(ef_vi* vi, ef_request_id id, ef_addr* out);
int ef_vi_receive_query_va(ef_vi* vi, ef_request_id id, ef_addr* out);

/* Timestamp utilities */
int ef_vi_ts_fetch(ef_vi* vi, ef_event* ev_out);
```

#### Dimensions / Constants

```c
#define EF_VI_MAX_QS              32    /* Max queues per VI */
#define EF_VI_EVENT_POLL_MIN_EVS  2     /* Min event array size */
#define EF_VI_DMA_ALIGN           64    /* Cache-line alignment for DMA buffers */
```

#### Typical ef_vi Flow (Low-Level Polling)

```cpp
// 1. Allocate resources
ef_pd* pd;
ef_pd_alloc_by_name(&pd, driver_handle, "eth0");

ef_vi* vi;
ef_vi_alloc_from_pd(&vi, driver_handle, pd, driver_handle,
                    512,  // EVQ capacity
                    512,  // RX capacity
                    512,  // TX capacity
                    NULL, 0, 0);  // No shared EVQ

// 2. Register DMA buffers (pre-allocated, pinned memory)
ef_addr rx_dma_buf = user_buffer_to_dma_addr(rx_buffer, RX_BUF_SIZE);
for (int i = 0; i < 512; i++) {
  ef_vi_receive_init(vi, rx_dma_buf + i * 2048, i);
  ef_vi_receive_post(vi, NULL, 0, i);  // Post empty descriptor
}

// 3. Busy-poll loop
while (running) {
  // Poll event queue
  ef_event evs[16];
  int n = ef_eventq_poll(vi, evs, 16);
  
  for (int i = 0; i < n; i++) {
    if (EF_EVENT_TYPE(evs[i]) == EF_EVENT_TYPE_RX) {
      uint32_t rq_id = evs[i].rx.rq_id;
      uint16_t len = evs[i].rx.len;
      uint8_t* pkt = (uint8_t*)(rx_dma_buf + rq_id * 2048);
      
      // Process packet at pkt, length len
      // ...
      
      // Re-post RX descriptor
      ef_vi_receive_init(vi, rx_dma_buf + rq_id * 2048, rq_id);
      ef_vi_receive_post(vi, NULL, 0, rq_id);
    }
    else if (EF_EVENT_TYPE(evs[i]) == EF_EVENT_TYPE_TX) {
      uint16_t desc_id = evs[i].tx.desc_id;
      // TX completed, buffer can be reused
    }
  }
  
  // TX: post a packet
  if (packet_to_send) {
    ef_vi_transmit_init(vi, tx_dma_addr, tx_id);
    ef_vi_transmit_post(vi, tx_dma_addr, pkt_len, tx_id);
    ef_vi_transmit_push(vi);  // Ring doorbell
  }
}
```

### ef_vi Key Headers (v9.0.2)

**Core headers in `src/include/etherfabric/`:**

- `ef_vi.h` — Main ef_vi API (VI allocation, RX/TX, event polling)
- `vi.h` — Lower-level VI object details
- `capabilities.h` — Capability flags (hardware features, timestamping modes)
- `ef_filter.h` — Hardware packet filtering, steering
- `pd.h` — Protection domain API
- `ef_memreg.h` — DMA memory registration
- `base.h` — Base types and macros

### Memory Requirements & Alignment

- **DMA buffers** must be:
  - Physically contiguous (or virtually, with IOMMU)
  - **Aligned to `EF_VI_DMA_ALIGN` (64 bytes)** or larger
  - Pinned in kernel (mlocked or hugetlbfs)
  - Registered with `ef_memreg_alloc()` or similar

- **RX ring buffers** typically 2–4 KB per packet
- **TX ring buffers** configurable, usually 64 KB – 1 MB per application

### Performance Characteristics (Solarflare NICs)

| Path | Latency | Notes |
|---|---|---|
| ef_vi RX (ingress to app buffer) | Sub-microsecond (0.5–2 µs) | Depends on CPU frequency, cache state, PCIe latency |
| ef_vi TX (app buffer to wire) | Sub-microsecond (0.3–1 µs) | Hardware timestamp + TX complete in ~5–20 µs |
| TCPDirect (TCP in OpenOnload) | 1–5 µs E2E | Higher-level protocol handling, still deterministic |
| OpenOnload (transparent TCP/UDP) | 5–15 µs E2E | Kernel transition overhead, still beats kernel sockets by 10–100x |
| Kernel TCP (SO_TIMESTAMP) | 10–100 µs+ | Jittery; subject to scheduler, page faults, locks |

---

## Part 4 — Memory Ordering & Synchronization in Solarflare APIs

### DMA vs. CPU Memory Ordering

- **RX**: Hardware DMA writes packet data to buffer → hardware updates RX ring descriptor → hardware posts event to event queue. CPU must respect **acquire** semantics when reading event queue.
- **TX**: CPU writes packet data to TX buffer → CPU posts TX descriptor → hardware reads → hardware transmits. Hardware respects **release** semantics.

### ef_vi Descriptor Ordering

```cpp
// RX: hardware to software
ef_event ev;
ef_eventq_poll(vi, &ev, 1);  // Implicit acquire on event read

// If EF_EVENT_TYPE_RX, packet data at ev.rx.rq_id is guaranteed visible
uint8_t* pkt = (uint8_t*)(rx_buf + ev.rx.rq_id * pkt_size);
// pkt data is safe to read (release-acquire pairing via hardware)

// TX: software to hardware
ef_vi_transmit_init(vi, tx_addr, id);
ef_vi_transmit_post(vi, tx_addr, len, id);
ef_vi_transmit_push(vi);  // Doorbell write — implicit release

// Hardware will see tx_addr data (all CPU caches flushed before doorbell)
```

### Lock-Free Patterns with ef_vi

- **SPSC ring buffer** (queue producer, strategy consumer):
  - Producer: `ef_vi_receive_post()` → consumer reads via ring index
  - No atomics needed if single producer/consumer; memory barriers via ef_vi API
  
- **MPSC work-stealing** (multiple strategy threads):
  - Each thread polls its own VI (no contention)
  - Shared VIs require atomic descriptor management (advanced topic)

---

## Part 5 — Filter API (Hardware Packet Steering)

`ef_filter_h` allows hardware to steer packets by flow (5-tuple) to specific VIs.

```cpp
struct ef_filter_spec {
  uint32_t type;  // EF_FILTER_TYPE_UNICAST_UCAST, EF_FILTER_TYPE_TCP_FULL, etc.
  uint8_t proto;  // IPPROTO_TCP, IPPROTO_UDP
  // ... IP addresses, ports, VLANs, etc.
};

int ef_vi_filter_add(ef_vi* vi, const ef_filter_spec* spec,
                     ef_filter_cookie* out_cookie);

int ef_vi_filter_del(ef_vi* vi, ef_filter_cookie cookie);
```

Use cases:
- Steer all packets from a specific counterparty to a dedicated VI (order entry).
- Steer market data multicast flows to a separate VI (fan-out ring).
- Bypass kernel for specific flows, let others go to kernel sockets.

---

## Part 6 — Hardware Timestamping

Both ef_vi and TCPDirect support **hardware RX/TX timestamps**, crucial for latency measurement.

### RX Timestamp

```cpp
// Event with timestamp
if (EF_EVENT_TYPE(ev) == EF_EVENT_TYPE_RX_TIMESTAMP) {
  uint32_t ts_sec = ev.rx_timestamp.ts_sec;
  uint32_t ts_nsec = ev.rx_timestamp.ts_nsec;
  // Packet arrived at hardware at (ts_sec, ts_nsec)
}
```

### TX Timestamp

```cpp
if (EF_EVENT_TYPE(ev) == EF_EVENT_TYPE_TX_WITH_TIMESTAMP) {
  uint32_t ts_sec = ev.tx_timestamp.ts_sec;
  uint32_t ts_nsec = ev.tx_timestamp.ts_nsec;
  uint32_t ts_frac = ev.tx_timestamp.ts_nsec_frac;  // Sub-nanosecond
  // Packet transmitted at (ts_sec, ts_nsec + ts_frac * 2^-4 ns)
}
```

### Synchronization

- **PTP (Precision Time Protocol)** — sync all machines on network to ~nanosecond precision.
- **Hardware timestamping at PHY** — eliminates software jitter.
- Typical trading setup: PTP + hardware timestamps on both RX and TX.

---

## Part 7 — Comparison: ef_vi vs. TCPDirect vs. OpenOnload

| Aspect | ef_vi | TCPDirect | OpenOnload |
|---|---|---|---|
| **API Level** | Lowest (direct hw) | Mid (explicit TCP) | Highest (transparent) |
| **Latency** | Sub-µs (0.5–2) | 1–5 µs | 5–15 µs |
| **Jitter** | Deterministic (fixed code paths) | Deterministic | Less deterministic (OS integration) |
| **Protocol** | Raw Ethernet, custom | TCP | Full TCP/UDP stack |
| **Learning curve** | Steep (hardware details) | Medium | Shallow (existing apps) |
| **Production trading** | ✓ (order entry, data ingestion) | ✓ (order entry, tight loops) | ✗ (acceptable for admin/monitoring) |
| **Binary compat** | No (explicit API) | No (explicit API) | ✓ (just prefix `onload`) |
| **Scaling** | Per-VI (dedicated cores) | Per-thread (assume single-threaded) | Per-process, multi-threaded okay |
| **AF_XDP support** | No (Solarflare hw only) | No (Solarflare hw only) | Yes (generic NICs via AF_XDP) |

---

## Part 8 — Building & Linking with ef_vi / TCPDirect

### OpenOnload Build

```bash
git clone https://github.com/Xilinx-CNS/onload.git
cd onload
./onload_build --install  # Install into /opt/onload or system paths
```

### Link Against ef_vi

```bash
gcc -o app app.c -I/opt/onload/include -L/opt/onload/lib -lef_vi -lpthread
```

### Link Against TCPDirect (if available)

```bash
# TCPDirect is typically built as part of OpenOnload
gcc -o app app.c -I/opt/onload/include -L/opt/onload/lib -lonload_zf -lpthread
```

### Runtime Environment

```bash
# If OpenOnload is in non-standard location
export LD_LIBRARY_PATH=/opt/onload/lib:$LD_LIBRARY_PATH

# Run with ef_vi directly
./app

# Run with Onload acceleration (if using BSD sockets)
onload ./app
```

---

## Part 9 — Common Gotchas & Best Practices

### 1. **DMA Buffer Alignment & Pinning**
   - Always `alignas(64)` or use `memalign(64, size)`.
   - Use mlocked (mlock) or hugetlbfs to ensure no page faults.
   - Test with `MAP_LOCKED` / `madvise(MADV_HUGEPAGE)`.

### 2. **Event Queue Overflow**
   - Size EVQ ≥ RX capacity + TX capacity + margin.
   - Monitor for `EF_EVENT_TYPE_*_ERROR` events (ring overrun).
   - Ensure hot-path events are polled frequently (microseconds).

### 3. **RX Descriptor Exhaustion**
   - If you don't re-post RX descriptors fast enough, ring runs dry → packet drops.
   - Use pre-allocated pools; poll/re-post in tight loop.

### 4. **Memory Ordering in Custom Rings**
   - If you build your own SPSC/MPSC rings over ef_vi buffers, use `std::atomic<size_t>` with proper memory order.
   - Don't assume sequential consistency; use `acquire`/`release` explicitly.

### 5. **Avoid Page Faults in Hot Path**
   - Pre-allocate all buffers at startup.
   - Disable ASLR (`echo 0 > /proc/sys/kernel/randomize_va_space`).
   - Use `mlockall()` sparingly (can cause latency elsewhere).

### 6. **Core Isolation & IRQ Affinity**
   - `isolcpus=X,Y,Z` in kernel boot parameters.
   - `nohz_full=X,Y,Z` to disable timer ticks.
   - Set thread affinity via `pthread_setaffinity_np()`.
   - Verify with `taskset -c <core> ./app`.

### 7. **Solarflare vs. AF_XDP Trade-offs**
   - **Solarflare (native ef_vi)**: Lowest latency, deterministic, no kernel AF_XDP overhead.
   - **AF_XDP (generic NICs)**: More portable, but higher latency (50–200 µs typical).

---

## Part 10 — Useful Links & Resources

- **GitHub Repo**: https://github.com/Xilinx-CNS/onload
- **Official Downloads**: https://www.xilinx.com/support/download/nic-software-and-drivers.html#open
- **Support (Commercial)**: support-nic@amd.com
- **Example Code** in repo: `src/examples/` (ef_vi_demo, packet capture, etc.)

---

## Changelog (Recent Versions)

### v9.0.2 (Oct 2024)
- Kernel 7.0 support
- Performance tuning for latest CPUs
- Bug fixes in AF_XDP integration

### v9.0.1 (Sep 2024)
- Stable release
- LTS kernel support (6.1, 6.6)

### v9.0.0 (Aug 2024)
- Major release
- Full C++20 support in examples
- Improved timestamping precision

### v8.1.3 (2023)
- Long-term support branch
- Backports for production stability

---

## Summary for Interview Prep

**For ULL trading systems:**
- Use **ef_vi** for market data ingestion (multicast, zero-copy, ~1 µs latency).
- Use **TCPDirect** for order entry (explicit TCP, bounded latency, hardware timestamps).
- Use **OpenOnload** for admin/monitoring (transparent sockets, no code changes).

**Key architectural pattern:**
```
┌─────────────────┐
│  Strategy Logic │
│  (1 core)       │
└────────┬────────┘
         │
    ┌────┴─────────────────────────────┐
    │                                   │
┌───▼──────────┐               ┌───────▼───────┐
│  SPMC        │               │  TCPDirect    │
│ Broadcast    │               │  Order Entry  │
│ Ring (ticks) │               │  (1 core)     │
└───▲──────────┘               └───────▲───────┘
    │                                  │
    │                                  │
┌───┴──────────┐               ┌───────┴───────┐
│  ef_vi       │               │  ef_vi        │
│  Multicast   │               │  TCP Stack    │
│  (1 core)    │               │  (Solarflare) │
└──────────────┘               └───────────────┘
```

Each VI runs on its own core → zero contention, deterministic latency, hardware steering.

---

**End of Reference**
