# Solarflare OpenOnload Repository — APIs Structure

**Cloned Location:** `/Users/haritha/github_repos/onload`  
**Repository:** https://github.com/Xilinx-CNS/onload  
**Size:** ~38 MB  
**License:** GPLv2 + BSD-2-Clause

---

## Directory Structure Summary

```
onload/
├── src/
│   ├── include/
│   │   ├── etherfabric/          ← ef_vi API headers
│   │   │   ├── ef_vi.h           (126 KB — main ef_vi API)
│   │   │   ├── vi.h              (50 KB — VI implementation)
│   │   │   ├── pd.h              (protection domain)
│   │   │   ├── base.h            (base types)
│   │   │   ├── capabilities.h    (hardware features)
│   │   │   ├── checksum.h
│   │   │   ├── memreg.h          (memory registration)
│   │   │   ├── timer.h
│   │   │   ├── efct_vi.h         (EFCT extensions)
│   │   │   ├── packedstream.h
│   │   │   ├── pio.h             (Programmed I/O)
│   │   │   └── internal/
│   │   │
│   │   ├── onload/               ← OpenOnload stack headers
│   │   │   ├── tcp_helper.h
│   │   │   ├── tcp_driver.h
│   │   │   ├── tcp_helper_fns.h
│   │   │   ├── drv/              (driver interface)
│   │   │   └── ul/               (user-level)
│   │   │
│   │   └── ci/                   ← Common Infrastructure
│   │       ├── net/              (networking)
│   │       ├── internal/         (TCP stats, internals)
│   │       ├── efhw/             (hardware-level)
│   │       └── driver/           (driver interface)
│   │
│   ├── lib/                      ← Library implementations
│   │   ├── ciapp/                (common app utilities)
│   │   ├── citools/              (tools)
│   │   ├── ciul/                 (user-level I/O)
│   │   ├── cplane/               (control plane)
│   │   ├── efhw/                 (hardware abstraction)
│   │   ├── efrm/                 (EtherFabric resource management)
│   │   ├── efthrm/               (threading/resource management)
│   │   ├── transport/            (TCP/UDP protocol stack)
│   │   │   ├── ip/
│   │   │   ├── unix/
│   │   │   └── ...
│   │   └── onload_ext/
│   │
│   ├── driver/                   ← Kernel module source
│   │   ├── linux_onload/         (main Onload driver)
│   │   ├── linux_net/            (network device drivers)
│   │   └── linux_resource/
│   │
│   └── tests/                    ← Unit & integration tests
│       ├── unit/
│       └── onload/
│
├── scripts/                      ← Build & setup scripts
│   ├── debian/
│   ├── onload_profiles/
│   └── onload_apps/
│
├── mk/                           ← Build system
│   ├── platform/
│   └── site/
│
└── .github/
    └── workflows/                ← CI/CD (GitHub Actions)
```

---

## Component Breakdown

### 1. **ef_vi API** (EtherFabric Virtual Interface)
**Location:** `src/include/etherfabric/`

**Main Headers:**
- **ef_vi.h** (126 KB) — Core API
  - `ef_pd` — Protection Domain allocation
  - `ef_vi` — Virtual Interface allocation/management
  - `ef_event` — Event types and structures
  - RX/TX descriptor posting
  - Event queue polling
  - Timestamping functions
  
- **vi.h** (50 KB) — VI implementation details
  - Hardware-specific optimizations
  - Descriptor ring management
  - Inline functions for fast path

- **pd.h** — Protection Domain API
  - `ef_pd_alloc_by_name()`
  - Resource isolation

- **capabilities.h** — Hardware capability flags
  - Feature detection (timestamps, filtering, etc.)

- **memreg.h** — DMA memory registration
  - Buffer pinning/registration

**Example Usage:**
```cpp
#include <etherfabric/ef_vi.h>

ef_pd* pd;
ef_vi* vi;
ef_event events[16];

ef_pd_alloc_by_name(&pd, dh, "eth0");
ef_vi_alloc_from_pd(&vi, dh, pd, dh, 512, 512, 512, NULL, 0, 0);
ef_eventq_poll(vi, events, 16);
```

