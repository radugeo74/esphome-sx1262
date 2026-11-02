# ESPHome wM-Bus SX1262 — XMQ-only numeric publishing (experimental)

ESPHome external components for receiving wireless M-Bus (wM-Bus) frames with a Heltec ESP32-S3 / SX1262 radio and exporting meter readings to Home Assistant.

> **Branch-specific documentation:** This README describes **`experimental/wmbusmeters-xmq-only`**, not the default branch and **not** `experimental/xmq-independent-pipeline`. The name “XMQ-only” refers to the **source of numeric sensor readings**, not a fully independent wM-Bus stack.

## What this branch actually does

1. The existing SX1262 radio component receives frames and the legacy wmbusmeters-based components perform normal frame conversion and handling.
2. `handleTelegram()` is **still invoked** to match configured meter IDs, parse the telegram and handle decryption as needed.
3. For opted-in meters, a generated XMQ grammar decodes the parsed application frame after that legacy handling.
4. With `xmq_only: true`, numeric ESPHome measurement sensors read values from XMQ results rather than from legacy numeric fields. When XMQ decoding fails, the frame is not marked handled for that meter; when an individual XMQ field is missing, that numeric sensor has no value to publish from this path.
5. With `xmq_shadow: true` and `xmq_only: false`, XMQ is diagnostic-only and normal readings continue to come from legacy.

**This is not yet a removal or replacement of `handleTelegram()`**, and it is not a universal decoder for arbitrary meters. If you need a truly independent pre-legacy decoding comparison, see the separate [independent pipeline branch](https://github.com/radugeo74/esphome-sx1262/tree/experimental/xmq-independent-pipeline).

## Current scope

| Feature | State on this branch |
| --- | --- |
| ESPHome on ESP32-S3 with ESP-IDF | Supported by the existing components; test with your own known-good board/pin configuration |
| SX1262 receiver | Existing radio implementation; not rewritten by XMQ-only work |
| Link modes | `T1` and `C1` configurable (or `Any`); actual application decode depends on the incoming frame |
| `fhkvdataiii` | XMQ grammar for heat-cost allocators |
| `mkradio4` | XMQ grammar for water meters |
| `xmq_shadow` | Observes XMQ decoder status and sampled values without changing numeric sensor authority |
| `xmq_only` | XMQ numeric output is authoritative **for the two supported driver types only**; other drivers are rejected by ESPHome configuration validation |
| XMQ code generation | Generated from `experimental/xmq/drivers/{fhkvdataiii,mkradio4}.xmq` when either flag is enabled |
| Home Assistant | Existing `wmbus_meter` sensor integration; configured entities only |
| Unknown/unconfigured meters | Can be observed at radio level; not automatically created as Home Assistant sensors |

The XMQ decoder uses bounded buffers and rate-limited diagnostic output. Its decoded numbers include heat allocation values / temperature fields or water consumption fields depending on the driver and telegram variant.

## ESPHome example

Adapt the **GPIO pins and board definition** to the actual Heltec revision you have verified. These sample meter IDs are fictional. Do not publish real meter IDs, Wi-Fi credentials, encryption keys or OTA secrets in a public repository.

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
  - source: github://radugeo74/esphome-sx1262@experimental/wmbusmeters-xmq-only
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
    meter_id: 0x12345678  # Replace with your real meter ID
    type: fhkvdataiii
    mode: [T1]
    xmq_shadow: true
    xmq_only: false     # Safe first test: legacy publishes the values
    # key: !secret heat_meter_key  # Supply only when required

  - id: water_meter
    radio_id: radio
    meter_id: 0x87654321
    type: mkradio4
    mode: [T1]
    xmq_shadow: true
    xmq_only: false

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Heat allocator reading"
    field: current_hca
    unit_of_measurement: "HCA"
    accuracy_decimals: 0

  - platform: wmbus_meter
    parent_id: water_meter
    name: "Water meter total"
    field: total_m3
    unit_of_measurement: "m³"
    accuracy_decimals: 1
```

This is an example, **not an OTA-ready replacement for your existing working YAML**. Keep any existing configured sensors, link modes, working radio GPIO mapping and credentials. Compile the actual full YAML before flashing. For reproducibility, pin a tested Git commit rather than relying indefinitely on a moving branch.

## Configuration modes

| Per-meter flags | Numeric sensor authority | Notes |
| --- | --- | --- |
| Both omitted or `false` | Legacy | No XMQ generation required unless another meter opts in |
| `xmq_shadow: true`, `xmq_only: false` | Legacy | XMQ runs after legacy handling, for observation |
| `xmq_only: true` | XMQ | Implicitly enables shadow decoding; legacy still identifies/decrypts; only `fhkvdataiii` and `mkradio4` accepted |

The `rssi_dbm` field is taken from the received telegram, and `timestamp` is taken from the legacy meter state, including when `xmq_only` is enabled. String fields and JSON are also implemented by the existing meter layer; **XMQ-only refers specifically to numeric measurements**.

### What to look for in logs

```text
XMQ shadow driver=fhkvdataiii ok=... fail=... last_status=...
XMQ shadow driver=mkradio4 ok=... fail=... last_status=...
XMQ value ...=...
XMQ badargs driver=... payload_status=...
```

Summaries are rate-limited (roughly one sample per 30 XMQ decode attempts per meter); **no log line does not mean no telegram was received**. Check ESPHome sensor updates as well as logs. An unknown nearby meter not handled by a configured meter is normal.

## Known limitations and next steps

- **Not a pure independent stack:** frame reception, ID matching, telegram handling and any decryption still depend on legacy code.
- XMQ numeric mapping is currently restricted to two supported drivers and their implemented fields. Missing/unsupported fields remain unavailable; there is no automatic fallback to a stale legacy numeric value in `xmq_only`.
- The shadow mode on this branch logs decoder success and values but does **not** provide the more extensive pre-legacy numeric parity diagnostics and independent transport/header checks developed later on `experimental/xmq-independent-pipeline`.
- Real-device validation of the **other branch** must not be presented as equivalent certification of this branch. Retain rollback firmware and validate sensor values, heap, reception, restart behavior and all configured meters before turning `xmq_only` on.
- Broader XMQ driver support and removal of `handleTelegram()` are future development work, not current capabilities.

## Credits and compatibility note

Derived from [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components), with SX1262 / ESP-IDF adaptations and experimental XMQ grammar generation.

**ESPHome:** do not copy an `esp32` folder into your local `components` directory; let ESPHome use its official ESP32 framework integration.
