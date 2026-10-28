#!/usr/bin/env python3
"""Convert IXML captures to typed little-endian integers, verify official outputs.

Experimental bridge only; quantities and formulas are intentionally scoped to
fixtures, not a replacement for upstream DIF/VIF and calculate evaluation.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from ixml_probe import extract_grammar, grammar_rules, marked_fixtures, run_ixml
from portable_xmq import blocks, parse, validate_driver, values


def uint16_captures(captures):
    result = {}
    for key, value in captures.items():
        binary = bytes.fromhex(value)
        if len(binary) == 2:
            result[key] = int.from_bytes(binary, "little", signed=False)
    return result


def reference_fields(path):
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    tests = [t for group in blocks(driver, "tests") for t in blocks(group["entries"], "test")]
    for test in tests:
        e = test["entries"]
        telegram = values(e, "telegram")
        expected = values(e, "json")
        if telegram and expected and "_" in telegram[0]:
            yield telegram[0].split("_", 1)[1], json.loads(expected[0])


def evaluate_fixture(name, numbers):
    # These two fixture-specific conversions demonstrate the bridge. The
    # generic DIF/VIF and calculate-expression evaluator is still outstanding.
    if name == "mkradio4":
        return {"target_m3": numbers["DV_prev"] / 10.0,
                "total_m3": (numbers["DV_prev"] + numbers["DV_curr"]) / 10.0}
    if name == "fhkvdataiii":
        return {"previous_hca": numbers["DV_prev_hca"],
                "current_hca": numbers["DV_curr_hca"]}
    raise ValueError(f"No fixture adapter for {name}")


def verify():
    verified = 0
    for path in sorted((ROOT / "drivers").glob("*.xmq")):
        grammar = grammar_rules(extract_grammar(path)[0])
        for payload, expected in reference_fields(path):
            captures = run_ixml(grammar, payload)
            numbers = uint16_captures(captures)
            observed = evaluate_fixture(path.stem, numbers)
            for key, actual in observed.items():
                want = expected[key]
                if abs(actual - want) > 1e-9:
                    raise AssertionError(f"{path.stem} {key}: {actual} != {want}")
            verified += 1
            print(f"PASS {path.stem}: {observed} (captures={len(captures)})", flush=True)
    if verified != 3:
        raise AssertionError(f"Expected 3 marked original fixtures, checked {verified}")
    return verified


if __name__ == "__main__":
    verify()
