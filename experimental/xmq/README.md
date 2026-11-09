# Native XMQ wM-Bus on official ESPHome SX126x

**Current branch:** `experimental/xmq-multidriver-auto` (native-only,
November 2026). This branch no longer packages the legacy
`wmbus_common`, `wmbus_meter`, `wmbus_radio` or
`socket_transmitter` components.

## Active sources

- `components/sx126x/`: patched official ESPHome SX126x T1/C1 streaming.
- `components/wmbus_native_sx126x/`: bounded native frame receiver,
  CRC/integrity gating, configured meter binding, strict XMQ AUTO and
  explicit decoding, application security and Home Assistant sensor output.
- `experimental/xmq/drivers/`: 52 upstream-anchored XMQ definitions.
- `experimental/xmq/tools/native_driver_specs.py`: explicit native
  decoder manifest. `generate_ixml_cpp.py` and
  `build_native_generated.py` produce the compile-time descriptors.
- `experimental/xmq/portable_cpp/`: portable XMQ records/math/date
  implementation and **offline** comparison catalog/parity helpers.
- `experimental/xmq/legacy_reference/`: 15 isolated, header-only
  historical `wmbus_pure` reference implementations for host tests.
  **Not part of the deployed SX126x native firmware.**
- `experimental/semtech-radio/`: native RF/host fixture tests,
  build-only Heltec YAML and CI guards.
- `experimental/xmq/espidf_cpp_probe/`: native ESP-IDF link/portability
  validation. It does not register the old wM-Bus radio component.

## Build/test contract

Use `.github/workflows/native-sx126x-adapter.yml` as the authoritative
continuous-integration workflow. The Heltec lab YAMLs import only
`sx126x` and `wmbus_native_sx126x`, never the old radio. CI also
validates fixture parity, test-only portable legacy references, the
security catalog and ESP-IDF/ESPHome builds.

Native receiver publication requires a configured meter identity,
validated packet/frame integrity and exact supported driver/security
profile. `driver: auto` is constrained by the same identity and
security rules as explicit drivers. `NOKEY` in an upstream fixture does
**not** prove an RF telegram is unencrypted.

These checks do not establish live RF parity, so do not flash experimental
lab firmware without separate radio/board verification. Real meter IDs,
cryptographic keys and HA API credentials must remain in private YAML
secrets rather than committed public configuration.

## Historical notes

Older XMQ shadow, `handleTelegram` and custom transceiver documentation
refer to retired branches, not to this native-only branch. Git history and
`docs/history/` preserve migration context. See top-level `README.md`
and `PROJECT_CONTEXT.md` for the driver batch chronology.
