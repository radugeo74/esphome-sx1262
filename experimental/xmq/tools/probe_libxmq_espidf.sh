#!/usr/bin/env bash
# Non-destructive upstream XMQ cross-compiler feasibility probe.
# Deliberately compile the actual upstream amalgamation (not a fake stub).
set -euo pipefail
SHA=293fd38cdcca176091c789c33a8797d67471c060
ROOT="https://raw.githubusercontent.com/wmbusmeters/wmbusmeters/${SHA}/src"
mkdir -p /tmp/xmq-espidf-probe
cd /tmp/xmq-espidf-probe
curl -fL --retry 3 -sS "${ROOT}/xmq.c" -o xmq.c
curl -fL --retry 3 -sS "${ROOT}/xmq.h" -o xmq.h
echo "ESP-IDF compiler: $(command -v xtensa-esp32s3-elf-gcc)"
xtensa-esp32s3-elf-gcc --version | head -n 1
# First attempt intentionally has no libxml2 or platform shims.
# Missing headers/symbols are actionable portability findings.
set +e
xtensa-esp32s3-elf-gcc -std=gnu11 -Os -ffunction-sections -fdata-sections \
  -DXMQ_NO_LIBXML -DXMQ_NO_JSON -DXMQ_NO_XMQ_PRINTING \
  -I. -c xmq.c -o xmq.o > compile.log 2>&1
rc=$?
set -e
echo "Cross-compile exit status: $rc"
if [[ $rc -eq 0 ]]; then
    xtensa-esp32s3-elf-size xmq.o
    xtensa-esp32s3-elf-nm -u xmq.o | head -n 100 || true
else
    echo "First 100 lines of compiler diagnostics:"
    sed -n '1,100p' compile.log
fi
# Expected feasibility probes are informational; do not hide failures.
echo "XMQ_ESP32_CROSS_COMPILE_STATUS=$rc"
