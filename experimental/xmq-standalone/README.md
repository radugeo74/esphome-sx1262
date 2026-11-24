# XMQ standalone — exclusive ESPHome SX1262 path

Branch: `experimental/xmq-standalone-no-legacy`, independent of the proven
`experimental/xmq-runtime-stage-a` rollback path.

## Implemented

- Native SX1262 T1/C1 PHY and CRC verification unchanged.
- wmbus_common radio-only mode omits the old `meters.cc` legacy decoder
  from the ESPHome build. The shared framing/CRC library remains available.
- Radio frame dispatch is **compiled without the legacy Telegram::parseHeader
  path** when the XMQ-only component is enabled.
- Independent XMQ driver registry and IXML application decoder for
  `fhkvdataiii` (4 numeric fields) and `mkradio4` (2 numeric fields).
- Twelve configured XMQ meter instances, independently routed by numeric
  meter IDs, publishing their fields and RSSI as ESPHome sensors.
- A single GitHub workflow compiles a complete Heltec ESPHome 2026.11.0
  firmware with 12 **synthetic meter IDs**, Wi-Fi, encrypted API and OTA
  credentials that are **fake and unusable for an actual device**.
- Host fixtures validate independent XMQ extraction for 7 heat + 5 water
  logical instances. No `wmbus_meter` or shadow observer in this fixture.

## Limitations and physical rollout gate

- The proprietary Techem T1 A0/A2 **known clear-form** envelope is handled
  only for documented versions and types. No independent cryptographic
  authentication exists for proprietary frames; do not treat a byte prefix
  as proof of authenticated cleartext.
- C1 radio still receives and verifies CRC; **XMQ does not yet decode
  encrypted C1 frames**, and no actual key should be committed.
- All 12 physical meters have NOT yet been observed using this standalone
  firmware. Full runtime operation is not established by a green CI build.
- The public synthetic build fixture has no real meter IDs and MUST NOT
  be installed on the user's Heltec. Keep the current Stage 6 firmware and
  OTA rollback image before attempting an authorized private installation.
- A private YAML using the 12 known IDs and 50 existing sensor names can
  be maintained locally; it must never be pushed to this public repository.
- Verify 12/12 live meters, state updates in Home Assistant, memory,
  SX1262 loop warnings, failure handling and safe rollback before replacing
  production firmware.

## Configuration

Use external components from this branch: `components` for
`sx126x,wmbus_common,wmbus_radio`, and
`experimental/xmq-standalone/components` for `xmq_meter`.
Set `wmbus_radio.direct_xmq_only: true`, `native_stream_mode: T1_C1`.
Use one `xmq_meter` entry per real meter, with `family: heat|water`,
`meter_id`, `radio_id` and the existing named sensor fields.

Never include `wmbus_meter:` or `xmq_shadow_probe:` in this firmware.

## Cleanup: exclusive radio dispatch

Standalone radio dispatch no longer contains a legacy parser branch or
meter-address subscription fallback. XMQ Frame subscribers are the only
consumers. SX127x optional headers are excluded under XMQ_STANDALONE_ONLY.
Retained: T1 3-of-6 and the existing validated Format A/B DLL CRC bridge,
whose replacement requires byte-for-byte real-frame regression tests.
