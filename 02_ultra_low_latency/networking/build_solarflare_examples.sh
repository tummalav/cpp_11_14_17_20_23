#!/usr/bin/env bash
set -euo pipefail

# Build helper for Solarflare networking examples in this directory.
#
# Optional environment:
#   SOLARFLARE_HOME=/opt/onload
#
# If SOLARFLARE_HOME is set and the libraries exist, this script also builds
# TCPDirect-linked binaries using:
#   libzf.so / libzf.a
#   libonload_ext.so / libonload_ext.a

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

echo "[1/4] Building fallback examples (portable)..."
g++ -std=c++17 -O2 -pthread solarflare_onload_tcp_example.cpp -o solarflare_onload_tcp_example
g++ -std=c++17 -O2 -pthread solarflare_onload_udp_example.cpp -o solarflare_onload_udp_example
g++ -std=c++17 -O2 -pthread solarflare_tcpdirect_example.cpp -o solarflare_tcpdirect_example
g++ -std=c++17 -O2 -pthread solarflare_ef_vi_example.cpp -o solarflare_ef_vi_example

if [[ -n "${SOLARFLARE_HOME:-}" ]]; then
  INC="${SOLARFLARE_HOME}/include"
  LIB="${SOLARFLARE_HOME}/lib"
  echo "[2/4] SOLARFLARE_HOME=${SOLARFLARE_HOME}"

  if [[ -d "${INC}" && -d "${LIB}" ]]; then
    if [[ -f "${LIB}/libzf.so" || -f "${LIB}/libzf.a" ]]; then
      echo "[3/4] Building TCPDirect-linked example..."
      g++ -std=c++17 -O2 -pthread -DUSE_TCPDIRECT solarflare_tcpdirect_example.cpp \
          -I"${INC}" -L"${LIB}" -Wl,-rpath,"${LIB}" \
          -lzf -lonload_ext -ldl -lrt \
          -o solarflare_tcpdirect_example_tcpdirect
    else
      echo "Skipping TCPDirect build: libzf(.so/.a) not found in ${LIB}"
    fi

    if [[ -f "${LIB}/libetherfabric.so" || -f "${LIB}/libetherfabric.a" ]]; then
      echo "[4/4] Building ef_vi-linked example..."
      g++ -std=c++17 -O2 -pthread -DUSE_EFVI solarflare_ef_vi_example.cpp \
          -I"${INC}" -L"${LIB}" -Wl,-rpath,"${LIB}" \
          -letherfabric \
          -o solarflare_ef_vi_example_efvi
    else
      echo "Skipping ef_vi build: libetherfabric(.so/.a) not found in ${LIB}"
    fi
  else
    echo "Skipping Solarflare-linked builds: missing ${INC} or ${LIB}"
  fi
fi

echo "Build complete."
