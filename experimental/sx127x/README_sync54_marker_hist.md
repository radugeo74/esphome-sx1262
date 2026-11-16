> **Actualizare 16.11.2026 — branch comun:** această implementare SX1276 este inclusă, fără modificări radio, în `experimental/dual-radio-sx126x-sx127x-sync54`, alături de componenta `sx126x` de pe Heltec. Pentru configurațiile noi folosiți `github://radugeo74/esphome-sx1262@experimental/dual-radio-sx126x-sx127x-sync54`. Logurile LILYGO din 16 noiembrie confirmă recepție FIFO cu CRC valid T1/A și C1/A; histograma marker rămâne diagnostic, nu dovadă de livrare a fiecărui cadru. Mențiunea „not compiled/hardware-validated yet” de mai jos este istoricul momentului când a fost scris experimentul. Noul ref comun nu a fost compilat separat. Motorul XMQ dinamic nu este încă implementat.

# SX1276 FIFO common 0x54 synchronization — marker histogram diagnosis

Branch: `experimental/sx127x-sync54-marker-hist`.
Based on `experimental/sx127x-fifo-t1-c1-stage2`. Firmware is
**not compiled/hardware-validated yet**. Do not replace the backup of
`experimental/sx127x-fifo-t1-stable` (known-good T1).

## Why

Log (67), common sync `54`: T1 verified=116 (at ~4min), C1/A verified=0,
`marker_3D=208`, `marker_CD=0`, `marker_other=176`, no RX/FIFO errors.

Log (68), full C1/A sync `54 CD`: C1/A verified=67 (at ~5min),
`header_ok=223`, `crc_bad=156`, overrun=timeout=queue_drop=0.

**Inference**: existing FIFO + downstream C1 CRC path work with full
sync; common 0x54 case fails to expose expected C1/A marker. It does not
yet prove whether the cause is preamble detection, bit alignment, false
single-byte syncs or another RF constraint.

## What changes

Only diagnostic counters and a once/minute log in the existing main
loop. An atomic 256-entry histogram tracks each byte actually read
after one-byte hardware sync `54`; the top 8 are reported. No payloads
or device IDs in the new diagnostic line. No per-frame logs in the
high-priority FIFO reader. CRC, parser, FIFO timing, RF registers,
meter list and XMQ unchanged.

## YAML for the T1+C1/A diagnostic

Keep your existing 125 kHz, FSK 100 kbps, 50 kHz deviation, AFC OFF,
AGC ON, pins, SPI, drivers, and 12 configured meters without sensors.

```yaml
external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/sx127x-sync54-marker-hist
    components: [sx127x, wmbus_common, wmbus_radio, wmbus_meter]
    refresh: always

sx127x:
  # Fragment only: keep original pins, RF settings, SPI and board wiring.
  packet_mode: true
  wmbus_t1_mode: true
  wmbus_raw_dclk: false
  bandwidth: 125_0kHz
  afc_auto: false
  agc_auto: true
  wmbus_continuous_dual: true
  wmbus_dual_sync: false
  wmbus_c1a_only: false
  sync_value: [0x54]
  wmbus_fifo_t1_stable: false
  wmbus_fifo_dual_probe: true

wmbus_radio:
  # Fragment only: keep original id and sx127x_id.
  native_stream_mode: T1_C1
  native_phy_diagnostics: true
```

Do NOT copy this YAML as a complete file: it omits required hardware
parameters and mappings from the already working configuration.

## Collect about five minutes

Expect new lines at the ~60-second diagnostics interval:

```text
SX127x SYNC54 marker histogram observed=...
  markers 3D=... XX=... YY=... ...
SX127x FIFO DUAL_PROBE hw_sync=54 T1_ok=... C1A_ok=... ...
```

Interpret the top-8 markers alongside the existing
`marker_3D/marker_CD/other` counts. Since the histogram is cumulative
and takes snapshots from concurrent atomic counters, occasional
one-event discrepancies are normal. Marker values are **not** validated
packets. A C1/A fix must also yield `C1A_ok > 0` with CRC checks
unchanged. Do not infer a unique reason for failure solely from marker
frequency.

For reference, the same branch may also be run with the **previously
tested** `wmbus_c1a_only: true`, `sync_value: [0x54, 0xCD]` and both
profile guard flags false; in that mode the `SYNC54 marker histogram`
will not be printed (marker CD has been consumed by hardware sync).

Rollback to `experimental/sx127x-fifo-t1-stable`, full sync
`[0x54, 0x3D]`, `wmbus_continuous_dual: false`,
`wmbus_fifo_t1_stable: true`,
`wmbus_fifo_dual_probe: false`, and
`wmbus_radio.native_stream_mode: T1`.
No extra GitHub Actions workflow added.
