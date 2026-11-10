#!/usr/bin/env python3
"""Compile selected standard DIF/VIF XMQ field selectors into native C++.

A first incremental replacement of hard-coded portable C++ matching.
Each new source field changes the generated selector set, not a C++ if/else
constant. Unsupported XMQ semantics fail closed during compilation.

Currently covers iperl and munia numeric fields. Status/transport/crypto
remain in their independent, strict native admission paths.
"""
from pathlib import Path

from portable_xmq import blocks, parse, validate_driver, values

SOURCES = ("iperl", "munia")
# Canonical native unit and VIF/extended VIF range for common XMQ concepts.
# Adding a new XMQ concept requires a reviewed compiler/runtime extension.
RANGES = {
    "Volume": ("Volume", "0x0010", "0x0017"),
    "VolumeFlow": ("VolumeFlow", "0x0038", "0x003F"),
    "ExternalTemperature": ("TemperatureC", "0x0064", "0x0067"),
    "RelativeHumidity": ("RelativeHumidity", "0x7B1A", "0x7B1A"),
}
UNITS = {
    "Volume": ("Volume", "m3"),
    "Flow": ("VolumeFlow", "m3h"),
    "Temperature": ("TemperatureC", "c"),
    "RelativeHumidity": ("RelativeHumidity", "rh"),
}


def compile_driver(path: Path):
    driver = validate_driver(parse(path.read_text(encoding="utf-8")))
    if values(driver, "name") != [path.stem]:
        raise ValueError(f"{path}: mismatched XMQ name")
    fields = [f["entries"] for group in blocks(driver, "fields")
              for f in blocks(group["entries"], "field")]
    if not fields:
        raise ValueError(f"{path}: no field declarations")
    rules = []
    status_seen = False
    for field in fields:
        names, quantities = values(field, "name"), values(field, "quantity")
        if len(names) != 1 or len(quantities) != 1:
            raise ValueError(f"{path}: ambiguous XMQ field")
        name, quantity = names[0], quantities[0]
        if quantity == "Flags" and name == "status":
            if status_seen or values(field, "attributes") != [
                "STATUS,INCLUDE_TPL_STATUS"
            ]:
                raise ValueError(f"{path}: changed status semantics")
            status_seen = True
            continue
        if quantity not in UNITS:
            raise ValueError(f"{path}: unsupported source field {name}/{quantity}")
        matches = blocks(field, "match")
        if len(matches) != 1 or values(field, "calculate"):
            raise ValueError(f"{path}: unsupported match/calculation {name}")
        match = matches[0]["entries"]
        allowed = {"measurement_type", "vif_range"}
        used = {e["key"] for e in match if "key" in e}
        if used != allowed or values(match, "measurement_type") != [
            "Instantaneous"
        ]:
            raise ValueError(f"{path}: unsupported selection for {name}")
        ranges = values(match, "vif_range")
        if len(ranges) != 1 or ranges[0] not in RANGES:
            raise ValueError(f"{path}: unsupported VIF range for {name}")
        kind, suffix = UNITS[quantity]
        range_kind, lower, upper = RANGES[ranges[0]]
        if kind != range_kind:
            raise ValueError(f"{path}: incompatible unit and selector for {name}")
        rules.append((f"{name}_{suffix}", kind, lower, upper))
    if not status_seen or len(rules) != 2:
        # The consumers use two strongly typed measurements today.
        # Additional fields must not be ignored or falsely advertised as
        # automatically supported. Extend the generic output interface first.
        raise ValueError(f"{path}: new source fields require runtime migration")
    return rules


def generate(driver_dir: Path) -> str:
    result = [
        "// Generated from XMQ match/quantity declarations; do not edit.",
        "// Strict native CRC, ID and TPL security gates remain mandatory.",
        "#pragma once",
        "#include <cstdint>",
        "namespace xmq_generated_standard {",
        "enum class Kind : uint8_t { Volume, VolumeFlow, TemperatureC, RelativeHumidity };",
        "struct Rule {const char* name; Kind kind; uint16_t vif_first,vif_last;};",
    ]
    for name in SOURCES:
        rules = compile_driver(driver_dir / f"{name}.xmq")
        result.append(f"namespace {name} {{")
        result.append("inline constexpr Rule fields[]={")
        for output, kind, start, end in rules:
            result.append(
                f'  {{"{output}",Kind::{kind},{start},{end}}},'
            )
        result.append("};")
        result.append("}")
    result.append("}")
    return "\n".join(result)+"\n"


if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--drivers", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    args.out.write_text(generate(args.drivers), encoding="utf-8")
