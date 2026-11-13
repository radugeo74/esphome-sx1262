# Dual-radio integration branch

Branch: `experimental/dual-radio-sx126x-sx127x` (forked from `experimental/xmq-runtime-engine`).

The `wmbus_radio` component supports exactly one radio source **per receiver**:

- `sx126x_id: native_radio` — existing patched SX126x native streaming implementation, hardware-tested on Heltec.
- `sx127x_id: sx127x_receiver` — new **experimental** patched official SX127x FSK FIFO-streaming T1+C1/A+B backend; build-only, not hardware-validated.

The same validated-frame decoder, DLL CRC checks, DLL/TPL ID routing, unhandled-meter logs and application drivers (`fhkvdataiii`, `mkradio4`, `mkradio4a`) remain shared. Both radio frontends are available as alternatives, not simultaneously on the same `wmbus_radio` instance.

**Experimental SX127x combined T1/C1:** `components/sx127x/` extends ESPHome 2026.10.1 with DIO1/FifoEmpty-driven 100 kb/s FIFO capture. Its high-priority task drains the 64-byte FIFO during the frame, computes the physical length from 3-of-6 (T1), the C1/A L-field plus CRC blocks, or the C1/B L-field (CRC included). The full frame enters the existing `wmbus_radio` length/CRC validator before any meter callbacks. **No raw or CRC-invalid frame may publish values.**

When `native_stream_mode: T1`, use `sync_value: [0x54, 0x3D]`: hardware consumes both bytes and the FIFO starts with T1-encoded data.

When `native_stream_mode: T1_C1`, use `sync_value: [0x54]`: the first byte in the FIFO is `0x3D` (T1 or C1/B) or `0xCD` (C1/A). Since T1 and C1/B share `54 3D`, both candidate lengths are considered independently. Invalid candidates are rejected by the shared DLL CRC verification. The C1/A and C1/B candidates are reconstructed with their `54 CD` / `54 3D` mode prefixes before invoking the common parser.

**Isolated SX1276 C1/A-only hardware-sync test (opt-in; T1 deliberately disabled):** Use `tests/lilygo-t3s3-sx1276-c1a-only-ci.yaml` as the sanitized build fixture. In the normal LILYGO YAML, under `sx127x:` add `wmbus_c1a_only: true` and change `sync_value` from `[0x54]` to **`[0x54, 0xCD]`**. Keep `wmbus_radio.native_stream_mode: T1_C1` to enable the existing shared C1 CRC decoder; it does *not* mean both RF modes are acquired during this test. Full two-byte sync consumes `54 CD` in the chip, so the **first FIFO byte is L**. The driver must not read an extra 0xCD byte. It reconstructs the full `54 CD` prefix before sending a C1/A candidate to the existing CRC validator. This is a controlled experiment to distinguish one-byte versus two-byte sync acquisition, **not** a permanent replacement for T1+C1 or an RF-validated fix.

The 60-second diagnostic now adds `hw_sync54CD_fifo`: number of FIFO-active acquisitions after hardware matching 54 CD. `marker_CD` stays **0 by design** in isolated mode, since CD is no longer in the FIFO. Use `hw_sync54CD_fifo > 0` as evidence that the two-byte sync produced FIFO bytes; `header_ok`, `queued`, and `crc_verified` identify later stages. The register read-back in `dump_config()` should show `RegSyncConfig[27]=0xB1` (two sync bytes) instead of 0xB0. In a simultaneous Heltec test, compare C1/A CRC-valid frame counts over the same time interval. Do not expect T1 from LILYGO while this test is active. To revert, remove `wmbus_c1a_only: true` and restore `sync_value: [0x54]`.

For either mode, keep `dio1_pin`, `wmbus_t1_mode: true`, `packet_mode: true`, `preamble_polarity: 0x55`, `crc_enable: false`, `payload_length: 0`, `bitrate: 100000`. **C1 encrypted payload still needs the meter AES key for application decoding; this PHY support does not bypass encryption.**

**FIFO reliability and automatic recovery (software only):** SX127x uses a 16-byte SPI FIFO burst only after the FSK `FifoLevel` flag confirms at least 16 bytes; the final 1–15 bytes are read only while DIO1/`FifoEmpty` indicates data. `FifoOverrun` aborts the frame before any dispatch. A 120ms end-to-end frame deadline prevents waiting forever on truncated T1/C1 packets. A one-second watchdog notices missed DIO1 wakeups, hardware RX timeouts and unexpected modem state; on failure it clears the FIFO, resets RX and discards stale notifications. An idle RX re-arm runs after 60 seconds without activity. Partial/CRC-invalid data never reaches meter sensors. The host test `tests/test_sx127x_fifo_guard.cpp` covers the FIFO-level/overflow and RX-state decision logic, not RF timing.

**LILYGO SX1276 RF tuning (after A/B log comparison):** SX127x native wM-Bus FSK now writes `RegAfcBw (0x13) = 0x10` (333.3 kHz) instead of retaining the SX1276 power-on value `0x0B` (50 kHz). The normal `RegRxBw` stays configurable with existing `sx127x.bandwidth`: keep `250_0kHz` (0x01) for the first AFC-only comparison; `200_0kHz` (0x09) is an optional separate future experiment. Existing `sx127x.rx_floor: -94` keeps RSSI threshold 0xBC. The proven reduction of false starts came from `preamble_detect: 2` (0xAA with tolerance 10), kept for this experiment. SX127x logs once during configure, before the FIFO task starts, the **actual read-back registers** RxBw, AfcBw, RxConfig, AfcFei, PreambleDetect, SyncConfig, and RssiThresh; it warns on discrepancies. For the LILYGO T1_C1 fixture, expect `0x01 0x10 0x1E 0x01 0xAA 0xB0 0xBC` in that order. `AfcAutoClearOn` remains bit 0 (`0x01`); do not copy the upstream's bit-4 write, which means AGC start. Do not change task priority/core or FIFO behavior in this AFC-only comparison. These changes do not affect SX126x.

