# SX127x RAW/DCLK T1 — offline CRC recovery experiment

This is **diagnostic-only**. The receiver still rejects invalid T1 3-of-6
symbols and applies its original CRC checks before meter dispatch.

## Firmware

Use the existing SX127x experimental branch and opt in:

```yaml
external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/sx127x-raw-dclk-capture-diag
    components: [sx127x, wmbus_common, wmbus_radio, wmbus_meter]

sx127x:
  bandwidth: 125_0kHz
  afc_auto: false
  agc_auto: true
  wmbus_raw_diag_capture: true
```

Keep the remaining board pins, radio options, and meter configuration as-is.
The six earliest rejected T1 candidates are stored in RAM, each up to 128 raw
bytes. No sensors are created. They are replayed by ESPHome `dump_config()`
when the API logger reconnects, or shown as brief summaries while receiving.

Look for `SX127x T1 RAW stored ... raw128=<HEX>`. Existing nested-sync
counters and post-error phase diagnostics remain active.

## Offline analysis (Python 3.10+)

Save a complete ESPHome API log after reconnecting (without rebooting the
device), then run from this directory:

```bash
python3 raw_t1_recover_offline.py --log lilygo.log
python3 raw_t1_recover_offline.py --raw 2CE71C6AC656...
python3 -m unittest -v test_raw_t1_recover_offline.py
```

The tool:
1. Decodes the expected T1 Format A length from its 3-of-6 L field.
2. Requires the *whole* encoded frame to be present in the raw capture.
3. Identifies invalid 3-of-6 symbols and tries replacements at Hamming
   distance exactly one. Default: at most two invalid symbols, maximum 128
   candidate combinations (both configurable via CLI).
4. Reconstructs each candidate's Format A CRC blocks (first 10 data bytes
   including L, then successive blocks of up to 16 bytes) and verifies
   CRC-16/EN-13757 (poly 0x3D65, init 0, xorout FFFF, CRC bytes MSB first).
5. Reports CRC-valid reconstructed telegrams; none are transmitted, decoded
   into HA sensors, or delivered to real meter handlers.

Possible statuses: `crc_valid`, `crc_fail`, `incomplete_capture`,
`bad_length_prefix`, `too_many_invalid_symbols`,
`no_single_bit_candidate`, `search_limit`.

**Important limitations**

- The method only changes *invalid* 3-of-6 symbols. A corrupted bit that
  turns one legal 3-of-6 code into another legal code is not corrected.
- A wrong initial L field, bit insertion/deletion, many errors, or a truncated
  capture is not silently accepted.
- CRC-valid offline recovery does **not** establish why the bit was corrupted
  (RF demodulation vs GPIO sampling). It must not be used to enable
  unverified automatic correction.
- RAW captures may include real meter identifiers and should not be committed
  to a public repository.

## Comparison

Compare the valid T1/C1 counts against Heltec only over the same local
timestamp interval. The offline tool classifies candidate recovery; it does
not estimate RF performance or modify live reception.
