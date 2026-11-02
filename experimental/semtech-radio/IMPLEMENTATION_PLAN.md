# Native ESPHome SX126x integration (SX127x-ready architecture)

Updated 2026-11-01. Scope decision: **implement only native ESPHome sx126x now**. SX127x is a future extension, not an implementation or CI requirement for this phase.

Official component: https://esphome.io/components/sx126x/
Future reference only: https://esphome.io/components/sx127x/

Base: experimental/xmq-independent-pipeline at 53c9ac81a63b59439780670f4c125fccdaaadbd3.

## Guardrails
- Do not alter production Heltec YAML, credentials, firmware or the established XMQ code path.
- Retain the current custom SX1262 backend intact for rollback.
- Do not run the native sx126x and custom SX1262 driver on the same physical radio.
- No automatically created meter sensors; unknown and unverified frames remain separate from validated readings.
- Keep radio PHY data acquisition separate from T1/C1 processing and XMQ decoding.

## Scope for this phase: native SX126x
1. Confirm the actual APIs and FSK receive event behavior in ESPHome 2026.10.1, including bytes, length, metadata and IRQ recovery.
2. Verify tested Heltec board wiring: SPI, busy, reset, DIO1, TCXO/RF switch; avoid assuming that current custom-radio YAML has every pin available.
3. Add a separate opt-in SX126x -> wM-Bus packet adapter. Preserve the existing custom SX1262 backend and configuration.
4. Feed radio bytes into reusable wM-Bus T1 3-of-6 / C1 frame conversion, CRC, header parsing, discovery/unverified and XMQ dispatch. A common adapter input contract should carry bytes, length, mode, RSSI and receive status so SX127x can be added later without touching XMQ.
5. Host tests with recorded real T1 and C1 packets, including corrupted/short inputs. Add ESPHome compile/link CI for the native sx126x opt-in configuration.
6. Validate real reception and Home Assistant sensor parity against current baseline before any production switch or removal of the legacy radio backend.

## Future SX127x extension (design only)
- Keep adapter independent of native ESPHome radio class names and radio-specific IRQ/SPI internals.
- No SX127x device implementation, test matrix, firmware compile or hardware support claim in this branch phase.
- Before future SX127x work, assess its 64-byte FSK packet-mode FIFO limitation against observed 65/84/89-byte raw captures; streaming may be necessary.
- Document any limitations in a separate future task.

## Definition of done for SX126x
- Native ESPHome SX126x receives real T1/C1 traffic from the current environment.
- Known-good configured meter values and identifiers remain unchanged; unconfigured/unverified traffic never updates measurements.
- No unintended duplicate callbacks, stale publication or restart loops.
- Heap/RSSI/radio recovery are measured and compared with current firmware.
- Rollback to original SX1262 backend remains available without changing private credentials.
