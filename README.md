# ESPHome wireless M-Bus on SX1262 — native ESP-IDF

ESPHome external components for receiving **wireless M-Bus (wM-Bus)** telegrams on ESP32-based Heltec hardware using an **SX1262** transceiver, and publishing configured meter measurements to Home Assistant.

> **Branch:** `native-espidf`. This is the **legacy wmbusmeters-based ESP-IDF implementation**. It does **not** include the experimental XMQ shadow, XMQ-only, or independent XMQ pipeline features documented on the other branches.

## Features

- SX1262 radio integration with ESPHome and the **ESP-IDF** framework.
- wM-Bus **T1** and **C1** reception support, subject to radio configuration and meter compatibility.
- Multiple configured meters using a `meter_id`, driver `type`, optional encryption `key`, and link-mode `mode`.
- Existing wmbusmeters-derived meter drivers compiled/registered as needed; driver availability depends on the files in `components/wmbus_common/`.
- Home Assistant numeric and text sensors via ESPHome, plus optional frame and telegram automations.
- Optional radio-frame forwarding through components such as `socket_transmitter` where separately configured.

**What is not included:** `xmq_shadow`, `xmq_only`, independent pre-legacy decoding, XMQ grammar generation, or XMQ/legacy parity diagnostics.

## Hardware

This repository includes an SX1262 implementation used with **Heltec ESP32-S3 boards**. The sample below uses:

- ESPHome board identifier: `heltec_wifi_lora_32_V3`
- ESP-IDF framework
- 8 MB flash configuration
- SPI on GPIO9/GPIO10/GPIO11, radio CS GPIO8, reset GPIO12 and IRQ GPIO14

**These settings are specific to the example configuration.** Heltec Wireless Stick Lite V2, WiFi LoRa 32 V3 and other Heltec revisions must **not** be treated as interchangeable without checking their actual board definition, radio wiring, flash size and pin mapping.

## ESPHome configuration example

This configuration shows a single `fhkvdataiii` heat-cost allocator with placeholder identification and secrets. Replace the sample ID, configure the driver and GPIOs for **your actual hardware**, and add the sensor fields you need.

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
  - source: github://radugeo74/esphome-sx1262@native-espidf

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
    meter_id: 0x12345678  # EXAMPLE ONLY — replace with your meter's real ID
    type: fhkvdataiii
    mode: [T1]
    # key: !secret heat_meter_key  # Uncomment only if required by your meter

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Heat meter RSSI"
    field: rssi_dbm
    unit_of_measurement: "dBm"
    device_class: signal_strength
    state_class: measurement
    entity_category: diagnostic

  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Heat allocation current"
    field: current_hca
    unit_of_measurement: "HCA"
    accuracy_decimals: 0

  - platform: wmbus_meter
    parent_id: heat_meter
    name: "Radiator temperature"
    field: temp_radiator_c
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement
    accuracy_decimals: 1
```

**Do not copy the sample values blindly.** A fictional `meter_id` will not match your real device; the selected driver must match its format. A key of all zeros is **not** a generic decryption key and should not be copied as an example credential. If your meter requires encryption, supply its actual key privately via ESPHome secrets.

For multiple meters, create more `wmbus_meter` entries and attach each sensor to the appropriate `parent_id`. Configure `mode: [T1]`, `mode: [C1]`, or `mode: [T1, C1]` according to what the meter actually transmits.

### Optional frame logging

You can add an `on_frame` automation under `wmbus_radio`, for example:

```yaml
wmbus_radio:
  id: radio
  radio_type: SX1262
  cs_pin: GPIO8
  reset_pin: GPIO12
  irq_pin: GPIO14
  on_frame:
    - then:
        - lambda: |-
            ESP_LOGD("wmbus_frame", "RSSI: %d dBm, bytes: %u",
                     frame->rssi(), (unsigned) frame->data().size());
```

This is an **alternative radio block** showing the optional automation, not a second `wmbus_radio` block to append to the configuration. Avoid high-volume raw-telegram logging in busy RF environments if it impacts processing latency.

## Operational notes

- A nearby meter with an ID not configured in ESPHome may be received but will not create Home Assistant entities automatically.
- Monitor serial/API logs, radio reception, ESPHome sensor updates, memory and reboot behavior when modifying hardware or firmware settings.
- Test the **complete** YAML with your ESPHome version before installing OTA. Save a working YAML and recovery firmware first.
- For reproducible deployments, consider pinning a tested commit rather than a moving branch reference.

## Experimental XMQ branches

| Branch | Purpose |
| --- | --- |
| **`native-espidf`** (this branch) | ESP-IDF + SX1262 + existing wmbusmeters decoder |
| [`experimental/wmbusmeters-xmq`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/wmbusmeters-xmq) | First XMQ shadow decoder, still publishing legacy values |
| [`experimental/wmbusmeters-xmq-only`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/wmbusmeters-xmq-only) | Experimental authoritative XMQ numeric values, while legacy processing remains |
| [`experimental/wmbus-pure-xmq`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/wmbus-pure-xmq) | Initial independent protocol checks and pre-legacy XMQ decode |
| [`experimental/xmq-independent-pipeline`](https://github.com/radugeo74/esphome-sx1262/tree/experimental/xmq-independent-pipeline) | Extended independent pipeline and field parity diagnostics |

The successful Heltec XMQ field-test observations on the newer experimental branch should **not** be interpreted as testing or certification of this `native-espidf` branch.

## Origin and credits

Derived from [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components), with SX1262/ESP-IDF adaptations.

> **ESPHome compatibility:** Do **not** copy an `esp32` folder into a local `components` directory. ESPHome already supplies the official ESP32 integration, and a duplicate component can cause configuration or build conflicts.
