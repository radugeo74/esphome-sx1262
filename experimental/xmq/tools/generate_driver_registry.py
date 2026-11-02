#!/usr/bin/env python3
"""Generate metadata-only driver signature table from XMQ detect.mvt declarations."""
import argparse
import re
from pathlib import Path

MVT = re.compile(r"^\s*mvt\s*=\s*([A-Z]{3})\s*,\s*([0-9A-Fa-f]{2})\s*,\s*([0-9A-Fa-f]{2})\s*$", re.M)
NAME = re.compile(r"^\s*name\s*=\s*([a-z][a-z0-9_]*)\s*$", re.M)

def manufacturer_code(name):
    if len(name) != 3 or any(not ('A' <= c <= 'Z') for c in name):
        raise ValueError("Invalid EN13757 manufacturer")
    return ((ord(name[0])-64)<<10) | ((ord(name[1])-64)<<5) | (ord(name[2])-64)

def generate(paths):
    records = []
    for path in paths:
        source = Path(path).read_text(encoding="utf-8")
        names = NAME.findall(source)
        if not names:
            raise ValueError(f"{path}: missing driver name")
        name = names[0]
        variants = MVT.findall(source)
        if not variants:
            raise ValueError(f"{path}: no supported detect.mvt entries")
        for manufacturer, version, device in variants:
            records.append((manufacturer_code(manufacturer), int(version,16), int(device,16), name))
    records = sorted(set(records))
    lines = ['// Generated from original XMQ detect.mvt; do not edit.',
             '#pragma once', '#include "pure_driver_registry.h"',
             'namespace wmbus_pure { namespace generated {',
             'inline constexpr DriverSignature signatures[] = {']
    lines += [f'  {{0x{m:04X}, 0x{v:02X}, 0x{t:02X}, "{n}"}},' for m,v,t,n in records]
    lines += ['};', 'inline constexpr size_t signature_count = sizeof(signatures)/sizeof(signatures[0]);',
              '} }']
    return "\n".join(lines)+"\n"

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("drivers", nargs="+", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    Path(args.out).write_text(generate(args.drivers), encoding="utf-8")
