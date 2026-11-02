# ESPHome wM-Bus SX1262 — Pure protocol diagnostics + XMQ (experimental)

ESPHome external components for reading wireless M-Bus (wM-Bus) meters with an SX1262 radio on a Heltec ESP32-S3, with an **experimental independent ("pure") protocol inspection path** and XMQ-generated application decoders.

> **Branch:** `experimental/wmbus-pure-xmq`. This is a development step between the earlier wmbusmeters-assisted XMQ branches and [`experimental/xmq-independent-pipeline`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/xmq-independent-pipeline). It is **not** a complete replacement for wmbusmeters or an all-meter universal decoder.

## How it works

The SX1262 radio receiver and the legacy wmbusmeters path are still present. For eligible telegrams, this branch additionally performs independent protocol checks:

1. **Header / identity:** parses the radio frame header independently, matches the configured meter ID and compares driver identification against an XMQ-generated signature registry.
2. **Transport / security:** inspects the transport CI and security envelope, including manufacturer-specific messages and diagnostic handling of Mode 5 preparation.
3. **Pre-legacy XMQ:** when shadow is enabled and a matching XMQ driver descriptor exists, attempts application payload decoding **before** calling `handleTelegram()`. This gives an independent observation path for recognized unencrypted manufacturer-specific frames.
4. **Legacy path:** still calls `handleTelegram()` to handle the meter in the established ESPHome integration.
5. **Post-legacy XMQ:** can also decode the parsed application frame, compare numeric fields with legacy data and log bounded diagnostics. This second path remains dependent on legacy parsing/decryption.

The pre-legacy and post-legacy paths have **different independence guarantees**. A successful post-legacy comparison alone does not mean the entire protocol stack operates without wmbusmeters.

## Scope and configuration

| Item | This branch |
| --- | --- |
| Hardware | ESP32-S3 / Heltec SX1262, ESP-IDF; keep your tested GPIO map |
| Radio | Existing receiver, T1 and C1 frame reception and low-level diagnostics |
| Pure parsing | Header, meter ID matching, driver signature selection, transport/security classification |
| XMQ decoders | `fhkvdataiii` heat-cost allocation (`CI 0xA0`) and `mkradio4` water (`CI 0xA2`) |
| Pure pre-legacy decode | Diagnostic attempt where a supported descriptor matches |
| Mode 5 | Security/prepare diagnostics, **not general proof of independent decrypted application decoding** |
| Sensor publication | Legacy by default; optional experimental XMQ numeric value source |
| Unknown meters | Can be recognized at radio/header level but are not automatically added as Home Assistant entities |

The XMQ tables and signature registry are generated during ESPHome code generation from `experimental/xmq/drivers/fhkvdataiii.xmq` and `mkradio4.xmq` when an XMQ flag is enabled. The available decoders are **not** a general catalog for every manufacturer's wM-Bus format.

### XMQ options (per configured meter)

| YAML options | What happens |
| --- | --- |
| Neither option specified | Normal legacy numeric sensor publishing; pure radio/header diagnostics may still run |
| `xmq_shadow: true` with `xmq_only: false` | **Recommended first test:** experimental XMQ observations run, legacy remains the publishing authority |
| `xmq_only: true` | **Experimental:** publishes supported XMQ numeric values, while the legacy path still handles meter identification, parsing/decryption and other state |

`xmq_only` does **not** disable or remove `handleTelegram()`, and does not guarantee an independent end-to-end decoder. In XMQ-only mode, a missing decoded numeric field should not be interpreted as a valid zero or as an automatically refreshed legacy value.

## Heltec configuration example

This is an **illustration**, not a drop-in production configuration. Match your actual board revision, pins, supported modes and sensors. Meter IDs below are placeholders; keep credentials and AES keys in `secrets.yaml`.

```yaml
esphome:
  name: heltec-wmbus

esp32:
  board: heltec_wifi_lora_32_V3
  flash_size: 8MB
  framework:
    type: esp-idf

logger:
  level: INFO

external_components:
  - source: github://radugeo74/esphome-sx1262@experimental/wmbus-pure-xmq
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
    meter_id: 0x12345678 # Fake example ID
    type: fhkvdataiii
    mode: [T1]
    xmq_shadow: true
    xmq_only: false

  - id: water_meter
    radio_id: radio
    meter_id: 0x87654321 # Fake example ID
    type: mkradio4
    mode: [T1]
    xmq_shadow: true
    xmq_only: false

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Heat allocation"
    field: current_hca
    unit_of_measurement: "HCA"
    accuracy_decimals: 0

  - platform: wmbus_meter
    parent_id: water_meter
    name: "Water total"
    field: total_m3
    unit_of_measurement: "m³"
    accuracy_decimals: 1
```

Verify the **full actual ESPHome configuration** before installing firmware, and preserve a known-good binary for rollback. Avoid committing actual meter identifiers or secrets. A moving branch URL is convenient for development; pin a reviewed commit for reproducible deployments.

## Diagnostics and interpretation

Typical useful log prefixes include:

- `Pure registry shadow`: independent registry selection versus configured/legacy driver name.
- `Pure TPL diag`: transport CI and security mode classification.
- `Pure mode5 diagnostic`: preparation status for supported Mode 5 envelopes.
- `Pure pipeline prelegacy`: number of attempts, successes/failures, driver and decoded value count **before** legacy processing.
- `XMQ shadow`, `XMQ HCA secondary decode`, `XMQ HCA parity gate`: post-legacy decoding and reasons comparison may not run.
- `XMQ field parity summary`: sampled numeric field comparisons with the legacy decoder.

A pre-legacy successful decode is useful evidence but is **not** proof that the same frame can be published without legacy, that every sensor field is fresh or that all security modes are supported. Counters may be rate-limited. If unrelated nearby meters are received, unhandled-meter warnings are generally expected.

## Relationship to the other branches

| Branch | Distinguishing feature |
| --- | --- |
| [`experimental/wmbusmeters-xmq`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/wmbusmeters-xmq) | First XMQ shadow integration, **after** legacy processing |
| [`experimental/wmbusmeters-xmq-only`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/wmbusmeters-xmq-only) | Adds XMQ numeric publishing as an option, but relies on legacy handling |
| **`experimental/wmbus-pure-xmq`** (this branch) | Adds independent frame/identity/transport checks and initial **pre-legacy** XMQ decoding |
| [`experimental/xmq-independent-pipeline`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/xmq-independent-pipeline) | More extensive independent pipeline, per-field parity and live Heltec shadow-test diagnostics |

The recent **12/12 meter reception** observations from Heltec concern the **newer independent-pipeline firmware**. They are not evidence that this earlier branch has undergone the same test.

## Remaining work

- Verify the pure path with multiple genuine T1/C1 telegrams and all configured meter types; distinguish CRC/header issues from payload decoder errors.
- Finish fresh-value and full numeric/date parity validation across the independent and legacy paths.
- Validate long-term heap usage, radio latency and Home Assistant updates before any broader XMQ-only rollout.
- Remove dependency on `handleTelegram()` only after identity, security, decoding and sensor lifecycle have independently passed real-device tests.
- Expand the XMQ driver catalog separately; do not assume a CI or manufacturer identifier implies a compatible payload.

## Credits and ESPHome note

Origin: [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components), adapted for SX1262/ESP-IDF and experimental XMQ decoding.

**Compatibility:** Do not place a copied `esp32` folder in your local `components` directory; let ESPHome use its official ESP32 platform integration.
