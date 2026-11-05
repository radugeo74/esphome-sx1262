#!/usr/bin/env python3
"""Batch 2: validate official XMQ fixture inventory and fail-closed dispatch.

These are *source/fixture adoption* checks, not proof of a live RF decoder.
Do not mistake XMQ detection metadata for a fully implemented driver.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/xmq/tools"))
from native_driver_specs import BATCH2_PENDING, SUPPORTED, POST_DECRYPT_ONLY
from portable_xmq import parse, validate_driver, blocks, values

EXPECTED = {
    "minomess": ("Zenner", 6, "total_m3"),
    "kamwater": ("Kamstrup", 5, "total_m3"),
    "apator162": ("Apator", 8, "total_m3"),
}

def check(name):
    p = ROOT / "experimental/xmq/drivers" / f"{name}.xmq"
    data = p.read_text(encoding="utf-8")
    driver = validate_driver(parse(data))
    assert values(driver["entries"], "name") == [name], name
    assert values(driver["entries"], "manufacturer") == [EXPECTED[name][0]], name
    groups = blocks(driver["entries"], "tests")
    assert groups, f"{name}: no upstream fixtures"
    fixture_blocks = [fixture for group in groups for fixture in blocks(group["entries"], "test")]
    assert len(fixture_blocks) >= EXPECTED[name][1], (
        f"{name}: expected at least {EXPECTED[name][1]} original cases; got {len(fixture_blocks)}"
    )
    for fixture in fixture_blocks:
        source = values(fixture["entries"], "telegram")
        expected = values(fixture["entries"], "json")
        assert len(source) == 1 and len(expected) == 1, name
        raw = re.sub(r"[|_\\s]", "", source[0])
        assert len(raw) >= 24 and len(raw) % 2 == 0, name
        assert re.fullmatch("[0-9A-Fa-f]+", raw), name
        reference = json.loads(expected[0])
        assert reference["driver"] == name, name
        assert reference["id"], name
        assert EXPECTED[name][2] in reference, name
    print(f"{name}: {len(fixture_blocks)} upstream fixtures inventoried (not live enabled)")
    return len(fixture_blocks)

def main():
    assert tuple(EXPECTED) == BATCH2_PENDING
    assert set(BATCH2_PENDING).isdisjoint(SUPPORTED + POST_DECRYPT_ONLY)
    for name in BATCH2_PENDING:
        check(name)
    compiled = (ROOT / "components/wmbus_native_sx126x/xmq_native_compiled.h")
    # The generated header exists during CI after build_native_generated.py.
    if compiled.is_file():
        text = compiled.read_text()
        for name in BATCH2_PENDING:
            assert f'std::strcmp(name, "{name}")' not in text
    print("PASS: official batch-2 sources present; all remain fail-closed")

if __name__ == "__main__":
    main()
