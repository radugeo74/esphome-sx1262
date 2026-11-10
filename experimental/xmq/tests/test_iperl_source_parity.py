#!/usr/bin/env python3
"""Fail-closed source parity for all original Sensus iPERL XMQ fixtures.

First case is upstream post-decryption application under N=1 TPL header.
It is never evidence for a clear on-air RF packet.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from portable_xmq import parse, validate_driver, blocks, values  # noqa: E402


def checked():
    source = ROOT / "drivers" / "iperl.xmq"
    driver = validate_driver(parse(source.read_text(encoding="utf-8")))
    if values(driver, "name") != ["iperl"] or values(driver, "meter_type") != ["WaterMeter"]:
        raise ValueError("iPERL: changed identity")
    detect = blocks(driver, "detect")
    if len(detect) != 1 or set(values(detect[0]["entries"], "mvt")) != {
        "SEN,68,06", "SEN,68,07", "SEN,7c,07"
    }:
        raise ValueError("iPERL: unexpected detection profiles")
    field_groups = blocks(driver, "fields")
    fields = [f["entries"] for group in field_groups
              for f in blocks(group["entries"], "field")]
    named = {values(field, "name")[0]: field for field in fields}
    if len(named) != 3 or set(named) != {"total", "max_flow", "status"}:
        raise ValueError("iPERL: source field contract changed")
    for field, quantity, match in [
        ("total", "Volume", "Volume"),
        ("max_flow", "Flow", "VolumeFlow"),
    ]:
        obj = named[field]
        m = blocks(obj, "match")
        if values(obj, "quantity") != [quantity] or len(m) != 1 or (
            values(m[0]["entries"], "measurement_type") != ["Instantaneous"]
            or values(m[0]["entries"], "vif_range") != [match]
        ):
            raise ValueError(f"iPERL: {field} unit/selector changed")
    status = named["status"]
    if values(status, "quantity") != ["Flags"] or values(
        status, "attributes"
    ) != ["STATUS,INCLUDE_TPL_STATUS"]:
        raise ValueError("iPERL: status meaning changed")
    suites = blocks(driver, "tests")
    if len(suites) != 1:
        raise ValueError("iPERL: no fixture suite")
    tests = blocks(suites[0]["entries"], "test")
    if len(tests) != 3:
        raise ValueError("iPERL: expected exactly three original fixtures")
    expected = [
        ("12345699", 0x00, 0x10, 7.704, "OK", True),
        ("33225544", 0x00, 0x00, 123.529, "OK", False),
        ("33225544", 0x06, 0x00, 123.529, "ERROR POWER_LOW", False),
    ]
    for sample, (device_id, tpl, n_bits, total, text, postdecrypted) in zip(tests, expected):
        e = sample["entries"]
        raw = values(e, "telegram")
        json_blob = values(e, "json")
        args = values(e, "args")
        if len(raw) != 1 or len(json_blob) != 1 or len(args) != 1:
            raise ValueError("iPERL: missing fixture record")
        frame = bytes.fromhex(raw[0].replace("_", "").replace("|", ""))
        actual_id = "".join(f"{n:02X}" for n in reversed(frame[4:8]))
        if (len(frame) != frame[0]+1 or frame[1:4] != bytes.fromhex("44AE4C")
                or frame[8:11] != bytes.fromhex("68077A")
                or actual_id != device_id or frame[12] != tpl
                or frame[13:15] != bytes([n_bits, 0])
                or args[0].split() != [
                    "MoreWater" if postdecrypted else "WaterWater",
                    "iperl", device_id, "NOKEY",
                ]):
            raise ValueError("iPERL: model/header/security evidence changed")
        if postdecrypted:
            if (frame[15:17] != bytes.fromhex("2F2F")
                    or frame[17:19] != bytes.fromhex("0413")
                    or frame[23:25] != bytes.fromhex("023B")
                    or frame[27:] != bytes.fromhex("2F2F2F2F")):
                raise ValueError("iPERL: post-decryption application changed")
            raw_total = int.from_bytes(frame[19:23], "little")
            raw_flow = int.from_bytes(frame[25:27], "little")
        else:
            if (len(frame) != 25 or frame[15:17] != bytes.fromhex("0413")
                    or frame[21:23] != bytes.fromhex("023B")):
                raise ValueError("iPERL: original clear application changed")
            raw_total = int.from_bytes(frame[17:21], "little")
            raw_flow = int.from_bytes(frame[23:25], "little")
        output = json.loads(json_blob[0])
        required = {"_", "media", "driver", "name", "id", "total_m3",
                    "max_flow_m3h", "status", "timestamp"}
        if (set(output) != required or output["_"] != "telegram"
                or output["media"] != "water" or output["driver"] != "iperl"
                or output["name"] != ("MoreWater" if postdecrypted else "WaterWater")
                or output["id"] != device_id or
                output["total_m3"] != total or
                output["total_m3"] != raw_total/1000 or
                output["max_flow_m3h"] != raw_flow/1000 or
                output["max_flow_m3h"] != 0 or
                output["status"] != text or
                output["timestamp"] != "1111-11-11T11:11:11Z"):
            raise ValueError("iPERL: source JSON/numeric/status parity changed")
        if postdecrypted and n_bits != 0x10:
            raise ValueError("iPERL: post-decrypted sample may not authorize RF")
    print("PASS iPERL source JSON: 3 original cases; 2 clear, 1 post-decrypted only.")


if __name__ == "__main__":
    checked()
