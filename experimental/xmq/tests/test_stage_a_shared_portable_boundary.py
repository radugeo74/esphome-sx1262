#!/usr/bin/env python3
"""Stage A: one shared application parser, no new live decoder admission.
Structural guard only; source XMQ golden C++ tests and real ESP-IDF probe
run separately in their existing jobs.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[3]
shared=root/"experimental/xmq/portable_cpp/apator162_ixml_fields.h"
compat=root/"experimental/semtech-radio/host_only/apator162_ixml_fields.h"
probe=root/"experimental/xmq/espidf_cpp_probe/main/main.cpp"
component=root/"components/wmbus_native_sx126x/__init__.py"
manifest=root/"experimental/xmq/tools/native_driver_specs.py"
assert shared.is_file() and compat.is_file() and probe.is_file()
shared_text=shared.read_text(encoding="utf-8")
legacy=compat.read_text(encoding="utf-8")
cpp=probe.read_text(encoding="utf-8")
names=manifest.read_text(encoding="utf-8")
assert "inline Status decode_clear_manufacturer(" in shared_text
assert '#include "../../xmq/portable_cpp/apator162_ixml_fields.h"' in legacy
assert "decode_clear_manufacturer" not in legacy
assert '#include "apator162_ixml_fields.h"' in cpp
assert 'apator162_reading.total_m3 - 7.512' in cpp
assert "BATCH2_PENDING = (\"minomess\", \"kamwater\", \"apator162\")" in names
assert "PORTABLE = (\"iperl\", \"hydrus\")" in names
assert "for header in (xmq / \"portable_cpp\").glob(\"*.h\")" in component.read_text(encoding="utf-8")
assert not (root/"components/wmbus_native_sx126x/apator162_ixml_fields.h").exists(), (
    "Shared module should be copied at build time only, not committed as a second source")
print("PASS stage A: same Apator162 application decoder for host and ESP-IDF, "
      "pending driver remains unavailable for live YAML and RF publication")
