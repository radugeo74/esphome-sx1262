#!/usr/bin/env python3
"""Native branch must not contain the original C++/Python wM-Bus components."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
for component in ("wmbus_common", "wmbus_meter", "wmbus_radio", "socket_transmitter"):
    assert not (ROOT / "components" / component).exists(), (
        f"Legacy component reintroduced: {component}"
    )

native = ROOT / "components" / "wmbus_native_sx126x"
init = (native / "__init__.py").read_text(encoding="utf-8")
assert 'DEPENDENCIES = ["sx126x"]' in init, "Native receiver dependency changed"
assert not re.search(r"(?m)^\s*(?:from|import)\s+.*wmbus_common", init)

reference = ROOT / "experimental/xmq/legacy_reference"
assert len(list(reference.glob("pure_*.h"))) == 15, (
    "Portable reference fixtures missing or unexpected count"
)
for name in ("xmq_decode_catalog.h", "xmq_field_parity.h"):
    assert (ROOT / "experimental/xmq/portable_cpp" / name).is_file(), (
        f"Portable comparison source missing: {name}"
    )

print("native-only components: PASS (legacy runtime removed; portable tests retained)")
