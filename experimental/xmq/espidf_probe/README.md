# ESP-IDF 5.5.6 feasibility probe

This minimal **ESP32-S3 baseline project** compiles an ESP-IDF firmware without
libxmq. It allows checking the toolchain and baseline flash/DRAM footprint.

The independent `probe_libxmq_espidf.sh` attempts to compile the **real**
upstream `xmq.c` with the ESP32-S3 compiler, reports errors and unresolved
symbols, and returns success even when the experiment fails (logging
`XMQ_ESP32_CROSS_COMPILE_STATUS=...`). This is intentionally a **diagnostic**,
not evidence that the engine works on ESP32.

When both tests complete, the next step is to resolve dependencies, integrate
libxmq into an ESP-IDF component, then compare build size and runtime heap
against the baseline. No production firmware or stable branch is modified.

## Result of first cross-compile (28 October 2026)

- ESP-IDF 5.5.6 baseline firmware for esp32s3: **built successfully**.
- Xtensa compiler: `xtensa-esp32s3-elf-gcc 14.2.0`: found and runnable.
- Upstream `xmq.c`, commit `293fd38cdcca176091c789c33a8797d67471c060`:
  `fatal error: libxml/tree.h: No such file or directory`.
- `XMQ_ESP32_CROSS_COMPILE_STATUS=1` means libxmq is **NOT compiled for ESP32-S3**.
- `XMQ_NO_LIBXML`, `XMQ_NO_JSON`, and `XMQ_NO_XMQ_PRINTING`
  did **not** prevent the libxml header dependency in this amalgamated source.
- Linux-host tests and original wmbusmeters decoding succeeding do not establish embedded portability.

### Next engineering decision

Investigate a real libxml2 DOM dependency/footprint and whether a small
portable XMQ/IXML subset can be extracted while retaining full driver semantics.
Do not fake libxml headers, assume flags remove the dependency, or mark
cross-compile as passed simply because the diagnostic script exits successfully.
A final ESP-IDF link test and runtime memory profile are still required.
