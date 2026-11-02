# Migration gates: official ESPHome SX126x only

**Target:** `sx126x` from ESPHome owns the hardware; wM-Bus framing, DLL
validation, configured-meter routing and XMQ remain application components.
No simultaneous owners for SX1262. SX127x is out of scope.

## Verified by CI
- Standalone SX126x receiver / listener can compile with ESPHome 2026.10.1.
- Host-side T1 symbol tests, C1 frame-length tests and pure DLL integration tests.
- Experimental native fixture must not instantiate `wmbus_radio` legacy SPI transceiver.

## Not yet verified
- Native radio captures actual T1 and C1 on Heltec with correct sync/bitrate,
  GPIO wiring, BUSY/TCXO/RF switch.
- Direct `SX126xListener` to the existing wM-Bus dispatch/XMQ handler chain.
- Real telemetry parity for the seven heat and five water meters.
- Production-safe handling of discovery, unverified telegrams, queue overflow
  and resuming RX after malformed packets.
- Radio owner selection removed from `wmbus_radio` Python schema; previous
  `radio_type: SX1262` will no longer be accepted in the final branch.

## Removal only AFTER integration tests and hardware parity
- Delete `transceiver_sx1262.h/.cpp`, `transceiver.h/.cpp` and old radio
  hardware setup/IRQ/task/SPI code, conditional on no other consumers.
- Keep packet parsing, T1/C1, pure validators, meter registry, XMQ generation,
  sensor output and discovery functions needed by native path.
- Update all CI fixtures and documentation to the single official owner.
- Run full firmware build, all unit tests, radio regression and a real
  receiving test on Heltec **before** considering the branch migrated.

The main / working deployed Heltec firmware is not changed by branch commits.
