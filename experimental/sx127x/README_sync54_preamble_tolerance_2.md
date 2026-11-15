# SX1276 FIFO T1+C1/A: single-variable preamble tolerance probe

**Branch:** `experimental/sx127x-sync54-preamble-tol2`
**Base:** `experimental/sx127x-sync54-marker-hist`
**Status:** experimental, not compiled or tested over RF yet.

## Observation and hypothesis

In log (70) with 1-byte sync `54` and 16-bit preamble detection,
tolerance=10, there were 691 RX starts, 375 markers `3D`,
315 other markers and **only one** marker `CD`, with CRC fail;
T1_verified=190, C1A_verified=0, queue_drop/overrun/timeout=0.
In log (68) with full `54 CD` sync, 67 C1/A frames passed CRC.

At setup `RegSyncConfig[0x27]=0xB0` selects an enabled, one-byte
sync with 0x55 polarity; `RegPreambleDetect[0x1F]=0xAA` selects
16-bit preamble detection with 10 tolerated errors. This tolerance
allows a high fraction of corrupted/incorrect preambles and may
contribute to early or false sync events. This is only a hypothesis.
Reducing tolerance to **2** keeps detector length, polarity and
hardware sync unchanged. Expected `RegPreambleDetect[0x1F]=0xA2`.

## YAML changes relative to log (70)

```yaml
external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/sx127x-sync54-preamble-tol2
    components: [sx127x, wmbus_common, wmbus_radio, wmbus_meter]
    refresh: always
```

In existing `sx127x:`, change *only*:
```yaml
  preamble_errors: 2
```

Keep:
```yaml
  preamble_detect: 2
  preamble_polarity: 0x55
  packet_mode: true
  bandwidth: 125_0kHz
  bitrate: 100000
  deviation: 50000
  afc_auto: false
  agc_auto: true
  bitsync: true
  wmbus_continuous_dual: true
  wmbus_dual_sync: false
  wmbus_c1a_only: false
  sync_value: [0x54]
  wmbus_fifo_dual_probe: true
  wmbus_fifo_t1_stable: false
```
In existing `wmbus_radio:` keep `native_stream_mode: T1_C1`
and `native_phy_diagnostics: true`. Preserve all board GPIOs, SPI,
all 12 meter definitions, XMQ and no HA sensors.

The `wmbus_fifo_dual_probe: true` schema guard now requires
`preamble_errors: 2` on **this branch only**. There is no change to
3-of-6, CRC, FIFO read/rearm, driver code or emission of unknown meters.

## Collect and compare

Capture 8–10 minutes without moving the radio or antenna. Compare
against log (70) with same runtime window. Look for:
- `RegPreambleDetect[1F]=0xA2` and FSK profile matches expected TRUE.
- `marker_CD` and **C1A_ok (CRC verified)**; a lone `CD` is not success.
- `marker_other`, `marker_3D`, verified T1 per minute and
  `overrun/timeout/queue_drop`.
- Beware denominator: marker counts/candidate success are **not**
  RF delivery rates. Reducing false syncs may also reject weak
  legitimate T1 or C1/A preambles.

**Rollback:** use branch
`experimental/sx127x-sync54-marker-hist` and restore
`preamble_errors: 10`; or restore known-good T1-only branch
`experimental/sx127x-fifo-t1-stable` with sync `[0x54,0x3D]`
and stable guard enabled.

No extra GitHub Actions workflows created.
