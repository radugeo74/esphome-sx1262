# Native SX126x → legacy wmbusmeters bridge

**Status (11 November 2026): T1/A hardware-validated on Heltec with 12/12 configured meters publishing; C1-B and sustained/negative tests remain unvalidated.** This is still an experimental bridge, not a claim of universal C1 or dynamic XMQ support.

This branch starts from `native-espidf`. Instead of replacing `wmbus_common`,
`wmbus_meter`, and all existing sensor definitions, the bridge feeds the
original `wmbus_radio::Frame` API from the patched `components/sx126x`
component. Meter filters, keys and decoded fields still use the original
wmbusmeters-derived C++ engine. No dynamic XMQ reload is implemented yet.

## One radio owner

Use the **new** schema:

```yaml
wmbus_radio:
  id: receiver
  sx126x_id: native_radio
  native_stream_mode: T1
```

Here `native_radio` must be the ID of an appropriately configured
`sx126x:` component, with SPI, BUSY, DIO1 and reset pins declared
**only there**. Do not add `radio_type`, `cs_pin`, `reset_pin` or
`irq_pin` under `wmbus_radio` in native mode. The schemas are mutually
exclusive. The legacy transceiver's FreeRTOS task and second SPI device are
not instantiated in this mode.

`native_stream_mode: T1_C1_B` opt-in uses the experimental combined
sync-word stream. It recognizes T1 format A and C1 format B **up to 128 raw bytes**.
The legacy wmbusmeters Format-B helper has an unsafe two-block CRC-length
calculation for longer inputs, so the new bridge rejects them until that
case is repaired and regression-tested. **C1 format A is not yet supported**
by this path. The combined option has not been RF
validated in this branch.

## Data path and security

1. SX126x detects a sync word and sends bounded raw FIFO fragments
   through `set_t1_stream_callback`.
2. The adapter assembles a maximum of **384 bytes**, aborting on
   overflow, null fragments, incomplete transfers and invalid framing.
3. T1 raw data is 3-of-6 decoded; C1-B retains the required `54 3D`
   mode prefix. Framing must match exactly.
4. The existing wmbusmeters `trimCRCsFrameFormatA/B` routines verify
   *all* DLL CRC blocks. Invalid CRCs are discarded before any meter
   receives an event.
5. Exactly framed CRC-stripped bytes are delivered as the same
   `wmbus_radio::Frame` that `wmbus_meter` already consumes.
   The existing meter's ID/key/link-mode validation still runs.
6. Semtech's **RSSI sync** (not premature RSSI average) is clamped to
   the legacy integer sensor API for compatibility. Fractional RSSI
   precision remains a future API change.

The adapter is synchronous in the SX126x main loop. The radio re-arms
RX before delivering its end-of-frame callback; no secondary receiver
task or SPI access takes place inside the adapter.

## Tests and removal gate

`tests/sx126x-wmbus-bridge.yaml` is a **build-only** synthetic fixture:
one SX126x, one wmbus_radio adapter, one configured mkradio4 meter and
the original numeric/RSSI HA sensors. The CI workflow compiles both
this integrated fixture and an isolated radio fixture.

Before permanently removing the old
`components/wmbus_radio/transceiver_sx1262.cpp/.h`, require:
- ESPHome 2026.10.1 full **ESP-IDF firmware build** SUCCESS with the
  new bridge, plus regression YAML using the old schema.
- Host/frame tests for valid and corrupt T1, C1-B, CRC, invalid symbols,
  unknown ID, wrong AES key, and power-loss/recovery.
- Actual Heltec measurements of T1 and applicable C1 formats, RSSI,
  receive stability and all configured household meters. **T1/A was observed for 12/12 configured meters on 11 November 2026; C1 and long-duration reception remain open.**
- Confirm the original YAML or firmware can be restored.

**Until the remaining tests and rollback gate pass, keep the legacy
transceiver source strictly for rollback.** The native `sx126x_id` configuration does **not instantiate** this legacy SPI/IRQ receiver, even though its source remains in the repository. The `wmbusmeters` log prefix refers to the application decoder that remains intentionally active for real HCA/water readings, **not** to the old radio driver. Deleting it after a compile-only pass
would be premature and could leave a working installation unrecoverable.

For the separate goal of updating XMQ rules without firmware recompilation,
see `docs/XMQ_RUNTIME_ARCHITECTURE.md`. This bridge is one prerequisite,
not the runtime XMQ engine itself.

Rollback schema validation: `tests/sx1262-legacy-rollback.yaml` exercises the original `radio_type: SX1262` path. It intentionally does NOT instantiate `sx126x:` and is never used concurrently with the new receiver.

## Migrating a full YAML from wmbus_native_sx126x

The old XMQ-only component is **not present** in this branch.
To preserve the original ESPHome entities, convert the YAML as follows:

- Keep existing `sx126x`, Heltec GPIOs, ESP-IDF, Wi-Fi, API, OTA, debug
  and the LED output. Import `sx126x`, `wmbus_common`,
  `wmbus_radio` and `wmbus_meter` from this branch.
- Replace `wmbus_native_sx126x` with exactly one
  `wmbus_radio: {id: ..., sx126x_id: ..., native_stream_mode: T1}`.
  `on_frame` receives a CRC-verified frame and can blink the LED.
  Unlike the old `on_verified`, this event is **not filtered** to
  configured meter IDs; the meter handlers still filter publications.
- Represent each configured meter with its own `wmbus_meter` instance,
  `radio_id`, a hexadecimal `meter_id: 0x...`, `type:` and
  `mode: [T1]`. `type: auto` is **not available** in this legacy
  wmbusmeters-based branch; specify `fhkvdataiii` for the heat
  allocators and `mkradio4` for the water meters.
- Move the old `xmq_sensors` and `rssi_sensors` into the top-level
  `sensor` list as `platform: wmbus_meter` sensors, with
  `parent_id` and `field`:
  `current_hca`, `previous_hca`, `temp_room_c`,
  `temp_radiator_c`, `target_m3`, `total_m3`, `rssi_dbm`.
  Keep existing sensor `name` and explicit `id` unchanged.
- `t1_raw_diagnostics`, `configured_meter_ids`,
  `meter_drivers`, `xmq_sensors`, and
  `rssi_sensors` are **not valid native-wmbus YAML options**.
  The XMQ hot-reload engine is still planned, not implemented.

A privacy-safe full-shape fixture with **7 synthetic heat + 5 synthetic
water meters and 50 synthetic meter sensors** is generated by
`python tests/build_full_meter_fixture.py` and compiled in CI.
The user's actual meter IDs, Wi-Fi passwords and network addresses
must **never** be added to the public repository.

An existing entity `name` or YAML `id` does not guarantee identical
Home Assistant registry `unique_id` when switching ESPHome platforms.
Before a live flash, back up HA entity mapping/history and the
current known-good firmware. Validate RF T1 data and numerical parity
before allowing any removal of the legacy transceiver.
