# SX1276 RAW DATA/DCLK — RX bandwidth 250 kHz A/B experiment

**Scope:** LILYGO T3-S3 SX1276 only. This branch is copied from the known v5 RAW gate (`experimental/sx127x-raw-dclk-v5-t1-gate`) with no firmware/driver changes. Existing Heltec SX126x, XMQ, wmbus_common, CRC checking, RAW T1 gate and C1 processing are untouched. This is a **YAML-only RF trial**, not a demonstrated RF improvement.

## Test (real LILYGO YAML only)

1. Baseline A: use `github://radugeo74/esphome-sx1262@experimental/sx127x-raw-dclk-v5-t1-gate` in `external_components.source`, with `sx127x.bandwidth: 200_0kHz`. Keep all other configuration unchanged.
2. Candidate B: switch only `sx127x.bandwidth: 250_0kHz`. Optionally use the tracking branch `github://radugeo74/esphome-sx1262@experimental/sx127x-raw-rxbw250-ab`; it has the **same firmware** as v5. Ideally keep the same branch in A and B to isolate just one YAML variable.
3. Verify boot snapshot: B must have `RegRxBw[12]=0x01` (250 kHz), `RegAfcBw[13]=0x10` (333.3 kHz), auto AFC/AGC enabled, FSK profile matches expected TRUE. A uses `RegRxBw[12]=0x09` (200 kHz).
4. Capture both LILYGO and Heltec on exactly the same clock-time window for 10–15 minutes each (A and B). Keep antenna, placement, power supply, Wi-Fi, frequency 868.95 MHz, bitrate 100 kb/s, deviation 50 kHz, bitsync, preamble settings, rx_floor and source branch otherwise unchanged. No new sensors.
5. Calculate T1/A and C1/A **CRC-valid** per minute, their ratio to Heltec in each window, and matching unique packet hex frames (if available). Also monitor LILYGO bad_symbol, bad_crc, ring_overflow, queue_drop. Never compare SX126x cumulative since-boot counters directly with per-window LILYGO counts.
6. If T1 or C1 consistently regresses under B, restore `bandwidth: 200_0kHz`; compare again with A if practical.

**Why this experiment?** RxBw is already exposed in YAML and hard-coding a new value would obscure the controlled test. Changing 200→250 kHz trades more accepted signal bandwidth for increased noise bandwidth. Prior tests of 200/250 kHz were run in a different, C1-only hardware sync mode; they do **not** establish the outcome for the current combined RAW DCLK T1/C1 configuration.

**Status:** branch and documentation prepared; no ESPHome compile / CI / hardware RF test executed here.
