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
consumers. SX127x optional radio support is retained: both its listener and PHY decode feedback remain available for compatible builds.
Retained: T1 3-of-6 and the existing validated Format A/B DLL CRC bridge,
whose replacement requires byte-for-byte real-frame regression tests.

### Independent DLL PHY CRC (EN 13757)

The radio path now uses `phy_crc.h` for format-A/B DLL CRC verification and
stripping instead of `wmbus_common/wmbus.cc` `trimCRCsFrameFormatA/B`.
It also performs an explicit frame-length check instead of calling
`checkWMBusFrame` (the legacy resynchronization/parsing helper). A regression
test includes an authentic CRC-verified T1/A telegram from Heltec log (80),
CRC mutations, truncated frames, and generated format-B fixtures. SX126x and
SX127x both retain PHY support. Common legacy link type declarations still
remain for compatibility; full removal of wmbus_common is a separate step.


### Wmbusmeters analyzer URLs in the ESPHome logs

The radio can print a ready-to-open analyzer URL for a CRC-valid, CRC-stripped
T1 or C1 telegram. It does not send network requests and does not use the
legacy decoder. No URL is emitted for a PHY-invalid telegram. The URL contains
raw meter identity/data; avoid sharing logs publicly without redaction.

```yaml
wmbus_radio:
  # ... existing radio configuration ...
  analyzer_links: unhandled  # default; options: off, unhandled, all
```

`unhandled` reports only frames not decoded by a configured XMQ meter;
`all` also reports decoded frames. URLs are capped to one per radio meter
identity in 60 seconds (64-entry fixed cache) to reduce log spam.
