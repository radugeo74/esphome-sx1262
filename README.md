Updated to work and compile with ESPHome 2026.7.0

# ESPHome wM-Bus for Heltec Wireless Stick Lite V2 (SX1262)

This project provides ESPHome support for reading **wM-Bus** meters using **SX1262** radio drivers. It is specifically tailored for the **Heltec Wireless Stick Lite V2** board.

---

> [!CAUTION]
> **Important Note on Compilation:**
> **Do not add the `esp32` folder to your local `components` directory.** Doing so will cause YAML compilation errors in newer versions of ESPHome. If the folder is not present, ESPHome will correctly use the official ESP32 framework version.

---

## Origin & Credits
Modified from [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components) with added/modified drivers for the **SX1262** radio.

## Hardware Support
* **Board:** [Heltec Wireless Stick Lite V2](https://heltec.org/project/wireless-stick-lite-v2/)
* **Radio:** Integrated SX1262

## Usage Example (YAML)

Use the following configuration as a template for your `.yaml` file. Ensure you replace the `meter_id` and secret credentials with your own.

```yaml
esphome:
  name: heltec-stick-lite-8mb
  friendly_name: Heltec stick lite 8Mb

esp32:
  board: heltec_wifi_lora_32_V3
  flash_size: 8MB
  framework:
    type: esp-idf

logger:
  level: INFO
  baud_rate: 115200

external_components:
  - source: github://radugeo74/esphome-sx1262@master # Forked from SzczepanLeon

api:

ota:
  - platform: esphome
    password: "your_password"

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "Heltec-Stick-Lite-8Mb"
    password: "your_password"

time:
  - platform: sntp
    id: time_sntp
    timezone: "Europe/London"

spi:
  clk_pin: GPIO9
  mosi_pin: GPIO10
  miso_pin: GPIO11

wmbus_radio:
  radio_type: SX1262
  cs_pin: GPIO8
  reset_pin: GPIO12
  irq_pin: GPIO14

wmbus_meter:
  - id: heat_meter_bathroom
    meter_id: 0x_________ # Replace with your meter ID
    type: fhkvdataiii
    key: "00000000000000000000000000000000"
    mode: 
      - T1

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "Bathroom Meter RSSI"
    field: "rssi_dbm"
    unit_of_measurement: "dBm"
    device_class: "signal_strength"
    state_class: "measurement"
    entity_category: "diagnostic"

  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "Bathroom Meter Current Reading"
    field: "current_hca"
    unit_of_measurement: "HCA"
    device_class: "energy"
    state_class: "total_increasing"
    accuracy_decimals: 0

  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "Bathroom Meter Radiator Temp"
    field: "temp_radiator_c"
    unit_of_measurement: "°C"
    device_class: "temperature"
    state_class: "measurement"
    accuracy_decimals: 1
