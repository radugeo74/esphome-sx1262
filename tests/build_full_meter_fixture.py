#!/usr/bin/env python3
"""Synthetic, no-real-identifiers CI coverage of the 12-meter SX126x bridge.

This is NOT the user's deployable YAML and must never be flashed. It mirrors
the structure of the private configuration without copying IDs, addresses,
credentials or home sensor names into the public repository.
"""
from copy import deepcopy
from pathlib import Path
import yaml

ROOT = Path(__file__).resolve().parent
source = ROOT / "sx126x-wmbus-bridge.yaml"
dest = ROOT / "sx126x-wmbus-12-meters-ci.yaml"
config = yaml.safe_load(source.read_text(encoding="utf-8"))
config["esphome"]["name"] = "sx126x-12-meter-ci"
config["esphome"]["friendly_name"] = "Synthetic 12-Meter Native SX126x CI"
config["api"] = {
    "encryption": {"key": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="}
}
config["ota"] = [{"platform": "esphome", "password": "test-no-ota"}]
config["wifi"] = {
    "ssid": "test-synthetic-ssid",
    "password": "test-synthetic-password",
    "ap": {"ssid": "test-native-ci", "password": "test-synthetic-ap-pass"},
}
config["time"] = [
    {"platform": "sntp", "id": "time_sntp", "timezone": "Europe/Bucharest"}
]
config["debug"] = {"update_interval": "60s"}
config["esp32"]["framework"]["advanced"] = {"loop_task_stack_size": 12288}
config["output"] = [
    {
        "platform": "ledc",
        "id": "status_led",
        "pin": "GPIO35",
        "frequency": "1000 Hz",
    }
]
config["wmbus_radio"]["on_frame"] = [
    {
        "then": [
            {
                "lambda": (
                    'ESP_LOGD("wmbus_rx", "CRC-valid frame %u bytes, RSSI %d dBm",'
                    ' (unsigned) frame->data().size(), (int) frame->rssi());'
                )
            },
            {
                "output.set_level": {"id": "status_led", "level": "5%"},
            },
            {"delay": "50ms"},
            {"output.turn_off": "status_led"},
        ],
    }
]

measurements = []
configured = []
field_types = {}
for i in range(12):
    is_heat = i < 7
    meter_id = f"0x{0x11110001 + i:08x}"
    meter_ref = f"synthetic_{'heat' if is_heat else 'water'}_{i + 1:02d}"
    configured.append(
        {
            "id": meter_ref,
            "radio_id": "wmbus_receiver",
            "meter_id": meter_id,
            "type": "fhkvdataiii" if is_heat else "mkradio4",
            "mode": ["T1"],
        }
    )
    fields = (
        ["rssi_dbm", "current_hca", "previous_hca", "temp_radiator_c", "temp_room_c"]
        if is_heat
        else ["rssi_dbm", "target_m3", "total_m3"]
    )
    for field in fields:
        sensor = {
            "platform": "wmbus_meter",
            "parent_id": meter_ref,
            "field": field,
            "name": f"Synthetic {i + 1:02d} {field}",
        }
        if field == "rssi_dbm":
            sensor.update(
                {
                    "unit_of_measurement": "dBm",
                    "device_class": "signal_strength",
                    "state_class": "measurement",
                    "accuracy_decimals": 0,
                    "entity_category": "diagnostic",
                }
            )
        elif field.endswith("_hca"):
            sensor.update(
                {
                    "unit_of_measurement": "HCA",
                    "state_class": "measurement" if field == "previous_hca" else "total_increasing",
                    "accuracy_decimals": 0,
                }
            )
        elif field.endswith("_c"):
            sensor.update(
                {
                    "unit_of_measurement": "°C",
                    "device_class": "temperature",
                    "state_class": "measurement",
                    "accuracy_decimals": 1,
                }
            )
        elif field.endswith("_m3"):
            sensor.update(
                {
                    "unit_of_measurement": "m³",
                    "device_class": "water",
                    "state_class": "total_increasing" if field == "total_m3" else "total",
                    "accuracy_decimals": 1,
                }
            )
        measurements.append(sensor)

config["wmbus_meter"] = configured
config["sensor"] = measurements + [
    {
        "platform": "debug",
        "free": {"name": "Synthetic Heap Free"},
        "min_free": {"name": "Synthetic Heap Min"},
        "block": {"name": "Synthetic Largest Block"},
        "fragmentation": {"name": "Synthetic Fragmentation"},
    },
    {"platform": "uptime", "name": "Synthetic Uptime"},
    {
        "platform": "wifi_signal",
        "name": "Synthetic WiFi Signal",
        "update_interval": "300s",
    },
]
config["captive_portal"] = None
assert len(config["wmbus_meter"]) == 12
assert len(measurements) == 50
assert len(config["sensor"]) == 53
assert {m["type"] for m in configured} == {"fhkvdataiii", "mkradio4"}
assert len({m["meter_id"] for m in configured}) == 12
content = (
    "# Synthetic 12-meter bridge fixture. DO NOT FLASH.\n"
    "# No real device, meter or network identifiers.\n"
    + yaml.safe_dump(config, allow_unicode=True, sort_keys=False)
)
dest.write_text(content, encoding="utf-8")
print(f"PASS: generated {dest.name} with 12 meters and 50 wM-Bus sensors")
