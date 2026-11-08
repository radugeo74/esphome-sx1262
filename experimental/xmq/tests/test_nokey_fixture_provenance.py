#!/usr/bin/env python3
"""No-key XMQ is test metadata, never a claim of plaintext original RF.

Classify *only* explicit, syntactically recognizable short/long OMS TPL
headers. Manufacturer-specific, ELL and wired packets remain unknown for
this narrow policy. This script never decrypts, handles production keys,
or decides whether a commercial model always requires an AES key.
"""
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "experimental/xmq"
sys.path.insert(0, str(BASE / "tools"))
from native_driver_specs import GENERATED, PORTABLE, CONFIGURED_NATIVE, SECURE_ONLY

FIXTURE = re.compile(
    r"\bargs\s*=\s*'([^']+)'[\s\S]*?\btelegram\s*=\s*([0-9A-Fa-f_]+)"
)

def classify(tg: str):
    # Only the supplied header before '_' is relevant. The application
    # after '_' can ALREADY BE DECRYPTED and is never an RF observation.
    raw_header = bytes.fromhex(tg.split("_", 1)[0])
    if raw_header and raw_header[0] == 0x68:
        return "WIRED_NOT_RF", None, None
    if len(raw_header) < 11:
        return "UNKNOWN", None, None
    ci = raw_header[10]
    if ci in (0xA0, 0xA2):
        return "MANUFACTURER_CI", None, None
    if ci not in (0x7A, 0x72):
        return "OTHER_TRANSPORT_UNKNOWN", None, None
    off = 13 if ci == 0x7A else 21
    if len(raw_header) < off + 2:
        return "TPL_INCOMPLETE", None, None
    cf = raw_header[off] | (raw_header[off + 1] << 8)
    mode, blocks = (cf >> 8) & 0x1F, (cf >> 4) & 0x0F
    if mode == 0 and blocks == 0:
        return "TPL_MODE0_CANDIDATE", mode, blocks
    if mode == 0 and blocks != 0:
        return "TPL_MODE0_INVALID", mode, blocks
    if mode == 5:
        return "TPL_MODE5_SECURE_HEADER", mode, blocks
    # Do not assert these other security codes are encrypted or clear:
    # vendor-specific and unsupported layouts can reuse these bits.
    return "TPL_UNSUPPORTED_OR_VENDOR", mode, blocks

def main():
    catalog = json.loads((BASE / "security_profile_catalog.json").read_text())
    names = tuple(d.name for d in GENERATED) + PORTABLE + CONFIGURED_NATIVE + SECURE_ONLY
    assert len(names) == 32 and len(set(names)) == 32
    assert {x["driver"] for x in catalog["drivers"]} == set(names)
    total = nokey = non_key_mode5 = all_mode5 = 0
    by_driver = Counter()
    modes = Counter()
    cases_with_markers = 0
    for driver in names:
        source = (BASE / "drivers" / f"{driver}.xmq").read_text(encoding="utf-8")
        examples = list(FIXTURE.finditer(source))
        expected = next(x for x in catalog["drivers"] if x["driver"] == driver)
        assert len(examples) == expected["total_tests"], driver
        for idx, example in enumerate(examples, 1):
            args, tg = example.group(1), example.group(2)
            no_key = "NOKEY" in args.split()
            marker = "_" in tg
            category, mode, blocks = classify(tg)
            modes[category] += 1
            total += 1
            nokey += int(no_key)
            cases_with_markers += int(marker)
            if category == "TPL_MODE5_SECURE_HEADER":
                all_mode5 += 1
                assert blocks is not None and blocks > 0, (driver, idx)
                if no_key:
                    non_key_mode5 += 1
                    by_driver[driver] += 1
                # An already-clear application following '_' NEVER turns
                # the original Mode-5 configuration into a Mode-0 frame.
                if marker:
                    assert category != "TPL_MODE0_CANDIDATE"
            if category == "TPL_MODE0_INVALID":
                assert blocks and not mode, (driver, idx)
            if driver in ("mkradio4a", "topaseskr", "lansenth", "lansensm"):
                # Upstream NOKEY supplied-clear examples retain Mode-5
                # security headers. The first Lansen TH example has no
                # '_' separator; this does NOT make it ciphertext.
                assert no_key and category == "TPL_MODE5_SECURE_HEADER"
                if driver in ("mkradio4a", "topaseskr"):
                    assert marker
                assert "2F2F" in tg.split("_", 1)[0].upper()
        assert sum("NOKEY" in x.group(1).split() for x in examples) == expected["nokey_tests"]
    assert (total, nokey, all_mode5, non_key_mode5) == (152, 146, 45, 43)
    assert by_driver == Counter({
        "hydrus": 3, "q400": 2, "minomess": 4, "apator162": 18,
        "mkradio4a": 2, "topaseskr": 2,
        "lansenth": 2, "lansensm": 2,
        "elvsense": 2,
        "qcaloric": 2,
        "sensostar": 1,
        "hcae2": 1,
        "hydrodigit": 1,
        "itron": 1,
    }), by_driver
    # Public iperl example #1 says "after decryption" and declares M=0,
    # N=1. The OMS Mode-0 gate correctly rejects that inconsistent CF;
    # the other two iPERL examples really have zero encrypted blocks.
    assert modes["TPL_MODE0_INVALID"] == 1
    iperl = list(FIXTURE.finditer((BASE/"drivers/iperl.xmq").read_text()))
    assert [classify(x.group(2))[0] for x in iperl] == [
        "TPL_MODE0_INVALID", "TPL_MODE0_CANDIDATE", "TPL_MODE0_CANDIDATE"
    ]
    assert cases_with_markers > 0 and modes["MANUFACTURER_CI"] >= 4
    print(f"PASS NOKEY provenance: {total} XMQ fixtures, {nokey} NOKEY, "
          f"{non_key_mode5} NOKEY with Mode-5 AES headers, "
          f"{modes['TPL_MODE0_INVALID']} invalid Mode-0 CF; "
          "no RF/keyless inference from fixture metadata or 2F2F.")

if __name__ == "__main__":
    main()
