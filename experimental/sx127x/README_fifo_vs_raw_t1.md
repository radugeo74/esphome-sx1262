# LILYGO T3-S3 SX1276 — controlled FIFO versus RAW DATA/DCLK test

Dedicated branch: `experimental/sx127x-fifo-vs-raw-t1`.

**Purpose:** determine whether T1 symbol/CRC errors come chiefly from the
SX1276 demodulated bitstream / RF conditions or the ESP32-S3 DCLK sampling
path. Do NOT change RF bandwidth/AFC between runs. Only one RX path runs at
a time: this is an A/B firmware comparison, not simultaneous reception.

## Common settings (both A and B)

In your existing LILYGO YAML, point the `external_components` GitHub
source at this branch. Retain the current Wi-Fi, API, SPI, pin, radio ID,
meter definitions, and other existing configuration. Do **not** add sensors.

Within **your existing** `sx127x:` block (preserve its `id` and other
unchanged settings), use the same FSK profile in both tests:

```yaml
  modulation: FSK
  frequency: 868.95MHz
  bitrate: 100000
  deviation: 50kHz
  bandwidth: 125_0kHz
  afc_auto: false
  agc_auto: true
  bitsync: true
  preamble_detect: 2
  preamble_errors: 10
  preamble_size: 4
  preamble_polarity: 0x55
  payload_length: 0
  crc_enable: false
  rx_start: true
  rx_floor: -94
  wmbus_t1_mode: true
  wmbus_dual_sync: false
  wmbus_c1a_only: false
  wmbus_continuous_dual: false
```

For `wmbus_radio:`, set `native_stream_mode: T1` in **both** runs.
Keep `sx127x_id` pointing to the existing radio ID, and use
`native_phy_diagnostics: true` so each firmware emits the common
`SX127x T1 A/B mode=...` statistics line.

### A — RAW / DATA-DCLK, T1-only

Add or replace these keys in the existing `sx127x:` block:

```yaml
  packet_mode: false
  wmbus_raw_dclk: true
  dio1_pin: GPIO33       # DCLK
  dio2_pin: GPIO34       # DATA
  sync_value: [0x54]    # software searches for 54 3D T1
  wmbus_raw_diag_capture: false
  wmbus_t1_recovery_diag: false
```

Notes: RAW does not use the SX1276 hardware sync gate; C1/A candidates
are **not queued** in this T1-only test. Recovery diagnostic is disabled
so it cannot inflate the on-air T1 success result.

### B — FIFO / SPI, T1-only

Change **only the path-specific keys** to:

```yaml
  packet_mode: true
  wmbus_raw_dclk: false
  dio1_pin: GPIO33       # FIFO empty interrupt; DIO1 mapping changes in firmware
  dio2_pin: GPIO34       # unused by this mode
  sync_value: [0x54, 0x3D]  # SX1276 full hardware sync; bytes consumed by radio
  wmbus_raw_diag_capture: false
  wmbus_t1_recovery_diag: false
```

Internally, B uses the existing SX127x FIFO reader. DIO1 is mapped to
`FifoEmpty`, SPI reads bursts when `FifoLevel` asserts, and the
existing `wmbus_radio` decoder does T1 3-of-6 and Format A CRC
validation. There is **no software Hamming correction** in B.

## Measurements

1. Run A for 5–10 minutes; preserve all logs including the first
   FSK register readback and at least five 60-second reports. Record
   elapsed time, same board, same antenna, same position, same USB supply.
2. Reflash B (do not move antenna/board); run the same duration and
   preserve logs.
3. Optionally reflash A again, to check whether RF conditions changed
   over time (A-B-A cross-check).
4. Compare only T1, not C1. Do not count PHY sync starts as received
   telegrams or assume each candidate corresponds to a distinct on-air
   transmission.
5. Main diagnostic to compare at each checkpoint:

```text
SX127x T1 A/B mode=RAW_DCLK t1_verified=... t1_bad_symbol=... success_permille=... queue_drops=... fifo_overruns=...
SX127x T1 A/B mode=FIFO_SPI t1_verified=... t1_bad_symbol=... success_permille=... queue_drops=... fifo_overruns=...
```

`success_permille = 1000 * t1_verified / (t1_verified + t1_bad_symbol)`.
It is a **candidate acceptance ratio**, not packet delivery ratio;
bad CRC and length are separate counters. Also compare `bad_crc`,
`bad_length`, `frame_timeouts`, `fifo_overruns`, `queue_drops`,
`rx_rearms`, and whether configured meters' messages are actually
handled.

## How to interpret

- FIFO markedly better with identical RF settings: investigate GPIO
  DCLK sampling, RAW synchronization, and ISR timing.
- Both similarly bad: FSK demodulation, actual RF environment, radio
  calibration and physical antenna path remain suspects.
- FIFO misses frames or times out: inspect FIFO timing / polling logic
  before claiming a receiver win.
- The result does not isolate the RF demodulator perfectly: FIFO mode
  uses hardware preamble/sync, whereas RAW mode uses software sync. That
  difference is part of the RX architecture being compared.

## Rollback

Revert to `experimental/sx127x-raw-dclk-capture-diag` and your
previous `native_stream_mode: T1_C1`, `packet_mode: false`,
`wmbus_raw_dclk: true`, and `sync_value: [0x54]`. That restores
the dual-mode RAW receiver. Do not overwrite the production YAML.
