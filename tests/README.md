# Dual-radio integration branch

Branch: `experimental/dual-radio-sx126x-sx127x` (forked from `experimental/xmq-runtime-engine`).

The `wmbus_radio` component supports exactly one radio source **per receiver**:

- `sx126x_id: native_radio` — existing patched SX126x native streaming implementation, hardware-tested on Heltec.
- `sx127x_id: sx127x_receiver` — new **experimental** patched official SX127x FSK FIFO-streaming T1 backend; build-only, not hardware-validated.

The same validated-frame decoder, DLL CRC checks, DLL/TPL ID routing, unhandled-meter logs and application drivers (`fhkvdataiii`, `mkradio4`, `mkradio4a`) remain shared. Both radio frontends are available as alternatives, not simultaneously on the same `wmbus_radio` instance.

**Experimental SX127x T1 streaming (software candidate):** `components/sx127x/` is a local extension of ESPHome 2026.10.1 with DIO1/FifoEmpty-driven 100 kb/s T1 capture. It reads bytes before the 64-byte FIFO fills, derives full raw physical length from EN13757-4 3-of-6 symbols, queues complete raw streams and feeds them to the same CRC-checked `wmbus_radio` decoder as SX126x. The SX127x hardware removes the configured `54 3D` sync bytes before FIFO; do not prepend them to T1 raw data. This implementation is **not RF-validated**, and timing/restart behavior may need adjustments on hardware. C1 is **not supported on SX127x** at this stage; `native_stream_mode: T1_C1` is explicitly rejected. Do not flash as a substitute for the working SX126x firmware until RF tests are successful.

Use the local external-component override (`sx127x`) and set `dio1_pin`, `wmbus_t1_mode: true`, `sync_value: [0x54, 0x3D]`, `packet_mode: true`, `payload_length: 0`, and `bitrate: 100000` in the experimental SX127x YAML. Pin values in CI are placeholders; actual modules differ in wiring and RF-band matching.

## CI fixtures

- `tests/sx126x-native-espidf.yaml`: standalone SX126x baseline.
- `tests/sx126x-wmbus-bridge.yaml`: SX126x plus shared wM-Bus application pipeline.
- `tests/sx127x-wmbus-lab.yaml`: build-only SX127x packet listener + same wM-Bus meter pipeline; synthetic meter ID and placeholder GPIOs.
- `tests/build_full_meter_fixture.py`: 12 synthetic meters and sensors for SX126x regressions.

CI on this branch validates and compiles both backends independently. Passing CI proves only schema and build compatibility, not SX127x RF correctness. No AES keys or real meter IDs are embedded.
