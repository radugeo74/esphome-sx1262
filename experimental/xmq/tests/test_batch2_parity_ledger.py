#!/usr/bin/env python3
"""Stage B upstream fixture parity ledger.

This is a machine-readable / human-readable acceptance ledger, NOT a driver
emulator. A fixture cannot be marked PASS unless an independent decoder
actually produced all required fields. Pending cases are visible, not green.
Only public upstream XMQ fixtures are used; never log telegrams or AES keys.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/xmq/tools"))
from native_driver_specs import CONFIGURED_NATIVE, CONFIGURABLE, SUPPORTED
from portable_xmq import parse, validate_driver, blocks, values
from test_batch2_field_gap_audit import tested_fields

EXPECTED_COUNTS = {"minomess": 6, "kamwater": 14, "apator162": 19}
VOLATILE = {"timestamp", "name", "_", "current_status_deprecated_by"}
SUPPORTED_BASELINE = {"minomess": "experimental clear-prefix only",
                      "kamwater": "experimental clear records / compact / encrypted transport",
                      "apator162": "experimental source / parser grammar"}

def read_cases(driver):
    source = (ROOT / "experimental/xmq/drivers" / (driver + ".xmq"))
    tree = validate_driver(parse(source.read_text(encoding="utf-8")))
    groups = blocks(tree, "tests")
    entries = [t["entries"] for g in groups for t in blocks(g["entries"], "test")]
    if len(entries) != EXPECTED_COUNTS[driver]:
        raise AssertionError(
            f"{driver}: upstream case count changed: {len(entries)}")
    cases = []
    for index, entry in enumerate(entries, 1):
        telegrams = values(entry, "telegram")
        jsons = values(entry, "json")
        if len(telegrams) != 1 or len(jsons) != 1:
            raise AssertionError(f"{driver} #{index}: missing source or expected JSON")
        encoded = "".join(telegrams[0].replace("|", "").replace("_", "").split())
        if not re.fullmatch(r"[A-Fa-f0-9]+", encoded) or len(encoded)%2:
            raise AssertionError(f"{driver} #{index}: malformed fixture hex")
        expected = json.loads(jsons[0])
        if expected.get("driver") != driver or not expected.get("id"):
            raise AssertionError(f"{driver} #{index}: wrong expected identity")
        fields = sorted(key for key in expected if key not in VOLATILE
                        and key not in {"driver", "id", "media"})
        if not fields:
            raise AssertionError(f"{driver} #{index}: no testable expected fields")
        comments = values(entry, "comment")
        args = values(entry, "args")
        cases.append({
            "case": index,
            "expected_fields": fields,
            "expected_field_count": len(fields),
            "source": "upstream XMQ test",
            "parity_status": "NOT_RUN",
            "reason": "No production-equivalent replay output compared with expected JSON",
            "comment": comments[0] if comments else "",
            "requires_key": bool(args and re.search(
                r"(?<![0-9A-Fa-f])[0-9A-Fa-f]{32}(?![0-9A-Fa-f])", args[0])),
            "post_decrypted_fixture": bool(args and "NOKEY" in args[0]),
        })
    return cases

def main():
    assert tuple(EXPECTED_COUNTS) == CONFIGURED_NATIVE
    assert set(CONFIGURED_NATIVE).isdisjoint(SUPPORTED)
    assert set(CONFIGURED_NATIVE).issubset(CONFIGURABLE)
    all_cases = {}
    positive_compared=0
    positive_pending=0
    intentionally_rejected=0
    for driver in CONFIGURED_NATIVE:
        all_cases[driver] = read_cases(driver)
        for row in all_cases[driver]:
            index=row["case"]
            expected=row["expected_fields"]
            # This per-field evidence is sourced from the separately
            # compiled host parity tests; NOT production-frame replay.
            compared=sorted(tested_fields(
                driver,index,{key:None for key in expected}))
            assert set(compared).issubset(set(expected))
            row["partial_field_comparisons"]=compared
            row["partial_field_not_compared"]=sorted(set(expected)-set(compared))
            negative=(driver=="kamwater" and index==9)
            row["negative_security_fixture"]=negative
            if negative:
                assert not compared
                intentionally_rejected+=len(expected)
            else:
                positive_compared+=len(compared)
                positive_pending+=len(expected)-len(compared)
    total = sum(len(c) for c in all_cases.values())
    assert (positive_compared,positive_pending,intentionally_rejected)==(177,0,4)
    assert total == 39
    report = {
        "stage": "B",
        "meaning": "Upstream XMQ fixture acceptance ledger, not decoder parity PASS",
        "case_count": total,
        "parity_pass": 0,
        "parity_not_run": total,
        "partial_positive_fields_compared": positive_compared,
        "partial_positive_fields_pending": positive_pending,
        "intentionally_rejected_source_fields": intentionally_rejected,
        "all_decodable_source_fields_have_partial_tests": positive_pending==0,
        "full_driver_parity_approved": False,
        "configured_native_software_available": True,
        "radio_or_firmware_enablement_approved": False,
        "baseline": SUPPORTED_BASELINE,
        "drivers": all_cases,
    }
    if "--json" in sys.argv:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print("Stage B full upstream reference ledger: " + str(total) + " cases")
        for driver, cases in all_cases.items():
            field_count = sum(case["expected_field_count"] for case in cases)
            keyed = sum(case["requires_key"] for case in cases)
            post = sum(case["post_decrypted_fixture"] for case in cases)
            print(f"{driver}: {len(cases)} fixtures, {field_count} expected "
                  f"field comparisons, {keyed} keyed, {post} NOKEY, "
                  "parity 0 PASS / all NOT_RUN")
        print(f"Partial evidence: {positive_compared} source JSON fields "
              f"from accepted fixtures, {positive_pending} positive "
              f"fields outstanding, {intentionally_rejected} fields "
              "from intentionally rejected invalid CCM fixture.")
        print("PASS ledger integrity; all 39 COMPLETE DRIVER parity checks "
              "remain NOT_RUN. Configured native software is available; physical RF/HA approval remains open.")

if __name__ == "__main__":
    main()
