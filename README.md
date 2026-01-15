Don't add esp32 folder to components, it will mess up with your yaml compilation on a new version of esphome. If it's not present it will use official version of esp32.

Modified from https://github.com/SzczepanLeon/esphome-components with radio SX1262 drivers.

Working with this version of esp32 board https://heltec.org/project/wireless-stick-lite-v2/


# Usage example:
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
  - source: github://radugeo74/esphome-sx1262@master # Forked from github://SzczepanLeon/esphome-components

api:

ota:
  - platform: esphome
    password: secret password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "Heltec-Stick-Lite-8Mb"
    password: secret password

time:
  - platform: sntp
    id: time_sntp
    timezone: secret

spi:
  clk_pin: GPIO9
  mosi_pin: GPIO10
  miso_pin: GPIO11

wmbus_meter:
  - id: heat_meter_bathroom
    meter_id: 0x_________ #change with yours
    type: fhkvdataiii
    key: "00000000000000000000000000000000"
    mode: 
      - T1
wmbus_radio:
  radio_type: SX1262
  cs_pin: GPIO8
  reset_pin: GPIO12
  irq_pin: GPIO14

sensor:
  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "ESP bathroom meter  RSSi"
    id: ESP_bathroom meter_RSSi
    field: "rssi_dbm"
    accuracy_decimals: 0
    unit_of_measurement: "dBm"
    device_class: "signal_strength"
    state_class: "measurement"
    entity_category: "diagnostic"
  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "ESP bathroom meter"
    id: ESP_bathroom_meter
    field: "current_hca"
    unit_of_measurement: "HCA"
    device_class: "energy"
    state_class: "total_increasing"
    accuracy_decimals: 0
  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "ESP heat meter bathroom - previous"
    id: ESP_heat_meter_bathroom_previous
    field: "previous_hca"
    unit_of_measurement: "HCA"
    device_class: "energy"
    state_class: "measurement"
    accuracy_decimals: 0
  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "ESP heat meter bathroom - radiator temperature"
    id: ESP_heat_meter_bathroom_radiator_temperature
    field: "temp_radiator_c"
    unit_of_measurement: "°C"
    device_class: "temperature"
    state_class: "measurement"
    accuracy_decimals: 1
  - platform: wmbus_meter
    parent_id: heat_meter_bathroom
    name: "ESP heat meter bathroom - room temperature"
    id: ESP_heat_meter_bathroom_room_temperature
    field: "temp_room_c"
    unit_of_measurement: "°C"
    device_class: "temperature"
    state_class: "measurement"
    accuracy_decimals: 1
