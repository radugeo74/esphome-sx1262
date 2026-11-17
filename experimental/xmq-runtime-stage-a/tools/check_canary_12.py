#!/usr/bin/env python3
"""Structural safety gate for the 12-meter canary overlay.

This test does not read or expose secrets and cannot prove runtime RF parity.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
overlay = (root / "heltec-xmq-canary-12-meters.yaml").read_text()
base = (root / "heltec-xmq-canary.yaml").read_text()

heat = ("baie", "dormitor_1", "sufragerie", "bucatarie",
        "dormitor_2", "dormitor_3", "wc_serviciu")
water = ("rece_baie", "rece_bucatarie", "calda_baie",
         "calda_bucatarie", "calda_wc_serviciu")
all_meters = [("heat", n) for n in heat] + [("water", n) for n in water]

assert "connectivity: !include heltec-xmq-canary.yaml" in overlay
assert "enabled: false" in base
assert len(re.findall(r"^  - id: ", overlay, re.M)) == 12
assert len(re.findall(r"^  - platform: wmbus_meter$", overlay, re.M)) == 50
assert "0x12345678" not in overlay
for prefix, name in all_meters:
    key = prefix + "_" + name
    assert len(re.findall(r"^  - id: " + re.escape(key) + r"$", overlay, re.M)) == 1, key
    assert overlay.count("meter_id: !secret " + key + "_id") == 1, key
    assert overlay.count("key: !secret " + key + "_key") == 1, key
    assert overlay.count("parent_id: " + key + "\n") == (5 if prefix == "heat" else 3), key
    assert overlay.count("type: " + ("fhkvdataiii" if prefix == "heat" else "mkradio4") + "\n") >= 1
assert len(re.findall(r"^    mode: \[T1\]$", overlay, re.M)) == 12
assert not re.search(r"^\s*(?:meter_id|key): (?!\!secret).+", overlay, re.M)
print("CANARY_12_OK: 7 heat + 5 water; all 12 IDs/AES keys local; 50 sensors; shadow disabled")
