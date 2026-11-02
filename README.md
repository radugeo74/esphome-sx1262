# ESPHome wM-Bus on Heltec SX1262 — experimental independent XMQ decoding

ESPHome external components for receiving **wireless M-Bus (wM-Bus)** telegrams with an **SX1262** radio on an ESP32-S3 Heltec board, and decoding selected meter formats into Home Assistant sensors.

> **Branch status (November 2026):** This README describes `experimental/xmq-independent-pipeline`, **not** necessarily the repository's default/stable branch. Independent **XMQ-generated** decoding is being validated against the existing wmbusmeters-based implementation. It is **not yet a universal replacement for the legacy decoder**.

## Current capabilities

| Area | Current implementation |
| --- | --- |
| Hardware | ESP32-S3 Heltec board with SX1262; tested with `board: heltec_wifi_lora_32_V3`, 8 MB flash, ESP-IDF |
| Radio | Existing SX1262 receiver; T1 and C1 reception, frame A handling, radio-level CRC/structure diagnostics |
| Meter configuration | Multiple `wmbus_meter` entries with specific meter IDs, driver types, link modes and optional keys |
| Supported independent application decoders | `fhkvdataiii` (heat-cost allocator, manufacturer CI `0xA0`) and `mkradio4` (water, manufacturer CI `0xA2`) |
| Independent path | Configured-meter matching, header/transport inspection, generated XMQ decoding, numeric parity against legacy |
| Home Assistant | `wmbus_meter` numeric sensors (e.g., `current_hca`, `previous_hca`, temperatures, `target_m3`, `total_m3`, RSSI) |
| Security/other CI formats | Experimental Mode 5 (CI `0x72`/`0x7A`) preparation, AES-based decrypt/diagnostics; **not proven** as a general-purpose XMQ payload decoder |
| Other meters | May be detected at radio/header level but **are not automatically decoded or exposed as entities** |

Independent XMQ tables are generated at ESPHome code-generation time from the experimental XMQ driver grammars. The normal configuration does not enable this experimental generation unless XMQ is opted in.

### What the two XMQ options mean

Both settings are optional per configured meter:

| Setting | Behavior |
| --- | --- |
| No XMQ flags (defaults) | Existing legacy behavior |
| `xmq_shadow: true`, `xmq_only: false` | **Recommended field test.** Decode and compare independently, but **legacy remains authoritative** for Home Assistant sensor publishing |
| `xmq_only: true` | **Experimental.** Uses independent XMQ values for supported drivers; do not deploy as the first test. Current implementation still calls legacy `handleTelegram()` for some handling/comparison work. The name does **not** mean legacy code has been removed from the firmware |

The parity logs distinguish equal, different and missing values; an `observed_match` only establishes **observed numeric equality**, not that every legacy field was freshly updated by that particular telegram. Date fields and the full telemetry lifecycle need further validation before removing legacy.

## Real-device validation to date

On **1 November 2026**, an experimental shadow build on a real Heltec ESP32-S3 (ESPHome **2026.10.1**) successfully received configured meters, updated Home Assistant sensors through legacy, and produced independent XMQ results for real **T1 A / CI A0 and A2** telegrams.

Two captured log sessions showed matching XMQ/legacy numeric fields for the observed configured heat/water meters, including `different=0` and `missing_xmq=0` in reported parity summaries. The capture covered **11 of 12 configured meters**, not yet an exhaustive test of every meter and repeated cycle. Some unconfigured nearby meters were correctly left unhandled.

**Limitations:** a successful host CI run does not prove runtime behavior; the short on-device capture cannot establish long-term memory stability, packet-loss behavior or comprehensive field freshness. There were occasional radio-operation latency warnings; monitor under sustained traffic. These observations are **not** certification for production-wide XMQ-only deployment.

## Example configuration — safe shadow mode

Adapt this to your **own board revision and existing verified GPIO mapping**. The pins below match the tested Heltec ESP32-S3 setup; do **not** blindly copy them to another Heltec model.

