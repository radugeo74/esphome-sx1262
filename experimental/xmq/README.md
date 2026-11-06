# XMQ — active context on `experimental/xmq-multidriver-auto`

> **Branch update 2026-11-06:** The prior 2026-11-01 text below is archived background for `experimental/xmq-independent-pipeline`. Do not use its claim that native XMQ-only still invokes legacy `handleTelegram` as a description of this branch's `components/wmbus_native_sx126x` implementation.

The active branch uses the official ESPHome SX126x radio with our native
C++ wM-Bus decoder; `wmbusmeters` original XMQ rules and source JSON
fixtures are compatibility evidence. **Read the root
[`PROJECT_CONTEXT.md`](../../PROJECT_CONTEXT.md) first** for the
branch-specific architecture, read-first instructions, security/admission
restrictions, and verified vs pending CI milestones.

- **Stage A:** shared-source consolidation complete, CI owner-confirmed green;
  see [`STAGE_A_CLOSEOUT.md`](STAGE_A_CLOSEOUT.md).
- **Stage B:** batch-2 source-fixture parity ongoing; source JSON subset
  coverage is **168/181 last confirmed green**, with **177/181 proposed
  pending CI confirmation** at this update (four remaining fields belong
  to an intentionally invalid CCM-tag fixture). This is partial evidence,
  **not** full-frame or per-driver production parity.
- **Live/pending manifest:** `tools/native_driver_specs.py`.
  `fhkvdataiii` and `mkradio4` remain the established active baseline;
  `iperl`/`hydrus` are manifest-supported native cleartext decoders;
  **`minomess`, `kamwater`, `apator162` remain blocked in
  `BATCH2_PENDING`**. No YAML activation, flashing, experimental OpenSSL
  integration, or unknown-meter sensor auto-publication.
- **Detailed Stage B gates:** [`BATCH2_STATUS.md`](BATCH2_STATUS.md),
  host XMQ fixture comparisons, native CI and security isolation checks.
  CRC alone does not prove cryptographic authenticity; an intentionally
  invalid CCM-tag source fixture must remain rejected.

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
