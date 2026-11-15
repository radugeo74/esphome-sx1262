# SX1276 FIFO / SPI T1-only — stage 1 stable baseline

Branch: `experimental/sx127x-fifo-t1-stable`
Parent: `experimental/sx127x-fifo-vs-raw-t1`

## Measured field baseline (LILYGO T3-S3 SX1276)

Logs 64/65 on 2026-11-15, same antenna and device, successive runs:

| Mode | Report checkpoint | Verified T1 | T1 rejected 3-of-6 | Candidate acceptance |
| --- | --- | ---: | ---: | ---: |
| FIFO 125 kHz | 18:10:18, approx 3 minutes | 87 | 36 | 70.7% |
| FIFO 125 kHz | 18:12:18, approx 5 minutes | 144 | 61 | 70.2% |
| FIFO 250 kHz | 18:20:02, approx 3 minutes | 56 | 32 | 63.6% |

Cumulative counter values, not true air-interface packet-delivery rates.
Runs were not concurrent, so this is a pragmatic rather than controlled RF
isolation. At 125 kHz: zero FIFO overruns, timeouts, queue drops. Some
sync/prefix rejects are expected and must NOT be equated to meter packet
losses. No firmware Hamming correction or recovery dispatch is used.

## T1-only profile

Retain your current `external_components` list, existing SX127x ID,
SPI, DIO0/DIO1/DIO2 pin mappings, Wi-Fi/API, `wmbus_common` drivers,
all 12 `wmbus_meter` entries and existing sensors. Change only
the GitHub component branch and the relevant keys of the existing
`sx127x:` and `wmbus_radio:` blocks. The following is a **block
fragment**, not a second `sx127x:` instance:

```yaml
# external_components URL must point to:
# github://radugeo74/esphome-sx1262@experimental/sx127x-fifo-t1-stable
sx127x:
  # Retain existing id, pin assignments, existing SPI and unrelated settings
  frequency: 868.95MHz
  modulation: FSK
  bandwidth: 125_0kHz
  bitrate: 100000
  deviation: 50kHz
  afc_auto: false
  agc_auto: true
  bitsync: true
  packet_mode: true
  crc_enable: false
  preamble_polarity: 0x55
  preamble_size: 4
  preamble_detect: 2
  preamble_errors: 10
  payload_length: 0
  rx_start: true
  wmbus_t1_mode: true
  wmbus_raw_dclk: false
  sync_value: [0x54, 0x3D]
  wmbus_c1a_only: false
  wmbus_dual_sync: false
  wmbus_continuous_dual: false
  wmbus_raw_diag_capture: false
  wmbus_t1_recovery_diag: false
  wmbus_fifo_t1_stable: true
```

NOTE: When applying the fragment, do NOT replace your whole existing
`sx127x:` block: you must retain `id`, `cs_pin`, `rst_pin`,
`dio0_pin`, `dio1_pin` and other hardware keys. The example above
deliberately omits these existing values.

In the existing `wmbus_radio:` block:

```yaml
  native_stream_mode: T1
  native_phy_diagnostics: true
```

The `wmbus_fifo_t1_stable: true` **opt-in schema guard** rejects
accidental changes to the field-tested T1-only profile. It does not
alter radio register writes, FIFO timing, CRC handling, existing
`wmbus_common` drivers, meter IDs, publication, or runtime path.
Disable the guard only for controlled future experiments.

## Expected checks

1. ESPHome config validation succeeds (avoid relying on stale validation
   cache; compile against this branch). The guard fails when any locked
   property differs, including `bandwidth: 250_0kHz` or AFC auto.
2. Boot shows `Bandwidth: 125000 Hz`, `Packet Mode: TRUE`,
   `Sync Value: 0x543d`, `FSK profile matches expected: TRUE`.
3. `SX127x T1 comparison RX path: FIFO_SPI` and
   `SX127x T1 A/B mode=FIFO_SPI` appear with diagnostics.
4. Run at least 10 minutes to obtain multiple 60-second statistics.
   Verify `overrun=0`, `frame_timeout=0`, `queue_drop=0`,
   no repeated reboots, and valid T1 frames handled by configured meters.
5. Compare candidate T1 acceptance and verified T1/min against the
   measured baseline, remembering RF environment fluctuates.

**Scope:** stage 1 only, T1 only. C1/A will not be received in this
profile; do not treat that as a regression in a later dual-mode test.
No changes to the 3-of-6 algorithm, Format-A CRC or XMQ decoder.
No additional CI workflow is created.

**Rollback:** point `external_components` back to
`experimental/sx127x-fifo-vs-raw-t1` (same tested FIFO receiver),
and remove `wmbus_fifo_t1_stable`. Restore the previous branch/config
if you need T1+C1 RAW on this device.
