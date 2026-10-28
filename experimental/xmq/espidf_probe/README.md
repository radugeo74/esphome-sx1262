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
