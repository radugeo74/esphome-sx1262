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