**Cached FSK register snapshot on ESPHome API log reconnect:** `configure_fsk_ook_()` captures seven actual register values once at boot, before the FIFO RX task runs: RxBw, AfcBw, RxConfig, AfcFei, PreambleDetect, SyncConfig, and RssiThresh. `dump_config()` displays these **stored** values, plus `matches_expected`, even when a logger attaches through Wi-Fi long after reboot. No live SX1276 SPI access is performed by `dump_config()`, preventing interference with FIFO reception. This is a boot snapshot, not a continuous register monitor.

**SX1276 C1/A pre-CRC probe (real LILYGO comparison):** With the existing `wmbus_radio.native_phy_diagnostics: true`, `native_stream_mode: T1_C1` and `sx127x.sync_value: [0x54]`, two additional 60-second aggregate lines appear, entirely outside the high-priority FIFO ISR:
- `SX127x SYNC54 probe irq_at_fifo preamble=... sync=... marker_3D=... marker_CD=... other=... unreadable=...` — IRQ bits from `RegIrqFlags1` **are only sampled on FIFO-active task wakes using the existing read**. These are *not* counts of every RF preamble or failed sync. `3D` can mean T1 or C1/B; `CD` is the first FIFO evidence of a possible C1/A, before length or CRC checks.
- `SX127x C1A stages marker_CD=... header_ok=... header_bad=... body_complete=... queued=... queue_drop=... read_abort=... crc_verified=...` — `header_ok` is only an L-field-based expected length; `body_complete` means bytes were collected; `queued` means sent to the existing CRC decoder; `crc_verified` is the existing validated C1/A count. Values are cumulative since boot.
Interpretation: `marker_CD=0` while Heltec verifies C1/A means the loss happened **before a classified C1/A FIFO marker**, potentially in RF sensitivity, preamble acquisition, sync, or FIFO waking; these counters cannot distinguish those stages. If `marker_CD>0` and `header_ok=0`, inspect prefix capture. If `queued>0` but `crc_verified=0`, investigate CRC/PHY content. No RF settings, FIFO behavior, task priority, sensors, shared parser or SX126x changed. Only summaries are logged, no meter IDs or payload bytes.

**Opt-in SX127x PHY diagnostics:** set `wmbus_radio.native_phy_diagnostics: true`. Aggregated log snapshots every 60 seconds include received sync starts, T1/C1A/C1B candidates, CRC-verified telegrams, invalid 3-of-6 symbols, CRC/length rejections, frame/modem timeout, FIFO overrun, invalid markers/prefixes, missed DIO1 IRQ, queue drops and RX re-arms. The counters are atomic between the FIFO task and the main loop. No AES key, meter ID or packet bytes are logged by these summaries. The summary stays off by default.

RF status: LILYGO T3-S3 SX1276 **has received real CRC-valid T1** and one earlier C1/A, but its reception is considerably worse than Heltec SX1262 (T1 losses and no repeatable C1/A). **The new AFC setting is not yet RF-validated.** CI is build-only; keep the working Heltec firmware unchanged and compare both receivers before promotion. GPIOs in the LilyGO ESP-IDF fixture match the board under test; generic sx127x lab fixtures are placeholders.


## CI fixtures

- `tests/sx126x-native-espidf.yaml`: standalone SX126x baseline.
- `tests/sx126x-wmbus-bridge.yaml`: SX126x plus shared wM-Bus application pipeline.
- `tests/sx127x-wmbus-lab.yaml`: SX127x T1-only FSK FIFO streaming; synthetic meter ID.
- `tests/sx127x-wmbus-c1-lab.yaml`: SX127x combined T1+C1-A/B, single-byte sync; synthetic meter ID.
- `tests/test_sx127x_t1_length.cpp`: 3-of-6 and C1 length/collision candidates.
- `tests/test_sx127x_fifo_guard.cpp`: FIFO bursts, short tails, overrun and RX watchdog decisions.
- `tests/test_sx127x_rf_profile.cpp`: verifies SX1276 register addresses, RX 200/250 kHz, AFC 333.3 kHz (versus default 50 kHz) and proper AGC/AFC bits.
- `tests/test_sx127x_snapshot_contract.py`: asserts setup captures the register snapshot and `dump_config()` cannot read live SPI.
- `tests/test_c1_crc_safety.py`: real decoder/CRC functions under ASan+UBSan with complete/corrupt/truncated/back-to-back T1+C1-A+C1-B candidate streams.
- `tests/lilygo-t3s3-sx1276-ci.yaml`: SX1276 build-only on LILYGO T3-S3 ESP32-S3/ESP-IDF (SPI GPIO5/6/3, CS7, RST8, DIO0=9, DIO1=33) with diagnostics enabled.
- `tests/build_full_meter_fixture.py`: 12 synthetic meters and sensors for SX126x regressions.

CI on this branch validates and compiles both backends independently. Passing CI proves only schema and build compatibility, not SX127x RF correctness. No AES keys or real meter IDs are embedded.
