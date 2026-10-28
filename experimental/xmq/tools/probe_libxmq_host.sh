#!/usr/bin/env bash
# Probe upstream libxmq portability without touching the production firmware.
set -euo pipefail
SHA="293fd38cdcca176091c789c33a8797d67471c060"
ROOT="https://raw.githubusercontent.com/wmbusmeters/wmbusmeters/${SHA}/src"
mkdir -p /tmp/wmbus-xmq-probe
cd /tmp/wmbus-xmq-probe
curl --fail --location --retry 3 --silent --show-error "${ROOT}/xmq.c" -o xmq.c
curl --fail --location --retry 3 --silent --show-error "${ROOT}/xmq.h" -o xmq.h
# Compile upstream amalgamation against host libxml2 headers. This does not prove ESP-IDF portability.
printf 'Pinned upstream commit: %s\n' "$SHA"
wc -c xmq.c xmq.h
printf '\nHeaders included by xmq.c:\n'
grep -E '^#[[:space:]]*include' xmq.c | sort -u || true
printf '\nBuilding with non-XML/JSON/printing options...\n'
gcc -std=gnu11 -O2 -Wall -Wno-unused-function \
  -DXMQ_NO_LIBXML -DXMQ_NO_JSON -DXMQ_NO_XMQ_PRINTING \
  -I. $(pkg-config --cflags libxml-2.0) -c xmq.c -o xmq.o
printf '\nCompiled object sizes:\n'
size xmq.o
printf '\nUndefined external symbols (portability inventory):\n'
nm -u xmq.o || true
