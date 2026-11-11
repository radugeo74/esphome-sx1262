# Native-only SX126x ESP-IDF regression tests

Current branch: `experimental/xmq-runtime-engine`. The **native SX126x receiver** is the only radio implementation. The existing `wmbus_common` application decoder and `wmbus_meter` Home Assistant sensor interface are retained.

CI (`.github/workflows/native-espidf-sx126x-smoke.yml`) runs on push and compiles with pinned ESPHome 2026.10.1:

- `tests/sx126x-native-espidf.yaml`: isolated patched SX126x receiver, build-only.
- `tests/sx126x-wmbus-bridge.yaml`: native receiver → `wmbus_radio` → `wmbus_meter` with a synthetic water meter, build-only.
- `tests/build_full_meter_fixture.py`: generates `tests/sx126x-wmbus-12-meters-ci.yaml` with **7 synthetic heat meters, 5 synthetic water meters and 50 wM-Bus sensors**; this fixture is compiled in CI, not flashed.
- `tests/test_decode3of6_bridge.cpp`: checks T1 symbol decoding under sanitizers.
- `tests/sx1262-legacy-rollback.yaml`: intentionally invalid **negative schema regression**; CI must reject the removed `radio_type: SX1262` configuration. It is **not** a usable firmware rollback YAML.

The legacy radio source files were removed from this branch. Recovery requires an older Git commit and a saved working firmware/YAML; there is no in-branch switch to the old receiver.

Hardware evidence (11 November 2026, pre-cleanup): 12/12 configured meters published real values with T1/A. CI validates builds only. The native-only source cleanup still requires a controlled on-device regression before it can be called hardware-validated. C1-B remains experimental. Dynamic XMQ rule hot-reload is not implemented.

**Do not flash the synthetic fixtures.** Keep real meter IDs, Wi-Fi credentials and keys in the private HA/ESPHome configuration.
