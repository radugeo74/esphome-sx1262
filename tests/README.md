# Dual-radio integration branch

Branch: `experimental/dual-radio-sx126x-sx127x` (forked from `experimental/xmq-runtime-engine`).

The `wmbus_radio` component supports exactly one radio source **per receiver**:

- `sx126x_id: native_radio` — existing patched SX126x native streaming implementation, hardware-tested on Heltec.
- `sx127x_id: sx127x_receiver` — new **experimental** official SX127x FSK packet-listener backend; build-only, not hardware-validated.

The same validated-frame decoder, DLL CRC checks, DLL/TPL ID routing, unhandled-meter logs and application drivers (`fhkvdataiii`, `mkradio4`, `mkradio4a`) remain shared. Both radio frontends are available as alternatives, not simultaneously on the same `wmbus_radio` instance.

**Important limitation:** ESPHome 2026.10.x SX127x FSK packet mode has a 64-byte FIFO; typical raw T1 3-of-6 frames and C1 frames may require >64 bytes. In packet mode the sync marker is stripped by the radio, while the current native decoder expects a PHY stream. The SX127x adapter is groundwork for compilation/integration testing only: it is **not yet a working full T1/C1 wM-Bus receiver**. Do not flash it as a replacement for the SX126x firmware. Full support needs a separate SX127x streaming/FIFO backend with correct sync, packet boundaries and RSSI semantics.

## CI fixtures

- `tests/sx126x-native-espidf.yaml`: standalone SX126x baseline.
- `tests/sx126x-wmbus-bridge.yaml`: SX126x plus shared wM-Bus application pipeline.
- `tests/sx127x-wmbus-lab.yaml`: build-only SX127x packet listener + same wM-Bus meter pipeline; synthetic meter ID and placeholder GPIOs.
- `tests/build_full_meter_fixture.py`: 12 synthetic meters and sensors for SX126x regressions.

CI on this branch validates and compiles both backends independently. Passing CI proves only schema and build compatibility, not SX127x RF correctness. No AES keys or real meter IDs are embedded.
