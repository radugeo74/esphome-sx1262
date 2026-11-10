#!/usr/bin/env python3
"""Source-anchored Ultraheat fixtures and transport provenance.

#1 is wired M-Bus; #2 is unencrypted wireless CI7A; #3 is ELL
with AES-CMAC/AFL and an already-decrypted application in XMQ.
Only #2 authorizes native no-key RF publication.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from portable_xmq import parse, validate_driver, blocks, values  # noqa: E402


def checked():
    driver = validate_driver(parse(
        (ROOT / "drivers" / "ultraheat.xmq").read_text(encoding="utf-8")))
    detect = blocks(driver, "detect")
    if (values(driver, "name") != ["ultraheat"]
            or values(driver, "meter_type") != ["HeatMeter"]
            or len(detect) != 1
            or values(detect[0]["entries"], "mvt") != ["LUG,04,04"]):
        raise ValueError("ultraheat: changed source MVT")
    fields = [f["entries"] for group in blocks(driver, "fields")
              for f in blocks(group["entries"], "field")]
    contracts = [
        ("heat", "Energy", "AnyEnergyVIF"),
        ("volume", "Volume", "Volume"),
        ("power", "Power", "PowerW"),
        ("flow", "Flow", "VolumeFlow"),
        ("flow", "Temperature", "FlowTemperature"),
        ("return", "Temperature", "ReturnTemperature"),
        ("status", "Flags", "ErrorFlags"),
    ]
    if len(fields) != len(contracts):
        raise ValueError("ultraheat: changed XMQ fields")
    for field, (name, quantity, selector) in zip(fields, contracts):
        match = blocks(field, "match")
        if (values(field, "name") != [name]
                or values(field, "quantity") != [quantity]
                or len(match) != 1
                or values(match[0]["entries"], "vif_range") != [selector]):
            raise ValueError("ultraheat: changed source field selector")
    status = fields[-1]
    lookups = blocks(status, "lookup")
    if (values(status, "attributes") != ["STATUS,INCLUDE_TPL_STATUS"]
            or len(lookups) != 1
            or values(lookups[0]["entries"], "name") != ["ERROR_FLAGS"]
            or values(lookups[0]["entries"], "map_type") != ["BitToString"]
            or values(lookups[0]["entries"], "mask_bits") != ["0xffff"]
            or values(lookups[0]["entries"], "default_message") != ["OK"]
            or blocks(lookups[0]["entries"], "map")):
        raise ValueError("ultraheat: unknown error-bit interpretation")
    suite = blocks(driver, "tests")
    if len(suite) != 1:
        raise ValueError("ultraheat: no original tests")
    samples = blocks(suite[0]["entries"], "test")
    if len(samples) != 3:
        raise ValueError("ultraheat: changed fixture count")
    originals = [
        ("MyUltra", "70444600", {
            "actuality_duration_s": 4, "flow_m3h": 0.808, "flow_c": 52,
            "heat_kwh": 8974.444444, "power_kw": 0.5,
            "return_c": 51, "target_kwh": 8513.611111,
            "volume_m3": 1995.07, "fabrication_no": "70444600",
            "meter_datetime": "2022-01-10 10:49", "status": "OK",
        }),
        ("MyUltra2", "71635605", {
            "flow_c": 35.1, "flow_m3h": 1.83, "heat_kwh": 24277,
            "meter_datetime": "2023-05-20 10:08:12",
            "power_kw": -0.2, "return_c": 35.2,
            "status": "TPL_STS_32", "volume_m3": 5699.39,
        }),
        ("MyUltra3", "72905678", {
            "actuality_duration_s": 303, "flow_m3h": 0,
            "flow_c": 74.9, "heat_kwh": 969, "power_kw": 0,
            "return_c": 51.9, "target_kwh": 0,
            "volume_m3": 28.16,
            "meter_datetime": "2026-07-27 20:56:17", "status": "OK",
        }),
    ]
    for number, (sample, (name, meter_id, fields)) in enumerate(
            zip(samples, originals), 1):
        entry = sample["entries"]
        telegram, raw_json, args = (values(entry, key) for key in
                                    ("telegram", "json", "args"))
        if len(telegram) != 1 or len(raw_json) != 1 or len(args) != 1:
            raise ValueError("ultraheat: missing source fields")
        raw = telegram[0]
        data = bytes.fromhex(raw.replace("_", ""))
        original = json.loads(raw_json[0])
        fixed = {"_": "telegram", "media": "heat", "driver": "ultraheat",
                 "name": name, "id": meter_id,
                 "timestamp": "1111-11-11T11:11:11Z"}
        if original != {**fixed, **fields}:
            raise ValueError(f"ultraheat: full source JSON #{number} changed")
        if args[0].split() != [name, "ultraheat", meter_id, "NOKEY"]:
            raise ValueError(f"ultraheat: fixture #{number} arguments changed")
        if number == 1:
            if (len(data) != 254 or data[:4] != bytes.fromhex("68F8F868")
                    or data[-1] != 0x16 or data[6] != 0x72):
                raise ValueError("ultraheat: wired M-Bus provenance changed")
        elif number == 2:
            if (len(data) != 60 or data[0]+1 != len(data)
                    or data[:4] != bytes.fromhex("3B44A732")
                    or data[8:11] != bytes.fromhex("04047A")
                    or data[12:17] != bytes.fromhex("2000202F2F")
                    or data[47:52] != bytes.fromhex("02FD170000")
                    or data[52:60] != bytes.fromhex("066D0C080AF42500")):
                raise ValueError("ultraheat: original clear wireless #2 changed")
            for start, prefix in (
                (17, "0C06"), (23, "0C14"), (29, "0B2D"),
                (34, "0B3B"), (39, "0A5A"), (43, "0A5E")
            ):
                if data[start:start+len(prefix)//2] != bytes.fromhex(prefix):
                    raise ValueError("ultraheat: six original records changed")
        else:
            # ELL CI=8C, AFL CI=90, AES-CMAC auth and short TPL inside.
            # NOKEY refers to a source supplied-clear application only.
            if (len(data) != 100 or data[:4] != bytes.fromhex("6344A732")
                    or data[10] != 0x8C or data[13] != 0x90
                    or data[30] != 0x7A
                    or data[33:36] != bytes.fromhex("400710")
                    or "_0C06" not in raw):
                raise ValueError("ultraheat: ELL/AFL AES provenance changed")
    print("PASS Ultraheat: all 3 source JSON; wired #1, true clear #2, ELL/AFL #3")


if __name__ == "__main__":
    checked()
