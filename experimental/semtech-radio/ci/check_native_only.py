#!/usr/bin/env python3
"""Fail CI if the official-SX126x fixture reintroduces the legacy radio.

This is a guard for the experimental build fixture, NOT evidence that the
production wM-Bus pipeline has already migrated to native hardware.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "experimental/semtech-radio/ci/native-sx126x-esphome.yaml"
PROBE = ROOT / "components/wmbus_native_sx126x/native_receiver_probe.h"
WORKFLOW = ROOT / ".github/workflows/native-sx126x-adapter.yml"

yaml = FIXTURE.read_text(encoding="utf-8")
probe = PROBE.read_text(encoding="utf-8")
workflow = WORKFLOW.read_text(encoding="utf-8")

def section(name):
    return bool(re.search(rf"(?m)^{re.escape(name)}:\s*$", yaml))

assert section("sx126x"), "The native fixture must declare official sx126x"
assert section("wmbus_native_sx126x"), "Native listener must be configured"
assert not section("wmbus_radio"), "Legacy radio cannot own the same SPI chip"
assert not re.search(r"(?m)^\s*radio_type\s*:", yaml), "Legacy radio_type is forbidden"
assert not re.search(r"(?m)^\s*(?:reset_pin|irq_pin|rx_gain)\s*:", yaml), "Legacy radio settings forbidden"
assert "wmbus_radio," not in yaml and "components: [wmbus_radio" not in yaml
assert "sx126x::SX126xListener" in probe, "Native component must use official listener"
assert "esphome compile experimental/semtech-radio/ci/native-sx126x-esphome.yaml" in workflow
print("native-only fixture: PASS (production migration still pending)")
