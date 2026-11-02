# ESPHome wM-Bus SX1262 — XMQ shadow decoder (experimental)

ESPHome external components for receiving wireless M-Bus telegrams using an SX1262 radio on Heltec ESP32-S3 hardware and exposing configured meter readings to Home Assistant.

> **This README describes the branch `experimental/wmbusmeters-xmq` only.** It implements an early **XMQ shadow decoder** alongside the existing wmbusmeters-based processing. It does **not** publish independent XMQ measurements as authoritative sensor values.

## What this branch does

1. The existing SX1262 receiver gets wM-Bus frames, with configured link modes including **T1** and **C1**.
2. The existing wmbusmeters-based meter calls `handleTelegram()` to identify/configure matching meters and parse/decrypt telegrams.
3. If a configured meter has `xmq_shadow: true`, and its telegram was matched and not rejected for decryption, the experimental XMQ decoder examines the **already parsed** telegram frame.
4. XMQ decoder success/failure counters and sampled numeric decoded fields are written to logs.
5. **All Home Assistant sensor values remain supplied by the existing legacy meter implementation.** Shadow decoding does not replace sensor publishing or `handleTelegram()`.

This branch is a useful staging point for testing XMQ grammars without changing normal sensor readings. It is **not** a standalone independent protocol stack or a universal meter decoder.

## Features and scope

| Feature | Current behavior |
| --- | --- |
| Radio | Existing SX1262 implementation; keep verified Heltec GPIO mappings |
| Framework | ESPHome ESP32-S3 with ESP-IDF |
| Configured meters | `wmbus_meter` instances by `meter_id`, `type`, key where needed, and `mode` |
| XMQ grammars | `fhkvdataiii` heat-cost allocators and `mkradio4` water meters |
| Supported shadow diagnostics | Decode status, successful/failed frame counters and sample numeric values |
| Code generation | Builds XMQ tables from `experimental/xmq/drivers/fhkvdataiii.xmq` and `mkradio4.xmq` when shadow is enabled |
| Home Assistant | Legacy `wmbus_meter` sensors; no independent XMQ publication |
| Unconfigured devices | May be seen by the radio, but are not automatically configured as sensors |

The grammars operate on supported telegram payload variants. The reception of a T1/C1 frame **does not guarantee** that its application payload is supported by XMQ.

## Configuration

This branch has just **one XMQ-specific per-meter option**:

```yaml
xmq_shadow: true
```

If omitted or set to `false`, XMQ shadow decoding is disabled for that meter. **`xmq_only` is not supported on this branch**; do not copy that option from newer branches.

### Example — Heltec ESP32-S3 / SX1262

Adapt the board and GPIO pins to your **verified hardware revision**. The meter ID below is fictional.

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
  - source: github://radugeo74/esphome-sx1262@experimental/wmbusmeters-xmq
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
    meter_id: 0x12345678 # Replace with an actual configured meter ID
    type: fhkvdataiii
    mode: [T1]
    xmq_shadow: true
    # key: !secret heat_meter_key  # If needed

  - id: water_meter
    radio_id: radio
    meter_id: 0x87654321
    type: mkradio4
    mode: [T1]
    xmq_shadow: true

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Heat allocator current"
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

The example is **not a drop-in replacement** for an existing full device configuration. Preserve your real radio settings, sensors, meter IDs and ESPHome options. Store credentials and meter encryption keys in `secrets.yaml`, not GitHub. Compile your actual device YAML and retain known-good firmware before any OTA installation.

## Understanding the logs

When shadow mode is enabled, look for messages such as:

```text
XMQ shadow driver=fhkvdataiii ok=... fail=... last_status=...
XMQ shadow driver=mkradio4 ok=... fail=... last_status=...
XMQ value current_hca=...
XMQ value total=...
XMQ badargs driver=... payload_status=...
```

The summary and decoded numeric fields are **rate-limited**, so an absent log line is not proof that a meter has not transmitted. This branch provides diagnostic counts and sample values, **not automated field-by-field XMQ/legacy parity**. For a meaningful evaluation, compare values with Home Assistant, check several telegram cycles per configured device, and monitor heap, radio responsiveness and resets.

Nearby unconfigured meters may generate “Telegram not handled by any handler”; this is normal if their IDs do not match your configuration.

## Relationship to newer experimental branches

| Branch | What changes |
| --- | --- |
| **`experimental/wmbusmeters-xmq`** (this branch) | Initial XMQ **shadow only**, after legacy telegram handling; legacy remains the only publishing source |
| [`experimental/wmbusmeters-xmq-only`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/wmbusmeters-xmq-only) | Adds `xmq_only: true` to use XMQ numeric values as the publishing source for the two supported drivers, while still depending on legacy for matching/decryption |
| [`experimental/xmq-independent-pipeline`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/xmq-independent-pipeline) | Adds independent pre-legacy decoding and more extensive radio/header/field parity diagnostics. Its recent Heltec testing does **not** automatically validate this older branch |

## Limitations and future direction

- `handleTelegram()` and the legacy wmbusmeters layer are still required for meter matching, parsing/decryption and Home Assistant values.
- XMQ shadow supports **two driver grammars**, not all wM-Bus manufacturers or modes.
- The log-only shadow output does not establish full sensor parity or data freshness.
- Decrypting a telegram does not itself guarantee the experimental XMQ grammar can decode the resulting application data.
- Independent decoding and eventual removal of legacy should be approached on the newer pipeline branch, after real-device verification.

## Origin and credits

Based on [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components) with SX1262/ESP-IDF work and experimental XMQ grammar decoding.

> **ESPHome note:** Do **not** copy an `esp32` directory into your local `components` directory; use the official ESPHome ESP32 framework integration.
