#!/usr/bin/env python3
"""Guard native SX126x against accidental reintroduction of legacy drivers.

The native XMQ decoder is independent of driver_*.cc in wmbus_common;
shared legacy support files remain in place for a separate cleanup.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
COMMON = ROOT / "components" / "wmbus_common"
NATIVE = ROOT / "components" / "wmbus_native_sx126x"

old_drivers = sorted(COMMON.glob("driver_*.cc"))
assert not old_drivers, (
    "Retired legacy C++ drivers reintroduced: " +
    ", ".join(p.name for p in old_drivers)
)

for name in ("meters.cc", "dvparser.cc", "aes.cc", "wmbus.cc"):
    assert (COMMON / name).is_file(), f"Common source unexpectedly removed: {name}"

init = (NATIVE / "__init__.py").read_text(encoding="utf-8")
assert 'DEPENDENCIES = ["sx126x"]' in init, "Native receiver dependency changed"
assert not re.search(r"(?m)^\s*(?:from|import)\s+.*wmbus_common", init), (
    "Native component must not import legacy wmbus_common"
)

for relative in (
    "experimental/semtech-radio/heltec-sx126x-t1-lab.yaml",
    "experimental/semtech-radio/heltec-sx126x-combined-lab.yaml",
    "experimental/semtech-radio/ci/native-sx126x-esphome.yaml",
):
    yaml = (ROOT / relative).read_text(encoding="utf-8")
    assert "components: [sx126x, wmbus_native_sx126x]" in yaml, (
        f"Native-only component list changed in {relative}"
    )
    assert not re.search(r"(?m)^(?:wmbus_common|wmbus_radio|wmbus_meter):\s*$", yaml), (
        f"Legacy wM-Bus component reintroduced in {relative}"
    )

print("native-only legacy driver retirement: PASS (91 legacy sources removed; shared core retained)")