```yaml
esphome:
  name: heltec-wmbus
  friendly_name: Heltec wM-Bus

esp32:
  board: heltec_wifi_lora_32_V3
  flash_size: 8MB
  framework:
    type: esp-idf

logger:
  level: INFO

external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/xmq-independent-pipeline
    refresh: always

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret heltec_api_key

ota:
  - platform: esphome
    password: !secret heltec_ota_password

spi:
  clk_pin: GPIO9
  mosi_pin: GPIO10
  miso_pin: GPIO11

wmbus_radio:
  id: radio
  radio_type: SX1262
  cs_pin: GPIO8
  reset_pin: GPIO12
  irq_pin: GPIO14

wmbus_meter:
  - id: heat_meter
    radio_id: radio
    meter_id: 0x12345678  # Dummy; replace with your own meter ID
    type: fhkvdataiii
    mode: [T1]
    xmq_shadow: true
    xmq_only: false
    # key: !secret heat_meter_key  # Only if required by your meter

  - id: water_meter
    radio_id: radio
    meter_id: 0x87654321  # Dummy; replace with your own meter ID
    type: mkradio4
    mode: [T1]
    xmq_shadow: true
    xmq_only: false

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Heat allocator current"
    field: current_hca
    unit_of_measurement: "HCA"
    state_class: total_increasing
    accuracy_decimals: 0

  - platform: wmbus_meter
    parent_id: water_meter
    name: "Water total"
    field: total_m3
    unit_of_measurement: "m³"
    device_class: water
    state_class: total_increasing
    accuracy_decimals: 1
```

This is an **illustrative** YAML, not a substitute for an existing working production configuration. Keep all existing meters, sensor mappings, link modes and secrets in your own configuration. Build/validate your **full actual YAML** before OTA; keep a known-good firmware for rollback. Do not commit real meter identifiers or secrets to a public repository.

## Experimental strict pre-legacy numeric publishing (new, not yet hardware-validated)

An additional per-meter opt-in, `xmq_pure_publish`, selects the **pre-legacy independent XMQ A0/A2 decode result** as the numeric sensor source. This is separate from existing `xmq_only` behavior:

```yaml
wmbus_meter:
  - id: test_meter
    meter_id: 0x12345678  # Sample only
    type: mkradio4
    mode: [T1]
    xmq_shadow: true
    xmq_only: true
    xmq_pure_publish: true  # Explicit, experimental opt-in
```

**Do not enable this on the current production Heltec configuration yet.** In this mode, missing/failed independent plaintext decoding produces **no new XMQ numeric reading** and does not fall back to post-legacy or stale values. Only finite decoded numeric fields are carried forward; legacy telegram handling and callback lifecycle **still execute**, so this is **not** end-to-end replacement of wmbusmeters. CI `0x72`/`0x7A`, other formats and unsupported drivers are outside this numeric publishing gate. This option requires `xmq_only: true` and currently targets only `fhkvdataiii` and `mkradio4`.

This change needs ESPHome compile/CI, a controlled canary and a parity/availability check before field deployment. Existing `xmq_shadow` and `xmq_only` configurations are unchanged unless the new option is explicitly set.

## Logs and testing

Useful diagnostic prefixes:

- `Independent XMQ plain diagnostic` — A0/A2 standalone decode attempt and status.
- `Independent XMQ vs legacy parity` — comparable, equal, differing and missing numeric fields.
- `Independent XMQ plain parity field` — bounded identification of the field causing a mismatch.
- `XMQ field parity`, `XMQ shadow` — supporting per-meter comparisons and decode result.
- `Pure DLL CRC parity`, `Pure radio parity` — independent radio/frame checks.
- `Pure TPL diag` and `Independent Mode5` — transport classification and **experimental** Mode 5 diagnostics.

When interpreting logs, verify that a **configured meter actually transmitted**: zero mismatches without a decoded frame are not validation. Warnings about an unhandled meter ID are expected for nearby, unconfigured devices.

