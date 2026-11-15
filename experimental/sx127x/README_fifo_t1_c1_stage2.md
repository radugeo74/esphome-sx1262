# Etapa 2 — LILYGO T3-S3 SX1276 FIFO T1+C1/A probe (no timeslicing)

Branch: `experimental/sx127x-fifo-t1-c1-stage2`.
Base: `experimental/sx127x-fifo-t1-stable`.
**STATUS: EXPERIMENTAL / NOT HARDWARE-VALIDATED.**
Do not replace or overwrite the tested T1-only firmware backup.

## Hardware feasibility / limits

SX1276 has **one** FSK sync pattern comparator. It cannot independently
match both 2-byte patterns `54 3D` (T1) and `54 CD` (C1/A) at once.
The existing continuous FIFO implementation uses **one-byte HW sync
`54`** and reads the next byte from FIFO to distinguish `3D` and
`CD`. It reuses native T1 / C1 parsing and mandatory CRC validation.
It does **not** periodically switch radio sync profiles.

Previously the shared `54` configuration on this board was observed
to lose many C1/A frames. This experiment is for quantifying that loss
and determining whether T1 remains usable; it is NOT a claimed fix
for the underlying hardware preamble/sync limitation.

## YAML change relative to Stage 1

Retain hardware pins, SPI, 868.95 MHz, FSK 100000 bit/s, deviation 50 kHz,
bandwidth `125_0kHz`, AFC OFF, AGC ON, BitSync ON, preamble values,
all 12 configured meters, XMQ drivers, Wi-Fi/API, and no HA sensors.

Use branch:
```yaml
external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/sx127x-fifo-t1-c1-stage2
    components: [sx127x, wmbus_common, wmbus_radio, wmbus_meter]
    refresh: always
```

Change **only** these keys within your existing `sx127x:` mapping:
```yaml
  packet_mode: true
  wmbus_raw_dclk: false
  bandwidth: 125_0kHz
  afc_auto: false
  agc_auto: true
  wmbus_t1_mode: true
  wmbus_dual_sync: false         # no timed 54 3D / 54 CD switching
  wmbus_continuous_dual: true   # common 54 hardware sync
  wmbus_c1a_only: false
  sync_value: [0x54]
  wmbus_fifo_t1_stable: false   # mutual exclusion with stage 1 guard
  wmbus_fifo_dual_probe: true   # strict stage 2 config check
```

In existing `wmbus_radio:`:
```yaml
  native_stream_mode: T1_C1
  native_phy_diagnostics: true
```

All configured `wmbus_meter:` entries can keep `mode: [T1]` for
an initial RF comparison; *this alone will not dispatch C1 telegrams
to T1-only meter subscriptions*. If you need *the same configured
meters* to accept C1/A, configure `mode: [T1, C1]` only after verifying
the supported meter syntax and its upstream availability. Unconfigured
C1/A candidates can still be decoded and logged as unhandled
once valid; no synthetic sensor entities are created.

## Expected boot readback

- `Bandwidth: 125000 Hz`
- `Packet Mode: TRUE`
- `Sync Value: 0x54`
- `Continuous T1/C1/A common 54 sync (preamble gate): TRUE`
- `RX path: FIFO_SPI (T1+C1 experimental)`
- `FSK profile matches expected: TRUE`

Do NOT expect the T1-only A/B `success_permille` in dual mode; its
denominator is not meaningful once marker-3D can yield two candidates.

## Run and compare

1. Capture a 10-minute log of this branch without moving antenna or board.
2. If possible, run the same 10-minute interval with stage-1 stable branch,
   then another stage-2 interval (B-A-B test).
3. Watch the cumulative line `SX127x FIFO dual probe hw_sync=54`.
4. Measure `T1_ok`, `C1A_ok`, `C1B_ok`,
   `marker_3D`, `marker_CD`, `prefix_reject`,
   `bad_symbol`, `bad_crc`, `overrun`, `timeout`,
   `queue_drop`, `rearm` and configured meter handling.
5. **A C1A marker alone is NOT C1A reception**: require
   `C1A_ok > 0` (CRC-valid), then check meter matches.
6. Note presence of suspicious C1 CRC failures and whether T1/min
   is degraded from the stage-1 baseline of ~28 valid T1/min.
7. If no C1/A verified, or T1 degrades materially, immediately
   roll back. Do not weaken CRC validation or dispatch incomplete frames.

## Rollback

Switch branch back to
`experimental/sx127x-fifo-t1-stable` and
```yaml
  wmbus_continuous_dual: false
  sync_value: [0x54, 0x3D]
  wmbus_fifo_dual_probe: false
  wmbus_fifo_t1_stable: true
```
Then restore `wmbus_radio.native_stream_mode: T1`.

## Scope

Reuses the **existing** FIFO SPI path, marker handling, T1 3-of-6
decoder, C1 parser, CRC checks and wmbus_common engine. Adds opt-in
guard and honest split diagnostics. No new GitHub Actions jobs, no new
drivers and no new HA entities. **Compilation and hardware logs remain
to be verified before calling Stage 2 complete.**
