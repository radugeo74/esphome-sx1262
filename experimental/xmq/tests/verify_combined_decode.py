#!/usr/bin/env python3
"""Combined manufacturer-payload decoding check against upstream fixture JSON.

Host-only experimental subset. Uses the XMQ IXML captures, typed DIF/VIF
records and original XMQ calendar formulas. Does not handle whole-frame
transport parsing or the full wmbusmeters expression language.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from calendar_probe import compute_dates
from driver_pipeline_probe import extract_records, decode
from portable_xmq import blocks, parse, validate_driver, values


def fixtures(path):
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    for test_group in blocks(driver, "tests"):
        for item in blocks(test_group["entries"], "test"):
            fields = item["entries"]
            telegram = values(fields, "telegram")
            expected = values(fields, "json")
            if telegram and expected and "_" in telegram[0]:
                yield telegram[0].split("_", 1)[1], json.loads(expected[0])


def decoded_heating(path, payload):
    records = extract_records(path, payload)
    required = {"426E", "026E", "0265", "025D", "02FD3A", "42FD3A"}
    if not required.issubset(records):
        raise ValueError("Missing HCA, temperature or date-raw DIF/VIF records")
    dates = compute_dates(path, {
        "previous_date_raw_counter": records["02FD3A"]["scaled"],
        "current_date_raw_counter": records["42FD3A"]["scaled"],
    })
    return {
        "previous_hca": records["426E"]["scaled"],
        "current_hca": records["026E"]["scaled"],
        "temp_room_c": records["0265"]["scaled"],
        "temp_radiator_c": records["025D"]["scaled"],
        **dates,
    }


def verify_all():
    expected_counts = {"fhkvdataiii": 2, "mkradio4": 1}
    total = 0
    for name, count in expected_counts.items():
        path = ROOT / "drivers" / f"{name}.xmq"
        examples = list(fixtures(path))
        if len(examples) != count:
            raise AssertionError(f"{name}: expected {count} marked fixtures, got {len(examples)}")
        for idx, (payload, expected) in enumerate(examples, 1):
            if name == "fhkvdataiii":
                observed = decoded_heating(path, payload)
            else:
                observed = {key: val for key, val in decode(path, payload).items()
                            if key in ("total_m3", "target_m3")}
            for field, got in observed.items():
                want = expected[field]
                if isinstance(got, (int, float)):
                    if abs(got - want) > 1e-6:
                        raise AssertionError(f"{name} #{idx} {field}: {got} != {want}")
                elif got != want:
                    raise AssertionError(f"{name} #{idx} {field}: {got!r} != {want!r}")
            total += 1
            print(f"PASS {name} #{idx}: {observed}", flush=True)
    if total != 3:
        raise AssertionError(f"Unexpected count: {total}")
    return total


if __name__ == "__main__":
    verify_all()