A first-field-test/rollback checklist is provided in [experimental/xmq/HELTEC_SHADOW_TEST.md](experimental/xmq/HELTEC_SHADOW_TEST.md). CI and portable decoder tests are under `experimental/xmq/`; they complement rather than replace tests on hardware.

## Roadmap / not yet complete

1. Observe multiple real telegrams from **all configured meters**, checking numeric readings, dates, publish continuity, heap use and reception performance.
2. Resolve any XMQ/legacy differences; distinguish current-telegram values from potentially retained legacy values.
3. Validate the independent sensor publishing lifecycle and failure handling before considering XMQ-only as default.
4. Remove reliance on `handleTelegram()` **only after** equivalent behavior is demonstrated. Expand the XMQ driver catalog beyond the current two formats in separate, controlled work.
5. Keep Mode 5, C1/security variants and other manufacturer formats behind explicit tests until they are validated with matching real meter payloads.

## Origin and credits

Based on [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components), with SX1262/ESP-IDF adaptations and ongoing experimental XMQ independent-decoder work.

> **ESPHome integration note:** Do **not** copy an `esp32` folder into local `components`; use the official ESPHome ESP32 framework integration. This avoids component conflicts on newer versions of ESPHome.

### Experimental direct plaintext XMQ dispatch (opt-in)

The two tested drivers (`mkradio4` and `fhkvdataiii`) can bypass the legacy
`meter->handleTelegram()` call for CRC-verified, CRC-stripped **plaintext**
A2/A0 telegrams. Enable **only on one canary meter first**:

```yaml
xmq_shadow: true
xmq_only: true
xmq_pure_publish: true
xmq_direct_plain: true
```

`xmq_direct_plain` defaults to `false`. It requires both `xmq_only` and
`xmq_pure_publish`; a configured encryption key is rejected at YAML validation.
On decode failure it **does not fall back** to legacy and does not publish the
frame. The existing radio receiver, frame-to-handler routing, sensor callback,
RSSI and entity IDs remain in use. This **does not** remove the wmbusmeters
library from the firmware or make encrypted C1/Mode-5 telegrams work directly.
Legacy meter objects are still created for configuration/metadata; the plaintext
numeric decode/publish path itself skips `handleTelegram()`.

Watch the `XMQ direct publish` log line and actual Home Assistant sensor
updates, then revert the flag to `false` if updates stop. Do not enable it on
all meters until an individual ESPHome firmware build and live canary test pass.


### Raw discovery / unverified frame feeds (experimental)

The radio now classifies each successfully converted frame **once**. Frames with
valid independent headers that do not match any configured meter are logged as
`DISCOVERED`, with ID, manufacturer, version, type, CI, link mode, RSSI and raw
hex. Frames converted by the radio but rejected by the independent header
parser are logged as `RAW_UNVERIFIED` with status, mode, RSSI and raw hex. No
measurements are inferred from unverified frames. Malformed frames that fail
radio-to-frame conversion still remain visible in the packet DEBUG logs; they
do not fire the frame-level callbacks.

Two optional ESPHome `wmbus_radio:` automation hooks expose the raw frame for
external publishing/recording without creating HA sensor entities for every
neighboring meter:

```yaml
wmbus_radio:
  # ...existing radio options...
  on_discovered:
    - lambda: |-
        ESP_LOGI("wmbus_discovery", "DISCOVERED raw=%s", frame->as_hex().c_str());
  on_unverified:
    - lambda: |-
        ESP_LOGW("wmbus_discovery", "RAW_UNVERIFIED raw=%s", frame->as_hex().c_str());
```

Attach your own bounded MQTT/event transport in those hooks if you want the
feed outside ESPHome logs; these hooks **do not auto-publish to MQTT**.
Configured meter handlers run only when independent header parsing succeeds and
the meter ID matches; invalid headers no longer cause all configured direct-XMQ
handlers to report redundant failures. General `on_frame` automations remain
unchanged. Verify ESPHome compilation/CI before applying this experimental mode
to a live receiver.
