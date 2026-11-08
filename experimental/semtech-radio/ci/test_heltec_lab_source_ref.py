#!/usr/bin/env python3
"""Ensure all build-only Heltec lab YAMLs use exact checked-out C++ sources.

This is a CI fidelity guard: an externally cloned branch can be older than
the commit under test, masking changes in the SX1262 driver/RSSI path.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LAB = ROOT / "experimental/semtech-radio"
FILES = (
    LAB / "heltec-sx126x-t1-lab.yaml",
    LAB / "heltec-sx126x-combined-lab.yaml",
    LAB / "heltec-sx126x-diagnostic.yaml",
)
for path in FILES:
    content = path.read_text(encoding="utf-8")
    assert "external_components:" in content, str(path)
    assert (
        "  - source:\n"
        "      type: local\n"
        "      path: ../../components\n"
        "    components: [sx126x, wmbus_native_sx126x]"
    ) in content, f"CI lab must use the checkout's components: {path}"
    assert "github://" not in content, f"Stale remote source in {path}"
    assert "refresh: always" not in content, f"Remote cache refresh in {path}"
    components = (path.parent / "../../components").resolve()
    assert (components / "sx126x/gfsk_rssi.h").is_file(), path
    assert (components / "wmbus_native_sx126x/__init__.py").is_file(), path
print("PASS Heltec T1/combined/diagnostic lab sources: same Git commit,"
      " no detached branch clones or stale RSSI source.")
