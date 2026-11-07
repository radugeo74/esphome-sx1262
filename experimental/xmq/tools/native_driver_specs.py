"""Single native XMQ driver capability manifest.

Adding a generated decoder requires a reviewed entry here and passing its
official upstream XMQ fixtures. Recognition-only XMQ files stay in the
metadata registry; they cannot be published by a guessed decoder.
"""
from dataclasses import dataclass
from pathlib import Path

@dataclass(frozen=True)
class GeneratedDriver:
    name: str
    ci: int
    payload_policy: str
    dates: bool = False

GENERATED = (
    GeneratedDriver("fhkvdataiii", 0xA0, "PreferManufacturer0f", True),
    GeneratedDriver("mkradio4", 0xA2, "Application"),
)
# Portable live cleartext decoders, independently validated with official
# upstream fixture records. These do not require a generated IXML grammar.
PORTABLE = ("iperl", "hydrus")
# A decoder for trusted, already-decrypted application bytes exists, but it
# MUST NOT be selected for live RF without an authenticated decrypt stage.
POST_DECRYPT_ONLY = ("mkradio4a",)
# Imported official source definitions under active review; do not allow
# YAML mapping or firmware runtime dispatch before passing decode fixtures.
BATCH2_PENDING = ("minomess", "kamwater", "apator162")
# Explicit-only native NOKEY (no AUTO and no legacy/crypto fallback).
MANUAL_PLAIN = ("lansenpu",)
SUPPORTED = tuple(d.name for d in GENERATED) + PORTABLE

def validate(root: Path):
    names = [d.name for d in GENERATED]
    if len(set(SUPPORTED + POST_DECRYPT_ONLY + BATCH2_PENDING + MANUAL_PLAIN)) != len(SUPPORTED + POST_DECRYPT_ONLY + BATCH2_PENDING):
        raise ValueError("Duplicate native XMQ driver")
    for name in BATCH2_PENDING + MANUAL_PLAIN:
        if not (root / "drivers" / f"{name}.xmq").is_file():
            raise ValueError(f"{name}: missing upstream batch-2 XMQ definition")
    for d in GENERATED:
        if d.payload_policy not in ("Application", "PreferManufacturer0f"):
            raise ValueError(f"{d.name}: unknown payload policy")
        if not (0 <= d.ci <= 255):
            raise ValueError(f"{d.name}: invalid CI")
        if not (root / "drivers" / f"{d.name}.xmq").is_file():
            raise ValueError(f"{d.name}: missing original XMQ definition")
    return tuple(root / "drivers" / f"{name}.xmq" for name in names)

def catalog_fragment():
    lines = ["#pragma once", "// Generated native XMQ descriptor entries. Do not edit."]
    for d in GENERATED:
        ns = f"generated_ixml::{d.name}"
        dates = (f"{ns}::date_calculations, {ns}::date_calculation_count"
                 if d.dates else "nullptr, 0")
        lines.append(
            f'{{"{d.name}", 0x{d.ci:02X}, PayloadPolicy::{d.payload_policy}, {{'
            f'{ns}::rules, {ns}::count, {ns}::capture_keys, '
            f'sizeof({ns}::capture_keys)/sizeof(CaptureKey), '
            f'{ns}::field_keys, sizeof({ns}::field_keys)/sizeof(FieldKey), '
            f'{ns}::calculations, {ns}::calculation_count, {dates}}}}},'
        )
    # ESPHome auto-includes all component .h files into main.cpp. A raw
    # initializer fragment would break that translation unit. A macro is
    # valid standalone and expands only where the catalog needs entries.
    entries = lines[2:]
    macro = ["#define XMQ_NATIVE_CATALOG_ENTRIES \\"]
    for index, entry in enumerate(entries):
        macro.append(entry + (" \\" if index + 1 < len(entries) else ""))
    return "\n".join(lines[:2] + macro) + "\n"

def compiled_fragment():
    lines = ["#pragma once", "// Generated from the single native driver manifest."]
    lines.append("inline bool native_xmq_driver_compiled(const char *name) {")
    lines.append("  return name && (")
    lines.append(" ||\n".join(
        f'      std::strcmp(name, "{name}") == 0' for name in SUPPORTED))
    lines.append("  );\n}")
    return "\n".join(lines) + "\n"
