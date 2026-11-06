#!/usr/bin/env python3
"""Real partial XMQ parity: run compiled C++ clear-application decoder on
upstream-supplied post-decryption bytes, then compare to JSON expected values.
Never claim raw encrypted frames or full driver parity.
"""
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/xmq/tools"))
from portable_xmq import parse, validate_driver, blocks, values

# Explicitly reviewed references from original driver tests:
# minomess 3-6: wireless MODE 5 fixtures with pre-decrypted application after '_'
# kamwater 1, 5, 6: ELL 8D20 NOKEY fixtures with pre-decrypted application.
# Other cases need wired M-Bus, compact profile handling, authenticated
# decryption, manufacturer-specific records or additional fields.
CASES = {"minomess": (3, 4, 5, 6), "kamwater": (1, 3, 5, 6)}
FIELDS = {
    "minomess": ("total_m3", "target_m3", "meter_date", "target_date", "status"),
    "kamwater": ("total_m3", "target_m3"),
}

def fixtures(driver):
    p = ROOT / "experimental/xmq/drivers" / (driver + ".xmq")
    parsed = validate_driver(parse(p.read_text(encoding="utf-8")))
    return [t["entries"] for g in blocks(parsed, "tests")
            for t in blocks(g["entries"], "test")]

def output_values(text):
    pairs = {}
    for line in text.splitlines():
        key, sep, val = line.partition("=")
        if not sep or key in pairs:
            raise AssertionError("Malformed/duplicate decoder output field")
        if val == "null":
            pairs[key] = None
        elif key.endswith("_m3"):
            pairs[key] = float(val)
        else:
            pairs[key] = val
    return pairs

def main():
    binary = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
        "/tmp/test-batch2-clear-fixture-cli")
    if not binary.is_file():
        raise SystemExit("Compile the actual C++ fixture decoder before running parity")
    compared = 0
    covered = 0
    for driver, indexes in CASES.items():
        entries = fixtures(driver)
        for number in indexes:
            entry = entries[number - 1]
            telegram = values(entry, "telegram")[0]
            # Deliberately require a single unambiguous upstream boundary.
            parts = telegram.split("_")
            if len(parts) != 2:
                raise AssertionError(f"{driver} #{number}: upstream clear boundary missing")
            expected = json.loads(values(entry, "json")[0])
            after = "".join(parts[1].replace("|", "").split())
            if not after or len(after) % 2:
                raise AssertionError(f"{driver} #{number}: invalid application hex")
            result = subprocess.run(
                [str(binary), driver, after], check=False,
                capture_output=True, text=True)
            if result.returncode:
                raise AssertionError(
                    f"{driver} #{number}: native decoder rejected upstream "
                    f"clear application (exit {result.returncode})")
            actual = output_values(result.stdout)
            if set(actual) != set(FIELDS[driver]):
                raise AssertionError(
                    f"{driver} #{number}: inconsistent native output fields")
            for field in FIELDS[driver]:
                expected_value = expected.get(field)
                actual_value = actual[field]
                if expected_value is None:
                    assert actual_value is None, (
                        f"{driver} #{number} {field}: expected absent/null "
                        f"but native decoder emitted a value")
                elif isinstance(expected_value, (float, int)):
                    assert isinstance(actual_value, float) and math.isclose(
                        actual_value, expected_value, rel_tol=0, abs_tol=1e-6), (
                        f"{driver} #{number} {field}: numeric mismatch")
                else:
                    assert actual_value == expected_value, (
                        f"{driver} #{number} {field}: mismatch")
                compared += 1
            covered += 1
            print(f"PARTIAL_PASS {driver} fixture #{number}: "
                  f"{len(FIELDS[driver])} explicit field comparisons")
    assert covered == 8 and compared == 27
    print(f"PASS clear-app subset: {covered} upstream fixtures, "
          f"{compared} field comparisons. NOT full-frame parity; "
          f"not authorization for RF or firmware.")

if __name__ == "__main__":
    main()
