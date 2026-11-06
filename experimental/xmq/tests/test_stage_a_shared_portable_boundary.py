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
# Kamwater status/numeric algorithms now have exactly one native header each.
cmake=(root/"experimental/xmq/espidf_cpp_probe/main/CMakeLists.txt").read_text(encoding="utf-8")
assert '"../../../../components/wmbus_native_sx126x"' in cmake
for name, symbol in (
    ("kamwater_status_semantics.h", "inline Status decode("),
    ("kamwater_numeric_semantics.h", "inline Status decode("),
):
    target=root/"components/wmbus_native_sx126x"/name
    alias=root/"experimental/semtech-radio/host_only"/name
    target_text=target.read_text(encoding="utf-8")
    alias_text=alias.read_text(encoding="utf-8")
    assert symbol in target_text
    assert '#include "standard_dif_vif.h"' in target_text
    assert f'#include "../../../components/wmbus_native_sx126x/{name}"' in alias_text
    assert "inline Status decode(" not in alias_text
    assert f'#include "{name}"' in cpp
assert "kw_status.status != \"DRY\"" in cpp
assert 'field.name=="max_flow_last_day_m3h"' in cpp
assert 'field.name=="min_flow_temperature_last_day_c"' in cpp
print("PASS stage A: same Apator162 plus Kamwater status/numeric parsers for host and ESP-IDF, "
      "pending driver remains unavailable for live YAML and RF publication")
