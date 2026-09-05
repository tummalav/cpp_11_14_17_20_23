# Solarflare APIs — Clone Status ✅

## What Was Cloned

**Repository:** Xilinx-CNS/onload  
**URL:** https://github.com/Xilinx-CNS/onload  
**Local Path:** `/Users/haritha/github_repos/onload`  
**Size:** ~38 MB  
**Branch:** master (latest)

---

## Three APIs Available

### 1. ✅ **ef_vi (EtherFabric Virtual Interface)** — FULLY AVAILABLE
**Low-level hardware access, zero-copy RX/TX, direct DMA**

**Location:** `src/include/etherfabric/`

Headers included:
- `ef_vi.h` (3,174 lines) — Core API
- `vi.h` (1,240 lines) — VI implementation
- `pd.h` — Protection domain
- `capabilities.h` — Hardware features
- `memreg.h` — Memory registration
- `base.h`, `checksum.h`, `timer.h`, `pio.h`, `efct_vi.h`, `packedstream.h`

**Total:** 6,216 lines of header files

**Example:**
```cpp
#include <etherfabric/ef_vi.h>

// Allocate protection domain
ef_pd* pd;
ef_pd_alloc_by_name(&pd, dh, "eth0");

// Allocate VI with 512-slot RX/TX rings
ef_vi* vi;
ef_vi_alloc_from_pd(&vi, dh, pd, dh, 512, 512, 512, NULL, 0, 0);

// Poll events
ef_event ev[16];
int n = ef_eventq_poll(vi, ev, 16);
```

---

### 2. ✅ **OpenOnload (Transparent Socket Stack)** — FULLY AVAILABLE
**Kernel module + user-level TCP/UDP stack, binary-compatible**

**Location:** 
- Headers: `src/include/onload/`
- Implementation: `src/lib/ciul/`, `src/lib/transport/`
- Kernel module: `src/driver/linux_onload/`

Key components:
- `src/lib/transport/ip/` — TCP/UDP protocol stack (1000s of .c files)
- `src/lib/ciul/` — User-level I/O
- `src/driver/linux_onload/` — Kernel module

**Usage:**
```bash
# Transparent acceleration (no code changes)
onload ./myapp

# MyApp uses normal BSD sockets
# OpenOnload intercepts and accelerates them
```

---

### 3. ⚠️ **TCPDirect (Zero-Fabric / ZF)** — PARTIAL (Open-Source Foundation Only)
**Explicit ultra-low-latency TCP API**

**Status:** 
- ✅ Foundation layers available (transport stack, kernel module)
- ❌ Full TCPDirect commercial APIs not included in open-source repo

**Location:** Integrated into `src/lib/transport/` and `src/lib/ciul/`

**Why?**
TCPDirect is primarily a **commercial/proprietary offering** from Xilinx/AMD.
The open-source repo includes the underlying transport stack that TCPDirect
leverages, but the full explicit API is available under a commercial license.

**What you can study:**
- TCP/UDP implementation: `src/lib/transport/ip/`
- Socket handling: `src/lib/transport/unix/`
- Hardware integration: ef_vi layer

**For full TCPDirect API:**
- Contact Xilinx/AMD support
- Request from: support-nic@amd.com
- Commercial license required

---

## Repository Structure Quick Map

```
onload/
├── src/
│   ├── include/etherfabric/     ← ef_vi headers (API you'll study)
│   ├── include/onload/          ← OpenOnload headers
│   ├── lib/
│   │   ├── transport/           ← TCP/UDP stack (TCPDirect foundation)
│   │   ├── ciul/                ← User-level I/O
│   │   └── ...
│   ├── driver/                  ← Kernel module source
│   └── tests/                   ← Test code
├── README.md                    ← Start here
├── DEVELOPING.md                ← Build instructions
├── API_COMPONENTS_SUMMARY.md    ← (Created by us)
└── .git/                        ← Full git history

```

---

## Quick Start

### 1. Explore the API Headers
```bash
cd /Users/haritha/github_repos/onload

# Main ef_vi API (read this)
less src/include/etherfabric/ef_vi.h

# VI implementation details
less src/include/etherfabric/vi.h

# OpenOnload socket integration
less src/include/onload/tcp_helper.h
```

### 2. Read Documentation
```bash
# Project overview
cat README.md

# Build & development guide
cat DEVELOPING.md
```

### 3. Understand the Stack
```bash
# TCP implementation
ls -la src/lib/transport/ip/tcp*.c | head -20

# Memory management
ls -la src/lib/transport/ip/mem*.c

# Filter/packet steering
ls -la src/lib/efrm/
```

### 4. Build (Optional)
```bash
cd /Users/haritha/github_repos/onload

# View build options
./onload_build --help

# Minimal build (no hardware dependencies)
./onload_build --no-sfc --no-efct

# Full build with Solarflare HW support
./onload_build --install
```

---

## What You Can Do Now

✅ **Study ef_vi API** — Read headers, understand DMA rings, event queues, timestamps  
✅ **Study OpenOnload** — Understand BSD socket interception, protocol stack  
✅ **Study TCP/UDP implementation** — Source code in `src/lib/transport/`  
✅ **Run tests** — Unit tests in `src/tests/`  
✅ **Build from source** — Full build system included  

❌ **TCPDirect explicit API** — Use foundation layer + contact Xilinx for commercial APIs

---

## Interview Prep Recommendations

### For Low-Latency Trading Interviews (Millennium, Jane Street, etc.)

**Study in this order:**
1. **ef_vi.h** — Understand the hardware abstraction layer
2. **RX/TX path** — How packets flow from NIC to memory
3. **Event queue** — How completion notifications work
4. **Memory ordering** — acquire/release semantics in ef_vi
5. **Lock-free patterns** — How to build SPSC/MPSC rings on top of ef_vi
6. **OpenOnload TCP stack** — How it intercepts sockets and accelerates them

**Key Questions to Understand:**
- How does ef_vi zero-copy work?
- What's the latency path from packet arrival to app notification?
- How does hardware timestamping work?
- How do you handle backpressure in a busy-poll RX loop?
- What's the fastest way to post RX descriptors?
- How do you avoid event queue overflow?

**Example implementations in repo:**
- Check `src/tests/` for actual ef_vi usage examples
- Look at `src/lib/transport/ip/` to see how OpenOnload implements TCP

---

## File Summary

| File | Purpose |
|---|---|
| `/Users/haritha/github_repos/onload/README.md` | Overview & getting started |
| `/Users/haritha/github_repos/onload/DEVELOPING.md` | Build instructions |
| `/Users/haritha/github_repos/onload/src/include/etherfabric/ef_vi.h` | **Core ef_vi API** |
| `/Users/haritha/github_repos/onload/src/include/etherfabric/vi.h` | VI internals |
| `/Users/haritha/github_repos/onload/src/lib/transport/` | TCP/UDP stack |
| `/Users/haritha/github_repos/onload/src/driver/linux_onload/` | Kernel module |
| `/Users/haritha/github_repos/onload/API_COMPONENTS_SUMMARY.md` | (Created by us - structure guide) |
| `/Users/haritha/github_repos/solarflare_apis_reference.md` | (Created by us - API reference) |

---

## Summary

🎯 **You now have:**
- Full ef_vi API source (headers + implementation)
- Full OpenOnload source (user-level stack + kernel module)
- Full TCP/UDP protocol stack (TCPDirect foundation)
- Complete git history and build system

🚀 **Next:** Start with `README.md` and `src/include/etherfabric/ef_vi.h`

