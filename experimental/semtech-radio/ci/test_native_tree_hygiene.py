#!/usr/bin/env python3
"""Reject obsolete legacy imports/build fixtures on the native-only branch."""
from pathlib import Path
import ast
import re

ROOT = Path(__file__).resolve().parents[3]
retired_paths = (
    ".github/workflows/xmq-feasibility.yml",
    "experimental/xmq/ci/esp32s3-esphome.yaml",
    "experimental/xmq/tests/xmq_shadow_ci.yaml",
    "experimental/xmq/espidf_probe",
)
for path in retired_paths:
    assert not (ROOT / path).exists(), f"Retired artifact reintroduced: {path}"

for yaml_rel in (
    "experimental/semtech-radio/heltec-sx126x-t1-lab.yaml",
    "experimental/semtech-radio/heltec-sx126x-combined-lab.yaml",
    "experimental/semtech-radio/ci/native-sx126x-esphome.yaml",
):
    yaml = (ROOT / yaml_rel).read_text(encoding="utf-8")
    assert "components: [sx126x, wmbus_native_sx126x]" in yaml
    assert not re.search(
        r"(?m)^\s*(?:wmbus_radio|wmbus_meter|wmbus_common):\s*$", yaml
    ), f"Legacy configuration introduced in {yaml_rel}"

# Check actual source/build references, not archived Markdown discussion.
bad = []
for dirname in ("components", "experimental", ".github"):
    for path in (ROOT / dirname).rglob("*"):
        if not path.is_file() or path.suffix not in (
            ".cpp", ".cc", ".h", ".py", ".yaml", ".yml", ".sh", ".c"
        ):
            continue
        if path == Path(__file__):
            continue
        text = path.read_text(encoding="utf-8")
        if re.search(
            r"(?:components/wmbus_(?:radio|meter|common)/"
            r"|\.\./wmbus_(?:radio|meter|common)/"
            r")",
            text,
        ):
            bad.append(str(path.relative_to(ROOT)))
assert not bad, f"Obsolete legacy source path reference(s): {bad}"


# Guard against silently copying an offline parity catalog into firmware.
# Inspect the literal allowlist without importing ESPHome in this host test.
native_init = ROOT / "components/wmbus_native_sx126x/__init__.py"
source = native_init.read_text(encoding="utf-8")
module = ast.parse(source)
declarations = [
    node.value for node in module.body
    if isinstance(node, ast.Assign)
    and any(isinstance(target, ast.Name)
            and target.id == "PORTABLE_RUNTIME_HEADERS"
            for target in node.targets)
]
assert len(declarations) == 1, "Expected one explicit runtime header allowlist"
copied = tuple(ast.literal_eval(declarations[0]))
assert copied == (
    "calculate_subset.h",
    "calendar_subset.h",
    "driver_runtime.h",
    "ixml_subset.h",
    "numeric_bridge.h",
    "record_decoder.h",
), "Portable-to-native header allowlist changed"
assert not set(copied) & {
    p.name for p in (ROOT / "components/wmbus_native_sx126x").glob("*.h")
}, "A copied portable header shadows an owned native header"
assert '.glob("*.h")' not in source, (
    "Unbounded portable header copying could overwrite native decoder sources"
)
for name in copied:
    assert (ROOT / "experimental/xmq/portable_cpp" / name).is_file(), (
        f"Missing runtime helper {name}"
    )
assert "xmq_decode_catalog.h" not in copied
assert "xmq_field_parity.h" not in copied

print("native-only source and fixture hygiene: PASS")
