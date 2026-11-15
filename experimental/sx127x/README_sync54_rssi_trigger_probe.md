# Experimental SX1276 FIFO T1+C1/A — RSSI-trigger probe

Branch: `experimental/sx127x-sync54-rssi-trigger-probe`.
Base: `experimental/sx127x-sync54-marker-hist`.
**UNVALIDATED: run a config check/compile before flashing.** The T1-only
stable firmware and all earlier experimental branches are untouched.

## Motivation

Log (70), common one-byte sync 0x54 with 16-bit preamble detection
(10 tolerated errors): 190 T1 verified at 7 min, C1/A zero;
at least one CD candidate later but CRC rejected.
Log (71), same common sync, preamble tolerance reduced to two:
346 T1 verified at 13 min and only **one CRC-verified C1/A**.
Log (68), full C1/A hardware sync 54 CD, verified 67 C1/A at ~5 min.

Tightening preamble tolerance does not resolve C1 reception.
The next *single-variable hardware experiment* disables the preamble
detector, changing `RegRxConfig` from `TRIGGER_PREAMBLE` to
`TRIGGER_RSSI`. The one-byte `54` hardware sync comparator
remains active; CRC remains mandatory.
This specifically tests whether the hardware preamble gate blocks C1/A.
It does **not** fix an incorrectly aligned sync marker and does not
guarantee packet capture.

## YAML delta relative to working log (70)

```yaml
external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/sx127x-sync54-rssi-trigger-probe
    components: [sx127x, wmbus_common, wmbus_radio, wmbus_meter]
    refresh: always
```

Under existing `sx127x:` set:
```yaml
  preamble_detect: 0   # preamble detector disabled, RSSI RX trigger
  preamble_errors: 10  # restore value from log (70); irrelevant while disabled
  wmbus_continuous_dual: true
  wmbus_dual_sync: false
  wmbus_c1a_only: false
  sync_value: [0x54]
  wmbus_fifo_t1_stable: false
  wmbus_fifo_dual_probe: true
```

Do not change SPI pins, antenna, frequency 868950000, bandwidth
125_0kHz, bitrate 100000, deviation 50000, AFC off, AGC on,
BitSync on, packet mode, radio T1_C1, XMQ, 12 meters or sensors.

The branch allows preamble detector OFF **only** for the strictly
guarded `wmbus_fifo_dual_probe: true` profile. Other dual-mode
configurations still require the preamble gate.

## Boot expectations

- `RegPreambleDetect[1F]=0x00` (detector OFF).
- `RegRxConfig[0D]=0x0D` (AGC on, AFC off, RSSI-trigger selection).
- `RegSyncConfig[27]=0xB0` (one-byte sync 54, unchanged).
- `Continuous T1/C1/A common 54 sync: TRUE; RX trigger: RSSI (experimental)`.
- `FSK profile matches expected: TRUE`.

## Risk / acceptance / rollback

RSSI-triggered acquisition may produce more false wakes/noisy marker
candidates. This could **hurt T1** or fill logs; DO NOT leave it running
unattended. Run a short supervised **3–5 minute** capture first;
stop if RX fails, FIFO overflows, queue drops, watchdog stalls
or T1 reception collapses. Compare equal-duration intervals
against logs (70) and (71), preferably same antenna and location.

Look at CRC-verified `C1A_ok` rather than `marker_CD` alone.
A few C1/A telegrams are not yet proof of parity with full-sync C1A-only.
Keep unknown meter telegrams log-only and no HA entities.

Rollback to `experimental/sx127x-sync54-marker-hist` with:
```yaml
  preamble_detect: 2
  preamble_errors: 10
```
Or use the proven stable T1-only branch
`experimental/sx127x-fifo-t1-stable`, full sync `[0x54, 0x3D]`,
continuous dual OFF and stable guard ON.

No new workflows are created.
