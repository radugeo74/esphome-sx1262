# Dual-radio integration branch

Branch: `experimental/dual-radio-sx126x-sx127x` (forked from `experimental/xmq-runtime-engine`).

The `wmbus_radio` component supports exactly one radio source **per receiver**:

- `sx126x_id: native_radio` — existing patched SX126x native streaming implementation, hardware-tested on Heltec.
- `sx127x_id: sx127x_receiver` — new **experimental** patched official SX127x FSK FIFO-streaming T1+C1/A+B backend; build-only, not hardware-validated.

The same validated-frame decoder, DLL CRC checks, DLL/TPL ID routing, unhandled-meter logs and application drivers (`fhkvdataiii`, `mkradio4`, `mkradio4a`) remain shared. Both radio frontends are available as alternatives, not simultaneously on the same `wmbus_radio` instance.

**Experimental SX127x combined T1/C1:** `components/sx127x/` extends ESPHome 2026.10.1 with DIO1/FifoEmpty-driven 100 kb/s FIFO capture. Its high-priority task drains the 64-byte FIFO during the frame, computes the physical length from 3-of-6 (T1), the C1/A L-field plus CRC blocks, or the C1/B L-field (CRC included). The full frame enters the existing `wmbus_radio` length/CRC validator before any meter callbacks. **No raw or CRC-invalid frame may publish values.**

When `native_stream_mode: T1`, use `sync_value: [0x54, 0x3D]`: hardware consumes both bytes and the FIFO starts with T1-encoded data.

When `native_stream_mode: T1_C1`, use `sync_value: [0x54]`: the first byte in the FIFO is `0x3D` (T1 or C1/B) or `0xCD` (C1/A). Since T1 and C1/B share `54 3D`, both candidate lengths are considered independently. Invalid candidates are rejected by the shared DLL CRC verification. The C1/A and C1/B candidates are reconstructed with their `54 CD` / `54 3D` mode prefixes before invoking the common parser.

For either mode, keep `dio1_pin`, `wmbus_t1_mode: true`, `packet_mode: true`, `preamble_polarity: 0x55`, `crc_enable: false`, `payload_length: 0`, `bitrate: 100000`. **C1 encrypted payload still needs the meter AES key for application decoding; this PHY support does not bypass encryption.**

**FIFO reliability and automatic recovery (software only):** SX127x uses a 16-byte SPI FIFO burst only after the FSK `FifoLevel` flag confirms at least 16 bytes; the final 1–15 bytes are read only while DIO1/`FifoEmpty` indicates data. `FifoOverrun` aborts the frame before any dispatch. A 120ms end-to-end frame deadline prevents waiting forever on truncated T1/C1 packets. A one-second watchdog notices missed DIO1 wakeups, hardware RX timeouts and unexpected modem state; on failure it clears the FIFO, resets RX and discards stale notifications. An idle RX re-arm runs after 60 seconds without activity. Partial/CRC-invalid data never reaches meter sensors. The host test `tests/test_sx127x_fifo_guard.cpp` covers the FIFO-level/overflow and RX-state decision logic, not RF timing.

**Opt-in SX127x PHY diagnostics:** set `wmbus_radio.native_phy_diagnostics: true`. Aggregated log snapshots every 60 seconds include received sync starts, T1/C1A/C1B candidates, CRC-verified telegrams, invalid 3-of-6 symbols, CRC/length rejections, frame/modem timeout, FIFO overrun, invalid markers/prefixes, missed DIO1 IRQ, queue drops and RX re-arms. The counters are atomic between the FIFO task and the main loop. No AES key, meter ID or packet bytes are logged by these summaries. The summary stays off by default.

This is **not RF-validated**. The SX127x FIFO timing, sync acquisition, and restart behavior need a dedicated hardware test. Do not flash as a replacement for the working SX126x firmware until tests pass. The CI GPIOs are placeholders.


## CI fixtures

- `tests/sx126x-native-espidf.yaml`: standalone SX126x baseline.
- `tests/sx126x-wmbus-bridge.yaml`: SX126x plus shared wM-Bus application pipeline.
- `tests/sx127x-wmbus-lab.yaml`: SX127x T1-only FSK FIFO streaming; synthetic meter ID.
- `tests/sx127x-wmbus-c1-lab.yaml`: SX127x combined T1+C1-A/B, single-byte sync; synthetic meter ID.
- `tests/test_sx127x_t1_length.cpp`: 3-of-6 and C1 length/collision candidates.
- `tests/test_sx127x_fifo_guard.cpp`: FIFO bursts, short tails, overrun and RX watchdog decisions.
- `tests/test_c1_crc_safety.py`: real decoder/CRC functions under ASan+UBSan with complete/corrupt/truncated/back-to-back T1+C1-A+C1-B candidate streams.
- `tests/lilygo-t3s3-sx1276-ci.yaml`: SX1276 build-only on LILYGO T3-S3 ESP32-S3/ESP-IDF (SPI GPIO5/6/3, CS7, RST8, DIO0=9, DIO1=33) with diagnostics enabled.
- `tests/build_full_meter_fixture.py`: 12 synthetic meters and sensors for SX126x regressions.

CI on this branch validates and compiles both backends independently. Passing CI proves only schema and build compatibility, not SX127x RF correctness. No AES keys or real meter IDs are embedded.
