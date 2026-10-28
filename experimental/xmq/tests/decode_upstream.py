#!/usr/bin/env python3
"""Run real upstream wmbusmeters binary against XMQ test fixtures.
This is a host reference decoder check; it does not imply an ESP32 port.
"""
import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def fixtures(path):
    source = path.read_text(encoding="utf-8")
    # Upstream fixtures have one-line args, telegram and json assignments.
    pattern = re.compile(
        r"(?ms)^\s*test\s*\{(?P<body>.*?)^\s*\}"
    )
    for n, match in enumerate(pattern.finditer(source), 1):
        body = match.group("body")
        def field(name):
            found = re.search(rf"(?m)^\s*{name}\s*=\s*(.+?)\s*$", body)
            if not found:
                raise ValueError(f"{path.name} test {n}: missing {name}")
            return found.group(1).strip()
        yield n, field("args").strip("'\""), field("telegram"), json.loads(field("json").strip("'\""))

def parse_json(output):
    for line in output.splitlines():
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                parsed = json.loads(line)
                if isinstance(parsed, dict) and "id" in parsed:
                    return parsed
            except json.JSONDecodeError:
                pass
    raise ValueError("No decoded meter JSON found in output: " + output[-2000:])

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--binary", required=True, type=Path)
    args = p.parse_args()
    total = 0
    for file in sorted((ROOT / "drivers").glob("*.xmq")):
        for index, cli, telegram, expected in fixtures(file):
            meter_args = cli.split()
            if len(meter_args) != 4:
                raise ValueError(f"Unexpected meter fixture args: {cli}")
            result = subprocess.run(
                [str(args.binary.resolve()), "--format=json", telegram, *meter_args],
                capture_output=True, text=True, timeout=30
            )
            combined = result.stdout + "\n" + result.stderr
            if result.returncode:
                raise RuntimeError(f"{file.stem} fixture {index}: rc={result.returncode}\n{combined[-2000:]}")
            actual = parse_json(combined)
            # Ignore nondeterministic timestamps and serialization metadata.
            for key, want in expected.items():
                if key in ("timestamp", "_"):
                    continue
                got = actual.get(key)
                if isinstance(want, (int, float)) and not isinstance(want, bool):
                    if not isinstance(got, (int, float)) or abs(got - want) > 1e-5:
                        raise AssertionError(f"{file.stem} #{index} {key}: expected {want}, got {got}")
                elif got != want:
                    raise AssertionError(f"{file.stem} #{index} {key}: expected {want!r}, got {got!r}")
            total += 1
            print(f"PASS {file.stem} fixture {index}: " + ", ".join(
                f"{k}={actual.get(k)}" for k in expected if k not in ("timestamp", "_")
            ), flush=True)
    if total != 4:
        raise AssertionError(f"Expected 4 fixtures, observed {total}")
    print(f"PASS {total} official wmbusmeters meter-decoding fixtures", flush=True)

if __name__ == "__main__":
    main()
