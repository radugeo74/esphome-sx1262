# Native SX126x → legacy wmbusmeters bridge

**Status:** integration under software test. **Not approved for RF deployment.**

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
sync-word stream. It recognizes T1 format A and C1 format B; **C1 format A
is not yet supported** by this path. The combined option has not been RF
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

Before removing the old
`components/wmbus_radio/transceiver_sx1262.cpp/.h`, require:
- ESPHome 2026.10.1 full **ESP-IDF firmware build** SUCCESS with the
  new bridge, plus regression YAML using the old schema.
- Host/frame tests for valid and corrupt T1, C1-B, CRC, invalid symbols,
  unknown ID, wrong AES key, and power-loss/recovery.
- Actual Heltec measurements of T1 and applicable C1 formats, RSSI,
  receive stability and all six household meters.
- Confirm the original YAML or firmware can be restored.

**Until these tests and the owner's RF validation pass, keep the legacy
transceiver strictly for rollback.** Deleting it after a compile-only pass
would be premature and could leave a working installation unrecoverable.

For the separate goal of updating XMQ rules without firmware recompilation,
see `docs/XMQ_RUNTIME_ARCHITECTURE.md`. This bridge is one prerequisite,
not the runtime XMQ engine itself.
