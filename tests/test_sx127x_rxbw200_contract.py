#!/usr/bin/env python3
"""A/B guard: 200-kHz C1/A-only trial changes only RX bandwidth, with boost OFF."""
from copy import deepcopy
from pathlib import Path
import yaml

root = Path(__file__).resolve().parents[1]
def config(name):
    return yaml.safe_load((root / "tests" / name).read_text())

baseline = config("lilygo-t3s3-sx1276-c1a-only-ci.yaml")
trial = config("lilygo-t3s3-sx1276-c1a-rxbw200-ci.yaml")
boost = config("lilygo-t3s3-sx1276-c1a-lna-boost-ci.yaml")

assert baseline["sx127x"]["bandwidth"] == "250_0kHz"
assert "wmbus_lna_boost_hf" not in baseline["sx127x"], "Baseline must be boost-OFF by default"
assert boost["sx127x"]["wmbus_lna_boost_hf"] is True
assert boost["sx127x"]["bandwidth"] == "250_0kHz"
assert trial["sx127x"]["wmbus_lna_boost_hf"] is False
assert trial["sx127x"]["bandwidth"] == "200_0kHz"
assert trial["sx127x"]["wmbus_c1a_only"] is True
assert trial["sx127x"]["sync_value"] == [0x54, 0xCD]
assert trial["wmbus_radio"]["native_stream_mode"] == "T1_C1"
assert trial["wmbus_radio"]["native_phy_diagnostics"] is True
assert trial["sx127x"]["bitrate"] == baseline["sx127x"]["bitrate"] == 100000
assert trial["sx127x"]["deviation"] == baseline["sx127x"]["deviation"] == 50000
assert trial["sx127x"]["frequency"] == baseline["sx127x"]["frequency"] == 868950000
assert "sensor" not in trial
assert "key" not in str(trial["wmbus_meter"])
assert trial["wmbus_meter"][0]["meter_id"] == 0x12345678

expected = deepcopy(baseline)
expected["esphome"]["name"] = "lilygo-t3s3-c1a-rxbw200-ci"
expected["sx127x"]["bandwidth"] = "200_0kHz"
expected["sx127x"]["wmbus_lna_boost_hf"] = False
assert trial == expected, "Unexpected difference versus 250-kHz C1/A-only boost-OFF baseline"

rf_header = (root / "components/sx127x/sx127x_reg.h").read_text()
rf_test = (root / "tests/test_sx127x_rf_profile.cpp").read_text()
assert "RX_BW_200_0 = 0x09" in rf_header
assert "RX_BW_250_0 = 0x01" in rf_header
assert "static_assert(RX_BW_200_0 == 0x09" in rf_test
assert "static_assert(AFC_BW_333_3 == 0x10" in rf_test
print("PASS: 200-kHz isolated trial differs only by RX bandwidth, boost OFF, same C1/A receiver and meters")