---

### 2. **TCPDirect (Zero-Fabric / ZF)**
**Location:** `src/lib/` (distributed across libraries)

**Components:**
- Not a standalone directory, but integrated into:
  - `ciul/` — User-level I/O (transport layer)
  - `transport/` — TCP/UDP protocol stack
  - `ciapp/` — Application utilities
  
**Note:** TCPDirect (commercial/advanced feature) may not be fully open-sourced in this repo. The core OpenOnload stack provides TCP/UDP via the `transport/` library, which is the foundation for TCPDirect.

**Key Protocol Files:**
- `src/lib/transport/ip/tcp_*.c` — TCP implementation
- `src/lib/transport/unix/tcp_fd.c` — TCP socket handling

**Example (implicit in libonload):**
```cpp
// TCPDirect is typically exposed as a linked library or set of APIs
// in the proprietary ZeroFabric package. The open-source repo includes
// the underlying transport stack that TCPDirect leverages.
```

---

### 3. **OpenOnload Stack** (Transparent Socket Acceleration)
**Location:** `src/lib/` + `src/driver/`

**User-Level Components:**
- `src/lib/ciul/` — Core user-level I/O
- `src/lib/transport/` — Full TCP/UDP/IP stack (userspace)
- `src/lib/citools/` — Utilities

**Kernel Module:**
- `src/driver/linux_onload/` — Main Onload driver
- `src/driver/linux_net/` — NIC driver interface
- `src/driver/linux_resource/` — Resource management

**Key Headers:**
- `src/include/onload/tcp_helper.h` — TCP socket helper
- `src/include/onload/tcp_driver.h` — Driver interface
- `src/include/onload/ul/tcp_helper.h` — User-level TCP

---

## How to Build

```bash
cd /Users/haritha/github_repos/onload

# View build options
./onload_build --help

# Build and install to /opt/onload
./onload_build --install

# Or build only ef_vi (if building subset)
# See DEVELOPING.md for detailed instructions
```

---

## Key Files to Study

### For ef_vi Learning:
1. **src/include/etherfabric/ef_vi.h** — Read complete API
2. **src/include/etherfabric/vi.h** — Understand VI internals
3. **DEVELOPING.md** — Build instructions
4. **README.md** — Overview & getting started

### For OpenOnload Learning:
1. **src/include/onload/tcp_helper.h** — Socket acceleration
2. **src/lib/transport/ip/** — TCP/UDP protocol stack
3. **src/driver/linux_onload/** — Kernel module

### For TCPDirect:
- TCPDirect is primarily a commercial/proprietary offering
- The open-source repo contains the underlying transport layer (`src/lib/transport/`)
- Full TCPDirect API available under commercial license from Xilinx/AMD

---

## Test & Example Code

```bash
# Find test files
find /Users/haritha/github_repos/onload/src/tests -name "*.c" | head -10

# Common test directories
src/tests/unit/lib/transport/
src/tests/onload/oof/

# Build tests
cd onload
./onload_build  # builds tests as well
```

---

## What You Have Now

✅ **Full OpenOnload source** — transparent socket acceleration  
✅ **Complete ef_vi API headers** — low-level hardware interface  
✅ **TCP/UDP stack implementation** — protocol layer  
✅ **Kernel module source** — driver code  
✅ **Build system** — Makefile-based (mk/)  

⚠️ **TCPDirect details** — Limited in open-source repo (commercial feature)  
⚠️ **Pre-built binaries** — Not included; must build from source

---

## Next Steps

```bash
# 1. Read the documentation
cat /Users/haritha/github_repos/onload/README.md

# 2. Read DEVELOPING.md for build details
cat /Users/haritha/github_repos/onload/DEVELOPING.md

# 3. Explore headers
less /Users/haritha/github_repos/onload/src/include/etherfabric/ef_vi.h

# 4. Build (if you want binaries)
cd /Users/haritha/github_repos/onload
./onload_build --no-sfc --no-efct  # Minimal build (no Solarflare HW deps)
```

