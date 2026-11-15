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

## Experimental on-device recovery diagnostic (opt-in, NO DISPATCH)

The same branch also exposes a separate switch under the existing `sx127x:`
section. **The switch defaults to false.** Enable it only on the LILYGO
SX1276 RAW DATA/DCLK receiver while measuring RF performance:

```yaml
sx127x:
  wmbus_raw_diag_capture: true
  wmbus_t1_recovery_diag: true
```

Do not remove your other SX127x pins and FSK settings. The diagnostic
requires `wmbus_raw_dclk: true` and is limited to T1 frames rejected by
the existing 3-of-6 decoder. In particular:

- `BAD_SYMBOL` frames are copied with nonblocking queue submission from
  the ESPHome main loop. They never enter the normal meter callback again.
- The recovery algorithm runs in a separate, **FreeRTOS priority 1** task
  and never touches DIO GPIO, SPI, the high-priority radio task, or the ISR.
- Worker queue depth: 2; raw cap: 128 bytes; max invalid symbols: 5;
  max candidate combinations: 1024; wall-time budget: 25 ms per job.
  It yields every 16 candidate attempts.
- Each candidate is decoded and accepted *for statistics only* when every
  Format A CRC-16/EN-13757 block matches, including all data and CRC bytes.
- No reconstructed bytes, addresses, or AES keys are logged, sent to
  `wmbus_meter`, Home Assistant, MQTT, or any other receiver.
- Queue-full events and CPU budget cancellations are counted, not retried.
  Normal T1/C1 packet handling is unchanged.

The logs contain a per-frame diagnostic summary:

```text
SX127x T1 recovery diagnostic result=<status> invalid=<N> combinations=<N> tried=<N> crc_valid=<N> elapsed_ms=<N> (NO DISPATCH)
```

Status enum in `components/sx127x/wmbus_t1_recovery_diag.h`:
`0=CRC_VALID, 1=CRC_FAIL, 2=INCOMPLETE, 3=BAD_PREFIX,
4=TOO_MANY_ERRORS, 5=NO_ONE_BIT_CHOICE, 6=COMBINATION_LIMIT,
7=TIME_BUDGET`. A valid CRC result is diagnostic evidence, **not** a
request to publish the frame.

The 60-second `SX127x T1 recovery diagnostic` line reports total
submitted, dropped, CRC-valid, CRC-failed, skipped, and time-budget cases.

Run the no-radio, standalone C++ regression from the repository root:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Icomponents/sx127x \
    experimental/sx127x/test_t1_recovery_diag.cpp -o /tmp/t1_diag_test
/tmp/t1_diag_test
```

To revert the experiment without changing branches, set
`wmbus_t1_recovery_diag: false` and reflash.

## Validation of recovered headers against configured meters

The recovery diagnostic now performs two additional checks, **still without
forwarding recovered data**:

1. A unique CRC-valid candidate must also have a coherent T1 Format A link
   header: `C=0x44` (SND_NR), valid manufacturer code (three letters A-Z),
   and an 8-digit packed BCD meter ID in the primary address.
2. The recovered ID is compared against exact configured meter subscriptions
   already registered through `wmbus_radio` / `wmbus_meter`.
   Wildcards and range subscriptions are intentionally not treated as exact
   matches. The diagnostic stores up to 32 exact IDs in atomic slots and
   accepts late registration; no YAML list of IDs is needed.

Each per-candidate summary now includes `header=coherent` or
`header=ambiguous_or_bad_header` plus `configured=yes/no`. These labels
refer strictly to *diagnostic classification*, not automatic acceptance.
The 60-second counters include `coherent`, `configured`,
`unconfigured`, `ambiguous_or_bad`, and `registered_exact`.

The diagnostic does not reveal meter IDs or frame contents in these summaries.
The existing RAW hex capture mode, if enabled, **does** show frame material;
continue treating exported logs as potentially sensitive.

A CRC-valid candidate is never dispatched; AES decryption, application-layer
driver recognition and meter link-mode policy are untouched. A configured ID
match alone is not proof that a recovered telegram can be safely published.
