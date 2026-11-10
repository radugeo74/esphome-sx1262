#!/usr/bin/env python3
"""Regenerate the exact native XMQ catalog used by ESPHome and host CI.

Do not scan arbitrary XMQ files into firmware: an unreviewed driver remains
metadata-only until the manifest declares a verified decoder.
"""
from pathlib import Path
from native_driver_specs import validate, catalog_fragment, compiled_fragment
from generate_ixml_cpp import generate
from generate_clear_profiles import generate as generate_clear_profiles
from generate_standard_selectors import generate as generate_standard_selectors
from generate_pulse_fields import generate as generate_pulse_fields
from generate_vario451_ixml import generate as generate_vario451_ixml
from generate_supercal_lse08 import generate as generate_supercal_lse08

def build(root: Path):
    xmq = root / "experimental" / "xmq"
    component = root / "components" / "wmbus_native_sx126x"
    sources = validate(xmq)
    generated = generate(sources)
    outputs = {
        "xmq_generated.h": generated,
        "xmq_generated_clear_profiles.h": generate_clear_profiles(xmq / "drivers"),
        "xmq_generated_standard_selectors.h": generate_standard_selectors(xmq / "drivers"),
        "xmq_generated_pulse_fields.h": generate_pulse_fields(xmq / "drivers"),
        "xmq_generated_vario451.h": generate_vario451_ixml(xmq / "drivers" / "vario451.xmq"),
        "xmq_generated_supercal_lse08.h": generate_supercal_lse08(xmq / "drivers"),
        "xmq_native_descriptors.h": catalog_fragment(),
        "xmq_native_compiled.h": compiled_fragment(),
    }
    for name, contents in outputs.items():
        (component / name).write_text(contents, encoding="utf-8")
    return outputs

if __name__ == "__main__":
    build(Path(__file__).resolve().parents[3])
