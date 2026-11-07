# XMQ — active context on `experimental/xmq-multidriver-auto`

> **Branch update 2026-11-07:** The prior 2026-11-01 text below is archived background for `experimental/xmq-independent-pipeline`. Do not use its claim that native XMQ-only still invokes legacy `handleTelegram` as a description of this branch's `components/wmbus_native_sx126x` implementation.

The active branch uses the official ESPHome SX126x radio with our native
C++ wM-Bus decoder; `wmbusmeters` original XMQ rules and source JSON
fixtures are compatibility evidence. **Read the root
[`PROJECT_CONTEXT.md`](../../PROJECT_CONTEXT.md) first** for the
branch-specific architecture, read-first instructions, security/admission
restrictions, and verified vs pending CI milestones.

- **Stage A:** shared-source consolidation complete, CI owner-confirmed green;
  see [`STAGE_A_CLOSEOUT.md`](STAGE_A_CLOSEOUT.md).
- **Stage B:** original public upstream fixtures are exercised by host
  source-field regressions. Four originally bad-CCM-tag Kamwater fields
  remain intentionally rejected; no fixture is treated as real
  over-the-air validation. Physical RX-to-HA approval is still open.
- **Native AUTO and Heltec:** `fhkvdataiii` and `mkradio4` remain the
  stable Techem baseline. `iperl`, `hydrus`, `lansenpu`, `lansendw`, `cma12w` are in
  the compiled `SUPPORTED` manifest. Minomess, Kamwater and
  Apator162 remain outside that generic manifest but are **selectable
  through the dedicated, strictly configured `driver: auto` clear
  transport path**, or through explicit `meter_drivers`. AES still
  requires explicit driver/key in `secure_meters`.
- **CI green:** GitHub run `42391722726`, code
  `a051a46a2f5de2680ced80c5c47420d1f6518a87`: all three jobs
  passed, including actual Heltec combined T1/C1 firmware compile
  with six synthetic or public-fixture AUTO bindings. No device was flashed.
  No unconfigured meter automatically creates or publishes sensors.
- **Detailed Stage B gates:** [`BATCH2_STATUS.md`](BATCH2_STATUS.md),
  host XMQ fixture comparisons, native CI and security isolation checks.
  CRC alone does not prove cryptographic authenticity; an intentionally
  invalid CCM-tag source fixture must remain rejected.

## New keyless models: Lansen DW and Elvaco CMA12w

Two further original XMQ model profiles use checked, genuinely clear
short TPL `0x7A` Mode-0 frames. `lansendw` maps door/window
`status`, `error_flags`, and numeric event counters;
`cma12w` maps current/one-hour temperature and optional text
battery/software/status. Both are compiled for Heltec and support
explicit driver assignment or configured-only `driver: auto`.
No unrelated `NOKEY` encrypted frame is accepted as plaintext.
All four original new-driver fixtures, identity/model mismatches,
malformed records, encryption negatives and synthetic CRC passed
in CI run `42391722726`.

## Configured-meter driver selection (native SX126x)

The active `wmbus_native_sx126x` component supports either automatic
driver selection or an explicit driver for a **configured** meter ID:

```yaml
wmbus_native_sx126x:
  sx126x_id: native_sx1262
  link_mode: T1_C1
  meter_drivers:
    - meter_id: "12345678"   # synthetic example only
      driver: auto
```

To select manually, replace `driver: auto` with
`driver: fhkvdataiii`, `mkradio4`, `iperl`, `hydrus`,
`lansenpu`, `lansendw`, `cma12w`, `minomess`, `kamwater` or `apator162`.
For Minomess radio converters, optionally add
`dll_meter_id: "87654321"` (synthetic) to bind the separate radio
sender. The actual data fields must be mapped through `xmq_sensors`
and `xmq_text_sensors`; `auto` does not create Home Assistant
entities for unconfigured meters.

AUTO can select all ten on the verified **unencrypted** native
path. Encrypted telegrams require `secure_meters` with an **explicit
driver and AES key**. A matched model alone is not authorization to
decode or publish. Passive discovery remains metadata-only. The
latest complete, green Heltec CI run is
[42391722726](https://github.com/radugeo74/esphome-sx1262/actions/runs/42391722726).
For a full explanation and examples, use the
[root README](../../README.md).

## Origin of the current pipeline

The current architecture comprises ESPHome SX126x radio integration
and the repository's native `wmbus_native_sx126x` C++ pipeline.
`SzczepanLeon/esphome-components` is relevant to **earlier project
history**, not the active radio/decoder dependency. Upstream
`wmbusmeters` XMQ definitions and any other imported source retain
their original copyrights and licensing requirements. The old
`wmbus_radio` / `wmbus_meter` notes below are archived historical
material, not current installation instructions.

---

## Historical 2026-11-01 note (different branch)

# XMQ / IXML decoding on ESPHome SX1262

## Current status (2026-11-01)

The original Stage 0 feasibility note is superseded by a working experimental
generated-XMQ decoder for two Techem families:

- `fhkvdataiii` — heat allocators, CI `0xA0`
- `mkradio4` — water meters, CI `0xA2`

The experimental pipeline is used on an ESP32-S3 Heltec with ESPHome 2026.10.1.
The owner reports ~126,000 s uninterrupted operation while decoding seven heat
allocators and five water meters. T1 and C1 reception is observed. This is field
evidence, not full protocol coverage.

## Important: XMQ-only is not yet legacy-independent

The YAML option `xmq_only: true` selects XMQ field publishing for the supported
drivers; however, `Meter::handle_frame` still invokes
`this->meter->handleTelegram(...)`. This means legacy frame processing and
some metadata are still used. Do not market this branch as a legacy-free
firmware, nor assume all upstream XMQ drivers are available.

The active generated descriptor catalog currently enumerates exactly
`fhkvdataiii` and `mkradio4`. `xmq_only` for other drivers is rejected
during ESPHome configuration validation.

## Components

- `drivers/*.xmq`: upstream-derived XMQ/IXML driver definitions, with
  original copyright/license notices preserved.
- `tools/generate_ixml_cpp.py`: generates the subset used in firmware.
- `tools/generate_driver_registry.py`: generates driver signatures.
- `portable_cpp/`: portable capture/evaluation runtime.
- `components/wmbus_meter/xmq_decode_catalog.h`: active descriptor list.
- `.github/workflows/xmq-feasibility.yml`: host/ESPHome verification workflow.

## Next milestone: independent frame-to-fields pipeline

Develop only on `experimental/xmq-independent-pipeline`. Keep the known-good
`experimental/wmbus-pure-xmq` branch and installed firmware unchanged.
Regression first: real valid/invalid T1/C1 frames, meter matching, encryption
failures, RSSI, total/target and HCA/temperature parity. Then implement a
bounded frame-to-fields path which does not call `handleTelegram` or depend on
legacy `Telegram` metadata. Finally remove legacy linking for XMQ-only and
extend the driver catalog based on reproducible fixtures.

Use the analyzer for unknown meter IDs, but do not auto-create Home Assistant
entities for unconfigured meters. Do not publish private meter IDs, secrets,
or production YAML in the repository.

## Source and licensing

The originating ESPHome integration is derived from SzczepanLeon and collaborators.
The XMQ driver definitions are based on wmbusmeters, GPL-3.0-or-later.
Record exact upstream revisions when preparing a release and retain licenses.
